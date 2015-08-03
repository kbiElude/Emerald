/**
 *
 * Emerald (kbi/elude @2015)
 *
 * The module intercepts "curve created" & "curve modified" notifications and implements
 * logic to provide the SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE curve mode's
 * automated serialization functionality. More information can be found under
 * SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR documentation.
 *
 * Note that this logic is only enabled if global SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR
 * property is set to SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE.
 *
 */
#ifndef CURVE_WATCHDOG_H
#define CURVE_WATCHDOG_H

#include "system/system_types.h"


/** TODO.
 *
 *  NOTE: Private usage only.
 */
PUBLIC void curve_watchdog_deinit();

/** TODO.
 *
 *  NOTE: Private usage only.
 */
PUBLIC void curve_watchdog_init();

#endif /* CURVE_WATCHDOG_H */