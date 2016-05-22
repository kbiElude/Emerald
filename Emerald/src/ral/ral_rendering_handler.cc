/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_timeline.h"
#include "raGL/raGL_rendering_handler.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_window.h"
#include "varia/varia_text_renderer.h"

#define REWIND_DELTA_TIME_MSEC          (1500)
#define REWIND_STEP_MIN_DELTA_TIME_MSEC (500)


/** Internal type definitions */
typedef struct
{
    audio_stream                          active_audio_stream;
    float                                 aspect_ratio;
    ral_context                           context;
    system_event                          context_set_event;
    system_window                         context_window;
    uint32_t                              fps;
    uint32_t                              last_frame_index;      /* only when fps policy is used */
    system_time                           last_frame_time;       /* cached for the runtime time adjustment mode support */
    volatile uint32_t                     n_frames_rendered;     /* NOTE: must be volatile for release builds to work correctly! */
    system_time                           playback_start_time;
    ral_rendering_handler_playback_status playback_status;
    ral_rendering_handler_policy          policy;
    void*                                 rendering_handler_backend; /* eg. raGL_rendering_handler instance for ES/GL contexts */
    system_time                           runtime_time_adjustment;
    bool                                  runtime_time_adjustment_mode;
    varia_text_renderer_text_string_id    text_string_id;
    system_thread_id                      thread_id;
    demo_timeline                         timeline;

    bool        is_space_key_pressed;
    system_time left_arrow_key_press_start_time;
    system_time rewind_delta;
    system_time rewind_step_min_delta;
    system_time right_arrow_key_press_start_time;
    bool        runtime_time_adjustment_is_paused;
    system_time runtime_time_adjustment_paused_frame_time;

    PFNRALRENDERINGHANDLERCREATERABACKENDRENDERINGHANDLERPROC pfn_create_raBackend_rendering_handler_proc;
    PFNRALRENDERINGHANDLERINITRABACKENDRENDERINGHANDLERPROC   pfn_init_raBackend_rendering_handler_proc;
    PFNRALRENDERINGHANDLERPOSTDRAWFRAMECALLBACKPROC           pfn_post_draw_frame_raBackend_proc;
    PFNRALRENDERINGHANDLERPREDRAWFRAMECALLBACKPROC            pfn_pre_draw_frame_raBackend_proc;
    PFNRALRENDERINGHANDLERPRESENTFRAMECALLBACKPROC            pfn_present_frame_raBackend_proc;
    PFNRALRENDERINGHANDLERRELEASEBACKENDRENDERINGHANDLERPROC  pfn_release_raBackend_rendering_handler_proc;
    PFNRALRENDERINGHANDLERREQUESTRENDERINGCALLBACKPROC        pfn_request_rendering_callback_raBackend_proc;

    PFNRALRENDERINGHANDLERRENDERINGCALLBACK                 pfn_rendering_callback;
    void*                                                   rendering_callback_user_arg;

    system_event            playback_in_progress_event; /* if modified, also update playback_stopped event. Only modify when rendering_cs is locked! */
    system_event            playback_stopped_event;     /* if modified, also update playback_in_progress event. Only modify when rendering_cs is locked! */
    system_critical_section rendering_cs;
    system_event            shutdown_request_event;
    system_event            shutdown_request_ack_event;
    system_event            playback_waiting_event; /* event set when rendering thread is about to start waiting on playback
                                                       events, reset when rendering thread is executing */

    uint32_t                                         n_backend_wait_events;
    uint32_t                                         n_default_wait_events;
    volatile bool                                    should_thread_live;
    system_event*                                    wait_events;           /* has n_backend_wait_events + n_default_wait_events items */
    ral_rendering_handler_custom_wait_event_handler* wait_event_handlers;
    REFCOUNT_INSERT_VARIABLES
} _ral_rendering_handler;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_rendering_handler,
                               ral_rendering_handler,
                              _ral_rendering_handler);

/** Internal variables */

/** TODO */
PRIVATE void _ral_rendering_handler_get_frame_properties(_ral_rendering_handler* handler_ptr,
                                                         bool                    should_update_frame_counter,
                                                         int32_t*                out_frame_index_ptr,
                                                         system_time*            out_frame_time_ptr)
{
    system_time curr_time = system_time_now();

    switch (handler_ptr->policy)
    {
        case RAL_RENDERING_HANDLER_POLICY_MAX_PERFORMANCE:
        {
            if (should_update_frame_counter)
            {
                handler_ptr->n_frames_rendered++;
            }

            *out_frame_index_ptr = (handler_ptr->n_frames_rendered);
            *out_frame_time_ptr  = curr_time                            +
                                   handler_ptr->runtime_time_adjustment -
                                   handler_ptr->playback_start_time;

            break;
        }

        case RAL_RENDERING_HANDLER_POLICY_FPS:
        {
            /* Calculate new frame's index */
            int32_t     frame_index;
            system_time frame_time = (curr_time                            +
                                      handler_ptr->runtime_time_adjustment -
                                      handler_ptr->playback_start_time);
            int32_t     frame_time_msec;
            int32_t     new_frame_time_msec;

            system_time_get_msec_for_time(frame_time,
                                          (uint32_t*) &frame_time_msec);

            frame_index = frame_time_msec * handler_ptr->fps / 1000 /* ms in s */;

            /* Convert the frame index to global frame time. */
            new_frame_time_msec = frame_index * 1000 /* ms in s */ / handler_ptr->fps;
            *out_frame_time_ptr = system_time_get_time_for_msec(new_frame_time_msec);

            *out_frame_index_ptr = frame_index;

            if (should_update_frame_counter)
            {
                handler_ptr->last_frame_index = frame_index;

                /* Count the frame and move on. */
                handler_ptr->n_frames_rendered++;
            }

            break;
        }

        case RAL_RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST:
        {
            *out_frame_time_ptr  = 0;
            *out_frame_index_ptr = 0;

            if (should_update_frame_counter)
            {
                handler_ptr->n_frames_rendered++;
            }

            break;
        }

        default:
        {
            LOG_FATAL("Unrecognized rendering handler policy [%d]",
                      handler_ptr->policy);
        }
    }
}

