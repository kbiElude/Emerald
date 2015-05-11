/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_threads.h"
#include "system/system_time.h"
#include "system/system_window.h"

/** Internal type definitions */
typedef struct
{
    ogl_context                           context;
    system_event                          context_set_event;
    uint32_t                              fps;
    bool                                  fps_counter_status;
    uint32_t                              last_frame_index;      /* only when fps policy is used */
    uint32_t                              n_frames_rendered;
    system_timeline_time                  playback_start_time;
    ogl_rendering_handler_playback_status playback_status;
    ogl_rendering_handler_policy          policy;
    ogl_text_string_id                    text_string_id;
    system_thread_id                      thread_id;

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
PUBLIC EMERALD_API void ogl_rendering_handler_lock_bound_context(__in __notnull ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    if (system_threads_get_thread_id() != rendering_handler_ptr->thread_id)
    {
        system_event_set                 (rendering_handler_ptr->unbind_context_request_event);
        system_event_wait_single_infinite(rendering_handler_ptr->unbind_context_request_ack_event);
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
        system_event_wait_single_infinite(rendering_handler->context_set_event);

        /* There are two events we have to wait for at this point. It's either kill rendering thread event
         * which should make us quit the loop, or 'playback active' event in which case we should call back
         * the user.
         */
        ogl_context_type   context_type   = OGL_CONTEXT_TYPE_UNDEFINED;
        PFNGLFINISHPROC    pGLFinish      = NULL;
        bool               should_live    = true;
        const system_event wait_events[]  =
        {
            rendering_handler->shutdown_request_event,
            rendering_handler->callback_request_event,
            rendering_handler->unbind_context_request_event,
            rendering_handler->playback_in_progress_event
        };

        ogl_context_get_property(rendering_handler->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            ogl_context_es_entrypoints* entrypoints = NULL;

            ogl_context_get_property(rendering_handler->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entrypoints);

            pGLFinish = entrypoints->pGLFinish;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            ogl_context_gl_entrypoints* entrypoints = NULL;

            ogl_context_get_property(rendering_handler->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entrypoints);

            pGLFinish = entrypoints->pGLFinish;

        }

        /* Retrieve context's DC which we will need for buffer swaps */
        HDC                       context_dc            = NULL;
        system_window             context_window        = NULL;
        system_hashed_ansi_string context_window_name   = NULL;

        ogl_context_get_property  (rendering_handler->context,
                                   OGL_CONTEXT_PROPERTY_DC,
                                  &context_dc);
        ogl_context_get_property  (rendering_handler->context,
                                   OGL_CONTEXT_PROPERTY_WINDOW,
                                  &context_window);
        system_window_get_property(context_window,
                                   SYSTEM_WINDOW_PROPERTY_NAME,
                                  &context_window_name);

        /* Bind the thread to GL */
        ogl_context_bind_to_current_thread(rendering_handler->context);

        /* Create a new text string which we will use to show performance info */
        ogl_text text_renderer = NULL;

        ogl_context_get_property(rendering_handler->context,
                                 OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                                &text_renderer);

        rendering_handler->text_string_id = ogl_text_add_string(text_renderer);

        /* On with the loop */
        while (should_live)
        {
            size_t event_set = 0;

            system_event_set(rendering_handler->playback_waiting_event);
            {
                event_set = system_event_wait_multiple_infinite(wait_events,
                                                                4,
                                                                false);
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
                ogl_context_unbind_from_current_thread();

                system_event_set                 (rendering_handler->unbind_context_request_ack_event);
                system_event_wait_single_infinite(rendering_handler->bind_context_request_event);

                ogl_context_bind_to_current_thread(rendering_handler->context);

                system_event_set(rendering_handler->bind_context_request_ack_event);
            }
            else
            {
                /* Playback in progress - determine frame index and frame time. */
                system_critical_section_enter(rendering_handler->rendering_cs);
                {
                    if (rendering_handler->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED)
                    {
                        system_timeline_time curr_time      = system_time_now();
                        system_timeline_time new_frame_time = 0;
                        uint32_t             frame_index    = 0;

                        switch (rendering_handler->policy)
                        {
                            case RENDERING_HANDLER_POLICY_MAX_PERFORMANCE:
                            {
                                frame_index    = (rendering_handler->n_frames_rendered++);
                                new_frame_time = curr_time - rendering_handler->playback_start_time;

                                break;
                            }

                            case RENDERING_HANDLER_POLICY_FPS:
                            {
                                new_frame_time = curr_time - rendering_handler->playback_start_time;
                                frame_index    = new_frame_time * rendering_handler->fps / HZ_PER_SEC;

                                new_frame_time = rendering_handler->playback_start_time +
                                                 frame_index * HZ_PER_SEC * rendering_handler->fps / 1000;

                                break;
                            }

                            case RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST:
                            {
                                new_frame_time = 0;
                                frame_index    = 0;

                                break;
                            }

                            default:
                            {
                                LOG_FATAL("Unrecognized rendering handler policy [%d]",
                                          rendering_handler->policy);
                            }
                        } /* switch (rendering_handler->policy) */

                        /* Call the user app's call-back */
                        system_timeline_time rendering_start_time = system_time_now();
                        {
                            rendering_handler->pfn_rendering_callback(rendering_handler->context,
                                                                      frame_index,
                                                                      new_frame_time,
                                                                      rendering_handler->rendering_callback_user_arg);

                            if (rendering_handler->fps_counter_status)
                            {
                                pGLFinish();
                            }
                        }
                        system_timeline_time rendering_end_time = system_time_now();

                        if (rendering_handler->fps_counter_status)
                        {
                            /* Render FPS info */
                            char                 rendering_time_buffer[128] = {0};
                            system_timeline_time rendering_time_delta       = rendering_end_time - rendering_start_time;
                            uint32_t             rendering_time_msec        = 0;
                            int                  rendering_time_pos[2]      = {0};
                            int                  rendering_time_text_height = 0;
                            int                  window_size[2];

                            system_window_get_property            (context_window,
                                                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                                                   window_size);
                            system_time_get_msec_for_timeline_time(rendering_time_delta,
                                                                  &rendering_time_msec);

                            sprintf_s(rendering_time_buffer,
                                      sizeof(rendering_time_buffer),
                                      "Rendering time: %.4fs (%d FPS)",
                                      float(rendering_time_msec) / 1000.0f,
                                      int(1000.0f / (rendering_time_msec != 0 ? rendering_time_msec : 1) ) );

                            ogl_text_set(text_renderer,
                                         rendering_handler->text_string_id,
                                         rendering_time_buffer);

                            ogl_text_get_text_string_property(text_renderer,
                                                              OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                              rendering_handler->text_string_id,
                                                             &rendering_time_text_height);

                            rendering_time_pos[1] = window_size[1] - rendering_time_text_height;

                            ogl_text_set_text_string_property(text_renderer,
                                                              rendering_handler->text_string_id,
                                                              OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                                              rendering_time_pos);

                            ogl_text_draw(rendering_handler->context,
                                          text_renderer);
                        } /* if (rendering_handler->fps_counter_status) */

                        /* Swap back buffer with the front buffer now */
                        rendering_handler->last_frame_index = frame_index;

                        ::SwapBuffers(context_dc);

                        if (rendering_handler->policy == RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST)
                        {
                            rendering_handler->playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;

                            system_event_reset(rendering_handler->playback_in_progress_event);
                        }
                    } /* if (rendering_handler->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED) */
                }
                system_critical_section_leave(rendering_handler->rendering_cs);
            }
        }

        /* Set the 'playback waiting' event in case there's a stop/play() operation outstanding */
        system_event_set(rendering_handler->playback_waiting_event);

        /* Unbind the thread from GL */
        ogl_context_unbind_from_current_thread();

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
    system_event_set                 (rendering_handler->context_set_event);
    system_event_set                 (rendering_handler->shutdown_request_event);
    system_event_wait_single_infinite(rendering_handler->shutdown_request_ack_event);

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

    /* Release rendering cs */
    system_critical_section_release(rendering_handler->rendering_cs);

    /* Release user callback serialization cs */
    system_critical_section_release(rendering_handler->callback_request_cs);

    /* Cool to release the descriptor now */
}

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_unlock_bound_context(__in __notnull ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    if (system_threads_get_thread_id() != rendering_handler_ptr->thread_id)
    {
        system_event_set                 (rendering_handler_ptr->bind_context_request_event);
        system_event_wait_single_infinite(rendering_handler_ptr->bind_context_request_ack_event);
    }
}

/** TODO */
PRIVATE ogl_rendering_handler ogl_rendering_handler_create_shared(__in __notnull system_hashed_ansi_string               name,
                                                                                 ogl_rendering_handler_policy            policy,
                                                                  __in           uint32_t                                desired_fps,
                                                                  __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
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

        new_handler->bind_context_request_event       = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->bind_context_request_ack_event   = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->callback_request_ack_event       = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->callback_request_event           = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->callback_request_cs              = system_critical_section_create();
        new_handler->callback_request_user_arg        = NULL;
        new_handler->context                          = NULL;
        new_handler->context_set_event                = system_event_create(true,   /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->fps                              = desired_fps;
        new_handler->fps_counter_status               = false;
        new_handler->n_frames_rendered                = 0;
        new_handler->playback_in_progress_event       = system_event_create(true,   /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->playback_start_time              = 0;
        new_handler->playback_status                  = RENDERING_HANDLER_PLAYBACK_STATUS_STOPPED;
        new_handler->playback_waiting_event           = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->pfn_callback_proc                = NULL;
        new_handler->pfn_rendering_callback           = pfn_rendering_callback;
        new_handler->policy                           = policy;
        new_handler->rendering_callback_user_arg      = user_arg;
        new_handler->rendering_cs                     = system_critical_section_create();
        new_handler->shutdown_request_event           = system_event_create(true,   /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->shutdown_request_ack_event       = system_event_create(true,   /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->unbind_context_request_event     = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */
        new_handler->unbind_context_request_ack_event = system_event_create(false,  /* manual_reset */
                                                                            false); /* start_state  */

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
                             NULL);
    }

    return (ogl_rendering_handler) new_handler;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_fps_policy(__in __notnull system_hashed_ansi_string               name,
                                                                                      __in           uint32_t                                desired_fps,
                                                                                      __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                     void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_FPS,
                                               desired_fps,
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_max_performance_policy(__in __notnull system_hashed_ansi_string               name,
                                                                                                  __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                                 void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_MAX_PERFORMANCE,
                                               0, /* desired_fps */
                                               pfn_rendering_callback, user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_render_per_request_policy(__in __notnull system_hashed_ansi_string               name,
                                                                                                     __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                                    void*                                   user_arg)
{
    return ogl_rendering_handler_create_shared(name,
                                               RENDERING_HANDLER_POLICY_RENDER_PER_REQUEST,
                                               0, /* desired_fps */
                                               pfn_rendering_callback,
                                               user_arg);
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_rendering_handler_get_property(__in  __notnull ogl_rendering_handler          rendering_handler,
                                                           __in            ogl_rendering_handler_property property,
                                                           __out __notnull void*                          out_result)
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

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_rendering_handler_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_is_current_thread_rendering_thread(__in __notnull ogl_rendering_handler rendering_handler)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;

    return (system_threads_get_thread_id() == rendering_handler_ptr->thread_id);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_play(__in __notnull ogl_rendering_handler rendering_handler,
                                                                  system_timeline_time  start_time)
{
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
                    rendering_handler_ptr->n_frames_rendered   = 0;
                    rendering_handler_ptr->playback_start_time = system_time_now() + start_time;

                    system_event_set(rendering_handler_ptr->playback_in_progress_event);
                }
                else
                {
                    /* Must be paused then. */
                    system_event_set(rendering_handler_ptr->playback_in_progress_event);
                }

                rendering_handler_ptr->playback_status = RENDERING_HANDLER_PLAYBACK_STATUS_STARTED;
            }
            system_critical_section_leave(rendering_handler_ptr->rendering_cs);

            result = true;
        }
    }

    return result;
}

/** Please see header for specification. */
PUBLIC EMERALD_API bool ogl_rendering_handler_request_callback_from_context_thread(__in __notnull ogl_rendering_handler                      rendering_handler,
                                                                                   __in __notnull PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc,
                                                                                   __in           void*                                      user_arg,
                                                                                   __in           bool                                       block_until_available)
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

            system_event_set                 (rendering_handler_ptr->callback_request_event);
            system_event_wait_single_infinite(rendering_handler_ptr->callback_request_ack_event);

            system_critical_section_leave(rendering_handler_ptr->callback_request_cs);
        }

        result = should_continue;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_rendering_handler_set_fps_counter_visibility(__in __notnull ogl_rendering_handler rendering_handler,
                                                                         __in           bool                  fps_counter_status)
{
    ((_ogl_rendering_handler*)rendering_handler)->fps_counter_status = fps_counter_status;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_rendering_handler_stop(__in __notnull ogl_rendering_handler rendering_handler)
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

            /* Wait until the playback starts */
            system_event_wait_single_infinite(rendering_handler_ptr->playback_waiting_event);

            result = true;
        } /* if (rendering_handler_ptr->playback_status == RENDERING_HANDLER_PLAYBACK_STATUS_STARTED) */
    } /* if (rendering_handler_ptr->context != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC bool _ogl_rendering_handler_on_bound_to_context(__in __notnull ogl_rendering_handler rendering_handler,
                                                       __in __notnull ogl_context           context)
{
    _ogl_rendering_handler* rendering_handler_ptr = (_ogl_rendering_handler*) rendering_handler;
    bool                    result                = false;

    ASSERT_DEBUG_SYNC(rendering_handler_ptr->context == NULL,
                      "Rendering handler is already bound to a context! A rendering handler cannot be bound to more than one context at a time. ");

    if (rendering_handler_ptr->context == NULL)
    {
        rendering_handler_ptr->context = context;
        result                         = true;

        ogl_context_retain(context);
        system_event_set  (rendering_handler_ptr->context_set_event);
    }

    return result;
}
