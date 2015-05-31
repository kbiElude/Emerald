/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_log.h"
#include "system/system_resource_pool.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include "system/system_time.h"

/* Defines */
#define KILL_POOL_WAIT_TABLE_EVENT_ID      (0)
#define TASK_AVAILABLE_WAIT_TABLE_EVENT_ID (1)
#define WAIT_TABLE_SIZE                    (2)

/* Internal type definitions */
typedef enum
{
    ORDER_TYPE_TASK,             /* single task */
    ORDER_TYPE_GROUP_OF_TASKS,   /* multiple tasks */
    ORDER_TYPE_JOB               /* multiple groups of tasks */
} _system_thread_pool_order_type;

typedef struct
{
    /** Event to set on finishing the task. Can be NULL. */
    __maybenull system_event event;
    /** Argument to use for execution call-back function. Can be NULL */
    __maybenull system_thread_pool_callback_argument on_execution_callback_argument;
    /** Function pointer to an execution call-back function. Can be NULL. */
    __notnull PFNSYSTEMTHREADPOOLCALLBACKPROC on_execution_callback;
    /** Argument to use for on-finish call-back function. Can be NULL */
    __maybenull system_thread_pool_callback_argument on_finish_callback_argument;
    /** Function pointer to an on-finish call-back function. Can be NULL. */
    __maybenull PFNSYSTEMTHREADPOOLCALLBACKPROC on_finish_callback;
    /** Priority of the task. Used for task scheduling */
    __in_range(THREAD_POOL_TASK_PRIORITY_FIRST, THREAD_POOL_TASK_PRIORITY_COUNT-1) system_thread_pool_task_priority task_priority;

} _system_thread_pool_task_descriptor;

typedef struct
{
    /** A resizable vector of system_thread_pool_task_descriptors. **/
    __notnull system_resizable_vector tasks;

    /** If true, tasks will be executed in a random order from different worker threads. If false, they will be
     *  executed in order, one after another, from the same worker thread
     */
    bool is_distributable;
} _system_thread_pool_task_group_descriptor;

typedef struct
{
    /* Order type. Determines contents of deref of order_ptr */
    _system_thread_pool_order_type order_type;

    /* Depending on order_type:
     *
     * _system_thread_pool_task_descriptor*       if order_type is ORDER_TYPE_TASK
     * _system_thread_pool_task_group_descriptor* if order_type is ORDER_TYPE_GROUP_OF_TASKS
     * _system_thread_pool_task_job_descriptor*   if order_type is ORDER_TYPE_JOB
     */
    __notnull void* order_ptr; 
} _system_thread_pool_order_descriptor;

typedef _system_thread_pool_order_descriptor* _system_thread_pool_order_descriptor_ptr;

/* Internal variables */
system_event            kill_pool_event                                             =  NULL;
system_event            kill_wait_table           [THREAD_POOL_AMOUNT_OF_THREADS]   = {NULL};
system_resource_pool    order_pool                                                  =  NULL;
system_resizable_vector queued_tasks              [THREAD_POOL_TASK_PRIORITY_COUNT] = {NULL}; /* Always lock queued_tasks_cs before accessing */
LONG                    queued_tasks_counter                                        = 0;
system_critical_section queued_tasks_cs                                             = {NULL};
system_event            task_available_event                                        =  NULL;
system_resource_pool    task_descriptor_pool                                        =  NULL;
system_resource_pool    task_group_descriptor_pool                                  =  NULL;
system_event            threads_spawned_event                                       =  NULL;
system_thread_id        thread_id_array           [THREAD_POOL_AMOUNT_OF_THREADS]   = {NULL};
system_event            wait_table                [WAIT_TABLE_SIZE]                 = {NULL};

/* Internal functions */
/* This procedure implements the process of submitting a single task into a priority queue. The reason
 * it is split from system_thread_pool_submit_single_task() is that it is also called from task execution
 * routine which already handles CS. 
 *
 * @param task_descriptor Descriptor of a task to submit.
 * @param enter_cs        True to enter queued_tasks_cs critical section before submitting, false if no need to.
 *                        False should only be used from private implementation!
 */
