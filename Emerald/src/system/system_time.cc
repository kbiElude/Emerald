/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_constants.h"
#include "system/system_log.h"
#include "system/system_time.h"

/** Private variables */
LARGE_INTEGER start_time     = {0, 0};
LARGE_INTEGER time_frequency = {0, 0};


/** Please see header for specification */
PUBLIC EMERALD_API uint32_t system_time_get_hz_per_sec()
{
    return HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_timeline_time system_time_get_timeline_time_for_msec(__in uint32_t msec)
{
    return msec * HZ_PER_SEC / 1000;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_timeline_time system_time_get_timeline_time_for_s(__in uint32_t seconds)
{
    return seconds * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_timeline_time system_time_get_timeline_time_for_ms(__in uint32_t minutes,
                                                                             __in uint8_t  seconds)
{
    return (minutes * 60 + seconds) * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_timeline_time system_time_get_timeline_time_for_hms(__in uint32_t hours,
                                                                              __in uint8_t  minutes,
                                                                              __in uint8_t  seconds)
{
    return (hours * 3600 + minutes * 60 + seconds) * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_msec_for_timeline_time(__in            system_timeline_time time,
                                                               __out __notnull uint32_t*            result_miliseconds)
{
    *result_miliseconds = time * 1000 / HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_s_for_timeline_time(__in            system_timeline_time time,
                                                            __out __notnull uint32_t*            result_seconds)
{
    *result_seconds = time / HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_ms_for_timeline_time(__in            system_timeline_time time,
                                                             __out __notnull uint32_t*            result_minutes,
                                                             __out __notnull uint8_t*             result_seconds)
{
    register uint32_t time_in_sec = time / HZ_PER_SEC;

    *result_minutes = time_in_sec / 60;
    *result_seconds = time_in_sec - *result_minutes * 60;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_hms_for_timeline_time(__in            system_timeline_time time,
                                                              __out __notnull uint32_t*            result_hours,
                                                              __out __notnull uint8_t*             result_minutes,
                                                              __out __notnull uint8_t*             result_seconds)
{
    register uint32_t time_in_sec         = time / HZ_PER_SEC;
    register uint32_t result_hours_temp   = 0;
    register uint32_t result_minutes_temp = 0;
    register uint32_t result_seconds_temp = 0;

    *result_hours      =  time_in_sec / 3600;
     result_hours_temp = *result_hours * 3600;

    *result_minutes      = (time_in_sec - result_hours_temp) / 60;
     result_minutes_temp = *result_minutes * 60;

    *result_seconds = (time_in_sec - result_hours_temp - result_minutes_temp);
}

/** Please see header for specification */
PUBLIC EMERALD_API system_timeline_time system_time_now()
{
    LARGE_INTEGER        current_time = {0, 0};
    system_timeline_time result       = 0;

    if (::QueryPerformanceCounter(&current_time) == FALSE)
    {
        LOG_FATAL("Could not obtain performance counter information.");
    }
    else
    {
        result = (system_timeline_time) ((current_time.QuadPart - start_time.QuadPart) * HZ_PER_SEC / time_frequency.QuadPart);
    }

    return result;
}

/** Please see header for specificaton */
PUBLIC void _system_time_init()
{
    if (::QueryPerformanceFrequency(&time_frequency) == FALSE)
    {
        LOG_FATAL("Could not obtain performance frequency information.");
    }

    if (::QueryPerformanceCounter(&start_time) == FALSE)
    {
        LOG_FATAL("Could not obtain start time information.");
    }
}

PUBLIC void _system_time_deinit()
{
}
