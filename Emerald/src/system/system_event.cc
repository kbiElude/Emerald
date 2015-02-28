/**
 *
 * Emerald (kbi/elude @2012-2014)
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
    DWORD result = ::WaitForSingleObject( (HANDLE) event,
                                          INFINITE);

    ASSERT_DEBUG_SYNC(result != WAIT_FAILED,
                      "WaitForSingleObject() failed.");

}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_event_wait_single_timeout(__in __notnull system_event         event,
                                                                        system_timeline_time timeout)
{
    uint32_t n_secs = 0;
    DWORD    result = 0;

    system_time_get_s_for_timeline_time(timeout, &n_secs);

    result = ::WaitForSingleObject(event, n_secs * 1000);

    ASSERT_DEBUG_SYNC(result != WAIT_FAILED,
                      "WaitForSingleObject() failed.");
    return (result == WAIT_TIMEOUT);
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple_infinite(__in __notnull __ecount(n_elements) const system_event* events,
                                                              __in                                int                 n_elements,
                                                                                                  bool                wait_on_all_objects)
{
    DWORD result = WAIT_FAILED;

    if (n_elements == 0)
    {
        result = WAIT_OBJECT_0;

        goto end;
    }

    if (wait_on_all_objects)
    {
        if (n_elements < MAXIMUM_WAIT_OBJECTS)
        {
            /* We'll use a dirty trick here in order to optimise - system_event only contains a HANDLE so we're perfectly
             * fine to cast every element of events array to a HANDLE.
             */
            result = ::WaitForMultipleObjects( (DWORD) n_elements,
                                               (const HANDLE*) events,
                                               wait_on_all_objects ? TRUE : FALSE,
                                               INFINITE);
        } /* if (n_elements < MAXIMUM_WAIT_OBJECTS) */
        else
        {
            const system_event* current_events = events;

            while (n_elements > 0)
            {
                uint32_t n_objects_to_wait_on = (n_elements < MAXIMUM_WAIT_OBJECTS) ? n_elements
                                                                                    : MAXIMUM_WAIT_OBJECTS;

                result = ::WaitForMultipleObjects( (DWORD) n_objects_to_wait_on,
                                                   (const HANDLE*) events,
                                                   TRUE,
                                                   INFINITE);

                if (result == WAIT_FAILED)
                {
                    goto end;
                }

                n_elements -= MAXIMUM_WAIT_OBJECTS;
                events     += MAXIMUM_WAIT_OBJECTS;
            } /* while (n_elements > 0) */
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(n_elements < MAXIMUM_WAIT_OBJECTS,
                          "Too many events to wait on at once.");

        result = ::WaitForMultipleObjects( (DWORD) n_elements,
                                               (const HANDLE*) events,
                                               wait_on_all_objects ? TRUE : FALSE,
                                               INFINITE);
    }

end:
    ASSERT_DEBUG_SYNC(result != WAIT_FAILED,
                      "WaitForMultipleObjects() failed.");

    return result - WAIT_OBJECT_0;
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple_timeout(__in __notnull __ecount(n_elements) const system_event*  events,
                                                             __in                                int                  n_elements,
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

    ASSERT_DEBUG_SYNC(wait_leave_result != WAIT_FAILED,
                      "WaitForMultipleObjects() failed.");

    return wait_leave_result - WAIT_OBJECT_0;
}
