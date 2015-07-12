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
PUBLIC EMERALD_API system_barrier system_barrier_create(unsigned int n_signals_before_release);

/** TODO */
PUBLIC EMERALD_API void system_barrier_release(system_barrier barrier);

/** TODO */
PUBLIC EMERALD_API void system_barrier_signal(system_barrier                            barrier,
                                              bool                                      wait_until_signalled,
                                              PFNSYSTEMBARRIERABOUTTOSIGNALCALLBACKPROC about_to_signal_callback_proc     = NULL,
                                              void*                                     about_to_signal_callback_user_arg = NULL);

/** TODO */
PUBLIC EMERALD_API void system_barrier_wait_until_signalled(system_barrier barrier);

#endif /* SYSTEM_BARRIER_H */
