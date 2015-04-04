/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_log.h"
#include "system/system_memory_manager.h"
#include "system/system_resizable_vector.h"
#include "gtest/gtest.h"


struct _memory_block
{
    unsigned int offset_aligned;
    unsigned int size;

    explicit _memory_block(__in unsigned int in_offset_aligned,
                           __in unsigned int in_size)
    {
        offset_aligned = in_offset_aligned;
        size           = in_size;
    }
};

PRIVATE system_resizable_vector memory_blocks = NULL; /* holds _memory_block* */


static void _free_memory_blocks()
{
    if (memory_blocks != NULL)
    {
        _memory_block* block_ptr = NULL;

        while (system_resizable_vector_pop(memory_blocks,
                                          &block_ptr) )
        {
            delete block_ptr;

            block_ptr = NULL;
        }

        system_resizable_vector_release(memory_blocks);
        memory_blocks = NULL;
    } /* if (memory_blocks != NULL) */
}

static void _mmanager_block_alloc_callback(__in system_memory_manager manager,
                                           __in unsigned int          offset_aligned,
                                           __in unsigned int          size,
                                           __in void*                 user_arg)
{
    _memory_block* new_block_ptr = new _memory_block(offset_aligned,
                                                     size);

    system_resizable_vector_push(memory_blocks,
                                 new_block_ptr);
}

static void _mmanager_block_freed_callback(__in system_memory_manager manager,
                                           __in unsigned int          offset_aligned,
                                           __in unsigned int          size,
                                           __in void*                 user_arg)
{
    /* Find the matching block */
    bool         has_found               = false;
    unsigned int n_alloced_memory_blocks = system_resizable_vector_get_amount_of_elements(memory_blocks);

    for (int n_block = 0;
             n_block < n_alloced_memory_blocks;
           ++n_block)
    {
        _memory_block* block_ptr = NULL;

        system_resizable_vector_get_element_at(memory_blocks,
                                               n_block,
                                              &block_ptr);

        /* Release the block if it is fully encapsulated by the region */
        if ( offset_aligned         <= block_ptr->offset_aligned &&
            (offset_aligned + size) >= block_ptr->offset_aligned + block_ptr->size)
        {
            system_resizable_vector_delete_element_at(memory_blocks,
                                                      n_block);

            has_found = true;
            n_block   = -1;

            n_alloced_memory_blocks--;
        }
    }

    if (!has_found)
    {
        LOG_ERROR("Freed memory block was not found in the internal storage!");
    }
}


TEST(MemoryManagerTest, AlignedPageSizeAllocations)
{
    /* This test verifies that we can fit exactly 4 pages in the managed memory block,
     * if the block size matches the page size.
     * It also checks that subsequent allocations will return failure.
     */
    const unsigned int allocation_size = 4096;
    const unsigned int n_allocations   = 4;
          unsigned int result_offset   = -1;

    system_memory_manager manager = system_memory_manager_create(allocation_size * n_allocations, /* memory_region_size */
                                                                 allocation_size,                 /* page_size */
                                                                 _mmanager_block_alloc_callback,
                                                                 _mmanager_block_freed_callback,
                                                                 NULL,                           /* user_arg */
                                                                 false);                         /* should_be_thread_safe */

    memory_blocks = system_resizable_vector_create(4, /* capacity */
                                                   sizeof(_memory_block*) );

    for (unsigned int n_allocation = 0;
                      n_allocation < n_allocations;
                    ++n_allocation)
    {
        bool result = false;

        result = system_memory_manager_alloc_block(manager,
                                                   allocation_size, /* size */
                                                   allocation_size, /* required_alignment */
                                                  &result_offset);

        ASSERT_TRUE(result);
        ASSERT_EQ  (result_offset,
                    n_allocation * allocation_size);

        /* Also check if the call-back occured */
        _memory_block* block_ptr = NULL;

        ASSERT_EQ(system_resizable_vector_get_amount_of_elements(memory_blocks),
                  n_allocation + 1);

        system_resizable_vector_get_element_at(memory_blocks,
                                               n_allocation,
                                              &block_ptr);

        ASSERT_EQ(block_ptr->offset_aligned,
                  n_allocation * allocation_size);
        ASSERT_EQ(block_ptr->size,
                  allocation_size);
    }

    /* Subsequent allocation should fail */
    ASSERT_FALSE(system_memory_manager_alloc_block(manager,
                                                   allocation_size, /* size */
                                                   allocation_size, /* required_alignment */
                                                  &result_offset) );

    /* All done */
    _free_memory_blocks();

    system_memory_manager_release(manager);
}

