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

#ifdef __linux__
    #include <pthread.h>
#endif


/* Internal critical section descriptor */
struct _system_critical_section
{
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t cs;
#endif

    unsigned int     n_enters;
    system_thread_id thread_id;

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
#ifdef _WIN32
        ::DeleteCriticalSection(&cs_ptr->cs);
#else
        int result = pthread_mutex_destroy(&cs_ptr->cs);

        ASSERT_DEBUG_SYNC(result == 0,
                          "pthread_mutex_destroy() call failed.");
#endif

        ASSERT_DEBUG_SYNC(cs_ptr->thread_id == 0,
                          "CS taken but about to be released.");
    }
}


/** Please see header for specification */
EMERALD_API system_critical_section system_critical_section_create()
{
    _system_critical_section* cs_ptr = new _system_critical_section;

#ifdef _WIN32
    ::InitializeCriticalSection(&cs_ptr->cs);
#else
    pthread_mutexattr_t mutex_attributes;
    int                 result;

    result = pthread_mutexattr_init(&mutex_attributes);
    ASSERT_DEBUG_SYNC(result == 0,
                      "pthread_mutexattr_init() call failed.");

    pthread_mutexattr_settype(&mutex_attributes,
                               PTHREAD_MUTEX_RECURSIVE);

    result = pthread_mutex_init(&cs_ptr->cs,
                                &mutex_attributes);

    ASSERT_DEBUG_SYNC(result == 0,
                      "pthread_mutex_init() call failed.");

    pthread_mutexattr_destroy(&mutex_attributes);
#endif

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
#ifdef _WIN32
            *(CRITICAL_SECTION**) out_result_ptr = &((_system_critical_section*) critical_section)->cs;
#else
            *(pthread_mutex_t**) out_result_ptr = &((_system_critical_section*) critical_section)->cs;
#endif

            break;
        }

        case SYSTEM_CRITICAL_SECTION_PROPERTY_OWNER_THREAD_ID:
        {
            *(system_thread_id*) out_result_ptr = cs_ptr->thread_id;

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
#ifdef _WIN32
        ::EnterCriticalSection(&cs_ptr->cs);
#else
        int result = pthread_mutex_lock(&cs_ptr->cs);

        ASSERT_DEBUG_SYNC(result == 0,
                          "pthread_mutex_lock() call failed.");
#endif

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

#ifdef _WIN32
            ::LeaveCriticalSection(&cs_ptr->cs);
#else
            int result = pthread_mutex_unlock(&cs_ptr->cs);

            ASSERT_DEBUG_SYNC(result == 0,
                              "pthread_mutex_unlock() call failed.");
#endif
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
#ifdef _WIN32
        if (::TryEnterCriticalSection(&cs_ptr->cs) != 0)
#else
        if (pthread_mutex_trylock(&cs_ptr->cs) == 0)
#endif
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