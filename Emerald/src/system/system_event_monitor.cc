/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_cond_variable.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_event_monitor.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_semaphore.h"
#include "system/system_threads.h"
#include "system/system_time.h"


/* Forward declarations */
PRIVATE void _system_event_monitor_cond_variable_deinit        (system_resource_pool_block          block);
PRIVATE void _system_event_monitor_cond_variable_init          (system_resource_pool_block          block);
PRIVATE bool _system_event_monitor_cond_variable_test_predicate(void*                               internal_cvar);
PRIVATE void _system_event_monitor_event_init                  (system_resource_pool_block          block);
PRIVATE bool _system_event_monitor_is_update_needed            (void*                               user_arg);
PRIVATE void _system_event_monitor_thread_entrypoint           (system_threads_entry_point_argument user_arg);
PRIVATE void _system_event_monitor_wait_op_deinit              (system_resource_pool_block          block);
PRIVATE void _system_event_monitor_wait_op_init                (system_resource_pool_block          block);


typedef struct
{
    system_cond_variable cvar;
    bool                 cvar_predicate;
} _system_event_monitor_cond_variable;

typedef struct
{
    bool              manual_reset;
    bool              regular_event_status;
    system_thread     thread_event_thread;
    system_event_type type;
} _system_event_monitor_event;

typedef struct
{
    _system_event_monitor_event* events[MAXIMUM_WAIT_OBJECTS];
    unsigned int                 n_events;
    bool                         wait_on_all_events;

    /* Only access after entering wait_semaphore */
    unsigned int                 n_signalled_event;

    /* Raised by the global monitor thread when all required waiting conditions
     * are met.
     */
    system_semaphore             wait_semaphore;
} _system_event_monitor_wait_op;

typedef struct _system_event_monitor
{
    system_critical_section wakeup_cs;

    /* Access fields below only with wakeup_cs locked. --> */

    /* Holds _system_event_monitor_wait_op* instances, as returned by internal_wait_op_pool.
     * DO NOT release these instances.
     */
    system_resizable_vector blocked_wait_ops;
    system_hash64map        event_to_internal_event_map;
    system_thread           thread;
    bool                    thread_should_die;
    system_cond_variable    wakeup_cvar;

    system_resource_pool    internal_cvar_pool;    /* holds _system_event_monitor_cond_variable* instances */
    system_resource_pool    internal_events_pool;  /* holds _system_event_monitor_event*         instances */
    system_resource_pool    internal_wait_op_pool; /* holds _system_event_monitor_wait_op*       instances */


    _system_event_monitor()
    {
        blocked_wait_ops            = system_resizable_vector_create(16); /* capacity */
        event_to_internal_event_map = system_hash64map_create       (sizeof(system_event) );
        internal_cvar_pool          = system_resource_pool_create   (sizeof(system_cond_variable),
                                                                     16, /* n_elements_to_preallocate */
                                                                     _system_event_monitor_cond_variable_init,
                                                                     _system_event_monitor_cond_variable_deinit);
        internal_events_pool        = system_resource_pool_create   (sizeof(_system_event_monitor_event),
                                                                     16, /* n_elements_to_preallocate */
                                                                     _system_event_monitor_event_init,
                                                                     NULL); /* deinit_fn */
        internal_wait_op_pool       = system_resource_pool_create   (sizeof(_system_event_monitor_wait_op),
                                                                     16, /* n_elements_to_preallocate */
                                                                     _system_event_monitor_wait_op_init,
                                                                     _system_event_monitor_wait_op_deinit);
        thread                      = NULL;
        thread_should_die           = false;
        wakeup_cs                   = system_critical_section_create();
        wakeup_cvar                 = system_cond_variable_create   (_system_event_monitor_is_update_needed,
                                                                     NULL, /* test_predicate_user_arg */
                                                                     wakeup_cs);
    }

    ~_system_event_monitor()
    {
        if (blocked_wait_ops != NULL)
        {
            system_resizable_vector_release(blocked_wait_ops);

            blocked_wait_ops = NULL;
        }

        if (event_to_internal_event_map != NULL)
        {
            system_hash64map_release(event_to_internal_event_map);

            event_to_internal_event_map = NULL;
        }

        if (internal_cvar_pool != NULL)
        {
            system_resource_pool_release(internal_cvar_pool);

            internal_cvar_pool = NULL;
        }

        if (internal_events_pool != NULL)
        {
            system_resource_pool_release(internal_events_pool);

            internal_events_pool = NULL;
        }

        if (internal_wait_op_pool != NULL)
        {
            system_resource_pool_release(internal_wait_op_pool);

            internal_wait_op_pool = NULL;
        }

        if (wakeup_cs != NULL)
        {
            system_critical_section_release(wakeup_cs);

            wakeup_cs = NULL;
        }

        if (wakeup_cvar != NULL)
        {
            system_cond_variable_release(wakeup_cvar);

            wakeup_cvar = NULL;
        }
    }
} _system_event_monitor;


