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
    SYSTEM_WINDOW_PROPERTY_DC,                /* not settable, system_window_dc */
    SYSTEM_WINDOW_PROPERTY_DIMENSIONS,        /* not settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_HANDLE,            /* not settable, system_window_handle */
    SYSTEM_WINDOW_PROPERTY_NAME,              /* not settable, system_hashed_ansi_string */
    SYSTEM_WINDOW_PROPERTY_POSITION,          /* not settable, int[2] */
    SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT, /* not settable, ogl_context */
    SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER, /* not settable, ogl_rendering_handler */
} system_window_property;


/** TODO */
PUBLIC EMERALD_API bool system_window_add_callback_func(__in __notnull system_window,
                                                        __in           system_window_callback_func_priority,
                                                        __in           system_window_callback_func,
                                                        __in __notnull void*,
                                                        __in __notnull void* /* user arg */);

/** TODO. Releases rendering handler! */
PUBLIC EMERALD_API bool system_window_close(__in __notnull __deallocate(mem) system_window);

/** TODO */
PUBLIC EMERALD_API system_window system_window_create_by_replacing_window(__in system_hashed_ansi_string /*name */,
                                                                          __in ogl_context_type          context_type,
                                                                          __in uint16_t                  /* n_multisampling_samples */,
                                                                          __in bool                      /* vsync_enabled */,
                                                                          __in system_window_handle      /* parent_window_handle */,
                                                                          __in bool                      /* multisampling_supported */);

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
                                                                 __in uint16_t         /* width */,
                                                                 __in uint16_t         /* height */,
                                                                 __in uint16_t         /* bpp */,
                                                                 __in uint16_t         /* freq */,
                                                                 __in uint16_t         /* n_multisampling_samples */, 
                                                                 __in bool             /* vsync_enabled */,
                                                                 __in bool             /* multisampling_supported */);

/** TODO */
PUBLIC EMERALD_API bool system_window_delete_callback_func(__in __notnull system_window,
                                                           __in           system_window_callback_func,
                                                           __in __notnull void*,
                                                           __in __notnull void* /* user arg */);

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
PUBLIC EMERALD_API bool system_window_set_cursor_visibility(__in __notnull system_window,
                                                                           bool);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_cursor(__in __notnull system_window,
                                                                system_window_mouse_cursor);

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
PUBLIC EMERALD_API bool system_window_set_rendering_handler(__in __notnull system_window,
                                                            __in __notnull ogl_rendering_handler);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_position(__in __notnull system_window,
                                                   __in __notnull int /* x */,
                                                   __in __notnull int /* y */);

/** TODO */
PUBLIC EMERALD_API bool system_window_set_size(__in __notnull system_window,
                                               __in           int /* width */,
                                               __in           int /* height */);
/** TODO */
PUBLIC void _system_window_init();

/** TODO */
PUBLIC void _system_window_deinit();

#endif /* SYSTEM_WINDOW_H */