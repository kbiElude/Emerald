/**
 *
 * Emerald (kbi/elude @2015)
 *
 * This module implements an event monitor, which operates in a dedicated thread. 
 *
 * Internal use only.
 */
#ifndef SYSTEM_EVENT_MONITOR_H
#define SYSTEM_EVENT_MONITOR_H

#include "system_types.h"


/** TODO */
PUBLIC void system_event_monitor_add_event(__in system_event event);

/** TODO */
PUBLIC void system_event_monitor_deinit();

/** TODO */
PUBLIC void system_event_monitor_delete_event(__in system_event event);

/** TODO */
PUBLIC void system_event_monitor_init();

/** TODO */
PUBLIC void system_event_monitor_reset_event(__in system_event event);

/** TODO */
PUBLIC void system_event_monitor_set_event(__in system_event event);

/** TODO */
PUBLIC void system_event_monitor_wait(__in      system_event* events,
                                      __in      unsigned int  n_events,
                                      __in      bool          wait_for_all_events,
                                      __in      system_time   timeout,
                                      __out_opt bool*         out_has_timed_out_ptr,
                                      __out_opt unsigned int* out_signalled_event_index_ptr);

#endif /* SYSTEM_EVENT_MONITOR_H */
