/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "audio/audio_stream.h"
#include "demo/demo_app.h"
#include "demo/demo_loader.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_callback_manager.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "main.h"


typedef struct _demo_app
{
    ogl_context               context;
    ogl_context_type          context_type;
    demo_loader               loader;
    system_hashed_ansi_string name;
    uint32_t                  refresh_rate;
    ogl_rendering_handler     rendering_handler;
    uint32_t                  resolution[2];
    bool                      should_run_fullscreen;
    system_event              shut_down_event;
    demo_timeline             timeline;
    bool                      use_vsync;
    system_window             window;

    explicit _demo_app(system_hashed_ansi_string in_name)
    {
        context_type          = OGL_CONTEXT_TYPE_GL;
        loader                = demo_loader_create( (demo_app) this);
        name                  = in_name;
        refresh_rate          = 60;
        rendering_handler     = NULL;
        resolution[0]         = 1280;
        resolution[1]         = 720;
        should_run_fullscreen = false;
        shut_down_event       = system_event_create(true); /* manual_reset */
        timeline              = NULL;
        use_vsync             = true;
        window                = NULL;
    }

    ~_demo_app()
    {
        ASSERT_DEBUG_SYNC(window == NULL,
                          "Rendering window is not NULL at demo_app teardown time");

        if (context != NULL)
        {
            ogl_context_release(context);

            context = NULL;
        } /* if (context != NULL) */

        if (loader != NULL)
        {
            demo_loader_release(loader);

            loader = NULL;
        } /* if (loader != NULL) */

        if (rendering_handler != NULL)
        {
            ogl_rendering_handler_release(rendering_handler);

            rendering_handler = NULL;
        } /* if (rendering_handler != NULL) */

        if (shut_down_event != NULL)
        {
            system_event_release(shut_down_event);

            shut_down_event = NULL;
        } /* if (shut_down_event != NULL) */

        if (timeline != NULL)
        {
            demo_timeline_release(timeline);

            timeline = NULL;
        } /* if (timeline != NULL) */
    }
} _demo_app;


/** TODO */
PRIVATE void _demo_app_on_audio_stream_finished_playing(const void* callback_data,
                                                                 void* user_arg)
{
    _demo_app* app_ptr = (_demo_app*) user_arg;

    system_event_set(app_ptr->shut_down_event);
}

/** "Window has been closed" call-back handler.
 *
 *  NOTE: This call-back is *not* made from the rendering thread.
 */
PRIVATE void _demo_app_window_closed_callback_handler(void* unused,
                                                      void* app)
{
    _demo_app* app_ptr = (_demo_app*) app;

    system_event_set(app_ptr->shut_down_event);
}

/** "Window closing" call-back handler. This entry-point is called from the rendering
 *  thread, right after message pump is deinitialized, but before the context is released.
 *
 *  It is guaranteed no rendering call-back will be made from the moment this place is
 *  entered. Hence, it's the only safe place to release any GL objects you created without
 *  running into a situation, where the released objects are re-used to render a frame.
 */
PRIVATE void _demo_app_window_closing_callback_handler(void* unused,
                                                       void* app)
{
    _demo_app* app_ptr = (_demo_app*) app;

    demo_loader_release(app_ptr->loader);
    app_ptr->loader = NULL;

    /* All done */
    system_event_set(app_ptr->shut_down_event);
}


