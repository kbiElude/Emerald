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

typedef void (*PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC)(__in system_memory_manager manager,
                                                     __in unsigned int          offset_aligned,
                                                     __in unsigned int          size);
typedef void (*PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC) (__in system_memory_manager manager,
                                                     __in unsigned int          offset_aligned,
                                                     __in unsigned int          size);


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
PUBLIC EMERALD_API bool system_memory_manager_alloc_block(__in  __notnull system_memory_manager manager,
                                                          __in            unsigned int          size,
                                                          __in            unsigned int          required_alignment,
                                                          __out __notnull unsigned int*         out_allocation_offset);

/** TODO */
PUBLIC EMERALD_API system_memory_manager system_memory_manager_create(__in           unsigned int                         memory_region_size,
                                                                      __in           unsigned int                         page_size,
                                                                      __in __notnull PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC pfn_on_memory_block_alloced,
                                                                      __in __notnull PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC  pfn_on_memory_block_freed,
                                                                      __in_opt       void*                                user_arg,
                                                                      __in           bool                                 should_be_thread_safe);

/** TODO */
PUBLIC EMERALD_API void system_memory_manager_free_block(__in __notnull system_memory_manager manager,
                                                         __in           unsigned int          alloc_offset);

/** TODO */
PUBLIC EMERALD_API void system_memory_manager_release(__in __notnull system_memory_manager manager);

#endif /* SYSTEM_MEMORY_MANAGER */
