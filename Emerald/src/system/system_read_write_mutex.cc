/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_read_write_mutex.h"
#include "system/system_threads.h"

#ifdef __linux
    #include <pthread.h>
#endif

/* TODO */
struct _system_read_write_mutex
{
    system_critical_section cs_write;
    volatile unsigned int   n_read_locks;
    volatile unsigned int   n_write_owner_read_locks;
    volatile unsigned int   n_write_locks;
    system_thread_id        write_owner_thread_id;

#ifdef _WIN32
    SRWLOCK                 lock;
#else
    pthread_rwlock_t        lock;
#endif

};


/** Frees objects internally used by r/w mutex object.
 *
 *  @param descriptor Mutex to release internal objects for
 */
PRIVATE void _deinit_read_write_mutex(_system_read_write_mutex* rw_mutex_ptr)
{
    ASSERT_DEBUG_SYNC(rw_mutex_ptr != NULL,
                      "Input RW mutex is null");

    if (rw_mutex_ptr != NULL)
    {
        system_critical_section_release(rw_mutex_ptr->cs_write);

#ifdef _WIN32
#else
        pthread_rwlock_destroy  (&rw_mutex_ptr->lock);
#endif
    }
}

/** Initializes r/w mutex object
 *
 *  @param descriptor Mutex to initialize
 */
