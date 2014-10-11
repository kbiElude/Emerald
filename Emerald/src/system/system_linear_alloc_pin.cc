/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_linear_alloc_pin.h"
#include "system/system_log.h"

/** TODO */
struct _system_linear_alloc_pin_pin_descriptor
{
    size_t n_blob;
    void*  next_alloc;  /* at the time of push */
};

/** TODO */
struct _system_linear_alloc_pin_blob_descriptor
{
    void* blob;
    void* last_alloc; /* last alloc available */
    void* next_alloc; /* e <preallocated_blob, preallocated_blob + aligned_entry_size * n_entries> */
};

/** TODO */
struct _system_linear_alloc_pin_descriptor
{
    size_t entry_size;
    size_t aligned_entry_size;

    size_t n_blobs;
    size_t n_current_blob;
    size_t n_entries_per_blob;

    size_t n_pins;
    size_t n_taken_pins;

    _system_linear_alloc_pin_blob_descriptor** blobs;
    _system_linear_alloc_pin_pin_descriptor**  pins;
};

/** TODO */
PRIVATE void _system_linear_free_blobs(_system_linear_alloc_pin_descriptor* descriptor)
{
    for (size_t n_blob = 0; n_blob < descriptor->n_blobs; ++n_blob)
    {
        delete descriptor->blobs[n_blob]->blob;
        delete descriptor->blobs[n_blob];
    }
    delete [] descriptor->blobs;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_linear_alloc_pin_handle system_linear_alloc_pin_create(size_t entry_size,
                                                                                 size_t n_entries_to_prealloc,
                                                                                 size_t n_pins_to_prealloc)
{
    _system_linear_alloc_pin_descriptor* new_descriptor = new (std::nothrow) _system_linear_alloc_pin_descriptor;

    ASSERT_DEBUG_SYNC(new_descriptor != NULL, "Could not allocate a descriptor.");
    if (new_descriptor != NULL)
    {
        new_descriptor->aligned_entry_size    = entry_size + (sizeof(void*) - entry_size % sizeof(void*) );
        new_descriptor->entry_size            = entry_size;
        new_descriptor->n_blobs               = 1;
        new_descriptor->n_current_blob        = 0;
        new_descriptor->n_entries_per_blob    = n_entries_to_prealloc;
        new_descriptor->n_pins                = n_pins_to_prealloc;
        new_descriptor->n_taken_pins          = 0;

        new_descriptor->blobs                 = new (std::nothrow) _system_linear_alloc_pin_blob_descriptor*[1];

        ASSERT_DEBUG_SYNC(new_descriptor != NULL, "Could not allocate blobs table.");
        if (new_descriptor->blobs != NULL)
        {
            new_descriptor->blobs[0] = new (std::nothrow) _system_linear_alloc_pin_blob_descriptor;

            ASSERT_DEBUG_SYNC(new_descriptor->blobs[0] != NULL, "Could not allocate a blob.");
            if (new_descriptor->blobs[0] != NULL)
            {
                /* Alloc the blob */
                _system_linear_alloc_pin_blob_descriptor* blob_descriptor = new_descriptor->blobs[0];

                ASSERT_DEBUG_SYNC(blob_descriptor != NULL, "Could not allocate blob descriptor.");
                if (blob_descriptor != NULL)
                {
                    blob_descriptor->blob       = new (std::nothrow) char [new_descriptor->aligned_entry_size * n_entries_to_prealloc];
                    blob_descriptor->next_alloc =        blob_descriptor->blob;
                    blob_descriptor->last_alloc = (char*)blob_descriptor->blob + (n_entries_to_prealloc - 1) * new_descriptor->aligned_entry_size;
                }

                /* Alloc pins */
                new_descriptor->pins = new (std::nothrow) _system_linear_alloc_pin_pin_descriptor*[n_pins_to_prealloc];

                ASSERT_DEBUG_SYNC(new_descriptor->pins != NULL, "Could not allocate new pins array");
                if (new_descriptor->pins != NULL)
                {
                    new_descriptor->pins[0] = new (std::nothrow) _system_linear_alloc_pin_pin_descriptor;

                    ASSERT_DEBUG_SYNC(new_descriptor->pins[0] != NULL, "Could not allocate pins table.");
                    if (new_descriptor->pins[0] != NULL)
                    {
                        new_descriptor->pins[0]->next_alloc = blob_descriptor->blob;
                        new_descriptor->pins[0]->n_blob     = 0;
                    }
                }
            }
        }
    }

    return (system_linear_alloc_pin_handle) new_descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API void* system_linear_alloc_pin_get_from_pool(system_linear_alloc_pin_handle handle)
{
    _system_linear_alloc_pin_descriptor*      descriptor      = (_system_linear_alloc_pin_descriptor*)      handle;
    _system_linear_alloc_pin_blob_descriptor* blob_descriptor = (_system_linear_alloc_pin_blob_descriptor*) descriptor->blobs[descriptor->n_current_blob];
    void*                                     result          = NULL;

    if (blob_descriptor->next_alloc >= blob_descriptor->last_alloc)
    {
        /* If there are more blobs available, just update the pointers. */
        if (descriptor->n_current_blob + 1 < descriptor->n_blobs)
        {
            LOG_TRACE("Pin-based linear allocator skips to next preallocated blob.");

            descriptor->n_current_blob                               ++;
            descriptor->blobs[descriptor->n_current_blob]->next_alloc = descriptor->blobs[descriptor->n_current_blob]->blob;
        }
        else
        {
            LOG_TRACE("Pin-based linear allocator capacity exceeded! Need to expand.");

            _system_linear_alloc_pin_blob_descriptor* new_blob_descriptor = new (std::nothrow) _system_linear_alloc_pin_blob_descriptor;
            void*                                     new_raw_blob        = new (std::nothrow) char[descriptor->aligned_entry_size * descriptor->n_entries_per_blob];

            ASSERT_DEBUG_SYNC(new_blob_descriptor != NULL && new_raw_blob != NULL, "Could not allocate new blob descriptor / raw blob.");
            if (new_blob_descriptor != NULL && new_raw_blob != NULL)
            {
                /* Got to resize blobs array. */
                _system_linear_alloc_pin_blob_descriptor** new_blobs = new (std::nothrow) _system_linear_alloc_pin_blob_descriptor*[descriptor->n_blobs+1];

                ASSERT_DEBUG_SYNC(new_blobs != NULL, "Could not allocate new blobs array.");
                if (new_blobs != NULL)
                {
                    memcpy(new_blobs, descriptor->blobs, sizeof(void*) * descriptor->n_blobs);

                    /* Insert new blob descriptor */
                    new_blob_descriptor->blob       = new_raw_blob;
                    new_blob_descriptor->last_alloc = (char*) new_raw_blob + (descriptor->n_entries_per_blob - 1) * descriptor->aligned_entry_size;
                    new_blob_descriptor->next_alloc = (char*) new_raw_blob;

                    new_blobs[descriptor->n_blobs] = new_blob_descriptor;
                    descriptor->n_blobs            ++;

                    /* Free former blobs array, leave the descriptors intact. */
                    delete [] descriptor->blobs;

                    descriptor->n_current_blob++;
                    descriptor->blobs = new_blobs;
                    blob_descriptor   = new_blob_descriptor;
                }
            }
        }
    }

    result                      =         blob_descriptor->next_alloc;
    blob_descriptor->next_alloc = (char*) result + descriptor->aligned_entry_size;

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_linear_alloc_pin_mark(system_linear_alloc_pin_handle handle)
{
    _system_linear_alloc_pin_descriptor* descriptor = (_system_linear_alloc_pin_descriptor*) handle;

    ASSERT_DEBUG_SYNC(descriptor->pins != NULL, "Pins array is unavailable.");
    if (descriptor->pins != NULL)
    {
        if (descriptor->n_taken_pins >= descriptor->n_pins)
        {
            /* The hard way.. */
            _system_linear_alloc_pin_pin_descriptor** cached_old_pins      = descriptor->pins;
            size_t                                    cached_old_n_pins    = descriptor->n_pins;
            size_t                                    new_n_pins_allocated = (cached_old_n_pins << 1);
            _system_linear_alloc_pin_pin_descriptor** new_pins             = new (std::nothrow) _system_linear_alloc_pin_pin_descriptor*[new_n_pins_allocated];

            ASSERT_DEBUG_SYNC(new_pins != NULL, "Could not allocate new pins array");
            if (new_pins != NULL)
            {
                /* Zero all pointers */
                memset(new_pins, 0, sizeof(_system_linear_alloc_pin_pin_descriptor*) * new_n_pins_allocated);
                
                /* First insert new pins. We'll copy the former pin pointers later on, because otherwise static analysis
                   gives us crap.
                 */
                for (size_t n_pin = cached_old_n_pins; n_pin < new_n_pins_allocated; ++n_pin)
                {
                    new_pins[n_pin] = new (std::nothrow) _system_linear_alloc_pin_pin_descriptor;

                    ASSERT_DEBUG_SYNC(new_pins[n_pin] != NULL, "Could not allocate new pin descriptor");
                    if (new_pins[n_pin] != NULL)
                    {
                        new_pins[n_pin]->next_alloc = NULL;
                        new_pins[n_pin]->n_blob     = 0xFFFFFFFF;
                    }
                }

                /* Update amonut of pins and pins pointer */
                descriptor->n_pins = new_n_pins_allocated;
                descriptor->pins   = new_pins;

                /* Finally, copy the former pointers */
                memcpy(new_pins, cached_old_pins, sizeof(_system_linear_alloc_pin_pin_descriptor*) * cached_old_n_pins);

                /** Free the former pins array, leave the descriptors intact */
                delete [] cached_old_pins;
            }
        }

        /* Mark a new pin */
        ASSERT_DEBUG_SYNC(descriptor->pins != NULL, "Pins array is NULL");
        if (descriptor->pins != NULL)
        {
            _system_linear_alloc_pin_blob_descriptor* blob_descriptor = descriptor->blobs[descriptor->n_current_blob];
            _system_linear_alloc_pin_pin_descriptor*  pin_descriptor  = descriptor->pins [descriptor->n_taken_pins];

            pin_descriptor->next_alloc = blob_descriptor->next_alloc;
            pin_descriptor->n_blob     = descriptor->n_current_blob;

            descriptor->n_taken_pins++;
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_linear_alloc_pin_release(__deallocate(mem) system_linear_alloc_pin_handle handle)
{
    _system_linear_alloc_pin_descriptor* descriptor = (_system_linear_alloc_pin_descriptor*) handle;

    /* Blobs */
    _system_linear_free_blobs(descriptor);

    descriptor->blobs = NULL;

    /* Pins */
    for (size_t n_pin = 0; n_pin < descriptor->n_pins; ++n_pin)
    {
        delete descriptor->pins[n_pin];
    }
    delete [] descriptor->pins;

    delete  descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_linear_alloc_pin_return_all(system_linear_alloc_pin_handle pin)
{
    _system_linear_alloc_pin_descriptor* pin_ptr = (_system_linear_alloc_pin_descriptor*) pin;

    pin_ptr->n_current_blob = 0;

    for (uint32_t n_blob = 0;
                  n_blob < pin_ptr->n_blobs;
                ++n_blob)
    {
        pin_ptr->blobs[n_blob]->next_alloc = pin_ptr->blobs[n_blob]->blob;
    } /* for (all blobs) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_linear_alloc_pin_unmark(system_linear_alloc_pin_handle handle)
{
    _system_linear_alloc_pin_descriptor* descriptor = (_system_linear_alloc_pin_descriptor*) handle;

    if (descriptor->n_taken_pins > 0)
    {
        _system_linear_alloc_pin_pin_descriptor*  pin_descriptor  = descriptor->pins[descriptor->n_taken_pins - 1];
        _system_linear_alloc_pin_blob_descriptor* blob_descriptor = descriptor->blobs[pin_descriptor->n_blob];

        descriptor->n_current_blob  = pin_descriptor->n_blob;
        blob_descriptor->next_alloc = pin_descriptor->next_alloc;

        descriptor->n_taken_pins--;
    }
}