PRIVATE inline void _system_thread_pool_submit_single_task(__notnull system_thread_pool_task_descriptor task_descriptor,
                                                                     bool                               enter_cs)
{
    /* Create order descriptor */
    _system_thread_pool_order_descriptor_ptr new_order_descriptor     = (_system_thread_pool_order_descriptor_ptr) system_resource_pool_get_from_pool(order_pool);
    _system_thread_pool_task_descriptor*     task_descriptor_internal = (_system_thread_pool_task_descriptor*)     task_descriptor;

    new_order_descriptor->order_type = ORDER_TYPE_TASK;
    new_order_descriptor->order_ptr  = task_descriptor;

    /* Insert into the queue. */
    if (enter_cs)
    {
        system_critical_section_enter(queued_tasks_cs);
    }
    {
        ASSERT_DEBUG_SYNC(task_descriptor_internal->task_priority >= THREAD_POOL_TASK_PRIORITY_FIRST &&
                          task_descriptor_internal->task_priority  < THREAD_POOL_TASK_PRIORITY_COUNT,
                          "Corrupt task priority [%d]", 
                          task_descriptor_internal->task_priority
                         );

        system_resizable_vector_push(queued_tasks[task_descriptor_internal->task_priority],
                                     new_order_descriptor);

        ::InterlockedIncrement(&queued_tasks_counter);
        system_event_set      (task_available_event);
    }
    if (enter_cs)
    {
        system_critical_section_leave(queued_tasks_cs);
    }
}

/** TODO */
void _init_system_thread_pool_task_descriptor(__in __notnull system_resource_pool_block task_descriptor_block)
{
    /* Left blank for future if needed. */
}

/** TODO */
void _init_system_thread_pool_task_group_descriptor(__in __notnull system_resource_pool_block task_group_descriptor_block)
{
    _system_thread_pool_task_group_descriptor* task_group_descriptor = (_system_thread_pool_task_group_descriptor*) task_group_descriptor_block;

    task_group_descriptor->tasks = system_resizable_vector_create(THREAD_POOL_PREALLOCATED_TASK_SLOTS);

    ASSERT_ALWAYS_SYNC(task_group_descriptor->tasks != NULL,
                       "Could not preallocate task slots for task group descriptor.");
}

/** TODO */
void _deinit_system_thread_pool_task_descriptor(__in __notnull system_resource_pool_block task_descriptor_block)
{
    /* Left blank for future if needed. */
}

/** TODO */
void _deinit_system_thread_pool_task_group_descriptor(__in __notnull system_resource_pool_block task_group_descriptor_block)
{
    _system_thread_pool_task_group_descriptor* task_group_descriptor = (_system_thread_pool_task_group_descriptor*) task_group_descriptor_block;

    system_resizable_vector_release(task_group_descriptor->tasks);
}

/** Procedure that executes a task, according to its descriptor's contents.
 *
 *  @param task_descriptor Descriptor describing how the task should be executed. Cannot be NULL.
 */
inline void _system_thread_pool_worker_execute_task(__notnull _system_thread_pool_task_descriptor* task_descriptor)
{
    /* Decrement the task counter */
    ::InterlockedDecrement(&queued_tasks_counter);

    /* Execute the task */
    task_descriptor->on_execution_callback(task_descriptor->on_execution_callback_argument);

    /* Execute any call-backs that were defined */
    if (task_descriptor->on_finish_callback != NULL)
    {
        task_descriptor->on_finish_callback(task_descriptor->on_finish_callback_argument);
    }

    if (task_descriptor->event != NULL)
    {
        system_event_set(task_descriptor->event);
    }
}

/** Procedure that either executes tasks one after another (if task group is not marked as distributable) or distributes the tasks
 *  for imminent execution to all available worker threads.
 *
 *  @param task_group_descriptor Task group descriptor. Cannot be NULL.
 */
