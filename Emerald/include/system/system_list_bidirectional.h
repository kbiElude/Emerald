/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef SYSTEM_LIST_BIDIRECTIONAL_H
#define SYSTEM_LIST_BIDIRECTIONAL_H

#include "system/system_types.h"


/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_append(system_list_bidirectional      list,
                                                         system_list_bidirectional_item item,
                                                         void*                          new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_clear(system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional system_list_bidirectional_create();

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_head_item(system_list_bidirectional list);

/** TODO
 *
 *  NOTE: Slow!
 */
PUBLIC EMERALD_API bool system_list_bidirectional_get_item_at(system_list_bidirectional       list,
                                                              unsigned int                    index,
                                                              system_list_bidirectional_item* out_item);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_get_item_data(system_list_bidirectional_item list_item,
                                                                void**                         out_result);

/** TODO */
PUBLIC EMERALD_API unsigned int system_list_bidirectional_get_number_of_elements(system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_next_item(system_list_bidirectional_item list_item);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_previous_item(system_list_bidirectional_item list_item);

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_tail_item(system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_end(system_list_bidirectional list,
                                                              void*                      new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_front(system_list_bidirectional list,
                                                                void*                      new_item_data);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_release(system_list_bidirectional list);

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_remove_item(system_list_bidirectional      list,
                                                              system_list_bidirectional_item list_item);

#endif /* SYSTEM_LIST_BIDIRECTIONAL_H */