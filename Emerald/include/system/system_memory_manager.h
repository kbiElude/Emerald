/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Implements a "first fit, decreasing size" memory allocation policy.
 *
 * The module transfers the task of physically allocating and releasing
 * the memory blocks to parents. This allows for cascaded memory allocator
 * implementation, and lets the module users re-use the same code for
 * managing CPU-side and GPU-side memory management processes.
 *
 * It is expected this allocator will be used to manage sparse buffers
 * whose memory region commitments may change over time, but rather
 * infrequently.
 *
 * Internal usage only.
 */
#ifndef SYSTEM_MEMORY_MANAGER
#define SYSTEM_MEMORY_MANAGER

#include "system_types.h"

typedef void (*PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC)(system_memory_manager manager,
                                                     unsigned int          offset_aligned,
                                                     unsigned int          size,
                                                     void*                 user_arg);
typedef void (*PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC) (system_memory_manager manager,
                                                     unsigned int          offset_aligned,
                                                     unsigned int          size,
                                                     void*                 user_arg);


/** TODO
 *
 *  Internally, the blocks are aligned on the page size boundary. The returned block's offset
 *  is aligned to the requested alignment.
 *
 *  The call-backs will NOT include the required alignment.
 *
 *  @param required_alignment If not 0, pass the expected alignment for the allocation.
 *                            Passing a zero indicates no specific alignment requirements
 *                            should be taken into consideration.
 *
 *  @param out_allocation_offset Offset, at which a contiguous set of memory pages capable to store
 *                               the requested number of bytes starts.
 *                               The offset IS padded to the requested alignment.
 *
 *  @return TODO
 **/
PUBLIC EMERALD_API bool system_memory_manager_alloc_block(system_memory_manager manager,
                                                          unsigned int          size,
                                                          unsigned int          required_alignment,
                                                          unsigned int*         out_allocation_offset);

/** TODO */
PUBLIC EMERALD_API system_memory_manager system_memory_manager_create(unsigned int                         memory_region_size,
                                                                      unsigned int                         page_size,
                                                                      PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC pfn_on_memory_block_alloced,
                                                                      PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC  pfn_on_memory_block_freed,
                                                                      void*                                user_arg,
                                                                      bool                                 should_be_thread_safe);

/** TODO */
PUBLIC EMERALD_API void system_memory_manager_free_block(system_memory_manager manager,
                                                         unsigned int          alloc_offset);

/** TODO */
PUBLIC EMERALD_API void system_memory_manager_release(system_memory_manager manager);

#endif /* SYSTEM_MEMORY_MANAGER */
