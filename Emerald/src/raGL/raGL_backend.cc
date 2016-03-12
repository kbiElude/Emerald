/**
 *
 * Emerald (kbi/elude @2015-2016)
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
#include "ral/ral_program.h"
#include "ral/ral_sampler.h"
#include "ral/ral_scheduler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_buffers.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_sampler.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_texture.h"
#include "raGL/raGL_textures.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_read_write_mutex.h"
#include "system/system_window.h"

#define N_HELPER_CONTEXTS         4
#define ROOT_HELPER_CONTEXT_INDEX 0
#define ROOT_WINDOW_ES_NAME       "Root window ES"
#define ROOT_WINDOW_GL_NAME       "Root window GL"


typedef struct _raGL_backend
{
    bool is_helper_context;

    system_hash64map        buffers_map;      /* maps ral_buffer to raGL_buffer instance; owns the mapped raGL_buffer instances */
    system_read_write_mutex buffers_map_rw_mutex;
    bool                    buffers_map_owner;

    system_hash64map        framebuffers_map; /* maps ral_framebuffer to raGL_framebuffer instance; owns the mapped raGL_framebuffer instances */
    system_read_write_mutex framebuffers_map_rw_mutex;

    system_hash64map        samplers_map;     /* maps ral_sampler to raGL_sampler instance; owns the mapped raGL_sampler instances */
    bool                    samplers_map_owner;
    system_read_write_mutex samplers_map_rw_mutex;

    system_hash64map        shaders_map;     /* maps ral_shader to raGL_shader instance; owns the mapped raGL_shader instances */
    bool                    shaders_map_owner;
    system_read_write_mutex shaders_map_rw_mutex;

    system_hash64map        program_id_to_raGL_program_map; /* maps GLid to raGL_program instance; does not own the raGL_program values; lock programs_map_cs before usage. */
    system_hash64map        programs_map;                   /* maps ral_program to raGL_program instance; owns the mapped raGL_program instances */
    system_read_write_mutex programs_map_rw_mutex;
    bool                    programs_map_owner;

    system_hash64map        texture_id_to_raGL_texture_map; /* maps GLid to raGL_texture instance; does NOT own the mapepd raGL_texture instances; lock textures_map_cs before usage. */
    system_hash64map        textures_map;                   /* maps ral_texture to raGL_texture instance; owns the mapped raGL_texture instances */
    bool                    textures_map_owner;
    system_read_write_mutex textures_map_rw_mutex;

    raGL_buffers  buffers;
    raGL_textures textures;

    ral_backend_type          backend_type;
    ogl_context               context_gl;
    ral_context               context_ral;
    system_hashed_ansi_string name;

    uint32_t max_framebuffer_color_attachments;

    _raGL_backend(ral_context               in_owner_context,
                  system_hashed_ansi_string in_name,
                  ral_backend_type          in_backend_type);
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

typedef struct _raGL_backend_global_context
{
    raGL_backend helper_backend;
    ral_context  helper_context;
    demo_window  helper_window;

    _raGL_backend_global_context()
    {
        helper_backend = NULL;
        helper_context = NULL;
        helper_window  = NULL;
    }

    ~_raGL_backend_global_context()
    {
        ASSERT_DEBUG_SYNC(helper_backend == NULL &&
                          helper_context == NULL &&
                          helper_window  == NULL,
                          "Helper context should have been initialized before ~_raGL_backend_global_context() is called.");
    }
} _raGL_backend_global_context;

typedef struct _raGL_backend_global
{
    ral_backend_type        backend_type;
    system_critical_section cs;
    uint32_t                n_owners;

    _raGL_backend_global_context helper_contexts[N_HELPER_CONTEXTS];

    _raGL_backend_global()
    {
        backend_type = RAL_BACKEND_TYPE_UNKNOWN;
        cs           = system_critical_section_create();
        n_owners     = 0;

        memset(helper_contexts,
               0,
               sizeof(helper_contexts) );
    }

    ~_raGL_backend_global()
    {
        system_critical_section_enter(cs);
        {
            ASSERT_DEBUG_SYNC(n_owners == 0,
                              "GL back-end global about to deinitialize, even though helper contexts are still active.");
        }
        system_critical_section_leave(cs);

        system_critical_section_release(cs);
        cs = NULL;
    }

    void deinit();
    void init  (ral_backend_type backend_type);
} _raGL_backend_global;

static _raGL_backend_global _global;


/* Forward declarations */
PRIVATE void        _raGL_backend_cache_limits                                   (_raGL_backend*           backend_ptr);
PRIVATE demo_window _raGL_backend_create_helper_window                           (ral_backend_type         backend_type,
                                                                                  uint32_t                 n_helper_window);
PRIVATE bool        _raGL_backend_get_object                                     (void*                    backend,
                                                                                  ral_context_object_type  object_type,
                                                                                  void*                    object_ral,
                                                                                  void**                   out_result_ptr);
PRIVATE void        _raGL_backend_get_object_vars                                (_raGL_backend*           backend_ptr,
                                                                                  ral_context_object_type  object_type,
                                                                                  system_read_write_mutex* out_rw_mutex_ptr,
                                                                                  system_hash64map*        out_hashmap_ptr,
                                                                                  bool*                    out_is_owner_ptr);
PRIVATE void        _raGL_backend_helper_context_renderer_callback               (ogl_context              context,
                                                                                  void*                    unused);
