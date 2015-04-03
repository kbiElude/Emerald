/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_LIST_BIDIRECTIONAL_H
#define SYSTEM_LIST_BIDIRECTIONAL_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_append(__in __notnull system_list_bidirectional      list,
                                                         __in __notnull system_list_bidirectional_item item,
                                                         __in_opt       void*                           new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_clear(__in __notnull system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional system_list_bidirectional_create();

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_head_item(__in __notnull system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_get_item_data(__in  __notnull system_list_bidirectional_item list_item,
                                                                __out __notnull void**                         out_result);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_next_item(__in __notnull system_list_bidirectional_item list_item);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_end(__in __notnull system_list_bidirectional list,
                                                               __in_opt       void*                      new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_front(__in __notnull system_list_bidirectional list,
                                                                 __in_opt       void*                      new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_release(__in __notnull system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_remove_item(__in __notnull system_list_bidirectional      list,
                                                              __in __notnull system_list_bidirectional_item list_item);

#endif /* SYSTEM_LIST_BIDIRECTIONAL_H */