/** TODO */
PRIVATE bool _ral_rendering_handler_key_down_callback(system_window window,
                                                      unsigned int  keycode,
                                                      void*         user_arg)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) user_arg;
    bool                    result                = rendering_handler_ptr->runtime_time_adjustment_mode;
    const system_time       time_now              = system_time_now();

    /* The rendering handler is only interested in key press notifications, if the
     * runtime time adjustment mode is enabled. The mode lets the user adjust the
     * frame time during the play-back, taking care of synchronizing the audio input
     * along the way.
     */
    if (rendering_handler_ptr->runtime_time_adjustment_mode)
    {
        system_critical_section_enter(rendering_handler_ptr->rendering_cs);

        switch (keycode)
        {
            case SYSTEM_WINDOW_KEY_LEFT:
            {
                if (rendering_handler_ptr->left_arrow_key_press_start_time != 0)
                {
                    /* Left arrow key press notification: continuous mode */
                    system_critical_section_enter(rendering_handler_ptr->rendering_cs);
                    {
                        const system_time min_adjustment = -(time_now - rendering_handler_ptr->playback_start_time);

                        rendering_handler_ptr->runtime_time_adjustment -= rendering_handler_ptr->rewind_delta;

                        /* Prevent against asking the user to render frames for negative frame indices! */
                        if (rendering_handler_ptr->runtime_time_adjustment < min_adjustment)
                        {
                            rendering_handler_ptr->runtime_time_adjustment = min_adjustment;
                        }
                    }
                    system_critical_section_leave(rendering_handler_ptr->rendering_cs);
                }
                else
                {
                    /* Left arrow key press notification: one-shot, potentially continuous. */
                    rendering_handler_ptr->left_arrow_key_press_start_time = system_time_now();
                }

                /* Update the audio stream playback position */
                if (rendering_handler_ptr->active_audio_stream != NULL)
                {
                    int32_t     frame_index;
                    system_time frame_time;

                    _ral_rendering_handler_get_frame_properties(rendering_handler_ptr,
                                                                false, /* should_update_frame_counter */
                                                               &frame_index,
                                                               &frame_time);

                    audio_stream_rewind(rendering_handler_ptr->active_audio_stream,
                                        frame_time);
                }

                break;
            }

            case SYSTEM_WINDOW_KEY_RIGHT:
            {
                if (rendering_handler_ptr->right_arrow_key_press_start_time != 0)
                {
                    /* Right arrow key press notification: continuous mode */
                    system_critical_section_enter(rendering_handler_ptr->rendering_cs);
                    {
                        rendering_handler_ptr->runtime_time_adjustment += rendering_handler_ptr->rewind_delta;
                    }
                    system_critical_section_leave(rendering_handler_ptr->rendering_cs);
                }
                else
                {
                    /* Right arrow key press notification: one-shot, potentially continuous. */
                    rendering_handler_ptr->right_arrow_key_press_start_time = system_time_now();
                }

                /* Update the audio stream playback position */
                if (rendering_handler_ptr->active_audio_stream != NULL)
                {
                    int32_t     frame_index;
                    system_time frame_time;

                    _ral_rendering_handler_get_frame_properties(rendering_handler_ptr,
                                                                false, /* should_update_frame_counter */
                                                               &frame_index,
                                                               &frame_time);

                    audio_stream_rewind(rendering_handler_ptr->active_audio_stream,
                                        frame_time);
                }

                break;
            }

            case SYSTEM_WINDOW_KEY_SPACE:
            {
                if (rendering_handler_ptr->is_space_key_pressed)
                {
                    /* Space press notification: Ignore continuous mode. */
                }
                else
                {
                    rendering_handler_ptr->is_space_key_pressed = true;

                    /* Space key press notification: one-shot.
                     *
                     * Pause or resume the animation, depending on whether the playback is currently
                     * paused or not. */
                    if (rendering_handler_ptr->runtime_time_adjustment_is_paused)
                    {
                        rendering_handler_ptr->runtime_time_adjustment = 0;

                        ral_rendering_handler_play( (ral_rendering_handler) rendering_handler_ptr,
                                                   rendering_handler_ptr->runtime_time_adjustment_paused_frame_time);

                        rendering_handler_ptr->runtime_time_adjustment_is_paused = false;
                    }
                    else
                    {
                        ral_rendering_handler_stop( (ral_rendering_handler) rendering_handler_ptr);

                        rendering_handler_ptr->runtime_time_adjustment_is_paused         = true;
                        rendering_handler_ptr->runtime_time_adjustment_paused_frame_time = rendering_handler_ptr->last_frame_time;
                    }
                }

                break;
            }

            default:
            {
                /* Ignore any other keycodes. */
                result = false;
            }
        }

        system_critical_section_leave(rendering_handler_ptr->rendering_cs);

    }

    return result;
}

