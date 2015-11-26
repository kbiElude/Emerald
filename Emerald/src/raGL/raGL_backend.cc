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
#include "ral/ral_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_buffers.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_samplers.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_textures.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

#define ROOT_WINDOW_ES_NAME "Root window ES"
#define ROOT_WINDOW_GL_NAME "Root window GL"


typedef enum
{
    RAGL_BACKEND_OBJECT_TYPE_BUFFER,
    RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER,
    RAGL_BACKEND_OBJECT_TYPE_SAMPLER,
    RAGL_BACKEND_OBJECT_TYPE_TEXTURE,

    /* Always last */
    RAGL_BACKEND_OBJECT_TYPE_COUNT
} raGL_backend_object_type;

typedef struct _raGL_backend
{
    system_hash64map        buffers_map;      /* maps ral_buffer to raGL_buffer instance; owns the mapped raGL_buffer instances */
    system_critical_section buffers_map_cs;
    system_hash64map        framebuffers_map; /* maps ral_framebuffer to raGL_framebuffer instance; owns the mapped raGL_framebuffer instances */
    system_critical_section framebuffers_map_cs;
    system_hash64map        samplers_map;     /* maps ral_sampler to raGL_sampler instance; owns the mapped raGL_sampler instances */
    system_critical_section samplers_map_cs;
    system_hash64map        textures_map;     /* maps ral_texture to raGL_texture instance; owns the mapped raGL_texture instances */
    system_critical_section textures_map_cs;

    raGL_buffers  buffers;
    raGL_samplers samplers;
    raGL_textures textures;

    ral_backend_type          backend_type;
    ogl_context               context_gl;
    ral_context               context_ral;
    system_hashed_ansi_string name;
    ral_framebuffer           system_framebuffer;

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
        name                              = NULL;
        samplers                          = NULL;
        samplers_map                      = system_hash64map_create       (sizeof(raGL_sampler) );
        samplers_map_cs                   = system_critical_section_create();
        system_framebuffer                = NULL;
        textures                          = NULL;

        ASSERT_DEBUG_SYNC(buffers_map != NULL,
                          "Could not create the buffers map");
        ASSERT_DEBUG_SYNC(framebuffers_map != NULL,
                          "Could not create the framebuffers map");
        ASSERT_DEBUG_SYNC(samplers_map != NULL,
                          "Could not create the samplers map");
    }

    ~_raGL_backend()
    {
        /* Release object managers */
        if (buffers != NULL)
        {
            raGL_buffers_release(buffers);

            buffers = NULL;
        }

        if (samplers != NULL)
        {
            raGL_samplers_release(samplers);

            samplers = NULL;
        }

        if (textures != NULL)
        {
            raGL_textures_release(textures);

            textures = NULL;
        }

        /* Delete the system framebuffer and then proceed with release of other dangling objects. */
        if (system_framebuffer != NULL)
        {
            ral_context_delete_framebuffers(context_ral,
                                            1, /* n_framebuffers */
                                           &system_framebuffer);

            system_framebuffer = NULL;
        } /* if (system_framebuffer != NULL) */

        for (uint32_t n_object_type = 0;
                      n_object_type < RAGL_BACKEND_OBJECT_TYPE_COUNT;
                    ++n_object_type)
        {
            system_critical_section cs              = NULL;
            system_hash64map*       objects_map_ptr = NULL;

            switch (n_object_type)
            {
                case RAGL_BACKEND_OBJECT_TYPE_BUFFER:
                {
                    cs              =  buffers_map_cs;
                    objects_map_ptr = &buffers_map;

                    break;
                }

                case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER:
                {
                    cs              =  framebuffers_map_cs;
                    objects_map_ptr = &framebuffers_map;

                    break;
                }

                case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:
                {
                    cs              =  samplers_map_cs;
                    objects_map_ptr = &samplers_map;

                    break;
                }

                case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:
                {
                    cs              = textures_map_cs;
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
                            case RAGL_BACKEND_OBJECT_TYPE_BUFFER:      raGL_buffer_release     ( (raGL_buffer&)      current_object); break;
                            case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER: raGL_framebuffer_release( (raGL_framebuffer&) current_object); break;
                            case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:     raGL_sampler_release    ( (raGL_sampler&)     current_object); break;
                            case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:     raGL_texture_release    ( (raGL_texture&)     current_object); break;

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

        system_critical_section_release(buffers_map_cs);
        buffers_map_cs = NULL;

        system_critical_section_release(framebuffers_map_cs);
        framebuffers_map_cs = NULL;

        system_critical_section_release(samplers_map_cs);
        samplers_map_cs = NULL;
    }
} _raGL_backend;

typedef struct
{

    _raGL_backend*           backend_ptr;
    raGL_backend_object_type object_type;
    const void*              ral_callback_arg_ptr;

} _raGL_backend_on_objects_created_rendering_callback_arg;


/* Forward declarations */
PRIVATE void _raGL_backend_cache_limits                         (_raGL_backend* backend_ptr);
PRIVATE void _raGL_backend_init_system_fbo                      (_raGL_backend* backend_ptr);
PRIVATE void _raGL_backend_on_buffers_created                   (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_buffers_deleted                   (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_framebuffers_created              (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_framebuffers_deleted              (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_objects_created_rendering_callback(ogl_context    context,
                                                                 void*          callback_arg);
PRIVATE void _raGL_backend_on_samplers_created                  (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_samplers_deleted                  (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_textures_created                  (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_on_textures_deleted                  (const void*    callback_arg_data,
                                                                 void*          backend);
PRIVATE void _raGL_backend_subscribe_for_notifications          (_raGL_backend* backend_ptr,
                                                                 bool           should_subscribe);


/** TODO */
PRIVATE void _raGL_backend_cache_limits(_raGL_backend* backend_ptr)
{
    ogl_context_gl_limits* limits_ptr = NULL;

    ogl_context_get_property(backend_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    backend_ptr->max_framebuffer_color_attachments = limits_ptr->max_color_attachments;
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
PRIVATE void _raGL_backend_init_system_fbo(_raGL_backend* backend_ptr)
{
    GLuint system_fbo_id    = 0;

    /* Create the ral_framebuffer instance from an existing FBO the ogl_context has already set up. */
    ogl_context_get_property(backend_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                            &system_fbo_id);

    ASSERT_DEBUG_SYNC(false,
                      "TODO");

    // TODO ..
}

/** TODO */
PRIVATE void _raGL_backend_on_buffers_created(const void* callback_arg_data,
                                              void*       backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_data != NULL,
                      "Callback argument is NULL");

    /* Request a rendering call-back to create the buffer instances */
    _raGL_backend_on_objects_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.object_type          = RAGL_BACKEND_OBJECT_TYPE_BUFFER;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_data;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* All done - the rendering callback does all the dirty job needed. */
}

/** TODO */
PRIVATE void _raGL_backend_on_buffers_deleted(const void* callback_arg_data,
                                              void*       backend)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** TODO */
PRIVATE void _raGL_backend_on_framebuffers_created(const void* callback_arg_data,
                                                   void*       backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_data != NULL,
                      "Callback argument is NULL");

    /* Request a rendering call-back to create the framebuffer instances */
    _raGL_backend_on_objects_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.object_type          = RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_data;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* All done - the rendering callback does all the dirty job needed. */
}

/** TODO */
PRIVATE void _raGL_backend_on_framebuffers_deleted(const void* callback_arg_data,
                                                   void*       backend)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** TODO */
PRIVATE void _raGL_backend_on_objects_created_rendering_callback(ogl_context context,
                                                                 void*       callback_arg)
{
    _raGL_backend_on_objects_created_rendering_callback_arg* callback_arg_ptr        = (_raGL_backend_on_objects_created_rendering_callback_arg*) callback_arg;
    const ogl_context_gl_entrypoints*                        entrypoints_ptr         = NULL;
    uint32_t                                                 n_objects_to_initialize = 0;
    system_hash64map                                         objects_map             = NULL;
    system_critical_section                                  objects_map_cs          = NULL;
    bool                                                     result                  = false;
    GLuint*                                                  result_object_ids_ptr   = NULL;

    union
    {
        ral_buffer*      ral_buffer_ptrs;
        ral_framebuffer* ral_framebuffer_ptrs;
        void**           ral_object_ptrs;
        ral_sampler*     ral_sampler_ptrs;
        ral_texture*     ral_texture_ptrs;
    };

    ASSERT_DEBUG_SYNC(callback_arg != NULL,
                      "Callback argument is NULL");

    ogl_context_get_property(callback_arg_ptr->backend_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* Retrieve object type-specific data */
    ral_buffer_ptrs = NULL;

    switch (callback_arg_ptr->object_type)
    {
        case RAGL_BACKEND_OBJECT_TYPE_BUFFER:
        {
            n_objects_to_initialize = ((ral_context_callback_buffers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->n_buffers;
            objects_map             = callback_arg_ptr->backend_ptr->buffers_map;
            objects_map_cs          = callback_arg_ptr->backend_ptr->buffers_map_cs;
            ral_buffer_ptrs         = ((ral_context_callback_buffers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->created_buffers;

            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER:
        {
            n_objects_to_initialize = ((ral_context_callback_framebuffers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->n_framebuffers;
            objects_map             = callback_arg_ptr->backend_ptr->framebuffers_map;
            objects_map_cs          = callback_arg_ptr->backend_ptr->framebuffers_map_cs;
            ral_framebuffer_ptrs    = ((ral_context_callback_framebuffers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->created_framebuffers;

            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:
        {
            n_objects_to_initialize = ((ral_context_callback_samplers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->n_samplers;
            objects_map             = callback_arg_ptr->backend_ptr->samplers_map;
            objects_map_cs          = callback_arg_ptr->backend_ptr->samplers_map_cs;
            ral_sampler_ptrs        = ((ral_context_callback_samplers_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->created_samplers;

            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:
        {
            n_objects_to_initialize = ((ral_context_callback_textures_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->n_textures;
            objects_map             = callback_arg_ptr->backend_ptr->textures_map;
            objects_map_cs          = callback_arg_ptr->backend_ptr->textures_map_cs;
            ral_texture_ptrs        = ((ral_context_callback_textures_created_callback_arg*) callback_arg_ptr->ral_callback_arg_ptr)->created_textures;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_backend_object_type value.");

            goto end;
        }
    } /* switch (callback_arg_ptr->object_type) */

    /* Generate the IDs */
    result_object_ids_ptr = new (std::nothrow) GLuint[n_objects_to_initialize];

    if (result_object_ids_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    } /* if (result_fb_ids_ptr == NULL) */

    switch (callback_arg_ptr->object_type)
    {
        case RAGL_BACKEND_OBJECT_TYPE_BUFFER:
        {
            /* IDs are associated by raGL_buffers */
            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER:
        {
            entrypoints_ptr->pGLGenFramebuffers(n_objects_to_initialize,
                                                result_object_ids_ptr);

            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:
        {
            /* IDs are associated by raGL_samplers */
            break;
        }

        case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:
        {
            /* IDs are associated by raGL_textures */
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_backend_object_type value.");

            goto end;
        }
    } /* switch (callback_arg_ptr->object_type) */

    /* Create raGL instances for each object ID */
    for (uint32_t n_object_id = 0;
                  n_object_id < n_objects_to_initialize;
                ++n_object_id)
    {
        void* new_object = NULL;

        switch (callback_arg_ptr->object_type)
        {
            case RAGL_BACKEND_OBJECT_TYPE_BUFFER:
            {
                raGL_buffers_allocate_buffer_memory_for_ral_buffer(callback_arg_ptr->backend_ptr->buffers,
                                                                   ral_buffer_ptrs[n_object_id],
                                                                   (raGL_buffer*) &new_object);

                break;
            }

            case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER:
            {
                GLuint current_object_id = result_object_ids_ptr[n_object_id];

                new_object = raGL_framebuffer_create(context,
                                                     current_object_id,
                                                     ral_framebuffer_ptrs[n_object_id]);

                break;
            }

            case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:
            {
                new_object = raGL_samplers_get_sampler(callback_arg_ptr->backend_ptr->samplers,
                                                       ral_sampler_ptrs[n_object_id]);

                break;
            }

            case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:
            {
                new_object = raGL_textures_get_texture_from_pool(callback_arg_ptr->backend_ptr->textures,
                                                                 ral_texture_ptrs[n_object_id]);

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
                                    (system_hash64) ral_object_ptrs[n_object_id],
                                    new_object,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */
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
                                          (system_hash64) ral_object_ptrs[n_object],
                                         &object_instance) )
                {
                    continue;
                }

                switch (callback_arg_ptr->object_type)
                {
                    case RAGL_BACKEND_OBJECT_TYPE_BUFFER:
                    {
                        raGL_buffers_free_buffer_memory(callback_arg_ptr->backend_ptr->buffers,
                                                        (raGL_buffer) object_instance);

                        break;
                    }

                    case RAGL_BACKEND_OBJECT_TYPE_FRAMEBUFFER:
                    {
                        raGL_framebuffer_release( (raGL_framebuffer) object_instance);

                        break;
                    }

                    case RAGL_BACKEND_OBJECT_TYPE_SAMPLER:
                    {
                        raGL_sampler_release( (raGL_sampler) object_instance);

                        break;
                    }

                    case RAGL_BACKEND_OBJECT_TYPE_TEXTURE:
                    {
                        raGL_texture_release( (raGL_texture) object_instance);

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized raGL_backend object type");
                    }
                } /* switch (callback_arg_ptr->object_type) */

                object_instance = NULL;
            } /* for (all requested objects) */
        } /* if (!result) */

        delete [] result_object_ids_ptr;

        result_object_ids_ptr = NULL;
    } /* if (result_object_ids_ptr != NULL) */

}

/** TODO */
PRIVATE void _raGL_backend_on_samplers_created(const void* callback_arg_data,
                                               void*       backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_data != NULL,
                      "Callback argument is NULL");

    /* Request a rendering call-back to create the sampler instances */
    _raGL_backend_on_objects_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.object_type          = RAGL_BACKEND_OBJECT_TYPE_SAMPLER;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_data;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* All done - the rendering callback does all the dirty job needed. */
}

/** TODO */
PRIVATE void _raGL_backend_on_samplers_deleted(const void* callback_arg_data,
                                               void*       backend)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

PRIVATE void _raGL_backend_on_textures_created(const void* callback_arg_data,
                                               void*       backend)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_data != NULL,
                      "Callback argument is NULL");

    /* Request a rendering call-back to create the sampler instances */
    _raGL_backend_on_objects_created_rendering_callback_arg rendering_callback_arg;

    rendering_callback_arg.backend_ptr          = backend_ptr;
    rendering_callback_arg.object_type          = RAGL_BACKEND_OBJECT_TYPE_TEXTURE;
    rendering_callback_arg.ral_callback_arg_ptr = callback_arg_data;

    ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                     _raGL_backend_on_objects_created_rendering_callback,
                                                    &rendering_callback_arg);

    /* All done - the rendering callback does all the dirty job needed. */
}

/** TODO */
PRIVATE void _raGL_backend_on_textures_deleted(const void* callback_arg_data,
                                               void*       backend)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
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
                                                        _raGL_backend_on_buffers_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_buffers_deleted,
                                                        backend_ptr);

        /* Framebuffer notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_framebuffers_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_framebuffers_deleted,
                                                        backend_ptr);

        /* Sampler notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_samplers_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_samplers_deleted,
                                                        backend_ptr);

        /* Texture notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_textures_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_textures_deleted,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        /* Buffer notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_BUFFERS_CREATED,
                                                           _raGL_backend_on_buffers_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_BUFFERS_DELETED,
                                                           _raGL_backend_on_buffers_deleted,
                                                           backend_ptr);

        /* Framebuffer notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_CREATED,
                                                           _raGL_backend_on_framebuffers_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_FRAMEBUFFERS_DELETED,
                                                           _raGL_backend_on_framebuffers_deleted,
                                                           backend_ptr);

        /* Sampler notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SAMPLERS_CREATED,
                                                           _raGL_backend_on_samplers_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SAMPLERS_DELETED,
                                                           _raGL_backend_on_samplers_deleted,
                                                           backend_ptr);

        /* Texture notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                           _raGL_backend_on_textures_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                           _raGL_backend_on_textures_deleted,
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
        _raGL_backend* root_backend_ptr = NULL;
        system_window  window           = NULL;
        bool           vsync_enabled    = false;

        root_backend_ptr = (_raGL_backend*) raGL_backend_get_root_context(type);

        ASSERT_DEBUG_SYNC(root_backend_ptr != NULL,
                          "Root window back-end is NULL");

        /* Create a GL context  */
        ral_context_get_property(context_ral,
                                 RAL_CONTEXT_PROPERTY_WINDOW,
                                &window);

        new_backend_ptr->context_gl = ogl_context_create_from_system_window(name,
                                                                            context_ral,
                                                                            (raGL_backend) new_backend_ptr,
                                                                            window,
                                                                            root_backend_ptr->context_gl);
        ASSERT_DEBUG_SYNC(new_backend_ptr->context_gl != NULL,
                          "Could not create the GL rendering context instance");

        /* Create buffer memory manager */
        new_backend_ptr->buffers = raGL_buffers_create((raGL_backend) new_backend_ptr,
                                                       new_backend_ptr->context_gl);

        /* Create sampler manager */
        new_backend_ptr->samplers = raGL_samplers_create(new_backend_ptr->context_gl);

        /* Create texture manager */
        new_backend_ptr->textures = raGL_textures_create(new_backend_ptr->context_gl);

        /* Sign up for notifications */
        _raGL_backend_subscribe_for_notifications(new_backend_ptr,
                                                  true); /* should_subscribe */

        /* Bind the context to current thread. As a side effect, stuff like text rendering will
         * be initialized.
         */
        ogl_context_bind_to_current_thread    (new_backend_ptr->context_gl);
        ogl_context_unbind_from_current_thread(new_backend_ptr->context_gl);

        /* Configure the system framebuffer */
        _raGL_backend_init_system_fbo(new_backend_ptr);

        /* Cache rendering context limits */
        _raGL_backend_cache_limits(new_backend_ptr);
    }

    return (raGL_backend) new_backend_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_framebuffer(void*              backend,
                                         ral_framebuffer    framebuffer_ral,
                                         raGL_framebuffer** out_framebuffer_raGL_ptr)
{
    _raGL_backend* backend_ptr = (_raGL_backend*) backend;
    bool           result      = false;

    /* Sanity checks */
    if (backend == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input backend is NULL");

        goto end;
    }

    /* Try to find the raGL framebuffer instance */
    system_critical_section_enter(backend_ptr->framebuffers_map_cs);

    if (!system_hash64map_get(backend_ptr->framebuffers_map,
                              (system_hash64) framebuffer_ral,
                              out_framebuffer_raGL_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Provided RAL framebuffer handle was not recognized");

        goto end;
    }

    /* All done */
    result = true;

end:
    if (backend_ptr != NULL)
    {
        system_critical_section_leave(backend_ptr->framebuffers_map_cs);
    }

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

    demo_window_get_property  (root_window,
                               DEMO_WINDOW_PROPERTY_WINDOW,
                              &root_window_system);
    system_window_get_property(root_window_system,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                              &root_context_ral);
    ral_context_get_property  (root_context_ral,
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
        case RAL_CONTEXT_PROPERTY_MAX_FRAMEBUFFER_COLOR_ATTACHMENTS:
        {
            *(uint32_t*) out_result_ptr = backend_ptr->max_framebuffer_color_attachments;

            break;
        }

        case RAL_CONTEXT_PROPERTY_N_OF_SYSTEM_FRAMEBUFFERS:
        {
            /* WGL manages the back buffers */
            *(uint32_t*) out_result_ptr = 1;

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_TEXTURE_FORMAT:
        {
            ogl_context_get_property(backend_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                                     out_result_ptr);

            break;
        }

        case RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFERS:
        {
            ASSERT_DEBUG_SYNC(false,
                              "system_framebuffer is NULL at the moment, TODO");

            *(ral_framebuffer*) out_result_ptr = backend_ptr->system_framebuffer;

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
        ogl_context_release_managers(backend_ptr->context_gl);

        /* Sign out from notifications */
        _raGL_backend_subscribe_for_notifications(backend_ptr,
                                                  false); /* should_subscribe */

        delete (_raGL_backend*) backend;
    } /* if (backend != NULL) */
}