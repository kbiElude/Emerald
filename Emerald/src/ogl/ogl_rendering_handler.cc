/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "audio/audio_device.h"
#include "audio/audio_stream.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
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

#define REWIND_DELTA_TIME_MSEC          (1500)
#define REWIND_STEP_MIN_DELTA_TIME_MSEC (500)


/** Internal type definitions */
typedef struct
{
    audio_stream                          active_audio_stream;
    ogl_context                           context;
    system_event                          context_set_event;
    uint32_t                              fps;
    uint32_t                              last_frame_index;      /* only when fps policy is used */
    system_time                           last_frame_time;       /* cached for the runtime time adjustment mode support */
    volatile uint32_t                     n_frames_rendered;     /* NOTE: must be volatile for release builds to work correctly! */
    system_time                           playback_start_time;
    ogl_rendering_handler_playback_status playback_status;
    ogl_rendering_handler_policy          policy;
    system_time                           runtime_time_adjustment;
    bool                                  runtime_time_adjustment_mode;
    ogl_text_string_id                    text_string_id;
    system_thread_id                      thread_id;

    bool        is_space_key_pressed;
    system_time left_arrow_key_press_start_time;
    system_time rewind_delta;
    system_time rewind_step_min_delta;
    system_time right_arrow_key_press_start_time;
    bool        runtime_time_adjustment_is_paused;
    system_time runtime_time_adjustment_paused_frame_time;

    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback;
    void*                                   rendering_callback_user_arg;

    system_critical_section                    callback_request_cs;
    system_event                               callback_request_ack_event;
    system_event                               callback_request_event;
    void*                                      callback_request_user_arg;
    PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc;

    system_event bind_context_request_event;
    system_event bind_context_request_ack_event;
    system_event unbind_context_request_event;
    system_event unbind_context_request_ack_event;

    system_event            playback_in_progress_event;
    system_critical_section rendering_cs;
    system_event            shutdown_request_event;
    system_event            shutdown_request_ack_event;
    system_event            playback_waiting_event; /* event set when rendering thread is about to start waiting on playback
                                                       events, reset when rendering thread is executing */

    REFCOUNT_INSERT_VARIABLES
} _ogl_rendering_handler;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_rendering_handler,
                               ogl_rendering_handler,
                              _ogl_rendering_handler);

/** Internal variables */

/** TODO */
PRIVATE bool _ogl_rendering_handler_key_down_callback(system_window window,
                                                      unsigned int  keycode,
                                                      void*         user_arg)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) user_arg;
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
                } /* if (rendering_handler_ptr->left_arrow_key_press_start_time != 0) */
                else
                {
                    /* Left arrow key press notification: one-shot, potentially continuous. */
                    rendering_handler_ptr->left_arrow_key_press_start_time = system_time_now();
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
                } /* if (rendering_handler_ptr->right_arrow_key_press_start_time != 0) */
                else
                {
                    /* Right arrow key press notification: one-shot, potentially continuous. */
                    rendering_handler_ptr->right_arrow_key_press_start_time = system_time_now();
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

                        ogl_rendering_handler_play( (ogl_rendering_handler) rendering_handler_ptr,
                                                   rendering_handler_ptr->runtime_time_adjustment_paused_frame_time);

                        rendering_handler_ptr->runtime_time_adjustment_is_paused = false;
                    } /* if (rendering_handler_ptr->runtime_time_adjustment_is_paused) */
                    else
                    {
                        ogl_rendering_handler_stop( (ogl_rendering_handler) rendering_handler_ptr);

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
        } /* switch (keycode) */
    } /* if (rendering_handler_ptr->runtime_time_adjustment_mode) */

    return result;
}

/** TODO */
PRIVATE bool _ogl_rendering_handler_key_up_callback(system_window window,
                                                    unsigned int  keycode,
                                                    void*         user_arg)
{
    /* Please see _ogl_rendering_handler_key_down_callback() documentation for more details */
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) user_arg;
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
        } /* switch (keycode) */
    } /* if (rendering_handler_ptr->runtime_time_adjustment_mode) */

    return result;
}

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_lock_bound_context(ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    if (system_threads_get_thread_id() != rendering_handler_ptr->thread_id)
    {
        system_event_set        (rendering_handler_ptr->unbind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->unbind_context_request_ack_event);
    }
}

