/**
 *
 * Emerald (kbi/elude @2015)
 *
 * A barrier is an object which can block signalling threads until n_signals_before_release threads
 * signal the barrier.
 * The parameter is specified at barrier creation time.
 *
 * A barrier cannot be re-used.
 *
 */
#ifndef SYSTEM_BARRIER_H
#define SYSTEM_BARRIER_H

#include "system_types.h"

typedef void (*PFNSYSTEMBARRIERABOUTTOSIGNALCALLBACKPROC)(void* user_arg);


/** TODO */
PUBLIC EMERALD_API system_barrier system_barrier_create(__in unsigned int n_signals_before_release);

/** TODO */
PUBLIC EMERALD_API void system_barrier_release(__in __notnull __post_invalid system_barrier barrier);

/** TODO */
PUBLIC EMERALD_API void system_barrier_signal(__in __notnull system_barrier                            barrier,
                                              __in           bool                                      wait_until_signalled,
                                              __in_opt       PFNSYSTEMBARRIERABOUTTOSIGNALCALLBACKPROC about_to_signal_callback_proc     = NULL,
                                              __in_opt       void*                                     about_to_signal_callback_user_arg = NULL);

/** TODO */
PUBLIC EMERALD_API void system_barrier_wait_until_signalled(__in __notnull system_barrier barrier);

#endif /* SYSTEM_BARRIER_H */
