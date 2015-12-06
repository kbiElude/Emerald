/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_texture.h" /* TEMP TEMP TEMP */
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_texture.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_window.h"

typedef bool (*PFNRALBACKENDGETFRAMEBUFFERPROC)(void*                      backend,
                                                ral_framebuffer            framebuffer_ral,
                                                void**                     out_framebuffer_backend_ptr);
typedef void (*PFNRALBACKENDGETPROPERTYPROC)   (void*                      backend,
                                                ral_context_property       property,
                                                void*                      out_result_ptr);
typedef void (*PFNRALBACKENDRELEASEPROC)       (void*                      backend);


typedef struct _ral_context
{
    /* Rendering back-end instance:
     *
     * OpenGL context:    raGL_backend instance
     * OpenGL ES context: raGL_backend instance
     **/
    void*                              backend; 
    ral_backend_type                   backend_type;
    PFNRALBACKENDGETFRAMEBUFFERPROC    pfn_backend_get_framebuffer_proc;
    PFNRALBACKENDGETPROPERTYPROC       pfn_backend_get_property_proc;
    PFNRALBACKENDRELEASEPROC           pfn_backend_release_proc;

    system_callback_manager callback_manager;

    system_resizable_vector buffers; /* owns ral_buffer instances */
    system_critical_section buffers_cs;
    uint32_t                n_buffers_created;

    system_resizable_vector framebuffers; /* owns ral_framebuffer instances */
    system_critical_section framebuffers_cs;
    uint32_t                n_framebuffers_created;

    system_resizable_vector samplers; /* owns ral_sampler instances */
    system_critical_section samplers_cs;
    uint32_t                n_samplers_created;

    system_resizable_vector textures;                 /* owns ral_texture instances */
    system_hash64map        textures_by_filename_map; /* does NOT own the ral_texture values */
    system_hash64map        textures_by_name_map;     /* does NOT own the ral_texture values */
    system_critical_section textures_cs;
    uint32_t                n_textures_created;

    system_hashed_ansi_string name;
    ogl_rendering_handler     rendering_handler;
    system_window             window;

    REFCOUNT_INSERT_VARIABLES;


    _ral_context(system_hashed_ansi_string in_name,
                 system_window             in_window)
    {
        backend                  = NULL;
        backend_type             = RAL_BACKEND_TYPE_UNKNOWN;
        buffers                  = system_resizable_vector_create(sizeof(ral_buffer) );
        buffers_cs               = system_critical_section_create();
        callback_manager         = system_callback_manager_create((_callback_id) RAL_CONTEXT_CALLBACK_ID_COUNT);
        framebuffers             = system_resizable_vector_create(sizeof(ral_framebuffer) );
        framebuffers_cs          = system_critical_section_create();
        n_buffers_created        = 0;
        n_framebuffers_created   = 0;
        n_samplers_created       = 0;
        n_textures_created       = 0;
        name                     = in_name;
        rendering_handler        = NULL;
        samplers                 = system_resizable_vector_create(sizeof(ral_sampler) );
        samplers_cs              = system_critical_section_create();
        textures                 = system_resizable_vector_create(sizeof(ral_texture) );
        textures_by_filename_map = system_hash64map_create       (sizeof(ral_texture) );
        textures_by_name_map     = system_hash64map_create       (sizeof(ral_texture) );
        textures_cs              = system_critical_section_create();
        window                   = in_window;

        pfn_backend_get_property_proc = NULL;

        system_window_get_property(in_window,
                                   SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                  &rendering_handler);

        ASSERT_DEBUG_SYNC(buffers != NULL,
                          "Could not create the buffers vector.");
        ASSERT_DEBUG_SYNC(callback_manager != NULL,
                          "Could not create a callback manager instance");
        ASSERT_DEBUG_SYNC(framebuffers != NULL,
                          "Could not create the framebuffers vector.");
        ASSERT_DEBUG_SYNC(samplers != NULL,
                          "Could not create the samplers vector.");
        ASSERT_DEBUG_SYNC(textures != NULL,
                          "Could not create the textures vector.");
        ASSERT_DEBUG_SYNC(textures_by_filename_map != NULL,
                          "Could not create the filename->texture map");
        ASSERT_DEBUG_SYNC(textures_by_name_map != NULL,
                          "Could not create the name->texture map.");
    }

    ~_ral_context()
    {
        if (buffers != NULL)
        {
            /* Buffer instances should have been released by _ral_context_release().. */
            uint32_t n_buffers = 0;

            system_resizable_vector_get_property(buffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_buffers);

            ASSERT_DEBUG_SYNC(n_buffers == 0,
                              "Memory leak detected");

            system_resizable_vector_release(buffers);
            buffers = NULL;
        } /* if (buffers != NULL) */

        if (buffers_cs != NULL)
        {
            system_critical_section_release(buffers_cs);

            buffers_cs = NULL;
        } /* if (buffers_cs != NULL) */

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

        if (textures != NULL)
        {
            /* Texture instances should have been released by _ral_context_release().. */
            uint32_t n_textures = 0;

            system_resizable_vector_get_property(textures,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_textures);

            ASSERT_DEBUG_SYNC(n_textures == 0,
                              "Memory leak detected");

            system_resizable_vector_release(textures);
            textures = NULL;
        } /* if (textures != NULL) */

        if (textures_by_filename_map != NULL)
        {
            system_hash64map_release(textures_by_filename_map);

            textures_by_filename_map = NULL;
        } /* if (textures_by_filename_map != NULL) */

        if (textures_by_name_map != NULL)
        {
            system_hash64map_release(textures_by_name_map);

            textures_by_name_map = NULL;
        } /* if (textures_by_name_map != NULL) */

        if (textures_cs != NULL)
        {
            system_critical_section_release(textures_cs);

            textures_cs = NULL;
        } /* if (textures_cs != NULL) */


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

/* TODO */
PRIVATE void _ral_context_add_textures_to_texture_hashmaps(_ral_context* context_ptr,
                                                           uint32_t      n_textures,
                                                           ral_texture*  new_textures)
{
    system_critical_section_enter(context_ptr->textures_cs);
    {
        for (uint32_t n_texture = 0;
                      n_texture < n_textures;
                    ++n_texture)
        {
            system_hashed_ansi_string image_filename      = NULL;
            system_hash64             image_filename_hash = -1;
            system_hashed_ansi_string image_name          = NULL;
            system_hash64             image_name_hash     = -1;

            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &image_filename);
            ral_texture_get_property(new_textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_NAME,
                                    &image_name);

            if (image_filename != NULL)
            {
                ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(image_filename) > 0,
                                  "Invalid gfx_image filename property value.");

                image_filename_hash = system_hashed_ansi_string_get_hash(image_filename);

                ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                             image_filename_hash),
                                   "A ral_texture instance already exists for filename [%s]",
                                   system_hashed_ansi_string_get_buffer(image_filename) );

                system_hash64map_insert(context_ptr->textures_by_filename_map,
                                        image_filename_hash,
                                        new_textures[n_texture],
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            } /* if (image_filename != NULL) */

            ASSERT_DEBUG_SYNC(image_name                                       != NULL &&
                              system_hashed_ansi_string_get_length(image_name) > 0,
                              "Invalid gfx_image name property value.");

            image_name_hash = system_hashed_ansi_string_get_hash(image_name);

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(context_ptr->textures_by_name_map,
                                                         image_name_hash),
                              "A ral_texture instance already exists for name [%s]",
                              system_hashed_ansi_string_get_buffer(image_name) );

            system_hash64map_insert(context_ptr->textures_by_name_map,
                                    image_name_hash,
                                    new_textures[n_texture],
                                    NULL,  /* on_remove_callback          */
                                    NULL); /* on_remove_callback_user_arg */
        } /* for (all textures) */
    }
    system_critical_section_leave(context_ptr->textures_cs);
}

