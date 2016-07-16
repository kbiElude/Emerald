/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_backend.h"
#include "raGL/raGL_command_buffer.h"
#include "raGL/raGL_dep_tracker.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_rendering_handler.h"
#include "raGL/raGL_types.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_window.h"
#include "varia/varia_text_renderer.h"

enum
{
    /* Order must be reflected in raGL_rendering_handler_get_wait_event() */
    RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID,
    RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID,
};

typedef struct _raGL_rendering_handler
{
    ral_context           context_ral;
    system_window         context_window;
    ral_rendering_handler rendering_handler_ral;

    ral_rendering_handler_custom_wait_event_handler* handlers;

    ral_context  call_passthrough_context;
    bool         call_passthrough_mode;

    system_event bind_context_request_event;
    system_event bind_context_request_ack_event;
    system_event unbind_context_request_event;
    system_event unbind_context_request_ack_event;

    system_critical_section                     callback_request_cs;
    system_event                                callback_request_ack_event;
    system_event                                callback_request_event;
    void*                                       callback_request_user_arg;
    PFNRAGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc;

    bool                                    ral_callback_buffer_swap_needed;
    system_critical_section                 ral_callback_cs;
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK ral_callback_pfn_callback_proc;
    void*                                   ral_callback_user_arg;

    raGL_backend     backend;
    ral_backend_type backend_type;
    ogl_context      context_gl;
    bool             default_fb_has_depth_attachment;
    bool             default_fb_has_stencil_attachment;
    bool             default_fb_id_set;
    GLuint           default_fb_raGL_id;
    raGL_dep_tracker dep_tracker;
    bool             is_helper_context;
    bool             is_multisample_pf;
    bool             is_vsync_enabled;

    PFNGLBINDFRAMEBUFFERPROC  pGLBindFramebuffer;
    PFNGLBLITFRAMEBUFFERPROC  pGLBlitFramebuffer;
    PFNGLCLEARPROC            pGLClear;
    PFNGLCLEARCOLORPROC       pGLClearColor;
    PFNGLDISABLEPROC          pGLDisable;
    PFNGLENABLEPROC           pGLEnable;
    PFNGLFINISHPROC           pGLFinish;
    PFNGLMEMORYBARRIERPROC    pGLMemoryBarrier;
    PFNGLSCISSORPROC          pGLScissor;
    PFNGLVIEWPORTPROC         pGLViewport;


    explicit _raGL_rendering_handler(ral_rendering_handler in_rendering_handler_ral);

    ~_raGL_rendering_handler()
    {
        system_critical_section_release(ral_callback_cs);

        system_event_release(bind_context_request_event);
        system_event_release(bind_context_request_ack_event);
        system_event_release(callback_request_ack_event);
        system_event_release(callback_request_event);
        system_event_release(unbind_context_request_event);
        system_event_release(unbind_context_request_ack_event);

        delete [] handlers;
        handlers = nullptr;

        /* Release user callback serialization cs */
        system_critical_section_release(callback_request_cs);

    }
} _raGL_rendering_handler;

PRIVATE void _raGL_rendering_handler_rendering_thread_callback_requested_event_handler(uint32_t ignored,
                                                                                       void*    rendering_handler_raBackend);
PRIVATE void _raGL_rendering_handler_context_sharing_setup_request_event_handler      (uint32_t ignored,
                                                                                       void*    rendering_handler_raBackend);


static const ral_rendering_handler_custom_wait_event_handler ref_handlers[] =
{
    { nullptr, RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID, _raGL_rendering_handler_rendering_thread_callback_requested_event_handler},
    { nullptr, RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID,     _raGL_rendering_handler_context_sharing_setup_request_event_handler}
};
static const uint32_t n_ref_handlers = sizeof(ref_handlers) / sizeof(ref_handlers[0]);


