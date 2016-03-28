/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_file_serializer.h"
#include "system/system_list_bidirectional.h"
#include "system/system_memory_manager.h"
#include "system/system_resource_pool.h"
#include <string.h>

#define MIN_PAGE_SIZE 4096

/* If this is defined, alloc and free operations will be logged to a text file.
 * This is useful for hunting down bugs in the memory manager layer */
#ifdef _DEBUG
    //#define LOG_ALLOC_HISTORY
#endif /* _DEBUG */

/* If this is defined, the implementation will perform a sanity check of the
 * page owner counters upon free_block calls.
 * This will seriously deteriorate performance, so should only be enabled for debugging */
#ifdef _DEBUG
    /* DO NOT leave enabled! */
    //#define ENABLE_INTEGRITY_CHECKS_AT_FREE_CALL_TIME
#endif /* _DEBUG */


typedef struct _system_memory_manager_block
{
    unsigned int block_offset; /* NOT aligned to page size */
    unsigned int block_size;

    unsigned int page_offset; /* tells the page-aligned offset, relative to the beginning of the manager's memory block */
    unsigned int page_size;

    unsigned int result_offset; /* the offset returned by the alloc() function. takes user-specified alignment into consideration */
} _system_memory_manager_block;

typedef struct _system_memory_manager
{
    system_list_bidirectional alloced_blocks;        /* holds allocated memory region descriptors (owned by block_descriptor_pool) */
    system_list_bidirectional available_blocks;      /* holds available memory region descriptors (owned by block_descriptor_pool) */
    system_resource_pool      block_descriptor_pool; /* holds _system_memory_manager_block items */

    system_critical_section              cs;
    unsigned int                         memory_region_size;
    unsigned int                         page_size;
    PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC pfn_on_memory_block_alloced;
    PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC  pfn_on_memory_block_freed;
    void*                                user_arg;

    #ifdef LOG_ALLOC_HISTORY
        system_file_serializer alloc_log_serializer;
        static unsigned int    n_alloc_log_serializers;
    #endif


    /* Each item holds the number of blocks that are assigned to corresponding pages.
     *
     * A page should only be committed once if it has never been used before.
     *
     * A page can only be de-committed (hence, a "freed" call-back should be made) if
     * it does not hold any content.
     */
    unsigned int* page_owners;

    explicit _system_memory_manager(bool                                 in_thread_safe,
                                    unsigned int                         in_memory_region_size,
                                    unsigned int                         in_page_size,
                                    void*                                in_user_arg,
                                    PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC in_pfn_on_memory_block_alloced,
                                    PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC  in_pfn_on_memory_block_freed)
    {
        in_page_size = max(in_page_size, MIN_PAGE_SIZE);

        const unsigned int n_page_owners = in_memory_region_size / in_page_size + 1;

        if (in_thread_safe)
        {
            cs = system_critical_section_create();
        }
        else
        {
            cs = NULL;
        }

        alloced_blocks              = system_list_bidirectional_create();
        available_blocks            = system_list_bidirectional_create();
        block_descriptor_pool       = system_resource_pool_create     (sizeof(_system_memory_manager_block),
                                                                       16,  /* n_elements */
                                                                       NULL,  /* init_fn */
                                                                       NULL); /* deinit_fn */
        memory_region_size          = in_memory_region_size;
        page_owners                 = new (std::nothrow) unsigned int[n_page_owners];
        page_size                   = in_page_size;
        pfn_on_memory_block_alloced = in_pfn_on_memory_block_alloced;
        pfn_on_memory_block_freed   = in_pfn_on_memory_block_freed;
        user_arg                    = in_user_arg;

        #ifdef LOG_ALLOC_HISTORY
        {
            char temp[1024];

            snprintf(temp,
                     sizeof(temp),
                     "log_memory_manager_%d.txt",
                     (n_alloc_log_serializers++) );

            alloc_log_serializer = system_file_serializer_create_for_writing(system_hashed_ansi_string_create(temp) );

            snprintf(temp,
                     sizeof(temp),
                     "constructor(in_memory_region_size:%d in_page_size:%d)\n",
                     in_memory_region_size,
                     in_page_size);

            system_file_serializer_write       (alloc_log_serializer,
                                                strlen(temp),
                                                temp);
            system_file_serializer_flush_writes(alloc_log_serializer);
        }
        #endif /* LOG_ALLOC_HISTORY */

        memset(page_owners,
               0,
               n_page_owners * sizeof(unsigned int) );
    }

    ~_system_memory_manager()
    {
        if (alloced_blocks != NULL)
        {
            system_list_bidirectional_release(alloced_blocks);

            alloced_blocks = NULL;
        }

        #ifdef LOG_ALLOC_HISTORY
        {
            system_file_serializer_release(alloc_log_serializer);

            alloc_log_serializer = NULL;
        }
        #endif /* LOG_ALLOC_HISTORY */

        if (available_blocks != NULL)
        {
            system_list_bidirectional_release(available_blocks);

            available_blocks = NULL;
        }

        if (block_descriptor_pool != NULL)
        {
            system_resource_pool_release(block_descriptor_pool);

            block_descriptor_pool = NULL;
        }

        if (cs != NULL)
        {
            system_critical_section_release(cs);

            cs = NULL;
        }

        if (page_owners != NULL)
        {
            delete [] page_owners;

            page_owners = NULL;
        }
    }
} _system_memory_manager;

