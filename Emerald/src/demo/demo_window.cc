/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "audio/audio_stream.h"
#include "demo/demo_app.h"
#include "demo/demo_flyby.h"
#include "demo/demo_loader.h"
#include "demo/demo_window.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_window.h"


typedef struct _demo_window
{
    system_hashed_ansi_string name;

    ral_backend_type backend_type;
    ral_context      context;       /* DO NOT release */
    demo_flyby       flyby;

    PFNDEMOWINDOWLOADERSETUPCALLBACKPROC pfn_loader_setup_callback_proc;
    void*                                loader_setup_callback_proc_user_arg;

    demo_loader   loader;
    demo_timeline timeline;

    uint32_t              refresh_rate;
    ral_rendering_handler rendering_handler;
    system_time           rendering_start_time;
    uint32_t              resolution[2];
    bool                  should_run_fullscreen;
    uint32_t              target_frame_rate;
    bool                  use_timeline;
    bool                  use_vsync;
    bool                  visible;
    system_window         window;

    system_event          shut_down_event;
    system_event          window_closed_event;

    explicit _demo_window(system_hashed_ansi_string in_name,
                          ral_backend_type          in_backend_type,
                          bool                      in_use_timeline)
    {
        backend_type                        = in_backend_type;
        context                             = nullptr;
        flyby                               = nullptr;
        loader                              = nullptr;
        loader_setup_callback_proc_user_arg = nullptr;
        name                                = in_name;
        pfn_loader_setup_callback_proc      = nullptr;
        refresh_rate                        = 60;
        rendering_handler                   = nullptr;
        rendering_start_time                = -1;
        resolution[0]                       = 1280;
        resolution[1]                       = 720;
        should_run_fullscreen               = false;
        shut_down_event                     = system_event_create(true); /* manual_reset */
        target_frame_rate                   = 60;
        timeline                            = nullptr;
        use_timeline                        = in_use_timeline;
        use_vsync                           = true;
        visible                             = true;
        window                              = nullptr;
        window_closed_event                 = system_event_create(true); /* manual_reset */
    }

    ~_demo_window()
    {
        ASSERT_DEBUG_SYNC(window == nullptr,
                          "Rendering window is not NULL at demo_window teardown time");

        if (flyby != nullptr)
        {
            demo_flyby_release(flyby);

            flyby = nullptr;
        }

        if (shut_down_event != nullptr)
        {
            system_event_release(shut_down_event);

            shut_down_event = nullptr;
        }

        if (window_closed_event != nullptr)
        {
            system_event_release(window_closed_event);

            window_closed_event = nullptr;
        }
    }
} _demo_window;

/* Forward declarations */
PRIVATE bool _demo_window_init                              (_demo_window* window_ptr);
PRIVATE void _demo_window_on_audio_stream_finished_playing  (const void*   callback_data,
                                                             void*         user_arg);
PRIVATE void _demo_window_window_closed_callback_handler    (void*         unused,
                                                             void*         window);
PRIVATE void _demo_window_window_closing_callback_handler   (void*         unused,
                                                             void*         window);
PRIVATE void _demo_window_subscribe_for_window_notifications(_demo_window* window_ptr,
                                                             bool          should_subscribe);