_raGL_rendering_handler::_raGL_rendering_handler(ral_rendering_handler in_rendering_handler_ral)
{
    context_ral           = nullptr;
    rendering_handler_ral = in_rendering_handler_ral;

    call_passthrough_context = nullptr;
    call_passthrough_mode    = false;

    bind_context_request_event       = system_event_create(false); /* manual_reset */
    bind_context_request_ack_event   = system_event_create(false); /* manual_reset */
    callback_request_ack_event       = system_event_create(false); /* manual_reset */
    callback_request_event           = system_event_create(false); /* manual_reset */
    callback_request_cs              = system_critical_section_create();
    callback_request_user_arg        = nullptr;
    is_helper_context                = false;
    is_multisample_pf                = false;
    is_vsync_enabled                 = false;
    pfn_callback_proc                = nullptr;
    unbind_context_request_event     = system_event_create (false); /* manual_reset */
    unbind_context_request_ack_event = system_event_create (false); /* manual_reset */

    ral_callback_buffer_swap_needed = false;
    ral_callback_cs                 = system_critical_section_create();

    backend                           = nullptr;
    backend_type                      = RAL_BACKEND_TYPE_UNKNOWN;
    context_gl                        = nullptr;
    default_fb_has_depth_attachment   = false;
    default_fb_has_stencil_attachment = false;
    default_fb_id_set                 = false;
    default_fb_raGL_id                = -1;
    dep_tracker                       = nullptr;

    pGLBindFramebuffer = nullptr;
    pGLBlitFramebuffer = nullptr;
    pGLClear           = nullptr;
    pGLClearColor      = nullptr;
    pGLDisable         = nullptr;
    pGLEnable          = nullptr;
    pGLFinish          = nullptr;
    pGLMemoryBarrier   = nullptr;
    pGLScissor         = nullptr;
    pGLViewport        = nullptr;

    handlers = new ral_rendering_handler_custom_wait_event_handler[n_ref_handlers];

    memcpy(handlers,
           ref_handlers,
           n_ref_handlers * sizeof(ral_rendering_handler_custom_wait_event_handler) );
    
    handlers[RAGL_RENDERING_HANDLER_RENDERING_THREAD_CALLBACK_REQUEST_ID].event = callback_request_event;
    handlers[RAGL_RENDERING_HANDLER_CONTEXT_SHARING_SETUP_REQUEST_ID].event     = unbind_context_request_event;
}


/** TODO */
PRIVATE void _raGL_rendering_handler_context_sharing_setup_request_event_handler(uint32_t ignored,
                                                                                 void*    rendering_handler_raBackend)
{
    _raGL_rendering_handler* rendering_handler_ptr = static_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    /* This event is used for setting up context sharing. In order for it to succeed, we must unbind the context from
     * current thread, then wait for a 'job done' signal, and bind the context back to this thread */
    ogl_context_unbind_from_current_thread(rendering_handler_ptr->context_gl);

    system_event_set        (rendering_handler_ptr->unbind_context_request_ack_event);
    system_event_wait_single(rendering_handler_ptr->bind_context_request_event);

    ogl_context_bind_to_current_thread(rendering_handler_ptr->context_gl);

    system_event_set(rendering_handler_ptr->bind_context_request_ack_event);
}

/** TODO */
PRIVATE void _raGL_rendering_handler_ral_based_rendering_callback_handler(ogl_context context,
                                                                          void*       rendering_handler)
{
    ral_present_job          present_job;
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    present_job = rendering_handler_ptr->ral_callback_pfn_callback_proc(rendering_handler_ptr->context_ral,
                                                                        -1,      /* frame_time                */
                                                                        -1,      /* n_frame                   */
                                                                        nullptr, /* rendering_area_px_topdown */
                                                                        rendering_handler_ptr->ral_callback_user_arg);

    raGL_rendering_handler_execute_present_job(reinterpret_cast<ral_rendering_handler>(rendering_handler),
                                               present_job);

    ral_present_job_release(present_job);
}

