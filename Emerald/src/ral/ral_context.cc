/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_sampler.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"


typedef bool (*PFNRALBACKENDGETFRAMEBUFFERPROC)(void*                backend,
                                                ral_framebuffer      framebuffer_ral,
                                                void**               out_framebuffer_backend_ptr);
typedef void (*PFNRALBACKENDGETPROPERTYPROC)   (void*                backend,
                                                ral_context_property property,
                                                void*                out_result_ptr);
typedef void (*PFNRALBACKENDRELEASEPROC)       (void*                backend);


typedef struct _ral_context
{
    /* Rendering back-end instance:
     *
     * OpenGL context:    raGL_backend instance
     * OpenGL ES context: raGL_backend instance
     **/
    void*                           backend; 
    PFNRALBACKENDGETFRAMEBUFFERPROC pfn_backend_get_framebuffer_proc;
    PFNRALBACKENDGETPROPERTYPROC    pfn_backend_get_property_proc;
    PFNRALBACKENDRELEASEPROC        pfn_backend_release_proc;

    system_callback_manager callback_manager;

    system_resizable_vector framebuffers; /* owns ral_framebuffer instances */
    system_critical_section framebuffers_cs;
    uint32_t                n_framebuffers_created;

    system_resizable_vector samplers; /* owns ral_sampler instances */
    system_critical_section samplers_cs;
    uint32_t                n_samplers_created;

    system_hashed_ansi_string name;
    ogl_rendering_handler     rendering_handler;
    system_window             window;

    REFCOUNT_INSERT_VARIABLES;


    _ral_context(system_hashed_ansi_string in_name,
                 system_window             in_window,
                 ogl_rendering_handler     in_rendering_handler)
    {
        backend                = NULL;
        callback_manager       = system_callback_manager_create((_callback_id) RAL_CONTEXT_CALLBACK_ID_COUNT);
        framebuffers           = system_resizable_vector_create(sizeof(ral_framebuffer) );
        framebuffers_cs        = system_critical_section_create();
        n_framebuffers_created = 0;
        n_samplers_created     = 0;
        name                   = in_name;
        rendering_handler      = in_rendering_handler;
        samplers               = system_resizable_vector_create(sizeof(ral_sampler) );
        samplers_cs            = system_critical_section_create();
        window                 = in_window;

        pfn_backend_get_property_proc = NULL;

        ASSERT_DEBUG_SYNC(callback_manager != NULL,
                          "Could not create a callback manager instance");
        ASSERT_DEBUG_SYNC(framebuffers != NULL,
                          "Could not create the framebuffers vector.");
        ASSERT_DEBUG_SYNC(samplers != NULL,
                          "Could not create the samplers vector.");
    }

    ~_ral_context()
    {
        if (framebuffers != NULL)
        {
            /* Framebuffer instances should have been released by _ral_context_release().. */
            uint32_t n_framebuffers = 0;

            system_resizable_vector_get_property(framebuffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_framebuffers);

            ASSERT_DEBUG_SYNC(n_framebuffers == 0,
                              "Memory leak detected");

            system_resizable_vector_release(framebuffers);
            framebuffers = NULL;
        } /* if (framebuffers != NULL) */

        if (framebuffers_cs != NULL)
        {
            system_critical_section_release(framebuffers_cs);

            framebuffers_cs = NULL;
        } /* if (framebuffers_cs != NULL) */

        if (samplers != NULL)
        {
            /* Sampler instances should have been released by _ral_context_release().. */
            uint32_t n_samplers = 0;

            system_resizable_vector_get_property(samplers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_samplers);

            ASSERT_DEBUG_SYNC(n_samplers == 0,
                              "Memory leak detected");

            system_resizable_vector_release(samplers);
            samplers = NULL;
        } /* if (samplers != NULL) */

        if (samplers_cs != NULL)
        {
            system_critical_section_release(samplers_cs);

            samplers_cs = NULL;
        } /* if (samplers_cs != NULL) */


        /* Callback manager needs to be deleted at the end. */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */
    }
} _ral_context;

typedef enum
{

    RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
    RAL_CONTEXT_OBJECT_TYPE_SAMPLER

} _ral_context_object_type;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_context,
                               ral_context,
                              _ral_context);

/** TODO */
PRIVATE void _ral_context_release(void* context)
{
    _ral_context* context_ptr = (_ral_context*) context;

    /* Delete all framebuffer instances */
    {
        system_critical_section_enter(context_ptr->framebuffers_cs);
        {
            ral_framebuffer* framebuffers   = NULL;
            uint32_t         n_framebuffers = 0;

            system_resizable_vector_get_property(context_ptr->framebuffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                                &framebuffers);
            system_resizable_vector_get_property(context_ptr->framebuffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_framebuffers);

            ral_context_delete_framebuffers((ral_context) context,
                                            n_framebuffers,
                                            framebuffers);
        }
        system_critical_section_leave(context_ptr->framebuffers_cs);
    }

    /* Delete all sampler instances */
    {
        system_critical_section_enter(context_ptr->samplers_cs);
        {
            ral_sampler* samplers   = NULL;
            uint32_t     n_samplers = 0;

            system_resizable_vector_get_property(context_ptr->samplers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                                &samplers);
            system_resizable_vector_get_property(context_ptr->samplers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_samplers);

            ral_context_delete_samplers((ral_context) context,
                                        n_samplers,
                                        samplers);
        }
        system_critical_section_leave(context_ptr->samplers_cs);
    }

    /* Release the back-end */
    if (context_ptr->backend                  != NULL &&
        context_ptr->pfn_backend_release_proc != NULL)
    {
        context_ptr->pfn_backend_release_proc(context_ptr->backend);

        context_ptr->backend = NULL;
    }
}

