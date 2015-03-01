/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_HASH64MAP_H
#define SYSTEM_HASH64MAP_H

#include "dll_exports.h"
#include "system_types.h"

/** TODO */
PUBLIC EMERALD_API void system_hash64map_clear(__in __notnull system_hash64map);

/** Creates a new instance of a 64-bit hash-map.
 *
 *  @param size_t                Size of a single element (in bytes).
 *  @param should_be_thread_safe TODO
 *
 *  @return New 64-bit hash-map instance.
 */
PUBLIC EMERALD_API system_hash64map system_hash64map_create(__in size_t element_size,
                                                            __in bool   should_be_thread_safe = false);

/** Tells whether the hash-map contains an object identified by user-provided hash.
 *
 *  @param system_hash64map Hash-map to search.
 *  @param system_hash64    Hash to look for.
 *
 *  @return True if the item was found, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_contains(__in system_hash64map,
                                                  __in system_hash64);

/** Retrieves an item identified by user-provided hash from user-provided hash-map.
 *
 *  @param system_hash64map 64-bit hash-map to retrieve the item from.
 *  @param system_hash64    Hash to look for.
 *  @param void*            Item, if found, will be copied to the location described by the pointer.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool	system_hash64map_get(__in      system_hash64map,
                                             __in      system_hash64,
                                             __out_opt void*);

/** Retrieves amount of elements in the 64-bit hash-map.
 *
 *  @param system_hash64map 64-bit hash-map to retrieve the result for.
 *
 *  @return Amount of elements in the hash-map.
 */
PUBLIC EMERALD_API size_t system_hash64map_get_amount_of_elements(__in system_hash64map);

/** Retrieves element at a given index of the 64-bit hash-map.
 *
 *  @param system_hash64map 64-bit hash map to use.
 *  @param size_t           Index of the element to retrieve.
 *  @param void*            Item, if found, will be copied to the location described by the pointer. Can be null.
 *  @param system_hash64*   Hash of the item, if found, will be copied to the location described by the pointer.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_get_element_at(__in                  system_hash64map,
                                                                              size_t,
                                                        __out_opt __maybenull void*,
                                                        __out_opt __maybenull system_hash64* _pOutHash);

/** Compares two 64-bit hash-maps.
 *
 *  @param system_hash64map Base 64-bit hash-map.
 *  @param system_hash64map 64-bit hash-map to compare to.
 *
 *  @return true if equal, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_is_equal(__in system_hash64map,
                                                  __in system_hash64map);

/** Inserts a new element into a 64-bit hash-map.
 *
 *  @param system_hash64map                              64-bit hash-map to operate on.
 *  @param system_hash64                                 Hash the item should use.
 *  @param void*                                         Element to insert. Note that this value will be copied by value to internal storage of the hash-map.
 *  @param PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC        Function pointer to be used if the item is ever released. Can be null.
 *  @param _system_hash64map_on_remove_callback_argument Argument to be passed with on-removal callback. Can be null
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_insert(__in                 system_hash64map,
                                                __in                 system_hash64,
                                                __in_opt             void*,
                                                __in     __maybenull PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC,
                                                __in     __maybenull _system_hash64map_on_remove_callback_argument);

/** TODO */
PUBLIC EMERALD_API void system_hash64map_lock(__in system_hash64map                    map,
                                              __in system_read_write_mutex_access_type lock_type);

/** Removes an element from the 64-bit hash-map.
 *
 *  @param system_hash64map 64-bit hash-map to operate on.
 *  @param system_hash64    Hash of the item to remove.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_remove(__in system_hash64map,
                                                __in system_hash64);

/** Release a 64-bit hash-map. Do not use the object after calling this function.
 *
 *  @param system_hash64map 64-bit hash-map to release.
 */
PUBLIC EMERALD_API void system_hash64map_release(__in __deallocate(mem) system_hash64map);

/** TODO */
PUBLIC EMERALD_API void system_hash64map_unlock(__in system_hash64map                    map,
                                                __in system_read_write_mutex_access_type lock_type);

#endif /* SYSTEM_HASH64MAP_H */