/** TODO */
PRIVATE bool _demo_window_init(_demo_window* window_ptr)
{
    system_pixel_format pixel_format = nullptr; /* ownership will be taken by the window */
    bool                result       = false;
    char                rendering_handler_name[256];

    /* Sanity checks */
    if (window_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(window_ptr != nullptr,
                          "Input demo_window instance is NULL");

        goto end;
    }

    if (window_ptr->window != nullptr)
    {
        ASSERT_DEBUG_SYNC(window_ptr->window == nullptr,
                          "The specified window is already shown.");

        goto end;
    }

    /* Fill out a pixel format descriptor.
     *
     * Emerald maintains its own "default framebuffer", to which rendering handlers should render.
     * Its attachments will be initialized, according to the descriptor we're just about to configure.
     *
     * Reason for this is cross-compatibility. MSAA set-up is significantly different under Linux
     * and Windows, and it's simpler to reconcile both platforms this way. For multisampling, simply
     * render to multisample framebuffers and then do a resolve right before the buffer swap op.
     */
    pixel_format = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                              8,  /* color_buffer_green_bits */
                                              8,  /* color_buffer_blue_bits  */
                                              0,  /* color_buffer_alpha_bits */
                                              16, /* depth_buffer_bits       */
                                              1,  /* n_samples               */
                                              0); /* stencil_buffer_bits     */

    if (window_ptr->should_run_fullscreen)
    {
        system_screen_mode screen_mode = nullptr;

        if (!system_screen_mode_get_for_resolution(window_ptr->resolution[0],
                                                   window_ptr->resolution[1],
                                                   window_ptr->refresh_rate,
                                                  &screen_mode) )
        {
            LOG_FATAL("No suitable screen mode found for resolution [%u x %u x %uHz]",
                      window_ptr->resolution[0],
                      window_ptr->resolution[1],
                      window_ptr->refresh_rate);

            goto end;
        }

        window_ptr->window = system_window_create_fullscreen((demo_window) window_ptr,
                                                             window_ptr->backend_type,
                                                             screen_mode,
                                                             window_ptr->use_vsync,
                                                             pixel_format);
    }
    else
    {
        int window_x1y1x2y2[4] = {0};

        /* Determine centered position for a renderer window of the specified size. */
        system_window_get_centered_window_position_for_primary_monitor(reinterpret_cast<const int*>(window_ptr->resolution),
                                                                        window_x1y1x2y2);

        /* Spawn the window. */
        window_ptr->window = system_window_create_not_fullscreen((demo_window) window_ptr,
                                                                 window_ptr->backend_type,
                                                                 window_x1y1x2y2,
                                                                 window_ptr->name,
                                                                 false, /* scalable */
                                                                 window_ptr->use_vsync,
                                                                 window_ptr->visible,
                                                                 pixel_format);
    }

    if (window_ptr->window == nullptr)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not create a rendering window");

        goto end;
    }

    /* Set up the rendering handler.
     *
     * The object does a few important things:
     *
     * 1) It's responsible for the rendering thread's lifetime.
     * 2) Controls the frame refresh rate (as in: rendering handler call-back frequency and timing). It can
     *    work in a number of modes; we're interested in a constant frame-rate, so we configure it to work
     *    with FPS policy, but it could also be configured to render as many frames as possible, or only if
     *    explicitly requested by making ogl_rendering_handler_request_callback_from_context_thread() calls.
     *
     *    For FPS policy, the playback is stopped by default. ogl_rendering_handler_play() call must be
     *    issued to start the rendering process. The "request callback from context thread" calls are also available,
     *    and can be made at any time from both the rendering thread or any other thread within the process.
     *
     * 3) Provides additional functionality needed for correct timeline & vsync support. Feel free to have a lurk
     *    if curious.
     */
    ASSERT_DEBUG_SYNC(window_ptr->rendering_handler == nullptr,
                      "An ogl_rendering_handler instance is already present.");

    snprintf(rendering_handler_name,
             sizeof(rendering_handler_name),
             "Rendering handler for window [%s]",
             system_hashed_ansi_string_get_buffer(window_ptr->name) );


    if (window_ptr->target_frame_rate == 0)
    {
        window_ptr->rendering_handler = ral_rendering_handler_create_with_render_per_request_policy(window_ptr->backend_type,
                                                                                                    system_hashed_ansi_string_create(rendering_handler_name),
                                                                                                    nullptr,  /* pfn_rendering_callback */
                                                                                                    nullptr); /* user_arg               */
    }
    else
    if (window_ptr->target_frame_rate == ~0)
    {
        window_ptr->rendering_handler = ral_rendering_handler_create_with_max_performance_policy(window_ptr->backend_type,
                                                                                                 system_hashed_ansi_string_create(rendering_handler_name),
                                                                                                 nullptr,  /* pfn_rendering_callback */
                                                                                                 nullptr); /* user_arg               */
    }
    else
    {
        window_ptr->rendering_handler = ral_rendering_handler_create_with_fps_policy(window_ptr->backend_type,
                                                                                     system_hashed_ansi_string_create(rendering_handler_name),
                                                                                     window_ptr->target_frame_rate,
                                                                                     nullptr,  /* pfn_rendering_callback */
                                                                                     nullptr); /* user_arg               */
    }

    /* Bind the rendering handler to the renderer window. Note that this also implicitly
     * creates a RAL context for the window. */
    system_window_set_property(window_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_ptr->rendering_handler);

    /* Cache the rendering context */
    system_window_get_property(window_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                              &window_ptr->context);

    ASSERT_DEBUG_SYNC(window_ptr->context != nullptr,
                      "Rendering context is NULL");

    /* Create a flyby instance */
    window_ptr->flyby = demo_flyby_create(window_ptr->context);

    ASSERT_DEBUG_SYNC(window_ptr->flyby != nullptr,
                      "Could not create a flyby instance");

    /* Set up a timeline object instance */
    if (window_ptr->use_timeline)
    {
        window_ptr->timeline = demo_timeline_create(window_ptr->name,
                                                    window_ptr->context,
                                                    window_ptr->window);

        if (window_ptr->timeline == nullptr)
        {
            ASSERT_DEBUG_SYNC(window_ptr->timeline != nullptr,
                              "Could not create a demo_timeline instance");

            goto end;
        }
    }

    /* Set up mouse click & window tear-down callbacks */
    _demo_window_subscribe_for_window_notifications(window_ptr,
                                                    true); /* should_subscribe */

    /* All done */
    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _demo_window_on_audio_stream_finished_playing(const void* callback_data,
                                                                 void* user_arg)
{
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(user_arg);

    system_event_set(window_ptr->shut_down_event);
}

