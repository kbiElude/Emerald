/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_semaphore.h"
#include "system/system_time.h"

#ifdef __linux
    #include <errno.h>
    #include <semaphore.h>
#endif


/** Internal structure describing a single semaphore object */
struct _system_semaphore
{
    system_critical_section cs; /* needed for multiple enter support */

#ifdef _WIN32
    HANDLE semaphore;
#else
    sem_t semaphore;
#endif
};

/** Please see header for specification */
EMERALD_API system_semaphore system_semaphore_create(uint32_t semaphore_capacity,
                                                     uint32_t semaphore_default_value)
{
    _system_semaphore* semaphore_ptr = new _system_semaphore;

    ASSERT_DEBUG_SYNC(semaphore_capacity <= MAX_SEMAPHORE_CAPACITY,
                      "Invalid semaphore capacity requested.");
    ASSERT_DEBUG_SYNC(semaphore_default_value <= semaphore_capacity,
                      "Invalid semaphore's default value requested.");

    semaphore_ptr->cs = system_critical_section_create();

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

    /* Configure the semaphore to the user-specified value */
    int32_t diff = semaphore_capacity - semaphore_default_value;

    for (int32_t n = 0;
                 n < diff;
               ++n)
    {
        int result = sem_wait(&semaphore_ptr->semaphore);

        ASSERT_DEBUG_SYNC(result == 0,
                          "sem_wait() failed");
    }
#endif


    return (system_semaphore) semaphore_ptr;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter(system_semaphore semaphore,
                                        system_time      timeout,
                                        bool*            out_has_timed_out_ptr)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
#ifdef _WIN32
        DWORD        result;
        unsigned int timeout_msec = 0;

        if (timeout == SYSTEM_TIME_INFINITE)
        {
            timeout_msec = INFINITE;
        }
        else
        {
            system_time_get_msec_for_time(timeout,
                                         &timeout_msec);
        }

        system_critical_section_enter(semaphore_ptr->cs);
        {
            result = ::WaitForSingleObject(semaphore_ptr->semaphore,
                                           timeout_msec);
        }
        system_critical_section_leave(semaphore_ptr->cs);

        if (out_has_timed_out_ptr != NULL)
        {
            *out_has_timed_out_ptr = (result == WAIT_TIMEOUT);
        }
#else
        if (timeout == SYSTEM_TIME_INFINITE)
        {
            system_critical_section_enter(semaphore_ptr->cs);
            {
                sem_wait(&semaphore_ptr->semaphore);
            }
            system_critical_section_leave(semaphore_ptr->cs);
        }
        else
        {
            int             result;
            struct timespec timeout_api;
            unsigned int    timeout_msec = 0;

            system_time_get_msec_for_time(timeout,
                                         &timeout_msec);

            timeout_api.tv_sec  = timeout_msec / 1000;
            timeout_api.tv_nsec = long(timeout_msec % 1000) * NSEC_PER_SEC;

            system_critical_section_enter(semaphore_ptr->cs);
            {
                result = sem_timedwait(&semaphore_ptr->semaphore,
                                       &timeout_api);
            }
            system_critical_section_leave(semaphore_ptr->cs);

            if (result == -1        &&
                errno  == ETIMEDOUT)
            {
                if (out_has_timed_out_ptr != NULL)
                {
                    *out_has_timed_out_ptr = true;
                }
            }
            else
            {
                if (out_has_timed_out_ptr != NULL)
                {
                    *out_has_timed_out_ptr = false;
                }
            }
        }
#endif
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter_multiple(system_semaphore semaphore,
                                                 uint32_t         count)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
        system_critical_section_enter(semaphore_ptr->cs);
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
        system_critical_section_leave(semaphore_ptr->cs);
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave(system_semaphore semaphore)
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
EMERALD_API void system_semaphore_leave_multiple(system_semaphore semaphore,
                                                 uint32_t         count)
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

/** Please see header for specification */
EMERALD_API void system_semaphore_release(system_semaphore semaphore)
{
    _system_semaphore* semaphore_ptr = (_system_semaphore*) semaphore;

    if (semaphore_ptr != NULL)
    {
        system_critical_section_release(semaphore_ptr->cs);

#ifdef _WIN32
        ::CloseHandle(semaphore_ptr->semaphore);
#else
        sem_destroy(&semaphore_ptr->semaphore);
#endif
    }

    delete semaphore_ptr;
}
