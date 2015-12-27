/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_texture.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_buffers.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_textures.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

#define ROOT_WINDOW_ES_NAME "Root window ES"
#define ROOT_WINDOW_GL_NAME "Root window GL"


typedef struct _raGL_backend
{
    system_hash64map        buffers_map;      /* maps ral_buffer to raGL_buffer instance; owns the mapped raGL_buffer instances */
    system_critical_section buffers_map_cs;
    system_hash64map        framebuffers_map; /* maps ral_framebuffer to raGL_framebuffer instance; owns the mapped raGL_framebuffer instances */
    system_critical_section framebuffers_map_cs;
    system_hash64map        samplers_map;     /* maps ral_sampler to raGL_sampler instance; owns the mapped raGL_sampler instances */
    system_critical_section samplers_map_cs;

    system_hash64map        texture_id_to_raGL_texture_map; /* maps GLid to raGL_texture instance; does NOT own the mapepd raGL_texture instances; lock textures_map_cs before usage. */
    system_hash64map        textures_map;                   /* maps ral_texture to raGL_texture instance; owns the mapped raGL_texture instances */
    system_critical_section textures_map_cs;

    raGL_buffers  buffers;
    raGL_textures textures;

    ral_backend_type          backend_type;
    ogl_context               context_gl;
    ral_context               context_ral;
    system_hashed_ansi_string name;

    uint32_t max_framebuffer_color_attachments;

    _raGL_backend(ral_context               in_owner_context,
                  system_hashed_ansi_string in_name,
                  ral_backend_type          in_backend_type)
    {
        backend_type                      = in_backend_type;
        buffers                           = NULL;
        buffers_map                       = system_hash64map_create       (sizeof(raGL_buffer) );
        buffers_map_cs                    = system_critical_section_create();
        context_gl                        = NULL;
        context_ral                       = in_owner_context;
        framebuffers_map                  = system_hash64map_create       (sizeof(raGL_framebuffer) );
        framebuffers_map_cs               = system_critical_section_create();
        max_framebuffer_color_attachments = 0;
        name                              = in_name;
        samplers_map                      = system_hash64map_create       (sizeof(raGL_sampler) );
        samplers_map_cs                   = system_critical_section_create();
        textures                          = NULL;
        texture_id_to_raGL_texture_map    = system_hash64map_create       (sizeof(raGL_texture) );
        textures_map                      = system_hash64map_create       (sizeof(raGL_texture) );
        textures_map_cs                   = system_critical_section_create();

        ASSERT_DEBUG_SYNC(buffers_map != NULL,
                          "Could not create the buffers map");
        ASSERT_DEBUG_SYNC(framebuffers_map != NULL,
                          "Could not create the framebuffers map");
        ASSERT_DEBUG_SYNC(samplers_map != NULL,
                          "Could not create the samplers map");
    }

    ~_raGL_backend();
} _raGL_backend;

typedef struct
{

    _raGL_backend*                                           backend_ptr;
    const ral_context_callback_objects_created_callback_arg* ral_callback_arg_ptr;

} _raGL_backend_on_objects_created_rendering_callback_arg;

typedef struct
{
    _raGL_backend*                                           backend_ptr;
    const ral_context_callback_objects_deleted_callback_arg* ral_callback_arg_ptr;

} _raGL_backend_on_objects_deleted_rendering_callback_arg;


/* Forward declarations */
PRIVATE void _raGL_backend_cache_limits                                   (_raGL_backend*          backend_ptr);
PRIVATE bool _raGL_backend_get_object                                     (void*                   backend,
                                                                           ral_context_object_type object_type,
                                                                           void*                   object_ral,
                                                                           void**                  out_result_ptr);
PRIVATE void _raGL_backend_on_buffer_client_memory_sourced_update_request (const void*             callback_arg_data,
                                                                           void*                   backend);
PRIVATE void _raGL_backend_on_objects_created                             (const void*             callback_arg_data,
                                                                           void*                   backend);
PRIVATE void _raGL_backend_on_objects_created_rendering_callback          (ogl_context             context,
                                                                           void*                   callback_arg);
PRIVATE void _raGL_backend_on_objects_deleted                             (const void*             callback_arg,
                                                                           void*                   backend);
PRIVATE void _raGL_backend_on_objects_deleted_rendering_callback          (ogl_context             context,
                                                                           void*                   callback_arg);
PRIVATE void _raGL_backend_on_texture_client_memory_sourced_update_request(const void*             callback_arg_data,
                                                                           void*                   backend);
PRIVATE void _raGL_backend_on_texture_mipmap_generation_request           (const void*             callback_arg_data,
                                                                           void*                   backend);
PRIVATE void _raGL_backend_release_raGL_object                            (_raGL_backend*          backend_ptr,
                                                                           ral_context_object_type object_type,
                                                                           void*                   object_raGL,
                                                                           void*                   object_ral);
