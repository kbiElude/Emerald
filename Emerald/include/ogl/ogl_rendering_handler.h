/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_RENDERING_HANDLER_H
#define OGL_RENDERING_HANDLER_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_rendering_handler,
                             ogl_rendering_handler)

typedef enum
{
    /* settable, float.
     *
     * The aspect ratio property value will be used if no timeline instance has been
     * assigned to the renering handler instance. */
    OGL_RENDERING_HANDLER_PROPERTY_ASPECT_RATIO,

    /* not settable, ogl_rendering_handler_playback_status */
    OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,

    /* not settable, ogl_rendering_handler_policy */
    OGL_RENDERING_HANDLER_PROPERTY_POLICY,

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
    OGL_RENDERING_HANDLER_PROPERTY_RUNTIME_TIME_ADJUSTMENT_MODE,

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
    OGL_RENDERING_HANDLER_PROPERTY_TIMELINE,

} ogl_rendering_handler_property;

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string               name,
                                                                                                     PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                     void*                                   user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string               name,
                                                                                      uint32_t                                desired_fps,
                                                                                      PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                      void*                                   user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string               name,
                                                                                                  PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                                  void*                                   user_arg);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_get_property(ogl_rendering_handler          rendering_handler,
                                                           ogl_rendering_handler_property property,
                                                           void*                          out_result);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_is_current_thread_rendering_thread(ogl_rendering_handler rendering_handler);

/** TODO. Blocks rendering handler - _ogl_rendering_handler_unlock_bound_context() call must follow! */
PUBLIC EMERALD_API void ogl_rendering_handler_lock_bound_context(ogl_rendering_handler rendering_handler);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_play(ogl_rendering_handler rendering_handler,
                                                   system_time           start_time);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_request_callback_from_context_thread(ogl_rendering_handler                      rendering_handler,
                                                                                   PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC pfn_callback_proc,
                                                                                   void*                                      user_arg,
                                                                                   bool                                       block_until_available = true);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_set_property(ogl_rendering_handler          rendering_handler,
                                                           ogl_rendering_handler_property property,
                                                           const void*                    value);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_stop(ogl_rendering_handler rendering_handler);

/** TODO */
PUBLIC bool _ogl_rendering_handler_on_bound_to_context(ogl_rendering_handler rendering_handler,
                                                       ogl_context           context);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_unlock_bound_context(ogl_rendering_handler rendering_handler);

#endif /* OGL_RENDERING_HANDLER_H */