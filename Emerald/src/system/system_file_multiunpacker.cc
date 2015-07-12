/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_barrier.h"
#include "system/system_file_multiunpacker.h"
#include "system/system_file_unpacker.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"


typedef struct _system_file_multiunpacker_unpacker
{
    struct _system_file_multiunpacker* owner_ptr;
    system_hashed_ansi_string          packed_filename;
    system_file_unpacker               unpacker;

    explicit _system_file_multiunpacker_unpacker(system_hashed_ansi_string   in_packed_filename,
                                                 _system_file_multiunpacker* in_owner_ptr)
    {
        owner_ptr       = in_owner_ptr;
        packed_filename = in_packed_filename;
        unpacker        = NULL;
    }

    ~_system_file_multiunpacker_unpacker()
    {
        if (unpacker != NULL)
        {
            system_file_unpacker_release(unpacker);

            unpacker = NULL;
        }
    }
} _system_file_multipacker_unpacker;

typedef struct _system_file_multiunpacker
{
    system_barrier          sync_barrier;
    system_resizable_vector unpackers; /* _system_file_multipacker_unpacker */

    explicit _system_file_multiunpacker(unsigned int n_unpackers)
    {
        sync_barrier = system_barrier_create         (n_unpackers);
        unpackers    = system_resizable_vector_create(n_unpackers,
                                                      false); /* should_be_thread_safe */
    }

    ~_system_file_multiunpacker()
    {
        if (sync_barrier != NULL)
        {
            system_barrier_wait_until_signalled(sync_barrier);
            system_barrier_release             (sync_barrier);

            sync_barrier = NULL;
        } /* if (sync_barrier != NULL) */

        if (unpackers != NULL)
        {
            unsigned int n_unpackers = 0;

            system_resizable_vector_get_property(unpackers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_unpackers);

            for (unsigned int n_unpacker = 0;
                              n_unpacker < n_unpackers;
                            ++n_unpacker)
            {
                _system_file_multipacker_unpacker* unpacker_ptr = NULL;

                system_resizable_vector_get_element_at(unpackers,
                                                       n_unpacker,
                                                      &unpacker_ptr);

                ASSERT_DEBUG_SYNC(unpacker_ptr != NULL,
                                  "Could not retrieve a _system_file_multipacker_unpacker instance.");

                delete unpacker_ptr;
                unpacker_ptr = NULL;
            } /* for (all unpackers) */

            system_resizable_vector_release(unpackers);

            unpackers = NULL;
        } /* if (unpackers != NULL) */
    }
} _system_file_multiunpacker;


/** TODO */
PRIVATE volatile void _system_file_multiunpacker_spawn_unpacker_thread_entrypoint(system_thread_pool_callback_argument argument)
{
    _system_file_multiunpacker_unpacker* unpacker_ptr = (_system_file_multiunpacker_unpacker*) argument;

    unpacker_ptr->unpacker = system_file_unpacker_create(unpacker_ptr->packed_filename);

    ASSERT_DEBUG_SYNC(unpacker_ptr->unpacker != NULL,
                      "Could not spawn a system_file_unpacker instance");

    /* Signal the owner barrier upon exit */
    system_barrier_signal(unpacker_ptr->owner_ptr->sync_barrier,
                          false /* wait_until_signalled */);
}


/** Please see header for spec */
PUBLIC EMERALD_API system_file_multiunpacker system_file_multiunpacker_create(const system_hashed_ansi_string* packed_filenames,
                                                                              unsigned int                     n_packed_filenames)
{
    _system_file_multiunpacker* new_multiunpacker_ptr = new (std::nothrow) _system_file_multiunpacker(n_packed_filenames);

    ASSERT_DEBUG_SYNC(new_multiunpacker_ptr != NULL,
                      "Out of memory");

    if (new_multiunpacker_ptr != NULL)
    {
        new_multiunpacker_ptr->sync_barrier = system_barrier_create         (n_packed_filenames);
        new_multiunpacker_ptr->unpackers    = system_resizable_vector_create(n_packed_filenames,
                                                                             true); /* should_be_thread_safe */

        ASSERT_DEBUG_SYNC(new_multiunpacker_ptr->sync_barrier != NULL,
                          "Could not instantiate a sync barrier.");
        ASSERT_DEBUG_SYNC(new_multiunpacker_ptr->unpackers != NULL,
                          "Could not set up a system_file_unpacker vector.");

        /* Set up thread pool tasks */
        for (unsigned int n_packed_filename = 0;
                          n_packed_filename < n_packed_filenames;
                        ++n_packed_filename)
        {
            _system_file_multiunpacker_unpacker* current_unpacker_ptr = new (std::nothrow) _system_file_multiunpacker_unpacker(packed_filenames[n_packed_filename],
                                                                                                                               new_multiunpacker_ptr);
            system_thread_pool_task_descriptor   new_task;

            ASSERT_DEBUG_SYNC(current_unpacker_ptr != NULL,
                              "Out of memory");

            new_task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                              _system_file_multiunpacker_spawn_unpacker_thread_entrypoint,
                                                                              current_unpacker_ptr);

            system_resizable_vector_push         (new_multiunpacker_ptr->unpackers,
                                                  current_unpacker_ptr);
            system_thread_pool_submit_single_task(new_task);
        } /* for (all packed filenames) */
    } /* if (new_multiunpacker_ptr != NULL) */

    return (system_file_multiunpacker) new_multiunpacker_ptr;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_multiunpacker_get_indexed_property(system_file_multiunpacker           multiunpacker,
                                                                       unsigned int                        n_unpacker,
                                                                       _system_file_multiunpacker_property property,
                                                                       void*                               out_result)
{
    _system_file_multiunpacker* multiunpacker_ptr = (_system_file_multiunpacker*) multiunpacker;

    switch (property)
    {
        case SYSTEM_FILE_MULTIUNPACKER_PROPERTY_FILE_UNPACKER:
        {
            _system_file_multipacker_unpacker* unpacker_ptr = NULL;

            if (!system_resizable_vector_get_element_at(multiunpacker_ptr->unpackers,
                                                        n_unpacker,
                                                       &unpacker_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid unpacker index [%d]",
                                  n_unpacker);
            }
            else
            {
                *(system_file_unpacker*) out_result = unpacker_ptr->unpacker;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _system_file_multiunpacker_property value.");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_multiunpacker_release(system_file_multiunpacker multiunpacker)
{
    _system_file_multiunpacker* multiunpacker_ptr = (_system_file_multiunpacker*) multiunpacker;

    system_barrier_wait_until_signalled(multiunpacker_ptr->sync_barrier);

    delete multiunpacker_ptr;
    multiunpacker_ptr = NULL;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_multiunpacker_wait_till_ready(system_file_multiunpacker multiunpacker)
{
    _system_file_multiunpacker* unpacker_ptr = (_system_file_multiunpacker*) multiunpacker;

    system_barrier_wait_until_signalled(unpacker_ptr->sync_barrier);
}
