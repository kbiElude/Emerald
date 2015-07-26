/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO
 *
 */
#ifndef SYSTEM_FILE_MONITOR_H
#define SYSTEM_FILE_MONITOR_H

#include "system_types.h"

typedef void (*PFNFILECHANGEDETECTEDPROC)(system_hashed_ansi_string file_name,
                                          void*                     user_arg);


/** TODO */
PUBLIC void system_file_monitor_deinit();

/** TODO */
PUBLIC void system_file_monitor_init();

/** TODO.
 *
 *  NOTE: File monitor may issue spurious, platform-specific callbacks. These should be treated as
 *        hints by the user. System-specific caching is likely to interfere with the file monitor.
 *
 *        Always ensure to unregister a subscription, before you proceed with destruction of variables
 *        that the call-back handler could use, were it called.
 *
 */
PUBLIC EMERALD_API void system_file_monitor_monitor_file_changes(system_hashed_ansi_string file_name,
                                                                 bool                      should_enable,
                                                                 PFNFILECHANGEDETECTEDPROC pfn_file_changed_proc,
                                                                 void*                     user_arg);


#endif /* SYSTEM_FILE_MONITOR_H */