/** TODO */
PRIVATE void _raGL_rendering_handler_rendering_thread_callback_requested_event_handler(uint32_t ignored,
                                                                                       void*    rendering_handler_raBackend)
{
    bool                     needs_buffer_swap     = true;
    _raGL_rendering_handler* rendering_handler_ptr = static_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    /* Call-back requested. Sync and handle the request */
    raGL_backend_sync();

    /* Reset dependency tracker and force full cache flush. Expectation is that each frame's contents
     * is going to be rendered by submitting a single present job, and that rendering thread callbacks
     * are exclusive to back-end usage (CPU->GPU transfers, GL object creation & set-up, etc.), so this
     * should not hurt performance too much, but will keep us safe.
     */
    raGL_dep_tracker_reset(rendering_handler_ptr->dep_tracker);

    rendering_handler_ptr->pGLMemoryBarrier (GL_ALL_BARRIER_BITS);

    /* Execute GL commands */
    rendering_handler_ptr->pfn_callback_proc(rendering_handler_ptr->context_gl,
                                             rendering_handler_ptr->callback_request_user_arg);

    if (rendering_handler_ptr->ral_callback_buffer_swap_needed)
    {
        uint32_t window_size    [2];
        uint32_t window_x1y1x2y2[4];

        ASSERT_DEBUG_SYNC(!rendering_handler_ptr->is_helper_context,
                          "Buffer swaps are unavailable for helper contexts");

        if (rendering_handler_ptr->default_fb_raGL_id == -1)
        {
            ral_context      context_ral     = nullptr;
            raGL_framebuffer default_fb_raGL = nullptr;

            ogl_context_get_property(rendering_handler_ptr->context_gl,
                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                                    &default_fb_raGL);

            raGL_framebuffer_get_property(default_fb_raGL,
                                          RAGL_FRAMEBUFFER_PROPERTY_ID,
                                         &rendering_handler_ptr->default_fb_raGL_id);
        }

        /* Blit the context FBO's contents to the back buffer */
        system_window_get_property(rendering_handler_ptr->context_window,
                                   SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                                   window_x1y1x2y2);

        window_size[0] = window_x1y1x2y2[2] - window_x1y1x2y2[0];
        window_size[1] = window_x1y1x2y2[3] - window_x1y1x2y2[1];

        rendering_handler_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                                  0);
        rendering_handler_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                                  rendering_handler_ptr->default_fb_raGL_id);

        rendering_handler_ptr->pGLBlitFramebuffer(0,              /* srcX0 */
                                                  0,              /* srcY0 */
                                                  window_size[0], /* srcX1 */
                                                  window_size[1], /* srcY1 */
                                                  0,              /* dstX0 */
                                                  0,              /* dstY0 */
                                                  window_size[0], /* dstX1 */
                                                  window_size[1], /* dstY1 */
                                                  GL_COLOR_BUFFER_BIT,
                                                  GL_NEAREST);

        ogl_context_swap_buffers(rendering_handler_ptr->context_gl);
    }

    /* Reset callback data */
    rendering_handler_ptr->callback_request_user_arg       = nullptr;
    rendering_handler_ptr->pfn_callback_proc               = nullptr;
    rendering_handler_ptr->ral_callback_buffer_swap_needed = false;

    /* Set ack event */
    system_event_set(rendering_handler_ptr->callback_request_ack_event);
}


/** TODO */
PUBLIC void* raGL_rendering_handler_create(ral_rendering_handler rendering_handler_ral)
{
    _raGL_rendering_handler* new_handler_ptr = nullptr;

    new_handler_ptr = new (std::nothrow) _raGL_rendering_handler(rendering_handler_ral);

    return new_handler_ptr;
}