PRIVATE void _raGL_backend_subscribe_for_buffer_notifications             (_raGL_backend*          backend_ptr,
                                                                           ral_buffer              buffer,
                                                                           bool                    should_subscribe);
PRIVATE void _raGL_backend_subscribe_for_notifications                    (_raGL_backend*          backend_ptr,
                                                                           bool                    should_subscribe);
PRIVATE void _raGL_backend_subscribe_for_texture_notifications            (_raGL_backend*          backend_ptr,
                                                                           ral_texture             texture,
                                                                           bool                    should_subscribe);


/** TODO */
_raGL_backend::~_raGL_backend()
{
    /* Release object managers */
    ogl_context_release_managers(context_gl);

    if (buffers != NULL)
    {
        raGL_buffers_release(buffers);

        buffers = NULL;
    }

    if (textures != NULL)
    {
        raGL_textures_release(textures);

        textures = NULL;
    }

    for (uint32_t n_object_type = 0;
                  n_object_type < RAL_CONTEXT_OBJECT_TYPE_COUNT;
                ++n_object_type)
    {
        system_critical_section cs              = NULL;
        system_hash64map*       objects_map_ptr = NULL;

        switch (n_object_type)
        {
            case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
            {
                cs              =  buffers_map_cs;
                objects_map_ptr = &buffers_map;

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
            {
                cs              =  framebuffers_map_cs;
                objects_map_ptr = &framebuffers_map;

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                cs              =  samplers_map_cs;
                objects_map_ptr = &samplers_map;

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
            {
                cs              =  textures_map_cs;
                objects_map_ptr = &textures_map;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized raGL_backend_object_type value.");

                continue;
            }
        } /* switch (n_object_type) */

        system_critical_section_enter(cs);
        {
            if (*objects_map_ptr != NULL)
            {
                uint32_t n_objects = 0;

                system_hash64map_get_property(*objects_map_ptr,
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_objects);

                for (uint32_t n_object = 0;
                              n_object < n_objects;
                            ++n_object)
                {
                    void* current_object = NULL;

                    if (!system_hash64map_get_element_at(*objects_map_ptr,
                                                         n_object,
                                                        &current_object,
                                                         NULL) ) /* result_hash_ptr */
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve raGL object instance");

                        continue;
                    }

                    switch (n_object_type)
                    {
                        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      raGL_buffer_release     ( (raGL_buffer&)      current_object); break;
                        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: raGL_framebuffer_release( (raGL_framebuffer&) current_object); break;
                        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     raGL_sampler_release    ( (raGL_sampler&)     current_object); break;
                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:     raGL_texture_release    ( (raGL_texture&)     current_object); break;

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized raGL_backend_object_type value.");
                        }
                    } /* switch (n_object_type) */

                    current_object = NULL;
                } /* for (all object map items) */

                system_hash64map_release(*objects_map_ptr);
                *objects_map_ptr = NULL;
            } /* if (objects_map != NULL) */

            system_critical_section_leave(cs);
        } /* for (all raGL object types) */
    }

    if (context_gl != NULL)
    {
        ogl_context_release(context_gl);

        context_gl = NULL;
    } /* if (context_gl != NULL) */

    if (texture_id_to_raGL_texture_map != NULL)
    {
        system_hash64map_release(texture_id_to_raGL_texture_map);

        texture_id_to_raGL_texture_map = NULL;
    } /* if (texture_id_to_raGL_texture_map != NULL) */

    /* Unsubscribe from notifications */
    _raGL_backend_subscribe_for_notifications(this,
                                              false); /* should_subscribe */

    /* Release the critical sections */
    system_critical_section_release(buffers_map_cs);
    buffers_map_cs = NULL;

    system_critical_section_release(framebuffers_map_cs);
    framebuffers_map_cs = NULL;

    system_critical_section_release(samplers_map_cs);
    samplers_map_cs = NULL;

    system_critical_section_release(textures_map_cs);
    textures_map_cs = NULL;
}