PRIVATE void        _raGL_backend_link_program_handler                           (void*                    program_raGL_raw);
PRIVATE void        _raGL_backend_on_buffer_client_memory_sourced_update_request (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_buffer_to_buffer_copy_request               (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_objects_created                             (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_objects_created_rendering_callback          (ogl_context              context,
                                                                                  void*                    callback_arg);
PRIVATE void        _raGL_backend_on_objects_deleted                             (const void*              callback_arg,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_objects_deleted_rendering_callback          (ogl_context              context,
                                                                                  void*                    callback_arg);
PRIVATE void        _raGL_backend_on_shader_attach_request                       (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_shader_body_updated_notification            (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_texture_client_memory_sourced_update_request(const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_on_texture_mipmap_generation_request           (const void*              callback_arg_data,
                                                                                  void*                    backend);
PRIVATE void        _raGL_backend_release_raGL_object                            (_raGL_backend*           backend_ptr,
                                                                                  ral_context_object_type  object_type,
                                                                                  void*                    object_raGL,
                                                                                  void*                    object_ral);
PRIVATE void        _raGL_backend_subscribe_for_buffer_notifications             (_raGL_backend*           backend_ptr,
                                                                                  ral_buffer               buffer,
                                                                                  bool                     should_subscribe);
PRIVATE void        _raGL_backend_subscribe_for_notifications                    (_raGL_backend*           backend_ptr,
                                                                                  bool                     should_subscribe);
PRIVATE void        _raGL_backend_subscribe_for_program_notifications            (_raGL_backend*           backend_ptr,
                                                                                  ral_program              program,
                                                                                  bool                     should_subscribe);
PRIVATE void        _raGL_backend_subscribe_for_shader_notifications             (_raGL_backend*           backend_ptr,
                                                                                  ral_shader               shader,
                                                                                  bool                     should_subscribe);
PRIVATE void        _raGL_backend_subscribe_for_texture_notifications            (_raGL_backend*           backend_ptr,
                                                                                  ral_texture              texture,
                                                                                  bool                     should_subscribe);


/** TODO */
void _raGL_backend_global::deinit()
{
    system_critical_section_enter(_global.cs);
    {
        for (uint32_t n_helper_context = 0;
                      n_helper_context < N_HELPER_CONTEXTS;
                    ++n_helper_context)
        {
            demo_window_close(_global.helper_contexts[n_helper_context].helper_window);

            _global.helper_contexts[n_helper_context].helper_backend = NULL;
            _global.helper_contexts[n_helper_context].helper_context = NULL;
            _global.helper_contexts[n_helper_context].helper_window  = NULL;
        }
    }
    system_critical_section_leave(_global.cs);
}

/** TODO */
void _raGL_backend_global::init(ral_backend_type backend_type)
{
    system_critical_section_enter(_global.cs);

    for (uint32_t n_helper_context = 0;
                  n_helper_context < N_HELPER_CONTEXTS;
                ++n_helper_context)
    {
        raGL_backend                           backend      = NULL;
        _raGL_backend*                         backend_ptr  = NULL;
        ral_context                            context      = NULL;
        volatile _raGL_backend_global_context& context_data = _global.helper_contexts[n_helper_context];
        static const bool                      true_value   = true;
        demo_window                            window       = _raGL_backend_create_helper_window(backend_type,
                                                                                                 n_helper_context);

        demo_window_get_property(window,
                                 DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                                &context);
        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_BACKEND,
                                &backend);

        context_data.helper_backend = backend;
        context_data.helper_context = context;
        context_data.helper_window  = window;

        backend_ptr = (_raGL_backend*) backend;

        if (((_raGL_backend*) _global.helper_contexts[0].helper_backend)->buffers != NULL)
        {
            backend_ptr->buffers = ((_raGL_backend*) _global.helper_contexts[0].helper_backend)->buffers;

            raGL_buffers_retain(backend_ptr->buffers);
        }
        else
        {
            backend_ptr->buffers = raGL_buffers_create((raGL_backend) backend_ptr,
                                                       backend_ptr->context_gl);
        }

        if (((_raGL_backend*) _global.helper_contexts[0].helper_backend)->textures != NULL)
        {
            backend_ptr->textures = ((_raGL_backend*) _global.helper_contexts[0].helper_backend)->textures;

            raGL_textures_retain(backend_ptr->textures);
        }
        else
        {
            backend_ptr->textures = raGL_textures_create(backend_ptr->context_ral,
                                                         backend_ptr->context_gl);
        }

        ogl_context_set_property(ral_context_get_gl_context(context),
                                 OGL_CONTEXT_PROPERTY_IS_HELPER_CONTEXT,
                                &true_value);

        ((_raGL_backend*) backend)->is_helper_context = true;
    }

    _global.backend_type = backend_type;

    /* Assign the helper rendering thread to RAL scheduler.
     *
     * NOTE: We deliberately skip the zeroth helper context. It acts as a root context for all spawned
     *       rendering contexts. When creating a new GL context, it *must* be unbound from the rendering
     *       thread, or else the new context is not created successfully.
     *       It's a pity we need to mantain such a zombie context, but working around this would be very
     *       error-prone.
     **/
    for (uint32_t n_helper_context = 1;
                  n_helper_context < N_HELPER_CONTEXTS;
                ++n_helper_context)
    {
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(_global.helper_contexts[n_helper_context].helper_context),
                                                         _raGL_backend_helper_context_renderer_callback,
                                                         (void*) n_helper_context,
                                                         false,  /* swap_buffers_afterward */
                                                         OGL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_DONT_BLOCK);

    }

    system_critical_section_leave(_global.cs);
}


/** TODO */
_raGL_backend::_raGL_backend(ral_context               in_owner_context,
                             system_hashed_ansi_string in_name,
                             ral_backend_type          in_backend_type)
{
    static_assert(N_HELPER_CONTEXTS >= 1, "");

    _global.backend_type = in_backend_type;

    if (_global.helper_contexts[0].helper_backend != NULL)
    {
        _raGL_backend* root_backend_ptr = (_raGL_backend*) _global.helper_contexts[0].helper_backend;

        buffers_map                    = root_backend_ptr->buffers_map;
        buffers_map_rw_mutex           = root_backend_ptr->buffers_map_rw_mutex;
        buffers_map_owner              = false;
        program_id_to_raGL_program_map = root_backend_ptr->program_id_to_raGL_program_map;
        programs_map                   = root_backend_ptr->programs_map;
        programs_map_owner             = false;
        programs_map_rw_mutex          = root_backend_ptr->programs_map_rw_mutex;
        samplers_map                   = root_backend_ptr->samplers_map;
        samplers_map_owner             = false;
        samplers_map_rw_mutex          = root_backend_ptr->samplers_map_rw_mutex;
        shaders_map                    = root_backend_ptr->shaders_map;
        shaders_map_owner              = false;
        shaders_map_rw_mutex           = root_backend_ptr->shaders_map_rw_mutex;
        textures                       = NULL;
        texture_id_to_raGL_texture_map = root_backend_ptr->texture_id_to_raGL_texture_map;
        textures_map                   = root_backend_ptr->textures_map;
        textures_map_rw_mutex          = root_backend_ptr->textures_map_rw_mutex;
        textures_map_owner             = false;
    }
    else
    {
        buffers_map                    = system_hash64map_create       (sizeof(raGL_buffer) );
        buffers_map_rw_mutex           = system_read_write_mutex_create();
        buffers_map_owner              = true;
        program_id_to_raGL_program_map = system_hash64map_create       (sizeof(raGL_program) );
        programs_map                   = system_hash64map_create       (sizeof(raGL_program) );
        programs_map_rw_mutex          = system_read_write_mutex_create();
        programs_map_owner             = true;
        samplers_map                   = system_hash64map_create       (sizeof(raGL_sampler) );
        samplers_map_rw_mutex          = system_read_write_mutex_create();
        samplers_map_owner             = true;
        shaders_map                    = system_hash64map_create       (sizeof(raGL_shader) );
        shaders_map_rw_mutex           = system_read_write_mutex_create();
        shaders_map_owner              = true;
        textures                       = NULL;
        texture_id_to_raGL_texture_map = system_hash64map_create       (sizeof(raGL_texture) );
        textures_map                   = system_hash64map_create       (sizeof(raGL_texture) );
        textures_map_rw_mutex          = system_read_write_mutex_create();
        textures_map_owner             = true;
    }

    backend_type                      = in_backend_type;
    buffers                           = NULL;
    context_gl                        = NULL;
    context_ral                       = in_owner_context;
    framebuffers_map                  = system_hash64map_create       (sizeof(raGL_framebuffer) );
    framebuffers_map_rw_mutex         = system_read_write_mutex_create();
    is_helper_context                 = false;
    max_framebuffer_color_attachments = 0;
    name                              = in_name;
}

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

    if (context_gl != NULL)
    {
        ogl_context_release(context_gl);

        context_gl = NULL;
    } /* if (context_gl != NULL) */

    for (uint32_t n_object_type = 0;
                  n_object_type < RAL_CONTEXT_OBJECT_TYPE_COUNT;
                ++n_object_type)
    {
        bool                    is_owner          = false;
        uint32_t                n_objects_leaking = 0;
        system_hash64map        objects_map       = NULL;
        system_hash64map*       objects_map_ptr   = &objects_map;
        system_read_write_mutex rw_mutex          = NULL;

        _raGL_backend_get_object_vars(this,
                                      (ral_context_object_type) n_object_type,
                                     &rw_mutex,
                                     &objects_map,
                                     &is_owner);

        if (!is_owner)
        {
            continue;
        }

        system_hash64map_get_property(objects_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_objects_leaking);

        if (n_objects_leaking > 0)
        {
            char* error_string = NULL;
            char  temp[128];

            switch (n_object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_BUFFER:      error_string = "GL back-end leaks [%d] buffers";      break;
                case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER: error_string = "GL back-end leaks [%d] framebuffers"; break;
                case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:     error_string = "GL back-end leaks [%d] programs";     break;
                case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:     error_string = "GL back-end leaks [%d] samplers";     break;
                case RAL_CONTEXT_OBJECT_TYPE_SHADER:      error_string = "GL back-end leaks [%d] shaders";      break;
                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:     error_string = "GL back-end leaks [%d] textures";     break;

                default:
                {
                    error_string = "!?";
                }
            } /* switch (n_object_type) */

            snprintf(temp,
                     sizeof(temp),
                     error_string,
                     n_objects_leaking);

            LOG_ERROR("%s",
                      temp);
        } /* if (n_objects_leaking > 0) */

        system_hash64map_release(*objects_map_ptr);
        *objects_map_ptr = NULL;
    }

    if (texture_id_to_raGL_texture_map != NULL &&
        textures_map_owner)
    {
        system_hash64map_release(texture_id_to_raGL_texture_map);

        texture_id_to_raGL_texture_map = NULL;
    } /* if (texture_id_to_raGL_texture_map != NULL) */

    /* Unsubscribe from notifications */
    _raGL_backend_subscribe_for_notifications(this,
                                              false); /* should_subscribe */

    /* Release the critical sections */
    if (buffers_map_owner)
    {
        system_read_write_mutex_release(buffers_map_rw_mutex);
    }
    buffers_map_rw_mutex = NULL;

    system_read_write_mutex_release(framebuffers_map_rw_mutex);
    framebuffers_map_rw_mutex = NULL;

    if (programs_map_owner)
    {
        if (program_id_to_raGL_program_map != NULL)
        {
            system_hash64map_release(program_id_to_raGL_program_map);

            program_id_to_raGL_program_map = NULL;
        }

        system_read_write_mutex_release(programs_map_rw_mutex);
    }
    program_id_to_raGL_program_map = NULL;
    programs_map_rw_mutex          = NULL;

    if (samplers_map_owner)
    {
        system_read_write_mutex_release(samplers_map_rw_mutex);
    }
    samplers_map_rw_mutex = NULL;

    if (shaders_map_owner)
    {
        system_read_write_mutex_release(shaders_map_rw_mutex);
    }
    shaders_map_rw_mutex = NULL;

    if (textures_map_owner)
    {
        system_read_write_mutex_release(textures_map_rw_mutex);
    }
    textures_map_rw_mutex = NULL;
}

/** TODO */
PRIVATE demo_window _raGL_backend_create_helper_window(ral_backend_type backend_type,
                                                       uint32_t         n_window)
{
    demo_window               window                   = NULL;
    demo_window_create_info   window_create_info;
    char                      window_name[128];
    system_hashed_ansi_string window_name_has          = NULL;

    snprintf(window_name,
             sizeof(window_name),
             "Helper window %u",
             n_window);

    window_name_has = system_hashed_ansi_string_create(window_name);

    /* Spawn a new helper window. */
    window_create_info.resolution[0] = 4;
    window_create_info.resolution[1] = 4;
    window_create_info.visible = false;

    window = demo_window_create(window_name_has,
                                window_create_info,
                                backend_type,
                                false /* use_timeline */);

    return window;
}

/** TODO */
PRIVATE bool _raGL_backend_get_object(void*                   backend,
                                      ral_context_object_type object_type,
                                      void*                   object_ral,
                                      void**                  out_result_ptr)
{
    _raGL_backend*          backend_ptr  = (_raGL_backend*) backend;
    bool                    is_owner     = false;
    system_hash64map        map          = NULL;
    system_read_write_mutex map_rw_mutex = NULL;
    bool                    result       = false;

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
    _raGL_backend_get_object_vars(backend_ptr,
                                  object_type,
                                 &map_rw_mutex,
                                 &map,
                                 &is_owner);

    /* Try to find the object instance */
    result = true;

    system_read_write_mutex_lock(map_rw_mutex,
                                 ACCESS_READ);

    if (!system_hash64map_get(map,
                              (system_hash64) object_ral,
                              out_result_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Provided RAL object handle was not recognized");

        result = false;
    }

    system_read_write_mutex_unlock(map_rw_mutex,
                                   ACCESS_READ);

end:
    return result;
}

/** TODO */
PRIVATE void _raGL_backend_get_object_vars(_raGL_backend*           backend_ptr,
                                           ral_context_object_type  object_type,
                                           system_read_write_mutex* out_rw_mutex_ptr,
                                           system_hash64map*        out_hashmap_ptr,
                                           bool*                    out_is_owner_ptr)
{
    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            *out_hashmap_ptr  = backend_ptr->buffers_map;
            *out_rw_mutex_ptr = backend_ptr->buffers_map_rw_mutex;
            *out_is_owner_ptr = backend_ptr->buffers_map_owner;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER:
        {
            *out_hashmap_ptr  = backend_ptr->framebuffers_map;
            *out_rw_mutex_ptr = backend_ptr->framebuffers_map_rw_mutex;
            *out_is_owner_ptr = true;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            *out_hashmap_ptr  = backend_ptr->programs_map;
            *out_rw_mutex_ptr = backend_ptr->programs_map_rw_mutex;
            *out_is_owner_ptr = backend_ptr->programs_map_owner;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            *out_hashmap_ptr  = backend_ptr->samplers_map;
            *out_rw_mutex_ptr = backend_ptr->samplers_map_rw_mutex;
            *out_is_owner_ptr = backend_ptr->samplers_map_owner;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            *out_hashmap_ptr  = backend_ptr->shaders_map;
            *out_rw_mutex_ptr = backend_ptr->shaders_map_rw_mutex;
            *out_is_owner_ptr = backend_ptr->shaders_map_owner;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            *out_hashmap_ptr  = backend_ptr->textures_map;
            *out_rw_mutex_ptr = backend_ptr->textures_map_rw_mutex;
            *out_is_owner_ptr = backend_ptr->textures_map_owner;

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
PRIVATE void _raGL_backend_helper_context_renderer_callback(ogl_context context,
                                                            void*       unused)
{
    ral_scheduler scheduler = NULL;

    demo_app_get_property(DEMO_APP_PROPERTY_GPU_SCHEDULER,
                         &scheduler);

    ral_scheduler_use_backend_thread(scheduler,
                                     _global.backend_type);
}

/** TODO
 *
 *  NOTE: This function calls raGL_program_unlock(), once raGL_program_link() finishes.
 **/
PRIVATE void _raGL_backend_link_program_handler(void* program_raGL_raw)
{
    raGL_program program_raGL = (raGL_program) program_raGL_raw;

    if (!raGL_program_link(program_raGL) )
    {
        system_hashed_ansi_string program_name;
        ral_program               program_ral;

        raGL_program_get_property(program_raGL,
                                  RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                                 &program_ral);
        ral_program_get_property (program_ral,
                                  RAL_PROGRAM_PROPERTY_NAME,
                                 &program_name);

        LOG_ERROR("raGL_program_link() returned failure for program [%s]",
                  system_hashed_ansi_string_get_buffer(program_name) );

        ASSERT_DEBUG_SYNC(false,
                          "RAL program linking failed.");
    }

    raGL_program_unlock(program_raGL);
}

/** TODO */
PRIVATE void _raGL_backend_on_buffer_to_buffer_copy_request(const void* callback_arg_data,
                                                            void*       backend)
{
    _raGL_backend*                          backend_ptr      = (_raGL_backend*)                          backend;
    ral_buffer_copy_to_buffer_callback_arg* callback_arg_ptr = (ral_buffer_copy_to_buffer_callback_arg*) callback_arg_data;
    raGL_buffer                             dst_buffer_raGL  = NULL;
    raGL_buffer                             src_buffer_raGL  = NULL;

    /* Identify the raGL_buffer instances for the dst & src ral_buffer objects and forward
     * the request. */
    system_read_write_mutex_lock(backend_ptr->buffers_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) callback_arg_ptr->dst_buffer,
                                 &dst_buffer_raGL) ||
            !system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) callback_arg_ptr->src_buffer,
                                 &src_buffer_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_buffer instance found for the specified dst / src ral_buffer instances.");
        }
    }
    system_read_write_mutex_unlock(backend_ptr->buffers_map_rw_mutex,
                                   ACCESS_READ);

    if (dst_buffer_raGL != NULL &&
        src_buffer_raGL != NULL)
    {
        raGL_buffer_copy_buffer_to_buffer(dst_buffer_raGL,
                                          src_buffer_raGL,
                                          callback_arg_ptr->n_copy_ops,
                                          callback_arg_ptr->copy_ops);
    }
}

/** TODO */
PRIVATE void _raGL_backend_on_buffer_clear_region_request(const void* callback_arg_data,
                                                          void*       backend)
{
    _raGL_backend*                        backend_ptr      = (_raGL_backend*) backend;
    raGL_buffer                           buffer_raGL      = NULL;
    ral_buffer_clear_region_callback_arg* callback_arg_ptr = (ral_buffer_clear_region_callback_arg*) callback_arg_data;

    system_read_write_mutex_lock(backend_ptr->buffers_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) callback_arg_ptr->buffer,
                                 &buffer_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_buffer instance found for the specified ral_buffer instances.");
        }
    }
    system_read_write_mutex_unlock(backend_ptr->buffers_map_rw_mutex,
                                   ACCESS_READ);

    if (buffer_raGL != NULL)
    {
        raGL_buffer_clear_region(buffer_raGL,
                                 callback_arg_ptr->n_clear_ops,
                                 callback_arg_ptr->clear_ops);
    }
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
    system_read_write_mutex_lock(backend_ptr->buffers_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->buffers_map,
                                  (system_hash64) callback_arg_ptr->buffer,
                                 &buffer_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_buffer instance found for the specified ral_buffer instance.");
        }
    }
    system_read_write_mutex_unlock(backend_ptr->buffers_map_rw_mutex,
                                   ACCESS_READ);

    if (buffer_raGL != NULL)
    {
        raGL_buffer_update_regions_with_client_memory(buffer_raGL,
                                                      callback_arg_ptr->n_updates,
                                                      callback_arg_ptr->updates);
    }
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

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            for (uint32_t n_created_object = 0;
                          n_created_object < callback_arg_ptr->n_objects;
                        ++n_created_object)
            {
                _raGL_backend_subscribe_for_program_notifications(backend_ptr,
                                                                 (ral_program) callback_arg_ptr->created_objects[n_created_object],
                                                                 true); /* should_subscribe */
            }

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            for (uint32_t n_created_object = 0;
                          n_created_object < callback_arg_ptr->n_objects;
                        ++n_created_object)
            {
                _raGL_backend_subscribe_for_shader_notifications(backend_ptr,
                                                                 (ral_shader) callback_arg_ptr->created_objects[n_created_object],
                                                                 true); /* should_subscribe */
            }

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
    bool                                                           is_object_owner         = false;
    uint32_t                                                       n_objects_to_initialize = 0;
    system_hash64map                                               objects_map             = NULL;
    system_read_write_mutex                                        objects_map_rw_mutex    = NULL;
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
                                 &objects_map_rw_mutex,
                                 &objects_map,
                                 &is_object_owner);

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

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            /* IDs are assoiated by raGL_program */
            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            entrypoints_ptr->pGLGenSamplers(n_objects_to_initialize,
                                            result_object_ids_ptr);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            /* ID associated by raGL_shader */
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

            case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
            {
                ral_program program_ral = (ral_program) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                new_object = raGL_program_create(callback_arg_ptr->backend_ptr->context_ral,
                                                 program_ral);

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

            case RAL_CONTEXT_OBJECT_TYPE_SHADER:
            {
                ral_shader shader_ral = (ral_shader) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id];

                new_object = raGL_shader_create(callback_arg_ptr->backend_ptr->context_ral,
                                                context,
                                                shader_ral);

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

        system_read_write_mutex_lock(objects_map_rw_mutex,
                                     ACCESS_WRITE);
        {
            ASSERT_DEBUG_SYNC(!system_hash64map_contains(objects_map,
                                                         (system_hash64) new_object),
                              "Created GL backend instance is already recognized?");

            system_hash64map_insert(objects_map,
                                    (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->created_objects[n_object_id],
                                    new_object,
                                    NULL,  /* on_remove_callback */
                                    NULL); /* on_remove_callback_user_arg */

            /* For a few object types, we also need to fill additional id->object maps */
            switch (callback_arg_ptr->ral_callback_arg_ptr->object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
                {
                    GLuint       new_program_id   = 0;
                    raGL_program new_program_raGL = (raGL_program) new_object;

                    raGL_program_get_property(new_program_raGL,
                                              RAGL_PROGRAM_PROPERTY_ID,
                                             &new_program_id);

                    ASSERT_DEBUG_SYNC(new_program_id != 0,
                                      "New raGL_program's GL id is 0");

                    system_hash64map_insert(callback_arg_ptr->backend_ptr->program_id_to_raGL_program_map,
                                            (system_hash64) new_program_id,
                                            new_object,
                                            NULL,  /* on_remove_callback          */
                                            NULL); /* on_remove_callback_user_arg */

                    break;
                }

                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
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

                    break;
                } /* case RAL_CONTEXT_OBJECT_TYPE_TEXTURE: */
            } /* switch (callback_arg_ptr->ral_callback_arg_ptr->object_type) */
        }
        system_read_write_mutex_unlock(objects_map_rw_mutex,
                                       ACCESS_WRITE);
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
    _raGL_backend*                                           backend_ptr         = (_raGL_backend*)                                           backend;
    const ral_context_callback_objects_deleted_callback_arg* callback_arg_ptr    = (const ral_context_callback_objects_deleted_callback_arg*) callback_arg;
    bool                                                     is_owner            = false;
    system_hash64map                                         object_map          = NULL;
    system_read_write_mutex                                  object_map_rw_mutex = NULL;
    void*                                                    object_raGL         = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(backend_ptr != NULL,
                      "Backend instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg != NULL,
                      "Callback argument is NULL");

    _raGL_backend_get_object_vars(backend_ptr,
                                  callback_arg_ptr->object_type,
                                 &object_map_rw_mutex,
                                 &object_map,
                                 &is_owner);

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

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            for (uint32_t n_deleted_object = 0;
                          n_deleted_object < callback_arg_ptr->n_objects;
                        ++n_deleted_object)
            {
                _raGL_backend_subscribe_for_program_notifications(backend_ptr,
                                                                 (ral_program) callback_arg_ptr->deleted_objects[n_deleted_object],
                                                                 false); /* should_subscribe */
            }

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            for (uint32_t n_deleted_object = 0;
                          n_deleted_object < callback_arg_ptr->n_objects;
                        ++n_deleted_object)
            {
                _raGL_backend_subscribe_for_shader_notifications(backend_ptr,
                                                                 (ral_shader) callback_arg_ptr->deleted_objects[n_deleted_object],
                                                                 false); /* should_subscribe */
            }

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

    /* Remove the object from all hashmaps relevant to the object */
    system_read_write_mutex_lock(object_map_rw_mutex,
                                 ACCESS_WRITE);
    {
        /* Retrieve raGL object instances for the specified RAL objects */
        for (uint32_t n_deleted_object = 0;
                      n_deleted_object < callback_arg_ptr->n_objects;
                    ++n_deleted_object)
        {
            if (!system_hash64map_get(object_map,
                                      (system_hash64) callback_arg_ptr->deleted_objects[n_deleted_object],
                                     &object_raGL) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "No raGL instance found for the specified RAL object.");

                continue;
            }

            /* Remove the object from any relevant hash-maps we maintain */
            system_hash64map_remove(object_map,
                                    (system_hash64) callback_arg_ptr->deleted_objects[n_deleted_object]);

            switch (callback_arg_ptr->object_type)
            {
                case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
                {
                    GLuint object_id = 0;

                    raGL_program_get_property((raGL_program) object_raGL,
                                              RAGL_PROGRAM_PROPERTY_ID,
                                             &object_id);

                    ASSERT_DEBUG_SYNC(object_id != 0,
                                      "raGL_program's GL id is 0");

                    system_hash64map_remove(backend_ptr->program_id_to_raGL_program_map,
                                            (system_hash64) object_id);

                    break;
                }

                case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
                {
                    GLuint object_id = 0;

                    raGL_texture_get_property((raGL_texture) object_raGL,
                                              RAGL_TEXTURE_PROPERTY_ID,
                                             &object_id);

                    ASSERT_DEBUG_SYNC(object_id != 0,
                                      "raGL_texture's GL id is 0");

                    system_hash64map_remove(backend_ptr->texture_id_to_raGL_texture_map,
                                            (system_hash64) object_id);

                    break;
                }
            } /* switch (callback_arg_ptr->object_type) */
        } /* for (all deleted objects) */
    } /* for (all deleted objects) */
    system_read_write_mutex_unlock(object_map_rw_mutex,
                                   ACCESS_WRITE);
}