TEST(MemoryManagerTest, UnalignedNonPageSizeAllocations)
{
    /* This test verifies that we can fit the right number of allocations in the managed memory block.
     * It is assumed the allocations need not be aligned to any specific boundary.
     *
     * The test also checks that subsequent allocations return failure.
     */
          unsigned int alloced_data_size    = 0;
          unsigned int n_allocations        = 0;
          unsigned int result_offset        = -1;
    const unsigned int total_available_size = 4096;
    const unsigned int page_size            = total_available_size / 4;

    system_memory_manager manager = system_memory_manager_create(total_available_size,           /* memory_region_size */
                                                                 page_size,
                                                                 _mmanager_block_alloc_callback,
                                                                 _mmanager_block_freed_callback,
                                                                 NULL,                           /* user_arg */
                                                                 false);                         /* should_be_thread_safe */

    memory_blocks = system_resizable_vector_create(4, /* capacity */
                                                   sizeof(_memory_block*) );

    while (alloced_data_size < total_available_size)
    {
        unsigned int alloc_size = rand() % (total_available_size / 4);
        bool         result     = false;

        if (alloced_data_size + alloc_size > total_available_size)
        {
            alloc_size = total_available_size - alloced_data_size;
        }

        result = system_memory_manager_alloc_block(manager,
                                                   alloc_size,
                                                   0, /* required_alignment */
                                                  &result_offset);

        ASSERT_TRUE(result);
        ASSERT_EQ  (result_offset,
                    alloced_data_size); /* The returned offset should be an accumulated number of bytes allocated so far */

        /* Also check if the call-back occured. Now that is a bit of a tricky part as the call-backs
         * are coalesced if the allocation stretches between multiple pages, so instead of counting
         * the number of call-backs, we need to make sure a sufficient number of pages has been
         * committed.
         */
        unsigned int       accumulated_page_size = 0;
        _memory_block*     block_ptr             = NULL;
        const unsigned int n_blocks              = system_resizable_vector_get_amount_of_elements(memory_blocks);

        for (unsigned int n_block = 0;
                          n_block < n_blocks;
                        ++n_block)
        {
            system_resizable_vector_get_element_at(memory_blocks,
                                                   n_block,
                                                  &block_ptr);

            ASSERT_TRUE((block_ptr->offset_aligned % page_size) == 0);
            ASSERT_TRUE((block_ptr->size           % page_size) == 0);

            accumulated_page_size += block_ptr->size;
        }

        ASSERT_TRUE(accumulated_page_size >= alloced_data_size + alloc_size);

        alloced_data_size += alloc_size;
        n_allocations     ++;
    }

    /* Subsequent allocation should fail */
    ASSERT_FALSE(system_memory_manager_alloc_block(manager,
                                                   1,          /* size */
                                                   0,          /* required_alignment */
                                                  &result_offset) );

    /* All done */
    _free_memory_blocks();

    system_memory_manager_release(manager);
}

