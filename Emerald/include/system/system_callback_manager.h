/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef SYSTEM_CALLBACK_MANAGER_H
#define SYSTEM_CALLBACK_MANAGER_H

#include "dll_exports.h"
#include "system_types.h"

/** Defines an arbitrary call-back ID. Call-back IDs should be kept as small as possible. */
typedef unsigned int _callback_id;

typedef enum
{
    CALLBACK_SYNCHRONICITY_ASYNCHRONOUS,
    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,

    CALLBACK_SYNCHRONICITY_UNKNOWN
} _callback_synchronicity;

/** Defines a call-back function pointer type */
typedef void (*PFNSYSTEMCALLBACKPROC)(const void* callback_data, void* user_arg);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void system_callback_manager_add_callback_support(__in __notnull system_callback_manager callback_manager,
                                                         __in           _callback_id            callback_id,
                                                         __in            uint32_t               callback_proc_data_size);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void system_callback_manager_call_back(__in __notnull system_callback_manager callback_manager,
                                              __in           _callback_id            callback_id,
                                              __in __notnull void*                   callback_proc_data);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC system_callback_manager system_callback_manager_create(__in _callback_id max_callback_id);

/** TODO */
PUBLIC void system_callback_manager_subscribe_for_callbacks(__in __notnull system_callback_manager callback_manager,
                                                            __in           _callback_id            callback_id,
                                                            __in __notnull _callback_synchronicity callback_synchronicity,
                                                            __in __notnull PFNSYSTEMCALLBACKPROC   pfn_callback_proc,
                                                            __in __notnull void*                   callback_proc_user_arg);

/** TODO
 *
 *  Internal usage only.
 **/
PUBLIC void system_callback_manager_release(__in __notnull system_callback_manager);

#endif /* SYSTEM_CALLBACK_MANAGER_H */
