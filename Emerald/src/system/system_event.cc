/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_event.h"

typedef enum
{
    /* Created by system_event_create() */
    SYSTEM_EVENT_TYPE_REGULAR,
    /* Created by system_event_create_from_thread() */
    SYSTEM_EVENT_TYPE_THREAD,
} _system_event_type;

typedef struct _system_event
{
#ifdef _WIN32
    HANDLE event;
#else
    /* TODO */
#endif
    _system_event_type type;

    explicit _system_event(__in _system_event_type in_type)
    {
        event = NULL;
        type  = in_type;
    }
} _system_event;


/** Please see header for specification */
PUBLIC EMERALD_API __maybenull system_event system_event_create(bool manual_reset)
{
    _system_event* event_ptr = new (std::nothrow) _system_event(SYSTEM_EVENT_TYPE_REGULAR);

    ASSERT_ALWAYS_SYNC(event_ptr != NULL,
                       "Out of memory");

    event_ptr->event = ::CreateEventA(NULL,         /* no special security attributes */
                                      manual_reset,
                                      false,        /* bInitialState */
                                      0);           /* no name */

    ASSERT_ALWAYS_SYNC(event_ptr->event != NULL,
                       "Could not create an event object.");

    return (system_event) event_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API __maybenull system_event system_event_create_from_thread(__in system_thread thread)
{
    _system_event* event_ptr = new (std::nothrow) _system_event(SYSTEM_EVENT_TYPE_THREAD);

    ASSERT_ALWAYS_SYNC(event_ptr != NULL,
                       "Out of memory");

    event_ptr->event = thread;

    return (system_event) event_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_release(__in __notnull __deallocate(mem) system_event event)
{
    if (event != NULL)
    {
        HANDLE descriptor = ((_system_event*) event)->event;

        ::CloseHandle(descriptor);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_reset(__in __notnull system_event event)
{
    _system_event* event_ptr = (_system_event*) event;

    if (event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
    {
        ::ResetEvent( ((_system_event*) event)->event);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid event type for a system_event_reset() call");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_set(__in __notnull system_event event)
{
    _system_event* event_ptr = (_system_event*) event;

    if (event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
    {
        ::SetEvent( ((_system_event*) event)->event);
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid event type for a system_event_set() call ");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_event_wait_single_peek(__in __notnull system_event event)
{
    return (::WaitForSingleObject( ((_system_event*) event)->event,
                                  0) == WAIT_OBJECT_0);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_wait_single(__in __notnull system_event         event,
                                                 __in           system_timeline_time timeout)
{
    DWORD    result         = 0;
    uint32_t timeout_winapi = 0;

    if (timeout != SYSTEM_TIME_INFINITE)
    {
        system_time_get_msec_for_timeline_time(timeout,
                                              &timeout_winapi);
    }
    else
    {
        timeout_winapi = INFINITE;
    }

    ::WaitForSingleObject( ((_system_event*) event)->event,
                           timeout_winapi);

    ASSERT_DEBUG_SYNC(result != WAIT_FAILED,
                      "WaitForSingleObject() failed.");

}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple(__in __notnull __ecount(n_elements) const system_event*  events,
                                                     __in                                int                  n_elements,
                                                                                         bool                 wait_on_all_objects,
                                                                                         system_timeline_time timeout,
                                                     __out __notnull                     bool*                out_result_ptr)
{
    DWORD    result         = WAIT_FAILED;
    uint32_t timeout_winapi = 0;

    if (timeout != SYSTEM_TIME_INFINITE)
    {
        system_time_get_msec_for_timeline_time(timeout,
                                              &timeout_winapi);
    }
    else
    {
        timeout_winapi = INFINITE;
    }

    if (n_elements == 0)
    {
        result = WAIT_OBJECT_0;

        goto end;
    }

    if (wait_on_all_objects)
    {
        ASSERT_DEBUG_SYNC(n_elements < MAXIMUM_WAIT_OBJECTS,
                          "Sanity check failed");

        if (n_elements < MAXIMUM_WAIT_OBJECTS)
        {
            HANDLE internal_events[MAXIMUM_WAIT_OBJECTS];

            for (unsigned int n_event = 0;
                              n_event < n_elements;
                            ++n_event)
            {
                internal_events[n_event] = ((_system_event*) events[n_event])->event;
            }

            result = ::WaitForMultipleObjects( (DWORD) n_elements,
                                               internal_events,
                                               wait_on_all_objects ? TRUE : FALSE,
                                               timeout_winapi);
        } /* if (n_elements < MAXIMUM_WAIT_OBJECTS) */
        else
        {
            const system_event* current_events = events;
            HANDLE              internal_events[MAXIMUM_WAIT_OBJECTS];

            while (n_elements > 0)
            {
                uint32_t n_objects_to_wait_on = (n_elements < MAXIMUM_WAIT_OBJECTS) ? n_elements
                                                                                    : MAXIMUM_WAIT_OBJECTS;

                for (unsigned int n_event = 0;
                                  n_event < n_objects_to_wait_on;
                                ++n_event)
                {
                    internal_events[n_event] = ((_system_event*) events[n_event])->event;
                }

                result = ::WaitForMultipleObjects( (DWORD) n_objects_to_wait_on,
                                                   internal_events,
                                                   TRUE,
                                                   timeout_winapi);

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

        HANDLE internal_events[MAXIMUM_WAIT_OBJECTS];

        for (unsigned int n_event = 0;
                          n_event < n_elements;
                        ++n_event)
        {
            internal_events[n_event] = ((_system_event*) events[n_event])->event;
        }

        result = ::WaitForMultipleObjects((DWORD) n_elements,
                                          internal_events,
                                          wait_on_all_objects ? TRUE : FALSE,
                                          timeout_winapi);
    }

end:
    ASSERT_DEBUG_SYNC(result != WAIT_FAILED,
                      "WaitForMultipleObjects() failed (%u).",
                      ::GetLastError() );

    return result - WAIT_OBJECT_0;
}