/* TODO */
PUBLIC void raGL_rendering_handler_enumerate_custom_wait_event_handlers(void*                                                   rendering_handler_backend,
                                                                        uint32_t*                                               opt_out_n_custom_wait_event_handlers_ptr,
                                                                        const ral_rendering_handler_custom_wait_event_handler** opt_out_custom_wait_event_handlers_ptr)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_backend);

    if (opt_out_n_custom_wait_event_handlers_ptr != nullptr)
    {
        *opt_out_n_custom_wait_event_handlers_ptr = n_ref_handlers;
    }

    if (opt_out_custom_wait_event_handlers_ptr != nullptr)
    {
        *opt_out_custom_wait_event_handlers_ptr = rendering_handler_ptr->handlers;
    }
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void raGL_rendering_handler_execute_present_job(void*           rendering_handler_raGL,
                                                                              ral_present_job present_job)
{
    uint32_t                 n_present_job_tasks   = 0;
    ral_present_task*        present_job_tasks     = nullptr;
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raGL);

    if (ral_present_job_get_sorted_tasks(present_job,
                                        &n_present_job_tasks,
                                        &present_job_tasks))
    {
        /* Present tasks need to be executed one-after-another. We theoretically could distribute these to
         * separate worker rendering threads, but context sharing is defined pretty loosely and GL implementations
         * are pretty crap at supporting it.
         */
        for (uint32_t n_present_task = 0;
                      n_present_task < n_present_job_tasks;
                    ++n_present_task)
        {
            /* NOTE: GL back-end doesn't really care about inputs & output synchronization, since GL
             *       does not support buffer-/image-based region sync, as eg. Vulkan does.
             */
            raGL_command_buffer task_cmd_buffer_raGL = nullptr;
            ral_command_buffer  task_cmd_buffer_ral  = nullptr;

            ral_present_task_get_property(present_job_tasks[n_present_task],
                                          RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER,
                                         &task_cmd_buffer_ral);

            raGL_backend_get_command_buffer(rendering_handler_ptr->backend,
                                            task_cmd_buffer_ral,
                                           &task_cmd_buffer_raGL);

            raGL_command_buffer_execute(task_cmd_buffer_raGL,
                                        rendering_handler_ptr->dep_tracker);
        }

        ral_present_job_get_property(present_job,
                                     RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED,
                                    &rendering_handler_ptr->ral_callback_buffer_swap_needed);

        if (rendering_handler_ptr->call_passthrough_mode)
        {
            ASSERT_DEBUG_SYNC(!rendering_handler_ptr->ral_callback_buffer_swap_needed,
                              "Cannot swap buffers");
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve a sorted list of present tasks");
    }
}

