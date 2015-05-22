/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_SEMAPHORE_H
#define SYSTEM_SEMAPHORE_H

#include "dll_exports.h"
#include "system_types.h"

/** Creates a semaphore object, allowing up to @param semaphore_capacity
 *  enter() calls from various threads before blocking the caller thread.
 *
 *  @param semaphore_capacity Capacity to use for the created semaphore.
 *
 *  @return Semaphore object.
 */
EMERALD_API system_semaphore system_semaphore_create(__in uint32_t semaphore_capacity);

/** Releases a semaphore object. Once called, do not use the object passed
 *  in the argument.
 *
 *  @param system_semaphore Semaphore to release.
 */
EMERALD_API void system_semaphore_release(__in __deallocate(mem) system_semaphore semaphore);

/** Enters a semaphore. This function will block if semaphore is fully occupied and will return
 *  to caller only after sufficient number of semaphore slots become available.
 *
 *  @param system_semaphore Semaphore object to use.
 */
EMERALD_API void system_semaphore_enter(__in system_semaphore semaphore);

/** Enters a semaphore @param count times. This function will block if semaphore is fully occupied and will return
 *  to caller only after sufficient number of semaphore slots become available.
 *
 *  @param system_semaphore Semaphore object to use.
 *  @param count            Number of enters to do on the semaphore object.
 */
EMERALD_API void system_semaphore_enter_multiple(__in system_semaphore semaphore,
                                                 __in uint32_t         count);

/** Leaves a semaphore.
 *
 *  @param system_semaphore Semaphore object to use.
 */
EMERALD_API void system_semaphore_leave(__in system_semaphore);

/** Leaves a semaphore @param count times.
 *
 *  @param system_semaphore Semaphore object to use.
 *  @param count            Number of leaves to do on the semaphore object.
 */
EMERALD_API void system_semaphore_leave_multiple(__in system_semaphore semaphore,
                                                 __in uint32_t         count);

#endif /* SYSTEM_SEMAPHORE_H */