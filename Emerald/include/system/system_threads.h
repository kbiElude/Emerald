/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_THREADS_H
#define SYSTEM_THREADS_H

#include "system_types.h"

/** Retrieves current thread id
 *
 *  @return Returns id of currently running thread.
 */
PUBLIC EMERALD_API system_thread_id system_threads_get_thread_id();

/** Waits until specified thread finishes executing */
PUBLIC EMERALD_API void system_threads_join_thread(__in      system_thread        thread,
                                                   __in      system_timeline_time timeout,
                                                   __out_opt bool*                out_has_timed_out_ptr);

/** Spawns a new thread. Waits until the thread 
 *
 *  @param PFNSYSTEMTHREADSENTRYPOINTPROC      Entry point function pointer.
 *  @param system_threads_entry_point_argument Argument to pass on to the entry point function.
 *  @param thread_wait_event                   TODO
 *
 *  @return Thread if successful, 0 otherwise.
 */
PUBLIC EMERALD_API system_thread_id system_threads_spawn(__in  __notnull   PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func,
                                                         __in  __maybenull system_threads_entry_point_argument callback_func_argument,
                                                         __out __maybenull system_event*                       thread_wait_event,
                                                         __out __maybenull system_thread*                      out_thread_ptr = NULL);

/** Initializes threads module. Should only be called once from DLL entry point */
PUBLIC void _system_threads_init();

/** Deinitializes threads module. Should only be called once from DLL entry point */
PUBLIC void _system_threads_deinit();

#endif /* SYSTEM_THREADS_H */