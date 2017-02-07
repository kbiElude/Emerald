/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "demo/demo_flyby.h"
#include "demo/demo_timeline.h"
#include "demo/demo_window.h"
#include "raGL/raGL_rendering_handler.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_texture_pool.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_pixel_format.h"
#include "system/system_resources.h"
#include "system/system_screen_mode.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_window.h"
#include "ui/ui.h"
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
    system_time                           last_texture_pool_gc_time;
    volatile uint32_t                     n_frames_rendered;     /* NOTE: must be volatile for release builds to work correctly! */
    system_time                           playback_start_time;
    ral_rendering_handler_playback_status playback_status;
    ral_rendering_handler_policy          policy;
    void*                                 rendering_handler_backend; /* eg. raGL_rendering_handler instance for ES/GL contexts */
    system_time                           runtime_time_adjustment;
    bool                                  runtime_time_adjustment_mode;
    varia_text_renderer                   text_renderer;
    varia_text_renderer_text_string_id    text_string_id;
    system_thread_id                      thread_id;
    demo_timeline                         timeline;
    ui                                    ui_instance;

    bool        is_space_key_pressed;
    system_time left_arrow_key_press_start_time;
    system_time rewind_delta;
    system_time rewind_step_min_delta;
    system_time right_arrow_key_press_start_time;
    bool        runtime_time_adjustment_is_paused;
    system_time runtime_time_adjustment_paused_frame_time;

    PFNRALRENDERINGHANDLERCREATERABACKENDRENDERINGHANDLERPROC pfn_create_raBackend_rendering_handler_proc;
    PFNRALRENDERINGHANDLEREXECUTEPRESENTJOB                   pfn_execute_present_job_raBackend_proc;
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
                                          reinterpret_cast<uint32_t*>(&frame_time_msec) );

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
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(user_arg);
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
                if (rendering_handler_ptr->active_audio_stream != nullptr)
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
                if (rendering_handler_ptr->active_audio_stream != nullptr)
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

                        ral_rendering_handler_play(reinterpret_cast<ral_rendering_handler>(rendering_handler_ptr),
                                                   rendering_handler_ptr->runtime_time_adjustment_paused_frame_time);

                        rendering_handler_ptr->runtime_time_adjustment_is_paused = false;
                    }
                    else
                    {
                        ral_rendering_handler_stop(reinterpret_cast<ral_rendering_handler>(rendering_handler_ptr) );

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
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(user_arg);
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
                if (rendering_handler_ptr->active_audio_stream != nullptr)
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
                if (rendering_handler_ptr->active_audio_stream != nullptr)
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
        float           aspect_ratio      = 0.0f;
        system_time     curr_time         = system_time_now();
        demo_flyby      flyby             = nullptr;
        int32_t         frame_index       = 0;
        ral_present_job frame_present_job = nullptr;
        system_time     new_frame_time    = 0;
        int             rendering_area[4] = {0};
        demo_window     window_demo       = nullptr;
        uint32_t        window_size[2];
        uint32_t        window_x1y1x2y2[4];

        ral_context_get_property  (rendering_handler_ptr->context,
                                   RAL_CONTEXT_PROPERTY_WINDOW_DEMO,
                                  &window_demo);
        system_window_get_property(rendering_handler_ptr->context_window,
                                   SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2,
                                   window_x1y1x2y2);

        ASSERT_DEBUG_SYNC(window_demo != nullptr,
                          "No demo_window instance associated with RAL context");

        demo_window_get_property(window_demo,
                                 DEMO_WINDOW_PROPERTY_FLYBY,
                                &flyby);

        window_size[0] = window_x1y1x2y2[2] - window_x1y1x2y2[0];
        window_size[1] = window_x1y1x2y2[3] - window_x1y1x2y2[1];

        system_critical_section_enter(rendering_handler_ptr->rendering_cs);
        {
            bool presentable_output_defined = false;

            /* Let the texture pool do a run over all unused textures and release those that
             * have not been used in a while.
             *
             * This does not need to be executed too frequently, so only spend cycles on this
             * if at least a second has passed since the last time the process has been reported
             * 
             */
            system_time time_now = system_time_now();

            if (time_now - rendering_handler_ptr->last_texture_pool_gc_time > system_time_get_time_for_s(1) )
            {
                ral_texture_pool texture_pool = nullptr;

                ral_context_get_private_property(rendering_handler_ptr->context,
                                                 RAL_CONTEXT_PRIVATE_PROPERTY_TEXTURE_POOL,
                                                &texture_pool);

                if (!ral_texture_pool_collect_garbage(texture_pool) )
                {
                    rendering_handler_ptr->last_texture_pool_gc_time = time_now;
                }
            }

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
                         static_cast<int>(frame_time_hour),
                         static_cast<int>(frame_time_minute),
                         static_cast<int>(frame_time_second),
                         static_cast<int>(frame_time_msec),
                         frame_index);

                varia_text_renderer_set(rendering_handler_ptr->text_renderer,
                                        rendering_handler_ptr->text_string_id,
                                        temp_buffer);
            }

            /* Determine AR for the frame. The aspect ratio is configurable on two levels:
             *
             * 1) If the rendering handler has been assigned a timeline instance, we retrieve the AR
             *    value by querying the object.
             * 2) Otherwise, we use the OGL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO property value.
             */
            if (rendering_handler_ptr->timeline != nullptr)
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
                const int rendering_area_height = static_cast<unsigned int>(float(rendering_area_width) / aspect_ratio);

                rendering_area[0] = 0;
                rendering_area[1] = (window_size[1] - rendering_area_height) / 2;
                rendering_area[2] = rendering_area_width;
                rendering_area[3] = window_size[1] - rendering_area[1];
            }

            /* If the flyby is activated for the context the pipeline owns, update it now */
            bool is_flyby_active = false;

            demo_flyby_get_property(flyby,
                                    DEMO_FLYBY_PROPERTY_IS_ACTIVE,
                                   &is_flyby_active);

            if (is_flyby_active)
            {
                demo_flyby_update(flyby);
            }

            if (rendering_handler_ptr->pfn_rendering_callback != nullptr ||
                rendering_handler_ptr->timeline               != nullptr)
            {
                 /* If timeline instance has been specified, prefer it over the rendering call-back func ptr */
                 if (rendering_handler_ptr->timeline != nullptr)
                 {
                     frame_present_job = demo_timeline_render(rendering_handler_ptr->timeline,
                                                              frame_index,
                                                              new_frame_time,
                                                              rendering_area);
                 }
                 else
                 {
                     if (rendering_handler_ptr->pfn_rendering_callback != nullptr)
                     {
                         ral_rendering_handler_rendering_callback_frame_data frame_data;

                         frame_data.frame_time                = new_frame_time;
                         frame_data.n_frame                   = frame_index;
                         frame_data.rendering_area_px_topdown = rendering_area;

                         frame_present_job = rendering_handler_ptr->pfn_rendering_callback(rendering_handler_ptr->context,
                                                                                           rendering_handler_ptr->rendering_callback_user_arg,
                                                                                          &frame_data);
                     }
                 }

                 if (frame_present_job != nullptr)
                 {
                     ral_present_job_get_property(frame_present_job,
                                                  RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED,
                                                 &presentable_output_defined);
                 }

                 if (presentable_output_defined)
                 {
                     /* If there are UI components to render, also attach relevant tasks */
                     uint32_t n_ui_controls = 0;

                     ui_get_property(rendering_handler_ptr->ui_instance,
                                     UI_PROPERTY_N_CONTROLS,
                                    &n_ui_controls);

                     if (n_ui_controls > 0)
                     {
                         ral_present_task         presentable_output_task         = nullptr;
                         uint32_t                 presentable_output_task_index   = -1;
                         uint32_t                 presentable_output_task_io      = -1;
                         ral_present_task_io_type presentable_output_task_io_type;
                         ral_texture_view         presentable_texture_view         = nullptr;
                         ral_present_task         ui_task;
                         ral_present_task_id      ui_task_id;

                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID,
                                                     &presentable_output_task_index);
                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_INDEX,
                                                     &presentable_output_task_io);
                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_TYPE,
                                                     &presentable_output_task_io_type);

                         ral_present_job_get_task_with_id(frame_present_job,
                                                          presentable_output_task_index,
                                                         &presentable_output_task);

                         ral_present_task_get_io_property(presentable_output_task,
                                                          presentable_output_task_io_type,
                                                          presentable_output_task_io,
                                                          RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,
                                                          reinterpret_cast<void**>(&presentable_texture_view) );

                         ui_task = ui_get_present_task(rendering_handler_ptr->ui_instance,
                                                       presentable_texture_view);

                         /* Adjust connections accordingly */
                         ral_present_job_add_task(frame_present_job,
                                                  ui_task,
                                                 &ui_task_id);
                         ral_present_task_release(ui_task);

                         ASSERT_DEBUG_SYNC(presentable_output_task_io_type == RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                           "Cannot render UI over a presentable texture view which acts as a task input");

                         ral_present_job_connect_tasks(frame_present_job,
                                                       presentable_output_task_index,
                                                       presentable_output_task_io,
                                                       ui_task_id,
                                                       0,        /* n_dst_task_input          */
                                                       nullptr); /* out_opt_connection_id_ptr */

                         ral_present_job_set_presentable_output(frame_present_job,
                                                                ui_task_id,
                                                                false, /* is_input_io */
                                                                0);    /* n_io        */
                     }

                     /* Draw the text strings over the presentable texture */
                     {
                         /* Retrieve the text rendering present task .. */
                         ral_present_task         draw_text_strings_task          = nullptr;
                         ral_present_task_id      draw_text_strings_task_id       = -1;
                         ral_present_task         presentable_output_task         = nullptr;
                         uint32_t                 presentable_output_task_index   = -1;
                         uint32_t                 presentable_output_task_io      = -1;
                         ral_present_task_io_type presentable_output_task_io_type;
                         ral_texture_view         presentable_texture_view         = nullptr;

                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID,
                                                     &presentable_output_task_index);
                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_INDEX,
                                                     &presentable_output_task_io);
                         ral_present_job_get_property(frame_present_job,
                                                      RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_TYPE,
                                                     &presentable_output_task_io_type);

                         ral_present_job_get_task_with_id(frame_present_job,
                                                          presentable_output_task_index,
                                                         &presentable_output_task);

                         ral_present_task_get_io_property(presentable_output_task,
                                                          presentable_output_task_io_type,
                                                          presentable_output_task_io,
                                                          RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,
                                                          reinterpret_cast<void**>(&presentable_texture_view) );

                         draw_text_strings_task = varia_text_renderer_get_present_task(rendering_handler_ptr->text_renderer,
                                                                                       presentable_texture_view);

                         /* Attach the new present task to the present job */
                         ral_present_job_add_task(frame_present_job,
                                                  draw_text_strings_task,
                                                 &draw_text_strings_task_id);
                         ral_present_task_release(draw_text_strings_task);

                         ASSERT_DEBUG_SYNC(presentable_output_task_io_type == RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                           "Cannot render text strings over a presentable texture view which acts as a task input");

                         ral_present_job_connect_tasks(frame_present_job,
                                                       presentable_output_task_index,
                                                       presentable_output_task_io,
                                                       draw_text_strings_task_id,
                                                       0,        /* n_dst_task_input          */
                                                       nullptr); /* out_opt_connection_id_ptr */

                         ral_present_job_set_presentable_output(frame_present_job,
                                                                draw_text_strings_task_id,
                                                                false, /* is_input_io */
                                                                0);    /* n_io        */
                     }
                }

                /* We may need to flatten (i.e. unroll any group tasks that may be defined) the graph at this point. */
                ral_present_job_flatten(frame_present_job);

                /* OK, go for it */
                rendering_handler_ptr->pfn_execute_present_job_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                              frame_present_job);

                rendering_handler_ptr->pfn_post_draw_frame_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                          frame_present_job);
            }

            /* Do a buffer flip */
            {
                bool should_swap_buffers = false;

                if (presentable_output_defined)
                {
                    ral_present_job_get_property(frame_present_job,
                                                 RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED,
                                                &should_swap_buffers);
                }
                else
                {
                    should_swap_buffers = true;
                }

                rendering_handler_ptr->last_frame_index = frame_index;
                rendering_handler_ptr->last_frame_time  = new_frame_time;

                if (should_swap_buffers)
                {
                    /* OK to present the frame now */
                    rendering_handler_ptr->pfn_present_frame_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                            rendering_handler_ptr->rendering_cs);
                }

                if (rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST)
                {
                    rendering_handler_ptr->playback_status = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                    system_event_reset(rendering_handler_ptr->playback_in_progress_event);
                    system_event_set  (rendering_handler_ptr->playback_stopped_event);
                }

                if (frame_present_job)
                {
                    ral_present_job_release(frame_present_job);
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
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(in_arg);

    {
        rendering_handler_ptr->thread_id = system_threads_get_thread_id();

        LOG_INFO("Rendering handler thread [%x] started",
                 rendering_handler_ptr->thread_id );

        /* Wait until the handler is bound to a context */
        system_event_wait_single(rendering_handler_ptr->context_set_event);

        /* Cache some variables.. */
        ral_context               context_ral         = rendering_handler_ptr->context;
        system_window             context_window      = nullptr;
        system_hashed_ansi_string context_window_name = nullptr;
        system_pixel_format       context_window_pf   = nullptr;
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

        ral_context_retain(context_ral);

        /* Let the back-end initialize as well. */
        rendering_handler_ptr->pfn_init_raBackend_rendering_handler_proc(rendering_handler_ptr->context,
                                                                         reinterpret_cast<ral_rendering_handler>(rendering_handler_ptr),
                                                                         rendering_handler_ptr->rendering_handler_backend);

        if (!system_hashed_ansi_string_contains(context_window_name,
                                                system_hashed_ansi_string_create("Helper") ))
        {
            /* Set up the text renderer for non-helper contexts. */
            const float               text_color[4]          = {1.0f, 1.0f, 1.0f, 1.0f};
            const  float              text_default_size      = 0.75f;
            const float               text_scale             = 0.75f;
            const int                 text_string_position[] = {0, window_size[1]};
            system_hashed_ansi_string window_name            = nullptr;

            system_window_get_property(rendering_handler_ptr->context_window,
                                       SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                       window_size);
            system_window_get_property(rendering_handler_ptr->context_window,
                                       SYSTEM_WINDOW_PROPERTY_NAME,
                                      &window_name);

            rendering_handler_ptr->text_renderer = varia_text_renderer_create(window_name,
                                                                              rendering_handler_ptr->context,
                                                                              system_resources_get_meiryo_font_table(),
                                                                              window_size[0],
                                                                              window_size[1]);

            varia_text_renderer_set_text_string_property(rendering_handler_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                        &text_default_size);
            varia_text_renderer_set_text_string_property(rendering_handler_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                         text_color);

            rendering_handler_ptr->text_string_id = varia_text_renderer_add_string(rendering_handler_ptr->text_renderer);

            varia_text_renderer_set_text_string_property(rendering_handler_ptr->text_renderer,
                                                         rendering_handler_ptr->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                         text_color);
            varia_text_renderer_set_text_string_property(rendering_handler_ptr->text_renderer,
                                                         rendering_handler_ptr->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                         text_string_position);
            varia_text_renderer_set_text_string_property(rendering_handler_ptr->text_renderer,
                                                         rendering_handler_ptr->text_string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE,
                                                        &text_scale);

            /* We could also use a UI */
            rendering_handler_ptr->ui_instance = ui_create(rendering_handler_ptr->text_renderer,
                                                           system_hashed_ansi_string_create("UI renderer") );
        }

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
                                                       nullptr); /* out_result_ptr */
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
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(in_arg);

    /* Release the timeline, if one was assigned. */
    if (rendering_handler_ptr->timeline != nullptr)
    {
        demo_timeline_release(rendering_handler_ptr->timeline);

        rendering_handler_ptr->timeline = nullptr;
    }

    /* Release various renderers */
    if (rendering_handler_ptr->text_renderer != nullptr)
    {
        varia_text_renderer_release(rendering_handler_ptr->text_renderer);

        rendering_handler_ptr->text_renderer = nullptr;
    }

    if (rendering_handler_ptr->ui_instance != nullptr)
    {
        ui_release(rendering_handler_ptr->ui_instance);

        rendering_handler_ptr->ui_instance = nullptr;
    }

    /* Follow with the rest .. */
    if (rendering_handler_ptr->context != nullptr)
    {
        /* Unsubscribe from the window call-backs */
        system_window window = nullptr;

        ral_context_get_property(rendering_handler_ptr->context,
                                 RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                &window);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                           reinterpret_cast<void*>(&_ral_rendering_handler_key_down_callback),
                                           rendering_handler_ptr);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                           reinterpret_cast<void*>(&_ral_rendering_handler_key_up_callback),
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

    rendering_handler_ptr->context = nullptr;

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

            rendering_handler_ptr->wait_events[rendering_handler_ptr->n_backend_wait_events + n_default_handler] = nullptr;
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
    rendering_handler_ptr->context_set_event = nullptr;

    system_event_release(rendering_handler_ptr->playback_stopped_event);
    rendering_handler_ptr->playback_stopped_event = nullptr;

    system_event_release(rendering_handler_ptr->playback_waiting_event);
    rendering_handler_ptr->playback_waiting_event = nullptr;

    system_event_release(rendering_handler_ptr->shutdown_request_ack_event);
    rendering_handler_ptr->shutdown_request_ack_event = nullptr;

    /* Release rendering cs */
    system_critical_section_release(rendering_handler_ptr->rendering_cs);

    /* Cool to release the descriptor now */
}