inline void _system_thread_pool_worker_execute_task_group(__notnull _system_thread_pool_task_group_descriptor* task_group_descriptor)
{
    system_resizable_vector& tasks_vector = task_group_descriptor->tasks;
    unsigned int             n_tasks      = system_resizable_vector_get_amount_of_elements(tasks_vector);

    if (task_group_descriptor->is_distributable)
    {
        /* Order does not matter - distribute the tasks through all available worker threads. */
        while (true)
        {
            /* Popping is the fastest operation for the resizable vectors */
            _system_thread_pool_task_descriptor* task_descriptor = NULL;
            bool                                 result_get      = false;

            result_get = system_resizable_vector_pop(task_group_descriptor->tasks,
                                                    &task_descriptor);

            if (task_descriptor != NULL)
            {
                _system_thread_pool_submit_single_task( (system_thread_pool_task_descriptor) task_descriptor,
                                                        false);
            }
            else
            {
                if (result_get)
                {
                    ASSERT_DEBUG_SYNC(system_resizable_vector_get_amount_of_elements(task_group_descriptor->tasks) == 0,
                                      "Null task descriptor reported even though there are more tasks available!");
                }

                break;
            }
        }
    }
    else
    {
        /* Order matters - we can only execute the tasks in the current thread, one by one */
        for (unsigned int n_task = 0;
                          n_task < n_tasks;
                        ++n_task)
        {
            _system_thread_pool_task_descriptor* task_descriptor = NULL;
            bool                                 result_get      = false;

            result_get = system_resizable_vector_get_element_at(task_group_descriptor->tasks,
                                                                n_task,
                                                               &task_descriptor);

            ASSERT_DEBUG_SYNC(result_get,
                              "Could not retrieve task descriptor");
            ASSERT_ALWAYS_SYNC(task_descriptor != NULL,
                               "Task descriptor is null!");

            _system_thread_pool_worker_execute_task(task_descriptor);

            system_resource_pool_return_to_pool(task_descriptor_pool,
                                                (system_resource_pool_block) task_descriptor);
        }

        system_resizable_vector_empty(task_group_descriptor->tasks);
    }
}

/** Procedure that searches through enqueued tasks (starting with the highest priority tasks, and moving
 *  on to the lowest priority ones). After findign a task, it handles it accordingly to its type.
 *
 **/
PRIVATE inline void _system_thread_pool_worker_find_and_execute_available_task()
{
    system_critical_section_enter(queued_tasks_cs);
    {
        bool needs_to_leave_queued_tasks_cs = true;

        /* Obtain an available task that is of the highest priority */
        void* order_ptr = NULL;

        for (system_thread_pool_task_priority current_priority  = (system_thread_pool_task_priority) (THREAD_POOL_TASK_PRIORITY_COUNT - 1);
                                              current_priority >= THREAD_POOL_TASK_PRIORITY_FIRST;
                                              ((int&)current_priority) --)
        {
            system_resizable_vector& queue = queued_tasks[current_priority];

            bool result_get = system_resizable_vector_pop(queue,
                                                         &order_ptr);

            if (result_get &&
                order_ptr != NULL)
            {
                break;
            }
        }

        /* Handle the order */
        if (order_ptr != NULL)
        {
            _system_thread_pool_order_descriptor_ptr order_descriptor = (_system_thread_pool_order_descriptor_ptr) order_ptr;

            switch(order_descriptor->order_type)
            {
                case ORDER_TYPE_TASK:
                {
                    /* Leave the CS - we will be directly executing the task now so no need to keep the lock on */
                    system_critical_section_leave(queued_tasks_cs);

                    needs_to_leave_queued_tasks_cs = false;

                    /* Execute the task now */
                    _system_thread_pool_worker_execute_task((_system_thread_pool_task_descriptor*) order_descriptor->order_ptr);

                    /* Return the descriptor back to the pool */
                    system_resource_pool_return_to_pool(task_descriptor_pool,
                                                        (system_resource_pool_block) order_descriptor->order_ptr);

                    break;
                }

                case ORDER_TYPE_GROUP_OF_TASKS:
                {
                    /* Queue the encapsulated tasks. */
                    _system_thread_pool_worker_execute_task_group((_system_thread_pool_task_group_descriptor*) order_descriptor->order_ptr);

                    /* Leave the CS */
                    system_critical_section_leave(queued_tasks_cs);

                    needs_to_leave_queued_tasks_cs = false;

                    /* Return the descriptor back to the pool */
                    system_resource_pool_return_to_pool(task_group_descriptor_pool,
                                                        (system_resource_pool_block) order_descriptor->order_ptr);

                    break;
                }

                case ORDER_TYPE_JOB: /* TODO TODO TODO */
                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unrecognized order type [%d]",
                                       order_descriptor->order_type);

                    system_critical_section_leave(queued_tasks_cs);
                }
            } /* switch(order_descriptor->order_type) */
        } /* if (raw_ptr != NULL) */

        if (needs_to_leave_queued_tasks_cs)
        {
            system_critical_section_leave(queued_tasks_cs);
        }
    }
}

/** Entry point for every thread pool worker. Waits for task notifications and handles
 *  submitted tasks / tasks groups or jobs.
 *
 *  @param system_threads_entry_point_argument Not used.
 *
 **/
