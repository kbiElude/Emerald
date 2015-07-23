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

/** TODO */
PUBLIC void system_file_monitor_monitor_file_changes(system_hashed_ansi_string file_name,
                                                     bool                      should_enable,
                                                     PFNFILECHANGEDETECTEDPROC pfn_file_changed_proc,
                                                     void*                     user_arg);


#endif /* SYSTEM_FILE_MONITOR_H */