/* TODO */
PUBLIC void raGL_rendering_handler_init_from_rendering_thread(ral_context           context_ral,
                                                              ral_rendering_handler rendering_handler_ral,
                                                              void*                 rendering_handler_raGL)
{
    system_window            context_window                = nullptr;
    unsigned char            context_window_n_depth_bits   = 0;
    unsigned char            context_window_n_stencil_bits = 0;
    unsigned char            context_window_n_samples      = 0;
    system_pixel_format      context_window_pf             = nullptr;
    _raGL_rendering_handler* rendering_handler_ptr         = static_cast<_raGL_rendering_handler*>(rendering_handler_raGL);

    ral_context_get_property (context_ral,
                              RAL_CONTEXT_PROPERTY_BACKEND,
                             &rendering_handler_ptr->backend);
    raGL_backend_get_property(rendering_handler_ptr->backend,
                              RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                             &rendering_handler_ptr->context_gl);

    raGL_backend_get_private_property(rendering_handler_ptr->backend,
                                      RAGL_BACKEND_PRIVATE_PROPERTY_DEP_TRACKER,
                                     &rendering_handler_ptr->dep_tracker);

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context_gl != nullptr,
                      "raGL_context instance is NULL");
    ASSERT_DEBUG_SYNC(rendering_handler_ptr->dep_tracker != nullptr,
                      "NULL dep tracker retrieved.");

    rendering_handler_ptr->context_ral = context_ral;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context_gl != nullptr,
                      "Active GL context is NULL");

    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &rendering_handler_ptr->backend_type);
    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_IS_HELPER_CONTEXT,
                            &rendering_handler_ptr->is_helper_context);
    ogl_context_get_property(rendering_handler_ptr->context_gl,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &context_window);

    ASSERT_DEBUG_SYNC(context_window != nullptr,
                      "No window associated with the GL context");

    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                              &context_window_pf);
    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_IS_VSYNC_ENABLED,
                              &rendering_handler_ptr->is_vsync_enabled);

    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES,
                                    &context_window_n_samples);
    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_DEPTH_BITS,
                                    &context_window_n_depth_bits);
    system_pixel_format_get_property(context_window_pf,
                                     SYSTEM_PIXEL_FORMAT_PROPERTY_STENCIL_BITS,
                                    &context_window_n_stencil_bits);

    rendering_handler_ptr->context_window                    = context_window;
    rendering_handler_ptr->default_fb_has_depth_attachment   = (context_window_n_depth_bits   >  0);
    rendering_handler_ptr->default_fb_has_stencil_attachment = (context_window_n_stencil_bits >  0);
    rendering_handler_ptr->is_multisample_pf                 = (context_window_n_samples      >  1);

    if (rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entrypoints_ptr = nullptr;

        ogl_context_get_property(rendering_handler_ptr->context_gl,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entrypoints_ptr);

        rendering_handler_ptr->pGLBindFramebuffer = entrypoints_ptr->pGLBindFramebuffer;
        rendering_handler_ptr->pGLBlitFramebuffer = entrypoints_ptr->pGLBlitFramebuffer;
        rendering_handler_ptr->pGLClear           = entrypoints_ptr->pGLClear;
        rendering_handler_ptr->pGLClearColor      = entrypoints_ptr->pGLClearColor;
        rendering_handler_ptr->pGLDisable         = entrypoints_ptr->pGLDisable;
        rendering_handler_ptr->pGLEnable          = entrypoints_ptr->pGLEnable;
        rendering_handler_ptr->pGLFinish          = entrypoints_ptr->pGLFinish;
        rendering_handler_ptr->pGLMemoryBarrier   = entrypoints_ptr->pGLMemoryBarrier;
        rendering_handler_ptr->pGLScissor         = entrypoints_ptr->pGLScissor;
        rendering_handler_ptr->pGLViewport        = entrypoints_ptr->pGLViewport;
    }
    else
    {
        const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;

        ASSERT_DEBUG_SYNC(rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL,
                          "Unrecognized backend type");

        ogl_context_get_property(rendering_handler_ptr->context_gl,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

        rendering_handler_ptr->pGLBindFramebuffer = entrypoints_ptr->pGLBindFramebuffer;
        rendering_handler_ptr->pGLBlitFramebuffer = entrypoints_ptr->pGLBlitFramebuffer;
        rendering_handler_ptr->pGLClear           = entrypoints_ptr->pGLClear;
        rendering_handler_ptr->pGLClearColor      = entrypoints_ptr->pGLClearColor;
        rendering_handler_ptr->pGLDisable         = entrypoints_ptr->pGLDisable;
        rendering_handler_ptr->pGLEnable          = entrypoints_ptr->pGLEnable;
        rendering_handler_ptr->pGLFinish          = entrypoints_ptr->pGLFinish;
        rendering_handler_ptr->pGLMemoryBarrier   = entrypoints_ptr->pGLMemoryBarrier;
        rendering_handler_ptr->pGLScissor         = entrypoints_ptr->pGLScissor;
        rendering_handler_ptr->pGLViewport        = entrypoints_ptr->pGLViewport;
    }

    ogl_context_bind_to_current_thread(rendering_handler_ptr->context_gl);
}

