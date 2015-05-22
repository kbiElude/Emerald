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

/* Internal 64-bit hash-map bin entry descriptor */
struct _system_hash64map_bin_entry_descriptor
{
    void*                                         element;
    system_hash64                                 hash;
    PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC        remove_callback;
    _system_hash64map_on_remove_callback_argument remove_callback_argument;
};

/* Internal 64-bit hash-map descriptor */
struct _system_hash64map_descriptor
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
PRIVATE void _system_hash64map_bin_entry_init_descriptor(_system_hash64map_bin_entry_descriptor*       descriptor,
                                                         system_hash64                                 hash,
                                                         void*                                         element,
                                                         PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC        remove_callback,
                                                         _system_hash64map_on_remove_callback_argument remove_callback_argument)
{
    descriptor->element                  = element;
    descriptor->hash                     = hash;
    descriptor->remove_callback          = remove_callback;
    descriptor->remove_callback_argument = remove_callback_argument;
}

/** TODO */
PRIVATE void _system_hash64map_init_descriptor(_system_hash64map_descriptor* descriptor,
                                               uint32_t                      n_bins,
                                               size_t                        element_size,
                                               bool                          should_be_thread_safe)
{
    descriptor->access_mutex                   = NULL;
    descriptor->any_entry_used_remove_callback = false;
    descriptor->bin_entry_pool                 = system_resource_pool_create(sizeof(_system_hash64map_bin_entry_descriptor),
                                                                             128 /* n-elements */,
                                                                             NULL /* init_fn */,
                                                                             NULL /* deinit_fn */);
    descriptor->bins                           = new system_resizable_vector[n_bins];
    descriptor->element_size                   = element_size;
    descriptor->n_bins                         = n_bins;

    for (uint32_t n_bin = 0;
                  n_bin < n_bins;
                ++n_bin)
    {
        descriptor->bins[n_bin] = system_resizable_vector_create(HASH64MAP_BIN_ENTRIES_AMOUNT);
    }

    if (should_be_thread_safe)
    {
        descriptor->access_mutex = system_read_write_mutex_create();
    }
}

/** Releases a 64-bit hash-map bin descriptor. Do not access the descriptor after calling this function.
 *
 *  @param pool_ptr   TODO
 *  @param descriptor Descriptor to release.
 **/
PRIVATE void _system_hash64map_bin_entry_deinit_descriptor(_system_hash64map_descriptor*           pool_ptr,
                                                           _system_hash64map_bin_entry_descriptor* descriptor)
{
    if (descriptor->remove_callback != NULL)
    {
        descriptor->remove_callback(descriptor->remove_callback_argument);
    }

    system_resource_pool_return_to_pool(pool_ptr->bin_entry_pool,
                                        (system_resource_pool_block) descriptor);
}

/** Releases a 64-bit hash-map descriptor. Do not access the descriptor after calling this function.
 *
 *  @param descriptor Descriptor to release.
 **/
