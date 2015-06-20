/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_WINDOW_H
#define SYSTEM_WINDOW_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"

typedef enum
{
    /* Used by UI for event capturing. Do not use unless. */
    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_SYSTEM,

    /* Application levels follow */
    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_LOW
} system_window_callback_func_priority;

typedef enum
{
    /* not settable, int[2].
     *
     * Returned coordinates are in screen space. */
    SYSTEM_WINDOW_PROPERTY_CURSOR_POSITION,

#ifdef _WIN32
    /* not settable, system_window_dc */
    SYSTEM_WINDOW_PROPERTY_DC,
#endif

    /* settable, system_window_mouse_cursor */
    SYSTEM_WINDOW_PROPERTY_CURSOR,

    /* not settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_DIMENSIONS,

    /* not settable, system_window_handle */
    SYSTEM_WINDOW_PROPERTY_HANDLE,

    /* not settable, uint16_t */
    SYSTEM_WINDOW_PROPERTY_FULLSCREEN_BPP,

    /* not settable, uint16_t */
    SYSTEM_WINDOW_PROPERTY_FULLSCREEN_REFRESH_RATE,

    /* not settable, bool.
     *
     * Only set after window is closed (either by the system, or by the user) */
    SYSTEM_WINDOW_PROPERTY_IS_CLOSED,

    /* settable, bool.
     *
     * The setter should ONLY be used internally!
     */
    SYSTEM_WINDOW_PROPERTY_IS_CLOSING,

    /* not settable, bool */
    SYSTEM_WINDOW_PROPERTY_IS_FULLSCREEN,

    /* not settable, bool */
    SYSTEM_WINDOW_PROPERTY_IS_SCALABLE,

    /* not settable, bool */
    SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,

    /* not settable, system_hashed_ansi_string */
    SYSTEM_WINDOW_PROPERTY_NAME,

    /* not settable, system_window_handle */
    SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,

    /* not settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_POSITION,

    /* not settable, ogl_context */
    SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,

    /* not settable, ogl_rendering_handler */
    SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,

    /* not settable, system_hashed ansi string */
    SYSTEM_WINDOW_PROPERTY_TITLE,

    /* not settable, int[4] */
    SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2
} system_window_property;


/** TODO */
PUBLIC EMERALD_API bool system_window_add_callback_func(__in __notnull system_window                        window,
                                                        __in           system_window_callback_func_priority priority,
                                                        __in           system_window_callback_func          callback_func,
                                                        __in __notnull void*                                pfn_callback_func,
                                                        __in __notnull void*                                callback_func_user_arg);

/** TODO. Releases rendering handler! */
PUBLIC EMERALD_API bool system_window_close(__in __notnull __deallocate(mem) system_window window);

/** TODO */
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(__in system_hashed_ansi_string name,
                                                                          __in ogl_context_type          context_type,
                                                                          __in uint16_t                  n_multisampling_samples,
                                                                          __in bool                      vsync_enabled,
                                                                          __in system_window_handle      parent_window_handle,
                                                                          __in bool                      multisampling_supported);

/** TODO */
PUBLIC EMERALD_API system_window system_window_create_not_fullscreen(__in                       ogl_context_type          context_type,
                                                                     __in __notnull __ecount(4) const int*                x1y1x2y2,
                                                                     __in __notnull             system_hashed_ansi_string title,
                                                                     __in                       bool                      scalable,
                                                                     __in                       uint16_t                  n_multisampling_samples,
                                                                     __in                       bool                      vsync_enabled,
                                                                     __in                       bool                      multisampling_supported,
                                                                     __in                       bool                      visible);

/** TODO */
PUBLIC EMERALD_API system_window system_window_create_fullscreen(__in ogl_context_type context_type,
                                                                 __in uint16_t         width,
                                                                 __in uint16_t         height,
                                                                 __in uint16_t         bpp,
                                                                 __in uint16_t         freq,
                                                                 __in uint16_t         n_multisampling_samples,
                                                                 __in bool             vsync_enabled,
                                                                 __in bool             multisampling_supported);

/** TODO */
PUBLIC EMERALD_API bool system_window_delete_callback_func(__in __notnull system_window               window,
                                                           __in           system_window_callback_func callback_func,
                                                           __in __notnull void*                       pfn_callback_func,
                                                           __in __notnull void*                       callback_func_user_arg);

/** TODO
 *
 *  Note: Internal use only.
 */
PUBLIC void system_window_execute_callback_funcs(__in __notnull system_window               window,
                                                 __in           system_window_callback_func func,
                                                 __in_opt       void*                       arg_wparam = NULL,  /* TODO: remove in next commit! */
                                                 __in_opt       void*                       arg_lparam = NULL); /* TODO: remove in next commit! */

/** Retrieves coordinates that can be passed to system_window_create_not_fullscreen() in order
 *  to position the window in the center of primary monitor.
 *
 *  @param dimensions        Horizontal and vertical size for the window to be centered.
 *  @param result_dimensions Deref will be used to store centered coordinates, if there's sufficient space available on the monitor.
 *
 *  @return true if the coordinates have been stored in deref of @param result_dimensions, false otherwise
 **/
PUBLIC EMERALD_API bool system_window_get_centered_window_position_for_primary_monitor(__in_ecount(2)  __notnull const int* dimensions,
                                                                                       __out_ecount(4) __notnull int*       result_dimensions);

/** TODO */
PUBLIC EMERALD_API void system_window_get_property(__in  __notnull system_window          window,
                                                   __in            system_window_property property,
                                                   __out __notnull void*                  out_result);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_cursor_visibility(__in __notnull system_window window,
                                                                           bool          visibility);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_cursor(__in __notnull system_window              window,
                                                                system_window_mouse_cursor cursor);

/** TODO.
 *
 *  Internal use only
 */
PUBLIC bool system_window_set_property(__in system_window          window,
                                       __in system_window_property property,
                                       __in const void*            data);

/** Binds a rendering handler to the window.
 *
 *  You can only bind one rendering handler to a window. Once you do so, it is (currently)
 *  impossible to unbind it, so please use the function with proper care.
 *
 *  The function retains the rendering handler instance.
 *
 *  @param system_window         Window to bind the rendering handler to.
 *  @param ogl_rendering_handler Rendering handler to bind to the window.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_window_set_rendering_handler(__in __notnull system_window         window,
                                                            __in __notnull ogl_rendering_handler rendering_handler);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_position(__in __notnull system_window window,
                                                   __in __notnull int           x,
                                                   __in __notnull int           y);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_size(__in __notnull system_window window,
                                               __in           int           width,
                                               __in           int           height);

/** Blocks the caller until the specified window becomes closed.
 *
 *  NOTE: Internal use only.
 *
 *  @param window Window instance to use for the operation. Must not be NULL.
 */
PUBLIC void system_window_wait_until_closed(__in __notnull system_window window);


/** TODO */
PUBLIC void _system_window_init();

/** TODO */
PUBLIC void _system_window_deinit();

#endif /* SYSTEM_WINDOW_H */