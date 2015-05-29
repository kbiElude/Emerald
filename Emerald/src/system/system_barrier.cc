/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_atomics.h"
#include "system/system_barrier.h"
#include "system/system_event.h"

typedef struct _system_barrier
{
    unsigned int n_signals_before_release;
    unsigned int n_threads_signalled;
    system_event sync;

    explicit _system_barrier(const unsigned int& in_n_signals_before_release)
    {
        n_signals_before_release = in_n_signals_before_release;
        n_threads_signalled      = 0;
        sync                     = system_event_create(true,   /* manual_reset */
                                                       false); /* start_state  */
    }
} _system_barrier;

/** Please see header for specification */
PUBLIC EMERALD_API system_barrier system_barrier_create(__in unsigned int n_signals_before_release)
{
    _system_barrier* barrier_ptr = new (std::nothrow) _system_barrier(n_signals_before_release);

    ASSERT_DEBUG_SYNC(barrier_ptr != NULL,
                      "Out of memory");

    if (n_signals_before_release == 0)
    {
        system_barrier_signal( (system_barrier) barrier_ptr,
                               false);
    }

    return (system_barrier) barrier_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_barrier_release(__in __notnull __post_invalid system_barrier barrier)
{
    _system_barrier* barrier_ptr = (_system_barrier*) barrier;

    if (barrier_ptr->sync != NULL)
    {
        system_event_release(barrier_ptr->sync);

        barrier_ptr->sync = NULL;
    }

    delete barrier_ptr;
    barrier_ptr = NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_barrier_signal(__in __notnull system_barrier                            barrier,
                                              __in           bool                                      wait_until_signalled,
                                              __in_opt       PFNSYSTEMBARRIERABOUTTOSIGNALCALLBACKPROC about_to_signal_callback_proc,
                                              __in_opt       void*                                     about_to_signal_callback_user_arg)
{
    _system_barrier* barrier_ptr = (_system_barrier*) barrier;

    if (system_atomics_increment(&barrier_ptr->n_threads_signalled) >= barrier_ptr->n_signals_before_release)
    {
        if (about_to_signal_callback_proc != NULL)
        {
            about_to_signal_callback_proc(about_to_signal_callback_user_arg);
        } /* if (about_to_signal_callback_proc != NULL) */

        system_event_set(barrier_ptr->sync);
    }

    if (wait_until_signalled)
    {
        system_event_wait_single_infinite(barrier_ptr->sync);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_barrier_wait_until_signalled(__in __notnull system_barrier barrier)
{
    _system_barrier* barrier_ptr = (_system_barrier*) barrier;

    system_event_wait_single_infinite(barrier_ptr->sync);
}