PRIVATE _system_event_monitor* monitor_ptr = NULL;


/** TODO */
PRIVATE void _system_event_monitor_cond_variable_deinit(system_resource_pool_block block)
{
    _system_event_monitor_cond_variable* cvar_ptr = (_system_event_monitor_cond_variable*) block;

    system_cond_variable_release(cvar_ptr->cvar);
    cvar_ptr->cvar = NULL;
}

/** TODO */
PRIVATE void _system_event_monitor_cond_variable_init(system_resource_pool_block block)
{
    _system_event_monitor_cond_variable* cvar_ptr = (_system_event_monitor_cond_variable*) block;

    cvar_ptr->cvar           = system_cond_variable_create(_system_event_monitor_cond_variable_test_predicate,
                                                           cvar_ptr);
    cvar_ptr->cvar_predicate = false;
}

/** TODO */
PRIVATE bool _system_event_monitor_cond_variable_test_predicate(void* internal_cvar)
{
    _system_event_monitor_cond_variable* cvar_ptr = (_system_event_monitor_cond_variable*) internal_cvar;

    return cvar_ptr->cvar_predicate;
}

/** TODO */
PRIVATE void _system_event_monitor_event_init(system_resource_pool_block block)
{
    _system_event_monitor_event* event_ptr = (_system_event_monitor_event*) block;

    event_ptr->manual_reset         = false;
    event_ptr->regular_event_status = false;
    event_ptr->thread_event_thread  = 0;
    event_ptr->type                 = SYSTEM_EVENT_TYPE_UNDEFINED;
}

