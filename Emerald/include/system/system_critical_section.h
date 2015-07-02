/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_CRITICAL_SECTION_H
#define SYSTEM_CRITICAL_SECTION_H

#include "system_types.h"


REFCOUNT_INSERT_DECLARATIONS(system_critical_section,
                             system_critical_section)


typedef enum
{
    /* Linux:   pthread_mutex_t*.
     * Windows: CRITICAL_SECTION*.
     *
     * Only for internal usage!
     */
    SYSTEM_CRITICAL_SECTION_PROPERTY_HANDLE,

    /* system_thread_id */
    SYSTEM_CRITICAL_SECTION_PROPERTY_OWNER_THREAD_ID
} system_critical_section_property;

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
PUBLIC EMERALD_API void system_critical_section_get_property(__in  system_critical_section          critical_section,
                                                             __in  system_critical_section_property property,
                                                             __out void*                            out_result_ptr);

/** Leaves a critical section
 *
 *  @param system_critical_section Critical section to leave.
 */
PUBLIC EMERALD_API void system_critical_section_leave(__in system_critical_section critical_section);

/** TODO */
PUBLIC EMERALD_API bool system_critical_section_try_enter(__in system_critical_section critical_section);

#endif /* SYSTEM_CRITICAL_SECTION_H */
