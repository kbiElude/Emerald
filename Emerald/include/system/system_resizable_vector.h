/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Vector container implementation. Can only hold pointer-sized items.
 */
#ifndef SYSTEM_RESIZABLE_VECTOR_H
#define SYSTEM_RESIZABLE_VECTOR_H

#include "system_types.h"

#define ITEM_NOT_FOUND (0xFFFFFFFF)


typedef enum
{
    /* Result pointer may become invalid after any call that modifies the resizable vector
     * so use with caution.
     *
     * Ownership is not transferred to caller.
     *
     */
    SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,

    /* unsigned int */
    SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS
} system_resizable_vector_property;

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_clear(system_resizable_vector vector);

/** Creates a new instance of a resizable vector object.
 *
 *  @param size_t                Base capacity of the object. After creation, no memory allocations will be performed, provided
 *                               the capacity is not exceeded.
 *  @param size_t                Size of a single element.
 *  @param should_be_thread_safe TODO
 *
 *  @return Resizable vector instance.
 */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create(size_t capacity,
                                                                          bool   should_be_thread_safe = false);

/** TODO */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create_copy(system_resizable_vector vector);

/** Removes n-th element from the resizable vector object.
 *
 *  @param system_resizable_vector Resizable vector to perform the operation on.
 *  @param size_t                  Index of the element
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_delete_element_at(system_resizable_vector resizable_vector,
                                                                  size_t                  index);

/** Empties a given resizable vector object.
 *
 *  @param system_resizable_vector Resizable vector to empty. CANNOT be NULL.
 */
PUBLIC EMERALD_API void system_resizable_vector_empty(system_resizable_vector vector);

/** Finds a given element in the resizable vector object.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Object to find.
 *
 *  @return ITEM_NOT_FOUND if unsuccessful, index otherwise.
 */
PUBLIC EMERALD_API size_t system_resizable_vector_find(system_resizable_vector resizable_vector,
                                                       const void*             item);

/** Returns element stored at given index.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param size_t                  Index of object to retrieve.
 *  @param void*                   Deref will be filled with the value.
 *
 *  @return True if result was stored under @param void*, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_get_element_at(system_resizable_vector resizable_vector,
                                                               size_t                  index,
                                                               void*                   result);

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_get_property(system_resizable_vector          resizable_vector,
                                                             system_resizable_vector_property property,
                                                             void*                            out_result_ptr);

/** Inserts new element at given index of the resizable vector instance. This operation does not support cases where
 *  object is to be inserted at index that exceeds vector's capacity. Such an attempt will result in an assertion 
 *  failure.
 *
 *  NOTE: Slower than system_resizable_vector_push(). Pushing is preferred.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param size_t                  Index to insert the element at.
 *  @param void*                   Object to insert.
 */
PUBLIC EMERALD_API void system_resizable_vector_insert_element_at(system_resizable_vector resizable_vector,
                                                                  size_t                  index,
                                                                  void*                   element);

/** Resizable vector comparator.
 *
 *  @param system_resizable_vector Object to compare.
 *  @param system_resizable_vector Object to compare with.
 *
 *  @return True if identical, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_is_equal(system_resizable_vector resizable_vector_1,
                                                         system_resizable_vector resizable_vector_2);

/** TODO.
 *
 *  @param vector      Vector to use for the locking process. NOTE: The vector must have
 *                     been created for thread-safe access, otherwise an assertion failure
 *                     will occur.
 *  @param access_type Access type to be used for the lock.
 */
PUBLIC EMERALD_API void system_resizable_vector_lock(system_resizable_vector             vector,
                                                     system_read_write_mutex_access_type access_type);

/** Retrieves last object stored for a given resizable vector and removes it from the vector.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Deref will be used to store the value
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_pop(system_resizable_vector resizable_vector,
                                                    void*                   result);

/** Stores an element (having copied it by value) at next available cell of the resizable vector instance.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Object to store.
 */
PUBLIC EMERALD_API void system_resizable_vector_push(system_resizable_vector resizable_vector,
                                                     void*                   element);

/** Releases a system resizable vector instance. Object should not be accessed after this call.
 *
 *  @param system_resizable_vector Resizable vector to release.
 */
PUBLIC EMERALD_API void system_resizable_vector_release(system_resizable_vector vector);

/** Stores an element (having copied it by value) in a cell located at user-provided index of the resizable
 *  vector object.
 *
 *  NOTE: An assertion failure will occur if index >= vector's capacity.
 * 
 *  @param system_resizable_vector Resizable vector to operate on.
 *  @param size_t                  Index to place the item at.
 *  @param void*                   Object to store.
 */
PUBLIC EMERALD_API void system_resizable_vector_set_element_at(system_resizable_vector resizable_vector,
                                                               size_t                  index,
                                                               void*                   element);

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_sort(system_resizable_vector resizable_vector,
                                                     bool                  (*comparator_func_ptr)(const void*, const void*) );

/** TODO */
PUBLIC EMERALD_API bool system_resizable_vector_swap(system_resizable_vector resizable_vector,
                                                     uint32_t                index_a,
                                                     uint32_t                index_b);

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_unlock(system_resizable_vector             vector,
                                                       system_read_write_mutex_access_type access_type);

#endif /* SYSTEM_RESIZABLE_VECTOR_H */