/** TODO */
__forceinline bool _system_event_monitor_is_event_signalled(__in _system_event_monitor_event* event_ptr)
{
    bool result = false;

    if (event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
    {
        result = event_ptr->regular_event_status;
    }
    else
    {
        bool has_timed_out = false;

        ASSERT_DEBUG_SYNC(event_ptr->type == SYSTEM_EVENT_TYPE_THREAD,
                          "Unrecognized event type");

        system_threads_join_thread(event_ptr->thread_event_thread,
                                   0, /* timeout */
                                  &has_timed_out);

        result = !has_timed_out;
    }

    return result;
}

/** TODO */
PRIVATE bool _system_event_monitor_is_update_needed(void* user_arg)
{
    bool result = false;

    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Event monitor global instance is NULL");

    if (monitor_ptr != NULL)
    {
        system_critical_section_enter(monitor_ptr->wakeup_cs);
        {
            uint32_t n_blocked_wait_ops = 0;

            system_resizable_vector_get_property(monitor_ptr->blocked_wait_ops,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_blocked_wait_ops);

            /* NOTE: This effectively causes the event monitor to work as a spin-lock whenever
             *       at least one wait op is in progress, but I (kbi) wasn't able to figure out
             *       a better way to work around thread races.
             *
             *       Feel free to improve.
             */
            result = n_blocked_wait_ops > 0         ||
                     monitor_ptr->thread_should_die;
        }
        system_critical_section_leave(monitor_ptr->wakeup_cs);
    }

    return result;
}

/** TODO */
PRIVATE void _system_event_monitor_thread_entrypoint(system_threads_entry_point_argument user_arg)
{
    LOG_INFO("Event monitor thread starting.");

    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Event monitor global instance is NULL");

    while (!monitor_ptr->thread_should_die)
    {
        system_cond_variable_wait_begin(monitor_ptr->wakeup_cvar,
                                        SYSTEM_TIME_INFINITE,
                                        NULL); /* out_has_timed_out_ptr */
        {
            if (monitor_ptr->thread_should_die)
            {
                system_critical_section_leave(monitor_ptr->wakeup_cs);

                break;
            }

            /* NOTE: The worker thread has been awakened owing to at least one event having
             *       been set.
             *       While in this block, the wakeup_cs CS is locked.
             */
            uint32_t n_blocked_wait_ops = 0;

            system_resizable_vector_get_property(monitor_ptr->blocked_wait_ops,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_blocked_wait_ops);

            for (uint32_t n_blocked_wait_op = 0;
                          n_blocked_wait_op < n_blocked_wait_ops; )
            {
                _system_event_monitor_wait_op* current_wait_op_ptr  = NULL;
                bool                           has_awakened_wait_op = false;
                unsigned int                   n_signalled_event    = -1;

                if (!system_resizable_vector_get_element_at(monitor_ptr->blocked_wait_ops,
                                                            n_blocked_wait_op,
                                                           &current_wait_op_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve wait op descriptor at index [%d]",
                                      n_blocked_wait_op);

                    /* Please send help? */
                    ++n_blocked_wait_op;

                    continue;
                }

                /* Two paths here:
                 *
                 * 1) If we only need one of the events to be signalled, stop iterating over the events
                 *    as soon as we find a signalled event, and signal the wait op's condition variable.
                 *
                 * 2) If we need all of the wait op's, only signal the wait op's cond var if all of the
                 *    events are marked as signalled.
                 */
                has_awakened_wait_op = (current_wait_op_ptr->wait_on_all_events) ? true : false;
                n_signalled_event    = -1;

                for (unsigned int n_event = 0;
                                  n_event < current_wait_op_ptr->n_events;
                                ++n_event)
                {
                    _system_event_monitor_event* current_event_ptr  = current_wait_op_ptr->events[n_event];
                    bool                         is_event_signalled = false;

                    ASSERT_DEBUG_SYNC(current_event_ptr != NULL,
                                      "Currently processed event is NULL");

                    is_event_signalled = _system_event_monitor_is_event_signalled(current_event_ptr);

                    if (!current_wait_op_ptr->wait_on_all_events && is_event_signalled)
                    {
                        has_awakened_wait_op = true;
                        n_signalled_event    = n_event;

                        break;
                    }
                    else
                    if (current_wait_op_ptr->wait_on_all_events && !is_event_signalled)
                    {
                        has_awakened_wait_op = false;

                        break;
                    }
                } /* for (all events making up the wait op) */

                /* Move to the next wait op if can't acknowledge & discard the current one. */
                if (has_awakened_wait_op)
                {
                    if (current_wait_op_ptr->wait_on_all_events)
                    {
                        /* All required events are in a signalled state. */
                        for (unsigned int n_event = 0;
                                          n_event < current_wait_op_ptr->n_events;
                                        ++n_event)
                        {
                            _system_event_monitor_event* current_event_ptr  = current_wait_op_ptr->events[n_event];
                            bool                         is_event_signalled = _system_event_monitor_is_event_signalled(current_event_ptr);

                            /* If event should be automatically unsignalled, unsignal it now (since we're using it
                             * for the current wait op) */
                            ASSERT_DEBUG_SYNC(is_event_signalled,
                                              "Event is not signalled");

                            if ( current_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR &&
                                !current_event_ptr->manual_reset)
                            {
                                ASSERT_DEBUG_SYNC(current_event_ptr->regular_event_status,
                                                  "About to wake up a wait op but one of the required events is unsignalled.");

                                current_event_ptr->regular_event_status = false;
                            }
                        } /* for (all events) */

                        n_signalled_event = 0;
                    } /* if (current_wait_op_ptr->wait_on_all_events) */
                    else
                    {
                        /* Only one the required events is in a signalled state. Unsignal the first one we find, if necessary */
                        _system_event_monitor_event* current_event_ptr         = current_wait_op_ptr->events[n_signalled_event];
                        bool                         has_found_signalled_event = false;

                        if (current_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR &&
                            current_event_ptr->regular_event_status)
                        {
                            if (!current_event_ptr->manual_reset)
                            {
                                current_event_ptr->regular_event_status = false;
                            }

                            has_found_signalled_event = true;
                        }

                        ASSERT_DEBUG_SYNC(has_found_signalled_event,
                                          "About to wake up a wait op but none of the events are signalled.");
                    }

                    /* Signal the wait op's semaphore to awaken the waiting thread */
                    current_wait_op_ptr->n_signalled_event = n_signalled_event;

                    system_semaphore_leave(current_wait_op_ptr->wait_semaphore);

                    /* Remove the op from the blocked wait ops vector */
                    system_resizable_vector_delete_element_at(monitor_ptr->blocked_wait_ops,
                                                              n_blocked_wait_op);

                    --n_blocked_wait_ops;
                } /* if (has_awakened_wait_op) */
                else
                {
                    ++n_blocked_wait_op;
                }
            } /* for (all blocked wait ops) */
        }
        system_cond_variable_wait_end(monitor_ptr->wakeup_cvar);
    }

    LOG_INFO("Event monitor thread quitting.");
}

/** TODO */
PRIVATE void _system_event_monitor_wait_op_deinit(system_resource_pool_block block)
{
    _system_event_monitor_wait_op* wait_op_ptr = (_system_event_monitor_wait_op*) block;

    system_semaphore_release(wait_op_ptr->wait_semaphore);
    wait_op_ptr->wait_semaphore = NULL;
}

/** TODO */
PRIVATE void _system_event_monitor_wait_op_init(system_resource_pool_block block)
{
    _system_event_monitor_wait_op* wait_op_ptr = (_system_event_monitor_wait_op*) block;

    wait_op_ptr->n_events           = 0;
    wait_op_ptr->wait_on_all_events = false;
    wait_op_ptr->wait_semaphore     = system_semaphore_create(1,  /* semaphore_capacity      */
                                                              0); /* semaphore_default_value */
}


/** Please see header for spec */
PUBLIC void system_event_monitor_add_event(__in system_event event)
{
    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Global event monitor instance is NULL");

    system_critical_section_enter(monitor_ptr->wakeup_cs);
    {
        /* Configure the event's internal descriptor */
        _system_event_monitor_event* internal_event_ptr = (_system_event_monitor_event*) system_resource_pool_get_from_pool(monitor_ptr->internal_events_pool);

        system_event_get_property(event,
                                  SYSTEM_EVENT_PROPERTY_MANUAL_RESET,
                                 &internal_event_ptr->manual_reset);
        system_event_get_property(event,
                                  SYSTEM_EVENT_PROPERTY_TYPE,
                                 &internal_event_ptr->type);

        if (internal_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR)
        {
            internal_event_ptr->regular_event_status = false; /* NOT signalled by default, as per system_event impl */
        }
        else
        {
            system_event_get_property(event,
                                      SYSTEM_EVENT_PROPERTY_OWNED_THREAD,
                                     &internal_event_ptr->thread_event_thread);
        }

        /* Store it */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(monitor_ptr->event_to_internal_event_map,
                                                     (system_hash64) event),
                          "Internal event->internal event map already holds the specified event!");

        system_hash64map_insert(monitor_ptr->event_to_internal_event_map,
                                (system_hash64) event,
                                internal_event_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }
    system_critical_section_leave(monitor_ptr->wakeup_cs);
}

/** Please see header for spec */
PUBLIC void system_event_monitor_deinit()
{
    /* Shut down the worker thread */
    monitor_ptr->thread_should_die = true;

    system_cond_variable_signal(monitor_ptr->wakeup_cvar,
                                false);              /* should_broadcast */
    system_threads_join_thread (monitor_ptr->thread,
                                INFINITE,            /* timeout */
                                NULL);               /* out_has_timed_out_ptr */

    /* Release the monitor instance */
    if (monitor_ptr != NULL)
    {
        delete monitor_ptr;

        monitor_ptr = NULL;
    }
}

/** Please see header for spec */
PUBLIC void system_event_monitor_delete_event(__in system_event event)
{
    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Global event monitor instance is NULL");

    system_critical_section_enter(monitor_ptr->wakeup_cs);
    {
        _system_event_monitor_event* internal_event_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(monitor_ptr->event_to_internal_event_map,
                                                    (system_hash64) event),
                          "Internal event->internal event map does not hold the specified event!");

        if (system_hash64map_get(monitor_ptr->event_to_internal_event_map,
                                (system_hash64) event,
                                &internal_event_ptr) )
        {
            system_hash64map_remove(monitor_ptr->event_to_internal_event_map,
                                    (system_hash64) event);

            system_resource_pool_return_to_pool(monitor_ptr->internal_events_pool,
                                                (system_resource_pool_block) internal_event_ptr);
        }
    }
    system_critical_section_leave(monitor_ptr->wakeup_cs);
}

