/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_assertions.h"
#include "system/system_threads.h"

/* Internal critical section descriptor */
struct _critical_section_descriptor
{
    CRITICAL_SECTION cs;
    unsigned int     n_enters;
    DWORD            thread_id;
};

/** Please see header for specification */
EMERALD_API system_critical_section system_critical_section_create()
{
    _critical_section_descriptor* descriptor = new _critical_section_descriptor;

    ::InitializeCriticalSection(&descriptor->cs);

    descriptor->n_enters  = 0;
    descriptor->thread_id = 0;

    return (system_critical_section) descriptor;
}

/** TODO */
PUBLIC EMERALD_API system_thread_id system_critical_section_get_owner(__in system_critical_section critical_section)
{
    _critical_section_descriptor* descriptor = (_critical_section_descriptor*) critical_section;
    system_thread_id              result     = 0;

    ASSERT_DEBUG_SYNC(descriptor != NULL, "CS to release is null");
    if (descriptor != NULL)
    {
        result = descriptor->thread_id;
    }

    return result;
}

/** Please see header for specification */
EMERALD_API void system_critical_section_release(__in __deallocate(mem) system_critical_section critical_section)
{
    _critical_section_descriptor* descriptor = (_critical_section_descriptor*) critical_section;

    ASSERT_DEBUG_SYNC(descriptor != NULL, "CS to release is null");
    if (descriptor != NULL)
    {
        ::DeleteCriticalSection(&descriptor->cs);

        ASSERT_DEBUG_SYNC(descriptor->thread_id == 0, "CS taken but about to be released.");
        delete descriptor;
    }
}

/** Please see header for specification */
EMERALD_API void system_critical_section_enter(__in system_critical_section critical_section)
{
    system_thread_id              curr_thread_id = system_threads_get_thread_id();
    _critical_section_descriptor* descriptor     = (_critical_section_descriptor*) critical_section;
    
    if (descriptor != NULL)
    {
        ::EnterCriticalSection(&descriptor->cs);

        ASSERT_DEBUG_SYNC(descriptor->n_enters == 0 && descriptor->thread_id == 0 ||
                          descriptor->n_enters != 0,
                          "CS already taken but entered from a different thread!");

        if (descriptor->n_enters == 0 && descriptor->thread_id == 0 ||
            descriptor->n_enters != 0)
        {
            descriptor->thread_id = system_threads_get_thread_id();
        }

        descriptor->n_enters++;
    }
}

/** Please see header for specification */
EMERALD_API void system_critical_section_leave(__in system_critical_section critical_section)
{
    system_thread_id              curr_thread_id = system_threads_get_thread_id();
    _critical_section_descriptor* descriptor     = (_critical_section_descriptor*) critical_section;

    if (descriptor != NULL)
    {
        ASSERT_DEBUG_SYNC(descriptor->n_enters  >  0,              "Cannot leave - not entered.");
        ASSERT_DEBUG_SYNC(descriptor->thread_id != 0,              "CS not taken but about to be left.");
        ASSERT_DEBUG_SYNC(descriptor->thread_id == curr_thread_id, "Leave() request from a non-owner!");

        if (descriptor->n_enters > 0 && descriptor->thread_id == curr_thread_id)
        {
            descriptor->n_enters--;

            if (descriptor->n_enters == 0)
            {
                descriptor->thread_id = 0;
            }

            ::LeaveCriticalSection(&descriptor->cs);
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_critical_section_try_enter(__in system_critical_section critical_section)
{
    system_thread_id              curr_thread_id = system_threads_get_thread_id();
    _critical_section_descriptor* descriptor     = (_critical_section_descriptor*) critical_section;
    bool                          result         = false;

    if (descriptor != NULL)
    {
        if (::TryEnterCriticalSection(&descriptor->cs) == TRUE)
        {
            result = true;

            ASSERT_DEBUG_SYNC(descriptor->n_enters == 0 && descriptor->thread_id == 0 ||
                              descriptor->n_enters != 0,
                              "CS already taken but entered from a different thread!");

            if (descriptor->n_enters == 0 && descriptor->thread_id == 0 ||
                descriptor->n_enters != 0)
            {
                descriptor->thread_id = system_threads_get_thread_id();
            }

            descriptor->n_enters++;
        }
    }

    return result;
}