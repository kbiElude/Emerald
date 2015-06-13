/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_semaphore.h"
#include "system/system_time.h"

/** Internal structure describing a single semaphore object */
struct _system_semaphore
{
#ifdef _WIN32
    HANDLE semaphore;
#else
    sem_t semaphore;
#endif
};

/** Please see header for specification */
EMERALD_API system_semaphore system_semaphore_create(__in uint32_t semaphore_capacity,
                                                     __in uint32_t semaphore_default_value)
{
    _system_semaphore* semaphore_ptr = new _system_semaphore;

    ASSERT_DEBUG_SYNC(semaphore_default_value <= semaphore_capacity,
                      "Invalid semaphore's default value requested.");

#ifdef _WIN32
    semaphore_ptr->semaphore = ::CreateSemaphore(0,                  /* lpSemaphoreAttributes */
                                                 semaphore_default_value,
                                                 semaphore_capacity, /* lMaximumCount */
                                                 0);                 /* lpName */

    ASSERT_DEBUG_SYNC(semaphore_ptr->semaphore != NULL,
                      "Could not create a semaphore");
#else
    int result = sem_init(&semaphore_ptr->semaphore,
                          0, /* pshared */
                          semaphore_capacity);

    ASSERT_DEBUG_SYNC(result == 0,
                      "Could not create a semaphore");

    TODO;
#endif


    return (system_semaphore) semaphore_ptr;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_release(__in __deallocate(mem) system_semaphore semaphore)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
#ifdef _WIN32
        ::CloseHandle(semaphore_ptr->semaphore);
#else
        sem_destroy(&semaphore_ptr->semaphore);
#endif
    }

    delete semaphore_ptr;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter(__in      system_semaphore     semaphore,
                                        __in      system_timeline_time timeout,
                                        __out_opt bool*                out_has_timed_out_ptr)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
        unsigned int timeout_msec = 0;

        if (timeout == SYSTEM_TIME_INFINITE)
        {
            timeout_msec = INFINITE;
        }
        else
        {
            system_time_get_msec_for_timeline_time(timeout,
                                                  &timeout_msec);
        }

#ifdef _WIN32
        DWORD result = ::WaitForSingleObject(semaphore_ptr->semaphore,
                                             timeout_msec);

        if (out_has_timed_out_ptr != NULL)
        {
            *out_has_timed_out_ptr = (result == WAIT_TIMEOUT);
        }
#else
        sem_wait(&semaphore_ptr->semaphore);

        TODO;
#endif
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter_multiple(__in system_semaphore semaphore, __in uint32_t count)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
        for (uint32_t n_release = 0;
                      n_release < count;
                    ++n_release)
        {
#ifdef _WIN32
            ::WaitForSingleObject(semaphore_ptr->semaphore,
                                  INFINITE);
#else
            sem_wait(&semaphore_ptr->semaphore);
#endif
        }
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave(__in system_semaphore semaphore)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
#ifdef _WIN32
        ::ReleaseSemaphore(semaphore_ptr->semaphore,
                           1,     /* lReleaseCount   */
                           NULL); /* lpPreviousCount */
#else
        sem_post(&semaphore_ptr->semaphore);
#endif
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave_multiple(__in system_semaphore semaphore,
                                                 __in uint32_t         count)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
#ifdef _WIN32
        ::ReleaseSemaphore(semaphore_ptr->semaphore,
                           count, /* lReleaseCount   */
                           0);    /* lpPreviousCount */
#else
        /* ?? This should be a safe work-around but.. */
        for (unsigned int n = 0;
                          n < count;
                        ++n)
        {
            sem_post(&semaphore_ptr->semaphore);
        }
#endif
    }
}