/** Please see header for specification */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      system_window             window,
                                      ogl_rendering_handler     rendering_handler)
{
    _ral_context* new_context_ptr = new (std::nothrow) _ral_context(name,
                                                                    window,
                                                                    rendering_handler);

    ASSERT_ALWAYS_SYNC(new_context_ptr != NULL,
                       "Out of memory");
    if (new_context_ptr != NULL)
    {
        /* Instantiate the rendering back-end */
        ogl_context      context      = NULL;
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                                  &context);
        ogl_context_get_property  (context,
                                   OGL_CONTEXT_PROPERTY_TYPE,
                                  &context_type);

        switch (context_type)
        {
            case OGL_CONTEXT_TYPE_ES:
            case OGL_CONTEXT_TYPE_GL:
            {
                new_context_ptr->backend                          = (void*) raGL_backend_create( (ral_context) new_context_ptr);
                new_context_ptr->pfn_backend_get_framebuffer_proc = raGL_backend_get_framebuffer;
                new_context_ptr->pfn_backend_get_property_proc    = raGL_backend_get_property;
                new_context_ptr->pfn_backend_release_proc         = raGL_backend_release;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported context type requested.");
            }
        } /* switch(type) */

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_context_ptr,
                                                       _ral_context_release,
                                                       OBJECT_TYPE_RAL_CONTEXT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Contexts\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_context_ptr != NULL) */

    return (ral_context) new_context_ptr;
}


PRIVATE bool _ral_context_create_objects(_ral_context*            context_ptr,
                                         _ral_context_object_type object_type,
                                         uint32_t                 n_objects,
                                         void**                   object_create_info_ptrs,
                                         void**                   out_result_object_ptrs)
{
    uint32_t*               object_counter_ptr    = NULL;
    system_critical_section object_storage_cs     = NULL;
    system_resizable_vector object_storage_vector = NULL;
    const char*             object_type_name      = NULL;
    bool                    result                = false;
    void**                  result_objects_ptr    = NULL;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
            object_storage_cs     = context_ptr->framebuffers_cs;
            object_storage_vector = context_ptr->framebuffers;
            object_type_name      = "RAL Framebuffer";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
            object_storage_cs     = context_ptr->samplers_cs;
            object_storage_vector = context_ptr->samplers;
            object_type_name      = "RAL Sampler";

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized context object type");

            goto end;
        }
    } /* switch (object_type) */

    /* After framebuffer instances are created, we will need to fire a notification,
     * so that the backend can create renderer-specific objects for each RAL framebuffer.
     *
     * The notification's argument takes an array of framebuffer instances. Allocate space
     * for that array. */
    result_objects_ptr = new void*[n_objects];

    if (result_objects_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    memset(result_objects_ptr,
           0,
           sizeof(void*) * n_objects);

    /* Create the new object instances. */
    for (uint32_t n_object = 0;
                  n_object < n_objects;
                ++n_object)
    {
        char temp[128];

        snprintf(temp,
                 sizeof(temp),
                 object_type_name,
                 (*object_counter_ptr)++);

        switch (object_type)
        {
            case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
            {
                result_objects_ptr[n_object] = ral_framebuffer_create( (ral_context) context_ptr,
                                                                      system_hashed_ansi_string_create(temp) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                result_objects_ptr[n_object] = ral_sampler_create(system_hashed_ansi_string_create(temp),
                                                                  (ral_sampler_create_info*) (object_create_info_ptrs + n_object) );

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL context object type");

                goto end;
            }
        } /* switch (object_type) */

        if (result_objects_ptr[n_object] == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a %s at index [%d]",
                              object_type_name,
                              n_object);

            goto end;
        }
    } /* for (all framebuffers to create) */

    /* Notify the subscribers */
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            ral_context_callback_framebuffers_created_callback_arg callback_arg;

            callback_arg.created_framebuffers = (ral_framebuffer*) result_objects_ptr;
            callback_arg.n_framebuffers       = n_objects;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            ral_context_callback_samplers_created_callback_arg callback_arg;

            callback_arg.created_samplers = (ral_sampler*) result_objects_ptr;
            callback_arg.n_samplers       = n_objects;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                              &callback_arg);

            break;
        }

    } /* switch (object_type) */

    /* Store the new objects */
    system_critical_section_enter(object_storage_cs);
    {
        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            system_resizable_vector_push(object_storage_vector,
                                         result_objects_ptr[n_object]);
        }
    }
    system_critical_section_leave(object_storage_cs);

    /* Configure the output variables */
    memcpy(out_result_object_ptrs,
           result_objects_ptr,
           sizeof(void*) * n_objects);

    /* All done */
    result = true;
