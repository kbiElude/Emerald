/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_thread_pool.h"
#include "shared.h"
#include "system/system_event.h"
#include "system/system_threads.h"
#include "system/system_thread_pool.h"
#include "gtest/gtest.h"

struct few_simple_tasks_submitted_separately_argument
{
    unsigned char*   test_buffer;
    unsigned int     n;
    system_event     wait_event;
    system_thread_id worker_thread_id;
};

struct few_complex_tasks_submitted_separately_argument
{
    system_event     wait_event;
    unsigned int     cnt_executions;
    system_thread_id worker_thread_id;
};


THREAD_POOL_TASK_HANDLER void _few_simple_tasks_submitted_separately_worker(void* arg)
{
    few_simple_tasks_submitted_separately_argument* input = (few_simple_tasks_submitted_separately_argument*) arg;

    for (unsigned int index = input->n * 64;
                      index < (input->n+1)*64;
                    ++index)
    {
        input->test_buffer[index] = index;
    }

    input->worker_thread_id = system_threads_get_thread_id();

    system_event_set(input->wait_event);
}

THREAD_POOL_TASK_HANDLER void _few_complex_tasks_submitted_separately_worker(void* arg)
{
    few_complex_tasks_submitted_separately_argument* input = (few_complex_tasks_submitted_separately_argument*) arg;

    ::Sleep(250);

    input->cnt_executions++;
    input->worker_thread_id = system_threads_get_thread_id();

    system_event_set(input->wait_event);
}


/****************************** TESTS ***********************************/
TEST(ThreadPoolTest, FewSimpleTasksSubmittedSeparately)
{
    unsigned char test_buffer[256] = {0};
    system_event  wait_events[4]   = {0};

    /* set up */
    ZeroMemory(test_buffer,
               256);

    for (unsigned int n = 0;
                      n < 4;
                      n++)
    {
        wait_events[n] = system_event_create(true); /* manual_reset */
    }

    /* go */
    for (unsigned int n = 0;
                      n < 4;
                      n++)
    {
        /* Alloc input */
        few_simple_tasks_submitted_separately_argument* input = new few_simple_tasks_submitted_separately_argument;

        input->n           = n;
        input->test_buffer = test_buffer;
        input->wait_event  = wait_events[n];

        /* Create task descriptor */
        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                    _few_simple_tasks_submitted_separately_worker,
                                                                                                                    input);

        ASSERT_TRUE(task_descriptor != NULL);

        system_thread_pool_submit_single_task(task_descriptor);
    }

    /* wait till all tasks finish */
    system_event_wait_multiple_infinite(wait_events,
                                        4,     /* n_elements */
                                        true); /* wait_on_all_objects */

    for (unsigned int n = 0;
                      n < 256;
                      n++)
    {
        ASSERT_TRUE(test_buffer[n] == n);
    }

    /* clean up */
    for (unsigned int n = 0;
                      n < 4;
                    ++n)
    {
        system_event_release(wait_events[n]);
    }
}

TEST(ThreadPoolTest, FewComplexTasksSubmittedSeparately)
{
    system_event wait_events[16] = {0};

    /* set up */
    for (unsigned int n = 0;
                      n < 16;
                      n++)
    {
        wait_events[n] = system_event_create(true); /* manual_reset */
    }

    /* go */
    few_complex_tasks_submitted_separately_argument inputs[16];

    for (unsigned int n = 0;
                      n < 16;
                      n++)
    {
        /* Alloc input */
        inputs[n].cnt_executions = 0;
        inputs[n].wait_event     = wait_events[n];

        /* Create task descriptor */
        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                    _few_complex_tasks_submitted_separately_worker,
                                                                                                                    inputs + n);

        ASSERT_TRUE(task_descriptor != NULL);

        system_thread_pool_submit_single_task(task_descriptor);
    }

    /* wait till all tasks finish */
    system_event_wait_multiple_infinite(wait_events,
                                        16,          /* n_elements */
                                        true);       /* wait_on_all_objects */

    for (unsigned int n = 0;
                      n < 16;
                    ++n)
    {
        ASSERT_EQ(inputs[n].cnt_executions, 1);

        printf("[n]: thread=%d\n",
               inputs[n].worker_thread_id);
    }

    /* clean up */
    for (unsigned int n = 0; n < 16; ++n)
    {
        system_event_release(wait_events[n]);
    }
}