/** Please see header for spec */
PUBLIC void system_event_monitor_init()
{
    monitor_ptr = new (std::nothrow) _system_event_monitor;

    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Out of memory");

    if (monitor_ptr != NULL)
    {
        /* Launch the monitor thread.
         *
         * NOTE: We must NOT wait on the thread_wait_event. Since system_event_monitor is solely
         *       responsible for signalling condition variables, both for regular and thread events,
         *       we would have locked up, had we waited for the thread_wait_event to be signalled
         *       at system_event_monitor_deinit() time.
         *       However, since we use POSIX or Windows API to wait on threads, we can use the thread
         *       handle instead.
         */
        system_threads_spawn(_system_event_monitor_thread_entrypoint,
                             NULL, /* callback_func_argument */
                             NULL, /* thread_wait_event */
                            &monitor_ptr->thread);
    }
}

/** Please see header for spec */
PUBLIC void system_event_monitor_reset_event(__in system_event event)
{
    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Global event monitor instance is NULL");

    system_critical_section_enter(monitor_ptr->wakeup_cs);
    {
        _system_event_monitor_event* internal_event_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(monitor_ptr->event_to_internal_event_map,
                                                    (system_hash64) event),
                          "Specified event is not recognized");

        if (system_hash64map_get(monitor_ptr->event_to_internal_event_map,
                                (system_hash64) event,
                                &internal_event_ptr))
        {
            ASSERT_DEBUG_SYNC(internal_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR,
                              "Cannot reset a non-regular event");

            internal_event_ptr->regular_event_status = false;
        }
    }
    system_critical_section_leave(monitor_ptr->wakeup_cs);
}

