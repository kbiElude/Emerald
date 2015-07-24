/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_EVENT_H
#define SYSTEM_EVENT_H

#include "system/system_types.h"
#include "system/system_time.h"

typedef enum
{
    #ifdef _WIN32
        /* HANDLE.
         *
         * Internal usage only. Only valid for change notification events.
         */
        SYSTEM_EVENT_PROPERTY_CHANGE_NOTIFICATION_HANDLE,
    #endif

    /* bool */
    SYSTEM_EVENT_PROPERTY_MANUAL_RESET,

    /* system_thread.
     *
     * Only valid if event type == SYSTEM_EVENT_TYPE_THREAD
     */
    SYSTEM_EVENT_PROPERTY_OWNED_THREAD,

    /* system_event_type */
    SYSTEM_EVENT_PROPERTY_TYPE
} system_event_property;

typedef enum
{
    /* Created by system_event_create() */
    SYSTEM_EVENT_TYPE_REGULAR,
    /* Created by system_event_create_from_thread() */
    SYSTEM_EVENT_TYPE_THREAD,

    #ifdef _WIN32
        /* Created by system_event_create_from_change_notification_handle(). */
        SYSTEM_EVENT_TYPE_CHANGE_NOTIFICATION_HANDLE,
    #endif

    SYSTEM_EVENT_TYPE_UNDEFINED
} system_event_type;

/** Creates an event synchronisation object.
 *
 *  All threads waiting on the event will block, until the event is "set"
 *  by another thread.
 *
 *  @param bool True to reset the object with each finished wait() request, false to make it resettable only
 *              by means of calling system_event_reset() func.
 *
 *  @return Event handle object.
 */
PUBLIC EMERALD_API system_event system_event_create(bool manual_reset);

#ifdef _WIN32
    /** TODO */
    PUBLIC EMERALD_API system_event system_event_create_from_change_notification_handle(HANDLE handle);
#endif

/** Creates an event synchronization object, wrapping a thread. The event will be signalled, the moment
 *  the thread dies.
 *
 *  Calling system_event_set() or system_event_reset() on the event will throw assertion failures, and will
 *  have not change the event's signal status.
 *
 *  @return Event handle object.
 */
PUBLIC EMERALD_API system_event system_event_create_from_thread(system_thread thread);

/** TODO
 *
 *  Internal use only.
 */
PUBLIC void system_event_get_property(system_event          event,
                                      system_event_property property,
                                      void*                 out_result);

/** Releases an event synchronisation object.
 *
 *  @param system_event Event object to release. Can be NULL.
 */
PUBLIC EMERALD_API void system_event_release(system_event event);

/** Resets an event object.
 *
 *  @param system_event Event object.
 */
PUBLIC EMERALD_API void system_event_reset(system_event event);

/** Sets an event object.
 *
 *  @param system_event Event object.
 */
PUBLIC EMERALD_API void system_event_set(system_event event);

/** Function that waits on a single event object for a very short period of time and reports whether the wait()
 *  would block.
 *
 *  If function returns false and the event object was configured for automatic reset, the event can be considered
 *  to have resetted itself!
 *
 *  @param system_event_handle Event object handle.
 *
 *  @return true if the wait() would not block, false otherwise.
 */
PUBLIC EMERALD_API bool system_event_wait_single_peek(system_event event);

/** Waits infinitely on a single event object.
 *
 *  @paran system_event Event object to wait on.
 */
PUBLIC EMERALD_API void system_event_wait_single(system_event event,
                                                 system_time  timeout = SYSTEM_TIME_INFINITE);

/** Waits on multiple objects until user-defined amount of time passes. Function can either block till one of the events become available or
 *  till all of them are free.
 *
 *  @param system_event*        Array to wait on
 *  @param int                  Amount of elements in the array. Can't be negative
 *  @param bool                 True to wait on all objects, false to wait on only one object.
 *  @param system_time Time-out.
 *  @param timeout_occured      Deref set to true if time-out occured. Otherwise set to false.
 *
 *  @return Index of the event causing wait operation to unblock, if @param bool is false. Otherwise undetermined.
 */
PUBLIC EMERALD_API size_t system_event_wait_multiple(const system_event* events,
                                                     int                 n_elements,
                                                     bool                wait_on_all_objects,
                                                     system_time         timeout,
                                                     bool*               out_has_timed_out_ptr);

#endif /* SYSTEM_EVENT_H */
