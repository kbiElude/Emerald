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
    /* not settable, ogl_rendering_handler_playback_status */
    OGL_RENDERING_HANDLER_PROPERTY_PLAYBACK_STATUS,

    /* not settable, ogl_rendering_handler_policy */
    OGL_RENDERING_HANDLER_PROPERTY_POLICY,

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
PUBLIC EMERALD_API bool ogl_rendering_handler_stop(ogl_rendering_handler rendering_handler);

/** TODO */
PUBLIC bool _ogl_rendering_handler_on_bound_to_context(ogl_rendering_handler rendering_handler,
                                                       ogl_context           context);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_unlock_bound_context(ogl_rendering_handler rendering_handler);

#endif /* OGL_RENDERING_HANDLER_H */