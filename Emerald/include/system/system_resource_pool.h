/**
 *
 *  Emerald (kbi/elude @2012-2015)
 *
 *  @brief Resource pool is a structure that pre-allocates user-defined amount of elements in blobs.
 *         First get() call retrieves the element from the array. If the element is returned to the
 *         pool, it is moved to a resizable vector, and then reported to the caller() with the first
 *         get_from_pool() call.
 */
#ifndef SYSTEM_RESOURCE_POOL_H
#define SYSTEM_RESOURCE_POOL_H

#include "system/system_types.h"


/*  Creates a new resource pool.
 *
 *  @param size_t                           Size of a single element to store in the resource pool.
 *  @param size_t                           Amount of elements to prealloc for a single blob.
 *  @param PFNSYSTEMRESOURCEPOOLINITBLOCK   Function to call whenever a new item is requested by the callers. This
 *                                          function will be called ONLY when requesting the item for the first time.
 *  @param PFNSYSTEMRESOURCEPOOLDEINITBLOCK Function to call when the pool is being released for each of the items returned
 *                                          to the pool that have already been initialized.
 *
 *  @return Handle to the new resource pool object.
 */
PUBLIC EMERALD_API system_resource_pool system_resource_pool_create(size_t                           element_size,
                                                                    size_t                           n_elements_to_preallocate,
                                                                    PFNSYSTEMRESOURCEPOOLINITBLOCK   init_fn,
                                                                    PFNSYSTEMRESOURCEPOOLDEINITBLOCK deinit_fn);

/*  Obtains a pointer to preallocated space from the pool.
 *
 *  NOTE: It is caller's responsibility to fill the space with data! The block may already
 *        contain data from previous allocations.
 *
 *  @return Pointer to a block with amount of space as defined when creating the pool.
 */
PUBLIC EMERALD_API system_resource_pool_block system_resource_pool_get_from_pool(system_resource_pool pool);

/** TODO */
PUBLIC EMERALD_API void system_resource_pool_return_all_allocations(system_resource_pool pool);

/*  Returns a block back to the pool.
 *
 *  NOTE: This function does not check whether the pointer actually comes from managed
 *        blobs (for performance reasons).
 *
 *  @param system_resource_pool       Pool to release the block to.
 *  @param system_resource_pool_block Block to return back to the pool.
 */
PUBLIC EMERALD_API void system_resource_pool_return_to_pool(system_resource_pool       pool,
                                                            system_resource_pool_block block);

/*  Releases a resource pool object. This renders all managed blocks invalid!
 *
 *  @param system_resource_pool Handle to the resource pool.
 */
PUBLIC EMERALD_API void system_resource_pool_release(system_resource_pool pool);


#endif /* SYSTEM_RESOURCE_POOL_H */