PRIVATE void _init_read_write_mutex(_system_read_write_mutex* rw_mutex_ptr)
{
    ASSERT_DEBUG_SYNC(rw_mutex_ptr != NULL,
                      "Input RW mutex is null");

    if (rw_mutex_ptr != NULL)
    {
        rw_mutex_ptr->cs_write                 = system_critical_section_create();
        rw_mutex_ptr->n_write_locks            = 0;
        rw_mutex_ptr->n_write_owner_read_locks = 0;
        rw_mutex_ptr->write_owner_thread_id    = 0;

#ifdef _WIN32
        InitializeSRWLock(&rw_mutex_ptr->lock);
#else
        pthread_rwlock_init(&rw_mutex_ptr->lock,
                            NULL); /* __attr */
#endif
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_read_write_mutex system_read_write_mutex_create()
{
    _system_read_write_mutex* rw_mutex_ptr = new _system_read_write_mutex();

    _init_read_write_mutex(rw_mutex_ptr);

    return (system_read_write_mutex) rw_mutex_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_id system_read_write_mutex_get_write_thread_id(system_read_write_mutex mutex)
{
    _system_read_write_mutex* mutex_ptr = reinterpret_cast<_system_read_write_mutex*>(mutex);

    return mutex_ptr->write_owner_thread_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_read_write_mutex_is_locked(system_read_write_mutex mutex)
{
    _system_read_write_mutex* mutex_ptr = reinterpret_cast<_system_read_write_mutex*>(mutex);

    return mutex_ptr->n_read_locks             > 0 ||
           mutex_ptr->n_write_owner_read_locks > 0 ||
           mutex_ptr->n_write_locks            > 0;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_read_write_mutex_is_write_locked(system_read_write_mutex mutex)
{
    return (((_system_read_write_mutex*) mutex)->n_write_locks > 0);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_lock(system_read_write_mutex             mutex,
                                                     system_read_write_mutex_access_type access_type)
{
    system_thread_id          current_thread_id = system_threads_get_thread_id();
    _system_read_write_mutex* rw_mutex_ptr      = reinterpret_cast<_system_read_write_mutex*>(mutex);

    /* Whatever the requested access is, ignore multiple lock requests if the calling thread owns the mtuex for write access */
    bool can_continue = true;

    system_critical_section_enter(rw_mutex_ptr->cs_write);
    {
        if (access_type                         == ACCESS_READ        &&
            rw_mutex_ptr->write_owner_thread_id == current_thread_id)
        {
            /* Ignore the request - the caller already owns a write lock. */
            rw_mutex_ptr->n_write_owner_read_locks++;

            system_critical_section_leave(rw_mutex_ptr->cs_write);
            return;
        }

        if (access_type                         == ACCESS_WRITE       &&
            rw_mutex_ptr->write_owner_thread_id == current_thread_id)
        {
            rw_mutex_ptr->n_write_locks++;

            system_critical_section_leave(rw_mutex_ptr->cs_write);
            return;
        }
    }
    system_critical_section_leave(rw_mutex_ptr->cs_write);

    if (access_type == ACCESS_READ)
    {
        #ifdef _WIN32
        {
            AcquireSRWLockShared(&rw_mutex_ptr->lock);
        }
        #else
        {
            int result = pthread_rwlock_rdlock(&rw_mutex_ptr->lock);

            ASSERT_DEBUG_SYNC(result == 0,
                              "pthread_rwlock_rdlock() failed.");
        }
        #endif

        ++rw_mutex_ptr->n_read_locks;
        return;
    }
    else
    {
        ASSERT_DEBUG_SYNC(access_type == ACCESS_WRITE,
                          "Unsupported access type requested (%d)", access_type);

        #ifdef _WIN32
        {
            AcquireSRWLockExclusive(&rw_mutex_ptr->lock);

            system_critical_section_enter(rw_mutex_ptr->cs_write);
            {
                /* At this time only this thread is accessing the descriptor */
                rw_mutex_ptr->write_owner_thread_id = current_thread_id;
                rw_mutex_ptr->n_write_locks         ++;
            }
            system_critical_section_leave(rw_mutex_ptr->cs_write);
        }
        #else
        {
            int result = pthread_rwlock_wrlock(&rw_mutex_ptr->lock);

            ASSERT_DEBUG_SYNC(result == 0,
                              "pthread_rwlock_wrlock() failed.");

            /* At this time only this thread is accessing the descriptor */
            system_critical_section_enter(rw_mutex_ptr->cs_write);
            {
                rw_mutex_ptr->write_owner_thread_id = current_thread_id;
                rw_mutex_ptr->n_write_locks         ++;
            }
            system_critical_section_leave(rw_mutex_ptr->cs_write);
        }
        #endif
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_release(system_read_write_mutex mutex)
{
    ASSERT_DEBUG_SYNC(mutex != NULL,
                      "Cannot release a NULL mutex");

    if (mutex != NULL)
    {
        _system_read_write_mutex* rw_mutex_ptr = reinterpret_cast<_system_read_write_mutex*>(mutex);

        _deinit_read_write_mutex(rw_mutex_ptr);

        delete rw_mutex_ptr;
        rw_mutex_ptr = NULL;
    }

}

/** Please see header for specification */
PUBLIC EMERALD_API void system_read_write_mutex_unlock(system_read_write_mutex             mutex,
                                                       system_read_write_mutex_access_type access_type)
{
    system_thread_id          current_thread_id = system_threads_get_thread_id();
    _system_read_write_mutex* rw_mutex_ptr      = reinterpret_cast<_system_read_write_mutex*>(mutex);

    if (access_type == ACCESS_READ)
    {
        if (rw_mutex_ptr->write_owner_thread_id == current_thread_id)
        {
            ASSERT_DEBUG_SYNC(rw_mutex_ptr->n_write_owner_read_locks > 0,
                              "Invalid unlock request");

            rw_mutex_ptr->n_write_owner_read_locks--;
        }
        else
        {
            #ifdef _WIN32
            {
                ReleaseSRWLockShared(&rw_mutex_ptr->lock);
            }
            #else
            {
                pthread_rwlock_unlock(&rw_mutex_ptr->lock);
            }
            #endif

            --rw_mutex_ptr->n_read_locks;
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(access_type == ACCESS_WRITE,
                          "Cannot unlock - unrecognized access type requested (%d)", access_type);
        ASSERT_DEBUG_SYNC(rw_mutex_ptr->write_owner_thread_id != 0,
                          "Error unlocking mutex - no write owner thread id registered.");

        if (rw_mutex_ptr->n_write_locks == 1)
        {
            ASSERT_DEBUG_SYNC(rw_mutex_ptr->n_write_owner_read_locks == 0,
                              "Read locks acquired when about to release a write lock. Please release all read locks prior to releasing a write lock");

            rw_mutex_ptr->write_owner_thread_id = 0;
            rw_mutex_ptr->n_write_locks         = 0;

            #ifdef _WIN32
            {
                ReleaseSRWLockExclusive(&rw_mutex_ptr->lock);
            }
            #else
            {
                pthread_rwlock_unlock(&rw_mutex_ptr->lock);
            }
            #endif
        }
        else
        {
            rw_mutex_ptr->n_write_locks --;
        }
    }
}
