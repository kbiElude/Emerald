/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Curve watchdog runs a dedicated thread which spends most of the time waiting on two events:
 *
 * a) "please die"            event: when set, causes the thread to leave the waiting loop and gracefully quit.
 * b) "curve modified by app" event: when set, causes the thread to enter an inner loop, whose purpose is to
 *                                   wait until each curve we have been notified about is not modified for
 *                                   a certain amount of time (eg. a few seconds). Once that time passes and
 *                                   the curve is not modified in-between, we serialize it to a human-readable
 *                                   text file.
 *
 * This effectively lays down foundation for half of the _SERIALIZE curve behavior mode.
 *
 * The other half is the reverse situation, where the user modifies text file corresponding to
 * a spawned curve container and expects the Emerald's curve instance to refresh. For this to happen, we
 * use Emerald's file monitor to watch for file changes. Should we receive a call-back for a known curve container,
 * we reset curve's configuration to the one described in the file.
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_watchdog.h"
#include "system/system_callback_manager.h"
#include "system/system_event.h"
#include "system/system_global.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_threads.h"


typedef struct _curve_watchdog_curve
{
    curve_container curve;

    _curve_watchdog_curve(curve_container in_curve)
    {
        ASSERT_DEBUG_SYNC(in_curve != NULL,
                          "Input curve container instance is NULL");

        curve = in_curve;
    }
} _curve_watchdog_curve;

typedef struct _curve_watchdog
{
    system_event curve_modified_by_app_event;
    system_event please_die_event;
    system_event watchdog_thread_wait_event;

    system_hash64map        curve_has_hash_to_descriptor_map; /* maps HAS' hash to a _curve_watchdog_curve instance */
    system_critical_section curve_has_hash_to_descriptor_map_cs;

    explicit _curve_watchdog()
    {
        curve_has_hash_to_descriptor_map    = system_hash64map_create(sizeof(_curve_watchdog_curve*) );
        curve_has_hash_to_descriptor_map_cs = system_critical_section_create();
        curve_modified_by_app_event         = system_event_create    (false); /* manual_reset */
        please_die_event                    = system_event_create    (true);  /* manual_reset */
        watchdog_thread_wait_event          = NULL;
    }

    ~_curve_watchdog()
    {
        /* Wait for the watchdog thread to quit */
        if (watchdog_thread_wait_event != NULL)
        {
            ASSERT_DEBUG_SYNC(please_die_event != NULL,
                              "Please die event is NULL");

            system_event_set        (please_die_event);
            system_event_wait_single(watchdog_thread_wait_event,
                                     SYSTEM_TIME_INFINITE);
        } /* if (watchdog_thread_wait_event != NULL) */

        /* Safe to release watchdog objects at this point. */
        if (curve_has_hash_to_descriptor_map != NULL)
        {
            uint32_t n_entries = 0;

            system_hash64map_get_property(curve_has_hash_to_descriptor_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_entries);

            for (uint32_t n_entry = 0;
                          n_entry < n_entries;
                        ++n_entry)
            {
                _curve_watchdog_curve* curve_ptr = NULL;

                if (!system_hash64map_get_element_at(curve_has_hash_to_descriptor_map,
                                                     n_entry,
                                                    &curve_ptr,
                                                     NULL) ) /* result_hash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve curve descriptor at index [%d]",
                                      n_entry);

                    continue;
                }

                delete curve_ptr;
            } /* for (all hash-map entries) */

            system_hash64map_release(curve_has_hash_to_descriptor_map);
            curve_has_hash_to_descriptor_map = NULL;
        }

        if (curve_has_hash_to_descriptor_map_cs != NULL)
        {
            system_critical_section_release(curve_has_hash_to_descriptor_map_cs);

            curve_has_hash_to_descriptor_map_cs = NULL;
        }

        if (curve_modified_by_app_event != NULL)
        {
            system_event_release(curve_modified_by_app_event);

            curve_modified_by_app_event = NULL;
        }

        if (please_die_event != NULL)
        {
            system_event_release(please_die_event);

            please_die_event = NULL;
        }

        if (watchdog_thread_wait_event != NULL)
        {
            system_event_release(watchdog_thread_wait_event);

            watchdog_thread_wait_event = NULL;
        }
    }
} _curve_watchdog;


/** TODO */
PRIVATE _curve_watchdog* watchdog_ptr = NULL;


/** TODO */
PRIVATE void _curve_watchdog_on_curve_added(const void* callback_data,
                                                  void* unused)
{
    curve_container           curve                    = (curve_container) callback_data;
    system_hashed_ansi_string curve_has                = NULL;
    system_hash64             curve_has_hash           = 0;
    _curve_watchdog_curve*    new_curve_descriptor_ptr = NULL;

    ASSERT_DEBUG_SYNC(curve != NULL,
                      "Input curve container instance is NULL");
    ASSERT_DEBUG_SYNC(watchdog_ptr != NULL,
                      "Curve watchdog instance is NULL");

    /* Retrieve curve's name */
    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_NAME,
                                &curve_has);

    ASSERT_DEBUG_SYNC(curve_has != NULL,
                      "Input curve has a NULL name");

    curve_has_hash = system_hashed_ansi_string_get_hash(curve_has);

    /* Create a descriptor for the new curve container */
    new_curve_descriptor_ptr = new (std::nothrow) _curve_watchdog_curve(curve);

    ASSERT_ALWAYS_SYNC(new_curve_descriptor_ptr != NULL,
                       "Out of memory");

    /* Store it in the watchdog map */
    system_critical_section_enter(watchdog_ptr->curve_has_hash_to_descriptor_map_cs);
    {
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(watchdog_ptr->curve_has_hash_to_descriptor_map,
                                                     curve_has_hash),
                          "Curve hash-map already stores an entry for curve [%s]",
                          system_hashed_ansi_string_get_buffer(curve_has) );

        system_hash64map_insert(watchdog_ptr->curve_has_hash_to_descriptor_map,
                                curve_has_hash,
                                new_curve_descriptor_ptr,
                                NULL,  /* on_remove_callback         */
                                NULL); /* on_remove_callback_argument*/
    }
    system_critical_section_leave(watchdog_ptr->curve_has_hash_to_descriptor_map_cs);

    /* No need to ping the watchdog thread about the new curve. */
}

