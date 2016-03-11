#ifndef RAL_SCHEDULER_H
#define RAL_SCHEDULER_H

#include "ral/ral_types.h"
#include "system/system_types.h"


typedef void (*PFNRALSCHEDULERCALLBACKPROC)(void* user_arg);

typedef struct ral_scheduler_job_info
{
    /* Call-back to be done from a backend thread */
    PFNRALSCHEDULERCALLBACKPROC pfn_callback_ptr;
    void*                       callback_user_arg;

    /* Call-back to be issued after pfn_callback_ptr returns, from a thread
     * taken from the engine-wide thread pool.
     *
     * NULL permitted.
     */
    PFNSYSTEMTHREADPOOLCALLBACKPROC pfn_callback_when_done_ptr;
    void*                           callback_when_done_user_arg;

    /* Event to set after pfn_callback_ptr returns.
     *
     * NULL permitted.
     */
    system_event signal_event;

    ral_scheduler_job_info()
    {
        callback_user_arg           = NULL;
        callback_when_done_user_arg = NULL;

        pfn_callback_ptr            = NULL;
        pfn_callback_when_done_ptr  = NULL;

        signal_event = NULL;
    }
} ral_scheduler_job_info;


/** TODO */
PUBLIC ral_scheduler ral_scheduler_create();

/** TODO */
PUBLIC void ral_scheduler_free_backend_threads(ral_scheduler    scheduler,
                                               ral_backend_type backend_type);

/** TODO */
PUBLIC void ral_scheduler_release(ral_scheduler scheduler);

/** TODO */
PUBLIC void ral_scheduler_schedule_job(ral_scheduler                 scheduler,
                                       ral_backend_type              backend_type,
                                       const ral_scheduler_job_info& job_info);

/** The function will turn the calling thread into a worker thread, assigned to a pool of threads
 *  for the specified back-end type. It will only be awakened by the scheduler when an async job
 *  is scheduled for execution and assigned to this thread by the scheduler.
 *
 *  To release all threads for a particular backend and return the execution flow for all of them
 *  at once, call ral_scheduler_free_backend_threads().
 *
 *  NOTE: This function should ONLY be called by a rendering back-end.
 **/
PUBLIC void ral_scheduler_use_backend_thread(ral_scheduler    scheduler,
                                             ral_backend_type backend_type);


#endif /* RAL_SCHEDULER_H */