/** Please see header for specification */
PUBLIC EMERALD_API demo_app demo_app_create(system_hashed_ansi_string name)
{
    _demo_app* new_app_ptr = new _demo_app(name);

    ASSERT_DEBUG_SYNC(new_app_ptr != NULL,
                      "Out of memory");

    return (demo_app) new_app_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_get_property(const demo_app    app,
                                              demo_app_property property,
                                              void*             out_result_ptr)
{
    const _demo_app* app_ptr = (const _demo_app*) app;

    switch (property)
    {
        case DEMO_APP_PROPERTY_CONTEXT_TYPE:
        {
            *(ogl_context_type*) out_result_ptr = app_ptr->context_type;

            break;
        }

        case DEMO_APP_PROPERTY_FULLSCREEN:
        {
            *(bool*) out_result_ptr = app_ptr->should_run_fullscreen;

            break;
        }

        case DEMO_APP_PROPERTY_LOADER:
        {
            *(demo_loader*) out_result_ptr = app_ptr->loader;

            break;
        }

        case DEMO_APP_PROPERTY_RESOLUTION:
        {
            memcpy(out_result_ptr,
                   app_ptr->resolution,
                   sizeof(app_ptr->resolution) );

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
PUBLIC EMERALD_API void demo_app_release(demo_app app)
{
    /* Force Emerald's internal resources deinitialization. This call is optional. */
    main_force_deinit();

    /* Release all stuff */
    delete (_demo_app*) app;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_run(demo_app app)
{
    _demo_app*          app_ptr            = (_demo_app*) app;
    system_pixel_format window_pf          = NULL;
    int                 window_x1y1x2y2[4] = {0};

    /* Fill out a pixel format descriptor.
     *
     * Emerald maintains its own "default framebuffer", to which rendering handlers should render.
     * Its attachments will be initialized, according to the descriptor we're just about to configure.
     *
     * Two reasons for this:
     *
     * 1) Cross-compatibility. MSAA set-up is significantly different under Linux
     *    and Windows, and it's simpler to reconcile both platforms this way.
     *
     * 2) Compatibility with non-ES/-GL rendering APIs.
     */
    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           1,
                                           0); /* stencil_buffer_bits     */

    if (app_ptr->should_run_fullscreen)
    {
        system_screen_mode screen_mode = NULL;

        if (!system_screen_mode_get_for_resolution(app_ptr->resolution[0],
                                                   app_ptr->resolution[1],
                                                   app_ptr->refresh_rate,
                                                  &screen_mode) )
        {
            LOG_FATAL("No suitable screen mode found for resolution [%d x %d x %dHz]",
                      app_ptr->resolution[0],
                      app_ptr->resolution[1],
                      app_ptr->refresh_rate);
        }

        app_ptr->window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                          screen_mode,
                                                          true, /* vsync_enabled */
                                                          window_pf);
    }
    else
    {
        /* Determine centered position for a renderer window of the specified size. */
        system_window_get_centered_window_position_for_primary_monitor( (const int*) app_ptr->resolution,
                                                                        window_x1y1x2y2);

        /* Spawn the window. It needs not be scalable, but we do want it to be vsync'ed and, well, visible. :) */
        app_ptr->window = system_window_create_not_fullscreen(app_ptr->context_type,
                                                              window_x1y1x2y2,
                                                              app_ptr->name,
                                                              false, /* scalable */
                                                              app_ptr->use_vsync,
                                                              true,  /* visible */
                                                              window_pf);
    }

    ASSERT_ALWAYS_SYNC(app_ptr->window != NULL,
                       "Could not create rendering window");

    /* Set up the rendering context.
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
    app_ptr->rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                              app_ptr->refresh_rate,
                                                                              NULL,
                                                                              NULL); /* user_arg */

    /* Bind the rendering handler to the renderer window. This needs to be only done once. */
    system_window_get_property(app_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &app_ptr->context);
    system_window_set_property(app_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &app_ptr->rendering_handler);

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (app_ptr->window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _demo_app_window_closed_callback_handler,
                                        app_ptr);
    system_window_add_callback_func    (app_ptr->window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _demo_app_window_closing_callback_handler,
                                        app_ptr);

    /* Set up a timeline object instance */
    app_ptr->timeline = demo_timeline_create(app_ptr->name,
                                             app_ptr->context);

    ASSERT_DEBUG_SYNC(app_ptr->timeline != NULL,
                      "Could not create a demo_timeline instance");

    /* Let the loader do its job .. */
    demo_loader_run(app_ptr->loader,
                    app_ptr->timeline,
                    app_ptr->window);

    /* Any audio streams loaded? Bind one to the window */
    uint32_t n_loaded_audio_streams = 0;

    demo_loader_get_property(app_ptr->loader,
                             DEMO_LOADER_PROPERTY_N_LOADED_AUDIO_STREAMS,
                            &n_loaded_audio_streams);
    ASSERT_DEBUG_SYNC(n_loaded_audio_streams <= 1,
                      "Only up to one audio stream is supported by demo_app");

    if (n_loaded_audio_streams == 1)
    {
        audio_stream            loaded_audio_stream    = NULL;
        system_callback_manager loaded_audio_stream_cm = NULL;

        demo_loader_get_object_by_index(app_ptr->loader,
                                        DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM,
                                        0, /* index */
                                       &loaded_audio_stream);

        system_window_set_property(app_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                  &loaded_audio_stream);

        /* Sign up for 'finished playing' call-back so we know when to kill the window */
        audio_stream_get_property(loaded_audio_stream,
                                  AUDIO_STREAM_PROPERTY_CALLBACK_MANAGER,
                                 &loaded_audio_stream_cm);

        system_callback_manager_subscribe_for_callbacks(loaded_audio_stream_cm,
                                                        AUDIO_STREAM_CALLBACK_ID_FINISHED_PLAYING,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_app_on_audio_stream_finished_playing,
                                                        app_ptr); /* callback_user_arg */

        demo_loader_release_object_by_index(app_ptr->loader,
                                            DEMO_LOADER_OBJECT_TYPE_AUDIO_STREAM,
                                            0); /* index */
    } /* if (n_loaded_audio_streams == 1) */

    /* Launch the demo playback */
    ogl_rendering_handler_set_property(app_ptr->rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_TIMELINE,
                                      &app_ptr->timeline);

    ogl_rendering_handler_play(app_ptr->rendering_handler,
                               0); /* start_time */

    system_event_wait_single(app_ptr->shut_down_event);

    /* Clean up - DO NOT release any GL objects here, the rendering context is dead by this time! */
    system_window_close (app_ptr->window);
    app_ptr->window = NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_app_set_property(demo_app          app,
                                              demo_app_property property,
                                              const void*       data_ptr)
{
    _demo_app* app_ptr = (_demo_app*) app;

    switch (property)
    {
        case DEMO_APP_PROPERTY_CONTEXT_TYPE:
        {
            app_ptr->context_type = *(ogl_context_type*) data_ptr;

            break;
        }

        case DEMO_APP_PROPERTY_FULLSCREEN:
        {
            app_ptr->should_run_fullscreen = *(bool*) data_ptr;

            break;
        }

        case DEMO_APP_PROPERTY_RESOLUTION:
        {
            memcpy(app_ptr->resolution,
                   data_ptr,
                   sizeof(app_ptr->resolution) );

            break;
        }

        case DEMO_APP_PROPERTY_USE_VSYNC:
        {
            app_ptr->use_vsync = *(bool*) data_ptr;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid demo_app_property value passed to demo_app_set_property()");
        }
    } /* switch (property) */
}