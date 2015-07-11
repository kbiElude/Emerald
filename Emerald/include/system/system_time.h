/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include "system/system_types.h"

#define SYSTEM_TIME_INFINITE (0xFFFFFFFF)


/** Returns hz per sec used by the engine build.
 *
 *  @return Hz per sec.
 */
PUBLIC EMERALD_API uint32_t system_time_get_hz_per_sec();

/** TODO */
PUBLIC EMERALD_API system_time system_time_get_time_for_msec(__in uint32_t msec);

/** Converts seconds to system_time value.
 *
 *  @param uint32_t Amount of seconds to use. Allowed values are from [0, 2^32-1]
 *
 *  @return Corresponding system_time value.
 */
PUBLIC EMERALD_API system_time system_time_get_time_for_s(__in uint32_t seconds);

/** Converts amount of time represented in ms format to system_time value.
 *
 *  @param uint8_t  Amount of seconds to use. Allowed values are from [0, 60]
 *  @param uint32_t Amount of minutes to use. Allowed values are from [0, 2^32-1]
 *
 *  @return Corresponding system_time value.
 */
PUBLIC EMERALD_API system_time system_time_get_time_for_ms(__in uint32_t minutes,
                                                           __in uint8_t  seconds);

/** Converts amount of time represented in hms format to system_time value.
 *
 *  @param uint8_t  Amount of seconds to use. Allowed values are from [0, 60].
 *  @param uint8_t  Amount of minutes to use. Allowed values are from [0, 60].
 *  @param uint32_t Amount of hours to use. Allwoed values are from [0, 2^32-1].
 *
 *  @return Corresponding system_time value.
 */
PUBLIC EMERALD_API system_time system_time_get_time_for_hms(__in uint32_t hours,
                                                            __in uint8_t  minutes,
                                                            __in uint8_t  seconds);

/** TODO */
PUBLIC EMERALD_API void system_time_get_msec_for_time(__in            system_time time,
                                                      __out __notnull uint32_t*   result_miliseconds);

/** Converts system_time to amount of seconds corresponding to the value.
 *
 *  @param system_time Value to convert from.
 *  @param uint32_t*            Deref will be used to store amount of seconds the value corresponds to.
 *                              CANNOT be null.
 */
PUBLIC EMERALD_API void system_time_get_s_for_time(__in            system_time time,
                                                   __out __notnull uint32_t*   result_seconds);

/** Converts system_time to amount of minutes + seconds corresponding to the value.
 *
 *  @param system_time Value to convert from.
 *  @param uint32_t*            Deref will be used to store amount of seconds the value corresponds to.
 *                              CANNOT be null. Result value will be limited to [0, 60).
 *  @param uint8_t *            Deref will be used to store amount of minutes the value corresponds to.
 *                              CANNOT be null. Result avlue will be from range [0, 2^32-1].
 */
PUBLIC EMERALD_API void system_time_get_ms_for_time(__in            system_time time,
                                                    __out __notnull uint32_t*   result_minutes,
                                                    __out __notnull uint8_t*    result_seconds);

/** Converts system_time to amount of hours + minutes + seconds corresponding to the value.
 *
 *  @param system_time Value to convert from.
 *  @param uint8_t*             Deref will be used to store amount of seconds the value corresponds to.
 *                              CANNOT be null. Result value will be limited to [0, 60).
 *  @param uint8_t*             Deref will be used to store amount of minutes the value corresponds to.
 *                              CANNOT be null. Result avlue will be from range [0, 60).
 *  @param uint32_t*            Deref will be used to store amount of hours the value corresponds to.
 *                              CANNOT be null. Result avlue will be from range [0, 2^32-1].
 */
PUBLIC EMERALD_API void system_time_get_hms_for_time(__in            system_time time,
                                                     __out __notnull uint32_t*   result_hours,
                                                     __out __notnull uint8_t*    result_minutes,
                                                     __out __notnull uint8_t*    result_seconds);

/** Retrieves system_time for the time of call.
 *
 *  @return Time at the moment of call */
PUBLIC EMERALD_API system_time system_time_now();

/** Initializes time module. */
PUBLIC void _system_time_init();

/** Deinitializes time module. */
PUBLIC void _system_time_deinit();

#endif /* SYSTEM_TIME_H */
