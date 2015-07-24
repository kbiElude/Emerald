/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_threads.h"

#if !defined(USE_EMULATED_EVENTS) && defined(_WIN32)
    #define USE_RAW_HANDLES
#endif

#ifndef USE_RAW_HANDLES
    #include "system/system_event_monitor.h"
#endif

#ifdef __linux__
    #include <string.h>
#endif


typedef struct _system_event
{
    #if defined(USE_RAW_HANDLES)
        HANDLE event;
    #endif

    bool              manual_reset;
    system_thread     owned_thread; /* only relevant for thread events */
    system_event_type type;

    explicit _system_event(system_event_type in_type)
    {
        #if defined(USE_RAW_HANDLES)
            event = NULL;
        #endif

        manual_reset = false;
        type         = in_type;

#ifdef _WIN32
        owned_thread = NULL;
#else
        owned_thread = 0;
#endif
    }
} _system_event;


/** Please see header for specification */
PUBLIC EMERALD_API system_event system_event_create(bool manual_reset)
{
    _system_event* event_ptr = new (std::nothrow) _system_event(SYSTEM_EVENT_TYPE_REGULAR);

    ASSERT_ALWAYS_SYNC(event_ptr != NULL,
                       "Out of memory");

    event_ptr->manual_reset = manual_reset;

#if defined(USE_RAW_HANDLES)
    {
        event_ptr->event = ::CreateEventA(NULL,         /* no special security attributes */
            manual_reset,
            false,        /* bInitialState - if this value is ever changed, update system_event_monitor! */
            0);           /* no name */

        ASSERT_ALWAYS_SYNC(event_ptr->event != NULL,
            "Could not create an event object.");
    }
    #else
    {
        system_event_monitor_add_event( (system_event) event_ptr);
    }
    #endif

    return (system_event) event_ptr;
}

