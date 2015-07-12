/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_HASH64MAP_H
#define SYSTEM_HASH64MAP_H

#include "system_types.h"

typedef enum
{
    /* uint32_t */
    SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS
} system_hash64map_property;

/** TODO */
PUBLIC EMERALD_API void system_hash64map_clear(system_hash64map);

/** Creates a new instance of a 64-bit hash-map.
 *
 *  @param size_t                Size of a single element (in bytes).
 *  @param should_be_thread_safe TODO
 *
 *  @return New 64-bit hash-map instance.
 */
PUBLIC EMERALD_API system_hash64map system_hash64map_create(size_t element_size,
                                                            bool   should_be_thread_safe = false);

/** Tells whether the hash-map contains an object identified by user-provided hash.
 *
 *  @param system_hash64map Hash-map to search.
 *  @param system_hash64    Hash to look for.
 *
 *  @return True if the item was found, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_contains(system_hash64map hash_map,
                                                  system_hash64    hash);

/** Retrieves an item identified by user-provided hash from user-provided hash-map.
 *
 *  @param system_hash64map 64-bit hash-map to retrieve the item from.
 *  @param system_hash64    Hash to look for.
 *  @param void*            Item, if found, will be copied to the location described by the pointer.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool	system_hash64map_get(system_hash64map map,
                                             system_hash64    hash,
                                             void*            result_element);

/** Retrieves element at a given index of the 64-bit hash-map.
 *
 *  @param system_hash64map 64-bit hash map to use.
 *  @param size_t           Index of the element to retrieve.
 *  @param void*            Item, if found, will be copied to the location described by the pointer. Can be null.
 *  @param system_hash64*   Hash of the item, if found, will be copied to the location described by the pointer.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_get_element_at(system_hash64map map,
                                                        size_t           n_element,
                                                        void*            result_element,
                                                        system_hash64*   result_hash);

/** TODO */
PUBLIC EMERALD_API void system_hash64map_get_property(system_hash64map          map,
                                                      system_hash64map_property property,
                                                      void*                     out_result);

/** Compares two 64-bit hash-maps.
 *
 *  @param system_hash64map Base 64-bit hash-map.
 *  @param system_hash64map 64-bit hash-map to compare to.
 *
 *  @return true if equal, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_is_equal(system_hash64map map_1,
                                                  system_hash64map map_2);

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
PUBLIC EMERALD_API bool system_hash64map_insert(system_hash64map                              map,
                                                system_hash64                                 hash,
                                                void*                                         element,
                                                PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC        callback,
                                                _system_hash64map_on_remove_callback_argument callback_argument);

/** TODO */
PUBLIC EMERALD_API void system_hash64map_lock(system_hash64map                    map,
                                              system_read_write_mutex_access_type lock_type);

/** Removes an element from the 64-bit hash-map.
 *
 *  @param system_hash64map 64-bit hash-map to operate on.
 *  @param system_hash64    Hash of the item to remove.
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_hash64map_remove(system_hash64map map,
                                                system_hash64    hash);

/** Release a 64-bit hash-map. Do not use the object after calling this function.
 *
 *  @param system_hash64map 64-bit hash-map to release.
 */
PUBLIC EMERALD_API void system_hash64map_release(system_hash64map map);

/** TODO */
PUBLIC EMERALD_API void system_hash64map_unlock(system_hash64map                    map,
                                                system_read_write_mutex_access_type lock_type);

#endif /* SYSTEM_HASH64MAP_H */