/** TODO */
PRIVATE bool _ral_context_delete_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                n_objects,
                                         void**                  object_ptrs)
{
    system_critical_section cs            = NULL;
    system_resizable_vector object_vector = NULL;
    bool                    result        = false;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            cs            = context_ptr->buffers_cs;
            object_vector = context_ptr->buffers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            cs            = context_ptr->framebuffers_cs;
            object_vector = context_ptr->framebuffers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            cs            = context_ptr->samplers_cs;
            object_vector = context_ptr->samplers;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            cs            = context_ptr->textures_cs;
            object_vector = context_ptr->textures;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");

            goto end;
        }
    } /* switch (object_type) */

    /* Notify the subscribers about the event */
    ral_context_callback_objects_deleted_callback_arg callback_arg;

    callback_arg.deleted_objects = object_ptrs;
    callback_arg.object_type     = object_type;
    callback_arg.n_objects       = n_objects;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
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

    /* Release the objects */
    system_critical_section_enter(cs);
    {
        for (uint32_t n_object = 0;
                      n_object < n_objects;
                    ++n_object)
        {
            uint32_t object_vector_index = ITEM_NOT_FOUND;

            if (object_ptrs[n_object] == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Object at index [%d] is NULL",
                                  n_object);

                continue;
            } /* if (object_ptrs[n_object] == NULL) */

            object_vector_index = system_resizable_vector_find(object_vector,
                                                               object_ptrs[n_object]);

            if (object_vector_index != ITEM_NOT_FOUND)
            {
                system_resizable_vector_delete_element_at(object_vector,
                                                          object_vector_index);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Released object is not tracked by RAL");
            }

            switch (object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      ral_buffer_release     ( (ral_buffer&)      object_ptrs[n_object]); break;
                case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) object_ptrs[n_object]); break;
                case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     object_ptrs[n_object]); break;
                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:     ral_texture_release    ( (ral_texture&)     object_ptrs[n_object]); break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL context object type");

                    goto end;
                }
            } /* switch (object_type) */

            object_ptrs[n_object] = NULL;
        } /* for (all specified framebuffer instances) */
    }
    system_critical_section_leave(cs);

    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _ral_context_delete_textures_from_texture_hashmaps(_ral_context* context_ptr,
                                                                uint32_t      n_textures,
                                                                ral_texture*  textures)
{
    system_critical_section_enter(context_ptr->textures_cs);
    {
        for (uint32_t n_texture = 0;
                      n_texture < n_textures;
                    ++n_texture)
        {
            system_hashed_ansi_string texture_filename      = NULL;
            system_hash64             texture_filename_hash = -1;
            system_hashed_ansi_string texture_name          = NULL;
            system_hash64             texture_name_hash     = -1;

            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_FILENAME,
                                    &texture_filename);
            ral_texture_get_property(textures[n_texture],
                                     RAL_TEXTURE_PROPERTY_NAME,
                                    &texture_name);

            texture_filename_hash = system_hashed_ansi_string_get_hash(texture_filename);
            texture_name_hash     = system_hashed_ansi_string_get_hash(texture_name);

            if (texture_filename != NULL)
            {
                ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_filename_map,
                                                            texture_filename_hash),
                                  "Texture with filename [%s] is not stored in the filename->texture hashmap.",
                                  system_hashed_ansi_string_get_buffer(texture_filename) );

                system_hash64map_remove(context_ptr->textures_by_filename_map,
                                        texture_filename_hash);
            } /* if (texture_filename != NULL) */

            ASSERT_DEBUG_SYNC(system_hash64map_contains(context_ptr->textures_by_name_map,
                                                        texture_name_hash),
                              "Texture [%s] is not stored in the name->texture hashmap.",
                              system_hashed_ansi_string_get_buffer(texture_name) );

            system_hash64map_remove(context_ptr->textures_by_name_map,
                                    texture_name_hash);
        } /* for (all specified textures) */
    }
    system_critical_section_leave(context_ptr->textures_cs);
}

