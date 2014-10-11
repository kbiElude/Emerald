/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_LOOKASIDE_LIST_H
#define SYSTEM_LOOKASIDE_LIST_H

#include "dll_exports.h"
#include "system_types.h"

/** Creates a new lookaside list object. Items are copied by value. Each item has the same size
 *  as all other entries in a single lookaside list. 
 *  
 *  When look-aside list runs out of items, it allocates user-defined amount of items for further
 *  usage (so that heap is not over-abused)
 *
 *  @param size_t Item size.
 *  @param size_t Amount of entries to allocate when the look-aside list runs out of items.
 *
 *  @return Look-aside list instance.
 */
PUBLIC EMERALD_API system_lookaside_list system_lookaside_list_create(__in size_t, __in size_t);

/** Releases a look-aside list object. The object should not be accessed after calling this method.
 *
 *  @param system_lookaside_list Look-aside list object to release.
 */
PUBLIC EMERALD_API void system_lookaside_list_release(__in __deallocate(mem) system_lookaside_list);

/** Retrieves amount of items available from the look-aside list.
 *
 *  @param system_lookaside_list Look-aside list to perform the operation on. Cannot be NULL.
 *
 *  @return Amount of items taken.
 */
PUBLIC EMERALD_API uint32_t system_lookaside_list_get_amount_of_available_items(__in system_lookaside_list);

/** Retrieves an item from the look-aside list. 
 *
 *  Caller is not granted ownership of the item - the list will release the item IF the list is ever freed.
 *  Caller is expected to release the item back to the look-aside list when no longer needed, using system_lookaside_list_release_item().
 *
 *  @param system_lookaside_list Look-aisde list to retrieve the item from.
 *
 *  @return New item.
 */
PUBLIC EMERALD_API system_lookaside_list_item system_lookaside_list_get_item(__in system_lookaside_list);

/** Returns the item back to the look-aside list's pool.
 *
 *  @param system_lookaside_list      Look-aside list to store the item in.
 *  @param system_lookaside_list_item Item to move back to the look-aside list's pool.
 */
PUBLIC EMERALD_API void system_lookaside_list_release_item(__in system_lookaside_list, __in __deallocate(mem) system_lookaside_list_item);

#endif /* SYSTEM_LOOKASIDE_LIST_H */