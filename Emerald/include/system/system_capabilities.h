/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_CAPABILITIES_H
#define SYSTEM_CAPABILITIES_H

#include "system_types.h"


/** TODO */
PUBLIC EMERALD_API void system_capabilities_get(__in            system_capabilities_property property,
                                                __out __notnull void*                        out_result);

/** TODO */
PUBLIC void system_capabilities_init();

#endif /* SYSTEM_CAPABILITIES_H */
