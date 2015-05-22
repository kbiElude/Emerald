/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_BST_H
#define SYSTEM_BST_H

#include "dll_exports.h"
#include "system_types.h"


/** TODO */
PUBLIC EMERALD_API system_bst system_bst_create(__in           size_t                       key_size,
                                                __in           size_t                       value_size,
                                                __in __notnull system_bst_value_lower_func  key_lower_func,
                                                __in __notnull system_bst_value_equals_func key_equals_func,
                                                __in __notnull system_bst_key               initial_key,
                                                __in __notnull system_bst_value             initial_value);

/** TODO */
PUBLIC EMERALD_API bool system_bst_get(__in  __notnull   system_bst        bst,
                                       __in  __notnull   system_bst_key    key,
                                       __out __maybenull system_bst_value* value);

/** TODO */
PUBLIC EMERALD_API void system_bst_insert(__in __notnull system_bst       bst,
                                          __in __notnull system_bst_key   key,
                                          __in __notnull system_bst_value value);

/** TODO */
PUBLIC EMERALD_API void system_bst_release(__in __notnull system_bst);

#endif /* SYSTEM_BST_H */
