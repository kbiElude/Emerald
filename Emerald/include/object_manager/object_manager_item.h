/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * TODO
 *
 */
#ifndef OBJECT_MANAGER_ITEM_H
#define OBJECT_MANAGER_ITEM_H

#include "system/system_types.h"

/** TODO */
PUBLIC object_manager_item object_manager_item_create(__in __notnull system_hashed_ansi_string  name,
                                                      __in __notnull system_hashed_ansi_string  origin_file,
                                                      __in           int                        origin_line,
                                                      __in           object_manager_object_type,
                                                      __in __notnull void*);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string object_manager_item_get_name(__in __notnull object_manager_item);

/** TODO */
PUBLIC EMERALD_API void object_manager_item_get_origin_details(__in  __notnull object_manager_item,
                                                               __out __notnull system_hashed_ansi_string*,
                                                               __out __notnull int*);

/** TODO */
PUBLIC EMERALD_API void* object_manager_item_get_raw_pointer(__in __notnull object_manager_item);

/** TODO */
PUBLIC EMERALD_API object_manager_object_type object_manager_item_get_type(__in __notnull object_manager_item);

/** TODO */
PUBLIC void object_manager_item_release(__in __notnull __post_invalid object_manager_item);

#endif /* OBJECT_MANAGER_ITEM_H */
