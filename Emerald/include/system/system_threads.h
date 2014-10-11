/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_THREADS_H
#define SYSTEM_THREADS_H

#include "dll_exports.h"
#include "system_types.h"

/** Retrieves current thread id 
 *
 *  @return Returns id of currently running thread.
 */
PUBLIC EMERALD_API system_thread_id system_threads_get_thread_id();

/** Spawns a new thread. Waits until the thread 
 *
 *  @param PFNSYSTEMTHREADSENTRYPOINTPROC      Entry point function pointer.
 *  @param system_threads_entry_point_argument Argument to pass on to the entry point function.
 *  @param thread_wait_event                   TODO
 *
 *  @return Thread if successful, 0 otherwise.
 */
PUBLIC EMERALD_API system_thread_id system_threads_spawn(__in __notnull PFNSYSTEMTHREADSENTRYPOINTPROC, __in __maybenull system_threads_entry_point_argument, __out __maybenull system_event* thread_wait_event);

/** Initializes threads module. Should only be called once from DLL entry point */
PUBLIC void _system_threads_init();

/** Deinitializes threads module. Should only be called once from DLL entry point */
PUBLIC void _system_threads_deinit();

#endif /* SYSTEM_THREADS_H */