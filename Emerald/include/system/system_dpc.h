/**
 *
 * Emerald (kbi/elude @2016)
 *
 * Internal usage only.
 */
#ifndef SYSTEM_DPC_H
#define SYSTEM_DPC_H

#include "system/system_types.h"

typedef void (*PFNSYSTEMDPCCALLBACKPROC)(void* object,
                                         void* callback_arg);

/** TODO */
PUBLIC void system_dpc_deinit();

/** TODO */
PUBLIC void system_dpc_init();

/** TODO */
PUBLIC bool system_dpc_is_object_pending_callback(void* object);

/** TODO
 *
 *  NOTE: Requires an initialized global thread pool!
 */
PUBLIC void system_dpc_schedule(void*                    object,
                                PFNSYSTEMDPCCALLBACKPROC pfn_callback_proc,
                                void*                    callback_arg);

#endif /* SYSTEM_DPC_H */