/** TODO */
PRIVATE void _raGL_backend_on_objects_deleted_rendering_callback(ogl_context context,
                                                                 void*       callback_arg)
{
    const _raGL_backend_on_objects_deleted_rendering_callback_arg* callback_arg_ptr    = (const _raGL_backend_on_objects_deleted_rendering_callback_arg*) callback_arg;
    const ogl_context_gl_entrypoints*                              entrypoints_ptr     = NULL;
    bool                                                           is_owner            = false;
    system_hash64map                                               object_map          = NULL;
    system_read_write_mutex                                        object_map_rw_mutex = NULL;
    void*                                                          object_raGL         = NULL;

    _raGL_backend_get_object_vars(callback_arg_ptr->backend_ptr,
                                  callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                 &object_map_rw_mutex,
                                 &object_map,
                                 &is_owner);

    if (object_map == NULL)
    {
        /* raGL objects have already been released, ignore the request. */
        goto end;
    }

    for (uint32_t n_deleted_object = 0;
                  n_deleted_object < callback_arg_ptr->ral_callback_arg_ptr->n_objects;
                ++n_deleted_object)
    {
        /* Retrieve raGL object instances for the specified RAL objects */
        if (!system_hash64map_get(object_map,
                                  (system_hash64) callback_arg_ptr->ral_callback_arg_ptr->deleted_objects[n_deleted_object],
                                 &object_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL instance found for the specified RAL object.");

            continue;
        }

        /* Release the raGL instance */
        _raGL_backend_release_raGL_object(callback_arg_ptr->backend_ptr,
                                          callback_arg_ptr->ral_callback_arg_ptr->object_type,
                                          object_raGL,
                                          (const void**) callback_arg_ptr->ral_callback_arg_ptr->deleted_objects[n_deleted_object]);
    } /* for (all deleted objects) */
end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_on_shader_attach_request(const void* callback_arg_data,
                                                    void*       backend)
{
    _raGL_backend*                                         backend_ptr      = (_raGL_backend*)                                         backend;
    _ral_program_callback_shader_attach_callback_argument* callback_arg_ptr = (_ral_program_callback_shader_attach_callback_argument*) callback_arg_data;
    const raGL_program                                     program_raGL     = ral_context_get_program_gl(backend_ptr->context_ral,
                                                                                                         callback_arg_ptr->program);
    bool                                                   result           = false;
    const raGL_shader                                      shader_raGL      = ral_context_get_shader_gl (backend_ptr->context_ral,
                                                                                                         callback_arg_ptr->shader);

    if (!raGL_program_attach_shader(program_raGL,
                                    shader_raGL) )
    {
        system_hashed_ansi_string program_name;

        ral_program_get_property(callback_arg_ptr->program,
                                 RAL_PROGRAM_PROPERTY_NAME,
                                &program_name);

        LOG_ERROR("raGL_program_attach_shader() returned failure for program [%s]",
                  system_hashed_ansi_string_get_buffer(program_name) );

        goto end;
    }

    /* If all shader stages are assigned shaders, we need to link the program.
     *
     * This needs to:
     *
     * 1) either be handled asynchronously, by scheduling a job via GPU scheduler, if
     *    this is an async request.
     * 2) or handled in the calling thread otherwise
     **/
    if (callback_arg_ptr->all_shader_stages_have_shaders_attached)
    {
        /* NOTE: unlock will be handled by link_program_handler */
        raGL_program_lock(program_raGL);

        if (callback_arg_ptr->async)
        {
            ral_scheduler_job_info new_job;
            ral_scheduler          scheduler = NULL;

            demo_app_get_property(DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                 &scheduler);

            new_job.callback_user_arg = program_raGL;
            new_job.pfn_callback_ptr  = _raGL_backend_link_program_handler;

            ral_scheduler_schedule_job(scheduler,
                                       _global.backend_type,
                                       new_job);
        }
        else
        {
            _raGL_backend_link_program_handler(program_raGL);
        }
    }

end:
    ;
}

/** TODO */
PRIVATE void _raGL_backend_on_shader_body_updated_notification(const void* callback_arg_data,
                                                               void*       backend)
{
    _raGL_backend*    backend_ptr       = (_raGL_backend*) backend;
    bool              result            = false;
    ral_shader        shader_to_compile = (ral_shader)     callback_arg_data;
    const raGL_shader shader_raGL       = ral_context_get_shader_gl(backend_ptr->context_ral,
                                                                    shader_to_compile);

    if (!raGL_shader_compile(shader_raGL) )
    {
        system_hashed_ansi_string shader_name;

        ral_shader_get_property(shader_to_compile,
                                RAL_SHADER_PROPERTY_NAME,
                               &shader_name);

        LOG_ERROR("raGL_shader_compile() returned failure for shader [%s]",
                  system_hashed_ansi_string_get_buffer(shader_name) );

        ASSERT_DEBUG_SYNC(false,
                          "RAL shader compilation failed.");
    }

    /* For each program which has the shader attached, we need to re-link the owning program.
     * We cannot do this asynchronously, in order to avoid thread races, where one of the rendering
     * threads may attempt to use a program object which is already outdated..
     *
     * NOTE: We could run into problems here, if other threads attempt to add or remove a program
     *       while another one is re-linked in another thread. TODO is to fix this.
     */
    {
        uint32_t      n_programs = 0;
        ral_scheduler scheduler  = NULL;

        demo_app_get_property        (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                     &scheduler);
        system_hash64map_get_property(backend_ptr->programs_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_programs);

        for (uint32_t n_program = 0;
                      n_program < n_programs;
                    ++n_program)
        {
            raGL_program program_raGL = NULL;
            ral_program  program_ral  = NULL;

            system_hash64map_get_element_at(backend_ptr->programs_map,
                                            n_program,
                                           &program_raGL,
                                            NULL); /* result_hash_ptr */

            raGL_program_get_property(program_raGL,
                                      RAGL_PROGRAM_PROPERTY_PARENT_RAL_PROGRAM,
                                     &program_ral);

            if (ral_program_is_shader_attached(program_ral,
                                               shader_to_compile) )
            {
                ral_scheduler_job_info new_job;

                /* NOTE: _raGL_backend_link_program_handler will unlock the raGL program,
                 *       once linking completes.
                 */
                raGL_program_lock(program_raGL);


                _raGL_backend_link_program_handler(program_raGL);
            }
        } /* for (all programs) */
    }
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
    system_read_write_mutex_lock(backend_ptr->textures_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->textures_map,
                                  (system_hash64) callback_arg_ptr->texture,
                                 &texture_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_texture instance found for the specified ral_texture instance.");
        }
    }
    system_read_write_mutex_unlock(backend_ptr->textures_map_rw_mutex,
                                   ACCESS_READ);

    if (texture_raGL != NULL)
    {
        raGL_texture_update_with_client_sourced_data(texture_raGL,
                                                     callback_arg_ptr->n_updates,
                                                     callback_arg_ptr->updates);
    }
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
    system_read_write_mutex_lock(backend_ptr->textures_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->textures_map,
                                  (system_hash64) texture,
                                 &texture_raGL) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "No raGL_texture instance found for the specified ral_texture instance.");
        }
    }
    system_read_write_mutex_unlock(backend_ptr->textures_map_rw_mutex,
                                   ACCESS_READ);

    if (texture_raGL != NULL)
    {
        raGL_texture_generate_mipmaps(texture_raGL);
    }
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

        case RAL_CONTEXT_OBJECT_TYPE_PROGRAM:
        {
            raGL_program_release( (raGL_program) object_raGL);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SAMPLER:
        {
            raGL_sampler_release( (raGL_sampler) object_raGL);

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_SHADER:
        {
            raGL_shader_release( (raGL_shader) object_raGL);

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

        /* Program notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_PROGRAMS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_PROGRAMS_DELETED,
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

        /* Shader notifications */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SHADERS_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_objects_created,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_SHADERS_DELETED,
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

        /* Program notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_PROGRAMS_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_PROGRAMS_DELETED,
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

        /* Shader notifications */
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SHADERS_CREATED,
                                                           _raGL_backend_on_objects_created,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_SHADERS_DELETED,
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
                                                        RAL_BUFFER_CALLBACK_ID_BUFFER_TO_BUFFER_COPY_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_buffer_to_buffer_copy_request,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(buffer_ral_callback_manager,
                                                        RAL_BUFFER_CALLBACK_ID_CLEAR_REGION_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_buffer_clear_region_request,
                                                        backend_ptr);
        system_callback_manager_subscribe_for_callbacks(buffer_ral_callback_manager,
                                                        RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_buffer_client_memory_sourced_update_request,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(buffer_ral_callback_manager,
                                                           RAL_BUFFER_CALLBACK_ID_BUFFER_TO_BUFFER_COPY_REQUESTED,
                                                           _raGL_backend_on_buffer_to_buffer_copy_request,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(buffer_ral_callback_manager,
                                                           RAL_BUFFER_CALLBACK_ID_CLEAR_REGION_REQUESTED,
                                                           _raGL_backend_on_buffer_clear_region_request,
                                                           backend_ptr);
        system_callback_manager_unsubscribe_from_callbacks(buffer_ral_callback_manager,
                                                           RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                                           _raGL_backend_on_buffer_client_memory_sourced_update_request,
                                                           backend_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_program_notifications(_raGL_backend* backend_ptr,
                                                               ral_program    program,
                                                               bool           should_subscribe)
{
    system_callback_manager program_ral_callback_manager = NULL;

    ral_program_get_property(program,
                             RAL_PROGRAM_PROPERTY_CALLBACK_MANAGER,
                            &program_ral_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(program_ral_callback_manager,
                                                        RAL_PROGRAM_CALLBACK_ID_SHADER_ATTACHED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_shader_attach_request,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(program_ral_callback_manager,
                                                           RAL_PROGRAM_CALLBACK_ID_SHADER_ATTACHED,
                                                           _raGL_backend_on_shader_attach_request,
                                                           backend_ptr);
    }
}

/** TODO */
PRIVATE void _raGL_backend_subscribe_for_shader_notifications(_raGL_backend* backend_ptr,
                                                              ral_shader     shader,
                                                              bool           should_subscribe)
{
    system_callback_manager shader_ral_callback_manager = NULL;

    ral_shader_get_property(shader,
                            RAL_SHADER_PROPERTY_CALLBACK_MANAGER,
                           &shader_ral_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(shader_ral_callback_manager,
                                                        RAL_SHADER_CALLBACK_ID_GLSL_BODY_UPDATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _raGL_backend_on_shader_body_updated_notification,
                                                        backend_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(shader_ral_callback_manager,
                                                           RAL_SHADER_CALLBACK_ID_GLSL_BODY_UPDATED,
                                                           _raGL_backend_on_shader_body_updated_notification,
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
    _raGL_backend* new_backend_ptr = NULL;

    /* Initialize helper contexts if none are available. */
    system_critical_section_enter(_global.cs);
    {
        if (_global.helper_contexts[0].helper_context == NULL)
        {
            _global.helper_contexts[0].helper_context = (ral_context) ~0;

            _global.init(type);

            _global.n_owners++;
        }
        else
        {
            bool all_helper_contexts_inited = true;

            for (uint32_t n_helper_context = 1;
                          n_helper_context < N_HELPER_CONTEXTS;
                        ++n_helper_context)
            {
                all_helper_contexts_inited &= (_global.helper_contexts[n_helper_context].helper_context != NULL);
            }

            if (all_helper_contexts_inited)
            {
                _global.n_owners++;
            }
        }
    }
    system_critical_section_leave(_global.cs);

    new_backend_ptr = new (std::nothrow) _raGL_backend(context_ral,
                                                       name,
                                                       type);

    ASSERT_ALWAYS_SYNC(new_backend_ptr != NULL,
                       "Out of memory");
    if (new_backend_ptr != NULL)
    {
        new_backend_ptr->context_ral = context_ral;

        if (_global.n_owners > 0)
        {
            new_backend_ptr->buffers  = ((_raGL_backend*) _global.helper_contexts[0].helper_backend)->buffers;
            new_backend_ptr->textures = ((_raGL_backend*) _global.helper_contexts[0].helper_backend)->textures;

            raGL_buffers_retain (new_backend_ptr->buffers);
            raGL_textures_retain(new_backend_ptr->textures);
        }

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
    _raGL_backend* root_backend_ptr = (_raGL_backend*) _global.helper_contexts[0].helper_backend;
    system_window  window           = NULL;
    bool           vsync_enabled    = false;

    /* Create a GL context  */
    ral_context_get_property(backend_ptr->context_ral,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
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
PUBLIC void raGL_backend_get_buffer_property_by_id(raGL_backend                 backend,
                                                   GLuint                       bo_id,
                                                   raGL_buffers_buffer_property property,
                                                   void*                        out_result_ptr)
{
    raGL_buffers_get_buffer_property(((_raGL_backend*) backend)->buffers,
                                     bo_id,
                                     property,
                                     out_result_ptr);
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
PUBLIC ral_context raGL_backend_get_helper_context(ral_backend_type type)
{
    ral_context result = NULL;

    system_critical_section_enter(_global.cs);
    {
        if (_global.helper_contexts[0].helper_context == NULL)
        {
            _global.init(type);
        }
        else
        {
            ASSERT_DEBUG_SYNC(type == _global.backend_type,
                              "Only one backend can be initialized during execution.");
        }

        result = _global.helper_contexts[0].helper_context;
    }
    system_critical_section_leave(_global.cs);

    return result;
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_program(void*       backend,
                                     ral_program program_ral,
                                     void**      out_program_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                    program_ral,
                                    out_program_raGL_ptr);
}

/** Please see header for specification */
PUBLIC bool raGL_backend_get_program_by_id(raGL_backend  backend,
                                           GLuint        program_id,
                                           raGL_program* out_program_raGL_ptr)
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

    if (program_id == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input program_id is 0");

        goto end;
    }

    if (out_program_raGL_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output variable is NULL");

        goto end;
    }

    /* Retrieve the requested texture object */
    result = true;

    system_read_write_mutex_lock(backend_ptr->programs_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->program_id_to_raGL_program_map,
                                  (system_hash64) program_id,
                                  out_program_raGL_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Program GL id [%d] is not recognized.",
                              program_id);

            result = false;
        }
    }
    system_read_write_mutex_unlock(backend_ptr->programs_map_rw_mutex,
                                   ACCESS_READ);

    /* All done */
end:
    return result;
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
PUBLIC bool raGL_backend_get_shader(void*      backend,
                                    ral_shader shader_ral,
                                    void**     out_shader_raGL_ptr)
{
    return _raGL_backend_get_object(backend,
                                    RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                    shader_ral,
                                    out_shader_raGL_ptr);
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
    result = true;

    system_read_write_mutex_lock(backend_ptr->textures_map_rw_mutex,
                                 ACCESS_READ);
    {
        if (!system_hash64map_get(backend_ptr->texture_id_to_raGL_texture_map,
                                  (system_hash64) texture_id,
                                  out_texture_raGL_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Texture GL id [%d] is not recognized.",
                              texture_id);

            result = false;
        }
    }
    system_read_write_mutex_unlock(backend_ptr->textures_map_rw_mutex,
                                   ACCESS_READ);

    /* All done */
end:
    return result;
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
        const bool is_helper_context = backend_ptr->is_helper_context;

        ogl_context_request_callback_from_context_thread(backend_ptr->context_gl,
                                                         _raGL_backend_release_rendering_callback,
                                                         backend_ptr);

        if (!is_helper_context)
        {
            ral_scheduler scheduler = NULL;

            demo_app_get_property             (DEMO_APP_PROPERTY_GPU_SCHEDULER,
                                              &scheduler);
            ral_scheduler_free_backend_threads(scheduler,
                                               _global.backend_type);

            system_critical_section_enter(_global.cs);
            {
                --_global.n_owners;

                if (_global.n_owners == 0)
                {
                    for (int32_t n_helper_context  = N_HELPER_CONTEXTS - 1;
                                 n_helper_context >= 0;
                               --n_helper_context)
                    {
                        demo_window_close(_global.helper_contexts[n_helper_context].helper_window);

                        _global.helper_contexts[n_helper_context].helper_backend = NULL;
                        _global.helper_contexts[n_helper_context].helper_context = NULL;
                        _global.helper_contexts[n_helper_context].helper_window  = NULL;
                    }
                }
            }
            system_critical_section_leave(_global.cs);
        }
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