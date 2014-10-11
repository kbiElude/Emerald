/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_constants.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_threads.h"

/** Internal type definitions */
typedef struct
{
    PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func;
    system_threads_entry_point_argument callback_func_argument;
    system_event                        kill_event;
    system_thread_id                    thread_id;
} _system_threads_thread_descriptor;

/** Internal variables */
system_resizable_vector active_threads_vector    = NULL; /* contains HANDLEs of active vectors. */
system_critical_section active_threads_vector_cs = NULL;
system_resource_pool    thread_descriptor_pool   = NULL;

/** Internal functions */
/** Entry point wrapper.
 *
 *  @param argument _system_threads_thread_descriptor*
 */
PRIVATE DWORD __stdcall _system_threads_entry_point_wrapper(void* argument)
{
    _system_threads_thread_descriptor*  thread_descriptor = (_system_threads_thread_descriptor*) argument;
    PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func     = thread_descriptor->callback_func;
    system_threads_entry_point_argument callback_func_arg = thread_descriptor->callback_func_argument;
    system_event                        exit_event        = thread_descriptor->kill_event;

    /* Release the descriptor back to the pool */
    system_resource_pool_return_to_pool(thread_descriptor_pool, (system_resource_pool_block) thread_descriptor);

    callback_func(callback_func_arg);

    if (exit_event != NULL)
    {
        system_event_set(exit_event);
    }

    /* We're done */
    return 0;
}


/** Please see header for specification */
PUBLIC EMERALD_API system_thread_id system_threads_get_thread_id()
{
    return ::GetCurrentThreadId();
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_id system_threads_spawn(__in __notnull PFNSYSTEMTHREADSENTRYPOINTPROC callback_func, __in __maybenull system_threads_entry_point_argument callback_func_argument, __out __maybenull system_event* thread_wait_event)
{
    /* Create a new descriptor */
    system_thread_id result = 0;

    ASSERT_ALWAYS_SYNC(thread_descriptor_pool != NULL, "Thread descriptor pool is null");
    if (thread_descriptor_pool != NULL)
    {
        _system_threads_thread_descriptor* thread_descriptor = (_system_threads_thread_descriptor*) system_resource_pool_get_from_pool(thread_descriptor_pool);

        ASSERT_ALWAYS_SYNC(thread_descriptor != NULL, "Could not allocate thread descriptor");
        if (thread_descriptor != NULL)
        {
            thread_descriptor->callback_func          = callback_func;
            thread_descriptor->callback_func_argument = callback_func_argument;

            /* Spawn the thread. */
            HANDLE new_thread_handle = ::CreateThread(NULL,                   /* no security attribs */
                                                      0,                      /* default stack size */
                                                      &_system_threads_entry_point_wrapper,
                                                      thread_descriptor,
                                                      0,                      /* run immediately after creation */
                                                      &thread_descriptor->thread_id
                                                     );

            ASSERT_ALWAYS_SYNC(new_thread_handle != NULL, "Could not create a new thread");

            result = thread_descriptor->thread_id;

            /* Waiting on threads seems to be broken under Vista SP2.. */
            if (thread_wait_event != NULL)
            {
                thread_descriptor->kill_event = system_event_create(true, false);
                
                *thread_wait_event = thread_descriptor->kill_event;
            }
            else
            {
                thread_descriptor->kill_event = NULL;
            }

            ::CloseHandle(new_thread_handle);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC void _system_threads_init()
{
    ASSERT_ALWAYS_SYNC(active_threads_vector    == NULL, "Active threads vector already initialized.");
    ASSERT_ALWAYS_SYNC(active_threads_vector_cs == NULL, "Active threads vector CS already initialized.");
    ASSERT_ALWAYS_SYNC(thread_descriptor_pool   == NULL, "Thread descriptor pool already initialized.");

    if (active_threads_vector == NULL && active_threads_vector_cs == NULL && thread_descriptor_pool == NULL)
    {
        active_threads_vector    = system_resizable_vector_create(BASE_THREADS_CAPACITY, sizeof(HANDLE) );
        active_threads_vector_cs = system_critical_section_create();
        thread_descriptor_pool   = system_resource_pool_create(sizeof(_system_threads_thread_descriptor), BASE_THREADS_CAPACITY, NULL, NULL);

        ASSERT_ALWAYS_SYNC(active_threads_vector    != NULL, "Could not initialize active threads vector.");
        ASSERT_ALWAYS_SYNC(active_threads_vector_cs != NULL, "Could not initialize active threads vector CS.");
        ASSERT_ALWAYS_SYNC(thread_descriptor_pool   != NULL, "Could not initialize thread descriptor pool.");
    }
}

/** Please see header for specification */
PUBLIC void _system_threads_deinit()
{
    ASSERT_ALWAYS_SYNC(active_threads_vector    != NULL, "Active threads vector not initialized.");
    ASSERT_ALWAYS_SYNC(active_threads_vector_cs != NULL, "Active threads vector CS not initialized.");
    ASSERT_ALWAYS_SYNC(thread_descriptor_pool   != NULL, "Thread descriptor pool not initialized.");

    if (active_threads_vector != NULL && active_threads_vector_cs != NULL && thread_descriptor_pool != NULL)
    {
        /* Wait till all threads quit. */
        system_critical_section_enter(active_threads_vector_cs);
        {
            LOG_INFO("Waiting for all threads to quit..");

            unsigned int n_threads = system_resizable_vector_get_amount_of_elements(active_threads_vector);

            if (n_threads > 0)
            {
                HANDLE* threads = new (std::nothrow) HANDLE[n_threads];

                ASSERT_ALWAYS_SYNC(threads != NULL, "Could not alloc handle table");
                if (threads != NULL)
                {
                    for (unsigned int n_thread = 0; n_thread < n_threads; ++n_thread)
                    {
                        HANDLE thread_handle = 0;
                        bool   result_get    = false;

                        result_get = system_resizable_vector_get_element_at(active_threads_vector, n_thread, &thread_handle);
                        ASSERT_DEBUG_SYNC(result_get, "Could not retrieve thread handle.");

                        memcpy(threads[n_thread], &thread_handle, sizeof(HANDLE) );
                    }

                    ::WaitForMultipleObjects(n_threads, threads, TRUE, INFINITE);
                }
            }

            LOG_INFO("All threads quit. Proceeding.");

            /** Okay, release all objects we can release while inside the critical section */
            system_resource_pool_release   (thread_descriptor_pool); thread_descriptor_pool = NULL;
            system_resizable_vector_release(active_threads_vector);  active_threads_vector  = NULL;
        }
        system_critical_section_leave(active_threads_vector_cs);

        system_critical_section_release(active_threads_vector_cs);
        active_threads_vector_cs = NULL;
    }
}
