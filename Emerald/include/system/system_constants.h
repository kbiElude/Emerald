/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SYSTEM_CONSTANTS_H
#define SYSTEM_CONSTANTS_H

#include "system_types.h"

/* Name of window class that will be used by all engine-spawned windows */
#define EMERALD_WINDOW_CLASS_NAME ("Emerald")

/* Length of a buffer used by variants for forced setters */
#define CONVERSION_BUFFER_LENGTH (32768)

/* Defines amount of ticks representing 1 second. This is used for storing time information, relative
 * to timeline. 
 */
#define HZ_PER_SEC (100)

/* Defines amount of webcam entries that will be allocated space in webcam manager on start. */
#define BASE_WEBCAMS_AMOUNT (2)

/* Defines amount of precached entries for threads info storage */
#define BASE_THREADS_CAPACITY (8)

/* Defines amount of precached entries for window handle storage */
#define BASE_WINDOW_STORAGE (2)

/* Defines amount of threads active in thread pool */
#define THREAD_POOL_AMOUNT_OF_THREADS (4)

/* Defines amount of precached task descriptors to be used by thread pool. */
#define THREAD_POOL_PREALLOCATED_TASK_DESCRIPTORS (64)

/* Defines amount of precached task group descriptors to be used by thread pool. */
#define THREAD_POOL_PREALLOCATED_TASK_GROUP_DESCRIPTORS (8)

/* Defines amount of task slots to preallocate for a single task group descriptor */
#define THREAD_POOL_PREALLOCATED_TASK_SLOTS (8)

/* Defines amount of task containers created per-priority */
#define THREAD_POOL_PREALLOCATED_TASK_CONTAINERS (64)

/* Defines amount of preallocated order descriptors */
#define THREAD_POOL_PREALLOCATED_ORDER_DESCRIPTORS (THREAD_POOL_PREALLOCATED_TASK_CONTAINERS * THREAD_POOL_TASK_PRIORITY_COUNT)

/* Defines start capacity for file serializer's writing facility. */
#define FILE_SERIALIZER_START_CAPACITY (65536)

/* Define log level threshold. Logs with level lower than this value will be dropped on pre-processor phase. */
#define LOGLEVEL_BASE (LOGLEVEL_INFORMATION)

/* Defines base capacity for 4x4 matrix storage */
#define MATRIX4X4_BASE_CAPACITY (512)

/* Defines base capacity for variants storage */
#define VARIANTS_BASE_CAPACITY (128)

/* Defines base capacity for container used internally by the assertion module */
#define ASSERTIONS_BASE_CAPACITY (4)

/* Defines maximum length of an assertion failure string. */
#define ASSERTION_FAILURE_MAX_LENGTH (1024)

/* Defines maximum length of a single log entry. */
#define LOG_MAX_LENGTH (32768)

/* Defines default amount of bins used for a single 64-bit hash-map */
#define HASH64MAP_BINS_AMOUNT (16)

/* Defines default amount of entries in a single 64-bit hash-map's bin */
#define HASH64MAP_BIN_ENTRIES_AMOUNT (16)

#endif /* SYSTEM_CONSTANTS_H */
