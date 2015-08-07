/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_hash64map.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include <string.h>


/* Internal 64-bit hash-map bin entry descriptor */
struct _system_hash64map_bin_entry
{
    void*                                  element;
    system_hash64                          hash;
    PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC remove_callback;
    void*                                  remove_callback_argument;
};

/* Internal 64-bit hash-map descriptor */
struct _system_hash64map
{
    system_resizable_vector* bins;
    size_t                   element_size;
    uint32_t                 n_bins;

    system_read_write_mutex  access_mutex;
    system_resource_pool     bin_entry_pool;

    /** Helper for faster clear() code-path */
    bool                     any_entry_used_remove_callback;
};

/** TODO */
PRIVATE void _system_hash64map_bin_entry_init(_system_hash64map_bin_entry*           entry_ptr,
                                              system_hash64                          hash,
                                              void*                                  element,
                                              PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC remove_callback,
                                              void*                                  remove_callback_argument)
{
    entry_ptr->element                  = element;
    entry_ptr->hash                     = hash;
    entry_ptr->remove_callback          = remove_callback;
    entry_ptr->remove_callback_argument = remove_callback_argument;
}

/** TODO */
PRIVATE void _system_hash64map_init(_system_hash64map* hash64map_ptr,
                                    uint32_t           n_bins,
                                    size_t             element_size,
                                    bool               should_be_thread_safe)
{
    hash64map_ptr->access_mutex                   = NULL;
    hash64map_ptr->any_entry_used_remove_callback = false;
    hash64map_ptr->bin_entry_pool                 = system_resource_pool_create(sizeof(_system_hash64map_bin_entry),
                                                                                128 /* n-elements */,
                                                                                NULL /* init_fn */,
                                                                                NULL /* deinit_fn */);
    hash64map_ptr->bins                           = new system_resizable_vector[n_bins];
    hash64map_ptr->element_size                   = element_size;
    hash64map_ptr->n_bins                         = n_bins;

    for (uint32_t n_bin = 0;
                  n_bin < n_bins;
                ++n_bin)
    {
        hash64map_ptr->bins[n_bin] = system_resizable_vector_create(HASH64MAP_BIN_ENTRIES_AMOUNT);
    }

    if (should_be_thread_safe)
    {
        hash64map_ptr->access_mutex = system_read_write_mutex_create();
    }
}

/** Releases a 64-bit hash-map bin descriptor. Do not access the descriptor after calling this function.
 *
 *  @param pool_ptr   TODO
 *  @param descriptor Descriptor to release.
 **/
PRIVATE void _system_hash64map_bin_entry_deinit(_system_hash64map*           hash64map_ptr,
                                                _system_hash64map_bin_entry* entry_ptr)
{
    if (entry_ptr->remove_callback != NULL)
    {
        entry_ptr->remove_callback(entry_ptr->remove_callback_argument);
    }

    system_resource_pool_return_to_pool(hash64map_ptr->bin_entry_pool,
                                        (system_resource_pool_block) entry_ptr);
}

/** Releases a 64-bit hash-map descriptor. Do not access the descriptor after calling this function.
 *
 *  @param descriptor Descriptor to release.
 **/