/** TODO */
PRIVATE void _curve_watchdog_on_curve_deleted(const void* callback_data,
                                                    void* unused)
{
    ASSERT_DEBUG_SYNC(watchdog_ptr != NULL,
                      "Curve watchdog instance is NULL");

    /* Identify the curve descriptor */
    system_critical_section_enter(watchdog_ptr->curve_has_hash_to_descriptor_map_cs);
    {
        _curve_watchdog_curve* curve_ptr = NULL;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(watchdog_ptr->curve_has_hash_to_descriptor_map,
                                                    curve_has_hash),
                          "Curve hash-map already stores an entry for curve [%s]",
                          system_hashed_ansi_string_get_buffer(curve_has) );
    }
    system_critical_section_leave(watchdog_ptr->curve_has_hash_to_descriptor_map_cs);


}

/** TODO */
PRIVATE void _curve_watchdog_on_curve_modified(const void* callback_data,
                                                     void* unused)
{
    ASSERT_DEBUG_SYNC(watchdog_ptr != NULL,
                      "Curve watchdog instance is NULL");

}

/** TODO */
PRIVATE void _curve_watchdog_thread_entrypoint(system_threads_entry_point_argument unused)
{
    system_event wait_table[2] = {NULL};

    LOG_INFO("Curve watchdog thread starting..");

    /* Construct the wait table */
    ASSERT_DEBUG_SYNC(watchdog_ptr != NULL,
                      "Watchdog instance is NULL");

    wait_table[0] = watchdog_ptr->please_die_event;
    wait_table[1] = watchdog_ptr->curve_modified_by_app_event;

    /* Run the loop */
    while (true)
    {
        size_t wait_result;

        wait_result = system_event_wait_multiple(wait_table,
                                                 sizeof(wait_table) / sizeof(wait_table[0]),
                                                 false, /* wait_on_all_objects */
                                                 SYSTEM_TIME_INFINITE,
                                                 NULL); /* out_has_timed_out_ptr */

        if (wait_result == 0)
        {
            /* Please die event was set. Leave the loop */
            break;
        }
        else
        {
            /* One of the curves was reported to have been modified. */
            ASSERT_DEBUG_SYNC(wait_result == 1,
                              "Invalid event wait result");


        }
    } /* while (true) */

    LOG_INFO("Curve watchdog thread ending..");
}