PRIVATE void _system_hash64map_deinit_descriptor(_system_hash64map_descriptor* descriptor)
{
    for (uint32_t n_bin = 0;
                  n_bin < descriptor->n_bins;
                ++n_bin)
    {
        system_resizable_vector_release(descriptor->bins[n_bin]);
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_release(descriptor->access_mutex);

        descriptor->access_mutex = NULL;
    }

    if (descriptor->bins != NULL)
    {
        delete [] descriptor->bins;

        descriptor->bins = NULL;
    }

    if (descriptor->bin_entry_pool)
    {
        system_resource_pool_release(descriptor->bin_entry_pool);
    }

    delete descriptor;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_hash64map_clear(__in __notnull system_hash64map map)
{
    _system_hash64map_descriptor* map_ptr = (_system_hash64map_descriptor*) map;

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_lock(map_ptr->access_mutex,
                                     ACCESS_WRITE);
    }

    if (map_ptr->any_entry_used_remove_callback)
    {
        /** TODO */
        ASSERT_ALWAYS_SYNC(false,
                           "Unsupported code-path");
    }
    else
    {
        /* Since no entry uses remove call-back, we can just tell the resource pool to reset! */
        system_resource_pool_return_all_allocations(map_ptr->bin_entry_pool);

        /* Clear all bins. This should be fast since we only need to reset vector descriptors */
        for (uint32_t n_bin = 0;
                      n_bin < map_ptr->n_bins;
                    ++n_bin)
        {
            system_resizable_vector_clear(map_ptr->bins[n_bin]);
        } /* for (all bins) */
    }

    if (map_ptr->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(map_ptr->access_mutex,
                                       ACCESS_WRITE);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hash64map system_hash64map_create(__in size_t element_size,
                                                            __in bool   should_be_thread_safe)
{
    _system_hash64map_descriptor* new_descriptor = new _system_hash64map_descriptor;

    _system_hash64map_init_descriptor(new_descriptor,
                                      HASH64MAP_BINS_AMOUNT,
                                      element_size,
                                      should_be_thread_safe);

    /* Return the descriptor */
    return (system_hash64map) new_descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_contains(__in system_hash64map hash_map,
                                                  __in system_hash64    hash)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) hash_map;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % descriptor->n_bins;
    system_resizable_vector bin               = descriptor->bins[hash_index];
    uint32_t                n_vector_elements = system_resizable_vector_get_amount_of_elements(bin);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry_descriptor* element    = NULL;
        bool                                    result_get = false;

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

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_get(__in      system_hash64map map,
                                             __in      system_hash64    hash,
                                             __out_opt void*            result_element)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) map;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % descriptor->n_bins;
    system_resizable_vector bin               = descriptor->bins[hash_index];
    uint32_t                n_vector_elements = system_resizable_vector_get_amount_of_elements(bin);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry_descriptor* bin_entry_descriptor = NULL;
        bool                                    result_get           = system_resizable_vector_get_element_at(bin,
                                                                                                              n_element,
                                                                                                             &bin_entry_descriptor);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retreive bin entry descriptor.");

        if (bin_entry_descriptor->hash == hash)
        {
            if (result_element != NULL)
            {
                memcpy(result_element,
                      &bin_entry_descriptor->element,
                       descriptor->element_size);
            }

            result = true;
            break;
        }
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_hash64map_get_amount_of_elements(__in system_hash64map map)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) map;
    size_t                        result     = 0;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    for (uint32_t n_bin = 0;
                  n_bin < descriptor->n_bins;
                ++n_bin)
    {
        result += system_resizable_vector_get_amount_of_elements(descriptor->bins[n_bin]);
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_get_element_at(__in      system_hash64map map,
                                                                  size_t           n_element,
                                                        __out_opt void*            result_element,
                                                        __out_opt system_hash64*   result_hash)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) map;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_READ);
    }

    uint32_t n_elements_counted = 0;
    bool     result             = false;

    for (uint32_t n_bin = 0;
                  n_bin < descriptor->n_bins;
                ++n_bin)
    {
        uint32_t n_elements_in_bin = system_resizable_vector_get_amount_of_elements(descriptor->bins[n_bin]);

        if (n_elements_counted + n_elements_in_bin > n_element)
        {
            // The element is located in this bin.
            _system_hash64map_bin_entry_descriptor* bin_entry_descriptor = NULL;
            bool                                    result_get           = false;

            result_get = system_resizable_vector_get_element_at(descriptor->bins[n_bin],
                                                                n_element - n_elements_counted,
                                                               &bin_entry_descriptor);

            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve hash64map bin descriptor.");

            if (result_element != NULL)
            {
                memcpy(result_element,
                      &bin_entry_descriptor->element,
                       descriptor->element_size);
            }

            if (result_hash != NULL)
            {
                *result_hash = bin_entry_descriptor->hash;
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

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_is_equal(__in system_hash64map map_1,
                                                  __in system_hash64map map_2)
{
    _system_hash64map_descriptor* descriptor_1 = (_system_hash64map_descriptor*) map_1;
    _system_hash64map_descriptor* descriptor_2 = (_system_hash64map_descriptor*) map_2;
    bool                          result       = false;

    if (descriptor_1->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor_1->access_mutex,
                                     ACCESS_READ);
    }
    if (descriptor_2->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor_2->access_mutex,
                                     ACCESS_READ);
    }

    if (descriptor_1->element_size == descriptor_2->element_size &&
        descriptor_1->n_bins       == descriptor_2->n_bins)
    {
        result = true;

        for (uint32_t n_bin = 0;
                      n_bin < descriptor_1->n_bins && result;
                    ++n_bin)
        {
            result = system_resizable_vector_is_equal(descriptor_1->bins[n_bin],
                                                      descriptor_2->bins[n_bin]);
        }
    }

    if (descriptor_1->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor_1->access_mutex,
                                       ACCESS_READ);
    }
    if (descriptor_2->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor_2->access_mutex,
                                       ACCESS_READ);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_insert(__in             system_hash64map                              map,
                                                __in             system_hash64                                 hash,
                                                __in_opt         void*                                         element,
                                                __in __maybenull PFNSYSTEMHASH64MAPONREMOVECALLBACKPROC        callback,
                                                __in __maybenull _system_hash64map_on_remove_callback_argument callback_argument)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) map;

    /* Lock rw mutex */
    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    uint32_t                hash_index = hash % descriptor->n_bins;
    system_resizable_vector bin        = descriptor->bins[hash_index];

    /* Create bin entry descriptor */
    _system_hash64map_bin_entry_descriptor* new_bin_entry_descriptor = NULL;

    new_bin_entry_descriptor = (_system_hash64map_bin_entry_descriptor*) system_resource_pool_get_from_pool(descriptor->bin_entry_pool);

    _system_hash64map_bin_entry_init_descriptor(new_bin_entry_descriptor,
                                                hash,
                                                element,
                                                callback,
                                                callback_argument);

    /* In debug mode, before storing the entry, verify such element has not already
     * been stored. */
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
        descriptor->any_entry_used_remove_callback = true;
    }

    system_resizable_vector_push(bin,
                                 new_bin_entry_descriptor);

    /* Unlock rw mutex */
    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }

    return true;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_lock(__in system_hash64map                    map,
                                              __in system_read_write_mutex_access_type lock_type)
{
    _system_hash64map_descriptor* map_ptr = (_system_hash64map_descriptor*) map;

    ASSERT_DEBUG_SYNC(map_ptr->access_mutex != NULL,
                      "system_hash64map not created in a MT-safe mode.");

    system_read_write_mutex_lock( ((_system_hash64map_descriptor*) map)->access_mutex,
                                  lock_type);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hash64map_remove(__in system_hash64map map,
                                                __in system_hash64    hash)
{
    _system_hash64map_descriptor* descriptor = (_system_hash64map_descriptor*) map;

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_lock(descriptor->access_mutex,
                                     ACCESS_WRITE);
    }

    bool                    result            = false;
    uint32_t                hash_index        = hash % descriptor->n_bins;
    system_resizable_vector bin               = descriptor->bins[hash_index];
    uint32_t                n_vector_elements = system_resizable_vector_get_amount_of_elements(bin);

    for (uint32_t n_element = 0;
                  n_element < n_vector_elements;
                ++n_element)
    {
        _system_hash64map_bin_entry_descriptor* element_descriptor = NULL;
        bool                                    result_get         = false;

        result_get = system_resizable_vector_get_element_at(bin,
                                                            n_element,
                                                           &element_descriptor);

        ASSERT_DEBUG_SYNC(result_get,
                          "Could not retrieve hash64map bin entry descriptor.");

        if (element_descriptor->hash == hash)
        {
            result = system_resizable_vector_delete_element_at(bin,
                                                               n_element);

            _system_hash64map_bin_entry_deinit_descriptor(descriptor,
                                                          element_descriptor);

            break;
        }
    }

    if (descriptor->access_mutex != NULL)
    {
        system_read_write_mutex_unlock(descriptor->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_release(__in __deallocate(mem) system_hash64map map)
{
    _system_hash64map_deinit_descriptor( (_system_hash64map_descriptor*) map);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_hash64map_unlock(__in system_hash64map                    map,
                                                __in system_read_write_mutex_access_type lock_type)
{
    _system_hash64map_descriptor* map_ptr = (_system_hash64map_descriptor*) map;

    ASSERT_DEBUG_SYNC(map_ptr->access_mutex != NULL,
                      "system_hash64map not created in a MT-safe mode.");

    system_read_write_mutex_unlock( ((_system_hash64map_descriptor*) map)->access_mutex,
                                    lock_type);
}