/** TODO */
PUBLIC void raGL_rendering_handler_lock_bound_context(raGL_rendering_handler rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    if (!ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) )
    {
        system_event_set        (rendering_handler_ptr->unbind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->unbind_context_request_ack_event);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_post_draw_frame(void* rendering_handler_raBackend,
                                                   bool  has_rendered_frame)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);
    uint32_t                 window_size[2];
    uint32_t                 window_x1y1x2y2[4];

    system_window_get_property(rendering_handler_ptr->context_window,
                               SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                               window_x1y1x2y2);

    window_size[0] = window_x1y1x2y2[2] - window_x1y1x2y2[0];
    window_size[1] = window_x1y1x2y2[3] - window_x1y1x2y2[1];

    if (!has_rendered_frame)
    {
        /* Well, if we get here, we're in trouble. Clear the color buffer with red color
         * to indicate utter disaster. */
        rendering_handler_ptr->pGLClearColor(1.0f,  /* red   */
                                             0.0f,  /* green */
                                             0.0f,  /* blue  */
                                             1.0f); /* alpha */

        rendering_handler_ptr->pGLClear(GL_COLOR_BUFFER_BIT);
    }

    if (rendering_handler_ptr->is_multisample_pf                   &&
        rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        rendering_handler_ptr->pGLDisable(GL_MULTISAMPLE);
    }

    /* Blit the context FBO's contents to the back buffer */
    rendering_handler_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              0);
    rendering_handler_ptr->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                              rendering_handler_ptr->default_fb_raGL_id);

    rendering_handler_ptr->pGLBlitFramebuffer(0,              /* srcX0 */
                                              0,              /* srcY0 */
                                              window_size[0], /* srcX1 */
                                              window_size[1], /* srcY1 */
                                              0,              /* dstX0 */
                                              0,              /* dstY0 */
                                              window_size[0], /* dstX1 */
                                              window_size[1], /* dstY1 */
                                              GL_COLOR_BUFFER_BIT,
                                              GL_NEAREST);
}

/** TODO */
PUBLIC void raGL_rendering_handler_pre_draw_frame(void* rendering_handler_raBackend)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    ASSERT_DEBUG_SYNC(!rendering_handler_ptr->is_helper_context,
                      "Playback is unavailable for helper contexts");

    /* Sync with other contexts.. */
    raGL_backend_sync();

    /* Bind the context's default FBO and call the user app's call-back */
    if (!rendering_handler_ptr->default_fb_id_set)
    {
        raGL_framebuffer default_fb_raGL = nullptr;

        ogl_context_get_property(rendering_handler_ptr->context_gl,
                                 OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                                &default_fb_raGL);

        raGL_framebuffer_get_property(default_fb_raGL,
                                      RAGL_FRAMEBUFFER_PROPERTY_ID,
                                     &rendering_handler_ptr->default_fb_raGL_id);

        rendering_handler_ptr->default_fb_id_set = true;
    }

   rendering_handler_ptr->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                              rendering_handler_ptr->default_fb_raGL_id);

    /* Clear the attachments before we head on. Disable any rendering modes which
     * would affect the process */
    rendering_handler_ptr->pGLClearColor(0.0f,
                                         0.0f,
                                         0.0f,
                                         1.0f);
    rendering_handler_ptr->pGLClear     (GL_COLOR_BUFFER_BIT                                                                   |
                                        (rendering_handler_ptr->default_fb_has_depth_attachment   ? GL_DEPTH_BUFFER_BIT   : 0) |
                                        (rendering_handler_ptr->default_fb_has_stencil_attachment ? GL_STENCIL_BUFFER_BIT : 0) );

    /* Enable multisampling if needed */
    if (rendering_handler_ptr->is_multisample_pf && rendering_handler_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        rendering_handler_ptr->pGLEnable(GL_MULTISAMPLE);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_present_frame(void*                   rendering_handler_raBackend,
                                                 system_critical_section rendering_cs)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_raBackend);

    ogl_context_swap_buffers(rendering_handler_ptr->context_gl);

    /* If vertical sync is enabled, force a full pipeline flush + GPU-side execution
     * of the enqueued commands. This helps fighting the tearing A LOT for scenes
     * that take more time to render (eg. the test "greets" scene which slides
     * from right to left), since the frame misses are less frequent. It also puts
     * less pressure on the CPU, since we're no longer spinning.
     */
    if (rendering_handler_ptr->is_vsync_enabled)
    {
        system_critical_section_leave(rendering_cs);
        {
            rendering_handler_ptr->pGLFinish();
        }
        system_critical_section_enter(rendering_cs);
    }
}