PRIVATE void _system_thread_pool_worker_entrypoint(system_threads_entry_point_argument)
{
    system_thread_id current_thread_id = system_threads_get_thread_id();
    bool             should_live       = true;

    /* Enter wait loop */
    LOG_INFO("Worker thread [%d] starting.",
             current_thread_id);
    {
        /* Wait till all thread events become available */
        system_event_wait_single(threads_spawned_event);

        while (should_live)
        {
            /* Wait for:
             *
             * - kill event signal (wakes up all threads)
             * - job arrived signal (wakes up one thread at a time)
             **/
            size_t event_index = 0;

            if (queued_tasks_counter != 0)
            {
                /* Another task CAN still be available - check up on that. */
                event_index = TASK_AVAILABLE_WAIT_TABLE_EVENT_ID;
            }
            else
            {
                /* Most likely no tasks around - wait for an event */
                event_index = system_event_wait_multiple(wait_table,
                                                         WAIT_TABLE_SIZE,
                                                         false,
                                                         SYSTEM_TIME_INFINITE,
                                                         NULL); /* out_result_ptr */
            }

            switch (event_index)
            {
                case KILL_POOL_WAIT_TABLE_EVENT_ID:
                {
                    should_live = false;

                    break;
                } /* case KILL_POOL_WAIT_TABLE_EVENT_ID:*/

                case TASK_AVAILABLE_WAIT_TABLE_EVENT_ID:
                {
                    _system_thread_pool_worker_find_and_execute_available_task();

                    break;
                } /* case TASK_AVAILABLE_WAIT_TABLE_EVENT_ID:*/

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Should never happen");
                } /* default: */
            }
        }
    }
    LOG_INFO("Worker thread [%d] ending.", current_thread_id);
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_pool_task_descriptor system_thread_pool_create_task_descriptor_handler_only(system_thread_pool_task_priority     task_priority,
                                                                                                             PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                                             system_thread_pool_callback_argument execution_handler_argument)
{
    _system_thread_pool_task_descriptor* result = (_system_thread_pool_task_descriptor*) system_resource_pool_get_from_pool(task_descriptor_pool);

    result->event                          = NULL;
    result->on_execution_callback          = execution_handler;
    result->on_execution_callback_argument = execution_handler_argument;
    result->on_finish_callback             = NULL;
    result->on_finish_callback_argument    = NULL;
    result->task_priority                  = task_priority;

    return (system_thread_pool_task_descriptor) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_pool_task_descriptor system_thread_pool_create_task_descriptor_handler_with_callback(system_thread_pool_task_priority     task_priority,
                                                                                                                      PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                                                      system_thread_pool_callback_argument execution_handler_argument,
                                                                                                                      PFNSYSTEMTHREADPOOLCALLBACKPROC      on_finish_handler,
                                                                                                                      system_thread_pool_callback_argument on_finish_handler_argument)
{
    _system_thread_pool_task_descriptor* result = (_system_thread_pool_task_descriptor*) system_resource_pool_get_from_pool(task_descriptor_pool);

    result->event                          = NULL;
    result->on_execution_callback          = execution_handler;
    result->on_execution_callback_argument = execution_handler_argument;
    result->on_finish_callback             = on_finish_handler;
    result->on_finish_callback_argument    = on_finish_handler_argument;
    result->task_priority                  = task_priority;

    return (system_thread_pool_task_descriptor) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_pool_task_descriptor system_thread_pool_create_task_descriptor_handler_with_event_signal(system_thread_pool_task_priority     task_priority,
                                                                                                                          PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                                                          system_thread_pool_callback_argument execution_handler_argument,
                                                                                                                          system_event                         event)
{
    _system_thread_pool_task_descriptor* result = (_system_thread_pool_task_descriptor*) system_resource_pool_get_from_pool(task_descriptor_pool);

    result->event                          = event;
    result->on_execution_callback          = execution_handler;
    result->on_execution_callback_argument = execution_handler_argument;
    result->on_finish_callback             = NULL;
    result->on_finish_callback_argument    = NULL;
    result->task_priority                  = task_priority;

    return (system_thread_pool_task_descriptor) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_pool_task_group_descriptor system_thread_pool_create_task_group_descriptor(bool is_distributable)
{
    _system_thread_pool_task_group_descriptor* result = (_system_thread_pool_task_group_descriptor*) system_resource_pool_get_from_pool(task_group_descriptor_pool);

    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Could not create task group descriptor.");

    if (result != NULL)
    {
        ASSERT_DEBUG_SYNC(result->tasks != NULL,
                          "Tasks not allocated for a task group descriptor.");

        if (result->tasks != NULL)
        {
            ASSERT_DEBUG_SYNC(system_resizable_vector_get_amount_of_elements(result->tasks) == 0,
                              "Task group descriptor already cotnains task(s)!");
        }

        result->is_distributable = is_distributable;
    }

    return (system_thread_pool_task_group_descriptor) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_thread_pool_submit_single_task(__notnull system_thread_pool_task_descriptor task_descriptor)
{
    _system_thread_pool_submit_single_task(task_descriptor,
                                           true); /* enter_cs */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_thread_pool_submit_single_task_group(__notnull system_thread_pool_task_group_descriptor task_descriptor)
{
    /* Create order descriptor */
    _system_thread_pool_order_descriptor_ptr   new_order_descriptor           = (_system_thread_pool_order_descriptor_ptr)   system_resource_pool_get_from_pool(order_pool);
    _system_thread_pool_task_group_descriptor* task_group_descriptor_internal = (_system_thread_pool_task_group_descriptor*) task_descriptor;

    new_order_descriptor->order_type = ORDER_TYPE_GROUP_OF_TASKS;
    new_order_descriptor->order_ptr  = task_descriptor;

    /* Insert into the queue. */
    system_critical_section_enter(queued_tasks_cs);
    {
        /* De-batching of the tasks should be performed with critical priority */
        system_resizable_vector_push(queued_tasks[THREAD_POOL_TASK_PRIORITY_CRITICAL],
                                     new_order_descriptor);
        system_event_set            (task_available_event);
    }
    system_critical_section_leave(queued_tasks_cs);
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_thread_pool_release_task_group_descriptor(__deallocate(mem) system_thread_pool_task_group_descriptor descriptor)
{
    ASSERT_DEBUG_SYNC(task_group_descriptor_pool != NULL, "Task group descriptor pool is NULL.");

    if (task_group_descriptor_pool != NULL)
    {
        system_resource_pool_return_to_pool(task_group_descriptor_pool,
                                            (system_resource_pool_block) descriptor);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_thread_pool_insert_task_to_task_group(system_thread_pool_task_group_descriptor task_group_descriptor,
                                                                     system_thread_pool_task_descriptor       task_descriptor)
{
    _system_thread_pool_task_group_descriptor* descriptor = (_system_thread_pool_task_group_descriptor*) task_group_descriptor;

    system_resizable_vector_push(descriptor->tasks,
                                 task_descriptor);
}

/** Please see header for specification */
PUBLIC void _system_thread_pool_init()
{
    ASSERT_ALWAYS_SYNC(task_descriptor_pool == NULL,
                       "Task descriptor pool has already been initialized.");
    ASSERT_ALWAYS_SYNC(task_group_descriptor_pool == NULL,
                       "Task group descriptor pool has already been initialized.");
    ASSERT_ALWAYS_SYNC(kill_pool_event == NULL,
                       "Kill pool event already initialized.");
    ASSERT_ALWAYS_SYNC(queued_tasks_cs == NULL,
                       "Queued tasks cs already initialized.");
    ASSERT_ALWAYS_SYNC(threads_spawned_event == NULL,
                       "Threads spawned event already initialized.");
    ASSERT_ALWAYS_SYNC(task_available_event == NULL,
                       "Task available event already initialized.");
    ASSERT_ALWAYS_SYNC(order_pool == NULL,
                       "Order pool already initialized.");

    if (task_descriptor_pool       == NULL &&
        task_group_descriptor_pool == NULL &&
        kill_pool_event            == NULL &&
        queued_tasks_cs            == NULL &&
        threads_spawned_event      == NULL && 
        task_available_event       == NULL &&
        order_pool                 == NULL)
    {
        order_pool                 = system_resource_pool_create(sizeof(_system_thread_pool_order_descriptor),
                                                                 THREAD_POOL_PREALLOCATED_ORDER_DESCRIPTORS,
                                                                 0,  /* init_fn   */
                                                                 0); /* deinit_fn */
        task_descriptor_pool       = system_resource_pool_create(sizeof(_system_thread_pool_task_descriptor),
                                                                 THREAD_POOL_PREALLOCATED_TASK_DESCRIPTORS,
                                                                 _init_system_thread_pool_task_descriptor,
                                                                 _deinit_system_thread_pool_task_descriptor);
        task_group_descriptor_pool = system_resource_pool_create(sizeof(_system_thread_pool_task_group_descriptor),
                                                                 THREAD_POOL_PREALLOCATED_TASK_GROUP_DESCRIPTORS,
                                                                 _init_system_thread_pool_task_group_descriptor,
                                                                 _deinit_system_thread_pool_task_group_descriptor);
        kill_pool_event            = system_event_create        (true); /* manual_reset */
        threads_spawned_event      = system_event_create        (true); /* manual_reset */
        task_available_event       = system_event_create        (true); /* manual_reset */

        for (size_t n_priority = THREAD_POOL_TASK_PRIORITY_FIRST;
                    n_priority < THREAD_POOL_TASK_PRIORITY_COUNT;
                  ++n_priority)
        {
            queued_tasks[n_priority] = system_resizable_vector_create(THREAD_POOL_PREALLOCATED_TASK_CONTAINERS);

            ASSERT_ALWAYS_SYNC(queued_tasks[n_priority] != NULL,
                               "Could not create a queued tasks resizable vector.");
        }

        queued_tasks_cs = system_critical_section_create();

        ASSERT_ALWAYS_SYNC(order_pool != NULL,
                           "Order pool could not have been created.");
        ASSERT_ALWAYS_SYNC(task_descriptor_pool != NULL,
                           "Task descriptor pool could not have been created.");
        ASSERT_ALWAYS_SYNC(task_group_descriptor_pool != NULL,
                           "Task group descriptor pool could not have been created.");
        ASSERT_ALWAYS_SYNC(kill_pool_event != NULL,
                           "Could not initialize kill pool event.");
        ASSERT_ALWAYS_SYNC(queued_tasks_cs != NULL,
                           "Could not initialize queued tasks CS.");
        ASSERT_ALWAYS_SYNC(threads_spawned_event != NULL,
                           "Could not initialize threads spawned event.");
        ASSERT_ALWAYS_SYNC(task_available_event != NULL,
                           "Could not initialize task available event.");

        if (task_descriptor_pool       != NULL &&
            task_group_descriptor_pool != NULL &&
            kill_pool_event            != NULL &&
            threads_spawned_event      != NULL &&
            task_available_event       != NULL &&
            queued_tasks_cs            != NULL)
        {
            /* Got to spawn thread pool worker threads now. */
            for (unsigned int n_thread = 0;
                              n_thread < THREAD_POOL_AMOUNT_OF_THREADS;
                            ++n_thread)
            {
                thread_id_array[n_thread] = system_threads_spawn(_system_thread_pool_worker_entrypoint,
                                                                 NULL,
                                                                 kill_wait_table + n_thread);

                ASSERT_ALWAYS_SYNC(thread_id_array[n_thread] != NULL,
                                   "Could not spawn worker thread [%d] !",
                                   n_thread);
            }

            wait_table[KILL_POOL_WAIT_TABLE_EVENT_ID     ] = kill_pool_event;
            wait_table[TASK_AVAILABLE_WAIT_TABLE_EVENT_ID] = task_available_event;

            /* Wait table is ready - signal the event */
            system_event_set(threads_spawned_event);
        }
    }
}

/** Please see header for specification */
PUBLIC void _system_thread_pool_deinit()
{
    /* TODO: There's a bug somewhere that causes the wait below to lock up if we used
     *       an infinite stall. */
    const system_timeline_time timeout_time = system_time_get_timeline_time_for_s(5);
    bool                       wait_result  = false;

    /* Set the kill event and wait till all worker threads die out */
    system_event_set          (kill_pool_event);
    system_event_wait_multiple(kill_wait_table,
                               THREAD_POOL_AMOUNT_OF_THREADS,
                               true,
                               timeout_time,
                              &wait_result);

    /* All threads should have died by now. Deinit all remaining objects */
    system_resource_pool_release(order_pool);
    system_resource_pool_release(task_descriptor_pool);
    system_resource_pool_release(task_group_descriptor_pool);
    system_event_release        (kill_pool_event);
    system_event_release        (threads_spawned_event);
    system_event_release        (task_available_event);

    system_critical_section_enter  (queued_tasks_cs);
    system_critical_section_leave  (queued_tasks_cs);
    system_critical_section_release(queued_tasks_cs);

    for (unsigned int n_thread = 0;
                      n_thread < THREAD_POOL_AMOUNT_OF_THREADS;
                    ++n_thread)
    {
        system_event_release(kill_wait_table[n_thread]);
    }
}
