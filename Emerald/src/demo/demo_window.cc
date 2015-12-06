/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "audio/audio_stream.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "demo/demo_window.h"
#include "ogl/ogl_rendering_handler.h"
#include "ral/ral_context.h"
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

    PFNDEMOWINDOWLOADERSETUPCALLBACKPROC pfn_loader_setup_callback_proc;
    void*                                loader_setup_callback_proc_user_arg;

    demo_loader   loader;
    demo_timeline timeline;

    uint32_t              refresh_rate;
    ogl_rendering_handler rendering_handler;
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
        context                             = NULL;
        loader                              = NULL;
        loader_setup_callback_proc_user_arg = NULL;
        name                                = in_name;
        pfn_loader_setup_callback_proc      = NULL;
        refresh_rate                        = 60;
        rendering_handler                   = NULL;
        rendering_start_time                = -1;
        resolution[0]                       = 1280;
        resolution[1]                       = 720;
        should_run_fullscreen               = false;
        shut_down_event                     = system_event_create(true); /* manual_reset */
        target_frame_rate                   = 60;
        timeline                            = NULL;
        use_timeline                        = in_use_timeline;
        use_vsync                           = true;
        visible                             = true;
        window                              = NULL;
        window_closed_event                 = system_event_create(true); /* manual_reset */
    }

    ~_demo_window()
    {
        ASSERT_DEBUG_SYNC(window == NULL,
                          "Rendering window is not NULL at demo_window teardown time");

        if (shut_down_event != NULL)
        {
            system_event_release(shut_down_event);

            shut_down_event = NULL;
        } /* if (shut_down_event != NULL) */

        if (window_closed_event != NULL)
        {
            system_event_release(window_closed_event);

            window_closed_event = NULL;
        }
    }
} _demo_window;


/** TODO */
PRIVATE void _demo_window_on_audio_stream_finished_playing(const void* callback_data,
                                                                 void* user_arg)
{
    _demo_window* window_ptr = (_demo_window*) user_arg;

    system_event_set(window_ptr->shut_down_event);
}

/** "Window has been closed" call-back handler.
 *
 *  NOTE: This call-back is *not* made from the rendering thread.
 */
PRIVATE void _demo_window_window_closed_callback_handler(void* unused,
                                                         void* window)
{
    _demo_window* app_ptr = (_demo_window*) window;

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
    _demo_window* window_ptr = (_demo_window*) window;

    if (window_ptr->loader != NULL)
    {
        demo_loader_release(window_ptr->loader);

        window_ptr->loader = NULL;
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
                                        (void*) _demo_window_window_closed_callback_handler,
                                        window_ptr);
        system_window_add_callback_func(window_ptr->window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _demo_window_window_closing_callback_handler,
                                        window_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_window_delete_callback_func(window_ptr->window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                           (void*) _demo_window_window_closed_callback_handler,
                                           window_ptr);
        system_window_delete_callback_func(window_ptr->window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                           (void*) _demo_window_window_closing_callback_handler,
                                           window_ptr);
    }
}


/** TODO */
PUBLIC bool demo_window_close(demo_window window)
{
    bool          result     = false;
    _demo_window* window_ptr = (_demo_window*) window;

    if (window == NULL)
    {
        goto end;
    }

    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_stop(window_ptr->rendering_handler);
    }

    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(window_ptr->rendering_handler);

        window_ptr->rendering_handler = NULL;
    }

    if (window_ptr->window != NULL)
    {
        system_window_close(window_ptr->window);

        window_ptr->window = NULL;
    }

    result = true;
end:
    system_event_set(window_ptr->window_closed_event);

    return result;
}

/** Please see header for specification */
PUBLIC demo_window demo_window_create(system_hashed_ansi_string name,
                                      ral_backend_type          backend_type,
                                      bool                      use_timeline)
{
    _demo_window* window_ptr = new (std::nothrow) _demo_window(name,
                                                               backend_type,
                                                               use_timeline);

    ASSERT_ALWAYS_SYNC(window_ptr != NULL,
                       "Out of memory");

    return (demo_window) window_ptr;
}