#ifdef _WIN32
    /** Please see header for specification */
    PUBLIC EMERALD_API system_event system_event_create_from_change_notification_handle(HANDLE handle)
    {
        _system_event* event_ptr = new (std::nothrow) _system_event(SYSTEM_EVENT_TYPE_CHANGE_NOTIFICATION_HANDLE);

        ASSERT_ALWAYS_SYNC(event_ptr != NULL,
                           "Out of memory");

        event_ptr->event        = handle;
        event_ptr->manual_reset = true;

        #if !defined(USE_RAW_HANDLES)
        {
            system_event_monitor_add_event( (system_event) event_ptr);
        }
        #endif

        return (system_event) event_ptr;
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API system_event system_event_create_from_thread(system_thread thread)
{
    _system_event* event_ptr = new (std::nothrow) _system_event(SYSTEM_EVENT_TYPE_THREAD);

    ASSERT_ALWAYS_SYNC(event_ptr != NULL,
                       "Out of memory");

    event_ptr->manual_reset = true;

    #if defined(USE_RAW_HANDLES)
    {
        event_ptr->event = thread;
    }
    #else
    {
        event_ptr->owned_thread = thread;

        system_event_monitor_add_event( (system_event) event_ptr);
    }
    #endif

    return (system_event) event_ptr;
}

/** Please see header for specification */
PUBLIC void system_event_get_property(system_event          event,
                                      system_event_property property,
                                      void*                 out_result)
{
    _system_event* event_ptr = (_system_event*) event;

    switch (property)
    {
        case SYSTEM_EVENT_PROPERTY_MANUAL_RESET:
        {
            *(bool*) out_result = event_ptr->manual_reset;

            break;
        }

        case SYSTEM_EVENT_PROPERTY_OWNED_THREAD:
        {
            ASSERT_DEBUG_SYNC(event_ptr->type == SYSTEM_EVENT_TYPE_THREAD,
                              "SYSTEM_EVENT_PROPERTY_OWNED_THREAD property queried for a non-thread event.");

            *(system_thread*) out_result = event_ptr->owned_thread;

            break;
        }

        case SYSTEM_EVENT_PROPERTY_TYPE:
        {
            *(system_event_type*) out_result = event_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_event_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_release(system_event event)
{
    #if defined(USE_RAW_HANDLES)
    {
        if (event != NULL)
        {
            _system_event* event_ptr = (_system_event*) event;

            switch (event_ptr->type)
            {
                case SYSTEM_EVENT_TYPE_CHANGE_NOTIFICATION_HANDLE:
                {
                    ::FindCloseChangeNotification(event_ptr->event);

                    break;
                }

                case SYSTEM_EVENT_TYPE_REGULAR:
                case SYSTEM_EVENT_TYPE_THREAD:
                {
                    ::CloseHandle(event_ptr->event);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized event type");
                }
            }
        } /* if (event != NULL) */
    }
    #else
    {
        system_event_monitor_delete_event(event);
    }
    #endif

    delete (_system_event*) event;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_reset(system_event event)
{
    _system_event* event_ptr = (_system_event*) event;

    if (event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
    {
        #if !defined(USE_RAW_HANDLES)
        {
            system_event_monitor_reset_event(event);
        }
        #else
        {
            ::ResetEvent( ((_system_event*) event)->event);
        }
        #endif
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid event type for a system_event_reset() call");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_set(system_event event)
{
    _system_event* event_ptr = (_system_event*) event;

    if (event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
    {
        #if defined(USE_RAW_HANDLES)
        {
            ::SetEvent( ((_system_event*) event)->event);
        }
        #else
        {
            system_event_monitor_set_event(event);
        }
        #endif
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid event type for a system_event_set() call ");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_event_wait_single_peek(system_event event)
{
    #if !defined(USE_RAW_HANDLES)
    {
        bool has_timed_out = false;

        system_event_monitor_wait(&event,
                                  1,             /* n_events */
                                  false,         /* wait_for_all_events */
                                  0,             /* timeout */
                                 &has_timed_out,
                                  NULL);         /* out_signalled_event_index_ptr */

        return !has_timed_out;
    }
    #else
    {
        return (::WaitForSingleObject( ((_system_event*) event)->event,
                                      0) == WAIT_OBJECT_0);
    }
    #endif
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_event_wait_single(system_event event,
                                                 system_time  timeout)
{
    #if defined(USE_RAW_HANDLES)
    {
        DWORD    result         = 0;
        uint32_t timeout_winapi = 0;

        if (timeout != SYSTEM_TIME_INFINITE)
        {
            system_time_get_msec_for_time(timeout,
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
    #else
    {
        system_event_monitor_wait(&event,
                                  1,       /* n_events */
                                  false,   /* wait_for_all_events */
                                  timeout,
                                  NULL,    /* has_timed_out_ptr */
                                  NULL);   /* out_signalled_event_index_ptr */
    }
    #endif
}

/** Please see header for specification */
PUBLIC EMERALD_API size_t system_event_wait_multiple(const system_event* events,
                                                     int                 n_elements,
                                                     bool                wait_on_all_objects,
                                                     system_time         timeout,
                                                     bool*               out_has_timed_out_ptr)
{
    unsigned int result;

    #if defined(USE_RAW_HANDLES)
    {
        uint32_t timeout_winapi = 0;

        result = WAIT_FAILED;

        if (timeout != SYSTEM_TIME_INFINITE)
        {
            system_time_get_msec_for_time(timeout,
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

                if (out_has_timed_out_ptr != NULL)
                {
                    *out_has_timed_out_ptr = (result == WAIT_TIMEOUT);
                }
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

                    if (result == WAIT_FAILED ||
                        result == WAIT_TIMEOUT)
                    {
                        if (out_has_timed_out_ptr != NULL)
                        {
                            *out_has_timed_out_ptr = (result == WAIT_TIMEOUT);
                        }

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
    #else
    {
        bool has_timed_out = false;

        result = -1;

        if (n_elements == 0)
        {
            result = 0;

            goto end;
        }

        if (wait_on_all_objects)
        {
            ASSERT_DEBUG_SYNC(n_elements < MAXIMUM_WAIT_OBJECTS,
                              "Sanity check failed");

            if (n_elements < MAXIMUM_WAIT_OBJECTS)
            {
                system_event internal_events[MAXIMUM_WAIT_OBJECTS];

                memcpy(internal_events,
                       events,
                       sizeof(system_event) * n_elements);

                system_event_monitor_wait(internal_events,
                                          n_elements,
                                          wait_on_all_objects,
                                          timeout,
                                         &has_timed_out,
                                         &result);

                if (!has_timed_out)
                {
                    result = 0;
                }
            } /* if (n_elements < MAXIMUM_WAIT_OBJECTS) */
            else
            {
                const system_event* current_events = events;
                system_event        internal_events[MAXIMUM_WAIT_OBJECTS];

                while (n_elements > 0)
                {
                    uint32_t n_objects_to_wait_on = (n_elements < MAXIMUM_WAIT_OBJECTS) ? n_elements
                                                                                        : MAXIMUM_WAIT_OBJECTS;

                    memcpy(internal_events,
                           events,
                           sizeof(system_event) * n_elements);

                    system_event_monitor_wait(internal_events,
                                              n_elements,
                                              wait_on_all_objects,
                                              timeout,
                                             &has_timed_out,
                                             &result);

                    if (has_timed_out)
                    {
                        break;
                    }

                    n_elements -= MAXIMUM_WAIT_OBJECTS;
                    events     += MAXIMUM_WAIT_OBJECTS;
                } /* while (n_elements > 0) */
            }
        } /* if (wait_on_all_objects) */
        else
        {
            ASSERT_DEBUG_SYNC(n_elements < MAXIMUM_WAIT_OBJECTS,
                              "Too many events to wait on at once.");

            bool         has_timed_out;
            system_event internal_events[MAXIMUM_WAIT_OBJECTS];

            memcpy(internal_events,
                   events,
                   sizeof(system_event) * n_elements);

            system_event_monitor_wait(internal_events,
                                      n_elements,
                                      wait_on_all_objects,
                                      timeout,
                                     &has_timed_out,
                                     &result);
        }

    end:
        if (out_has_timed_out_ptr != NULL)
        {
            *out_has_timed_out_ptr = has_timed_out;
        }

        return result;
    }
    #endif
}
