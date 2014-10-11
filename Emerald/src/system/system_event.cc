/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_time.h"

/** Please see header for specification */
PUBLIC EMERALD_API __maybenull system_event system_event_create(bool manual_reset, bool start_state)
{
    HANDLE result = ::CreateEventA(NULL,         /* no special security attributes */
                                   manual_reset, 
                                   start_state, 
                                   0);           /* no name */

    ASSERT_ALWAYS_SYNC(result != NULL, "Could not create an event object.");

    return (system_event) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_release(__in __notnull __deallocate(mem) system_event event)
{
    if (event != NULL)
    {
        HANDLE descriptor = (HANDLE) event;

        ::CloseHandle(descriptor);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_reset(__in __notnull system_event event)
{
    ::ResetEvent( (HANDLE) event);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_set(__in __notnull system_event event)
{
    ::SetEvent( (HANDLE) event);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_event_wait_single_peek(__in __notnull system_event event)
{
    return (::WaitForSingleObject( (HANDLE) event, 0) == WAIT_OBJECT_0);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_wait_single_infinite(__in __notnull system_event event)
{
    ::WaitForSingleObject( (HANDLE) event, INFINITE);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_event_wait_single_timeout(__in __notnull system_event event, system_timeline_time timeout)
{
    uint32_t n_secs = 0;

    system_time_get_s_for_timeline_time(timeout, &n_secs);

    return (::WaitForSingleObject(event, n_secs * 1000) == WAIT_TIMEOUT);
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple_infinite(__in __notnull __ecount(n_elements) const system_event* events,
                                                              __in                                size_t              n_elements,
                                                                                                  bool                wait_on_all_objects)
{
    /* We'll use a dirty trick here in order to optimise - system_event only contains a HANDLE so we're perfectly
     * fine to cast every element of events array to a HANDLE.
     */
    return (::WaitForMultipleObjects( (DWORD) n_elements,
                                      (const HANDLE*) events,
                                      wait_on_all_objects ? TRUE : FALSE,
                                      INFINITE) - WAIT_OBJECT_0);
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple_timeout(__in __notnull __ecount(n_elements) const system_event*  events,
                                                             __in                                size_t               n_elements,
                                                                                                 bool                 wait_on_all_objects,
                                                                                                 system_timeline_time timeout,
                                                             __out __notnull                     bool*                out_result)
{
    /* We'll use a dirty trick here in order to optimise - system_event only contains a HANDLE so we're perfectly
     * fine to cast every element of events array to a HANDLE.
     */
    uint32_t n_secs            = 0;
    DWORD    wait_leave_result = 0;

    system_time_get_s_for_timeline_time(timeout, &n_secs);

    wait_leave_result = ::WaitForMultipleObjects( (DWORD) n_elements,
                                                  (const HANDLE*) events,
                                                  wait_on_all_objects ? TRUE : FALSE,
                                                  n_secs * 1000);

    if (wait_leave_result == WAIT_TIMEOUT)
    {
        *out_result = true;
    }
    else
    {
        *out_result = false;
    }

    return wait_leave_result - WAIT_OBJECT_0;
}