/** "Window has been closed" call-back handler.
 *
 *  NOTE: This call-back is *not* made from the rendering thread.
 */
PRIVATE void _demo_window_window_closed_callback_handler(void* unused,
                                                         void* window)
{
    _demo_window* app_ptr = reinterpret_cast<_demo_window*>(window);

    system_event_set(app_ptr->shut_down_event);
}

/** "Window closing" call-back handler. This entry-point is called from the rendering
 *  thread, right after message pump is deinitialized, but before the context is released.
 *
 *  It is guaranteed no rendering call-back will be made from the moment this place is
 *  entered. Hence, it's the only safe place to release any GL objects you created without
 *  running into a situation, where the released objects are re-used to render a frame.
 */
PRIVATE void _demo_window_window_closing_callback_handler(void* unused,
                                                          void* window)
{
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    if (window_ptr->loader != nullptr)
    {
        demo_loader_release(window_ptr->loader);

        window_ptr->loader = nullptr;
    }

    /* All done */
    system_event_set(window_ptr->shut_down_event);
}

/** TODO */
PRIVATE void _demo_window_subscribe_for_window_notifications(_demo_window* window_ptr,
                                                              bool         should_subscribe)
{
    if (should_subscribe)
    {
        system_window_add_callback_func(window_ptr->window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        reinterpret_cast<void*>(_demo_window_window_closed_callback_handler),
                                        window_ptr);
        system_window_add_callback_func(window_ptr->window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        reinterpret_cast<void*>(_demo_window_window_closing_callback_handler),
                                        window_ptr);
    }
    else
    {
        system_window_delete_callback_func(window_ptr->window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                           reinterpret_cast<void*>(_demo_window_window_closed_callback_handler),
                                           window_ptr);
        system_window_delete_callback_func(window_ptr->window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                           reinterpret_cast<void*>(_demo_window_window_closing_callback_handler),
                                           window_ptr);
    }
}


