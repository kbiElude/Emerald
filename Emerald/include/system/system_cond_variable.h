/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Implements a condition variable in a cross-platform manner.
 */
#ifndef SYSTEM_COND_VARIABLE
#define SYSTEM_COND_VARIABLE

#include "system_types.h"

typedef bool (*PFNSYSTEMCONDVARIABLETESTPROC)(void* user_arg);


/** Creates a new condition variable. The CV can either use the user-specified critical section,
 *  or generate one for its internal use. In the former case, the CV *retains* the CS and releases
 *  it at destruction time.
 *
 *  @return The requested condition variable, or NULL if the request has failed.
 */
PUBLIC EMERALD_API system_cond_variable system_cond_variable_create(PFNSYSTEMCONDVARIABLETESTPROC pTestPredicate,
                                                                    void*                         test_predicate_user_arg,
                                                                    system_critical_section       in_cs = NULL);

/** Signals a specified condition variable.
 *
 *  This function can either wake one of the threads waiting on the conditional variables,
 *  or all of them.
 *
 *  @param cond_variable    Condition variable to signal. Must not be NULL.
 *  @param should_broadcast true to wake up all awaiting threads; false to wake only one of them.
 *                          Which of the threads will be awakened in the latter case is undefined.
 **/
PUBLIC EMERALD_API void system_cond_variable_signal(system_cond_variable cond_variable,
                                                    bool                 should_broadcast);

/** Blocks until a specified condition variable is assigned to the calling thread. No other thread is guaranteed,
 *  to work on the logic guarded by the condition variable, after the call returns.
 *
 *  If *out_has_timed_out_ptr is NOT true, the caller MUST call system_cond_variable_wait_end() on the
 *  condition variable, after it is done modifying the guarded state, lest the other enqueued threads
 *  never gain access.
 *
 *  @param cond_variable         Condition variable to use for the call. Must not be NULL.
 *  @param timeout               If timed-out wait is needed, provide a system_time describing the
 *                               minimum amount of time after which the caller should be returned execution flow.
 *                               If the time-out functionality is not needed, pass SYSTEM_TIME_INFINITY.
 *  @param out_has_timed_out_ptr Optional; deref will be assigned the boolean value, indicating if the request
 *                               has timed out (true) or not (false).
 *
 **/
PUBLIC EMERALD_API void system_cond_variable_wait_begin(system_cond_variable cond_variable,
                                                        system_time          timeout,
                                                        bool*                out_has_timed_out_ptr);

/** Releases the CV ownership from the current thread, letting other enqueued threads gain access to the guarded logic.
 *
 *  Must be called after a successful system_cond_variable_wait_begin() call.
 *
 *  @param cond_variable Condition variable to use for the call. Must not be NULL.
 **/
PUBLIC EMERALD_API void system_cond_variable_wait_end(system_cond_variable cond_variable);

/** Releases the condition variable.
 *
 *  @param cond_variable Condition variable to release. Must NOT be NULL.
 **/
PUBLIC EMERALD_API void system_cond_variable_release(system_cond_variable cond_variable);

#endif /* SYSTEM_COND_VARIABLE */
