/** Trivia:
 *
 *  1. The app can be closed the usual way or by clicking RBM. Feel free to change
 *     the behavior.
 *  2. You can move across the timeline by tapping/holding left/right cursor. The rendering process
 *     can be completely paused (& later resumed) by pressing space. By default, this behavior is only
 *     available in debug builds, but you can toggle that behavior by modifying demo_timeline code.
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"
#include "main.h"

#include "include/stage_intro.h"
#include "include/stage_part1.h"
#include "include/stage_part2.h"
#include "include/stage_part3.h"
#include "include/stage_part4.h"
#include "include/stage_outro.h"


PRIVATE ogl_context   _context           = NULL;
PRIVATE ogl_pipeline  _pipeline          = NULL;
PRIVATE uint32_t      _pipeline_stage_id = 0;
PRIVATE demo_timeline _timeline          = NULL;

/* This event will be set either if the user pressed the RBM, or the "window closing"
 * call-back occurs. */
PRIVATE system_event _please_die_event = system_event_create(true /* manual_reset */);

/* Aspect ratio which should be used by all demo parts */
PRIVATE const float _demo_aspect_ratio = 1280.0f / 720.0f;

/* Let's use a predefined window size during development. Final build will always
 * use a full-screen 1080p resolution. */
PRIVATE const int   _window_size[2] = {1280, 720};


/* Forward declarations */
PRIVATE void _deinit_demo_stages_rendering_callback(ogl_context context,
                                                    void*       unused);
PRIVATE void _init_demo_stages_rendering_callback  (ogl_context context,
                                                    void*       unused);
PRIVATE bool _rendering_rbm_callback_handler       (system_window           window,
                                                    unsigned short          x,
                                                    unsigned short          y,
                                                    system_window_vk_status new_status,
                                                    void*);
PRIVATE void _window_closed_callback_handler       (system_window           window);
PRIVATE void _window_closing_callback_handler      (system_window           window);


/** Calls deinit() for all demo stages we use in the demo.
 *
 *  @param context Rendering context handle.
 *  @param unused  Always NULL.
 **/
PRIVATE void _deinit_demo_stages_rendering_callback(ogl_context context,
                                                    void*       unused)
{
    stage_intro_deinit(context);
    stage_part1_deinit(context);
    stage_part2_deinit(context);
    stage_part3_deinit(context);
    stage_part4_deinit(context);
    stage_outro_deinit(context);
}

/** Calls init() for all demo stages we use in the demo.
 *
 *  @param context Rendering context handle.
 *  @param unused  Always NULL.
 **/
PRIVATE void _init_demo_stages_rendering_callback(ogl_context context,
                                                  void*       unused)
{
    stage_intro_init(context);
    stage_part1_init(context);
    stage_part2_init(context);
    stage_part3_init(context);
    stage_part4_init(context);
    stage_outro_init(context);
}

/* Sets up contents of the timeline */
PRIVATE void _init_timeline_contents()
{
    /* Now, create video segments for each logical part of the demo.
     *
     * For each video segment, we need to define a set of passes, which will be used to render
     * the frame contents. The passes will be executed one after another, in a serialized manner,
     * with the assumption the contents rendered by the last pass should be considered final frame
     * contents.
     *
     * For simplicity, the template defines only one pass for each part. Feel free to modify this.
     */
    demo_timeline_segment_pass intro_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Intro video segment (main pass)"),
            stage_intro_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_pass outro_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Outro video segment (main pass)"),
            stage_outro_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_pass part_1_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Part 1 (main pass)"),
            stage_part1_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_pass part_2_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Part 2 (main pass)"),
            stage_part2_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_pass part_3_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Part 3 (main pass)"),
            stage_part3_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_pass part_4_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Part 4 (main pass)"),
            stage_part4_render,
            NULL /* user_arg */
        }
    };
    demo_timeline_segment_id segment_id;

    _timeline = demo_timeline_create(system_hashed_ansi_string_create("Demo timeline"),
                                     _context);

    /* TODO: Tweak start/end times! */
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Intro video segment"),
                                    system_time_get_time_for_s   (0),    /* start_time */
                                    system_time_get_time_for_msec(5650), /* end_time */
                                    1280.0f / 720.0f,                    /* aspect_ratio */
                                    1,                                   /* n_passes */
                                    intro_segment_passes,
                                   &segment_id);
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Part 1 video segment"),
                                    system_time_get_time_for_msec(5650), /* start_time */
                                    system_time_get_time_for_msec(25850),/* end_time */
                                    1280.0f / 720.0f, /* aspect_ratio */
                                    1, /* n_passes */
                                    part_1_segment_passes,
                                   &segment_id);
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Part 2 video segment"),
                                    system_time_get_time_for_msec(25850), /* start_time */
                                    system_time_get_time_for_msec(51300), /* end_time */
                                    1280.0f / 720.0f, /* aspect_ratio */
                                    1, /* n_passes */
                                    part_2_segment_passes,
                                   &segment_id);
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Part 3 video segment"),
                                    system_time_get_time_for_msec(51300), /* start_time */
                                    system_time_get_time_for_msec(71400), /* end_time */
                                    1280.0f / 720.0f, /* aspect_ratio */
                                    1, /* n_passes */
                                    part_3_segment_passes,
                                   &segment_id);
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Part 4 video segment"),
                                    system_time_get_time_for_msec(71400), /* start_time */
                                    system_time_get_time_for_msec(89000), /* end_time */
                                    1280.0f / 720.0f, /* aspect_ratio */
                                    1, /* n_passes */
                                    part_4_segment_passes,
                                   &segment_id);
    demo_timeline_add_video_segment(_timeline,
                                    system_hashed_ansi_string_create("Outro video segment"),
                                    system_time_get_time_for_msec(89000), /* start_time */
                                    system_time_get_time_for_msec(98000), /* end_time */
                                    1280.0f / 720.0f, /* aspect_ratio */
                                    1, /* n_passes */
                                    outro_segment_passes,
                                   &segment_id);
}

