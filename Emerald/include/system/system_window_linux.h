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

#include <X11/cursorfont.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>


/** TODO */
PUBLIC void system_window_linux_close_window(system_window_linux window);

/** TODO */
PUBLIC void system_window_linux_deinit(system_window_linux window);

/** TODO */
PUBLIC void system_window_linux_deinit_global();

/** TODO */
PUBLIC bool system_window_linux_get_property(system_window_linux    window,
                                             system_window_property property,
                                             void*                  out_result);

/** TODO */
PUBLIC void system_window_linux_get_screen_size(int* out_screen_width_ptr,
                                                int* out_screen_height_ptr);

/** TODO */
PUBLIC system_window_linux system_window_linux_init(system_window owner);

/** TODO */
PUBLIC void system_window_linux_init_global();

/** TODO */
PUBLIC void system_window_linux_handle_window(system_window_linux window);

/** TODO */
PUBLIC bool system_window_linux_open_window(system_window_linux window,
                                            bool                is_first_window);

/** TODO */
PUBLIC bool system_window_linux_set_property(system_window_linux    window,
                                             system_window_property property,
                                             const void*            data);

#endif /* SYSTEM_WINDOW_LINUX_H */