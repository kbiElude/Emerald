/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Linux-specific implementation of a renderer window. Internal use only.
 */
#ifndef SYSTEM_WINDOW_LINUX_H
#define SYSTEM_WINDOW_LINUX_H

#include "system/system_types.h"
#include "system/system_window.h"

/** TODO */
PUBLIC void system_window_linux_close_window(__in system_window_linux window);

/** TODO */
PUBLIC void system_window_linux_deinit(__in system_window_linux window);

/** TODO */
PUBLIC void system_window_linux_deinit_global();

/** TODO */
PUBLIC bool system_window_linux_get_property(__in  system_window_linux    window,
                                             __in  system_window_property property,
                                             __out void*                  out_result);

/** TODO */
PUBLIC system_window_linux system_window_linux_init(__in system_window owner);

/** TODO */
PUBLIC void system_window_linux_init_global();

/** TODO */
PUBLIC void system_window_linux_handle_window(__in system_window_linux window);

/** TODO */
PUBLIC bool system_window_linux_open_window(__in system_window_linux window,
                                            __in bool                is_first_window);

/** TODO */
PUBLIC bool system_window_linux_set_property(__in system_window_linux    window,
                                             __in system_window_property property,
                                             __in const void*            data);

#endif /* SYSTEM_WINDOW_LINUX_H */