PRIVATE void _system_hash64map_deinit(_system_hash64map* hash64map_ptr)
{
    for (uint32_t n_bin = 0;
                  n_bin < hash64map_ptr->n_bins;
                ++n_bin)
    {
        system_resizable_vector_release(hash64map_ptr->bins[n_bin]);
    }

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_release(hash64map_ptr->access_mutex);

        hash64map_ptr->access_mutex = NULL;
    }

    if (hash64map_ptr->bins != NULL)
    {
        delete [] hash64map_ptr->bins;

        hash64map_ptr->bins = NULL;
    }

    if (hash64map_ptr->bin_entry_pool)
    {
        system_resource_pool_release(hash64map_ptr->bin_entry_pool);

        hash64map_ptr->bin_entry_pool = NULL;
    }

    delete hash64map_ptr;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_hash64map_clear(system_hash64map map)
{
    _system_hash64map* map_ptr = (_system_hash64map*) map;

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(map_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    for (uint32_t n_bin = 0;
                  n_bin < map_ptr->n_bins;
                ++n_bin)
    {
        uint32_t n_bin_elements = 0;

        if (map_ptr->any_entry_used_remove_callback)
        {
            system_resizable_vector_get_property(map_ptr->bins[n_bin],
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_bin_elements);

            for (uint32_t n_element = 0;
                          n_element < n_bin_elements;
                        ++n_element)
            {
                _system_hash64map_bin_entry* entry_ptr = NULL;

                system_resizable_vector_get_element_at(map_ptr->bins[n_bin],
                                                       n_element,
                                                      &entry_ptr);

                if (entry_ptr->remove_callback != NULL)
                {
                    entry_ptr->remove_callback(entry_ptr->remove_callback_argument);
                }
            }
        }

        system_resizable_vector_clear(map_ptr->bins[n_bin]);
    } /* for (all bins) */

    /* Since no entry uses remove call-back, we can just tell the resource pool to reset! */
    system_resource_pool_return_all_allocations(map_ptr->bin_entry_pool);

    map_ptr->any_entry_used_remove_callback = false;

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(map_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hash64map system_hash64map_create(size_t element_size,
                                                            bool   should_be_thread_safe)
{
    _system_hash64map* hash64map_ptr = new _system_hash64map;

    _system_hash64map_init(hash64map_ptr,
                           HASH64MAP_BINS_AMOUNT,
                           element_size,
                           should_be_thread_safe);

    /* Return the descriptor */
    return (system_hash64map) hash64map_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_contains(system_hash64map hash_map,
                                                  system_hash64    hash)
{
    _system_hash64map* hash64map_ptr = (_system_hash64map*) hash_map;

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(hash64map_ptr->access_mutex,
                                     ACCESS_READ);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % hash64map_ptr->n_bins;
    system_resizable_vector bin               = hash64map_ptr->bins[hash_index];
    uint32_t                n_vector_elements = 0;

    system_resizable_vector_get_property(bin,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_vector_elements);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry* element    = NULL;
        bool                         result_get = false;

        result_get = system_resizable_vector_get_element_at(bin,
                                                            n_element,
                                                           &element);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve hash64map descriptor.");

        if (element->hash == hash)
        {
            result = true;

            break;
        }
    }

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(hash64map_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_get(system_hash64map map,
                                             system_hash64    hash,
                                             void*            result_element_ptr)
{
    _system_hash64map* hash64map_ptr = (_system_hash64map*) map;

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(hash64map_ptr->access_mutex,
                                     ACCESS_READ);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % hash64map_ptr->n_bins;
    system_resizable_vector bin               = hash64map_ptr->bins[hash_index];
    uint32_t                n_vector_elements = 0;

    system_resizable_vector_get_property(bin,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_vector_elements);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry* bin_entry_descriptor = NULL;
        bool                         result_get           = system_resizable_vector_get_element_at(bin,
                                                                                                   n_element,
                                                                                                  &bin_entry_descriptor);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retreive bin entry descriptor.");

        if (bin_entry_descriptor->hash == hash)
        {
            if (result_element_ptr != NULL)
            {
                memcpy(result_element_ptr,
                      &bin_entry_descriptor->element,
                       hash64map_ptr->element_size);
            }

            result = true;
            break;
        }
    }

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(hash64map_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_get_element_at(system_hash64map map,
                                                        size_t           n_element,
                                                        void*            result_element_ptr,
                                                        system_hash64*   result_hash_ptr)
{
    _system_hash64map* hash64map_ptr = (_system_hash64map*) map;

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(hash64map_ptr->access_mutex,
                                     ACCESS_READ);
    }

    uint32_t n_elements_counted = 0;
    bool     result             = false;

    for (uint32_t n_bin = 0;
                  n_bin < hash64map_ptr->n_bins;
                ++n_bin)
    {
        uint32_t n_elements_in_bin = 0;

        system_resizable_vector_get_property(hash64map_ptr->bins[n_bin],
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_elements_in_bin);

        if (n_elements_counted + n_elements_in_bin > n_element)
        {
            // The element is located in this bin.
            _system_hash64map_bin_entry* bin_entry_ptr = NULL;
            bool                         result_get    = false;

            result_get = system_resizable_vector_get_element_at(hash64map_ptr->bins[n_bin],
                                                                n_element - n_elements_counted,
                                                               &bin_entry_ptr);

            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve hash64map bin descriptor.");

            if (result_element_ptr != NULL)
            {
                memcpy(result_element_ptr,
                      &bin_entry_ptr->element,
                       hash64map_ptr->element_size);
            }

            if (result_hash_ptr != NULL)
            {
                *result_hash_ptr = bin_entry_ptr->hash;
            }

            result = true;
            break;
        }
        else
        {
            // The element is not in the bin.
            n_elements_counted += n_elements_in_bin;
        }
    }

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(hash64map_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_is_equal(system_hash64map map_1,
                                                  system_hash64map map_2)
{
    _system_hash64map* map_1_ptr = (_system_hash64map*) map_1;
    _system_hash64map* map_2_ptr = (_system_hash64map*) map_2;
    bool               result       = false;

    if (map_1_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(map_1_ptr->access_mutex,
                                     ACCESS_READ);
    }
    if (map_2_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(map_2_ptr->access_mutex,
                                     ACCESS_READ);
    }

    if (map_1_ptr->element_size == map_2_ptr->element_size &&
        map_1_ptr->n_bins       == map_2_ptr->n_bins)
    {
        result = true;

        for (uint32_t n_bin = 0;
                      n_bin < map_1_ptr->n_bins && result;
                    ++n_bin)
        {
            result = system_resizable_vector_is_equal(map_1_ptr->bins[n_bin],
                                                      map_2_ptr->bins[n_bin]);
        }
    }

    if (map_1_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(map_1_ptr->access_mutex,
                                       ACCESS_READ);
    }
    if (map_2_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(map_2_ptr->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_get_property(system_hash64map          map,
                                                      system_hash64map_property property,
                                                      void*                     out_result_ptr)
{
    _system_hash64map* map_ptr = (_system_hash64map*) map;

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(map_ptr->access_mutex,
                                     ACCESS_READ);
    }

    switch (property)
    {
        case SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS:
        {
            uint32_t result = 0;

            for (uint32_t n_bin = 0;
                          n_bin < map_ptr->n_bins;
                        ++n_bin)
            {
                unsigned int temp = 0;

                system_resizable_vector_get_property(map_ptr->bins[n_bin],
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &temp);
                result += temp;
            }

            *(uint32_t*) out_result_ptr = result;
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_hash64map_property value");
        }
    } /* switch (property) */

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(map_ptr->access_mutex,
                                       ACCESS_READ);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_insert(system_hash64map                       map,
                                                system_hash64                          hash,
                                                void*                                  element,
                                                PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC callback,
                                                void*                                  callback_argument)
{
    _system_hash64map* hash64map_ptr = (_system_hash64map*) map;

    /* Lock rw mutex */
    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(hash64map_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    uint32_t                hash_index = hash % hash64map_ptr->n_bins;
    system_resizable_vector bin        = hash64map_ptr->bins[hash_index];

    /* Create bin entry descriptor */
    _system_hash64map_bin_entry* new_bin_entry_ptr = NULL;

    new_bin_entry_ptr = (_system_hash64map_bin_entry*) system_resource_pool_get_from_pool(hash64map_ptr->bin_entry_pool);

    _system_hash64map_bin_entry_init(new_bin_entry_ptr,
                                     hash,
                                     element,
                                     callback,
                                     callback_argument);

    /* In debug mode, before storing the entry, verify such element has not already
     * been stored.
     *
     * NOTE: The check below has been disabled owing to significant cost at execution
     *       time. Only enable for debugging purposes.
     */
#if 0
    #ifdef _DEBUG
    {
        const uint32_t n_items = system_resizable_vector_get_amount_of_elements(bin);

        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
        {
            _system_hash64map_bin_entry_descriptor* item_ptr = NULL;

            if (!system_resizable_vector_get_element_at(bin, n_item, &item_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve requested item from the hash masp bin");
            }

            ASSERT_DEBUG_SYNC(item_ptr->hash != hash,
                              "The hash-map already stores item at user-requested index.");
        } /* for (all items) */
    }
    #endif
#endif

    /* Store the entry */
    if (callback != NULL)
    {
        hash64map_ptr->any_entry_used_remove_callback = true;
    }

    system_resizable_vector_push(bin,
                                 new_bin_entry_ptr);

    /* Unlock rw mutex */
    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(hash64map_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_lock(system_hash64map                    map,
                                              system_read_write_mutex_access_type lock_type)
{
    _system_hash64map* map_ptr = (_system_hash64map*) map;

    ASSERT_DEBUG_SYNC(map_ptr->access_mutex != NULL,
                      "system_hash64map not created in a MT-safe mode.");

    system_read_write_mutex_lock( ((_system_hash64map*) map)->access_mutex,
                                  lock_type);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_remove( system_hash64map map,
                                                 system_hash64    hash)
{
    _system_hash64map* hash64map_ptr = (_system_hash64map*) map;

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(hash64map_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % hash64map_ptr->n_bins;
    system_resizable_vector bin               = hash64map_ptr->bins[hash_index];
    uint32_t                n_vector_elements = 0;

    system_resizable_vector_get_property(bin,
                                          SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         &n_vector_elements);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry* element_ptr = NULL;
        bool                         result_get  = false;

        result_get = system_resizable_vector_get_element_at(bin,
                                                            n_element,
                                                           &element_ptr);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve hash64map bin entry descriptor.");

        if (element_ptr->hash == hash)
        {
            result = system_resizable_vector_delete_element_at(bin,
                                                               n_element);

            _system_hash64map_bin_entry_deinit(hash64map_ptr,
                                               element_ptr);

            break;
        }
    }

    if (hash64map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(hash64map_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_release(system_hash64map map)
{
    _system_hash64map_deinit( (_system_hash64map*) map);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_unlock(system_hash64map                    map,
                                                system_read_write_mutex_access_type lock_type)
{
    _system_hash64map* map_ptr = (_system_hash64map*) map;

    ASSERT_DEBUG_SYNC(map_ptr->access_mutex != NULL,
                      "system_hash64map not created in a MT-safe mode.");

    system_read_write_mutex_unlock( ((_system_hash64map*) map)->access_mutex,
                                    lock_type);
}