/** TODO */
PRIVATE void _ogl_rendering_handler_thread_entrypoint(void* in_arg)
{
    _ogl_rendering_handler* rendering_handler = (_ogl_rendering_handler*) in_arg;

    {
        rendering_handler->thread_id = system_threads_get_thread_id();

        LOG_INFO("Rendering handler thread [%x] started",
                 rendering_handler->thread_id );

        /* Wait until the handler is bound to a context */
        system_event_wait_single(rendering_handler->context_set_event);

        /* Cache some variables.. */
        ogl_context_type          context_type             = OGL_CONTEXT_TYPE_UNDEFINED;
        system_window             context_window           = NULL;
        unsigned char             context_window_n_samples = 0;
        system_hashed_ansi_string context_window_name      = NULL;
        system_pixel_format       context_window_pf        = NULL;
        GLuint                    default_fbo_id           = -1;
        bool                      default_fbo_id_set       = false;
        bool                      is_multisample_pf        = false;
        bool                      is_root_window           = false;
        PFNGLBINDFRAMEBUFFERPROC  pGLBindFramebuffer       = NULL;
        PFNGLBLITFRAMEBUFFERPROC  pGLBlitFramebuffer       = NULL;
        PFNGLDISABLEPROC          pGLDisable               = NULL;
        PFNGLENABLEPROC           pGLEnable                = NULL;
        PFNGLFINISHPROC           pGLFinish                = NULL;
        bool                      should_live              = true;
        const system_event        wait_events[]            =
        {
            rendering_handler->shutdown_request_event,
            rendering_handler->callback_request_event,
            rendering_handler->unbind_context_request_event,
            rendering_handler->playback_in_progress_event
        };
        GLint                     window_size[2]           = {0};

        ogl_context_get_property  (rendering_handler->context,
                                   OGL_CONTEXT_PROPERTY_TYPE,
                                  &context_type);
        ogl_context_get_property  (rendering_handler->context,
                                   OGL_CONTEXT_PROPERTY_WINDOW,
                                  &context_window);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_IS_ROOT_WINDOW,
                                  &is_root_window);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,
                                  &context_window_pf);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_NAME,
                                  &context_window_name);

        system_pixel_format_get_property(context_window_pf,
                                         SYSTEM_PIXEL_FORMAT_PROPERTY_N_SAMPLES,
                                        &context_window_n_samples);

        is_multisample_pf = (context_window_n_samples > 1);

        ASSERT_DEBUG_SYNC(window_size[0] != 0 &&
                          window_size[1] != 0,
                          "Rendering window's width or height is 0");

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entrypoints = NULL;

            ogl_context_get_property(rendering_handler->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entrypoints);

            pGLBindFramebuffer = entrypoints->pGLBindFramebuffer;
            pGLBlitFramebuffer = entrypoints->pGLBlitFramebuffer;
            pGLFinish          = entrypoints->pGLFinish;
        }
        else
        {
            const ogl_context_gl_entrypoints* entrypoints = NULL;

            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            ogl_context_get_property(rendering_handler->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entrypoints);

            pGLBindFramebuffer = entrypoints->pGLBindFramebuffer;
            pGLBlitFramebuffer = entrypoints->pGLBlitFramebuffer;
            pGLDisable         = entrypoints->pGLDisable;
            pGLEnable          = entrypoints->pGLEnable;
            pGLFinish          = entrypoints->pGLFinish;
        }

        /* Bind the thread to GL */
        ogl_context_bind_to_current_thread(rendering_handler->context);

        /* Create a new text string which we will use to show performance info */
        const float text_color[4]          = {1.0f, 1.0f, 1.0f, 1.0f};
        ogl_text    text_renderer          = NULL;
        const float text_scale             = 0.75f;
        const int   text_string_position[] = {0, window_size[1]};

        ogl_context_get_property(rendering_handler->context,
                                 OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                                &text_renderer);

        rendering_handler->text_string_id = ogl_text_add_string(text_renderer);

        ogl_text_set_text_string_property(text_renderer,
                                          rendering_handler->text_string_id,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          text_color);
        ogl_text_set_text_string_property(text_renderer,
                                          rendering_handler->text_string_id,
                                          OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                          text_string_position);
        ogl_text_set_text_string_property(text_renderer,
                                          rendering_handler->text_string_id,
                                          OGL_TEXT_STRING_PROPERTY_SCALE,
                                         &text_scale);

        /* On with the loop */
        unsigned int last_frame_index = 0;

        while (should_live)
        {
            size_t event_set = 0;

            system_event_set(rendering_handler->playback_waiting_event);
            {
                event_set = system_event_wait_multiple(wait_events,
                                                       4,     /* n_elements */
                                                       false, /* wait_on_all_objects */
                                                       SYSTEM_TIME_INFINITE,
                                                       NULL); /* out_result_ptr */
            }
            system_event_reset(rendering_handler->playback_waiting_event);

            if (event_set == 0)
            {
                /* Got to leave now */
                should_live = false;
            }
            else
            if (event_set == 1)
            {
                /* Call-back requested */
                rendering_handler->pfn_callback_proc(rendering_handler->context,
                                                     rendering_handler->callback_request_user_arg);

                /* Reset callback data */
                rendering_handler->callback_request_user_arg = NULL;
                rendering_handler->pfn_callback_proc         = NULL;

                /* Set ack event */
                system_event_set(rendering_handler->callback_request_ack_event);
            }
            else
            if (event_set == 2)
            {
                /* This event is used for setting up context sharing. In order for it to succeed, we must unbind the context from
                 * current thread, then wait for a 'job done' signal, and bind the context back to this thread */
                ogl_context_unbind_from_current_thread(rendering_handler->context);

                system_event_set        (rendering_handler->unbind_context_request_ack_event);
                system_event_wait_single(rendering_handler->bind_context_request_event);

                ogl_context_bind_to_current_thread(rendering_handler->context);

                system_event_set(rendering_handler->bind_context_request_ack_event);
            }
            else
            {
                /* Playback in progress - determine frame index and frame time. */
                bool is_vsync_enabled = false;

                system_window_get_property(context_window,
                                           SYSTEM_WINDOW_PROPERTY_IS_VSYNC_ENABLED,
                                          &is_vsync_enabled);

                if (rendering_handler->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
                {
                    system_time curr_time      = system_time_now();
                    system_time new_frame_time = 0;
                    int32_t     frame_index    = 0;

                    system_critical_section_enter(rendering_handler->rendering_cs);
                    {
                        /* Determine current frame index & time */
                        switch (rendering_handler->policy)
                        {
                            case RENDERING_HANDLER_POLICY_MAX_PERFORMANCE:
                            {
                                frame_index    = (rendering_handler->n_frames_rendered++);
                                new_frame_time = curr_time                                  +
                                                 rendering_handler->runtime_time_adjustment -
                                                 rendering_handler->playback_start_time;

                                break;
                            }

                            case RENDERING_HANDLER_POLICY_FPS:
                            {
                                /* Calculate new frame's index */
                                system_time frame_time = (curr_time                                  +
                                                          rendering_handler->runtime_time_adjustment -
                                                          rendering_handler->playback_start_time);
                                int32_t     frame_time_msec;
                                int32_t     new_frame_time_msec;

                                system_time_get_msec_for_time(frame_time,
                                                              (uint32_t*) &frame_time_msec);

                                frame_index = frame_time_msec * rendering_handler->fps / 1000 /* ms in s */;

                                /* Now, this part is tricky. Occasionally, we may run into a situation where the newly
                                 * calculated frame index is exactly the same as was used for the previous frame.
                                 * This is unintended and causes irritating stuttering effect.
                                 *
                                 * To work around this, ensure we always move forward. This is not going to address the
                                 * problem, if the animation is ever played in reverse - you'll need to come up with
                                 * something more fancy, if you ever need to change the playback direction.
                                 */
                                if (frame_index == last_frame_index)
                                {
                                    frame_index++;
                                }

                                last_frame_index = frame_index;

                                /* Convert the frame index to global frame time. */
                                new_frame_time_msec = frame_index     * 1000 /* ms in s */     / rendering_handler->fps;
                                new_frame_time      = system_time_get_time_for_msec(new_frame_time_msec);

                                /* Count the frame and move on. */
                                rendering_handler->n_frames_rendered++;

                                break;
                            }

                            case RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST:
                            {
                                new_frame_time = 0;
                                frame_index    = 0;

                                rendering_handler->n_frames_rendered++;

                                break;
                            }

                            default:
                            {
                                LOG_FATAL("Unrecognized rendering handler policy [%d]",
                                          rendering_handler->policy);
                            }
                        } /* switch (rendering_handler->policy) */

                        /* Update the frame indicator, if the runtime time adjustment mode is on */
                        if (rendering_handler->runtime_time_adjustment_mode)
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

                            ogl_text_set(text_renderer,
                                         rendering_handler->text_string_id,
                                         temp_buffer);
                        }

                        /* Bind the context's default FBO and call the user app's call-back */
                        if (!default_fbo_id_set)
                        {
                            ogl_context_get_property(rendering_handler->context,
                                                     OGL_CONTEXT_PROPERTY_DEFAULT_FBO_ID,
                                                    &default_fbo_id);

                            ASSERT_DEBUG_SYNC( is_root_window && default_fbo_id == 0 ||
                                              !is_root_window && default_fbo_id != 0,
                                             "Rendering context's default FBO is assigned an invalid ID");

                            default_fbo_id_set = true;
                        }

                        if (rendering_handler->pfn_rendering_callback != NULL &&
                            default_fbo_id_set && default_fbo_id      != 0)
                        {
                            pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                               default_fbo_id);

                            if (is_multisample_pf && context_type == OGL_CONTEXT_TYPE_GL)
                            {
                                pGLEnable(GL_MULTISAMPLE);
                            }

                            rendering_handler->pfn_rendering_callback(rendering_handler->context,
                                                                      frame_index,
                                                                      new_frame_time,
                                                                      rendering_handler->rendering_callback_user_arg);

                            if (is_multisample_pf && context_type == OGL_CONTEXT_TYPE_GL)
                            {
                                pGLDisable(GL_MULTISAMPLE);
                            }

                            /* Blit the context FBO's contents to the back buffer */
                            pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                               0);
                            pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                               default_fbo_id);

                            pGLBlitFramebuffer(0,              /* srcX0 */
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

                        /* Swap back buffer with the front buffer now */
                        rendering_handler->last_frame_index = frame_index;
                        rendering_handler->last_frame_time  = new_frame_time;

                        ogl_context_swap_buffers(rendering_handler->context);

                        /* If vertical sync is enabled, force a full pipeline flush + GPU-side execution
                         * of the enqueued commands. This helps fighting the tearing A LOT for scenes
                         * that take more time to render (eg. the test "greets" scene which slides
                         * from right to left), since the frame misses are less frequent. It also puts
                         * less pressure on the CPU, since we're no longer spinning.
                         */
                        if (is_vsync_enabled)
                        {
                            system_critical_section_leave(rendering_handler->rendering_cs);
                            {
                                pGLFinish();
                            }
                            system_critical_section_enter(rendering_handler->rendering_cs);
                        } /* if (is_vsync_enabled) */

                        if (rendering_handler->policy == RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST)
                        {
                            rendering_handler->playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                            system_event_reset(rendering_handler->playback_in_progress_event);
                        }
                    }
                    system_critical_section_leave(rendering_handler->rendering_cs);
                } /* if (rendering_handler->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED) */
            } /* if (event_set == 3) */
        }

        /* Set the 'playback waiting' event in case there's a stop/play() operation outstanding */
        system_event_set(rendering_handler->playback_waiting_event);

        /* Unbind the thread from GL */
        ogl_context_unbind_from_current_thread(rendering_handler->context);

        /* Let any waiters know that we're done */
        system_event_set(rendering_handler->shutdown_request_ack_event);

        LOG_INFO("Rendering handler thread [%x] ending",
                 system_threads_get_thread_id() );
    }
}

