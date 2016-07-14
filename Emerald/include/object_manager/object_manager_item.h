/**
 *
 * Emerald (kbi/elude @2012-2016)
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
                                                      object_manager_object_type type,
                                                      void*                      ptr);

/** TODO */
PUBLIC system_hashed_ansi_string object_manager_item_get_name(object_manager_item item);

/** TODO */
PUBLIC void object_manager_item_get_origin_details(object_manager_item        item,
                                                   system_hashed_ansi_string* out_file_ptr,
                                                   int*                       out_file_line_ptr);

/** TODO */
PUBLIC void* object_manager_item_get_raw_pointer(object_manager_item item);

/** TODO */
PUBLIC object_manager_object_type object_manager_item_get_type(object_manager_item item);

/** TODO */
PUBLIC void object_manager_item_release(object_manager_item item);

#endif /* OBJECT_MANAGER_ITEM_H */
