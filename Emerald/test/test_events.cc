/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "test_events.h"
#include "shared.h"
#include "system/system_event.h"
#include "system/system_threads.h"
#include "gtest/gtest.h"

/* ------ "Wait, then set" functional test ------ */
typedef struct _wait_then_set_test_data
{
    system_event wakeup_event;

    _wait_then_set_test_data()
    {
        wakeup_event = NULL;
    }
} _wait_then_set_test_data;

PRIVATE void _wait_then_set_test_setter_thread(void* user_arg)
{
    _wait_then_set_test_data* data_ptr = (_wait_then_set_test_data*) user_arg;

    /* Wait for 2s */
    ::Sleep(2000);

    /* Set the event */
    system_event_set(data_ptr->wakeup_event);
}


TEST(EventsTest, WaitThenSet)
{
    /* Set up the descriptor to share with the setter thread */
    _wait_then_set_test_data data;

    data.wakeup_event = system_event_create(true); /* manual_reset */

    /* Spawn the setter thread */
    system_threads_spawn(_wait_then_set_test_setter_thread,
                         &data,
                         NULL); /* thread_wait_event */

    /* Instantly wait on the event */
    system_event_wait_single(data.wakeup_event);

    /* Clean up */
    system_event_release(data.wakeup_event);
    data.wakeup_event = NULL;
}