/** TODO */
PRIVATE bool _ral_rendering_handler_key_up_callback(system_window window,
                                                    unsigned int  keycode,
                                                    void*         user_arg)
{
    /* Please see _ral_rendering_handler_key_down_callback() documentation for more details */
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) user_arg;
    bool                    result                = rendering_handler_ptr->runtime_time_adjustment_mode;
    const system_time       time_now              = system_time_now();

    /* The rendering handler is only interested in key press notifications, if the
     * runtime time adjustment mode is enabled. The mode lets the user adjust the
     * frame time during the play-back, taking care of synchronizing the audio input
     * along the way.
     */
    if (rendering_handler_ptr->runtime_time_adjustment_mode)
    {
        switch (keycode)
        {
            case SYSTEM_WINDOW_KEY_LEFT:
            {
                system_critical_section_enter(rendering_handler_ptr->rendering_cs);
                {
                    const system_time min_adjustment = -(time_now - rendering_handler_ptr->playback_start_time);

                    rendering_handler_ptr->runtime_time_adjustment -= rendering_handler_ptr->rewind_delta;

                    /* Prevent against asking the user to render frames for negative frame indices! */
                    if (rendering_handler_ptr->runtime_time_adjustment < min_adjustment)
                    {
                        rendering_handler_ptr->runtime_time_adjustment = min_adjustment;
                    }
                }
                system_critical_section_leave(rendering_handler_ptr->rendering_cs);

                rendering_handler_ptr->left_arrow_key_press_start_time = 0;

                /* Update the audio stream playback position */
                if (rendering_handler_ptr->active_audio_stream != NULL)
                {
                    int32_t     frame_index;
                    system_time frame_time;

                    _ral_rendering_handler_get_frame_properties(rendering_handler_ptr,
                                                                false, /* should_update_frame_counter */
                                                               &frame_index,
                                                               &frame_time);

                    audio_stream_rewind(rendering_handler_ptr->active_audio_stream,
                                        frame_time);
                }

                break;
            }

            case SYSTEM_WINDOW_KEY_RIGHT:
            {
                system_critical_section_enter(rendering_handler_ptr->rendering_cs);
                {
                    rendering_handler_ptr->runtime_time_adjustment += rendering_handler_ptr->rewind_delta;
                }
                system_critical_section_leave(rendering_handler_ptr->rendering_cs);

                rendering_handler_ptr->right_arrow_key_press_start_time = 0;

                /* Update the audio stream playback position */
                if (rendering_handler_ptr->active_audio_stream != NULL)
                {
                    int32_t     frame_index;
                    system_time frame_time;

                    _ral_rendering_handler_get_frame_properties(rendering_handler_ptr,
                                                                false, /* should_update_frame_counter */
                                                               &frame_index,
                                                               &frame_time);

                    audio_stream_rewind(rendering_handler_ptr->active_audio_stream,
                                        frame_time);
                }

                break;
            }

            case SYSTEM_WINDOW_KEY_SPACE:
            {
                rendering_handler_ptr->is_space_key_pressed = false;

                break;
            }

            default:
            {
                /* Ignore any other keycodes. */
                result = false;
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE void _ral_rendering_handler_playback_in_progress_callback_handler(uint32_t id,
                                                                          void*    rendering_handler_ral)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler_ral);

    /* Playback in progress - determine frame index and frame time. */
    if (rendering_handler_ptr->playback_status == RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
    {
        float        aspect_ratio       = 0.0f;
        system_time  curr_time          = system_time_now();
        int32_t      frame_index        = 0;
        bool         has_rendered_frame = false;
        system_time  new_frame_time     = 0;
        int          rendering_area[4]  = {0};
        uint32_t     window_size[2];
        uint32_t     window_x1y1x2y2[4];

        system_window_get_property(rendering_handler_ptr->context_window,
                                   SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                                   window_x1y1x2y2);

        window_size[0] = window_x1y1x2y2[2] - window_x1y1x2y2[0];
        window_size[1] = window_x1y1x2y2[3] - window_x1y1x2y2[1];

        system_critical_section_enter(rendering_handler_ptr->rendering_cs);
        {
            /* Determine current frame index & time */
            _ral_rendering_handler_get_frame_properties(rendering_handler_ptr,
                                                        false, /* should_update_frame_counter */
                                                       &frame_index,
                                                       &new_frame_time);

            if (new_frame_time == rendering_handler_ptr->last_frame_time)
            {
                /* This frame should NOT be rendered! It's exactly the same one as we're showing right now */
                system_critical_section_leave(rendering_handler_ptr->rendering_cs);

                goto end;
            }
            else
            {
                /* Update the frame counter */
                rendering_handler_ptr->n_frames_rendered++;
            }

            rendering_handler_ptr->pfn_pre_draw_frame_raBackend_proc(rendering_handler_ptr->rendering_handler_backend);

            /* Update the frame indicator, if the runtime time adjustment mode is on */
            if (rendering_handler_ptr->runtime_time_adjustment_mode)
            {
                uint32_t frame_time_hour;
                uint8_t  frame_time_minute;
                uint8_t  frame_time_second;
                uint32_t frame_time_msec;
                char     temp_buffer[256];

                system_time_get_hms_for_time (new_frame_time,
                                             &frame_time_hour,
                                             &frame_time_minute,
                                             &frame_time_second);
                system_time_get_msec_for_time(new_frame_time,
                                             &frame_time_msec);

                frame_time_msec %= 1000; /* cap the milliseconds at 999 */

                snprintf(temp_buffer,
                         sizeof(temp_buffer),
                         "[%02d:%02d:%02d.%03d frame:%d]",
                         (int) frame_time_hour,
                         (int) frame_time_minute,
                         (int) frame_time_second,
                         (int) frame_time_msec,
                         frame_index);

#if 0
                todo

                varia_text_renderer_set(text_renderer,
                                        rendering_handler_ptr->text_string_id,
                                        temp_buffer);
#endif
            }

            /* Determine AR for the frame. The aspect ratio is configurable on two levels:
             *
             * 1) If the rendering handler has been assigned a timeline instance, we retrieve the AR
             *    value by querying the object.
             * 2) Otherwise, we use the OGL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO property value.
             */
            if (rendering_handler_ptr->timeline != NULL)
            {
                aspect_ratio = demo_timeline_get_aspect_ratio(rendering_handler_ptr->timeline,
                                                              new_frame_time);
            }
            else
            {
                aspect_ratio = rendering_handler_ptr->aspect_ratio;
            }

            /* Determine the rendering area. Note that we do NOT set either scissor box or viewport here.
             * The reason is that the user may need to modify these states during rendering and we would
             * have no way of forcing back the default scissor/viewport configuration in-between.
             *
             * Therefore, we will pass the rendering area data down to the rendering handlers and rely
             * on them taking this data into account. If they don't - well, it's a visual glitch that's
             * immediately visible :-)
             */
            {
                const int rendering_area_width  = window_size[0];
                const int rendering_area_height = (unsigned int) (float(rendering_area_width) / aspect_ratio);

                rendering_area[0] = 0;
                rendering_area[1] = (window_size[1] - rendering_area_height) / 2;
                rendering_area[2] = rendering_area_width;
                rendering_area[3] = window_size[1] - rendering_area[1];
            }

            if (rendering_handler_ptr->pfn_rendering_callback != NULL ||
                rendering_handler_ptr->timeline               != NULL)
            {
                 /* If timeline instance is specified, prefer it over the rendering call-back func ptr */
                 if (rendering_handler_ptr->timeline != NULL)
                 {
                     has_rendered_frame = demo_timeline_render(rendering_handler_ptr->timeline,
                                                               frame_index,
                                                               new_frame_time,
                                                               rendering_area);
                 }
                 else
                 {
                     if (rendering_handler_ptr->pfn_rendering_callback != NULL)
                     {
                         rendering_handler_ptr->pfn_rendering_callback(rendering_handler_ptr->context,
                                                                       frame_index,
                                                                       new_frame_time,
                                                                       rendering_area,
                                                                       rendering_handler_ptr->rendering_callback_user_arg);

                         has_rendered_frame = true;
                     }
                 }

                rendering_handler_ptr->pfn_post_draw_frame_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                          has_rendered_frame);
            }

            if (has_rendered_frame)
            {
                #if 0
                    todo

                    /* Draw the text strings right before we blit the render-target to the system's FBO. */
                    varia_text_renderer_draw(rendering_handler_ptr->context,
                                             text_renderer);
                #endif

                /* Swap back buffer with the front buffer now */
                rendering_handler_ptr->last_frame_index = frame_index;
                rendering_handler_ptr->last_frame_time  = new_frame_time;

                rendering_handler_ptr->pfn_present_frame_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                        rendering_handler_ptr->rendering_cs);

                if (rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST)
                {
                    rendering_handler_ptr->playback_status = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                    system_event_reset(rendering_handler_ptr->playback_in_progress_event);
                    system_event_set  (rendering_handler_ptr->playback_stopped_event);
                }
            }
            system_critical_section_leave(rendering_handler_ptr->rendering_cs);
        }
    }

end:
    ;
}

