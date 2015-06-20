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
PUBLIC void system_window_win32_close_window(__in system_window_win32 window);

/** TODO */
PUBLIC void system_window_win32_deinit(__in system_window_win32 window);

/** TODO */
PUBLIC bool system_window_win32_get_property(__in  system_window_win32    window,
                                             __in  system_window_property property,
                                             __out void*                  out_result);

/** TODO */
PUBLIC system_window_win32 system_window_win32_init(__in system_window owner);

/** TODO */
PUBLIC void system_window_win32_handle_window(__in system_window_win32 window);

/** TODO */
PUBLIC bool system_window_win32_open_window(__in system_window_win32 window,
                                            __in bool                is_first_window);

/** TODO */
PUBLIC bool system_window_win32_set_property(__in system_window_win32    window,
                                             __in system_window_property property,
                                             __in const void*            data);

#endif /* SYSTEM_WINDOW_WIN32_H */