/** Please see header for specification */
PUBLIC void demo_window_get_private_property(const demo_window            window,
                                             demo_window_private_property property,
                                             void*                        out_result_ptr)
{
    const _demo_window* window_ptr = (const _demo_window*) window;

    switch (property)
    {
        case DEMO_WINDOW_PRIVATE_PROPERTY_WINDOW:
        {
            *(system_window*) out_result_ptr = window_ptr->window;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_window_private_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_window_get_property(const demo_window    window,
                                                 demo_window_property property,
                                                 void*                out_result_ptr)
{
    const _demo_window* window_ptr = (const _demo_window*) window;

    switch (property)
    {
        case DEMO_WINDOW_PROPERTY_BACKEND_TYPE:
        {
            *(ral_backend_type*) out_result_ptr = window_ptr->backend_type;

            break;
        }

        case DEMO_WINDOW_PROPERTY_FULLSCREEN:
        {
            *(bool*) out_result_ptr = window_ptr->should_run_fullscreen;

            break;
        }

        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_FUNC_PTR:
        {
            *(PFNDEMOWINDOWLOADERSETUPCALLBACKPROC*) out_result_ptr = window_ptr->pfn_loader_setup_callback_proc;

            break;
        }
        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_USER_ARG:
        {
            *(void**) out_result_ptr = window_ptr->loader_setup_callback_proc_user_arg;

            break;
        }

        case DEMO_WINDOW_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = window_ptr->name;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT:
        {
            *(ral_context*) out_result_ptr = window_ptr->context;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RENDERING_HANDLER:
        {
            *(ogl_rendering_handler*) out_result_ptr = window_ptr->rendering_handler;

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
            *(uint32_t*) out_result_ptr = window_ptr->refresh_rate;

            break;
        }

        case DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE:
        {
            *(uint32_t*) out_result_ptr = window_ptr->target_frame_rate;

            break;
        }

        case DEMO_WINDOW_PROPERTY_TIMELINE:
        {
            *(demo_timeline*) out_result_ptr = window_ptr->timeline;

            break;
        }

        case DEMO_WINDOW_PROPERTY_VISIBLE:
        {
            *(bool*) out_result_ptr = window_ptr->visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_app_property value used for a demo_app_get_property() call.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_stop_rendering(demo_window window)
{
    ogl_rendering_handler_playback_status playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
    bool                                  result          = false;
    _demo_window*                         window_ptr      = (_demo_window*) window;

    /* Sanity checks */
    if (window == NULL)
    {
        ASSERT_DEBUG_SYNC(window != NULL,
                          "Input window instance is NULL");

        goto end;
    }

    if (window_ptr->window == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified window is not shown.");

        goto end;
    }

    ogl_rendering_handler_get_property(window_ptr->rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,
                                      &playback_status);

    if (playback_status != RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No rendering active for the specified window.");

        goto end;
    }

    /* Pause the rendering */
    result = ogl_rendering_handler_stop(window_ptr->rendering_handler);

end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_window_release(demo_window window)
{
    _demo_window* window_ptr = (_demo_window*) window;

    ASSERT_DEBUG_SYNC(window != NULL,
                      "Input demo_window instance is NULL");

    if (window_ptr->loader != NULL)
    {
        demo_loader_release(window_ptr->loader);

        window_ptr->loader = NULL;
    } /* if (window_ptr->loader != NULL) */

    if (window_ptr->timeline != NULL)
    {
        demo_timeline_release(window_ptr->timeline);

        window_ptr->timeline = NULL;
    } /* if (window_ptr->timeline != NULL) */

    if (window_ptr->rendering_handler != NULL)
    {
        ogl_rendering_handler_release(window_ptr->rendering_handler);

        window_ptr->rendering_handler = NULL;
    } /* if (window_ptr->rendering_handler != NULL) */

    /* Close the window */
    demo_window_close(window);

    /* Raise the "please die" event and wait till the window thread quits */
    system_event_set        (window_ptr->shut_down_event);
    system_event_wait_single(window_ptr->window_closed_event);

    /* Should be safe to delete the object at this point */
    delete (_demo_window*) window;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_show(demo_window window)
{
    system_pixel_format pixel_format = NULL; /* ownership will be taken by the window */
    char                rendering_handler_name[256];
    bool                result       = false;
    _demo_window*       window_ptr   = (_demo_window*) window;

    /* Sanity checks */
    if (window == NULL)
    {
        ASSERT_DEBUG_SYNC(window != NULL,
                          "Input demo_window instance is NULL");

        goto end;
    }

    if (window_ptr->window != NULL)
    {
        ASSERT_DEBUG_SYNC(window_ptr->window == NULL,
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
        system_screen_mode screen_mode = NULL;

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

        window_ptr->window = system_window_create_fullscreen(window_ptr->backend_type,
                                                             screen_mode,
                                                             window_ptr->use_vsync,
                                                             pixel_format);
    }
    else
    {
        int window_x1y1x2y2[4] = {0};

        /* Determine centered position for a renderer window of the specified size. */
        system_window_get_centered_window_position_for_primary_monitor( (const int*) window_ptr->resolution,
                                                                        window_x1y1x2y2);

        /* Spawn the window. */
        window_ptr->window = system_window_create_not_fullscreen(window_ptr->backend_type,
                                                                 window_x1y1x2y2,
                                                                 window_ptr->name,
                                                                 false, /* scalable */
                                                                 window_ptr->use_vsync,
                                                                 window_ptr->visible,
                                                                 pixel_format);
    }

    if (window_ptr->window == NULL)
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
    ASSERT_DEBUG_SYNC(window_ptr->rendering_handler == NULL,
                      "An ogl_rendering_handler instance is already present.");

    snprintf(rendering_handler_name,
             sizeof(rendering_handler_name),
             "Rendering handler for window [%s]",
             system_hashed_ansi_string_get_buffer(window_ptr->name) );


    if (window_ptr->target_frame_rate == 0)
    {
        window_ptr->rendering_handler = ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string_create(rendering_handler_name),
                                                                                                    NULL,  /* pfn_rendering_callback */
                                                                                                    NULL); /* user_arg               */
    }
    else
    if (window_ptr->target_frame_rate == ~0)
    {
        window_ptr->rendering_handler = ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string_create(rendering_handler_name),
                                                                                                 NULL,  /* pfn_rendering_callback */
                                                                                                 NULL); /* user_arg               */
    }
    else
    {
        window_ptr->rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create(rendering_handler_name),
                                                                                     window_ptr->target_frame_rate,
                                                                                     NULL,  /* pfn_rendering_callback */
                                                                                     NULL); /* user_arg               */
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

    ASSERT_DEBUG_SYNC(window_ptr->context != NULL,
                      "Rendering context is NULL");

    /* Set up a timeline object instance */
    if (window_ptr->use_timeline)
    {
        window_ptr->timeline = demo_timeline_create(window_ptr->name,
                                                    window_ptr->context,
                                                    window_ptr->window);

        if (window_ptr->timeline == NULL)
        {
            ASSERT_DEBUG_SYNC(window_ptr->timeline != NULL,
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

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_start_rendering(demo_window window,
                                                    system_time rendering_start_time)
{
    bool          result     = false;
    _demo_window* window_ptr = (_demo_window*) window;

    /* Sanity cvhecks */
    if (window == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input demo_window instance is NULL");

        goto end;
    }

    if (window_ptr->window == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified window first needs to be shown, before rendering can commence");

        goto end;
    }

    /* If the user submitted a call-back to configure the loader, spawn one,
     * call the user's entry-point to set it up, and then load the enqueued
     * stuff */
    if (window_ptr->pfn_loader_setup_callback_proc != NULL)
    {
        window_ptr->loader = demo_loader_create(window_ptr->context);

        if (window_ptr->loader == NULL)
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
            audio_stream            loaded_audio_stream    = NULL;
            system_callback_manager loaded_audio_stream_cm = NULL;

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
        } /* if (n_loaded_audio_streams == 1) */
    } /* if (window_ptr->pfn_loader_setup_callback_proc != NULL) */

    if (window_ptr->use_timeline)
    {
        ogl_rendering_handler_set_property(window_ptr->rendering_handler,
                                           OGL_RENDERING_HANDLER_PROPERTY_TIMELINE,
                                          &window_ptr->timeline);
    }

    /* Launch the demo playback. If the call should be non-blocking, we need to move to a new thread. */
    window_ptr->rendering_start_time = rendering_start_time;

    ogl_rendering_handler_play(window_ptr->rendering_handler,
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
PUBLIC EMERALD_API void demo_window_set_property(demo_window          window,
                                                 demo_window_property property,
                                                 const void*          data_ptr)
{
    _demo_window* window_ptr = (_demo_window*) window;

    /* Sanity checks */
    if (window_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input demo_window instance is NULL");

        goto end;
    } /* if (window_ptr == NULL) */

    switch (property)
    {
        case DEMO_WINDOW_PROPERTY_FULLSCREEN:
        {
            window_ptr->should_run_fullscreen = *(bool*) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_FUNC_PTR:
        {
            window_ptr->pfn_loader_setup_callback_proc = *(PFNDEMOWINDOWLOADERSETUPCALLBACKPROC*) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_LOADER_CALLBACK_USER_ARG:
        {
            window_ptr->loader_setup_callback_proc_user_arg = *(void**) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_REFRESH_RATE:
        {
            window_ptr->refresh_rate = *(uint32_t*) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_RESOLUTION:
        {
            memcpy(window_ptr->resolution,
                   data_ptr,
                   sizeof(window_ptr->resolution) );

            break;
        }

        case DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE:
        {
            window_ptr->target_frame_rate = *(uint32_t*) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_USE_VSYNC:
        {
            window_ptr->use_vsync = *(bool*) data_ptr;

            break;
        }

        case DEMO_WINDOW_PROPERTY_VISIBLE:
        {
            window_ptr->visible = *(bool*) data_ptr;

            if (window_ptr->window != NULL)
            {
                system_window_set_property(window_ptr->window,
                                           SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,
                                           data_ptr);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid demo_app_property value passed to demo_window_set_property()");
        }
    } /* switch (property) */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_window_wait_until_closed(demo_window window)
{
    bool          result     = false;
    _demo_window* window_ptr = (_demo_window*) window;

    /* Sanity checks */
    if (window == NULL)
    {
        ASSERT_DEBUG_SYNC(window != NULL,
                          "Input window is NULL");

        goto end;
    }

    /* Block until the relevant event is signaled */
    system_event_wait_single(window_ptr->shut_down_event);

end:
    return result;
}