/** TODO */
PUBLIC void raGL_rendering_handler_release(void* rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    delete rendering_handler_ptr;
}

/** Please see header for specification */
PUBLIC bool raGL_rendering_handler_request_callback_for_ral_rendering_handler(void*                                   rendering_handler_backend,
                                                                              PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                              void*                                   user_arg,
                                                                              raGL_rendering_handler_execution_mode   execution_mode)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler_backend);
    bool                     result;

    system_critical_section_enter(rendering_handler_ptr->ral_callback_cs);
    {
        rendering_handler_ptr->ral_callback_pfn_callback_proc = pfn_callback_proc;
        rendering_handler_ptr->ral_callback_user_arg          = user_arg;

        if (ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) ||
            rendering_handler_ptr->call_passthrough_mode)
        {
            _raGL_rendering_handler_ral_based_rendering_callback_handler(rendering_handler_ptr->context_gl,
                                                                         user_arg);

            result = true;
        }
        else
        {
            bool should_continue = false;

            if (execution_mode != RAGL_RENDERING_HANDLER_EXECUTION_MODE_ONLY_IF_IDLE_BLOCK_TILL_FINISHED)
            {
                system_critical_section_enter(rendering_handler_ptr->callback_request_cs);

                should_continue = true;
            }
            else
            {
                should_continue = system_critical_section_try_enter(rendering_handler_ptr->callback_request_cs);
            }

            if (should_continue)
            {
                rendering_handler_ptr->callback_request_user_arg = user_arg;
                rendering_handler_ptr->pfn_callback_proc         = _raGL_rendering_handler_ral_based_rendering_callback_handler;

                system_event_set(rendering_handler_ptr->callback_request_event);

                if (execution_mode != RAGL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_DONT_BLOCK)
                {
                    system_event_wait_single(rendering_handler_ptr->callback_request_ack_event);
                }

                system_critical_section_leave(rendering_handler_ptr->callback_request_cs);
            }

            result = should_continue;
        }

        rendering_handler_ptr->ral_callback_pfn_callback_proc = nullptr;
        rendering_handler_ptr->ral_callback_user_arg          = nullptr;
    }
    system_critical_section_leave(rendering_handler_ptr->ral_callback_cs);

    return result;
}

/** Please see header for specification */
PUBLIC void raGL_rendering_handler_set_property(raGL_rendering_handler           rendering_handler,
                                                raGL_rendering_handler_property  property,
                                                const void*                      value)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    switch (property)
    {
        case RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_CONTEXT:
        {
            rendering_handler_ptr->call_passthrough_context = *reinterpret_cast<const ral_context*>(value);

            break;
        }

        case RAGL_RENDERING_HANDLER_PROPERTY_CALL_PASSTHROUGH_MODE:
        {
            rendering_handler_ptr->call_passthrough_mode = *reinterpret_cast<const bool*>(value);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized raGL_rendering_handler_property value");
        }
    }

}

/** TODO */
PUBLIC void raGL_rendering_handler_unlock_bound_context(raGL_rendering_handler rendering_handler)
{
    _raGL_rendering_handler* rendering_handler_ptr = reinterpret_cast<_raGL_rendering_handler*>(rendering_handler);

    if (!ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler_ptr->rendering_handler_ral) )
    {
        system_event_set        (rendering_handler_ptr->bind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->bind_context_request_ack_event);
    }
}
