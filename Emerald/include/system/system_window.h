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
    /* settable, audio_device
     *
     * Can only be set once. Further set attempts will throw an assertion failure.
     * Can only be set when the rendering handler is not currently playing.
     */
    SYSTEM_WINDOW_PROPERTY_AUDIO_DEVICE,

    /* settable, audio_stream
     *
     * Can only be set once. Further set attempts will throw an assertion failure. (TODO: remove this limitation?)
     * Can only be set when the rendering handler is not currently playing.
     */
    SYSTEM_WINDOW_PROPERTY_AUDIO_STREAM,

    /* not settable, int[2].
     *
     * Returned coordinates are in screen space. */
    SYSTEM_WINDOW_PROPERTY_CURSOR_POSITION,

#ifdef _WIN32
    /* not settable, system_window_dc */
    SYSTEM_WINDOW_PROPERTY_DC,
#endif

    /* not settable, ogl_context_type */
    SYSTEM_WINDOW_PROPERTY_CONTEXT_TYPE,

    /* settable, system_window_mouse_cursor */
    SYSTEM_WINDOW_PROPERTY_CURSOR,

    /* settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_DIMENSIONS,

#ifdef __linux
    /* not settable, Display*.
     *
     * The returned display is window-specific.
     */
    SYSTEM_WINDOW_PROPERTY_DISPLAY,
#endif
    /* not settable, system_window_handle */
    SYSTEM_WINDOW_PROPERTY_HANDLE,

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
    SYSTEM_WINDOW_PROPERTY_IS_ROOT_WINDOW,

    /* not settable, bool */
    SYSTEM_WINDOW_PROPERTY_IS_SCALABLE,

    /* not settable, bool */
    SYSTEM_WINDOW_PROPERTY_IS_VISIBLE,

    /* not settable, system_hashed_ansi_string */
    SYSTEM_WINDOW_PROPERTY_NAME,

    /* not settable, system_window_handle */
    SYSTEM_WINDOW_PROPERTY_PARENT_WINDOW_HANDLE,

    /* not settable, system_pixel_format */
    SYSTEM_WINDOW_PROPERTY_PIXEL_FORMAT,

    /* settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_POSITION,

    /* not settable, ogl_context */
    SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,

    /* settable, ogl_rendering_handler
     *
     * You can only bind one rendering handler to a window. Once you do so, it is (currently)
     * impossible to unbind it, so please use the function with proper care.
     *
     * The specified rendering handler instance is retained.
     */
    SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,

#ifdef __linux
    /* not settable, int */
    SYSTEM_WINDOW_PROPERTY_SCREEN_INDEX,
#endif

    /* not settable, system_screen_mode */
    SYSTEM_WINDOW_PROPERTY_SCREEN_MODE,

    /* not settable, system_hashed ansi string */
    SYSTEM_WINDOW_PROPERTY_TITLE,

    /* not settable, int[4] */
    SYSTEM_WINDOW_PROPERTY_X1Y1X2Y2
} system_window_property;


/** TODO */
PUBLIC EMERALD_API bool system_window_add_callback_func(system_window                        window,
                                                        system_window_callback_func_priority priority,
                                                        system_window_callback_func          callback_func,
                                                        void*                                pfn_callback_func,
                                                        void*                                callback_func_user_arg);

/** TODO. Releases rendering handler, as well as the window instance! */
PUBLIC EMERALD_API bool system_window_close(system_window window);

/** TODO
 *
 *  @param name                 TODO.
 *  @param contxt_type          TODO.
 *  @param vsync_enabled        TODO.
 *  @param parent_window_handle TODO.
 *  @param pixel_format         TODO. system_window takes over the ownership of the object.
 *
 *  @return TODO
 */
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(system_hashed_ansi_string name,
                                                                          ogl_context_type          context_type,
                                                                          bool                      vsync_enabled,
                                                                          system_window_handle      parent_window_handle,
                                                                          system_pixel_format       pixel_format);

/** TODO
 *
 *  @param context_type  TODO
 *  @param x1y1x2y2      TODO
 *  @param title         TODO
 *  @param scalable      TODO
 *  @param vsync_enabled TODO
 *  @param visible       TODO
 *  @param pixel_format  TODO. system_window takes over the ownership of the object.
 *
 *  @return TODO
 */
PUBLIC EMERALD_API system_window system_window_create_not_fullscreen(ogl_context_type          context_type,
                                                                     const int*                x1y1x2y2,
                                                                     system_hashed_ansi_string title,
                                                                     bool                      scalable,
                                                                     bool                      vsync_enabled,
                                                                     bool                      visible,
                                                                     system_pixel_format       pixel_format);


/** TODO
 *
 *  @param context_type  TODO
 *  @param mode          TODO
 *  @param vsync_enabled TODO
 *  @param pixel_format  TODO. system_window takes over the ownership of the object.
 *
 *  @return TODO
 */
PUBLIC EMERALD_API system_window system_window_create_fullscreen(ogl_context_type    context_type,
                                                                 system_screen_mode  mode,
                                                                 bool                vsync_enabled,
                                                                 system_pixel_format pixel_format);

/** TODO */
PUBLIC EMERALD_API bool system_window_delete_callback_func(system_window               window,
                                                           system_window_callback_func callback_func,
                                                           void*                       pfn_callback_func,
                                                           void*                       callback_func_user_arg);

/** TODO
 *
 *  Note: Meaning of @param arg1 and @param arg2 is callback-specific. See
 *        call-back enum documentation for more details.
 *  Note: Internal use only.
 */
PUBLIC void system_window_execute_callback_funcs(system_window               window,
                                                 system_window_callback_func func,
                                                 void*                       arg1 = NULL,
                                                 void*                       arg2 = NULL);

/** Retrieves coordinates that can be passed to system_window_create_not_fullscreen() in order
 *  to position the window in the center of primary monitor.
 *
 *  @param dimensions        Horizontal and vertical size for the window to be centered.
 *  @param result_dimensions Deref will be used to store centered coordinates, if there's sufficient space available on the monitor.
 *
 *  @return true if the coordinates have been stored in deref of @param result_dimensions, false otherwise
 **/
PUBLIC EMERALD_API bool system_window_get_centered_window_position_for_primary_monitor(const int* dimensions,
                                                                                       int*       result_dimensions);

/** TODO */
PUBLIC EMERALD_API void system_window_get_property(system_window          window,
                                                   system_window_property property,
                                                   void*                  out_result);

/** TODO
 *
 *  NOTE: Internal use only.
 *
 *  @return TODO
 */
PUBLIC system_window system_window_get_root_window(ogl_context_type context_type = OGL_CONTEXT_TYPE_GL);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_cursor_visibility(system_window window,
                                                            bool          visibility);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_cursor(system_window              window,
                                                 system_window_mouse_cursor cursor);

/** TODO.
 *
 *  NOTE: Some property setters are for internal use only
 */
PUBLIC EMERALD_API bool system_window_set_property(system_window          window,
                                                   system_window_property property,
                                                   const void*            data);

/** Blocks the caller until the specified window becomes closed.
 *
 *  NOTE: Internal use only.
 *
 *  @param window Window instance to use for the operation. Must not be NULL.
 */
PUBLIC void system_window_wait_until_closed(system_window window);


/** TODO */
PUBLIC void _system_window_init();

/** TODO */
PUBLIC void _system_window_deinit();

#endif /* SYSTEM_WINDOW_H */