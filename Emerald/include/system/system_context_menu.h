/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_CONTEXT_MENU_H
#define SYSTEM_CONTEXT_MENU_H

#include "dll_exports.h"
#include "system_types.h"

REFCOUNT_INSERT_DECLARATIONS(system_context_menu, system_context_menu)

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_item(__in __notnull   system_context_menu, 
                                                        __in __notnull   system_hashed_ansi_string,
                                                        __in __notnull   PFNCONTEXTMENUCALLBACK,
                                                        __in __maybenull system_context_menu_callback_argument,
                                                        bool             is_checked,
                                                        bool             is_enabled);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_menu(__in __notnull system_context_menu, __in __notnull system_context_menu);

/** TODO */
PUBLIC EMERALD_API void system_context_menu_append_separator(__in __notnull system_context_menu);

/** TODO */
PUBLIC EMERALD_API system_context_menu system_context_menu_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void system_context_menu_show(__in __notnull system_context_menu, __in system_window_handle, __in int x, __in int y);

#endif /* SYSTEM_CONTEXT_MENU_H */
