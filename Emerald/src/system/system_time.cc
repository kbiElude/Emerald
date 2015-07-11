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
#ifdef _WIN32
    LARGE_INTEGER start_time     = {0, 0};
    LARGE_INTEGER time_frequency = {0, 0};
#else
    __uint64 start_time_msec;
#endif


/** Please see header for specification */
PUBLIC EMERALD_API uint32_t system_time_get_hz_per_sec()
{
    return HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_time system_time_get_time_for_msec(__in uint32_t msec)
{
    return msec * HZ_PER_SEC / 1000;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_time system_time_get_time_for_s(__in uint32_t seconds)
{
    return seconds * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_time system_time_get_time_for_ms(__in uint32_t minutes,
                                                                    __in uint8_t  seconds)
{
    return (minutes * 60 + seconds) * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_time system_time_get_time_for_hms(__in uint32_t hours,
                                                                     __in uint8_t  minutes,
                                                                     __in uint8_t  seconds)
{
    return (hours * 3600 + minutes * 60 + seconds) * HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_msec_for_time(__in            system_time time,
                                                      __out __notnull uint32_t*   result_miliseconds)
{
    *result_miliseconds = time * 1000 / HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_s_for_time(__in            system_time time,
                                                   __out __notnull uint32_t*   result_seconds)
{
    *result_seconds = time / HZ_PER_SEC;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_ms_for_time(__in            system_time time,
                                                    __out __notnull uint32_t*   result_minutes,
                                                    __out __notnull uint8_t*    result_seconds)
{
    register uint32_t time_in_sec = time / HZ_PER_SEC;

    *result_minutes = time_in_sec / 60;
    *result_seconds = time_in_sec - *result_minutes * 60;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_time_get_hms_for_time(__in            system_time time,
                                                     __out __notnull uint32_t*   result_hours,
                                                     __out __notnull uint8_t*    result_minutes,
                                                     __out __notnull uint8_t*    result_seconds)
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
PUBLIC EMERALD_API system_time system_time_now()
{
    system_time result = 0;

#ifdef _WIN32
    LARGE_INTEGER current_time = {0, 0};

    if (::QueryPerformanceCounter(&current_time) == FALSE)
    {
        LOG_FATAL("Could not obtain performance counter information.");
    }
    else
    {
        result = (system_time) ((current_time.QuadPart - start_time.QuadPart) * HZ_PER_SEC / time_frequency.QuadPart);
    }
#else
    struct timespec current_timespec;

    clock_gettime(CLOCK_MONOTONIC,
                 &current_timespec);

    result = (system_time) (((1000LL /* SEC_TO_MSEC */ * current_timespec.tv_sec + current_timespec.tv_nsec / 1000000LL /* MSEC_TO_NSEC */ - start_time_msec) * HZ_PER_SEC / 1000LL));
#endif

    return result;
}

/** Please see header for specificaton */
PUBLIC void _system_time_init()
{
    /* Retrieve the counter frequency */
#ifdef _WIN32
    if (::QueryPerformanceFrequency(&time_frequency) == FALSE)
    {
        LOG_FATAL("Could not obtain performance frequency information.");
    }

    /* Retrieve the start time, relative to which all the timestamps are going to be calculated */
    if (::QueryPerformanceCounter(&start_time) == FALSE)
    {
        LOG_FATAL("Could not obtain start time information.");
    }
#else
    struct timespec current_timespec;

    clock_gettime(CLOCK_MONOTONIC,
                 &current_timespec);

    start_time_msec = 1000LL /* SEC_TO_MSEC */ * current_timespec.tv_sec + current_timespec.tv_nsec / 1000000LL /* MSEC_TO_NSEC */;
#endif
}

PUBLIC void _system_time_deinit()
{
}
