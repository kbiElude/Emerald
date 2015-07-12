/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Windows-specific implementation of a renderer window. Internal use only.
 */
#ifndef SYSTEM_WINDOW_WIN32_H
#define SYSTEM_WINDOW_WIN32_H

#include "system/system_types.h"
#include "system/system_window.h"

/** TODO */
PUBLIC void system_window_win32_close_window(system_window_win32 window);

/** TODO */
PUBLIC void system_window_win32_deinit(system_window_win32 window);

/** TODO */
PUBLIC bool system_window_win32_get_property(system_window_win32    window,
                                             system_window_property property,
                                             void*                  out_result);

/** TODO */
PUBLIC void system_window_win32_get_screen_size(int* out_screen_width_ptr,
                                                int* out_screen_height_ptr);

/** TODO */
PUBLIC system_window_win32 system_window_win32_init(system_window owner);

/** TODO */
PUBLIC void system_window_win32_handle_window(system_window_win32 window);

/** TODO */
PUBLIC bool system_window_win32_open_window(system_window_win32 window,
                                            bool                is_first_window);

/** TODO */
PUBLIC bool system_window_win32_set_property(system_window_win32    window,
                                             system_window_property property,
                                             const void*            data);

#endif /* SYSTEM_WINDOW_WIN32_H */