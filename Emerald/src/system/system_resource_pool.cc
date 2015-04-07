/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_linear_alloc_pin.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

/** Internal type definitions */
typedef struct
{
    /* Allocator is used to obtain variants in a cache-line friendly manner.
     *  If a variant is released by caller, it is transferred to released_variants_pool to be
     *  provided for next create() call. Hence, we do not use pins.
     */
    system_linear_alloc_pin allocator;
    /* Critical section is used to make the implementation multithread-safe */
    system_critical_section cs;
    /* Please see system_resource_pool_create() documentation for details. */
    PFNSYSTEMRESOURCEPOOLDEINITBLOCK deinit_block_fn;
    /* Please see system_resource_pool_create() documentation for details. */
    PFNSYSTEMRESOURCEPOOLINITBLOCK init_block_fn;

    /* Released variants pool is sued to store already allocated blocks that have been released
     *  by the caller. They will be returned to the caller the next create() call. Only if the pool
     *  runs out of elements, should we obtain new blocks from the pool
     */
    system_resizable_vector released_blocks;
} _system_resource_pool_internals;


/** Please see header for specification */
PUBLIC EMERALD_API system_resource_pool system_resource_pool_create(__in size_t                           element_size,
                                                                    __in size_t                           n_elements_per_blob,
                                                                         PFNSYSTEMRESOURCEPOOLINITBLOCK   init_block_fn,
                                                                         PFNSYSTEMRESOURCEPOOLDEINITBLOCK deinit_block_fn)
{
    _system_resource_pool_internals* result = new (std::nothrow) _system_resource_pool_internals;

    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Could not create resource pool.");

    if (result != NULL)
    {
        result->allocator       = system_linear_alloc_pin_create(element_size,
                                                                 n_elements_per_blob,
                                                                 1); /* n_pins_to_prealloc */
        result->cs              = system_critical_section_create();
        result->deinit_block_fn = deinit_block_fn;
        result->init_block_fn   = init_block_fn;
        result->released_blocks = system_resizable_vector_create(n_elements_per_blob,
                                                                 sizeof(void*) );

        ASSERT_ALWAYS_SYNC(result->allocator != NULL,
                           "Allocator could not have been created.");
        ASSERT_ALWAYS_SYNC(result->cs != NULL,
                           "Could not create critical section");
        ASSERT_ALWAYS_SYNC(result->released_blocks != NULL,
                           "Released blocks vector could not have been created.");
    }
    
    return (system_resource_pool) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_resource_pool_block system_resource_pool_get_from_pool(__in __notnull system_resource_pool pool)
{
    _system_resource_pool_internals* descriptor = (_system_resource_pool_internals*) pool;
    system_resource_pool_block       result     = NULL;

    system_critical_section_enter(descriptor->cs);
    {
        unsigned int n_cached_released_blocks = system_resizable_vector_get_amount_of_elements(descriptor->released_blocks);

        if (n_cached_released_blocks > 0)
        {
            bool result_get = system_resizable_vector_pop(descriptor->released_blocks,
                                                         &result);

            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve resource pool block.");
        }
        else
        {
            result = (system_resource_pool_block) system_linear_alloc_pin_get_from_pool(descriptor->allocator);

            /* Init the block if a call-back function pointer was provided. */
            if (descriptor->init_block_fn != NULL)
            {
                descriptor->init_block_fn(result);
            }
        }
    }
    system_critical_section_leave(descriptor->cs);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "NULL blob returned by system_resource_pool");

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resource_pool_return_all_allocations(__in __notnull system_resource_pool pool)
{
    _system_resource_pool_internals* pool_ptr = (_system_resource_pool_internals*) pool;

    system_critical_section_enter(pool_ptr->cs);
    {
        if (pool_ptr->deinit_block_fn == NULL)
        {
            /* Faster code-path */
            system_linear_alloc_pin_return_all(pool_ptr->allocator);
        }
        else
        {
            /** TODO */
            ASSERT_ALWAYS_SYNC(false,
                               "Unsupported code-path");
        }
    }
    system_critical_section_leave(pool_ptr->cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resource_pool_return_to_pool(__in __notnull system_resource_pool       pool,
                                                            __in __notnull system_resource_pool_block block)
{
    _system_resource_pool_internals* descriptor = (_system_resource_pool_internals*) pool;

    ASSERT_DEBUG_SYNC(block != NULL,
                      "NULL block returned to the pool!");
    system_critical_section_enter(descriptor->cs);
    {
        system_resizable_vector_push(descriptor->released_blocks,
                                     block);
    }
    system_critical_section_leave(descriptor->cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_resource_pool_release(__in __notnull __deallocate(mem) system_resource_pool pool)
{
    _system_resource_pool_internals* descriptor = (_system_resource_pool_internals*) pool;

    ASSERT_ALWAYS_SYNC(descriptor != NULL,
                       "Pool cannot be null");

    if (descriptor != NULL)
    {
        /* Deinit all returned blocks before we continue */
        if (descriptor->deinit_block_fn != NULL)
        {
            unsigned int n_elements = system_resizable_vector_get_amount_of_elements(descriptor->released_blocks);

            for (unsigned int n_element = 0;
                              n_element < n_elements;
                            ++n_element)
            {
                system_resource_pool_block item       = NULL;
                bool                       result_get = false;

                result_get = system_resizable_vector_get_element_at(descriptor->released_blocks,
                                                                    n_element,
                                                                   &item);

                ASSERT_DEBUG_SYNC(result_get,
                                  "Could not retrieve block item to release.");

                descriptor->deinit_block_fn(item);
            }
        }

        /* Okay, carry on. */
        system_linear_alloc_pin_release(descriptor->allocator);
        system_critical_section_release(descriptor->cs);
        system_resizable_vector_release(descriptor->released_blocks);

        delete descriptor;
    }
}