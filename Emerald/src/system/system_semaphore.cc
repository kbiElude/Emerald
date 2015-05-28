/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_semaphore.h"

/** Internal structure describing a single semaphore object */
struct _system_semaphore_descriptor
{
#ifdef _WIN32
    HANDLE semaphore;
#else
    sem_t semaphore;
#endif
};

/** Please see header for specification */
EMERALD_API system_semaphore system_semaphore_create(__in uint32_t semaphore_capacity)
{
    _system_semaphore_descriptor* new_descriptor = new _system_semaphore_descriptor;

#ifdef _WIN32
    new_descriptor->semaphore = ::CreateSemaphore(0,                  /* lpSemaphoreAttributes */
                                                  semaphore_capacity, /* lInitialCount */
                                                  semaphore_capacity, /* lMaximumCount */
                                                  0);                 /* lpName */

    ASSERT_DEBUG_SYNC(new_descriptor->semaphore != NULL,
                      "Could not create a semaphore");
#else
    int result = sem_init(&new_descriptor->semaphore,
                          0, /* pshared */
                          semaphore_capacity);

    ASSERT_DEBUG_SYNC(result == 0,
                      "Could not create a semaphore");
#endif


    return (system_semaphore) new_descriptor;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_release(__in __deallocate(mem) system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
#ifdef _WIN32
        ::CloseHandle(descriptor->semaphore);
#else
        sem_destroy(&descriptor->semaphore);
#endif
    }

    delete descriptor;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter(__in system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
#ifdef _WIN32
        ::WaitForSingleObject(descriptor->semaphore,
                              INFINITE);
#else
        sem_wait(&descriptor->semaphore);
#endif
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter_multiple(__in system_semaphore semaphore, __in uint32_t count)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
        for (uint32_t n_release = 0;
                      n_release < count;
                    ++n_release)
        {
#ifdef _WIN32
            ::WaitForSingleObject(descriptor->semaphore,
                                  INFINITE);
#else
            sem_wait(&descriptor->semaphore);
#endif
        }
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave(__in system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
#ifdef _WIN32
        ::ReleaseSemaphore(descriptor->semaphore,
                           1,  /* lReleaseCount   */
                           0); /* lpPreviousCount */
#else
        sem_post(&descriptor->semaphore);
#endif
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave_multiple(__in system_semaphore semaphore,
                                                 __in uint32_t         count)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
#ifdef _WIN32
        ::ReleaseSemaphore(descriptor->semaphore,
                           count, /* lReleaseCount   */
                           0);    /* lpPreviousCount */
#else
        /* ?? This should be a safe work-around but.. */
        for (unsigned int n = 0;
                          n < count;
                        ++n)
        {
            sem_post(&descriptor->semaphore);
        }
#endif
    }
}