/** TODO */
PRIVATE void _ral_context_release(void* context)
{
    _ral_context* context_ptr = (_ral_context*) context;

    /* Release the back-end */
    if (context_ptr->backend                  != NULL &&
        context_ptr->pfn_backend_release_proc != NULL)
    {
        context_ptr->pfn_backend_release_proc(context_ptr->backend);

        context_ptr->backend = NULL;
    }

    /* Detect any leaking objects */
    uint32_t n_buffers      = 0;
    uint32_t n_framebuffers = 0;
    uint32_t n_samplers     = 0;
    uint32_t n_textures     = 0;

    system_resizable_vector_get_property(context_ptr->buffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_buffers);
    system_resizable_vector_get_property(context_ptr->framebuffers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_framebuffers);
    system_resizable_vector_get_property(context_ptr->samplers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_samplers);
    system_resizable_vector_get_property(context_ptr->textures,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_textures);

    ASSERT_DEBUG_SYNC(n_buffers == 0,
                      "Buffer object leak detected");
    ASSERT_DEBUG_SYNC(n_framebuffers == 0,
                      "Framebuffer object leak detected");
    ASSERT_DEBUG_SYNC(n_samplers == 0,
                      "Sampler object leak detected");
    ASSERT_DEBUG_SYNC(n_textures == 0,
                      "Texture object leak detected");
}