/** TODO */
PRIVATE void _ral_rendering_handler_shutdown_request_callback_handler(uint32_t id,
                                                                      void*    rendering_handler_ral)
{
    _ral_rendering_handler* rendering_handler_ptr = static_cast<_ral_rendering_handler*>(rendering_handler_ral);

    rendering_handler_ptr->should_thread_live = false;
}

/** TODO */
PRIVATE void _ral_rendering_handler_thread_entrypoint(void* in_arg)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) in_arg;

    {
        rendering_handler_ptr->thread_id = system_threads_get_thread_id();

        LOG_INFO("Rendering handler thread [%x] started",
                 rendering_handler_ptr->thread_id );

        /* Wait until the handler is bound to a context */
        system_event_wait_single(rendering_handler_ptr->context_set_event);

        /* Cache some variables.. */
        ral_context               context_ral         = rendering_handler_ptr->context;
        system_window             context_window      = NULL;
        system_hashed_ansi_string context_window_name = NULL;
        system_pixel_format       context_window_pf   = NULL;
        GLint                     window_size[2]      = {0};

        ral_context_get_property  (rendering_handler_ptr->context,
                                   RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                  &context_window);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                                  &context_window_pf);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_NAME,
                                  &context_window_name);

        ASSERT_DEBUG_SYNC(window_size[0] != 0 &&
                          window_size[1] != 0,
                          "Rendering window's width or height is 0");

        ral_context_retain                (context_ral);

#ifdef TODO
        /* Set up the text renderer.
         *
         * TODO
         */
        if (!is_helper_context)
        {
            /* Create a new text string which we will use to show performance info */
            const float         text_color[4]          = {1.0f, 1.0f, 1.0f, 1.0f};
            varia_text_renderer text_renderer          = NULL;
            const float         text_scale             = 0.75f;
            const int           text_string_position[] = {0, window_size[1]};

            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                                    &text_renderer);

            rendering_handler->text_string_id = varia_text_renderer_add_string(text_renderer);

            varia_text_renderer_set_text_string_property(text_renderer,
                                                         rendering_handler->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                         text_color);
            varia_text_renderer_set_text_string_property(text_renderer,
                                                         rendering_handler->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                         text_string_position);
            varia_text_renderer_set_text_string_property(text_renderer,
                                                         rendering_handler->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                        &text_scale);
        }
#endif

        /* On with the loop */
        unsigned int last_frame_index = 0;

        while (rendering_handler_ptr->should_thread_live)
        {
            size_t event_set = 0;

            system_event_set(rendering_handler_ptr->playback_waiting_event);
            {
                /* TODO: For FPS policy, we could theoretically wait until we actually reach the time
                 *       when frame rendering process should kick off. Right now, we spin until frame
                 *       index increments which is extremely dumb and puts a lot of CPU time down the
                 *       drain.
                 *       One way to tackle this would be to wait with a timeout based on desired_fps
                 *       property. If none of the three first events were set throughout that period,
                 *       we could spend the next 1/desired_fps msec rendering the frame. This has a
                 *       drawback though: if the frame takes too much time to render, we'll introduce
                 *       frame skipping.
                 *       Sticking to the "waste all resources" approach for now, but there's definitely
                 *       room for improvement.
                 */
                event_set = system_event_wait_multiple(rendering_handler_ptr->wait_events,
                                                       rendering_handler_ptr->n_backend_wait_events + rendering_handler_ptr->n_default_wait_events,
                                                       false, /* wait_on_all_objects */
                                                       SYSTEM_TIME_INFINITE,
                                                       NULL); /* out_result_ptr */
            }
            system_event_reset(rendering_handler_ptr->playback_waiting_event);

            const ral_rendering_handler_custom_wait_event_handler& wait_event_handler = rendering_handler_ptr->wait_event_handlers[event_set];

            if (event_set < rendering_handler_ptr->n_backend_wait_events)
            {
                wait_event_handler.pfn_callback_proc(wait_event_handler.id,
                                                     rendering_handler_ptr->rendering_handler_backend);
            }
            else
            {
                wait_event_handler.pfn_callback_proc(wait_event_handler.id,
                                                     rendering_handler_ptr);
            }
        }

        /* Set the 'playback waiting' event in case there's a stop/play() operation outstanding */
        system_event_set(rendering_handler_ptr->playback_waiting_event);

        /* Release the RAL context. At this moment, the GL context should automatically release itself right after.
         * Since it goes out of scope after the ral_context_release() call, we DO NOT unbind it from this thread.*/
        ral_context_release(context_ral);

        /* Let any waiters know that we're done */
        system_event_set(rendering_handler_ptr->shutdown_request_ack_event);

        LOG_INFO("Rendering handler thread [%x] ending",
                 system_threads_get_thread_id() );
    }
}

