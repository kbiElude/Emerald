/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_read_write_mutex.h"
#include "system/system_semaphore.h"
#include "system/system_threads.h"

/* Maximum amount of threads to be accesing the mutex at one go */
#define MAX_SEMAPHORE_COUNT (8)


/* TODO */
struct _read_write_mutex_descriptor
{
    system_critical_section cs_write;
    volatile unsigned int   n_read_locks;
    volatile unsigned int   n_write_locks;
    system_semaphore        semaphore;
    DWORD                   write_owner_thread_id;
};

/** Initializes r/w mutex object 
 *
 *  @param descriptor Mutex to initialize
 */
PRIVATE void _init_read_write_mutex(_read_write_mutex_descriptor* descriptor)
{
    ASSERT_DEBUG_SYNC(descriptor != NULL,
                      "Input descriptor is null");

    if (descriptor != NULL)
    {
        descriptor->cs_write              = system_critical_section_create();
        descriptor->n_read_locks          = 0;
        descriptor->n_write_locks         = 0;
        descriptor->semaphore             = system_semaphore_create(MAX_SEMAPHORE_COUNT);
        descriptor->write_owner_thread_id = 0;
    }
}

/** Frees objects internally used by r/w mutex object.
 *
 *  @param descriptor Mutex to release internal objects for
 */
PRIVATE void _deinit_read_write_mutex(_read_write_mutex_descriptor* descriptor)
{
    ASSERT_DEBUG_SYNC(descriptor != NULL,
                      "Input descriptor is null");

    if (descriptor != NULL)
    {
        system_semaphore_release(descriptor->semaphore);

        system_critical_section_release(descriptor->cs_write);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_read_write_mutex system_read_write_mutex_create()
{
    _read_write_mutex_descriptor* new_descriptor = new _read_write_mutex_descriptor;

    _init_read_write_mutex(new_descriptor);

    return (system_read_write_mutex) new_descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_read_write_mutex_is_write_locked(__in system_read_write_mutex mutex)
{
    return (((_read_write_mutex_descriptor*) mutex)->n_write_locks > 0);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_lock(__in system_read_write_mutex             mutex,
                                                     __in system_read_write_mutex_access_type access_type)
{
    DWORD                         current_thread_id = system_threads_get_thread_id();
    _read_write_mutex_descriptor* descriptor        = (_read_write_mutex_descriptor*) mutex;

    /* Whatever the rqeuested access is, ignore multiple lock requests if the calling thread owns the mtuex for write access */
    bool can_continue = true;

    if (access_type                       == ACCESS_READ        &&
        descriptor->write_owner_thread_id == current_thread_id)
    {
        /* Ignore the request - the caller already owns a write lock. */
        descriptor->n_read_locks++;

        return;
    }

    if (access_type                       == ACCESS_WRITE       &&
        descriptor->write_owner_thread_id == current_thread_id)
    {
        system_critical_section_enter(descriptor->cs_write);
        {
            descriptor->n_write_locks++;
        }
        system_critical_section_leave(descriptor->cs_write);

        return;
    }

    /* If read access request, take one semaphore slot and leave */
    if (access_type == ACCESS_READ)
    {
        system_semaphore_enter(descriptor->semaphore);

        return;
    }
    else
    {
        /* Write access? Eat up all semaphore slots so that no read access request will be passed till caller is done */
        ASSERT_DEBUG_SYNC(access_type == ACCESS_WRITE,
                          "Unsupported access type requested (%d)", access_type);

        system_critical_section_enter(descriptor->cs_write);
        {
            system_semaphore_enter_multiple(descriptor->semaphore,
                                            MAX_SEMAPHORE_COUNT);

            /* At this time only this thread is accessing the descriptor */
            descriptor->write_owner_thread_id = current_thread_id;
            descriptor->n_write_locks         ++;
        }
        system_critical_section_leave(descriptor->cs_write);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_release(__in __deallocate(mem) system_read_write_mutex mutex)
{
    ASSERT_DEBUG_SYNC(mutex != NULL,
                      "Cannot release a NULL mutex");

    if (mutex != NULL)
    {
        _read_write_mutex_descriptor* descriptor = (_read_write_mutex_descriptor*) mutex;

        _deinit_read_write_mutex(descriptor);

        delete mutex;
    }

}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_unlock(__in system_read_write_mutex             mutex,
                                                       __in system_read_write_mutex_access_type access_type)
{
    DWORD                         current_thread_id = system_threads_get_thread_id();
    _read_write_mutex_descriptor* descriptor        = (_read_write_mutex_descriptor*) mutex;

    if (access_type == ACCESS_READ)
    {
        if (descriptor->write_owner_thread_id == current_thread_id)
        {
            ASSERT_DEBUG_SYNC(descriptor->n_read_locks > 0,
                              "Invalid unlock request");

            descriptor->n_read_locks--;
        }
        else
        {
            system_semaphore_leave(descriptor->semaphore);
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(access_type == ACCESS_WRITE,
                          "Cannot unlock - unrecognized access type requested (%d)", access_type);
        ASSERT_DEBUG_SYNC(descriptor->write_owner_thread_id != 0,
                          "Error unlocking mutex - no write owner thread id registered.");
        ASSERT_DEBUG_SYNC(descriptor->write_owner_thread_id == current_thread_id,
                          "Error unlocking mutex - owner thread id is different than the current one.");

        if (descriptor->n_write_locks == 1)
        {
            ASSERT_DEBUG_SYNC(descriptor->n_read_locks == 0,
                              "Read locks acquired when about to release a write lock. Please release all read locks prior to releasing a write lock");

            descriptor->write_owner_thread_id = 0;
            descriptor->n_write_locks         = 0;

            system_semaphore_leave_multiple(descriptor->semaphore,
                                            MAX_SEMAPHORE_COUNT);
        }
        else
        {
            descriptor->n_write_locks --;
        }
    }
}
