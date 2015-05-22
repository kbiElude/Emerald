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
    HANDLE semaphore;
};

/** Please see header for specification */
EMERALD_API system_semaphore system_semaphore_create(__in uint32_t semaphore_capacity)
{
    _system_semaphore_descriptor* new_descriptor = new _system_semaphore_descriptor;

    new_descriptor->semaphore = ::CreateSemaphore(0,                  /* lpSemaphoreAttributes */
                                                  semaphore_capacity, /* lInitialCount */
                                                  semaphore_capacity, /* lMaximumCount */
                                                  0);                 /* lpName */
    
    ASSERT_DEBUG_SYNC(new_descriptor->semaphore != NULL,
                      "Could not create the semaphore");

    return (system_semaphore) new_descriptor;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_release(__in __deallocate(mem) system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
        ::CloseHandle(descriptor->semaphore);
    }

    delete descriptor;
}

/** Please see header for specification */
EMERALD_API void system_semaphore_enter(__in system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
        ::WaitForSingleObject(descriptor->semaphore, INFINITE);
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
            ::WaitForSingleObject(descriptor->semaphore,
                                  INFINITE);
        }
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave(__in system_semaphore semaphore)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
        ::ReleaseSemaphore(descriptor->semaphore,
                           1,  /* lReleaseCount   */
                           0); /* lpPreviousCount */
    }
}

/** Please see header for specification */
EMERALD_API void system_semaphore_leave_multiple(__in system_semaphore semaphore,
                                                 __in uint32_t         count)
{
    _system_semaphore_descriptor* descriptor = (_system_semaphore_descriptor*) semaphore;

    if (descriptor != NULL)
    {
        ::ReleaseSemaphore(descriptor->semaphore,
                           count, /* lReleaseCount   */
                           0);    /* lpPreviousCount */
    }
}