/** Please see header for specification */
PUBLIC bool ral_rendering_handler_bind_to_context(ral_rendering_handler rendering_handler,
                                                  ral_context           context)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context == nullptr,
                      "Rendering handler is already bound to a context! A rendering handler cannot be bound to more than one context at a time. ");

    if (rendering_handler_ptr->context == nullptr)
    {
        system_window context_window = nullptr;
        int           window_size[2];

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                &context_window);

        ASSERT_DEBUG_SYNC(context_window != nullptr,
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
                                             reinterpret_cast<void*>(_ral_rendering_handler_key_down_callback),
                                             rendering_handler_ptr)                       ||
            !system_window_add_callback_func(context_window,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                             reinterpret_cast<void*>(_ral_rendering_handler_key_up_callback),
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

    ASSERT_ALWAYS_SYNC(new_handler_ptr != nullptr,
                       "Out of memory while allocating memroy for rendering handler.");

    if (new_handler_ptr != nullptr)
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
        new_handler_ptr->last_texture_pool_gc_time                    = system_time_now();
        new_handler_ptr->left_arrow_key_press_start_time              = 0;
        new_handler_ptr->n_frames_rendered                            = 0;
        new_handler_ptr->playback_in_progress_event                   = system_event_create(true); /* manual_reset */
        new_handler_ptr->playback_start_time                          = 0;
        new_handler_ptr->playback_status                              = RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
        new_handler_ptr->playback_stopped_event                       = system_event_create(true); /* manual_reset */
        new_handler_ptr->playback_waiting_event                       = system_event_create(false); /* manual_reset */
        new_handler_ptr->pfn_create_raBackend_rendering_handler_proc  = nullptr;
        new_handler_ptr->pfn_execute_present_job_raBackend_proc       = nullptr;
        new_handler_ptr->pfn_init_raBackend_rendering_handler_proc    = nullptr;
        new_handler_ptr->pfn_post_draw_frame_raBackend_proc           = nullptr;
        new_handler_ptr->pfn_pre_draw_frame_raBackend_proc            = nullptr;
        new_handler_ptr->pfn_present_frame_raBackend_proc             = nullptr;
        new_handler_ptr->pfn_release_raBackend_rendering_handler_proc = nullptr;
        new_handler_ptr->pfn_rendering_callback                       = pfn_rendering_callback;
        new_handler_ptr->policy                                       = policy;
        new_handler_ptr->rendering_callback_user_arg                  = user_arg;
        new_handler_ptr->rendering_cs                                 = system_critical_section_create();
        new_handler_ptr->rendering_handler_backend                    = nullptr;
        new_handler_ptr->rewind_delta                                 = system_time_get_time_for_msec(REWIND_DELTA_TIME_MSEC);
        new_handler_ptr->rewind_step_min_delta                        = system_time_get_time_for_msec(REWIND_STEP_MIN_DELTA_TIME_MSEC);
        new_handler_ptr->right_arrow_key_press_start_time             = 0;
        new_handler_ptr->runtime_time_adjustment                      = 0;
        new_handler_ptr->runtime_time_adjustment_is_paused            = false;
        new_handler_ptr->runtime_time_adjustment_paused_frame_time    = 0;
        new_handler_ptr->should_thread_live                           = true;
        new_handler_ptr->shutdown_request_event                       = system_event_create (true);  /* manual_reset */
        new_handler_ptr->shutdown_request_ack_event                   = system_event_create (true);  /* manual_reset */
        new_handler_ptr->text_renderer                                = nullptr;
        new_handler_ptr->timeline                                     = nullptr;
        new_handler_ptr->ui_instance                                  = nullptr;

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
            {new_handler_ptr->shutdown_request_event,     UINT_MAX, _ral_rendering_handler_shutdown_request_callback_handler},
            {new_handler_ptr->playback_in_progress_event, UINT_MAX, _ral_rendering_handler_playback_in_progress_callback_handler},
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
                new_handler_ptr->pfn_execute_present_job_raBackend_proc        = raGL_rendering_handler_execute_present_job;
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
                             nullptr, /* thread_wait_event */
                             system_hashed_ansi_string_create("RAL rendering handler thread") );
    }

    return reinterpret_cast<ral_rendering_handler>(new_handler_ptr);
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
PUBLIC EMERALD_API void ral_rendering_handler_get_property(ral_rendering_handler          rendering_handler,
                                                           ral_rendering_handler_property property,
                                                           void*                          out_result_ptr)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);

    switch (property)
    {
        case RAL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO:
        {
            *reinterpret_cast<float*>(out_result_ptr) = rendering_handler_ptr->aspect_ratio;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = rendering_handler_ptr->context;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_IN_PROGRESS_EVENT:
        {
            *reinterpret_cast<system_event*>(out_result_ptr) = rendering_handler_ptr->playback_in_progress_event;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STOPPED_EVENT:
        {
            *reinterpret_cast<system_event*>(out_result_ptr) = rendering_handler_ptr->playback_stopped_event;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS:
        {
            *reinterpret_cast<ral_rendering_handler_playback_status*>(out_result_ptr) = rendering_handler_ptr->playback_status;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_POLICY:
        {
            *reinterpret_cast<ral_rendering_handler_policy*>(out_result_ptr) = rendering_handler_ptr->policy;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_HANDLER_BACKEND:
        {
            *reinterpret_cast<void**>(out_result_ptr) = rendering_handler_ptr->rendering_handler_backend;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = rendering_handler_ptr->runtime_time_adjustment_mode;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_TIMELINE:
        {
            *reinterpret_cast<demo_timeline*>(out_result_ptr) = rendering_handler_ptr->timeline;

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_UI:
        {
            *reinterpret_cast<ui*>(out_result_ptr) = rendering_handler_ptr->ui_instance;

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
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);

    return (system_threads_get_thread_id() == rendering_handler_ptr->thread_id);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_rendering_handler_play(ral_rendering_handler rendering_handler,
                                                   system_time           start_time)
{
    unsigned int            pre_n_frames_rendered = 0;
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != nullptr,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != nullptr)
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
                    system_window context_window              = nullptr;
                    audio_stream  context_window_audio_stream = nullptr;
                    bool          use_audio_playback          = false;

                    ral_context_get_property(rendering_handler_ptr->context,
                                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                            &context_window);

                    ASSERT_DEBUG_SYNC(context_window != nullptr,
                                      "No window associated with the rendering context!");

                    system_window_get_property(context_window,
                                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                              &context_window_audio_stream);

                    use_audio_playback = (context_window_audio_stream != nullptr);

                    if ( use_audio_playback                                                             &&
                        (rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_FPS              ||
                         rendering_handler_ptr->policy == RAL_RENDERING_HANDLER_POLICY_MAX_PERFORMANCE))
                    {
                        audio_device stream_audio_device = nullptr;

                        audio_stream_get_property(context_window_audio_stream,
                                                  AUDIO_STREAM_PROPERTY_AUDIO_DEVICE,
                                                 &stream_audio_device);

                        ASSERT_DEBUG_SYNC(stream_audio_device != nullptr,
                                          "No audio_device instance associated with an audio_stream instance.");

                        audio_device_get_property(stream_audio_device,
                                                  AUDIO_DEVICE_PROPERTY_LATENCY,
                                                 &audio_latency);

                        /* Start the audio playback. */
                        audio_stream_play(context_window_audio_stream,
                                          start_time);
                    }

                    rendering_handler_ptr->active_audio_stream = (use_audio_playback) ? context_window_audio_stream
                                                                                      : nullptr;
                    rendering_handler_ptr->n_frames_rendered   = 0;
                    rendering_handler_ptr->playback_start_time = system_time_now() - start_time - audio_latency;

                    /* All done! */
                    system_event_set  (rendering_handler_ptr->playback_in_progress_event);
                    system_event_reset(rendering_handler_ptr->playback_stopped_event);
                }
                else
                {
                    /* Must be paused then. */
                    if (rendering_handler_ptr->active_audio_stream != nullptr)
                    {
                        /* Resume the audio stream playback */
                        system_time  audio_latency             = 0;
                        audio_device audio_stream_audio_device = nullptr;

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
PUBLIC bool ral_rendering_handler_request_rendering_callback(ral_rendering_handler                   rendering_handler,
                                                             PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                             void*                                   user_arg,
                                                             bool                                    present_after_executed,
                                                             ral_rendering_handler_execution_mode    execution_mode)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);

    return rendering_handler_ptr->pfn_request_rendering_callback_raBackend_proc(rendering_handler_ptr->rendering_handler_backend,
                                                                                pfn_callback_proc,
                                                                                user_arg,
                                                                                present_after_executed,
                                                                                execution_mode);
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_rendering_handler_set_property(ral_rendering_handler          rendering_handler,
                                                           ral_rendering_handler_property property,
                                                           const void*                    value)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);

    switch (property)
    {
        case RAL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO:
        {
            rendering_handler_ptr->aspect_ratio = *reinterpret_cast<const float*>(value);

            ASSERT_DEBUG_SYNC(rendering_handler_ptr->aspect_ratio > 0.0f,
                              "Invalid aspect ratio value (%.4f) assigned.",
                              rendering_handler_ptr->aspect_ratio);

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->playback_status != RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED,
                              "OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK property set attempt while rendering play-back in progress");

            if (rendering_handler_ptr->pfn_rendering_callback == nullptr)
            {
                rendering_handler_ptr->pfn_rendering_callback = *reinterpret_cast<const PFNRALRENDERINGHANDLERRENDERINGCALLBACK*>(value);
            }

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->playback_status != RAL_RENDERING_HANDLER_PLAYBACK_STATUS_STARTED,
                              "OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT property set attempt while rendering play-back in progress");

            rendering_handler_ptr->rendering_callback_user_arg = *reinterpret_cast<void* const*>(value);

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            rendering_handler_ptr->runtime_time_adjustment_mode = *reinterpret_cast<const bool*>(value);

            break;
        }

        case RAL_RENDERING_HANDLER_PROPERTY_TIMELINE:
        {
            ASSERT_DEBUG_SYNC(rendering_handler_ptr->timeline == nullptr                                        ||
                              rendering_handler_ptr->timeline == *reinterpret_cast<const demo_timeline*>(value),
                              "Another timeline instance is already assigned to the rendering handler!");

            if (rendering_handler_ptr->timeline != *reinterpret_cast<const demo_timeline*>(value) )
            {
                rendering_handler_ptr->timeline = *reinterpret_cast<const demo_timeline*>(value);

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
PUBLIC EMERALD_API bool ral_rendering_handler_stop(ral_rendering_handler rendering_handler)
{
    _ral_rendering_handler* rendering_handler_ptr = reinterpret_cast<_ral_rendering_handler*>(rendering_handler);
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != nullptr,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != nullptr)
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
            if (rendering_handler_ptr->active_audio_stream != nullptr)
            {
                audio_stream_stop(rendering_handler_ptr->active_audio_stream);

                rendering_handler_ptr->active_audio_stream = nullptr;
            }

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