TEST(MemoryManagerTest, AlignedNonPageSizeAllocations)
{
    /* This test verifies that the aligned allocations work correctly. */
          unsigned int alloced_data_size    = 0;
          unsigned int n_allocations        = 0;
    const unsigned int required_alignment   = 53;
          unsigned int result_offset        = -1;
    const unsigned int total_available_size = 4096;
    const unsigned int page_size            = total_available_size / 4;

    system_memory_manager manager = system_memory_manager_create(total_available_size,           /* memory_region_size */
                                                                 page_size,
                                                                 _mmanager_block_alloc_callback,
                                                                 _mmanager_block_freed_callback,
                                                                 NULL,                           /* user_arg */
                                                                 false);                         /* should_be_thread_safe */

    memory_blocks = system_resizable_vector_create(4, /* capacity */
                                                   sizeof(_memory_block*) );

    while (alloced_data_size < total_available_size)
    {
        int  alloc_size = rand() % (total_available_size / 4);
        bool result     = false;

        if (alloced_data_size + alloc_size > total_available_size)
        {
            alloc_size = total_available_size - alloced_data_size - required_alignment;

            if (alloc_size < 0)
            {
                break;
            }
        }

        result = system_memory_manager_alloc_block(manager,
                                                   alloc_size,
                                                   required_alignment,
                                                  &result_offset);

        if (!result)
        {
            break;
        }

        ASSERT_EQ(result_offset % required_alignment,
                  0);

        /* Also check if the call-back occured. Now that is a bit of a tricky part as the call-backs
         * are coalesced if the allocation stretches between multiple pages, so instead of counting
         * the number of call-backs, we need to make sure a sufficient number of pages has been
         * committed.
         */
        unsigned int       accumulated_page_size = 0;
        _memory_block*     block_ptr             = NULL;
        const unsigned int n_blocks              = system_resizable_vector_get_amount_of_elements(memory_blocks);

        for (unsigned int n_block = 0;
                          n_block < n_blocks;
                        ++n_block)
        {
            system_resizable_vector_get_element_at(memory_blocks,
                                                   n_block,
                                                  &block_ptr);

            ASSERT_TRUE((block_ptr->offset_aligned % page_size) == 0);
            ASSERT_TRUE((block_ptr->size           % page_size) == 0);

            accumulated_page_size += block_ptr->size;
        }

        ASSERT_TRUE(accumulated_page_size >= alloced_data_size + alloc_size);

        alloced_data_size += alloc_size;
        n_allocations     ++;
    }

    /* All done */
    _free_memory_blocks();

    system_memory_manager_release(manager);
}

TEST(MemoryManagerTest, AllocReleaseAlloc)
{
    /* This test verifies that released memory blocks are re-usable */
    const unsigned int      required_alignment   = 1;
    const unsigned int      total_available_size = 16384;
    const unsigned int      alloc_size           = total_available_size / 4;
    const unsigned int      page_size            = total_available_size / 16;
    const unsigned int      n_allocable_blocks   = total_available_size / alloc_size;
    system_resizable_vector alloc_offsets        = system_resizable_vector_create(n_allocable_blocks,
                                                                                  sizeof(int) );

    system_memory_manager manager = system_memory_manager_create(total_available_size,           /* memory_region_size */
                                                                 page_size,
                                                                 _mmanager_block_alloc_callback,
                                                                 _mmanager_block_freed_callback,
                                                                 NULL,                           /* user_arg */
                                                                 false);                         /* should_be_thread_safe */

    memory_blocks = system_resizable_vector_create(4, /* capacity */
                                                   sizeof(_memory_block*) );

    /* 1. Alloc blocks of size alloc_size */
    for (unsigned int n_block = 0;
                      n_block < n_allocable_blocks;
                    ++n_block)
    {
        unsigned int alloc_offset = 0;

        bool result = system_memory_manager_alloc_block(manager,
                                                        alloc_size,
                                                        1, /* required_alignment */
                                                       &alloc_offset);

        ASSERT_TRUE(result);
        ASSERT_TRUE(system_resizable_vector_find(alloc_offsets,
                                                 (void*) alloc_offset) == ITEM_NOT_FOUND);

        system_resizable_vector_push(alloc_offsets,
                                     (void*) alloc_offset);
    } /* for (all allocable blocks) */

    /* 2. Free all those blocks */
    unsigned int alloc_offset = 0;

    while (system_resizable_vector_pop(alloc_offsets,
                                      &alloc_offset) )
    {
        system_memory_manager_free_block(manager,
                                         alloc_offset);
    }

    /* 3. Try to alloc all blocks again, as per step 1 */
    for (unsigned int n_block = 0;
                      n_block < n_allocable_blocks;
                    ++n_block)
    {
        unsigned int alloc_offset = 0;

        bool result = system_memory_manager_alloc_block(manager,
                                                        alloc_size,
                                                        1, /* required_alignment */
                                                       &alloc_offset);

        ASSERT_TRUE(result);
    }

    /* All done */
    _free_memory_blocks();

    system_resizable_vector_release(alloc_offsets);
    system_memory_manager_release  (manager);
}

