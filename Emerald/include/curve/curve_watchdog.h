/**
 *
 * Emerald (kbi/elude @2015)
 *
 * The module intercepts "curve created" & "curve modified" notifications and implements
 * logic to provide the SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE curve mode
 * functionality.
 *
 * Note that this logic is only in place if global SYSTEM_GLOBAL_PROPERTY_CURVE_CONTAINER_BEHAVIOR
 * property is set to SYSTEM_GLOBAL_CURVE_CONTAINER_BEHAVIOR_SERIALIZE.
 *
 */
#ifndef CURVE_WATCHDOG_H
#define CURVE_WATCHDOG_H

/** Preallocated amount of segments for a curve container */
#define CURVE_CONTAINER_START_SEGMENTS_AMOUNT (2)

#endif /* CURVE_CONSTANTS_H */