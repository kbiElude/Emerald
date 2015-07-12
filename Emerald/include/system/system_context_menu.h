/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_CONTEXT_MENU_H
#define SYSTEM_CONTEXT_MENU_H

#include "system_types.h"

#ifdef _WIN32

REFCOUNT_INSERT_DECLARATIONS(system_context_menu,
                             system_context_menu)

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_item(system_context_menu                   menu,
                                                        system_hashed_ansi_string             name,
                                                        PFNCONTEXTMENUCALLBACK                callback_func,
                                                        system_context_menu_callback_argument callback_func_argument,
                                                        bool                                  is_checked,
                                                        bool                                  is_enabled);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_menu(system_context_menu menu,
                                                        system_context_menu menu_to_append);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_separator(system_context_menu menu);

/** TODO */
PUBLIC EMERALD_API system_context_menu system_context_menu_create(system_hashed_ansi_string name);

/** TODO */
PUBLIC void system_context_menu_show(system_context_menu  menu,
                                     system_window_handle parent_window_handle,
                                     int                  x,
                                     int                  y);

#else
    /* TODO */
#endif

#endif /* SYSTEM_CONTEXT_MENU_H */