/** Please see header for spec */
PUBLIC void system_event_monitor_set_event(__in system_event event)
{
    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Global event monitor instance is NULL");

    system_critical_section_enter(monitor_ptr->wakeup_cs);
    {
        _system_event_monitor_event* internal_event_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(monitor_ptr->event_to_internal_event_map,
                                                    (system_hash64) event),
                          "Specified event is not recognized");

        if (system_hash64map_get(monitor_ptr->event_to_internal_event_map,
                                (system_hash64) event,
                                &internal_event_ptr))
        {
            ASSERT_DEBUG_SYNC(internal_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR,
                              "Cannot set a non-regular event");

            if ( internal_event_ptr->type == SYSTEM_EVENT_TYPE_REGULAR &&
                !internal_event_ptr->regular_event_status)
            {
                internal_event_ptr->regular_event_status = true;
            }

            /* Wake up the global monitor thread, so that the blocking wait ops
             * can be traversed and awakened as necessary */
            system_cond_variable_signal(monitor_ptr->wakeup_cvar,
                                        false); /* should_broadcast */
        }
    }
    system_critical_section_leave(monitor_ptr->wakeup_cs);
}

/** Please see header for spec */
PUBLIC void system_event_monitor_wait(__in      system_event*        events,
                                      __in      unsigned int         n_events,
                                      __in      bool                 wait_for_all_events,
                                      __in      system_timeline_time timeout,
                                      __out_opt bool*                out_has_timed_out_ptr,
                                      __out_opt unsigned int*        out_signalled_event_index_ptr)
{
    /* Sanity checks */
    bool needs_to_leave_monitor_wakeup_cs;

    ASSERT_DEBUG_SYNC(monitor_ptr != NULL,
                      "Event monitor global instance is NULL");
    ASSERT_DEBUG_SYNC(n_events < MAXIMUM_WAIT_OBJECTS,
                      "Too many events to wait on");

    system_critical_section_enter(monitor_ptr->wakeup_cs);
    needs_to_leave_monitor_wakeup_cs = true;

    /* Retrieve a "wait op" descriptor instance */
    unsigned int                   n_events_found = 0;
    _system_event_monitor_wait_op* wait_op_ptr    = (_system_event_monitor_wait_op*) system_resource_pool_get_from_pool(monitor_ptr->internal_wait_op_pool);

    for (unsigned int n_event = 0;
                      n_event < n_events;
                    ++n_event)
    {
        _system_event_monitor_event* internal_event_ptr = NULL;

        if (!system_hash64map_get(monitor_ptr->event_to_internal_event_map,
                                  (system_hash64) events[n_event],
                                 &internal_event_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot wait on event at index [%d]: event not recognized",
                              n_event);

            continue;
        }

        wait_op_ptr->events[n_events_found++] = internal_event_ptr;
    } /* for (all requested events) */

    ASSERT_DEBUG_SYNC(n_events == n_events_found,
                      "Sanity check failed");

    if (n_events_found != 0)
    {
        wait_op_ptr->n_events           = n_events_found;
        wait_op_ptr->wait_on_all_events = wait_for_all_events;

        ASSERT_DEBUG_SYNC(wait_op_ptr->wait_semaphore != NULL,
                          "Wait op instance's semaphore is NULL");

        /* Throw this wait op into the bag and block on it, until the global monitor thread
         * wakes it up, or we time out.
         */
        bool         has_timed_out         = false;
        unsigned int signalled_event_index = -1;

        system_resizable_vector_push(monitor_ptr->blocked_wait_ops,
                                     wait_op_ptr);

        system_cond_variable_signal(monitor_ptr->wakeup_cvar,
                                    false); /* should_broadcast */

        system_critical_section_leave(monitor_ptr->wakeup_cs);
        needs_to_leave_monitor_wakeup_cs = false;

        system_semaphore_enter(wait_op_ptr->wait_semaphore,
                               timeout,
                              &has_timed_out);

        /* Time-out? Lock the wakeup_cs before we start fiddling with the monitor's stuff! */
        system_critical_section_enter(monitor_ptr->wakeup_cs);
        needs_to_leave_monitor_wakeup_cs = true;

        signalled_event_index = wait_op_ptr->n_signalled_event;

        /* Remove the wait op from the blocked wait ops vector, if the op is still there */
        size_t item_index = system_resizable_vector_find(monitor_ptr->blocked_wait_ops,
                                                         (void*) wait_op_ptr);

        if (item_index != ITEM_NOT_FOUND)
        {
            system_resizable_vector_delete_element_at(monitor_ptr->blocked_wait_ops,
                                                      item_index);
        } /* if (item_index != ITEM_NOT_FOUND) */

        /* If the event timed out, the semaphore might not have been raised at the previous
         * system_semaphore_enter() call. Before we entered the wakeup_cs, however, the monitor
         * might have raised it.
         *
         * Ensure the semaphore is really down before we continue
         */
        if (has_timed_out)
        {
            system_semaphore_enter(wait_op_ptr->wait_semaphore,
                                   0); /* timeout */
        }

        system_resource_pool_return_to_pool(monitor_ptr->internal_wait_op_pool,
                                            (system_resource_pool_block) wait_op_ptr);

        /* Report to the caller. */
        if (out_has_timed_out_ptr != NULL)
        {
            *out_has_timed_out_ptr = has_timed_out;
        }

        if (out_signalled_event_index_ptr != NULL)
        {
            *out_signalled_event_index_ptr = signalled_event_index;
        }
    }
    else
    {
        /* Cannot wait - none of the specified events was recognized. */
        system_resource_pool_return_to_pool(monitor_ptr->internal_wait_op_pool,
                                            (system_resource_pool_block) wait_op_ptr);
    }

    if (needs_to_leave_monitor_wakeup_cs)
    {
        system_critical_section_leave(monitor_ptr->wakeup_cs);

        needs_to_leave_monitor_wakeup_cs = false;
    }
}