TEST(ThreadPoolTest, FewSimpleNonDistributableTasksInGroupTest)
{
    unsigned char test_buffer[256] = {0};
    system_event  wait_events[4]   = {0};

    /* set up */
    ZeroMemory(test_buffer, 256);

    for (unsigned int n = 0; n < 4; n++)
    {
        wait_events[n] = system_event_create(true); /* manual_reset */
    }

    /* go */
    system_thread_pool_task_group_descriptor       task_group_descriptor = system_thread_pool_create_task_group_descriptor(false);
    few_simple_tasks_submitted_separately_argument inputs[4];

    for (unsigned int n = 0; n < 4; n++)
    {
        /* Alloc input */
        inputs[n].n           = n;
        inputs[n].test_buffer = test_buffer;
        inputs[n].wait_event  = wait_events[n];

        /* Create task descriptor */
        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL, _few_simple_tasks_submitted_separately_worker, inputs + n);
        ASSERT_TRUE(task_descriptor != NULL);

        system_thread_pool_insert_task_to_task_group(task_group_descriptor, task_descriptor);
    }

    system_thread_pool_submit_single_task_group(task_group_descriptor);

    /* wait till all tasks finish */
    system_event_wait_multiple_infinite(wait_events, 4, true);

    for (unsigned int n = 0; n < 256; n++)
    {
        ASSERT_TRUE(test_buffer[n] == n);
    }

    for (unsigned int n = 1; n < 4; ++n)
    {
        ASSERT_EQ(inputs[0].worker_thread_id, inputs[n].worker_thread_id);
    }

    /* clean up */
    for (unsigned int n = 0; n < 4; ++n)
    {
        system_event_release(wait_events[n]);
    }
}

TEST(ThreadPoolTest, FewComplexDistributableTasksInGroupTest)
{
    system_event wait_events[16] = {0};

    /* set up */
    for (unsigned int n = 0; n < 16; n++)
    {
        wait_events[n] = system_event_create(true); /* manual_reset */
    }

    /* go */
    system_thread_pool_task_group_descriptor        task_group_descriptor = system_thread_pool_create_task_group_descriptor(true);
    few_complex_tasks_submitted_separately_argument inputs[16];

    for (unsigned int n = 0; n < 16; n++)
    {
        /* Alloc input */
        inputs[n].cnt_executions = 0;
        inputs[n].wait_event     = wait_events[n];

        /* Create task descriptor */
        system_thread_pool_task_descriptor task_descriptor = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL, _few_complex_tasks_submitted_separately_worker, inputs + n);
        ASSERT_TRUE(task_descriptor != NULL);

        system_thread_pool_insert_task_to_task_group(task_group_descriptor, task_descriptor);
    }

    system_thread_pool_submit_single_task_group(task_group_descriptor);

    /* wait till all tasks finish */
    system_event_wait_multiple_infinite(wait_events, 16, true);

    /* check if there was at least one work package that was handled by a different thread */
    bool all_packets_handled_by_the_same_thread = true;

    for (unsigned int n = 1; n < 16; ++n)
    {
        if (inputs[0].worker_thread_id != inputs[n].worker_thread_id)
        {
            all_packets_handled_by_the_same_thread = false;

            break;
        }
    }

    ASSERT_FALSE(all_packets_handled_by_the_same_thread);

    /* clean up */
    for (unsigned int n = 0; n < 16; ++n)
    {
        system_event_release(wait_events[n]);
    }
}