/** TODO */
PRIVATE void _raGL_backend_create_root_window(ral_backend_type backend_type)
{
    system_window             root_window                   = NULL;
    system_hashed_ansi_string root_window_name              = (backend_type == RAL_BACKEND_TYPE_ES) ? system_hashed_ansi_string_create(ROOT_WINDOW_ES_NAME)
                                                                                                    : system_hashed_ansi_string_create(ROOT_WINDOW_GL_NAME);
    system_pixel_format       root_window_pf                = NULL;
    ogl_rendering_handler     root_window_rendering_handler = NULL;
    static const int          root_window_x1y1x2y2[] =
    {
        0,
        0,
        64,
        64
    };

    /* Spawn a new root window. All client-created windows will share namespace with root
     * window's context.
     *
     * NOTE: Spawned window takes ownership of the pixel_format object.
     *
     * NOTE: The pixel format we use to initialize the root window uses 0 bits for the
     *       color, depth and stencil attachments. That's fine, we don't really need
     *       to render anything to the window; we only use it as a mean to share objects
     *       between contexts.
     */
    root_window_pf = system_pixel_format_create(8,  /* red_bits     */
                                                8,  /* green_bits   */
                                                8,  /* blue_bits    */
                                                0,  /* alpha_bits   */
                                                24, /* depth_bits   */
                                                1,  /* n_samples    */
                                                0); /* stencil_bits */

    root_window = system_window_create_not_fullscreen(backend_type,
                                                      root_window_x1y1x2y2,
                                                      root_window_name,
                                                      false, /* scalable      */
                                                      false, /* vsync_enabled */
                                                      false, /* visible       */
                                                      root_window_pf);

    ASSERT_DEBUG_SYNC(root_window != NULL,
                      "Root window context creation failed");

    root_window_rendering_handler = ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(root_window_name),
                                                                                                                                                        " rendering handler"),
                                                                                                NULL,  /* pfn_rendering_callback */
                                                                                                NULL); /* user_arg */

    ASSERT_DEBUG_SYNC(root_window_rendering_handler != NULL,
                      "Root window rendering handler creation failed");

    system_window_set_property(root_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &root_window_rendering_handler);

    /* The setter takes ownership of root_window, but for the sake of code maintainability,
     * retain the RH so that we can release it at the moment root_window is also released. */
    ogl_rendering_handler_retain(root_window_rendering_handler);
}

/** TODO */
PRIVATE bool _raGL_backend_get_object(void*                   backend,
                                      ral_context_object_type object_type,
                                      void*                   object_ral,
                                      void**                  out_result_ptr)
{
    _raGL_backend*          backend_ptr = (_raGL_backend*) backend;
    system_hash64map        map         = NULL;
    system_critical_section map_cs      = NULL;
    bool                    result      = false;

    /* Sanity checks */
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input backend is NULL");

        goto end;
    }

    /* If objecty_ral is NULL, return a NULL RAL equivalent */
    if (object_ral == NULL)
    {
        *out_result_ptr = NULL;
        result          = true;

        goto end;
    }

    /* Identify which critical section & map we should use to handle the query */
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            map    = backend_ptr->buffers_map;
            map_cs = backend_ptr->buffers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            map    = backend_ptr->framebuffers_map;
            map_cs = backend_ptr->framebuffers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            map    = backend_ptr->samplers_map;
            map_cs = backend_ptr->samplers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            map    = backend_ptr->textures_map;
            map_cs = backend_ptr->textures_map_cs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized object type");

            goto end;
        }
    } /* switch (object_type) */

    /* Try to find the raGL framebuffer instance */
    system_critical_section_enter(map_cs);

    if (!system_hash64map_get(map,
                              (system_hash64) object_ral,
                              out_result_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Provided RAL object handle was not recognized");

        goto end;
    }

    /* All done */
    result = true;

end:
    if (map_cs != NULL)
    {
        system_critical_section_leave(map_cs);
    }

    return result;
}

PRIVATE void _raGL_backend_get_object_vars(_raGL_backend*           backend_ptr,
                                           ral_context_object_type  object_type,
                                           system_critical_section* out_cs_ptr,
                                           system_hash64map*        out_hashmap_ptr)
{
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            *out_hashmap_ptr = backend_ptr->buffers_map;
            *out_cs_ptr      = backend_ptr->buffers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            *out_hashmap_ptr = backend_ptr->framebuffers_map;
            *out_cs_ptr      = backend_ptr->framebuffers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            *out_hashmap_ptr = backend_ptr->samplers_map;
            *out_cs_ptr      = backend_ptr->samplers_map_cs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            *out_hashmap_ptr = backend_ptr->textures_map;
            *out_cs_ptr      = backend_ptr->textures_map_cs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_backend_object_type value.");
        }
    } /* switch (object_type) */
}

/** TODO */
PRIVATE void _raGL_backend_on_buffer_client_memory_sourced_update_request(const void* callback_arg_data,
                                                                          void*       backend)
{
    _raGL_backend*                                      backend_ptr      = (_raGL_backend*)                                      backend;
    raGL_buffer                                         buffer_raGL      = NULL;
    ral_buffer_client_sourced_update_info_callback_arg* callback_arg_ptr = (ral_buffer_client_sourced_update_info_callback_arg*) callback_arg_data;

    /* Identify the raGL_buffer instance for the source ral_buffer object and forward
     * the request.
     *
     * Note that all notifications will be coming from the same ral_buffer
     * instance.*/
    system_critical_section_enter(backend_ptr->buffers_map_cs);
    {
        if (!system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) callback_arg_ptr->buffer,
                                 &buffer_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_buffer instance found for the specified ral_buffer instance.");

            goto end;
        }
    }
    system_critical_section_leave(backend_ptr->buffers_map_cs);

    raGL_buffer_update_regions_with_client_memory(buffer_raGL,
                                                  callback_arg_ptr->n_updates,
                                                  callback_arg_ptr->updates);

