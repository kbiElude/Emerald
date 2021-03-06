/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_LINEAR_ALLOC_H
#define SYSTEM_LINEAR_ALLOC_H

#include "system_types.h"


/** Create a pin-based linear allocator instance.
 *
 *  The pin-based linear allocator aligns the allocated blocks to pointer size.
 *  The pin-based linear allocator allows to very quickly de-allocate blocks on the fly by
 *  popping pins. This causes all blocks allocated since last pin insertion to be "freed".
 *  The pin-based linear allocator does not support de-allocation call-backs.
 *
 *  @param size_t Single allocation size.
 *  @param size_t Entries to prealloc. If more allocations than this value follow, you can encounter
 *                major performance breakdown as the buffers are re-allocated
 *  @param size_t Pins to preallocate. If more pins will be used, you can encounter major performance
 *                hit when re-allocating.
 *
 *  @return Pin-based linear allocator instance handle.
 */
PUBLIC EMERALD_API system_linear_alloc_pin system_linear_alloc_pin_create(size_t entry_size,
                                                                          size_t n_entries_to_prealloc,
                                                                          size_t n_pins_to_prealloc);

/** Retrieves a new memory block from linear allocator. Each block has exactly the same
 *  size as declared when creating the allocator.
 *
 *  NOTE: The block is owned by the allocator. DO NOT release it. The allocator will
 *        release it when being freed.
 *
 *  @param system_linear_alloc_pin_handle Handle to the linear allocator instance.
 *
 *  @return Pointer to the block for caller's usage.
 */
PUBLIC EMERALD_API void* system_linear_alloc_pin_get_from_pool(system_linear_alloc_pin allocator);

/** Marks a new pin for a given linear allocator. All allocations following the pin will be freed
 *  when system_linear_alloc_pin_release_pop_pin() is called. A pin is always consdiered to be inserted
 *  at the beginning of the preallocated space.
 *
 *  @param system_linear_alloc_pin_handle Pin-based linear alloc handle.
 */
PUBLIC EMERALD_API void system_linear_alloc_pin_mark(system_linear_alloc_pin allocator);

/** Releases a pin-based linear allocator instance. Do not use the handle after calling this function.
 *
 *  @param system_linear_alloc_pin_handle Handle to a linear allocator instance.
 */
PUBLIC EMERALD_API void system_linear_alloc_pin_release(system_linear_alloc_pin allocator);

/** TODO */
PUBLIC EMERALD_API void system_linear_alloc_pin_return_all(system_linear_alloc_pin allocator);

/** Pops a pin from a given linear allocator. Causes all allocations performed after the last pin to become
 *  no longer valid.
 *
 *  @param system_linear_alloc_pin_handle Pin-based linear alloc handle.
 */
PUBLIC EMERALD_API void system_linear_alloc_pin_unmark(system_linear_alloc_pin allocator);


#endif /* SYSTEM_LINEAR_ALLOC_H */

