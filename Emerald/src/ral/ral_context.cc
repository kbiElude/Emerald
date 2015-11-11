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
        name                   = in_name;
        rendering_handler      = in_rendering_handler;
        window                 = in_window;

        pfn_backend_get_property_proc = NULL;

        ASSERT_DEBUG_SYNC(callback_manager != NULL,
                          "Could not create a callback manager instance");
        ASSERT_DEBUG_SYNC(framebuffers != NULL,
                          "Could not create the framebuffers vector.")
    }

    ~_ral_context()
    {
        if (framebuffers != NULL)
        {
            /* Release all framebuffers */
            ASSERT_DEBUG_SYNC(false,
                              "TODO");
        } /* if (framebuffers != NULL) */

        if (framebuffers_cs != NULL)
        {
            system_critical_section_release(framebuffers_cs);

            framebuffers_cs = NULL;
        } /* if (framebuffers_cs != NULL) */


        /* Callback manager needs to be deleted at the end. */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */
    }
} _ral_context;


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

/** Please see header for specification */
PUBLIC bool ral_context_create_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* out_result_framebuffers_ptr)
{
    ral_context_callback_framebuffers_created_callback_arg callback_arg;
    _ral_context*                                          context_ptr             = (_ral_context*) context;
    bool                                                   result                  = false;
    ral_framebuffer*                                       result_framebuffers_ptr = NULL;

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

    /* After framebuffer instances are created, we will need to fire a notification,
     * so that the backend can create renderer-specific objects for each RAL framebuffer.
     *
     * The notification's argument takes an array of framebuffer instances. Allocate space
     * for that array. */
    result_framebuffers_ptr = new ral_framebuffer[n_framebuffers];

    if (result_framebuffers_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    memset(result_framebuffers_ptr,
           0,
           sizeof(ral_framebuffer) * n_framebuffers);

    /* Create the new framebuffer instances. */
    for (uint32_t n_framebuffer = 0;
                  n_framebuffer < n_framebuffers;
                ++n_framebuffer)
    {
        char temp[128];

        snprintf(temp,
                 sizeof(temp),
                 "RAL framebuffer %d",
                 context_ptr->n_framebuffers_created++);

        result_framebuffers_ptr[n_framebuffer] = ral_framebuffer_create(context,
                                                                        system_hashed_ansi_string_create(temp) );

        if (result_framebuffers_ptr[n_framebuffer] == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a RAL framebuffer at index [%d]",
                              n_framebuffer);

            goto end;
        }
    } /* for (all framebuffers to create) */

    /* Notify the subscribers */
    callback_arg.created_framebuffers = result_framebuffers_ptr;
    callback_arg.n_framebuffers       = n_framebuffers;

    system_callback_manager_call_back(context_ptr->callback_manager,
                                      RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                     &callback_arg);

    /* Store the new framebuffers */
    for (uint32_t n_framebuffer = 0;
                  n_framebuffer < n_framebuffers;
                ++n_framebuffer)
    {
        system_resizable_vector_push(context_ptr->framebuffers,
                                     result_framebuffers_ptr[n_framebuffer]);
    }

    /* Configure the output variables */
    memcpy(out_result_framebuffers_ptr,
           result_framebuffers_ptr,
           sizeof(ral_framebuffer) * n_framebuffers);

    /* All done */
    result = true;
end:
    if (!result)
    {
        if (result_framebuffers_ptr != NULL)
        {
            for (uint32_t n_framebuffer = 0;
                          n_framebuffer < n_framebuffers;
                        ++n_framebuffer)
            {
                if (result_framebuffers_ptr[n_framebuffer] != NULL)
                {
                    ral_framebuffer_release(result_framebuffers_ptr[n_framebuffer]);

                    result_framebuffers_ptr[n_framebuffer] = NULL;
                } /* if (result_framebuffers_ptr[n_framebuffer] != NULL) */
            } /* for (all potentially created RAL framebuffers) */
        } /* if (result_framebuffers_ptr != NULL) */
    } /* if (!result) */

    if (result_framebuffers_ptr != NULL)
    {
        delete [] result_framebuffers_ptr;

        result_framebuffers_ptr = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC void ral_context_delete_framebuffers(ral_context      context,
                                            uint32_t         n_framebuffers,
                                            ral_framebuffer* framebuffers)
{
    _ral_context* context_ptr = (_ral_context*) context;

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
    system_critical_section_enter(context_ptr->framebuffers_cs);
    {
        ral_context_callback_framebuffers_deleted_callback_arg callback_arg;

        for (uint32_t n_framebuffer = 0;
                      n_framebuffer < n_framebuffers;
                    ++n_framebuffer)
        {
            if (framebuffers[n_framebuffer] == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Framebuffer at index [%d] is NULL",
                                  n_framebuffer);

                continue;
            } /* if (framebuffers[n_framebuffer] == NULL) */

            ral_framebuffer_release(framebuffers[n_framebuffer]);
        } /* for (all specified framebuffer instances) */

        callback_arg.deleted_framebuffers = framebuffers;
        callback_arg.n_framebuffers       = n_framebuffers;

        /* Notify the subscribers about the event */
        system_callback_manager_call_back(context_ptr->callback_manager,
                                          RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                         &callback_arg);
    }
    system_critical_section_leave(context_ptr->framebuffers_cs);

end:
    ;
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