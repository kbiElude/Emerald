/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_CONTEXT_MENU_H
#define SYSTEM_CONTEXT_MENU_H

#include "dll_exports.h"
#include "system_types.h"

#ifdef _WIN32

REFCOUNT_INSERT_DECLARATIONS(system_context_menu,
                             system_context_menu)

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_item(__in __notnull   system_context_menu                   menu,
                                                        __in __notnull   system_hashed_ansi_string             name,
                                                        __in __notnull   PFNCONTEXTMENUCALLBACK                callback_func,
                                                        __in __maybenull system_context_menu_callback_argument callback_func_argument,
                                                                         bool                                  is_checked,
                                                                         bool                                  is_enabled);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_menu(__in __notnull system_context_menu menu,
                                                        __in __notnull system_context_menu menu_to_append);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_separator(__in __notnull system_context_menu menu);

/** TODO */
PUBLIC EMERALD_API system_context_menu system_context_menu_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void system_context_menu_show(__in __notnull system_context_menu  menu,
                                     __in           system_window_handle parent_window_handle,
                                     __in           int                  x,
                                     __in           int                  y);

#else
    /* TODO */
#endif

#endif /* SYSTEM_CONTEXT_MENU_H */
