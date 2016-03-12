/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_scheduler.h"
#include "system/system_event.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_semaphore.h"
#include "system/system_thread_pool.h"

#define N_MAX_SCHEDULABLE_JOBS 1024


typedef struct _ral_scheduler_backend
{
    system_semaphore        job_available_semaphore;
    system_resource_pool    job_pool;
    system_resizable_vector jobs;
    system_read_write_mutex jobs_mutex;
    volatile unsigned int   n_threads_active;
    system_event            please_leave_event;

    _ral_scheduler_backend()
    {
        job_available_semaphore = system_semaphore_create       (N_MAX_SCHEDULABLE_JOBS, /* semaphore_capacity      */
                                                                  0);                    /* semaphore_default_value */
        job_pool                = system_resource_pool_create   (sizeof(ral_scheduler_job_info),
                                                                 4,     /* n_elements_to_preallocate */
                                                                 NULL,  /* init_fn                   */
                                                                 NULL); /* deinit_fn                 */
        jobs                    = system_resizable_vector_create(4);    /* capacity                  */
        jobs_mutex              = system_read_write_mutex_create();
        n_threads_active        = 0;
        please_leave_event      = system_event_create           (true); /* manual_reset              */
    }

    ~_ral_scheduler_backend()
    {
        ASSERT_DEBUG_SYNC(n_threads_active == 0,
                          "RAL scheduler shutting down, even though %d backend threads are still up.",
                          n_threads_active);

        if (job_available_semaphore != NULL)
        {
            system_semaphore_release(job_available_semaphore);

            job_available_semaphore = NULL;
        }

        if (job_pool != NULL)
        {
            system_resource_pool_release(job_pool);

            job_pool = NULL;
        }

        if (jobs != NULL)
        {
            system_resizable_vector_release(jobs);

            jobs = NULL;
        }

        if (jobs_mutex != NULL)
        {
            system_read_write_mutex_release(jobs_mutex);

            jobs_mutex = NULL;
        }

        if (please_leave_event != NULL)
        {
            system_event_release(please_leave_event);

            please_leave_event = NULL;
        }
    }
} _ral_scheduler_backend;

typedef struct _ral_scheduler
{
    _ral_scheduler_backend backends[RAL_BACKEND_TYPE_COUNT];
} _ral_scheduler;


/** Please see header for specification */
PUBLIC ral_scheduler ral_scheduler_create()
{
    _ral_scheduler* scheduler_ptr = new (std::nothrow) _ral_scheduler;

    ASSERT_ALWAYS_SYNC(scheduler_ptr != NULL,
                       "Out of memory");

    return (ral_scheduler) scheduler_ptr;
}

/** Please see header for specification */
PUBLIC void ral_scheduler_free_backend_threads(ral_scheduler    scheduler,
                                               ral_backend_type backend_type)
{
    _ral_scheduler*         scheduler_ptr = (_ral_scheduler*) scheduler;
    _ral_scheduler_backend* backend_ptr   = scheduler_ptr->backends + backend_type;

    system_read_write_mutex_lock(backend_ptr->jobs_mutex,
                                 ACCESS_WRITE);
    {
        system_semaphore_leave_multiple(backend_ptr->job_available_semaphore,
                                        backend_ptr->n_threads_active);

        system_event_set(backend_ptr->please_leave_event);
    }
    system_read_write_mutex_unlock(backend_ptr->jobs_mutex,
                                   ACCESS_WRITE);

    /* Spin until all threads sign out */
    while (backend_ptr->n_threads_active != 0);
}

/** Please see header for specification */
PUBLIC void ral_scheduler_release(ral_scheduler scheduler)
{
    delete (_ral_scheduler*) scheduler;
}

/** Please see header for specification */
PUBLIC void ral_scheduler_schedule_job(ral_scheduler                 scheduler,
                                       ral_backend_type              backend_type,
                                       const ral_scheduler_job_info& job_info)
{
    _ral_scheduler*         scheduler_ptr = (_ral_scheduler*) scheduler;
    _ral_scheduler_backend* backend_ptr   = scheduler_ptr->backends + backend_type;

    system_read_write_mutex_lock(backend_ptr->jobs_mutex,
                                 ACCESS_WRITE);
    {
        ral_scheduler_job_info* new_job_ptr = (ral_scheduler_job_info*) system_resource_pool_get_from_pool(backend_ptr->job_pool);

        *new_job_ptr = job_info;

        #ifdef _DEBUG
        {
            uint32_t n_jobs_scheduled = 0;

            system_resizable_vector_get_property(backend_ptr->jobs,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 &n_jobs_scheduled);

            ASSERT_DEBUG_SYNC(n_jobs_scheduled < N_MAX_SCHEDULABLE_JOBS - 1,
                              "Maximum number of schedulable jobs reached");
        }
        #endif

        system_resizable_vector_push(backend_ptr->jobs,
                                     new_job_ptr);
        system_semaphore_leave      (backend_ptr->job_available_semaphore);
    }
    system_read_write_mutex_unlock(backend_ptr->jobs_mutex,
                                   ACCESS_WRITE);
}

/** Please see header for specification */
PUBLIC void ral_scheduler_use_backend_thread(ral_scheduler    scheduler,
                                             ral_backend_type backend_type)
{
    _ral_scheduler*         scheduler_ptr = (_ral_scheduler*) scheduler;
    _ral_scheduler_backend* backend_ptr   = scheduler_ptr->backends + backend_type;

    system_atomics_increment(&backend_ptr->n_threads_active);
    {
        ral_scheduler_job_info* job_ptr     = NULL;
        bool                    should_live = true;

        while (should_live)
        {
            system_semaphore_enter(backend_ptr->job_available_semaphore,
                                   SYSTEM_TIME_INFINITE);

            if (system_event_wait_single_peek(backend_ptr->please_leave_event) )
            {
                should_live = false;

                continue;
            }

            system_read_write_mutex_lock(backend_ptr->jobs_mutex,
                                         ACCESS_WRITE);
            {
                system_resizable_vector_pop(backend_ptr->jobs,
                                           &job_ptr);
            }
            system_read_write_mutex_unlock(backend_ptr->jobs_mutex,
                                           ACCESS_WRITE);

            /* Fire the call-back */
            job_ptr->pfn_callback_ptr(job_ptr->callback_user_arg);

            if (job_ptr->pfn_callback_when_done_ptr != NULL)
            {
                system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                           job_ptr->pfn_callback_when_done_ptr,
                                                                                           job_ptr->callback_when_done_user_arg);

                system_thread_pool_submit_single_task(task);
            }

            if (job_ptr->signal_event != NULL)
            {
                system_event_set(job_ptr->signal_event);
            }

            /* Return the descriptor back to the pool for recycling */
            system_resource_pool_return_to_pool(backend_ptr->job_pool,
                                                (system_resource_pool_block) job_ptr);
        };
    }
    system_atomics_decrement(&backend_ptr->n_threads_active);
}