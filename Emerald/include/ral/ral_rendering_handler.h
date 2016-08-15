/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef RAL_RENDERING_HANDLER_H
#define RAL_RENDERING_HANDLER_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(ral_rendering_handler,
                             ral_rendering_handler)

typedef void* (*PFNRALRENDERINGHANDLERCREATERABACKENDRENDERINGHANDLERPROC)(ral_rendering_handler                   rendering_handler_ral);
typedef void  (*PFNRALRENDERINGHANDLEREXECUTEPRESENTJOB)                  (void*                                   rendering_handler_raBackend,
                                                                           ral_present_job                         present_job);
typedef void  (*PFNRALRENDERINGHANDLERINITRABACKENDRENDERINGHANDLERPROC)  (ral_context                             context,
                                                                           ral_rendering_handler                   rendering_handler_ral,
                                                                           void*                                   rendering_handler_raBackend);
typedef void  (*PFNRALRENDERINGHANDLERPOSTDRAWFRAMECALLBACKPROC)          (void*                                   rendering_handler_raBackend,
                                                                           bool                                    has_rendered_frame);
typedef void  (*PFNRALRENDERINGHANDLERPREDRAWFRAMECALLBACKPROC)           (void*                                   rendering_handler_raBackend);
typedef void  (*PFNRALRENDERINGHANDLERPRESENTFRAMECALLBACKPROC)           (void*                                   rendering_handler_raBackend,
                                                                           system_critical_section                 rendering_cs);
typedef void  (*PFNRALRENDERINGHANDLERRELEASEBACKENDRENDERINGHANDLERPROC) (void*                                   rendering_handler_raBackend);
typedef bool  (*PFNRALRENDERINGHANDLERREQUESTRENDERINGCALLBACKPROC)       (void*                                   rendering_handler_raBackend,
                                                                           PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                                           void*                                   user_arg,
                                                                           bool                                    present_after_executed,
                                                                           ral_rendering_handler_execution_mode    execution_mode);
typedef void  (*PFNRALRENDERINGHANDLERWAITEVENTCALLBACKPROC)              (uint32_t                                id,
                                                                           void*                                   rendering_handler_raBackend);

typedef struct
{
    system_event                                event;
    uint32_t                                    id;
    PFNRALRENDERINGHANDLERWAITEVENTCALLBACKPROC pfn_callback_proc;
} ral_rendering_handler_custom_wait_event_handler;


typedef void (*PFNRALRENDERINGHANDLERENUMERATECUSTOMWAITEVENTHANDLERSPROC)(void*                                                   rendering_handler_backend,
                                                                           uint32_t*                                               opt_out_n_custom_wait_event_handlers_ptr,
                                                                           const ral_rendering_handler_custom_wait_event_handler** opt_out_custom_wait_event_handlers_ptr);

typedef enum
{
    /* settable, float.
     *
     * The aspect ratio property value will be used if no timeline instance has been
     * assigned to the renering handler instance. */
    RAL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO,

    /* not settable, system_event.
     *
     * System event which remains set while rendering playback is in progress.
     *
     * DO NOT reset or set! This event is exposed only as a mean to efficiently
     * block a thread until the playback starts.
     */
    RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_IN_PROGRESS_EVENT,

    /* not settable, system_event.
     *
     * System event which is signaled whenever rendering playback is NOT in progress.
     *
     * DO NOT reset or set! This event is exposed only as a mean to efficiently
     * block a thread until the playback stops.
     */
    RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STOPPED_EVENT,

    /* not settable, ral_rendering_handler_playback_status */
    RAL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,

    /* not settable, ral_rendering_handler_policy */
    RAL_RENDERING_HANDLER_PROPERTY_POLICY,

    /* settable, PFNRALRENDERINGHANDLERRENDERINGCALLBACK.
     *
     * Rendering call-back to use when play-back is in progress.
     */
    RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,

    /* settable, void*.
     *
     * Rendering call-back user argument.
     */
    RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK_USER_ARGUMENT,

    /* not settable, void*. (backend-specific; for ES / GL, this property holds a raGL_rendering_handler instance) */
    RAL_RENDERING_HANDLER_PROPERTY_RENDERING_HANDLER_BACKEND,

    /* settable, bool.
     *
     * Allows the user to fast forward/fast backward/pause/resume playback with key presses:
     *
     * fast backward:  hold the "left arrow" key pressed.
     * backward:       press the "left arrow" key.
     * fast forward:   hold the "right arrow" key pressed.
     * forward:        press the "right arrow" key.
     * pause / resume: press the "space" key.
     *
     * Debug builds:   enabled by default.
     * Release builds: disabled by default.
     */
    RAL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE,

    /* settable, demo_timeline.
     *
     * The timeline will be used to drive the frame rendering process, once it is assigned to the
     * rendering handler instance. Before that happens, the rendering callback function pointer,
     * provided at construction time, will be used.
     *
     * This property can only be assigned a value once per a rendering handler instance. Any repeated
     * attempts will result in an assertion failure.
     *
     * Setter for this property claims ownership of the assigned object. In other words, do NOT release
     * it when the timeline instance is no longer needed. The rendering handler will do this for you at
     * the right time.
     */
    RAL_RENDERING_HANDLER_PROPERTY_TIMELINE,

} ral_rendering_handler_property;

/** TODO */
PUBLIC bool ral_rendering_handler_bind_to_context(ral_rendering_handler rendering_handler,
                                                  ral_context           context);

/** TODO */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_render_per_request_policy(ral_backend_type                        backend_type,
                                                                                         system_hashed_ansi_string               name,
                                                                                         PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                         void*                                   user_arg);

/** TODO */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_fps_policy(ral_backend_type                        backend_type,
                                                                          system_hashed_ansi_string               name,
                                                                          uint32_t                                desired_fps,
                                                                          PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                          void*                                   user_arg);

/** TODO */
PUBLIC ral_rendering_handler ral_rendering_handler_create_with_max_performance_policy(ral_backend_type                        backend_type,
                                                                                      system_hashed_ansi_string               name,
                                                                                      PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                      void*                                   user_arg);

/** TODO */
PUBLIC void ral_rendering_handler_get_property(ral_rendering_handler          rendering_handler,
                                               ral_rendering_handler_property property,
                                               void*                          out_result);

/** TODO */
PUBLIC bool ral_rendering_handler_is_current_thread_rendering_thread(ral_rendering_handler rendering_handler);

/** TODO */
PUBLIC bool ral_rendering_handler_play(ral_rendering_handler rendering_handler,
                                       system_time           start_time);

/** TODO */
PUBLIC bool ral_rendering_handler_request_rendering_callback(ral_rendering_handler                   rendering_handler,
                                                             PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc,
                                                             void*                                   user_arg,
                                                             bool                                    present_after_executed,
                                                             ral_rendering_handler_execution_mode    execution_mode         = RAL_RENDERING_HANDLER_EXECUTION_MODE_WAIT_UNTIL_IDLE_BLOCK_TILL_FINISHED);

/** TODO */
PUBLIC EMERALD_API void ral_rendering_handler_set_property(ral_rendering_handler          rendering_handler,
                                                           ral_rendering_handler_property property,
                                                           const void*                    value);

/** TODO */
PUBLIC bool ral_rendering_handler_stop(ral_rendering_handler rendering_handler);

#endif /* RAL_RENDERING_HANDLER_H */