/** TODO */
PRIVATE void _ogl_rendering_handler_release(void* in_arg)
{
    _ogl_rendering_handler* rendering_handler = (_ogl_rendering_handler*) in_arg;

    /* Shut the rendering thread down */
    system_event_set        (rendering_handler->context_set_event);
    system_event_set        (rendering_handler->shutdown_request_event);
    system_event_wait_single(rendering_handler->shutdown_request_ack_event);

    /* Release the context */
    ogl_context_release(rendering_handler->context);

    /* Release created events */
    system_event_release(rendering_handler->bind_context_request_event);
    system_event_release(rendering_handler->bind_context_request_ack_event);
    system_event_release(rendering_handler->callback_request_ack_event);
    system_event_release(rendering_handler->callback_request_event);
    system_event_release(rendering_handler->context_set_event);
    system_event_release(rendering_handler->playback_in_progress_event);
    system_event_release(rendering_handler->playback_waiting_event);
    system_event_release(rendering_handler->shutdown_request_event);
    system_event_release(rendering_handler->shutdown_request_ack_event);
    system_event_release(rendering_handler->unbind_context_request_event);
    system_event_release(rendering_handler->unbind_context_request_ack_event);

    /* Unsubscribe from the window call-backs */
    if (rendering_handler->context != NULL)
    {
        system_window window = NULL;

        ogl_context_get_property(rendering_handler->context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &window);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                           (void*) &_ogl_rendering_handler_key_down_callback,
                                           rendering_handler);

        system_window_delete_callback_func(window,
                                           SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                           (void*) &_ogl_rendering_handler_key_up_callback,
                                           rendering_handler);
    } /* if (rendering_handler->context != NULL) */

    /* Release rendering cs */
    system_critical_section_release(rendering_handler->rendering_cs);

    /* Release user callback serialization cs */
    system_critical_section_release(rendering_handler->callback_request_cs);

    /* Cool to release the descriptor now */
}

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_unlock_bound_context(ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    if (system_threads_get_thread_id() != rendering_handler_ptr->thread_id)
    {
        system_event_set        (rendering_handler_ptr->bind_context_request_event);
        system_event_wait_single(rendering_handler_ptr->bind_context_request_ack_event);
    }
}

