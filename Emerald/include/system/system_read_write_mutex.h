/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_READ_WRITE_MUTEX
#define SYSTEM_READ_WRITE_MUTEX

#include "system_types.h"

/** Creates a read/write mutex object
 *
 *  @return Read/write mutex instance.
 */
PUBLIC EMERALD_API system_read_write_mutex system_read_write_mutex_create();

/** TODO */
PUBLIC EMERALD_API system_thread_id system_read_write_mutex_get_write_thread_id(system_read_write_mutex mutex);

/** TODO */
PUBLIC EMERALD_API bool system_read_write_mutex_is_locked(system_read_write_mutex mutex);

/** TODO */
PUBLIC EMERALD_API bool system_read_write_mutex_is_write_locked(system_read_write_mutex mutex);

/** Locks a read/write mutex object for either read or write access.
 *
 *  NOTE: Locking a write access for a mutex that's arleady been locked for
 *        read access can result in a lock up.
 *
 *  @param system_read_write_mutex Mutex to perform the operation on.
 *  @param system_read_write_mutex_access_type Access type to lock the mutex for.
 */
PUBLIC EMERALD_API void system_read_write_mutex_lock(system_read_write_mutex             mutex,
                                                     system_read_write_mutex_access_type access_type);

/** Releases a read/write mutex object. Do not use the object afterward.
 *
 *  @param system_read_write_mutex Mutex to release.
 */
PUBLIC EMERALD_API void system_read_write_mutex_release(system_read_write_mutex mutex);

/** Unlocks a read/write mutex object from either read or write access.
 *
 *  Note: if the mutex was not locked for particular access type, you can expect
 *        assertion failures or lock-ups.
 *
 *  Note: Multiple write requests will result in a lock up.
 *
 *  @param system_read_write_mutex Mutex to perform the operation on.
 *  @param system_read_write_mutex_access_type Access type to unlock the mutex from.
 */
PUBLIC EMERALD_API void system_read_write_mutex_unlock(system_read_write_mutex             mutex,
                                                       system_read_write_mutex_access_type access_type);

#endif /* SYSTEM_READ_WRITE_MUTEX */