/** TODO */
PRIVATE void _ral_rendering_handler_release(void* in_arg)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) in_arg;

    /* Release the timeline, if one was assigned. */
    if (rendering_handler_ptr->timeline != NULL)
    {
        demo_timeline_release(rendering_handler_ptr->timeline);

        rendering_handler_ptr->timeline = NULL;
    }

    if (rendering_handler_ptr->context != NULL)
    {
        /* Unsubscribe from the window call-backs */
        system_window window = NULL;

        ral_context_get_property(rendering_handler_ptr->context,
                                 RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                &window);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                           (void*) &_ral_rendering_handler_key_down_callback,
                                           rendering_handler_ptr);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                           (void*) &_ral_rendering_handler_key_up_callback,
                                           rendering_handler_ptr);

        /* Release the context. Do NOT NULLify it yet, as the rendering thread we're looking after
         * will still need this for the tear-down process. */
        ASSERT_DEBUG_SYNC(ral_context_get_refcounter(rendering_handler_ptr->context) > 1,
                          "Reference counter issue detected");

        ral_context_release(rendering_handler_ptr->context);
    }

    /* Shut the rendering thread down */
    system_event_set        (rendering_handler_ptr->context_set_event);
    system_event_set        (rendering_handler_ptr->shutdown_request_event);
    system_event_wait_single(rendering_handler_ptr->shutdown_request_ack_event);

    rendering_handler_ptr->context = NULL;

    /* Release the backend's rendering handler instance */
    if (rendering_handler_ptr->rendering_handler_backend != nullptr)
    {
        rendering_handler_ptr->pfn_release_raBackend_rendering_handler_proc(rendering_handler_ptr->rendering_handler_backend);

        rendering_handler_ptr->rendering_handler_backend = nullptr;
    }

    /* Release the wait handler arrays */
    if (rendering_handler_ptr->wait_events != nullptr)
    {
        for (uint32_t n_default_handler = 0;
                      n_default_handler < rendering_handler_ptr->n_default_wait_events;
                    ++n_default_handler)
        {
            system_event_release(rendering_handler_ptr->wait_events[rendering_handler_ptr->n_backend_wait_events + n_default_handler]);
        }

        delete [] rendering_handler_ptr->wait_events;

        rendering_handler_ptr->wait_events = nullptr;
    }

    if (rendering_handler_ptr->wait_event_handlers != nullptr)
    {
        delete [] rendering_handler_ptr->wait_event_handlers;

        rendering_handler_ptr->wait_event_handlers = nullptr;
    }

    /* Release created events */
    system_event_release(rendering_handler_ptr->context_set_event);
    system_event_release(rendering_handler_ptr->playback_in_progress_event);
    system_event_release(rendering_handler_ptr->playback_stopped_event);
    system_event_release(rendering_handler_ptr->playback_waiting_event);
    system_event_release(rendering_handler_ptr->shutdown_request_event);
    system_event_release(rendering_handler_ptr->shutdown_request_ack_event);

    /* Release rendering cs */
    system_critical_section_release(rendering_handler_ptr->rendering_cs);

    /* Cool to release the descriptor now */
}

/** Please see header for specification */
PUBLIC bool ral_rendering_handler_bind_to_context(ral_rendering_handler rendering_handler,
                                                  ral_context           context)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context == NULL,
                      "Rendering handler is already bound to a context! A rendering handler cannot be bound to more than one context at a time. ");

    if (rendering_handler_ptr->context == NULL)
    {
        system_window context_window = NULL;
        int           window_size[2];

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                &context_window);

        ASSERT_DEBUG_SYNC(context_window != NULL,
                          "No rendering window associated with the rendering context.");

        rendering_handler_ptr->context        = context;
        rendering_handler_ptr->context_window = context_window;
        result                                 = true;

        ral_context_retain(context);
        system_event_set  (rendering_handler_ptr->context_set_event);

        /* Register for key press callbacks. These are essential for the "runtime time adjustment" mode
         * to work correctly.
         *
         * NOTE: The low priority is there for purpose. We definitely do not want to interfere with
         *       other interceptors that might be out there (ogl_flyby, for the off-the-cuff example!)
         */
        if (!system_window_add_callback_func(context_window,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                             (void*) _ral_rendering_handler_key_down_callback,
                                             rendering_handler_ptr)                       ||
            !system_window_add_callback_func(context_window,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                             (void*) _ral_rendering_handler_key_up_callback,
                                             rendering_handler_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not subscribe for key press notifications");
        }

        /* Update aspect ratio to match the window's proportions. */
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        rendering_handler_ptr->aspect_ratio = float(window_size[0]) / window_size[1];
    }

    return result;
}