/** Please see header for specification */
PUBLIC ral_context ral_context_create(system_hashed_ansi_string name,
                                      system_window             window)
{
    _ral_context* new_context_ptr = new (std::nothrow) _ral_context(name,
                                                                    window);

    ASSERT_ALWAYS_SYNC(new_context_ptr != NULL,
                       "Out of memory");
    if (new_context_ptr != NULL)
    {
        /* Instantiate the rendering back-end */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_BACKEND_TYPE,
                                  &backend_type);

        switch (backend_type)
        {
            case RAL_BACKEND_TYPE_ES:
            case RAL_BACKEND_TYPE_GL:
            {
                new_context_ptr->backend_type                         = backend_type;
                new_context_ptr->backend                              = (void*) raGL_backend_create( (ral_context) new_context_ptr,
                                                                                                    name,
                                                                                                    backend_type);
                new_context_ptr->pfn_backend_get_framebuffer_proc     = raGL_backend_get_framebuffer;
                new_context_ptr->pfn_backend_get_property_proc        = raGL_backend_get_property;
                new_context_ptr->pfn_backend_release_proc             = raGL_backend_release;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unsupported backend type requested.");
            }
        } /* switch(backend_type) */

        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_context_ptr,
                                                       _ral_context_release,
                                                       OBJECT_TYPE_RAL_CONTEXT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Contexts\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_context_ptr != NULL) */

    return (ral_context) new_context_ptr;
}

