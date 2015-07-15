/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef SYSTEM_THREAD_POOL_H
#define SYSTEM_THREAD_POOL_H

#include "system_types.h"

/** This define should be used to mark all PRIVATE functions that act as task execution handler.
 *  These functions work in multi-threaded environment.
 */
#define THREAD_POOL_TASK_HANDLER volatile


/** Submits a single task for execution by the thread pool. This skips job creation and injects the task
 *  directly into the task queue. Mind that the task may not be instantly executed - this depends on whether
 *  there are tasks of higher priority already scheduled.
 *
 *  @param task Task descriptor.
 */
PUBLIC EMERALD_API void system_thread_pool_submit_single_task(system_thread_pool_task task);

/** Submits a group of tasks for execution by the thread pool. This skips job creation and injects the tasks
 *  directly into the task queue. Mind that the tasks may not be instantly executed - this depends on whether
 *  there are tasks of higher priority already scheduled.
 *
 *  @param task_group Task group.
 */
PUBLIC EMERALD_API void system_thread_pool_submit_single_task_group(system_thread_pool_task_group task_group);


/** Retrieves a task descriptor from internally managed resource pool, configured for specific user requirements.
 *
 *  @param task_priority
 *  @param execution_handler          Function to call back for executing the task.
 *  @param execution_handler_argument Argument to pass with execution_handler.
 *  @param on_finish_handler          Function to call back after task finishes executing.
 *  @param on_finish_handler_argument Argument to pass with on_finish_handler
 *  @param event                      Event to set after task finishes executing.
 *
 *  @return Task descriptor to fill.
 */
PUBLIC EMERALD_API system_thread_pool_task system_thread_pool_create_task_handler_only(system_thread_pool_task_priority     task_priority,
                                                                                       PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                       system_thread_pool_callback_argument execution_handler_argument);

PUBLIC EMERALD_API system_thread_pool_task system_thread_pool_create_task_handler_with_callback(system_thread_pool_task_priority     task_priority,
                                                                                                PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                                system_thread_pool_callback_argument execution_handler_argument,
                                                                                                PFNSYSTEMTHREADPOOLCALLBACKPROC      on_finish_handler,
                                                                                                system_thread_pool_callback_argument on_finish_handler_argument);

PUBLIC EMERALD_API system_thread_pool_task system_thread_pool_create_task_handler_with_event_signal(system_thread_pool_task_priority,
                                                                                                    PFNSYSTEMTHREADPOOLCALLBACKPROC      execution_handler,
                                                                                                    system_thread_pool_callback_argument execution_handler_argument,
                                                                                                    system_event                         event);

/** Creates a task group decriptor.
 *
 *  @param bool True to make the group of tasks distributable, false to have all the tasks
 *              executed sequentially from the same thread.
 *  @return New task group descriptor.
 */
PUBLIC EMERALD_API system_thread_pool_task_group system_thread_pool_create_task_group(bool is_distributable);

/** Releases a task group descriptor. Do not use the descriptor after calling this function.
 *
 *  @param system_thread_pool_task_group Task group to use.
 */
PUBLIC EMERALD_API void system_thread_pool_release_task_group(system_thread_pool_task_group group);

/** Inserts a task into a task group descriptor. The tasks are free to use any priorities they wish. Tasks will be inserted
 *  at once into priority queue, whem submitted (either by means of iterating over job contents or being submitted by user),
 *  so they will execute in no particular order, apart from the one enforced by priortiies.
 *
 *  @param system_thread_pool_task_group Task group to insert the new task in.
 *  @param system_thread_pool_task       Task to insert.
 */
PUBLIC EMERALD_API void system_thread_pool_insert_task_to_task_group(system_thread_pool_task_group group,
                                                                     system_thread_pool_task       task);

/** Initializes thread pool. Should be called once from DLL entry point. */
PUBLIC void _system_thread_pool_init();

/** Deinitializes thread pool. Should be called once from DLL entry point. */
PUBLIC void _system_thread_pool_deinit();

#endif /* SYSTEM_THREAD_POOL_H */