/** TODO */
PRIVATE ral_rendering_handler ral_rendering_handler_create_shared(ral_backend_type                        backend_type,
                                                                  system_hashed_ansi_string               name,
                                                                  ral_rendering_handler_policy            policy,
                                                                  uint32_t                                desired_fps,
                                                                  PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                  void*                                   user_arg)
{
    _ral_rendering_handler* new_handler_ptr = new (std::nothrow) _ral_rendering_handler;

    ASSERT_ALWAYS_SYNC(new_handler_ptr != NULL,
                       "Out of memory while allocating memroy for rendering handler.");

    if (new_handler_ptr != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_handler_ptr,
                                                       _ral_rendering_handler_release,
                                                       OBJECT_TYPE_RAL_RENDERING_HANDLER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Rendering Handlers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        new_handler_ptr->active_audio_stream                          = nullptr;
        new_handler_ptr->aspect_ratio                                 = 1.0f;
        new_handler_ptr->context                                      = nullptr;
        new_handler_ptr->context_set_event                            = system_event_create(true); /* manual_reset */
        new_handler_ptr->context_window                               = nullptr;
        new_handler_ptr->fps                                          = desired_fps;
        new_handler_ptr->is_space_key_pressed                         = false;
        new_handler_ptr->left_arrow_key_press_start_time              = 0;
        new_handler_ptr->n_frames_rendered                            = 0;
        new_handler_ptr->playback_in_progress_event                   = system_event_create(true); /* manual_reset */
        new_handler_ptr->playback_start_time                          = 0;
        new_handler_ptr->playback_status                              = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
        new_handler_ptr->playback_stopped_event                       = system_event_create(true); /* manual_reset */
        new_handler_ptr->playback_waiting_event                       = system_event_create(false); /* manual_reset */
        new_handler_ptr->pfn_create_raBackend_rendering_handler_proc  = nullptr;
        new_handler_ptr->pfn_init_raBackend_rendering_handler_proc    = nullptr;
        new_handler_ptr->pfn_post_draw_frame_raBackend_proc           = nullptr;
        new_handler_ptr->pfn_pre_draw_frame_raBackend_proc            = nullptr;
        new_handler_ptr->pfn_present_frame_raBackend_proc             = nullptr;
        new_handler_ptr->pfn_release_raBackend_rendering_handler_proc = nullptr;
        new_handler_ptr->pfn_rendering_callback                       = pfn_rendering_callback;
        new_handler_ptr->policy                                       = policy;
        new_handler_ptr->rendering_callback_user_arg                  = user_arg;
        new_handler_ptr->rendering_cs                                 = system_critical_section_create();
        new_handler_ptr->rewind_delta                                 = system_time_get_time_for_msec(REWIND_DELTA_TIME_MSEC);
        new_handler_ptr->rewind_step_min_delta                        = system_time_get_time_for_msec(REWIND_STEP_MIN_DELTA_TIME_MSEC);
        new_handler_ptr->right_arrow_key_press_start_time             = 0;
        new_handler_ptr->runtime_time_adjustment                      = 0;
        new_handler_ptr->runtime_time_adjustment_is_paused            = false;
        new_handler_ptr->runtime_time_adjustment_paused_frame_time    = 0;
        new_handler_ptr->should_thread_live                           = true;
        new_handler_ptr->shutdown_request_event                       = system_event_create (true);  /* manual_reset */
        new_handler_ptr->shutdown_request_ack_event                   = system_event_create (true);  /* manual_reset */
        new_handler_ptr->timeline                                     = nullptr;

        #ifdef _DEBUG
        {
            new_handler_ptr->runtime_time_adjustment_mode = true;
        }
        #else
        {
            new_handler_ptr->runtime_time_adjustment_mode = false;
        }
        #endif

        /* Create the event array we will use in the rendering thread */
        const ral_rendering_handler_custom_wait_event_handler default_handlers[] =
        {
            {new_handler_ptr->shutdown_request_event,     -1, _ral_rendering_handler_shutdown_request_callback_handler},
            {new_handler_ptr->playback_in_progress_event, -1, _ral_rendering_handler_playback_in_progress_callback_handler},
        };
        const uint32_t n_default_handlers = sizeof(default_handlers) / sizeof(default_handlers[0]);

        const ral_rendering_handler_custom_wait_event_handler*     backend_handlers_ptr                          = nullptr;
        uint32_t                                                   n_backend_handlers                            = 0;
        PFNRALRENDERINGHANDLERENUMERATECUSTOMWAITEVENTHANDLERSPROC pfn_enumerate_custom_wait_event_handlers_proc = nullptr;

        switch (backend_type)
        {
            case RAL_BACKEND_TYPE_ES:
            case RAL_BACKEND_TYPE_GL:
            {
                pfn_enumerate_custom_wait_event_handlers_proc                  = raGL_rendering_handler_enumerate_custom_wait_event_handlers;
                new_handler_ptr->pfn_create_raBackend_rendering_handler_proc   = raGL_rendering_handler_create;
                new_handler_ptr->pfn_init_raBackend_rendering_handler_proc     = raGL_rendering_handler_init_from_rendering_thread;
                new_handler_ptr->pfn_post_draw_frame_raBackend_proc            = raGL_rendering_handler_post_draw_frame;
                new_handler_ptr->pfn_pre_draw_frame_raBackend_proc             = raGL_rendering_handler_pre_draw_frame;
                new_handler_ptr->pfn_present_frame_raBackend_proc              = raGL_rendering_handler_present_frame;
                new_handler_ptr->pfn_release_raBackend_rendering_handler_proc  = raGL_rendering_handler_release;
                new_handler_ptr->pfn_request_rendering_callback_raBackend_proc = raGL_rendering_handler_request_callback_for_ral_rendering_handler;

                break;
            }

            default:
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Unsupported ral_backend_type value specified.");
            }
        }

        new_handler_ptr->rendering_handler_backend = new_handler_ptr->pfn_create_raBackend_rendering_handler_proc( (ral_rendering_handler) new_handler_ptr);

        pfn_enumerate_custom_wait_event_handlers_proc(new_handler_ptr->rendering_handler_backend,
                                                     &n_backend_handlers,
                                                     &backend_handlers_ptr);

        new_handler_ptr->n_backend_wait_events = n_backend_handlers;
        new_handler_ptr->n_default_wait_events = n_default_handlers;
        new_handler_ptr->wait_events           = new system_event                                   [n_default_handlers + n_backend_handlers];
        new_handler_ptr->wait_event_handlers   = new ral_rendering_handler_custom_wait_event_handler[n_default_handlers + n_backend_handlers];

        ASSERT_ALWAYS_SYNC(new_handler_ptr->wait_event_handlers != nullptr,
                           "Out of memory");

        for (uint32_t n_backend_handler = 0;
                      n_backend_handler < n_backend_handlers;
                    ++n_backend_handler)
        {
            memcpy(new_handler_ptr->wait_event_handlers + n_backend_handler,
                   backend_handlers_ptr + n_backend_handler,
                   sizeof(backend_handlers_ptr[0]) );

            new_handler_ptr->wait_events[n_backend_handler] = backend_handlers_ptr[n_backend_handler].event;
        }

        for (uint32_t n_default_handler = 0;
                      n_default_handler < n_default_handlers;
                    ++n_default_handler)
        {
            memcpy(new_handler_ptr->wait_event_handlers + n_backend_handlers + n_default_handler,
                   default_handlers + n_default_handler,
                   sizeof(default_handlers[0]) );

            new_handler_ptr->wait_events[n_backend_handlers + n_default_handler] = default_handlers[n_default_handler].event;
        }

        /* Proceed with initialization */
        system_event_set(new_handler_ptr->playback_stopped_event);

        system_threads_spawn(_ral_rendering_handler_thread_entrypoint,
                             new_handler_ptr,
                             NULL, /* thread_wait_event */
                             system_hashed_ansi_string_create("RAL rendering handler thread") );
    }

    return (ral_rendering_handler) new_handler_ptr;
}