#ifdef LOG_ALLOC_HISTORY
    unsigned int _system_memory_manager::n_alloc_log_serializers = 0;
#endif

/** Please see header for spec */
PUBLIC EMERALD_API bool system_memory_manager_alloc_block(system_memory_manager manager,
                                                          unsigned int          size,
                                                          unsigned int          required_alignment,
                                                          unsigned int*         out_allocation_offset)
{
    _system_memory_manager* manager_ptr = (_system_memory_manager*) manager;
    bool                    result      = false;

    if (manager_ptr->cs != NULL)
    {
        system_critical_section_enter(manager_ptr->cs);
    }

    #ifdef LOG_ALLOC_HISTORY
    {
        char temp[1024];

        snprintf(temp,
                 sizeof(temp),
                 "system_memory_manager_alloc_block(size:%d required_alignment:%d)\n",
                 size,
                 required_alignment);

        system_file_serializer_write       (manager_ptr->alloc_log_serializer,
                                            strlen(temp),
                                            temp);
        system_file_serializer_flush_writes(manager_ptr->alloc_log_serializer);
    }
    #endif /* LOG_ALLOC_HISTORY */

    /* Check if there's any memory region that can fit the requested block */
    system_list_bidirectional_item current_memory_item = system_list_bidirectional_get_head_item(manager_ptr->available_blocks);

    while (!result && current_memory_item != NULL)
    {
        _system_memory_manager_block* current_memory_block_ptr = NULL;
        unsigned int                  block_offset_padding     = 0;

        system_list_bidirectional_get_item_data(current_memory_item,
                                               (void**) &current_memory_block_ptr);

        if (required_alignment != 0)
        {
            block_offset_padding = (required_alignment - current_memory_block_ptr->block_offset % required_alignment) % required_alignment;
        }

        const unsigned int n_bytes_required = size + block_offset_padding; /* also count the n of bytes we need to move forward to adhere to the alignment requirements */

        /* Does this block fit within the current region? */
        if (current_memory_block_ptr->block_size >= n_bytes_required)
        {
            const unsigned int left_memory_block_size  = n_bytes_required;
            const unsigned int right_memory_block_size = current_memory_block_ptr->block_size - n_bytes_required;

            /* Split the region into two sub-regions.
             *
             * The left region describes the requested allocation.
             * The right region refers to the remaining space that can be further split.
             */
            _system_memory_manager_block* left_memory_block_ptr  = (_system_memory_manager_block*) system_resource_pool_get_from_pool(manager_ptr->block_descriptor_pool);
            _system_memory_manager_block* right_memory_block_ptr = NULL;

            left_memory_block_ptr->block_offset  = current_memory_block_ptr->block_offset;
            left_memory_block_ptr->block_size    = n_bytes_required;
            left_memory_block_ptr->page_offset   =  (left_memory_block_ptr->block_offset                                                                                                        / manager_ptr->page_size) * manager_ptr->page_size;
            left_memory_block_ptr->page_size     = ((left_memory_block_ptr->block_offset + left_memory_block_ptr->block_size - left_memory_block_ptr->page_offset + manager_ptr->page_size - 1) / manager_ptr->page_size) * manager_ptr->page_size;
            left_memory_block_ptr->result_offset = left_memory_block_ptr->block_offset + block_offset_padding;

            *out_allocation_offset = left_memory_block_ptr->result_offset;

            system_list_bidirectional_push_at_end(manager_ptr->alloced_blocks,
                                                  left_memory_block_ptr);

            /* Call back the owner. Note that:
             *
             * 1) we need to use the page-aligned offset and size here.
             * 2) we need not call back the user for pages that have already been reported.
             */
            unsigned int callback_page_index_end   = -1;
            unsigned int callback_page_index_mark  = -1;
            unsigned int callback_page_index_start = -1;
            unsigned int callback_page_offset      = left_memory_block_ptr->page_offset;
            unsigned int callback_page_size        = left_memory_block_ptr->page_size;

            ASSERT_DEBUG_SYNC(left_memory_block_ptr->block_offset + left_memory_block_ptr->block_size <= manager_ptr->memory_region_size,
                              "Allocation exceeding available pages!");

            ASSERT_DEBUG_SYNC((callback_page_offset % manager_ptr->page_size) == 0 &&
                              (callback_page_size   % manager_ptr->page_size) == 0,
                              "Callback values are not rounded to page size.");

            callback_page_index_start =  callback_page_offset                       / manager_ptr->page_size;
            callback_page_index_end   = (callback_page_offset + callback_page_size) / manager_ptr->page_size;
            callback_page_index_mark  = callback_page_index_start; /* used to delimit the pages that have already been called back about */

            for (unsigned int page_index = callback_page_index_start;
                              page_index < callback_page_index_end;
                            ++page_index)
            {
                manager_ptr->page_owners[page_index]++;

                /* At this moment, if a given page uses a counter != 1, it can only mean that the page
                 * has already been used in the past. In such case, do *not* issue the call-back for
                 * the considered page. However, the call-back should still be issued for all the
                 * pages we already traversed, which have not been committed earlier. */
                if (manager_ptr->pfn_on_memory_block_alloced != NULL       &&
                    manager_ptr->page_owners[page_index]     != 1          &&
                    callback_page_index_mark                 != page_index)
                {
                    manager_ptr->pfn_on_memory_block_alloced(manager,
                                                              callback_page_index_mark               * manager_ptr->page_size,
                                                             (page_index - callback_page_index_mark) * manager_ptr->page_size,
                                                             manager_ptr->user_arg);

                    /* Update the marker index */
                    callback_page_index_mark = callback_page_index_start;
                }
            } /* for (all affected pages) */

            if (manager_ptr->pfn_on_memory_block_alloced            != NULL                    &&
                manager_ptr->page_owners[callback_page_index_end-1] == 1                       &&
                callback_page_index_mark                            != callback_page_index_end)
            {
                manager_ptr->pfn_on_memory_block_alloced(manager,
                                                         callback_page_index_mark                            * manager_ptr->page_size,
                                                        (callback_page_index_end - callback_page_index_mark) * manager_ptr->page_size,
                                                        manager_ptr->user_arg);
            }

            /* Now for the right sub-region.. */
            if (right_memory_block_size > 0)
            {
                right_memory_block_ptr = (_system_memory_manager_block*) system_resource_pool_get_from_pool(manager_ptr->block_descriptor_pool);

                right_memory_block_ptr->block_offset  = left_memory_block_ptr->block_offset + left_memory_block_ptr->block_size;
                right_memory_block_ptr->block_size    = right_memory_block_size;
                right_memory_block_ptr->page_offset   = left_memory_block_ptr->page_offset - (manager_ptr->page_size + left_memory_block_ptr->page_offset % manager_ptr->page_size) % manager_ptr->page_size;
                right_memory_block_ptr->page_size     = 0;
                right_memory_block_ptr->result_offset = 0;

                system_list_bidirectional_push_at_end(manager_ptr->available_blocks,
                                                      right_memory_block_ptr);
            }

            /* Remove the memory item we have split from the list */
            system_list_bidirectional_remove_item(manager_ptr->available_blocks,
                                                  current_memory_item);
            system_resource_pool_return_to_pool  (manager_ptr->block_descriptor_pool,
                                                  (system_resource_pool_block) current_memory_block_ptr);

            /* Done */
            result = true;
        } /* if (current_memory_block_ptr->size >= n_bytes_required) */

        /* Move on.. */
        current_memory_item = system_list_bidirectional_get_next_item(current_memory_item);
    } /* while (current_memory_item != NULL) */

    /* All done */
    if (manager_ptr->cs != NULL)
    {
        system_critical_section_leave(manager_ptr->cs);
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API system_memory_manager system_memory_manager_create(unsigned int                         memory_region_size,
                                                                      unsigned int                         page_size,
                                                                      PFNSYSTEMMEMORYMANAGERALLOCBLOCKPROC pfn_on_memory_block_alloced,
                                                                      PFNSYSTEMMEMORYMANAGERFREEBLOCKPROC  pfn_on_memory_block_freed,
                                                                      void*                                user_arg,
                                                                      bool                                 should_be_thread_safe)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(memory_region_size != 0,
                      "Input memory region size is zero.");
    ASSERT_DEBUG_SYNC(page_size != 0,
                      "Input page size is zero");
    ASSERT_DEBUG_SYNC(memory_region_size >= page_size,
                      "Page size is smaller than the manageable memory region size");

    /* Spawn the allocator */
    _system_memory_manager* new_manager = new (std::nothrow) _system_memory_manager(should_be_thread_safe,
                                                                                    memory_region_size,
                                                                                    page_size,
                                                                                    user_arg,
                                                                                    pfn_on_memory_block_alloced,
                                                                                    pfn_on_memory_block_freed);

    ASSERT_ALWAYS_SYNC(new_manager != NULL,
                       "Out of memory");

    if (new_manager != NULL)
    {
        /* Create a descriptor for the memory region we are responsible for */
        _system_memory_manager_block* new_block_ptr = (_system_memory_manager_block*) system_resource_pool_get_from_pool(new_manager->block_descriptor_pool);

        new_block_ptr->block_offset  = 0;
        new_block_ptr->block_size    = memory_region_size;
        new_block_ptr->page_offset   = 0;
        new_block_ptr->page_size     = 0; /* block does not use any committed pages */
        new_block_ptr->result_offset = 0;

        /* ..and store it */
        system_list_bidirectional_push_at_end(new_manager->available_blocks,
                                              new_block_ptr);
    } /* if (new_allocator != NULL) */

    /* All done */
    return (system_memory_manager) new_manager;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_memory_manager_free_block(system_memory_manager manager,
                                                         unsigned int          alloc_offset)
{
    _system_memory_manager* manager_ptr = (_system_memory_manager*) manager;

    if (manager_ptr->cs)
    {
        system_critical_section_enter(manager_ptr->cs);
    }

    #ifdef LOG_ALLOC_HISTORY
    {
        char temp[1024];

        snprintf(temp,
                 sizeof(temp),
                 "system_memory_manager_free_block(alloc_offset:%d)\n",
                 alloc_offset);

        system_file_serializer_write       (manager_ptr->alloc_log_serializer,
                                            strlen(temp),
                                            temp);
        system_file_serializer_flush_writes(manager_ptr->alloc_log_serializer);
    }
    #endif /* LOG_ALLOC_HISTORY */

    /* We could optimize this, but, for simplicity's sake, let's do a linear search here for now. */
    system_list_bidirectional_item current_item = system_list_bidirectional_get_head_item(manager_ptr->alloced_blocks);
    bool                           has_found    = false;

    while (!has_found && current_item != NULL)
    {
        _system_memory_manager_block* current_block_ptr = NULL;

        system_list_bidirectional_get_item_data(current_item,
                                               (void**) &current_block_ptr);

        if (current_block_ptr->result_offset == alloc_offset)
        {
            /* We have a find! */
            system_list_bidirectional_remove_item(manager_ptr->alloced_blocks,
                                                  current_item);

            unsigned int callback_page_index_start =  current_block_ptr->block_offset                                 / manager_ptr->page_size;
            unsigned int callback_page_index_end   = (current_block_ptr->block_offset + current_block_ptr->page_size) / manager_ptr->page_size;
            unsigned int callback_page_index_mark  = callback_page_index_start; /* used to delimit the pages that have already been called back about */

            for (unsigned int page_index = callback_page_index_start;
                              page_index < callback_page_index_end;
                            ++page_index)
            {
                ASSERT_DEBUG_SYNC(manager_ptr->page_owners[page_index] > 0,
                                  "Zero counter detected!");

                --manager_ptr->page_owners[page_index];

                if (manager_ptr->pfn_on_memory_block_freed != NULL       &&
                    manager_ptr->page_owners[page_index]   != 0          &&
                    callback_page_index_mark               != page_index)
                {
                    const unsigned int free_start_offset = (callback_page_index_mark)              * manager_ptr->page_size;
                    const unsigned int free_size         = (page_index - callback_page_index_mark) * manager_ptr->page_size;

                    #ifdef _DEBUG
                    {
                        for (unsigned int index =  free_start_offset              / manager_ptr->page_size;
                                          index < (free_start_offset + free_size) / manager_ptr->page_size;
                                        ++index)
                        {
                            ASSERT_DEBUG_SYNC(manager_ptr->page_owners[index] == 0,
                                              "Sanity check failed");
                        }
                    }
                    #endif

                    /* Dealloc the whole blob */
                    manager_ptr->pfn_on_memory_block_freed(manager,
                                                           free_start_offset,
                                                           free_size,
                                                           manager_ptr->user_arg);

                    /* Update the marker index */
                    callback_page_index_mark = callback_page_index_start;
                }

                if (manager_ptr->page_owners[callback_page_index_mark] > 0)
                {
                    callback_page_index_mark++;
                }
            } /* for (all affected pages) */

            if (manager_ptr->pfn_on_memory_block_freed                != NULL                    &&
                callback_page_index_mark                              != callback_page_index_end &&
                manager_ptr->page_owners[callback_page_index_end - 1] == 0)
            {
                const unsigned int free_start_offset = callback_page_index_mark                            * manager_ptr->page_size;
                const unsigned int free_size         = (callback_page_index_end - callback_page_index_mark) * manager_ptr->page_size;

                #ifdef _DEBUG
                {
                    for (unsigned int index =  free_start_offset              / manager_ptr->page_size;
                                      index < (free_start_offset + free_size) / manager_ptr->page_size;
                                    ++index)
                    {
                        ASSERT_DEBUG_SYNC(manager_ptr->page_owners[index] == 0,
                                          "Sanity check failed");
                    }
                }
                #endif

                manager_ptr->pfn_on_memory_block_freed(manager,
                                                       free_start_offset,
                                                       free_size,
                                                       manager_ptr->user_arg);
            }

            has_found = true;

            system_list_bidirectional_push_at_front(manager_ptr->available_blocks,
                                                    current_block_ptr);

            /* Bail out of the loop */
            break;
        }

        /* Move on */
        current_item = system_list_bidirectional_get_next_item(current_item);
    } /* while (current_item != NULL) */

    ASSERT_DEBUG_SYNC(has_found,
                      "Submitted memory block was not found.");

    #if defined(_DEBUG) && defined(ENABLE_INTEGRITY_CHECKS_AT_FREE_CALL_TIME)
    {
        if (has_found)
        {
            /* Iterate over all pages.. */
            _system_memory_manager_block* current_block_ptr = NULL;
            const unsigned int            n_total_pages      = manager_ptr->memory_region_size / manager_ptr->page_size;

            for (unsigned int n_page = 0;
                              n_page < n_total_pages;
                            ++n_page)
            {
                      unsigned int n_page_committed_blocks = 0;
                const unsigned int page_start_offset       =  n_page      * manager_ptr->page_size;
                const unsigned int page_end_offset         = (n_page + 1) * manager_ptr->page_size - 1;

                /* Iterate over all allocations and count them manually */
                system_list_bidirectional_item current_block_item      = system_list_bidirectional_get_head_item(manager_ptr->alloced_blocks);
                void*                          current_block_item_data = NULL;

                while (current_block_item != NULL)
                {
                    _system_memory_manager_block* block_ptr = NULL;

                    system_list_bidirectional_get_item_data(current_block_item,
                                                           &current_block_item_data);

                    block_ptr = (_system_memory_manager_block*) current_block_item_data;

                    /* Does this block intersect with the page? */
                    /* 1.  BBBB
                     *    ppp
                     *
                     * 2.  BBBB
                     *      p
                     *
                     * 3.  BBBB
                     *      pppp
                     *
                     * 4.  BBBB
                     *    pppppp
                     */
                    if (page_start_offset <=  block_ptr->block_offset                          &&
                        page_end_offset   >=  block_ptr->block_offset                          &&
                        page_end_offset   <  (block_ptr->block_offset + block_ptr->block_size) ||

                        page_start_offset >=  block_ptr->block_offset                          &&
                        page_end_offset   <  (block_ptr->block_offset + block_ptr->block_size) ||

                        page_start_offset >=  block_ptr->block_offset                          &&
                        page_start_offset <  (block_ptr->block_offset + block_ptr->block_size) &&
                        page_end_offset   >  (block_ptr->block_offset + block_ptr->block_size) ||

                        page_start_offset <=  block_ptr->block_offset                          &&
                        page_end_offset   >  (block_ptr->block_offset + block_ptr->block_size) )
                    {
                        n_page_committed_blocks++;
                    }

                    /* Move on */
                    current_block_item = system_list_bidirectional_get_next_item(current_block_item);
                } /* while (current_block_item != NULL) */

                ASSERT_DEBUG_SYNC(n_page_committed_blocks == manager_ptr->page_owners[n_page],
                                  "Page owner counter is corrupt!");
            } /* for (all pages) */
        } /* if (has_found) */
    }
    #endif /* ENABLE_INTEGRITY_CHECKS_AT_FREE_CALL_TIME */

    if (manager_ptr->cs)
    {
        system_critical_section_leave(manager_ptr->cs);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_memory_manager_release(system_memory_manager manager)
{
    _system_memory_manager* manager_ptr = (_system_memory_manager*) manager;

    delete manager_ptr;
    manager_ptr = NULL;
}
