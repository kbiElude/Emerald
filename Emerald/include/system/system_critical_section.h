/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_CRITICAL_SECTION_H
#define SYSTEM_CRITICAL_SECTION_H

#include "dll_exports.h"
#include "system_types.h"

/** Initializes a single critical section object.
 *
 *  @return New critical section object.
 */
PUBLIC EMERALD_API system_critical_section system_critical_section_create();

/** Enters a critical section.
 *
 *  @param system_critical_section Critical section to enter.
 */
PUBLIC EMERALD_API void system_critical_section_enter(__in system_critical_section critical_section);

/** TODO */
PUBLIC EMERALD_API system_thread_id system_critical_section_get_owner(__in system_critical_section critical_section);

/** Leaves a critical section
 *
 *  @param system_critical_section Critical section to leave.
 */
PUBLIC EMERALD_API void system_critical_section_leave(__in system_critical_section critical_section);

/** Deinitializes a single critical section object. After this function
 *  is called, the object can be no longer used.
 *
 *  @param system_critical_section Critical section to free.
 */
PUBLIC EMERALD_API void system_critical_section_release(__in __deallocate(mem) system_critical_section critical_section);

/** TODO */
PUBLIC EMERALD_API bool system_critical_section_try_enter(__in system_critical_section critical_section);

#endif /* SYSTEM_CRITICAL_SECTION_H */