/** Please see header for specification */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_fps_policy(ral_backend_type                        backend_type,
                                                                          system_hashed_ansi_string               name,
                                                                          uint32_t                                desired_fps,
                                                                          PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                          void*                                   user_arg)
{
    return ral_rendering_handler_create_shared(backend_type,
                                               name,
                                               RAL_RENDERING_HANDLER_POLICY_FPS,
                                               desired_fps,
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_max_performance_policy(ral_backend_type                        backend_type,
                                                                                      system_hashed_ansi_string               name,
                                                                                      PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                      void*                                   user_arg)
{
    return ral_rendering_handler_create_shared(backend_type,
                                               name,
                                               RAL_RENDERING_HANDLER_POLICY_MAX_PERFORMANCE,
                                               0, /* desired_fps */
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_render_per_request_policy(ral_backend_type                        backend_type,
                                                                                         system_hashed_ansi_string               name,
                                                                                         PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                         void*                                   user_arg)
{
    return ral_rendering_handler_create_shared(backend_type,
                                               name,
                                               RAL_RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST,
                                               0, /* desired_fps */
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC void ral_rendering_handler_get_property(ral_rendering_handler          rendering_handler,
                                               ral_rendering_handler_property property,
                                               void*                          out_result_ptr)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;

    switch (property)
    {
        case RAL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO:
        {
            *(float*) out_result_ptr = rendering_handler_ptr->aspect_ratio;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_IN_PROGRESS_EVENT:
        {
            *(system_event*) out_result_ptr = rendering_handler_ptr->playback_in_progress_event;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STOPPED_EVENT:
        {
            *(system_event*) out_result_ptr = rendering_handler_ptr->playback_stopped_event;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS:
        {
            *(ral_rendering_handler_playback_status*) out_result_ptr = rendering_handler_ptr->playback_status;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_POLICY:
        {
            *(ral_rendering_handler_policy*) out_result_ptr = rendering_handler_ptr->policy;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_HANDLER_BACKEND:
        {
            *(void**) out_result_ptr = rendering_handler_ptr->rendering_handler_backend;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            *(bool*) out_result_ptr = rendering_handler_ptr->runtime_time_adjustment_mode;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_TIMELINE:
        {
            *(demo_timeline*) out_result_ptr = rendering_handler_ptr->timeline;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_rendering_handler_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC bool ral_rendering_handler_is_current_thread_rendering_thread(ral_rendering_handler rendering_handler)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;

    return (system_threads_get_thread_id() == rendering_handler_ptr->thread_id);
}

/** Please see header for specification */
PUBLIC bool ral_rendering_handler_play(ral_rendering_handler rendering_handler,
                                       system_time           start_time)
{
    unsigned int            pre_n_frames_rendered = 0;
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != NULL,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != NULL)
    {
        if (rendering_handler_ptr->playback_status == RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED ||
            rendering_handler_ptr->playback_status == RAL_RENDERING_HANDLER_PLAYBACK_STATUS_PAUSED)
        {
            system_critical_section_enter(rendering_handler_ptr->rendering_cs);
            {
                if (rendering_handler_ptr->playback_status == RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
                {
                    /* We need to consider audio latency at this point, otherwise things will get out-of-sync
                     * on some machines. Two cases here:
                     *
                     * 1) Non-continuous playback is needed ("per-frame policy"): audio playback is not
                     *    necessary. This is the easy case.
                     *
                     * 2) Continuous playback is needed ("max performance / fps policies"): audio playback
                     *    is necessary. In this case, we need to move backward as many frames as it takes for
                     *    the audio packets to be decoded and physically played. Since this can cause a situation
                     *    where the requested frame is negative (example: frame 0 playback request, but the latency
                     *    is ~10 frames, we'd need to start from frame -10!), we will NOT render frame contents
                     *    until the rendering loop starts drawing the frame corresponding to @param start_time. */
                    system_time   audio_latency               = 0;
                    system_window context_window              = NULL;
                    audio_stream  context_window_audio_stream = NULL;
                    bool          use_audio_playback          = false;

                    ral_context_get_property(rendering_handler_ptr->context,
                                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                            &context_window);

                    ASSERT_DEBUG_SYNC(context_window != NULL,
                                      "No window associated with the rendering context!");

                    system_window_get_property(context_window,
                                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                              &context_window_audio_stream);

                    use_audio_playback = (context_window_audio_stream != NULL);

                    if ( use_audio_playback                                                             &&
                        (rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_FPS              ||
                         rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_MAX_PERFORMANCE))
                    {
                        audio_device stream_audio_device = NULL;

                        audio_stream_get_property(context_window_audio_stream,
                                                  AUDIO_STREAM_PROPERTY_AUDIO_DEVICE,
                                                 &stream_audio_device);

                        ASSERT_DEBUG_SYNC(stream_audio_device != NULL,
                                          "No audio_device instance associated with an audio_stream instance.");

                        audio_device_get_property(stream_audio_device,
                                                  AUDIO_DEVICE_PROPERTY_LATENCY,
                                                 &audio_latency);

                        /* Start the audio playback. */
                        audio_stream_play(context_window_audio_stream,
                                          start_time);
                    }

                    rendering_handler_ptr->active_audio_stream = (use_audio_playback) ? context_window_audio_stream
                                                                                      : NULL;
                    rendering_handler_ptr->n_frames_rendered   = 0;
                    rendering_handler_ptr->playback_start_time = system_time_now() - start_time - audio_latency;

                    /* All done! */
                    system_event_set  (rendering_handler_ptr->playback_in_progress_event);
                    system_event_reset(rendering_handler_ptr->playback_stopped_event);
                }
                else
                {
                    /* Must be paused then. */
                    if (rendering_handler_ptr->active_audio_stream != NULL)
                    {
                        /* Resume the audio stream playback */
                        system_time  audio_latency             = 0;
                        audio_device audio_stream_audio_device = NULL;

                        audio_stream_get_property(rendering_handler_ptr->active_audio_stream,
                                                  AUDIO_STREAM_PROPERTY_AUDIO_DEVICE,
                                                 &audio_stream_audio_device);
                        audio_device_get_property(audio_stream_audio_device,
                                                  AUDIO_DEVICE_PROPERTY_LATENCY,
                                                 &audio_latency);

                        audio_stream_resume(rendering_handler_ptr->active_audio_stream);

                        rendering_handler_ptr->playback_start_time += audio_latency;
                    }

                    system_event_set  (rendering_handler_ptr->playback_in_progress_event);
                    system_event_reset(rendering_handler_ptr->playback_stopped_event);
                }

                pre_n_frames_rendered                  = rendering_handler_ptr->n_frames_rendered;
                rendering_handler_ptr->playback_status = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED;
            }
            system_critical_section_leave(rendering_handler_ptr->rendering_cs);

            result = true;
        }
    }

    if (result)
    {
        /* Spin until at least one frame is rendered. This is needed to work around thread racing
         * conditions in one of the RenderingHandler unit tests, reproducible under 32-bit Linux builds.
         */
        while (rendering_handler_ptr->n_frames_rendered == pre_n_frames_rendered)
        {
            ;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_rendering_handler_request_rendering_callback(ral_rendering_handler                   rendering_handler,
                                                                         PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                         void*                                   user_arg,
                                                                         bool                                    swap_buffers_afterward,
                                                                         raGL_rendering_handler_execution_mode   execution_mode)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);

    return rendering_handler_ptr->pfn_request_rendering_callback_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                                pfn_callback_proc,
                                                                                user_arg,
                                                                                swap_buffers_afterward,
                                                                                execution_mode);
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_rendering_handler_set_property(ral_rendering_handler          rendering_handler,
                                                           ral_rendering_handler_property property,
                                                           const void*                    value)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;

    switch (property)
    {
        case RAL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO:
        {
            rendering_handler_ptr->aspect_ratio = *(float*) value;

            ASSERT_DEBUG_SYNC(rendering_handler_ptr->aspect_ratio > 0.0f,
                              "Invalid aspect ratio value (%.4f) assigned.",
                              rendering_handler_ptr->aspect_ratio);

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->playback_status != RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED,
                              "OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK property set attempt while rendering play-back in progress");

            if (rendering_handler_ptr->pfn_rendering_callback == NULL)
            {
                rendering_handler_ptr->pfn_rendering_callback = *(PFNRALRENDERINGHANDLERRENDERINGCALLBACK*) value;
            }

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->playback_status != RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED,
                              "OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT property set attempt while rendering play-back in progress");

            rendering_handler_ptr->rendering_callback_user_arg = *(void**) value;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            rendering_handler_ptr->runtime_time_adjustment_mode = *(bool*) value;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_TIMELINE:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->timeline == NULL                    ||
                              rendering_handler_ptr->timeline == *(demo_timeline*) value,
                              "Another timeline instance is already assigned to the rendering handler!");

            if (rendering_handler_ptr->timeline != *(demo_timeline*) value)
            {
                rendering_handler_ptr->timeline = *(demo_timeline*) value;

                demo_timeline_retain(rendering_handler_ptr->timeline);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_rendering_handler_property value requested.");
        }
    }
}

/** Please see header for specification */
PUBLIC bool ral_rendering_handler_stop(ral_rendering_handler rendering_handler)
{
    _ral_rendering_handler* rendering_handler_ptr = (_ral_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != NULL,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != NULL)
    {
        if (rendering_handler_ptr->playback_status == RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
        {
            system_critical_section_enter(rendering_handler_ptr->rendering_cs);
            {
                /* NOTE: no racing condition here since the pair is always updated inside rendering_cs */
                system_event_reset(rendering_handler_ptr->playback_in_progress_event);
                system_event_set  (rendering_handler_ptr->playback_stopped_event);

                rendering_handler_ptr->playback_status = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
            }
            system_critical_section_leave(rendering_handler_ptr->rendering_cs);

            /* Stop the audio stream (if one is used) */
            if (rendering_handler_ptr->active_audio_stream != NULL)
            {
                audio_stream_stop(rendering_handler_ptr->active_audio_stream);

                rendering_handler_ptr->active_audio_stream = NULL;
            } /* if (rendering_handler_ptr->active_audio_stream != NULL) */

            /* Wait until the rendering process stops */
            if (!ral_rendering_handler_is_current_thread_rendering_thread(rendering_handler))
            {
                system_event_wait_single(rendering_handler_ptr->playback_waiting_event);
            }

            result = true;
        }
    }

    return result;
}

