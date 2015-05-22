/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_RESIZABLE_VECTOR_H
#define SYSTEM_RESIZABLE_VECTOR_H

#include "dll_exports.h"
#include "system_types.h"

#define ITEM_NOT_FOUND (0xFFFFFFFF)


/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_clear(__in __notnull system_resizable_vector vector);

/** Creates a new instance of a resizable vector object.
 *
 *  @param size_t                Base capacity of the object. After creation, no memory allocations will be performed, provided
 *                               the capacity is not exceeded.
 *  @param size_t                Size of a single element.
 *  @param should_be_thread_safe TODO
 *
 *  @return Resizable vector instance.
 */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create(__in size_t capacity,
                                                                          __in size_t element_size,
                                                                          __in bool   should_be_thread_safe = false);

/** TODO */
PUBLIC EMERALD_API system_resizable_vector system_resizable_vector_create_copy(__in __notnull system_resizable_vector);

/** Empties a given resizable vector object.
 *
 *  @param system_resizable_vector Resizable vector to empty. CANNOT be NULL.
 */
PUBLIC EMERALD_API void system_resizable_vector_empty(__in system_resizable_vector);

/** Removes n-th element from the resizable vector object.
 *
 *  @param system_resizable_vector Resizable vector to perform the operation on.
 *  @param size_t                  Index of the element
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_delete_element_at(__in system_resizable_vector,
                                                                  __in size_t);

/** Finds a given element in the resizable vector object.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Object to find.
 *
 *  @return ITEM_NOT_FOUND if unsuccessful, index otherwise.
 */
PUBLIC EMERALD_API size_t system_resizable_vector_find(__in system_resizable_vector,
                                                       __in void*);

/** Returns amount of elements stored in the vector.
 *
 *  @param system_resizable_vector Object to operate on.
 *
 *  @return Result.
 */
PUBLIC EMERALD_API unsigned int system_resizable_vector_get_amount_of_elements(__in system_resizable_vector);

/** TODO. Result pointer may become invalid after any call that modifies the resizable vector
 *  so use with caution.
 *
 *  Ownership is not transferred to caller.
 *
 */
PUBLIC EMERALD_API const void* system_resizable_vector_get_array(__in __notnull system_resizable_vector);

/** Returns capacity of a resizable vector object.
 *
 *  @param system_resizable_vector Object to operate on.
 *
 *  @return Result.
 */
PUBLIC EMERALD_API unsigned int system_resizable_vector_get_capacity(__in system_resizable_vector);

/** Returns element stored at given index.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param size_t                  Index of object to retrieve.
 *  @param void*                   Deref will be filled with the value.
 *
 *  @return True if result was stored under @param void*, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_get_element_at(__in            system_resizable_vector,
                                                               __in            size_t,
                                                               __out __notnull void*);

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
PUBLIC EMERALD_API void system_resizable_vector_insert_element_at(__in system_resizable_vector,
                                                                  __in size_t,
                                                                  __in void*);

/** Resizable vector comparator.
 *
 *  @param system_resizable_vector Object to compare.
 *  @param system_resizable_vector Object to compare with.
 *
 *  @return True if identical, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_is_equal(__in system_resizable_vector,
                                                         __in system_resizable_vector);

/** TODO.
 *
 *  @param vector      Vector to use for the locking process. NOTE: The vector must have
 *                     been created for thread-safe access, otherwise an assertion failure
 *                     will occur.
 *  @param access_type Access type to be used for the lock.
 */
PUBLIC EMERALD_API void system_resizable_vector_lock(__in __notnull system_resizable_vector             vector,
                                                     __in           system_read_write_mutex_access_type access_type);

/** Retrieves last object stored for a given resizable vector and removes it from the vector.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Deref will be used to store the value
 *
 *  @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool system_resizable_vector_pop(__in            system_resizable_vector,
                                                    __out __notnull void*                   result);

/** Stores an element (having copied it by value) at next available cell of the resizable vector instance.
 *
 *  @param system_resizable_vector Object to operate on.
 *  @param void*                   Object to store.
 */
PUBLIC EMERALD_API void system_resizable_vector_push(__in     system_resizable_vector,
                                                     __in_opt void*);

/** Releases a system resizable vector instance. Object should not be accessed after this call.
 *
 *  @param system_resizable_vector Resizable vector to release.
 */
PUBLIC EMERALD_API void system_resizable_vector_release(__in __deallocate(mem) system_resizable_vector);

/** Stores an element (having copied it by value) in a cell located at user-provided index of the resizable
 *  vector object.
 *
 *  NOTE: An assertion failure will occur if index >= vector's capacity.
 * 
 *  @param system_resizable_vector Resizable vector to operate on.
 *  @param size_t                  Index to place the item at.
 *  @param void*                   Object to store.
 */
PUBLIC EMERALD_API void system_resizable_vector_set_element_at(__in             system_resizable_vector,
                                                               __in             size_t,
                                                               __in __maybenull void*);

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_sort(__in __notnull system_resizable_vector,
                                                                    int (*)(void*, void*) );

/** TODO */
PUBLIC EMERALD_API bool system_resizable_vector_swap(__in __notnull system_resizable_vector,
                                                     __in            uint32_t,
                                                     __in            uint32_t);

/** TODO */
PUBLIC EMERALD_API void system_resizable_vector_unlock(__in __notnull system_resizable_vector             vector,
                                                       __in           system_read_write_mutex_access_type access_type);

#endif /* SYSTEM_RESIZABLE_VECTOR_H */