/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_add_callback_func(demo_window                          window,
                                                      system_window_callback_func_priority priority,
                                                      system_window_callback_func          callback_func,
                                                      void*                                pfn_callback_func,
                                                      void*                                callback_func_user_arg)
{
    bool          result     = false;
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    /* Sanity checks */
    if (window_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(window_ptr != nullptr,
                          "Input demo_window instance is NULL");

        goto end;
    }

    if (window_ptr->window == nullptr)
    {
        ASSERT_DEBUG_SYNC(window_ptr->window != nullptr,
                          "No system_window instance associated with the specified demo_window object.");

        goto end;
    }

    if (pfn_callback_func == nullptr)
    {
        ASSERT_DEBUG_SYNC(pfn_callback_func != nullptr,
                          "Input call-back function pointer is NULL");

        goto end;
    }

    /* Pass the request to the underlying system_window handler ..
     *
     * For "window closing" callbacks, we need to make sure our call-back handler gets called as the last one */
    result = system_window_add_callback_func(window_ptr->window,
                                             (callback_func != SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING) ? priority
                                                                                                           : SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                             callback_func,
                                             pfn_callback_func,
                                             callback_func_user_arg,
                                             true); /* should_use_priority */

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_window_close(demo_window window)
{
    bool          result     = false;
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    if (window == nullptr)
    {
        goto end;
    }

    if (window_ptr->rendering_handler != nullptr)
    {
        ral_rendering_handler_stop(window_ptr->rendering_handler);
    }

    if (window_ptr->rendering_handler != nullptr)
    {
        ral_rendering_handler_release(window_ptr->rendering_handler);

        window_ptr->rendering_handler = nullptr;
    }

    if (window_ptr->window != nullptr)
    {
        system_window_close(window_ptr->window);

        window_ptr->window = nullptr;
    }

    result = true;
end:
    if (window_ptr != nullptr)
    {
        system_event_set(window_ptr->window_closed_event);
    }

    return result;
}

/** Please see header for specification */
PUBLIC demo_window demo_window_create(system_hashed_ansi_string      name,
                                      const demo_window_create_info& create_info,
                                      ral_backend_type               backend_type,
                                      bool                           use_timeline)
{
    _demo_window* window_ptr = new (std::nothrow) _demo_window(name,
                                                               backend_type,
                                                               use_timeline);

    ASSERT_ALWAYS_SYNC(window_ptr != nullptr,
                       "Out of memory");

    if (window_ptr == nullptr)
    {
        goto end;
    }

    window_ptr->should_run_fullscreen               = create_info.fullscreen;
    window_ptr->loader_setup_callback_proc_user_arg = create_info.loader_callback_func_user_arg;
    window_ptr->pfn_loader_setup_callback_proc      = create_info.pfn_loader_callback_func_ptr;
    window_ptr->refresh_rate                        = create_info.refresh_rate;
    window_ptr->resolution[0]                       = create_info.resolution[0];
    window_ptr->resolution[1]                       = create_info.resolution[1];
    window_ptr->target_frame_rate                   = create_info.target_rate;
    window_ptr->use_vsync                           = create_info.use_vsync;
    window_ptr->visible                             = create_info.visible;

    _demo_window_init(window_ptr);

end:
    return (demo_window) window_ptr;
}

/** Please see header for specification */
PUBLIC void demo_window_get_private_property(const demo_window            window,
                                             demo_window_private_property property,
                                             void*                        out_result_ptr)
{
    const _demo_window* window_ptr = reinterpret_cast<const _demo_window*>(window);

    switch (property)
    {
        case DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW:
        {
            *reinterpret_cast<system_window*>(out_result_ptr) = window_ptr->window;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_window_private_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_window_get_property(const demo_window    window,
                                                 demo_window_property property,
                                                 void*                out_result_ptr)
{
    const _demo_window* window_ptr = reinterpret_cast<const _demo_window*>(window);

    switch (property)
    {
        case DEMO_WINDOW_PROPERTY_BACKEND_TYPE:
        {
            *reinterpret_cast<ral_backend_type*>(out_result_ptr) = window_ptr->backend_type;

            break;
        }

        case DEMO_WINDOW_PROPERTY_FLYBY:
        {
            *reinterpret_cast<demo_flyby*>(out_result_ptr) = window_ptr->flyby;

            break;
        }

        case DEMO_WINDOW_PROPERTY_FULLSCREEN:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = window_ptr->should_run_fullscreen;

            break;
        }

        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_FUNC_PTR:
        {
            *reinterpret_cast<PFNDEMOWINDOWLOADERSETUPCALLBACKPROC*>(out_result_ptr) = window_ptr->pfn_loader_setup_callback_proc;

            break;
        }
        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_USER_ARG:
        {
            *reinterpret_cast<void**>(out_result_ptr) = window_ptr->loader_setup_callback_proc_user_arg;

            break;
        }

        case DEMO_WINDOW_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = window_ptr->name;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = window_ptr->context;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RENDERING_HANDLER:
        {
            *reinterpret_cast<ral_rendering_handler*>(out_result_ptr) = window_ptr->rendering_handler;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RESOLUTION:
        {
            memcpy(out_result_ptr,
                   window_ptr->resolution,
                   sizeof(window_ptr->resolution) );

            break;
        }

        case DEMO_WINDOW_PROPERTY_REFRESH_RATE:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = window_ptr->refresh_rate;

            break;
        }

        case DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = window_ptr->target_frame_rate;

            break;
        }

        case DEMO_WINDOW_PROPERTY_TIMELINE:
        {
            *reinterpret_cast<demo_timeline*>(out_result_ptr) = window_ptr->timeline;

            break;
        }

        case DEMO_WINDOW_PROPERTY_VISIBLE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = window_ptr->visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_app_property value used for a demo_app_get_property() call.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_stop_rendering(demo_window window)
{
    ral_rendering_handler_playback_status playback_status = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
    bool                                  result          = false;
    _demo_window*                         window_ptr      = reinterpret_cast<_demo_window*>(window);

    /* Sanity checks */
    if (window == nullptr)
    {
        ASSERT_DEBUG_SYNC(window != nullptr,
                          "Input window instance is NULL");

        goto end;
    }

    if (window_ptr->window == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified window is not shown.");

        goto end;
    }

    ral_rendering_handler_get_property(window_ptr->rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,
                                      &playback_status);

    if (playback_status != RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
    {
        LOG_ERROR("No rendering active for the specified window.");

        goto end;
    }

    /* Pause the rendering */
    result = ral_rendering_handler_stop(window_ptr->rendering_handler);

end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_window_release(demo_window window)
{
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    ASSERT_DEBUG_SYNC(window != nullptr,
                      "Input demo_window instance is NULL");

    if (window_ptr->loader != nullptr)
    {
        demo_loader_release(window_ptr->loader);

        window_ptr->loader = nullptr;
    }

    if (window_ptr->timeline != nullptr)
    {
        demo_timeline_release(window_ptr->timeline);

        window_ptr->timeline = nullptr;
    }

    if (window_ptr->rendering_handler != nullptr)
    {
        ral_rendering_handler_release(window_ptr->rendering_handler);

        window_ptr->rendering_handler = nullptr;
    }

    /* Close the window */
    demo_window_close(window);

    /* Raise the "please die" event and wait till the window thread quits */
    system_event_set        (window_ptr->shut_down_event);
    system_event_wait_single(window_ptr->window_closed_event);

    /* Should be safe to delete the object at this point */
    delete reinterpret_cast<_demo_window*>(window);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_start_rendering(demo_window window,
                                                    system_time rendering_start_time)
{
    bool          result     = false;
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    /* Sanity cvhecks */
    if (window == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input demo_window instance is NULL");

        goto end;
    }

    if (window_ptr->window == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified window first needs to be shown, before rendering can commence");

        goto end;
    }

    /* If the user submitted a call-back to configure the loader, spawn one,
     * call the user's entry-point to set it up, and then load the enqueued
     * stuff */
    if (window_ptr->pfn_loader_setup_callback_proc != nullptr)
    {
        window_ptr->loader = demo_loader_create(window_ptr->context);

        if (window_ptr->loader == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "demo_loader_create() returned NULL");

            goto end;
        }

        /* Let the loader do its job .. */
        demo_loader_run(window_ptr->loader,
                        window_ptr->timeline,
                        (demo_window) window_ptr);

        /* Any audio streams loaded? Bind one to the window */
        uint32_t n_loaded_audio_streams = 0;

        demo_loader_get_property(window_ptr->loader,
                                 DEMO_LOADER_PROPERTY_N_LOADED_AUDIO_STREAMS,
                                &n_loaded_audio_streams);

        if (n_loaded_audio_streams > 1)
        {
            ASSERT_DEBUG_SYNC(n_loaded_audio_streams <= 1,
                              "Only up to one audio stream is supported by demo_app");
        }
        else
        if (n_loaded_audio_streams == 1)
        {
            audio_stream            loaded_audio_stream    = nullptr;
            system_callback_manager loaded_audio_stream_cm = nullptr;

            demo_loader_get_object_by_index(window_ptr->loader,
                                            DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM,
                                            0, /* index */
                                           &loaded_audio_stream);

            system_window_set_property(window_ptr->window,
                                       SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                      &loaded_audio_stream);

            /* Sign up for 'finished playing' call-back so we know when to kill the window */
            audio_stream_get_property(loaded_audio_stream,
                                      AUDIO_STREAM_PROPERTY_CALLBACK_MANAGER,
                                     &loaded_audio_stream_cm);

            system_callback_manager_subscribe_for_callbacks(loaded_audio_stream_cm,
                                                            AUDIO_STREAM_CALLBACK_ID_FINISHED_PLAYING,
                                                            CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                            _demo_window_on_audio_stream_finished_playing,
                                                            window_ptr); /* callback_user_arg */

            demo_loader_release_object_by_index(window_ptr->loader,
                                                DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM,
                                                0); /* index */
        }
    }

    if (window_ptr->use_timeline)
    {
        ral_rendering_handler_set_property(window_ptr->rendering_handler,
                                           RAL_RENDERING_HANDLER_PROPERTY_TIMELINE,
                                          &window_ptr->timeline);
    }

    /* Launch the demo playback. If the call should be non-blocking, we need to move to a new thread. */
    window_ptr->rendering_start_time = rendering_start_time;

    ral_rendering_handler_play(window_ptr->rendering_handler,
                               window_ptr->rendering_start_time);

    result = true;

end:
    if (!result)
    {
        /* Clean up */
        demo_window_close( (demo_window) window_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_wait_until_closed(demo_window window)
{
    bool          result     = false;
    _demo_window* window_ptr = reinterpret_cast<_demo_window*>(window);

    /* Sanity checks */
    if (window == nullptr)
    {
        ASSERT_DEBUG_SYNC(window != nullptr,
                          "Input window is NULL");

        goto end;
    }

    /* Block until the relevant event is signaled */
    system_event_wait_single(window_ptr->shut_down_event);

end:
    return result;
}
