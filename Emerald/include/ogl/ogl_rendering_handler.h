/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OGL_RENDERING_HANDLER_H
#define OGL_RENDERING_HANDLER_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_rendering_handler, ogl_rendering_handler)


/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_render_per_request_policy(__in __notnull system_hashed_ansi_string,
                                                                                                     __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK,
                                                                                                     __in           void*);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_fps_policy(__in __notnull system_hashed_ansi_string               name,
                                                                                      __in           uint32_t                                desired_fps,
                                                                                      __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback,
                                                                                      __in           void*                                   user_arg);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler ogl_rendering_handler_create_with_max_performance_policy(__in __notnull system_hashed_ansi_string name,
                                                                                                  __in __notnull PFNOGLRENDERINGHANDLERRENDERINGCALLBACK,
                                                                                                  __in           void*);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler_playback_status ogl_rendering_handler_get_playback_status(__in __notnull ogl_rendering_handler);

/** TODO */
PUBLIC EMERALD_API ogl_rendering_handler_policy ogl_rendering_handler_get_policy(__in __notnull ogl_rendering_handler);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_is_current_thread_rendering_thread(__in __notnull ogl_rendering_handler);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_play(__in __notnull ogl_rendering_handler,
                                                                  system_timeline_time);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_request_callback_from_context_thread(__in __notnull ogl_rendering_handler,
                                                                                   __in __notnull PFNOGLCONTEXTCALLBACKFROMCONTEXTTHREADPROC,
                                                                                   __in            void*);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_set_fps_counter_visibility(__in __notnull ogl_rendering_handler,
                                                                         __in           bool);

/** TODO */
PUBLIC EMERALD_API bool ogl_rendering_handler_stop(__in __notnull ogl_rendering_handler);

/** TODO. Blocks rendering handler - _ogl_rendering_handler_unlock_bound_context() call must follow! */
PUBLIC EMERALD_API void ogl_rendering_handler_lock_bound_context(__in __notnull ogl_rendering_handler);

/** TODO */
PUBLIC bool _ogl_rendering_handler_on_bound_to_context(__in __notnull ogl_rendering_handler,
                                                       __in __notnull ogl_context);

/** TODO */
PUBLIC EMERALD_API void ogl_rendering_handler_unlock_bound_context(__in __notnull ogl_rendering_handler);

#endif /* OGL_RENDERING_HANDLER_H */