end:
    if (!result)
    {
        if (result_objects_ptr != NULL)
        {
            for (uint32_t n_object = 0;
                          n_object < n_objects;
                        ++n_object)
            {
                if (result_objects_ptr[n_object] != NULL)
                {
                    switch (object_type)
                    {
                        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     result_objects_ptr[n_object]); break;

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized RAL context object type");
                        }
                    } /* switch (object_type) */

                    result_objects_ptr[n_object] = NULL;
                } /* if (result_objects_ptr[n_object] != NULL) */
            } /* for (all potentially created RAL objects) */
        } /* if (result_framebuffers_ptr != NULL) */
    } /* if (!result) */

    if (result_objects_ptr != NULL)
    {
        delete [] result_objects_ptr;

        result_objects_ptr = NULL;
    }

    return result;
}

/** TODO */
PRIVATE bool _ral_context_delete_objects(_ral_context*            context_ptr,
                                         _ral_context_object_type object_type,
                                         uint32_t                 n_objects,
                                         void**                   object_ptrs)
{
    system_critical_section cs     = NULL;
    bool                    result = false;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            cs = context_ptr->framebuffers_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            cs = context_ptr->samplers_cs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    } /* switch (object_type) */

    system_critical_section_enter(cs);
    {
        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            if (object_ptrs[n_object] == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Object at index [%d] is NULL",
                                  n_object);

                continue;
            } /* if (object_ptrs[n_object] == NULL) */

            switch (object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) object_ptrs[n_object]); break;
                case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     object_ptrs[n_object]); break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL context object type");

                    goto end;
                }
            } /* switch (object_type) */
        } /* for (all specified framebuffer instances) */
    }
    system_critical_section_leave(cs);

    /* Notify the subscribers about the event */
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            ral_context_callback_framebuffers_deleted_callback_arg callback_arg;

            callback_arg.deleted_framebuffers = (ral_framebuffer*) object_ptrs;
            callback_arg.n_framebuffers       = n_objects;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            ral_context_callback_samplers_deleted_callback_arg callback_arg;

            callback_arg.deleted_samplers = (ral_sampler*) object_ptrs;
            callback_arg.n_samplers       = n_objects;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                             &callback_arg);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    } /* switch (object_type) */

    result = true;

end:
    return result;
}


/** Please see header for specification */
PUBLIC bool ral_context_create_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* out_result_framebuffers_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_framebuffers == 0)
    {
        goto end;
    }

    if (out_result_framebuffers_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                         n_framebuffers,
                                         NULL, /* object_create_info_ptrs */
                                         (void**) out_result_framebuffers_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_create_samplers(ral_context                    context,
                                        uint32_t                       n_samplers,
                                        const ral_sampler_create_info* sampler_create_info_ptr,
                                        ral_sampler*                   out_result_samplers_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    }

    if (n_samplers == 0)
    {
        goto end;
    }

    if (sampler_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input sampler create info array is NULL");

        goto end;
    }

    if (out_result_samplers_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                         n_samplers,
                                         (void**) sampler_create_info_ptr,
                                         (void**) out_result_samplers_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_delete_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* framebuffers)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    } /* if (context == NULL) */

    if (n_framebuffers == 0)
    {
        goto end;
    }

    if (framebuffers == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input framebuffers instance is NULL");

        goto end;
    }

    /* Release the framebuffers */
    result = _ral_context_delete_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                         n_framebuffers,
                                         (void**) framebuffers);

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_delete_samplers(ral_context  context,
                                        uint32_t     n_samplers,
                                        ral_sampler* samplers)
{
    _ral_context* context_ptr = (_ral_context*) context;
    bool          result      = false;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    } /* if (context == NULL) */

    if (n_samplers == 0)
    {
        goto end;
    }

    if (samplers == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input samplers instance is NULL");

        goto end;
    }

    /* Release the framebuffers */
    result = _ral_context_delete_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                         n_samplers,
                                         (void**) samplers);

end:
    return result;
}

/** Please see header for specification */
PUBLIC void ral_context_get_property(ral_context          context,
                                     ral_context_property property,
                                     void*                out_result_ptr)
{
    _ral_context* context_ptr = (_ral_context*) context;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input context is NULL");

        goto end;
    } /* if (context == NULL) */

    /* If this is a backend-specific property, forward the request to the backend instance.
     * Otherwise, try to handle it. */
    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_TEXTURE_FORMAT:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        {
            context_ptr->pfn_backend_get_property_proc(context_ptr->backend,
                                                        property,
                                                        out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = context_ptr->callback_manager;

            break;
        }

        case RAL_CONTEXT_PROPERTY_RENDERING_HANDLER:
        {
            *(ogl_rendering_handler*) out_result_ptr = context_ptr->rendering_handler;

            break;
        }

        case RAL_CONTEXT_PROPERTY_WINDOW:
        {
            *(system_window*) out_result_ptr = context_ptr->window;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecpognized ral_context_property value.");
        }
    } /* switch (property) */
end:
    ;
}