/** Please see header for specification */
PUBLIC void curve_watchdog_deinit()
{
    if (watchdog_ptr != NULL)
    {
        /* Unsubscribe from curve notifications first */
        const system_callback_manager global_callback_manager = system_callback_manager_get();

        system_callback_manager_unsubscribe_from_callbacks(global_callback_manager,
                                                           CALLBACK_ID_CURVE_CONTAINER_ADDED,
                                                           _curve_watchdog_on_curve_added,
                                                           NULL); /* callback_proc_user_arg */
        system_callback_manager_unsubscribe_from_callbacks(global_callback_manager,
                                                           CALLBACK_ID_CURVE_CONTAINER_DELETED,
                                                           _curve_watchdog_on_curve_deleted,
                                                           NULL); /* callback_proc_user_arg */
        system_callback_manager_unsubscribe_from_callbacks(global_callback_manager,
                                                           CALLBACK_ID_CURVE_CONTAINER_MODIFIED,
                                                           _curve_watchdog_on_curve_modified,
                                                           NULL); /* callback_proc_user_arg */
        /* Should be safe to release the watchdog instance at this point. */
        delete watchdog_ptr;

        watchdog_ptr = NULL;
    }
}

/** Please see header for specification */
PUBLIC void curve_watchdog_init()
{
    ASSERT_DEBUG_SYNC(watchdog_ptr == NULL,
                      "Curve watch-dog instance is not NULL");

    watchdog_ptr = new (std::nothrow) _curve_watchdog();

    ASSERT_ALWAYS_SYNC(watchdog_ptr != NULL,
                       "Out of memory");

    if (watchdog_ptr != NULL)
    {
        const system_callback_manager global_callback_manager = system_callback_manager_get();

        /* Register for 'curve created', 'curve modified' and 'curve deleted' notifications */
        system_callback_manager_subscribe_for_callbacks(global_callback_manager,
                                                        CALLBACK_ID_CURVE_CONTAINER_ADDED,
                                                        CALLBACK_SYNCHRONICITY_ASYNCHRONOUS,
                                                        _curve_watchdog_on_curve_added,
                                                        NULL); /* callback_proc_user_arg */
        system_callback_manager_subscribe_for_callbacks(global_callback_manager,
                                                        CALLBACK_ID_CURVE_CONTAINER_DELETED,
                                                        CALLBACK_SYNCHRONICITY_ASYNCHRONOUS,
                                                        _curve_watchdog_on_curve_deleted,
                                                        NULL); /* callback_proc_user_arg */
        system_callback_manager_subscribe_for_callbacks(global_callback_manager,
                                                        CALLBACK_ID_CURVE_CONTAINER_MODIFIED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _curve_watchdog_on_curve_modified,
                                                        NULL); /* callback_proc_user_arg */

        /* Spawn the watchdog thread */
        system_threads_spawn(_curve_watchdog_thread_entrypoint,
                             NULL, /* callback_func_argument */
                            &watchdog_ptr->watchdog_thread_wait_event,
                             system_hashed_ansi_string_create("Curve watch-dog thread") );
    } /* if (watchdog_ptr != NULL) */
}