/** Right mouse button click call-back handler.
 *
 *  NOTE: This call-back is *not* made from the rendering thread.
 */
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    /* Set the event the main thread is blocked on, so the tear-down can proceed */
    system_event_set(_please_die_event);

    return true;
}

/** "Window has been closed" call-back handler.
 *
 *  NOTE: This call-back is *not* made from the rendering thread.
 */
PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_please_die_event);
}

/** "Window closing" call-back handler. This entry-point is called from the rendering
 *  thread, right after message pump is deinitialized, but before the context is released.
 *
 *  It is guaranteed no rendering call-back will be made from the moment this place is
 *  entered. Hence, it's the only safe place to release any GL objects you created without
 *  running into a situation, where the released objects are re-used to render a frame.
 */
PRIVATE void _window_closing_callback_handler(system_window window)
{
    _deinit_demo_stages_rendering_callback(_context,
                                           NULL);

    /* All done */
    system_event_set(_please_die_event);
}


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

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
     * 2) Compatibility with non-ES/-GL rendering APIs. Happy to elaborate in private comms about this :)
     */
    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

    /* To check full-screen mode performance, flip the value below */
#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    /* Determine centered position for a renderer window of the specified size. */
    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    /* Spawn the window. It needs not be scalable, but we do want it to be vsync'ed and, well, visible. :) */
    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 true,  /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

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
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,    /* fps */
                                                                            NULL,  /* pfn_rendering_callback - will be overwritten by demo_timeline object later on */
                                                                            NULL); /* user_arg */

    /* Bind the rendering handler to the renderer window. This needs to be only done once. */
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Request a rendering context call-back for the first time, so all demo stages can initialize themselves */
    ogl_context_request_callback_from_context_thread(_context,
                                                     _init_demo_stages_rendering_callback,
                                                     NULL); /* user_arg */

    /* Add video segments for each demo part to the timeline.*/
    _init_timeline_contents();

    /* Finally, bind the timeline to the rendering handler. This will cause the rendering process
     * to work, as defined by the demo_timeline instance. */
    ogl_rendering_handler_set_property(window_rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_TIMELINE,
                                      &_timeline);

    /* Load the audio stream and associate it with the window, so that the soundtrack plays whenever
     * we render stuff.
     *
     * TODO: Yeah, it would make more sense to link the audio stream with demo_timeline object. :)
     */
    {
        system_file_serializer audio_serializer  = NULL;
        audio_stream           audio_stream      = NULL;

        audio_serializer = system_file_serializer_create_for_reading(system_hashed_ansi_string_create("demo.mp3") );
        ASSERT_ALWAYS_SYNC(audio_serializer != NULL,
                           "Could not open demo.mp3. Is it in the working directory?");

        audio_stream = audio_stream_create(audio_device_get_default_device(),
                                            audio_serializer,
                                            window);
        ASSERT_ALWAYS_SYNC(audio_stream != NULL,
                           "Could not create an audio_stream instance. Huh.");

        system_window_set_property(window,
                                   SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                  &audio_stream);

        audio_stream_release          (audio_stream);
        system_file_serializer_release(audio_serializer);
    }

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Launch the rendering process and wait until the window closes. */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_please_die_event);

    /* Clean up - DO NOT release any GL objects here, the rendering context is dead by this time! */
    system_window_close (window);
    system_event_release(_please_die_event);

    /* Force Emerald's internal resources deinitialization. This call is optional. */
    main_force_deinit();

    return 0;
}