/** TODO */
PRIVATE bool _ral_context_create_objects(_ral_context*           context_ptr,
                                         ral_context_object_type object_type,
                                         uint32_t                n_objects,
                                         void**                  object_create_info_ptrs,
                                         void**                  out_result_object_ptrs)
{
    uint32_t*               object_counter_ptr    = NULL;
    system_critical_section object_storage_cs     = NULL;
    system_resizable_vector object_storage_vector = NULL;
    const char*             object_type_name      = NULL;
    bool                    result                = false;
    void**                  result_objects_ptr    = NULL;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            object_counter_ptr    = &context_ptr->n_buffers_created;
            object_storage_cs     =  context_ptr->buffers_cs;
            object_storage_vector =  context_ptr->buffers;
            object_type_name      = "RAL Buffer [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
            object_storage_cs     =  context_ptr->framebuffers_cs;
            object_storage_vector =  context_ptr->framebuffers;
            object_type_name      = "RAL Framebuffer [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            object_counter_ptr    = &context_ptr->n_framebuffers_created;
            object_storage_cs     =  context_ptr->samplers_cs;
            object_storage_vector =  context_ptr->samplers;
            object_type_name      = "RAL Sampler [%d]";

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
        {
            object_counter_ptr    = &context_ptr->n_textures_created;
            object_storage_cs     =  context_ptr->textures_cs;
            object_storage_vector =  context_ptr->textures;
            object_type_name      = "RAL Texture [%d]";

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
            case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
            {
                result_objects_ptr[n_object] = ral_buffer_create(system_hashed_ansi_string_create(temp),
                                                                 (const ral_buffer_create_info*) (object_create_info_ptrs + n_object) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
            {
                result_objects_ptr[n_object] = ral_framebuffer_create( (ral_context) context_ptr,
                                                                      system_hashed_ansi_string_create(temp) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                result_objects_ptr[n_object] = ral_sampler_create(system_hashed_ansi_string_create(temp),
                                                                  (const ral_sampler_create_info*) (object_create_info_ptrs + n_object) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
            {
                result_objects_ptr[n_object] = ral_texture_create((ral_context) context_ptr,
                                                                  system_hashed_ansi_string_create(temp),
                                                                  (const ral_texture_create_info*) (object_create_info_ptrs + n_object) );

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
            {
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                result_objects_ptr[n_object] = ral_texture_create_from_file_name((ral_context) context_ptr,
                                                                                 system_hashed_ansi_string_create(temp),
                                                                                 (system_hashed_ansi_string) (object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
            {
                /* NOTE: We may need the client app to specify the usage pattern in the future */
                result_objects_ptr[n_object] = ral_texture_create_from_gfx_image((ral_context) context_ptr,
                                                                                 system_hashed_ansi_string_create(temp),
                                                                                 (gfx_image) (object_create_info_ptrs + n_object),
                                                                                 RAL_TEXTURE_USAGE_IMAGE_LOAD_OPS_BIT | RAL_TEXTURE_USAGE_SAMPLED_BIT);

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
    ral_context_callback_objects_created_callback_arg callback_arg;

    callback_arg.created_objects = result_objects_ptr;
    callback_arg.object_type     = object_type;
    callback_arg.n_objects       = n_objects;

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                             &callback_arg);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
        {
            callback_arg.object_type = RAL_CONTEXT_OBJECT_TYPE_TEXTURE;

            system_callback_manager_call_back(context_ptr->callback_manager,
                                              RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
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
        } /* for (all objects) */
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
                        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      ral_buffer_release     ( (ral_buffer&)      result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: ral_framebuffer_release( (ral_framebuffer&) result_objects_ptr[n_object]); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     ral_sampler_release    ( (ral_sampler&)     result_objects_ptr[n_object]); break;

                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME:
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_GFX_IMAGE:
                        {
                            ral_texture_release( (ral_texture&) result_objects_ptr[n_object]);

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized RAL context object type");
                        }
                    } /* switch (object_type) */

                    result_objects_ptr[n_object] = NULL;
                } /* if (result_objects_ptr[n_object] != NULL) */
            } /* for (all potentially created RAL objects) */
        } /* if (result_objects_ptr != NULL) */
    } /* if (!result) */

    if (result_objects_ptr != NULL)
    {
        delete [] result_objects_ptr;

        result_objects_ptr = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_create_buffers(ral_context                   context,
                                       uint32_t                      n_buffers,
                                       const ral_buffer_create_info* buffer_create_info_ptr,
                                       ral_buffer*                   out_result_buffers_ptr)
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

    if (n_buffers == 0)
    {
        goto end;
    }

    if (buffer_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input buffer create info array is NULL");

        goto end;
    }

    if (out_result_buffers_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                         n_buffers,
                                         (void**) buffer_create_info_ptr,
                                         (void**) out_result_buffers_ptr);

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
PUBLIC bool ral_context_create_samplers(ral_context              context,
                                        uint32_t                 n_create_info_items,
                                        ral_sampler_create_info* create_info_ptrs,
                                        ral_sampler*             out_result_sampler_ptrs)
{
    bool          result      = false;
    _ral_context* context_ptr = (_ral_context*) context;

    /* Sanity checks */
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (n_create_info_items == 0)
    {
        result = true;

        goto end;
    }

    if (create_info_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input create_info_ptrs is NULL");

        goto end;
    }

    if (out_result_sampler_ptrs == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable out_result_sampler_ptrs is NULL");

        goto end;
    }

    /* Check if the samplers we already have match the requested sampler
     *
     * TODO: We really need a hash-map for this!
     * TODO: Distribute to multiple threads?
     */
    LOG_ERROR("Performance warning: Slow path in ral_context_get_sampler_by_create_info()");

    system_critical_section_enter(context_ptr->samplers_cs);
    {
        for (uint32_t n_current_create_info = 0;
                      n_current_create_info < n_create_info_items;
                    ++n_current_create_info)
        {
            ral_sampler_create_info* current_create_info_ptr = create_info_ptrs + n_current_create_info;
            uint32_t                 n_samplers              = 0;
            ral_sampler              result_sampler          = NULL;

            if (current_create_info_ptr == NULL)
            {
                ASSERT_DEBUG_SYNC(current_create_info_ptr != NULL,
                                  "One of the create info items is NULL");

                goto end;
            }

            system_resizable_vector_get_property(context_ptr->samplers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_samplers);

            for (uint32_t n_sampler = 0;
                          n_sampler < n_samplers;
                        ++n_sampler)
            {
                ral_sampler current_sampler = NULL;

                if (!system_resizable_vector_get_element_at(context_ptr->samplers,
                                                            n_sampler,
                                                           &current_sampler) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve a ral_sampler instance at index [%d]",
                                      n_sampler);

                    continue;
                }

                if (ral_sampler_is_equal_to_create_info(current_sampler,
                                                        current_create_info_ptr) )
                {
                    /* Found a match */
                    result_sampler = current_sampler;

                    break;
                }
            } /* for (all samplers registered) */

            if (result_sampler == NULL)
            {
                /* No luck - we need a new instance. */
                static int n_samplers_created = 0;
                char       name_buffer[64];

                snprintf(name_buffer,
                         sizeof(name_buffer),
                         "Sampler [%d]",
                         n_samplers_created++);

                result_sampler = ral_sampler_create(system_hashed_ansi_string_create(name_buffer),
                                                    current_create_info_ptr);

                system_resizable_vector_push(context_ptr->samplers,
                                             result_sampler);
            } /* if (result_sampler == NULL) */

            if (result_sampler == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not obtain a ral_sampler instance.");

                goto end;
            }

            out_result_sampler_ptrs[n_current_create_info] = result_sampler;
        } /* for (all create info items) */
    }
    system_critical_section_leave(context_ptr->samplers_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_create_textures(ral_context                    context,
                                        uint32_t                       n_textures,
                                        const ral_texture_create_info* texture_create_info_ptr,
                                        ral_texture*                   out_result_textures_ptr)
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

    if (n_textures == 0)
    {
        goto end;
    }

    if (texture_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture create info array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                         n_textures,
                                         (void**) texture_create_info_ptr,
                                         (void**) out_result_textures_ptr);

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_textures,
                                                      out_result_textures_ptr);
    } /* if (result != NULL) */
end:
    return result;
}

/** TODO */
PUBLIC bool ral_context_create_textures_from_file_names(ral_context                      context,
                                                        uint32_t                         n_file_names,
                                                        const system_hashed_ansi_string* file_names_ptr,
                                                        ral_texture*                     out_result_textures_ptr)
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

    if (n_file_names == 0)
    {
        goto end;
    }

    if (file_names_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input file name array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME,
                                         n_file_names,
                                         (void**) file_names_ptr,
                                         (void**) out_result_textures_ptr);

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_file_names,
                                                      out_result_textures_ptr);

    } /* if (result != NULL) */
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_create_textures_from_gfx_images(ral_context      context,
                                                        uint32_t         n_images,
                                                        const gfx_image* images,
                                                        ral_texture*     out_result_textures_ptr)
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

    if (n_images == 0)
    {
        goto end;
    }

    if (images == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input image array is NULL");

        goto end;
    }

    if (out_result_textures_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    result = _ral_context_create_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE_FROM_FILE_NAME,
                                         n_images,
                                         (void**) images,
                                         (void**) out_result_textures_ptr);

    if (result)
    {
        _ral_context_add_textures_to_texture_hashmaps(context_ptr,
                                                      n_images,
                                                      out_result_textures_ptr);

    } /* if (result != NULL) */
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_context_delete_buffers(ral_context context,
                                       uint32_t    n_buffers,
                                       ral_buffer* buffers)
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

    if (n_buffers == 0)
    {
        goto end;
    }

    if (buffers == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input uffers instance is NULL");

        goto end;
    }

    /* Release the buffers */
    result = _ral_context_delete_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                         n_buffers,
                                         (void**) buffers);

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
PUBLIC bool ral_context_delete_textures(ral_context  context,
                                        uint32_t     n_textures,
                                        ral_texture* textures)
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

    if (n_textures == 0)
    {
        goto end;
    }

    if (textures == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture array is NULL");

        goto end;
    }

    /* Release the textures */
    if (result)
    {
        _ral_context_delete_textures_from_texture_hashmaps(context_ptr,
                                                           n_textures,
                                                           textures);
    } /* if (result) */

    result = _ral_context_delete_objects(context_ptr,
                                         RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                         n_textures,
                                         (void**) textures);

end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API raGL_buffer ral_context_get_buffer_gl(ral_context context,
                                                         ral_buffer  buffer)
{
    raGL_buffer   buffer_raGL = NULL;
    _ral_context* context_ptr = (_ral_context*) context;

    raGL_backend_get_buffer(context_ptr->backend,
                            buffer,
                  (void**) &buffer_raGL);

    return buffer_raGL;
}

/** TODO */
PUBLIC EMERALD_API raGL_framebuffer ral_context_get_framebuffer_gl(ral_context     context,
                                                                   ral_framebuffer framebuffer)
{
    _ral_context*    context_ptr      = (_ral_context*) context;
    raGL_framebuffer framebuffer_raGL = NULL;

    raGL_backend_get_framebuffer(context_ptr->backend,
                                 framebuffer,
                       (void**) &framebuffer_raGL);

    return framebuffer_raGL;
}

/** TODO */
PUBLIC EMERALD_API ogl_context ral_context_get_gl_context(ral_context context)
{
    raGL_backend backend         = (raGL_backend) ((_ral_context*) context)->backend;
    ogl_context  backend_context = NULL;

    raGL_backend_get_property(backend,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &backend_context);

    return backend_context;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_context_get_property(ral_context          context,
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
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            *(raGL_backend*) out_result_ptr = (raGL_backend) context_ptr->backend;

            break;
        }

        case RAL_CONTEXT_PROPERTY_BACKEND_TYPE:
        {
            *(ral_backend_type*) out_result_ptr = context_ptr->backend_type;

            break;
        }

        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT:
        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_SIZE:
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

/** TODO */
PUBLIC EMERALD_API raGL_sampler ral_context_get_sampler_gl(ral_context context,
                                                           ral_sampler sampler)
{
    raGL_sampler  sampler_raGL = NULL;
    _ral_context* context_ptr  = (_ral_context*) context;

    raGL_backend_get_sampler(context_ptr->backend,
                             sampler,
                   (void**) &sampler_raGL);

    return sampler_raGL;
}

/** Please see header for specification */
PUBLIC ral_texture ral_context_get_texture_by_file_name(ral_context               context,
                                                        system_hashed_ansi_string file_name)
{
    _ral_context* context_ptr = (_ral_context*) context;
    ral_texture   result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (file_name == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input file name is NULL");

        goto end;
    }

    /* Is there a texture associated with the specified file name? */
    system_critical_section_enter(context_ptr->textures_cs);
    {
        system_hash64map_get(context_ptr->textures_by_filename_map,
                             system_hashed_ansi_string_get_hash(file_name),
                            &result);
    }
    system_critical_section_leave(context_ptr->textures_cs);

end:
    return result;
}

/** Please see header for specification */
PUBLIC ral_texture ral_context_get_texture_by_name(ral_context               context,
                                                   system_hashed_ansi_string name)
{
    _ral_context* context_ptr = (_ral_context*) context;
    ral_texture   result      = NULL;

    /* Sanity checks..*/
    if (context == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_context instance is NULL");

        goto end;
    }

    if (name == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture name is NULL");

        goto end;
    }

    /* Is there a texture associated with the specified name? */
    system_critical_section_enter(context_ptr->textures_cs);
    {
        system_hash64map_get(context_ptr->textures_by_name_map,
                             system_hashed_ansi_string_get_hash(name),
                            &result);
    }
    system_critical_section_leave(context_ptr->textures_cs);

end:
    return result;
}

/** TODO */
PUBLIC EMERALD_API raGL_texture ral_context_get_texture_gl(ral_context context,
                                                           ral_texture texture)
{
    _ral_context* context_ptr  = (_ral_context*) context;
    raGL_texture  texture_raGL = NULL;

    raGL_backend_get_texture(context_ptr->backend,
                             texture,
                   (void**) &texture_raGL);

    return texture_raGL;
}

PUBLIC GLuint ral_context_get_texture_gl_id(ral_context context,
                                            ral_texture texture)
{
    _ral_context* context_ptr  = (_ral_context*) context;
    GLuint        result       = 0;
    raGL_texture  texture_raGL = NULL;

    raGL_backend_get_texture (context_ptr->backend,
                              texture,
                    (void**) &texture_raGL);
    raGL_texture_get_property(texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &result);

    return result;
}

/** Please see header for specification */
PUBLIC void ral_context_init(ral_context context)
{
    _ral_context* context_ptr = (_ral_context*) context;

    switch (context_ptr->backend_type)
    {
        case RAL_BACKEND_TYPE_ES:
        case RAL_BACKEND_TYPE_GL:
        {
            raGL_backend_init( (raGL_backend) context_ptr->backend);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported backend type.");
        }
    } /* switch (context_ptr->backend_type) */
}

/** TODO */
PUBLIC void ral_context_set_property(ral_context          context,
                                     ral_context_property property,
                                     const void*          data)
{
    _ral_context* context_ptr = (_ral_context*) context;

    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND:
        {
            ASSERT_DEBUG_SYNC(context_ptr->backend == NULL,
                              "Backend is not NULL");

            context_ptr->backend = *(void**) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_property value.");
        }
    } /* switch (property) */
}