/** TODO */
PRIVATE ogl_rendering_handler ogl_rendering_handler_create_shared(system_hashed_ansi_string               name,
                                                                  ogl_rendering_handler_policy            policy,
                                                                  uint32_t                                desired_fps,
                                                                  PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                  void*                                   user_arg)
{
    _ogl_rendering_handler* new_handler = new (std::nothrow) _ogl_rendering_handler;

    ASSERT_ALWAYS_SYNC(new_handler != NULL,
                       "Out of memory while allocating memroy for rendering handler.");

    if (new_handler != NULL)
    {
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_handler,
                                                       _ogl_rendering_handler_release,
                                                       OBJECT_TYPE_OGL_RENDERING_HANDLER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Rendering Handlers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        new_handler->active_audio_stream                       = NULL;
        new_handler->bind_context_request_event                = system_event_create(false); /* manual_reset */
        new_handler->bind_context_request_ack_event            = system_event_create(false); /* manual_reset */
        new_handler->callback_request_ack_event                = system_event_create(false); /* manual_reset */
        new_handler->callback_request_event                    = system_event_create(false); /* manual_reset */
        new_handler->callback_request_cs                       = system_critical_section_create();
        new_handler->callback_request_user_arg                 = NULL;
        new_handler->context                                   = NULL;
        new_handler->context_set_event                         = system_event_create(true); /* manual_reset */
        new_handler->fps                                       = desired_fps;
        new_handler->is_space_key_pressed                      = false;
        new_handler->left_arrow_key_press_start_time           = 0;
        new_handler->n_frames_rendered                         = 0;
        new_handler->playback_in_progress_event                = system_event_create(true); /* manual_reset */
        new_handler->playback_start_time                       = 0;
        new_handler->playback_status                           = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
        new_handler->playback_waiting_event                    = system_event_create(false); /* manual_reset */
        new_handler->pfn_callback_proc                         = NULL;
        new_handler->pfn_rendering_callback                    = pfn_rendering_callback;
        new_handler->policy                                    = policy;
        new_handler->rendering_callback_user_arg               = user_arg;
        new_handler->rendering_cs                              = system_critical_section_create();
        new_handler->rewind_delta                              = system_time_get_time_for_msec(REWIND_DELTA_TIME_MSEC);
        new_handler->rewind_step_min_delta                     = system_time_get_time_for_msec(REWIND_STEP_MIN_DELTA_TIME_MSEC);
        new_handler->right_arrow_key_press_start_time          = 0;
        new_handler->runtime_time_adjustment                   = 0;
        new_handler->runtime_time_adjustment_is_paused         = false;
        new_handler->runtime_time_adjustment_paused_frame_time = 0;
        new_handler->shutdown_request_event                    = system_event_create(true);  /* manual_reset */
        new_handler->shutdown_request_ack_event                = system_event_create(true);  /* manual_reset */
        new_handler->unbind_context_request_event              = system_event_create(false); /* manual_reset */
        new_handler->unbind_context_request_ack_event          = system_event_create(false); /* manual_reset */

        #ifdef _DEBUG
        {
            new_handler->runtime_time_adjustment_mode = true;
        }
        #else
        {
            new_handler->runtime_time_adjustment_mode = false;
        }
        #endif

        ASSERT_ALWAYS_SYNC(new_handler->bind_context_request_event != NULL,
                           "Could not create 'bind context request' event");
        ASSERT_ALWAYS_SYNC(new_handler->bind_context_request_ack_event != NULL,
                          "Could not create 'bind context request ack' event");
        ASSERT_ALWAYS_SYNC(new_handler->callback_request_ack_event != NULL,
                           "Could not create 'callback request ack' event");
        ASSERT_ALWAYS_SYNC(new_handler->callback_request_event != NULL,
                           "Could not create 'callback request' event");
        ASSERT_ALWAYS_SYNC(new_handler->callback_request_cs != NULL,
                           "Could not create 'callback request' cs");
        ASSERT_ALWAYS_SYNC(new_handler->context_set_event != NULL,
                           "Could not create 'context set' event");
        ASSERT_ALWAYS_SYNC(new_handler->playback_in_progress_event != NULL,
                           "Could not create 'playback in progress' event");
        ASSERT_ALWAYS_SYNC(new_handler->playback_waiting_event != NULL,
                           "Could not create 'playback waiting' event");
        ASSERT_ALWAYS_SYNC(new_handler->shutdown_request_event != NULL,
                           "Could not create 'shutdown requested' event");
        ASSERT_ALWAYS_SYNC(new_handler->shutdown_request_ack_event != NULL,
                           "Could not create 'shutdown request ack' event");
        ASSERT_ALWAYS_SYNC(new_handler->unbind_context_request_event != NULL,
                           "Could not create 'unbind context request' event");
        ASSERT_ALWAYS_SYNC(new_handler->unbind_context_request_ack_event != NULL,
                           "Could not create 'unbind context request ack' event");

        system_threads_spawn(_ogl_rendering_handler_thread_entrypoint,
                             new_handler,
                             NULL, /* thread_wait_event */
                             system_hashed_ansi_string_create("OpenGL rendering handler thread") );
    }

    return (ogl_rendering_handler) new_handler;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string               name,
                                                                                      uint32_t                                desired_fps,
                                                                                      PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                      void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_FPS,
                                               desired_fps,
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string               name,
                                                                                                  PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                  void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_MAX_PERFORMANCE,
                                               0, /* desired_fps */
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string               name,
                                                                                                     PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                     void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST,
                                               0, /* desired_fps */
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_rendering_handler_get_property(ogl_rendering_handler          rendering_handler,
                                                           ogl_rendering_handler_property property,
                                                           void*                          out_result)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    switch (property)
    {
        case OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS:
        {
            *(ogl_rendering_handler_playback_status*) out_result = rendering_handler_ptr->playback_status;

            break;
        }

        case OGL_RENDERING_HANDLER_PROPERTY_POLICY:
        {
            *(ogl_rendering_handler_policy*) out_result = rendering_handler_ptr->policy;

            break;
        }

        case OGL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            *(bool*) out_result = rendering_handler_ptr->runtime_time_adjustment_mode;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_rendering_handler_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_is_current_thread_rendering_thread(ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    return (system_threads_get_thread_id() == rendering_handler_ptr->thread_id);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_play(ogl_rendering_handler rendering_handler,
                                                   system_time           start_time)
{
    unsigned int            pre_n_frames_rendered = 0;
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != NULL,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != NULL)
    {
        if (rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED ||
            rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_PAUSED)
        {
            system_critical_section_enter(rendering_handler_ptr->rendering_cs);
            {
                if (rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED)
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

                    ogl_context_get_property(rendering_handler_ptr->context,
                                             OGL_CONTEXT_PROPERTY_WINDOW,
                                            &context_window);

                    ASSERT_DEBUG_SYNC(context_window != NULL,
                                      "No window associated with the rendering context!");

                    system_window_get_property(context_window,
                                               SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,
                                              &context_window_audio_stream);

                    use_audio_playback = (context_window_audio_stream != NULL);

                    if ( use_audio_playback                                                         &&
                        (rendering_handler_ptr->policy == RENDERING_HANDLER_POLICY_FPS              ||
                         rendering_handler_ptr->policy == RENDERING_HANDLER_POLICY_MAX_PERFORMANCE))
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
                    system_event_set(rendering_handler_ptr->playback_in_progress_event);
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

                    system_event_set(rendering_handler_ptr->playback_in_progress_event);
                }

                pre_n_frames_rendered                  = rendering_handler_ptr->n_frames_rendered;
                rendering_handler_ptr->playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STARTED;
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

/** Please see header for specification. */
PUBLIC EMERALD_API bool ogl_rendering_handler_request_callback_from_context_thread(ogl_rendering_handler                      rendering_handler,
                                                                                   PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc,
                                                                                   void*                                      user_arg,
                                                                                   bool                                       block_until_available)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;
    bool                    result                = false;

    if (rendering_handler_ptr->thread_id == system_threads_get_thread_id() )
    {
        pfn_callback_proc(rendering_handler_ptr->context,
                          user_arg);

        result = true;
    }
    else
    {
        bool should_continue = false;

        if (block_until_available)
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
            rendering_handler_ptr->pfn_callback_proc         = pfn_callback_proc;

            system_event_set        (rendering_handler_ptr->callback_request_event);
            system_event_wait_single(rendering_handler_ptr->callback_request_ack_event);

            system_critical_section_leave(rendering_handler_ptr->callback_request_cs);
        }

        result = should_continue;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_rendering_handler_set_property(ogl_rendering_handler          rendering_handler,
                                                           ogl_rendering_handler_property property,
                                                           const void*                    value)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    switch (property)
    {
        case OGL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE:
        {
            rendering_handler_ptr->runtime_time_adjustment_mode = *(bool*) value;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_rendering_handler_property value requested.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_stop(ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context != NULL,
                      "Cannot play - the handler is not bound to any window.");

    if (rendering_handler_ptr->context != NULL)
    {
        if (rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
        {
            system_critical_section_enter(rendering_handler_ptr->rendering_cs);
            {
                system_event_reset(rendering_handler_ptr->playback_in_progress_event);

                rendering_handler_ptr->playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
            }
            system_critical_section_leave(rendering_handler_ptr->rendering_cs);

            /* Stop the audio stream (if one is used) */
            if (rendering_handler_ptr->active_audio_stream != NULL)
            {
                audio_stream_stop(rendering_handler_ptr->active_audio_stream);

                rendering_handler_ptr->active_audio_stream = NULL;
            } /* if (rendering_handler_ptr->active_audio_stream != NULL) */

            /* Wait until the rendering process stops */
            system_event_wait_single(rendering_handler_ptr->playback_waiting_event);

            result = true;
        } /* if (rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED) */
    } /* if (rendering_handler_ptr->context != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC bool _ogl_rendering_handler_on_bound_to_context(ogl_rendering_handler rendering_handler,
                                                       ogl_context           context)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context == NULL,
                      "Rendering handler is already bound to a context! A rendering handler cannot be bound to more than one context at a time. ");

    if (rendering_handler_ptr->context == NULL)
    {
        system_window context_window = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &context_window);

        ASSERT_DEBUG_SYNC(context_window != NULL,
                          "No rendering window associated with the rendering context.");

        rendering_handler_ptr->context = context;
        result                         = true;

        ogl_context_retain(context);
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
                                             (void*) _ogl_rendering_handler_key_down_callback,
                                             rendering_handler_ptr)                       ||
            !system_window_add_callback_func(context_window,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW,
                                             SYSTEM_WINDOW_CALLBACK_FUNC_KEY_UP,
                                             (void*) _ogl_rendering_handler_key_up_callback,
                                             rendering_handler_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not subscribe for key press notifications");
        }
    } /* if (rendering_handler_ptr->context == NULL) */

    return result;
}
