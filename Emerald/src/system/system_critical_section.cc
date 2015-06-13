/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_threads.h"

/* Internal critical section descriptor */
struct _system_critical_section
{
    CRITICAL_SECTION cs;
    unsigned int     n_enters;
    DWORD            thread_id;

    REFCOUNT_INSERT_VARIABLES
};

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION( system_critical_section,
                                system_critical_section,
                               _system_critical_section);


/** Please see header for specification */
PRIVATE void _system_critical_section_release(__in __deallocate(mem) void* user_arg)
{
    _system_critical_section* cs_ptr = (_system_critical_section*) user_arg;

    ASSERT_DEBUG_SYNC(cs_ptr != NULL,
                      "CS to release is null");

    if (cs_ptr != NULL)
    {
        ::DeleteCriticalSection(&cs_ptr->cs);

        ASSERT_DEBUG_SYNC(cs_ptr->thread_id == 0,
                          "CS taken but about to be released.");
    }
}


/** Please see header for specification */
EMERALD_API system_critical_section system_critical_section_create()
{
    _system_critical_section* cs_ptr = new _system_critical_section;

    ::InitializeCriticalSection(&cs_ptr->cs);

    cs_ptr->n_enters  = 0;
    cs_ptr->thread_id = 0;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(cs_ptr,
                                                   _system_critical_section_release,
                                                   OBJECT_TYPE_SYSTEM_CRITICAL_SECTION,
                                                   NULL);

    return (system_critical_section) cs_ptr;
}

/** TODO */
PUBLIC EMERALD_API void system_critical_section_get_property(__in  system_critical_section          critical_section,
                                                             __in  system_critical_section_property property,
                                                             __out void*                            out_result_ptr)
{
    _system_critical_section* cs_ptr = (_system_critical_section*) critical_section;

    ASSERT_DEBUG_SYNC(critical_section != NULL,
                      "Input argument is NULL");

    switch (property)
    {
        case SYSTEM_CRITICAL_SECTION_PROPERTY_HANDLE:
        {
            *(CRITICAL_SECTION**) out_result_ptr = &((_system_critical_section*) critical_section)->cs;

            break;
        }

        case SYSTEM_CRITICAL_SECTION_PROPERTY_OWNER_THREAD_ID:
        {
            *(DWORD*) out_result_ptr = cs_ptr->thread_id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_critical_section_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
EMERALD_API void system_critical_section_enter(__in system_critical_section critical_section)
{
    system_thread_id          curr_thread_id = system_threads_get_thread_id();
    _system_critical_section* cs_ptr         = (_system_critical_section*) critical_section;
    
    if (cs_ptr != NULL)
    {
        ::EnterCriticalSection(&cs_ptr->cs);

        ASSERT_DEBUG_SYNC(cs_ptr->n_enters == 0 && cs_ptr->thread_id == 0 ||
                          cs_ptr->n_enters != 0,
                          "CS already taken but entered from a different thread!");

        if (cs_ptr->n_enters == 0 && cs_ptr->thread_id == 0 ||
            cs_ptr->n_enters != 0)
        {
            cs_ptr->thread_id = system_threads_get_thread_id();
        }

        cs_ptr->n_enters++;
    }
}

/** Please see header for specification */
EMERALD_API void system_critical_section_leave(__in system_critical_section critical_section)
{
    system_thread_id          curr_thread_id = system_threads_get_thread_id();
    _system_critical_section* cs_ptr         = (_system_critical_section*) critical_section;

    if (cs_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(cs_ptr->n_enters  >  0,
                          "Cannot leave - not entered.");
        ASSERT_DEBUG_SYNC(cs_ptr->thread_id != 0,
                          "CS not taken but about to be left.");
#if 0
        /* This does not work correctly if the CS is used with condition variables */
        ASSERT_DEBUG_SYNC(cs_ptr->thread_id == curr_thread_id,
                          "Leave() request from a non-owner!");
#endif

        if (cs_ptr->n_enters  >  0               &&
            cs_ptr->thread_id == curr_thread_id)
        {
            cs_ptr->n_enters--;

            if (cs_ptr->n_enters == 0)
            {
                cs_ptr->thread_id = 0;
            }

            ::LeaveCriticalSection(&cs_ptr->cs);
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_critical_section_try_enter(__in system_critical_section critical_section)
{
    system_thread_id          curr_thread_id = system_threads_get_thread_id();
    _system_critical_section* cs_ptr         = (_system_critical_section*) critical_section;
    bool                      result         = false;

    if (cs_ptr != NULL)
    {
        if (::TryEnterCriticalSection(&cs_ptr->cs) != 0)
        {
            result = true;

            ASSERT_DEBUG_SYNC(cs_ptr->n_enters == 0 && cs_ptr->thread_id == 0 ||
                              cs_ptr->n_enters != 0,
                              "CS already taken but entered from a different thread!");

            if (cs_ptr->n_enters == 0 && cs_ptr->thread_id == 0 ||
                cs_ptr->n_enters != 0)
            {
                cs_ptr->thread_id = system_threads_get_thread_id();
            }

            cs_ptr->n_enters++;
        }
    }

    return result;
}