TEST(MemoryManagerTest, OverlappingPageDataDealloc)
{
    /* This test verifies that overlapping pages are deallocated properly.
     *
     * Assumptions:
     *
     * 1) Page size: 4096
     * 2) Allocations:
     *    a) 2048
     *    b) 2048
     *    c) 1024
     *    d) 4096
     *
     * Steps:
     *
     * 1. Allocate the aforementioned regions
     * 2. Deallocate them:
     *
     *    a) All pages should be left intact
     *    b) Page 0 should be released, all other pages should be left untouched.
     *    c) No page should be touched.
     *    d) Page 1 and 2 should be released.
     *
     * 3. Check the call-backs were correct.
     */
    const unsigned int      allocations[] =
    {
        2048,
        2048,
        1024,
        4096
    };
    system_resizable_vector alloc_offsets = system_resizable_vector_create(4, /* capacity */
                                                                           sizeof(unsigned int) );
    const unsigned int      memory_size   = 4096 * 2 + 1024;
    const unsigned int      n_allocations = sizeof(allocations) / sizeof(allocations[0]);
    const unsigned int      page_size     = 4096;

    system_memory_manager manager = system_memory_manager_create(memory_size,
                                                                 page_size,
                                                                 _mmanager_block_alloc_callback,
                                                                 _mmanager_block_freed_callback,
                                                                 NULL,                           /* user_arg */
                                                                 false);                         /* should_be_thread_safe */

    memory_blocks = system_resizable_vector_create(4, /* capacity */
                                                   sizeof(_memory_block*) );

    /* 1. Alloc the blocks */
    for (unsigned int n_block = 0;
                      n_block < n_allocations;
                    ++n_block)
    {
        unsigned int alloc_offset = 0;

        bool result = system_memory_manager_alloc_block(manager,
                                                        allocations[n_block],
                                                        1, /* required_alignment */
                                                       &alloc_offset);

        ASSERT_TRUE(result);

        system_resizable_vector_push(alloc_offsets,
                                     (void*) alloc_offset);
    } /* for (all allocable blocks) */

    /* 2. Free all those blocks */
    unsigned int alloc_offset = 0;

    for (unsigned int n_block = 0;
                      n_block < n_allocations;
                    ++n_block)
    {
        unsigned int alloc_offset = 0;

        system_resizable_vector_get_element_at(alloc_offsets,
                                               n_block,
                                              &alloc_offset);

        system_memory_manager_free_block(manager,
                                         alloc_offset);

        /* Make sure the memory blocks are as per description */
        switch (n_block)
        {
            case 0:
            {
                /* a) All pages should be left intact */
                ASSERT_TRUE(system_resizable_vector_get_amount_of_elements(memory_blocks) == 3);

                break;
            }

            case 1:
            {
                /* b) Page 0 should be released, all other pages should be left untouched. */
                ASSERT_TRUE(system_resizable_vector_get_amount_of_elements(memory_blocks) == 2);

                break;
            }

            case 2:
            {
                /* c) No page should be touched. */
                ASSERT_TRUE(system_resizable_vector_get_amount_of_elements(memory_blocks) == 2);

                break;
            }

            case 3:
            {
                /* d) Page 1 and 2 should be released. */
                ASSERT_TRUE(system_resizable_vector_get_amount_of_elements(memory_blocks) == 0);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid block index");
            }
        } /* switch (n_block) */
    } /* for (all blocks) */


    /* All done */
    _free_memory_blocks();

    system_memory_manager_release(manager);
}