end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_on_objects_created(const void* callback_arg_data,
                                              void*       backend)
{
    _raGL_backend*                                           backend_ptr      = (_raGL_backend*)                                     backend;
    const ral_context_callback_objects_created_callback_arg* callback_arg_ptr = (ral_context_callback_objects_created_callback_arg*) callback_arg_data;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_data != NULL,
                      "Callback argument is NULL");

    /* If the required managers are unavailable, create them now.
     *
     * NOTE: It would be much cleaner if these were created at init() time, but
     *       it's far from simple, given how the window+context+rendering handler
     *       set-up process is performed.
     **/
    if (backend_ptr->buffers == NULL)
    {
        backend_ptr->buffers = raGL_buffers_create((raGL_backend) backend_ptr,
                                                   backend_ptr->context_gl);
    }

    if (backend_ptr->textures == NULL)
    {
        backend_ptr->textures = raGL_textures_create(backend_ptr->context_ral,
                                                     backend_ptr->context_gl);
    }

    /* Request a rendering call-back to create the buffer instances */
    _raGL_backend_on_objects_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_ptr;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* Sign up for notifications we will want to forward to the raGL instances */
    switch (rendering_callback_arg.ral_callback_arg_ptr->object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            for (uint32_t n_created_object = 0;
                          n_created_object < callback_arg_ptr->n_objects;
                        ++n_created_object)
            {
                _raGL_backend_subscribe_for_buffer_notifications(backend_ptr,
                                                                 (ral_buffer) callback_arg_ptr->created_objects[n_created_object],
                                                                 true); /* should_subscribe */
            }

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            /* No call-backs for these yet.. */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            for (uint32_t n_created_texture = 0;
                          n_created_texture < callback_arg_ptr->n_objects;
                        ++n_created_texture)
            {
                _raGL_backend_subscribe_for_texture_notifications(backend_ptr,
                                                                  (ral_texture) callback_arg_ptr->created_objects[n_created_texture],
                                                                  true); /* should_subscribe */
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");
        }
    } /* switch (rendering_callback_arg.ral_callback_arg_ptr->object_type) */
}

