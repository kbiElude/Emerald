/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_BST_H
#define SYSTEM_BST_H

#include "system_types.h"


/** TODO */
PUBLIC EMERALD_API system_bst system_bst_create(size_t                       key_size,
                                                size_t                       value_size,
                                                system_bst_value_lower_func  key_lower_func,
                                                system_bst_value_equals_func key_equals_func,
                                                system_bst_key               initial_key,
                                                system_bst_value             initial_value);

/** TODO */
PUBLIC EMERALD_API bool system_bst_get(system_bst        bst,
                                       system_bst_key    key,
                                       system_bst_value* value);

/** TODO */
PUBLIC EMERALD_API void system_bst_insert(system_bst       bst,
                                          system_bst_key   key,
                                          system_bst_value value);

/** TODO */
PUBLIC EMERALD_API void system_bst_release(system_bst bst);

#endif /* SYSTEM_BST_H */
