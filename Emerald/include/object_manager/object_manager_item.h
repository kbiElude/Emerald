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
PUBLIC object_manager_item object_manager_item_create(system_hashed_ansi_string  name,
                                                      system_hashed_ansi_string  origin_file,
                                                      int                        origin_line,
                                                      object_manager_object_type,
                                                      void*);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string object_manager_item_get_name(object_manager_item);

/** TODO */
PUBLIC EMERALD_API void object_manager_item_get_origin_details(object_manager_item,
                                                               system_hashed_ansi_string*,
                                                               int*);

/** TODO */
PUBLIC EMERALD_API void* object_manager_item_get_raw_pointer(object_manager_item);

/** TODO */
PUBLIC EMERALD_API object_manager_object_type object_manager_item_get_type(object_manager_item);

/** TODO */
PUBLIC void object_manager_item_release(object_manager_item);

#endif /* OBJECT_MANAGER_ITEM_H */