/** TODO */
PRIVATE void _raGL_backend_on_objects_created_rendering_callback(ogl_context context,
                                                                 void*       callback_arg)
{
    const _raGL_backend_on_objects_created_rendering_callback_arg* callback_arg_ptr        = (const _raGL_backend_on_objects_created_rendering_callback_arg*) callback_arg;
    const ogl_context_gl_entrypoints*                              entrypoints_ptr         = NULL;
    uint32_t                                                       n_objects_to_initialize = 0;
    system_hash64map                                               objects_map             = NULL;
    system_critical_section                                        objects_map_cs          = NULL;
    bool                                                           result                  = false;
    GLuint*                                                        result_object_ids_ptr   = NULL;

    ASSERT_DEBUG_SYNC(callback_arg != NULL,
                      "Callback argument is NULL");

    ogl_context_get_property(callback_arg_ptr->backend_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    if (context == NULL)
    {
        context = callback_arg_ptr->backend_ptr->context_gl;
    }

    /* Retrieve object type-specific data */
    n_objects_to_initialize = callback_arg_ptr->ral_callback_arg_ptr->n_objects;

    _raGL_backend_get_object_vars(callback_arg_ptr->backend_ptr,
                                  callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                 &objects_map_cs,
                                 &objects_map);

    /* Generate the IDs */
    result_object_ids_ptr = new (std::nothrow) GLuint[n_objects_to_initialize];

    if (result_object_ids_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    } /* if (result_fb_ids_ptr == NULL) */

    switch (callback_arg_ptr->ral_callback_arg_ptr->object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            /* IDs are associated by raGL_buffers */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            entrypoints_ptr->pGLGenFramebuffers(n_objects_to_initialize,
                                                result_object_ids_ptr);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            entrypoints_ptr->pGLGenSamplers(n_objects_to_initialize,
                                            result_object_ids_ptr);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            /* IDs are associated by raGL_textures */
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_object_type value.");

            goto end;
        }
    } /* switch (callback_arg_ptr->ral_callback_arg_ptr->object_type) */

    /* Create raGL instances for each object ID */
    for (uint32_t n_object_id = 0;
                  n_object_id < n_objects_to_initialize;
                ++n_object_id)
    {
        void* new_object = NULL;

        switch (callback_arg_ptr->ral_callback_arg_ptr->object_type)
        {
            case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
            {
                ral_buffer buffer_ral = (ral_buffer) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                raGL_buffers_allocate_buffer_memory_for_ral_buffer(callback_arg_ptr->backend_ptr->buffers,
                                                                   buffer_ral,
                                                                   (raGL_buffer*) &new_object);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
            {
                GLuint          current_object_id = result_object_ids_ptr[n_object_id];
                ral_framebuffer framebuffer_ral   = (ral_framebuffer) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                new_object = raGL_framebuffer_create(context,
                                                     current_object_id,
                                                     framebuffer_ral);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
            {
                GLuint      current_object_id = result_object_ids_ptr[n_object_id];
                ral_sampler sampler_ral       = (ral_sampler) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                new_object = raGL_sampler_create(context,
                                                 current_object_id,
                                                 sampler_ral);

                break;
            }

            case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
            {
                ral_texture texture_ral = (ral_texture) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                new_object = raGL_textures_get_texture_from_pool(callback_arg_ptr->backend_ptr->textures,
                                                                 texture_ral);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized raGL_backend_object_type value.");

                goto end;
            }
        } /* switch (callback_arg_ptr->object_type) */

        if (new_object == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Failed to create a GL back-end object.");

            goto end;
        }

        system_critical_section_enter(objects_map_cs);
        {
            ASSERT_DEBUG_SYNC(!system_hash64map_contains(objects_map,
                                                         (system_hash64) new_object),
                              "Created GL backend instance is already recognized?");

            system_hash64map_insert(objects_map,
                                    (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id],
                                    new_object,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */

            /* Texture objects are actually stored in two hash maps. */
            if (callback_arg_ptr->ral_callback_arg_ptr->object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE)
            {
                GLuint       new_texture_id   = 0;
                raGL_texture new_texture_raGL = (raGL_texture) new_object;

                raGL_texture_get_property(new_texture_raGL,
                                          RAGL_TEXTURE_PROPERTY_ID,
                                         &new_texture_id);

                ASSERT_DEBUG_SYNC(new_texture_id != 0,
                                  "New raGL_texture's GL id is 0");

                system_hash64map_insert(callback_arg_ptr->backend_ptr->texture_id_to_raGL_texture_map,
                                        (system_hash64) new_texture_id,
                                        new_object,
                                        NULL,  /* on_remove_callback          */
                                        NULL); /* on_remove_callback_user_arg */
            } /*( if (callback_arg_ptr->object_type == RAGL_BACKEND_OBJECT_TYPE_TEXTURE) */
        }
        system_critical_section_leave(objects_map_cs);
    } /* for (all generated FB ids) */

    result = true;
end:
    if (result_object_ids_ptr != NULL)
    {
        if (!result)
        {
            /* Remove those object instances which have been successfully spawned */
            for (uint32_t n_object = 0;
                          n_object < n_objects_to_initialize;
                        ++n_object)
            {
                void* object_instance = NULL;

                if (!system_hash64map_get(objects_map,
                                          (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object],
                                         &object_instance) )
                {
                    continue;
                }

                _raGL_backend_release_raGL_object(callback_arg_ptr->backend_ptr,
                                                  callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                                  object_instance,
                                                  callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object]);

                object_instance = NULL;
            } /* for (all requested objects) */
        } /* if (!result) */

        delete [] result_object_ids_ptr;

        result_object_ids_ptr = NULL;
    } /* if (result_object_ids_ptr != NULL) */

}

/** TODO */
PRIVATE void _raGL_backend_on_objects_deleted(const void* callback_arg,
                                              void*       backend)
{
    _raGL_backend*                                           backend_ptr      = (_raGL_backend*)                                           backend;
    const ral_context_callback_objects_deleted_callback_arg* callback_arg_ptr = (const ral_context_callback_objects_deleted_callback_arg*) callback_arg;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg != NULL,
                      "Callback argument is NULL");

    /* Unsubscribe from any RAL object notifications */
    switch (callback_arg_ptr->object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            for (uint32_t n_deleted_object = 0;
                          n_deleted_object < callback_arg_ptr->n_objects;
                        ++n_deleted_object)
            {
                _raGL_backend_subscribe_for_buffer_notifications(backend_ptr,
                                                                 (ral_buffer) callback_arg_ptr->deleted_objects[n_deleted_object],
                                                                 false); /* should_subscribe */
            }

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            /* No call-backs for these yet.. */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            for (uint32_t n_deleted_texture = 0;
                          n_deleted_texture < callback_arg_ptr->n_objects;
                        ++n_deleted_texture)
            {
                _raGL_backend_subscribe_for_texture_notifications(backend_ptr,
                                                                  (ral_texture) callback_arg_ptr->deleted_objects[n_deleted_texture],
                                                                  false); /* should_subscribe */
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL context object type");
        }
    } /* switch (callback_arg_ptr->object_type) */

    /* Request a rendering call-back to release the objects */
    _raGL_backend_on_objects_deleted_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_ptr;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_deleted_rendering_callback,
                                                    &rendering_callback_arg);
}

