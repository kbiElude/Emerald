/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_EVENT_H
#define SYSTEM_EVENT_H

#include "system/system_types.h"


/** Creates an event synchronisation object.
 *
 *  @param bool True to reset the object with each finished wait() request, false to make it resettable only
 *              by means of calling system_event_reset() func.
 *  @param bool True to make the event waitable from start, false to require a wait in order to unblock any awaiting wait requests.
 *
 *  @return Event handle object.
 */
PUBLIC EMERALD_API __maybenull system_event system_event_create(bool manual_reset,
                                                                bool start_state);

/** Releases an event synchronisation object.
 *
 *  @param system_event Event object to release. Can be NULL.
 */
PUBLIC EMERALD_API void system_event_release(__in __maybenull __deallocate(mem) system_event event);

/** Resets an event object.
 *
 *  @param system_event Event object.
 */
PUBLIC EMERALD_API void system_event_reset(__in __notnull system_event event);

/** Sets an event object.
 *
 *  @param system_event Event object.
 */
PUBLIC EMERALD_API void system_event_set(__in __notnull system_event event);

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
PUBLIC EMERALD_API bool system_event_wait_single_peek(__in __notnull system_event event);

/** Waits infinitely on a single event object.
 *
 *  @paran system_event Event object to wait on.
 */
PUBLIC EMERALD_API void system_event_wait_single_infinite(__in __notnull system_event event);

/** TODO.
 *
 *  @return true if timed out, false otherwise.
 **/
PUBLIC EMERALD_API bool system_event_wait_single_timeout(__in __notnull system_event         event,
                                                                        system_timeline_time timeout);

/** Waits infinitely on multiple objects. Function can either block till one of the events become available or
 *  till all of them are free.
 *
 *  @param system_event* Array to wait on.
 *  @param int           Amount of elements in the array. Can't be negative
 *  @param bool          True to wait on all objects, false to wait on only one object.
 *
 *  @return Index of the event causing wait operation to unblock, if @param bool is false. Otherwise undetermined.
 */
PUBLIC EMERALD_API size_t system_event_wait_multiple_infinite(__in __notnull __ecount(n_elements) const system_event* events,
                                                              __in                                int                 n_elements,
                                                                                                  bool                wait_on_all_objects);

/** Waits on multiple objects until user-defined amount of time passes. Function can either block till one of the events become available or
 *  till all of them are free.
 *
 *  @param system_event*        Array to wait on
 *  @param int                  Amount of elements in the array. Can't be negative
 *  @param bool                 True to wait on all objects, false to wait on only one object.
 *  @param system_timeline_time Time-out.
 *  @param timeout_occured      Deref set to true if time-out occured. Otherwise set to false.
 *
 *  @return Index of the event causing wait operation to unblock, if @param bool is false. Otherwise undetermined.
 */
PUBLIC EMERALD_API size_t system_event_wait_multiple_timeout(__in  __notnull __ecount(n_elements) const system_event*  events,
                                                             __in                                 int                  n_elements,
                                                                                                  bool                 wait_on_all_objects,
                                                                                                  system_timeline_time timeout,
                                                             __out __notnull                      bool*                out_has_timeout_occured);

#endif /* SYSTEM_EVENT_H */
