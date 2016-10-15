/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_CRITICAL_LOG_H
#define SYSTEM_CRITICAL_LOG_H

#include <string.h>
#include "system_constants.h"
#include "system_critical_section.h"
#include "system_types.h"

/* TODO: Use TLS */
#ifdef EMERALD_EXPORTS
    extern system_critical_section log_file_handle_cs;
#else
    static system_critical_section log_file_handle_cs = system_critical_section_create();
#endif

#define _LOG(include_prefix, level,file,line,text,...)        \
    if (level >= LOGLEVEL_BASE && log_file_handle_cs != NULL) \
    {                                                         \
        system_critical_section_enter(log_file_handle_cs);    \
                                                              \
        static char log_helper[LOG_MAX_LENGTH];               \
                                                              \
        memset   (log_helper,                                 \
                  0,                                          \
                  LOG_MAX_LENGTH);                            \
                                                              \
        if (include_prefix)                                   \
            snprintf (log_helper,                             \
                      LOG_MAX_LENGTH,                         \
                      "[File %s // line %d]: " text,          \
                      file,                                   \
                      line,                                   \
                    ##__VA_ARGS__);                           \
        else                                                  \
            snprintf (log_helper,                             \
                      LOG_MAX_LENGTH,                         \
                      text,                                   \
                    ##__VA_ARGS__);                           \
                                                              \
        system_log_write(level,                               \
                         log_helper,                          \
                         include_prefix);                     \
                                                              \
        system_critical_section_leave(log_file_handle_cs);    \
    }

#define LOG_TRACE(text,...) _LOG(true,                 \
                                 LOGLEVEL_TRACE,       \
                                 __FILE__,             \
                                 __LINE__,             \
                                 text,                 \
                              ##__VA_ARGS__)
#define LOG_INFO(text,...)  _LOG(true,                 \
                                 LOGLEVEL_INFORMATION, \
                                 __FILE__,             \
                                 __LINE__,             \
                                 text,                 \
                               ##__VA_ARGS__)
#define LOG_ERROR(text,...) _LOG(true,                 \
                                 LOGLEVEL_ERROR,       \
                                 __FILE__,             \
                                 __LINE__,             \
                                 text,                 \
                               ##__VA_ARGS__)
#define LOG_FATAL(text,...) _LOG(true,                 \
                                 LOGLEVEL_FATAL,       \
                                 __FILE__,             \
                                 __LINE__,             \
                                 text,                 \
                               ##__VA_ARGS__)
#define LOG_RAW(text,...)   _LOG(false,                \
                                 LOGLEVEL_FATAL,       \
                                 __FILE__,             \
                                 __LINE__,             \
                                 text,                 \
                               ##__VA_ARGS__)

/** Creates a new log file, initalizes cache, starts logging thread.
 *  Should only be started once.
 *
 *  NOTE: This function is not exported - it should be launched from
 *        global init() only.
 */
PUBLIC void _system_log_init();

/** Closes the log file, shuts down logging thread, frees cache.
 *  Should only be called once.
 *
 *  NOTE: This function is not expotred - it should be launched from
 *        global deinit() only.
 */
PUBLIC void _system_log_deinit();

/** Inserts new entry into a log file. 
 *
 *  NOTE: This function DOES NOT perform any removal of incoming entries.
 *        Levels are only used to prefixing the entry with level info.
 *
 *  @param system_log_priority Entry priority
 *  @param const char*         Entry
 */
EMERALD_API void system_log_write(system_log_priority,
                                  const char*,
                                  bool);

#endif /* SYSTEM_CRITICAL_LOG_H */