/** TODO */
PRIVATE void _raGL_backend_on_objects_deleted_rendering_callback(ogl_context context,
                                                                 void*       callback_arg)
{
    const _raGL_backend_on_objects_deleted_rendering_callback_arg* callback_arg_ptr = (const _raGL_backend_on_objects_deleted_rendering_callback_arg*) callback_arg;
    const ogl_context_gl_entrypoints*                              entrypoints_ptr  = NULL;
    system_hash64map                                               object_map       = NULL;
    system_critical_section                                        object_map_cs    = NULL;
    void*                                                          object_raGL      = NULL;

    _raGL_backend_get_object_vars(callback_arg_ptr->backend_ptr,
                                  callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                 &object_map_cs,
                                 &object_map);

    if (object_map == NULL)
    {
        /* raGL objects have already been released, ignore the request. */
        goto end;
    }

    system_critical_section_enter(object_map_cs);
    {
        /* Retrieve raGL object instances for the specified RAL objects */
        for (uint32_t n_deleted_object = 0;
                      n_deleted_object < callback_arg_ptr->ral_callback_arg_ptr->n_objects;
                    ++n_deleted_object)
        {
            if (!system_hash64map_get(object_map,
                                      (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->deleted_objects[n_deleted_object],
                                     &object_raGL) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "No raGL instance found for the specified RAL object.");

                continue;
            }

            /* Remove the object from any relevant hash-maps we maintain */
            system_hash64map_remove(object_map,
                                    (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->deleted_objects[n_deleted_object]);

            if (callback_arg_ptr->ral_callback_arg_ptr->object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE)
            {
                GLuint object_id = 0;

                raGL_texture_get_property((raGL_texture) object_raGL,
                                          RAGL_TEXTURE_PROPERTY_ID,
                                         &object_id);

                ASSERT_DEBUG_SYNC(object_id != 0,
                                  "raGL_texture's GL id is 0");

                system_hash64map_remove(callback_arg_ptr->backend_ptr->texture_id_to_raGL_texture_map,
                                        (system_hash64) object_id);
            } /* if (callback_arg_ptr->object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE) */

            /* Release the raGL instance */
            _raGL_backend_release_raGL_object(callback_arg_ptr->backend_ptr,
                                              callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                              object_raGL,
                                              callback_arg_ptr->ral_callback_arg_ptr->deleted_objects[n_deleted_object]);
        } /* for (all deleted objects) */
    }
    system_critical_section_leave(object_map_cs);

end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_on_texture_client_memory_sourced_update_request(const void* callback_arg_data,
                                                                           void*       backend)
{
    _raGL_backend*                                                   backend_ptr      = (_raGL_backend*)                                                   backend;
    _ral_texture_client_memory_source_update_requested_callback_arg* callback_arg_ptr = (_ral_texture_client_memory_source_update_requested_callback_arg*) callback_arg_data;
    raGL_texture                                                     texture_raGL     = NULL;

    /* Identify the raGL_texture instance for the source ral_texture object and forward
     * the request.
     *
     * Note that all notifications will be coming from the same ral_texture
     * instance.*/
    system_critical_section_enter(backend_ptr->textures_map_cs);
    {
        if (!system_hash64map_get(backend_ptr->textures_map,
                                  (system_hash64) callback_arg_ptr->texture,
                                 &texture_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_texture instance found for the specified ral_texture instance.");

            goto end;
        }
    }
    system_critical_section_leave(backend_ptr->textures_map_cs);

    raGL_texture_update_with_client_sourced_data(texture_raGL,
                                                 callback_arg_ptr->n_updates,
                                                 callback_arg_ptr->updates);

end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_on_texture_mipmap_generation_request(const void* callback_arg_data,
                                                                void*       backend)
{
    _raGL_backend* backend_ptr  = (_raGL_backend*) backend;
    ral_texture    texture      = (ral_texture)    callback_arg_data;
    raGL_texture   texture_raGL = NULL;

    /* Identify the raGL_texture instance for the source ral_texture object and forward
     * the request. */
    system_critical_section_enter(backend_ptr->textures_map_cs);
    {
        if (!system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) texture,
                                 &texture_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_texture instance found for the specified ral_texture instance.");

            goto end;
        }
    }
    system_critical_section_leave(backend_ptr->textures_map_cs);

    raGL_texture_generate_mipmaps(texture_raGL);

end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_release_raGL_object(_raGL_backend*          backend_ptr,
                                               ral_context_object_type object_type,
                                               void*                   object_raGL,
                                               void*                   object_ral)
{
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            raGL_buffers_free_buffer_memory(backend_ptr->buffers,
                                            (raGL_buffer) object_raGL);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            raGL_framebuffer_release( (raGL_framebuffer) object_raGL);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            raGL_sampler_release( (raGL_sampler) object_raGL);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            raGL_texture_release( (raGL_texture) object_raGL);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_backend object type");
        }
    } /* switch (object_type) */
}

/** TODO */
PRIVATE void _raGL_backend_release_rendering_callback(ogl_context context,
                                                      void*       callback_arg)
{
    delete (_raGL_backend*) callback_arg;
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_notifications(_raGL_backend* backend_ptr,
                                                       bool           should_subscribe)
{
    system_callback_manager context_callback_manager = NULL;

    ral_context_get_property(backend_ptr->context_ral,
                             RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,
                            &context_callback_manager);

    if (should_subscribe)
    {
        /* Buffer notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_deleted,
                                                        backend_ptr);

        /* Framebuffer notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_deleted,
                                                        backend_ptr);

        /* Sampler notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_deleted,
                                                        backend_ptr);

        /* Texture notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_deleted,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        /* Buffer notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                                           _raGL_backend_on_objects_deleted,
                                                           backend_ptr);

        /* Framebuffer notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                           _raGL_backend_on_objects_deleted,
                                                           backend_ptr);

        /* Sampler notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                                           _raGL_backend_on_objects_deleted,
                                                           backend_ptr);

        /* Texture notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                           _raGL_backend_on_objects_deleted,
                                                           backend_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_buffer_notifications(_raGL_backend* backend_ptr,
                                                              ral_buffer     buffer,
                                                              bool           should_subscribe)
{
    system_callback_manager buffer_ral_callback_manager = NULL;

    ral_buffer_get_property(buffer,
                            RAL_BUFFER_PROPERTY_CALLBACK_MANAGER,
                           &buffer_ral_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(buffer_ral_callback_manager,
                                                        RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_buffer_client_memory_sourced_update_request,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(buffer_ral_callback_manager,
                                                           RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                                           _raGL_backend_on_buffer_client_memory_sourced_update_request,
                                                           backend_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_texture_notifications(_raGL_backend* backend_ptr,
                                                               ral_texture    texture,
                                                               bool           should_subscribe)
{
    system_callback_manager callback_manager = NULL;

    ral_texture_get_property(texture,
                             RAL_TEXTURE_PROPERTY_CALLBACK_MANAGER,
                            &callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(callback_manager,
                                                        RAL_TEXTURE_CALLBACK_ID_CLIENT_MEMORY_SOURCE_UPDATE_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_texture_client_memory_sourced_update_request,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(callback_manager,
                                                        RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_texture_mipmap_generation_request,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(callback_manager,
                                                           RAL_TEXTURE_CALLBACK_ID_CLIENT_MEMORY_SOURCE_UPDATE_REQUESTED,
                                                           _raGL_backend_on_texture_client_memory_sourced_update_request,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(callback_manager,
                                                           RAL_TEXTURE_CALLBACK_ID_MIPMAP_GENERATION_REQUESTED,
                                                           _raGL_backend_on_texture_mipmap_generation_request,
                                                           backend_ptr);
    }
}


/** Please see header for specification */
PUBLIC raGL_backend raGL_backend_create(ral_context               context_ral,
                                        system_hashed_ansi_string name,
                                        ral_backend_type          type)
{
    _raGL_backend* new_backend_ptr = new (std::nothrow) _raGL_backend(context_ral,
                                                                      name,
                                                                      type);

    ASSERT_ALWAYS_SYNC(new_backend_ptr != NULL,
                       "Out of memory");
    if (new_backend_ptr != NULL)
    {
        new_backend_ptr->context_ral = context_ral;

        /* Sign up for notifications */
        _raGL_backend_subscribe_for_notifications(new_backend_ptr,
                                                  true); /* should_subscribe */
    }

    return (raGL_backend) new_backend_ptr;
}

/** Please see header for specification */
PUBLIC void raGL_backend_init(raGL_backend backend)
{
    _raGL_backend* backend_ptr      = (_raGL_backend*) backend;
    _raGL_backend* root_backend_ptr = NULL;
    system_window  window           = NULL;
    bool           vsync_enabled    = false;

    if (!system_hashed_ansi_string_is_equal_to_raw_string(backend_ptr->name,
                                                          ROOT_WINDOW_ES_NAME) &&
         system_hashed_ansi_string_is_equal_to_raw_string(backend_ptr->name,
                                                          ROOT_WINDOW_GL_NAME) )
    {
        root_backend_ptr = (_raGL_backend*) raGL_backend_get_root_context(backend_ptr->backend_type);
    }

    /* Create a GL context  */
    ral_context_get_property(backend_ptr->context_ral,
                             RAL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    backend_ptr->context_gl = ogl_context_create_from_system_window(backend_ptr->name,
                                                                    backend_ptr->context_ral,
                                                                    backend,
                                                                    window,
                                                                    (root_backend_ptr != NULL) ? root_backend_ptr->context_gl
                                                                                               : NULL);

    ASSERT_DEBUG_SYNC(backend_ptr->context_gl != NULL,
                      "Could not create the GL rendering context instance");

    /* Bind the context to current thread. As a side effect, stuff like text rendering will be initialized. */
    ogl_context_bind_to_current_thread    (backend_ptr->context_gl);
    ogl_context_unbind_from_current_thread(backend_ptr->context_gl);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_buffer(void*      backend,
                                    ral_buffer buffer_ral,
                                    void**     out_buffer_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    buffer_ral,
                                    out_buffer_raGL_ptr);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_framebuffer(void*           backend,
                                         ral_framebuffer framebuffer_ral,
                                         void**          out_framebuffer_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                                    framebuffer_ral,
                                    out_framebuffer_raGL_ptr);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_sampler(void*       backend,
                                     ral_sampler sampler_ral,
                                     void**      out_sampler_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                    sampler_ral,
                                    out_sampler_raGL_ptr);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_texture(void*       backend,
                                     ral_texture texture_ral,
                                     void**      out_texture_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                    texture_ral,
                                    out_texture_raGL_ptr);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_texture_by_id(raGL_backend  backend,
                                           GLuint        texture_id,
                                           raGL_texture* out_texture_raGL_ptr)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;
    bool           result      = false;

    /* Sanity checks*/
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_backend instance is NULL");

        goto end;
    }

    if (texture_id == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input texture_id is 0");

        goto end;
    }

    if (out_texture_raGL_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    /* Retrieve the requested texture object */
    system_critical_section_enter(backend_ptr->textures_map_cs);
    {
        if (!system_hash64map_get(backend_ptr->texture_id_to_raGL_texture_map,
                                  (system_hash64) texture_id,
                                  out_texture_raGL_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Texture GL id [%d] is not recognized.",
                              texture_id);

            system_critical_section_leave(backend_ptr->textures_map_cs);
            goto end;
        }
    }
    system_critical_section_leave(backend_ptr->textures_map_cs);

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC ogl_context raGL_backend_get_root_context(ral_backend_type backend_type)
{
    _raGL_backend*                  root_backend_ptr   = NULL;
    ral_context                     root_context_ral   = NULL;
    const system_hashed_ansi_string root_window_name   = (backend_type == RAL_BACKEND_TYPE_ES) ? system_hashed_ansi_string_create(ROOT_WINDOW_ES_NAME)
                                                                                               : system_hashed_ansi_string_create(ROOT_WINDOW_GL_NAME);
    system_window                   root_window_system = NULL;
    demo_window                     root_window        = demo_app_get_window_by_name(root_window_name);

    if (root_window == NULL)
    {
        _raGL_backend_create_root_window(backend_type);

        root_window = demo_app_get_window_by_name(root_window_name);

        ASSERT_DEBUG_SYNC(root_window != NULL,
                          "Could not create a root window");
    } /* if (root_window == NULL) */

    demo_window_get_private_property(root_window,
                                     DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW,
                                    &root_window_system);
    system_window_get_property      (root_window_system,
                                     SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                                    &root_context_ral);
    ral_context_get_property        (root_context_ral,
                                     RAL_CONTEXT_PROPERTY_BACKEND,
                                    &root_backend_ptr);

    return root_backend_ptr->context_gl;
}


/** Please see header for specification */
PUBLIC void raGL_backend_get_property(void*                backend,
                                      ral_context_property property,
                                      void*                out_result_ptr)

{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input raGL_backend instance is NULL");

        goto end;
    } /* if (backend == NULL) */

    /* Retrieve the requested property value. */
    switch (property)
    {
        case RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT:
        {
            *(ogl_context*) out_result_ptr = backend_ptr->context_gl;

            break;
        }

        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        {
            const ogl_context_gl_limits* limits_ptr = NULL;

            ogl_context_get_property(backend_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_LIMITS,
                                    &limits_ptr);

            *(uint32_t*) out_result_ptr = limits_ptr->max_color_attachments;

            break;
        }

        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        {
            /* WGL manages the back buffers */
            *(uint32_t*) out_result_ptr = 1;

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT:
        {
            ogl_context_get_property(backend_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                                     out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_SIZE:
        {
            ogl_context_get_property(backend_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO_SIZE,
                                     out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        {
            ogl_context_get_property(backend_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                                     out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_context_property value.");
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void raGL_backend_release(void* backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    ASSERT_DEBUG_SYNC(backend != NULL,
                      "Input backend is NULL");

    if (backend != NULL)
    {
        /* Request a rendering context call-back to release backend's assets */
        ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                         _raGL_backend_release_rendering_callback,
                                                         backend_ptr);
    } /* if (backend != NULL) */
}

/** Please see header for specification */
PUBLIC void raGL_backend_set_private_property(raGL_backend                  backend,
                                              raGL_backend_private_property property,
                                              const void*                   data_ptr)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    switch (property)
    {
        case RAGL_BACKEND_PRIVATE_PROPERTY_RENDERING_CONTEXT:
        {
            ASSERT_DEBUG_SYNC(backend_ptr->context_gl == NULL,
                              "Rendering context is already set");

            backend_ptr->context_gl = *(ogl_context*) data_ptr;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_backend_private_property value.");
        }
    } /* switch (property) */
}