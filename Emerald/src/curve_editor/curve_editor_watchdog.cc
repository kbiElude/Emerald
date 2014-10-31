/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve_editor/curve_editor_general.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_watchdog.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_threads.h"
#include "system/system_time.h"

#define SIGNAL_COLLATE_TIMEOUT (1000) /* ms */


typedef struct _curve_editor_watchdog
{
    system_event wakeup_event;
    system_event wakeup_thread_kill_event;
    system_event wakeup_thread_killed_event;

    _curve_editor_watchdog()
    {
        wakeup_event             = system_event_create(false,  /* manual_reset */
                                                       false); /* start_state */
        wakeup_thread_kill_event = system_event_create(true,   /* manual_reset */
                                                       false); /* start_state */
    }

    ~_curve_editor_watchdog()
    {
        if (wakeup_event != NULL)
        {
            system_event_release(wakeup_event);

            wakeup_event = NULL;
        }

        if (wakeup_thread_kill_event != NULL)
        {
            system_event_release(wakeup_thread_kill_event);

            wakeup_thread_kill_event = NULL;
        }

        if (wakeup_thread_killed_event != NULL)
        {
            system_event_release(wakeup_thread_killed_event);

            wakeup_thread_killed_event = NULL;
        }
    }
} _curve_editor_watchdog;


/** TODO */
PRIVATE void _curve_editor_watchdog_monitor_thread_entrypoint(__in __notnull system_threads_entry_point_argument argument)
{
    /* This is new thread's entry-point */
    const system_timeline_time timeout      = system_time_get_timeline_time_for_msec(SIGNAL_COLLATE_TIMEOUT);
    _curve_editor_watchdog*    watchdog_ptr = (_curve_editor_watchdog*) argument;
    const system_event         events[]     =
    {
        watchdog_ptr->wakeup_event,
        watchdog_ptr->wakeup_thread_kill_event
    };

    LOG_INFO("Starting curve editor watchdog thread");

    while (true)
    {
        size_t n_event = system_event_wait_multiple_infinite(events,
                                                             sizeof(events) / sizeof(events[0]),
                                                             false); /* wait_on_all_objects */

        if (n_event == 1)
        {
            /* This a kill request */
            goto end;
        }

        /* We have been signalled. Wait again till a time out occurs or a kill request is requested.
         * This allows us to collate incoming signals */
        bool has_timeout_occurred = false;

        while (!has_timeout_occurred)
        {
            n_event = system_event_wait_multiple_timeout(events,
                                                         sizeof(events) / sizeof(events[0]),
                                                         false, /* wait_on_all_objects */
                                                         timeout,
                                                        &has_timeout_occurred);

            if (!has_timeout_occurred)
            {
                /* Is this a kill request? */
                if (n_event == 1)
                {
                    goto end;
                }

                /* OK, keep on spinning then */
            } /* if (!has_timeout_occurred) */
        }

        /* We should be OK to update curve window contents at this point */
        curve_editor_update_curve_list();
    }

end:
    LOG_INFO("Leaving curve editor watchdog thread");
}



/** Please see header for spec */
PUBLIC curve_editor_watchdog curve_editor_watchdog_create()
{
    _curve_editor_watchdog* watchdog_ptr = new (std::nothrow) _curve_editor_watchdog;

    ASSERT_ALWAYS_SYNC(watchdog_ptr != NULL,
                       "Out of memory");
    if (watchdog_ptr != NULL)
    {
        /* Spawn the worker thread */
        system_threads_spawn(_curve_editor_watchdog_monitor_thread_entrypoint,
                             watchdog_ptr,
                             &watchdog_ptr->wakeup_thread_killed_event);
    }

    return (curve_editor_watchdog) watchdog_ptr;
}

/** Please see header for spec */
PUBLIC void curve_editor_watchdog_release(__in __notnull curve_editor_watchdog watchdog)
{
    if (watchdog != NULL)
    {
        _curve_editor_watchdog* watchdog_ptr = (_curve_editor_watchdog*) watchdog;

        /* Kill the watchdog thread */
        system_event_set                 (watchdog_ptr->wakeup_thread_kill_event);
        system_event_wait_single_infinite(watchdog_ptr->wakeup_thread_killed_event);

        /* Safe to release the descriptor */
        delete watchdog_ptr;
        watchdog_ptr = NULL;
    }
}

/** Please see header for spec */
PUBLIC void curve_editor_watchdog_signal(__in __notnull curve_editor_watchdog watchdog)
{
    _curve_editor_watchdog* watchdog_ptr = (_curve_editor_watchdog*) watchdog;

    system_event_set(watchdog_ptr->wakeup_event);
}

