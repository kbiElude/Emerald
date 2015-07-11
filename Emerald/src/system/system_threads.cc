/**
 *
 * Emerald (kbi/elude @2012-2015)
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

#ifdef __linux__
    #include <errno.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <sys/syscall.h>
    #include <sys/types.h>
#endif

#ifdef USE_EMULATED_EVENTS
    #include "system/system_event_monitor.h"
#endif

/** Internal type definitions */
typedef struct
{
    PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func;
    system_threads_entry_point_argument callback_func_argument;
    system_event                        kill_event; /* TODO: this HANDLE is currently leaked. */
    system_thread                       thread_handle;
    system_thread_id                    thread_id;

#ifdef __linux
    /* No way to extract the TID outside of the concerned thread under POSIX,
     * so we need this little magic wand event here..
     */
    system_event thread_id_submitted_event;
    unsigned int thread_spawn_index;
#endif

} _system_threads_thread_descriptor;

/** Internal variables */
system_resizable_vector active_threads_vector    = NULL; /* contains system handles of active threads. */
system_critical_section active_threads_vector_cs = NULL;
unsigned int            n_threads_spawned        = 1;
system_resource_pool    thread_descriptor_pool   = NULL;

/** Internal functions */
/** Entry point wrapper.
 *
 *  @param argument _system_threads_thread_descriptor*
 */
#ifdef _WIN32
    DWORD WINAPI _system_threads_entry_point_wrapper(LPVOID argument)
#else
    void* _system_threads_entry_point_wrapper(void* argument)
#endif
{
    _system_threads_thread_descriptor*  thread_descriptor = (_system_threads_thread_descriptor*) argument;
    PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func     = thread_descriptor->callback_func;
    system_threads_entry_point_argument callback_func_arg = thread_descriptor->callback_func_argument;
    system_event                        exit_event        = thread_descriptor->kill_event;
    system_thread                       thread_handle     = thread_descriptor->thread_handle;

    #ifdef __linux
    {
        thread_descriptor->thread_id = system_threads_get_thread_id();

        system_event_set(thread_descriptor->thread_id_submitted_event);
    }
    #else
    {
        /* WinAPI's threading model is awesome ;) so this block is a stub. */
    }
    #endif

    /* Release the descriptor back to the pool */
    system_resource_pool_return_to_pool(thread_descriptor_pool,
                                        (system_resource_pool_block) thread_descriptor);

    /* Execute the user-specified entry-point */
    callback_func(callback_func_arg);

    /* Unregister the thread */
    system_critical_section_enter(active_threads_vector_cs);
    {
        size_t thread_handle_index = system_resizable_vector_find(active_threads_vector,
                                                                  (void*) thread_handle);

        ASSERT_DEBUG_SYNC(thread_handle_index != ITEM_NOT_FOUND,
                          "Thread handle was not found in the active threads vector");

        if (thread_handle_index != ITEM_NOT_FOUND)
        {
            system_resizable_vector_delete_element_at(active_threads_vector,
                                                      thread_handle_index);
        }
    }
    system_critical_section_leave(active_threads_vector_cs);

    /* We're done */
    return NULL;
}


/** Please see header for specification */
PUBLIC EMERALD_API system_thread_id system_threads_get_thread_id()
{
#ifdef _WIN32
    return ::GetCurrentThreadId();
#else
    return syscall(SYS_gettid);
#endif
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_threads_join_thread(__in      system_thread thread,
                                                   __in      system_time   timeout,
                                                   __out_opt bool*         out_has_timed_out_ptr)
{
    bool     has_timed_out = false;
    uint32_t timeout_msec  = 0;

#ifdef _WIN32
    if (timeout == SYSTEM_TIME_INFINITE)
    {
        timeout_msec = INFINITE;
    }
    else
    {
        system_time_get_msec_for_time(timeout,
                                     &timeout_msec);
    }

    has_timed_out = (::WaitForSingleObject( thread,
                                            timeout_msec) ) == WAIT_TIMEOUT;
#else
    if (timeout == SYSTEM_TIME_INFINITE)
    {
        pthread_join(thread,
                     NULL); /* __thread_return */
    }
    else
    {
        struct timespec timeout_api;
        unsigned int    timeout_msec;

        if (timeout_msec == 0)
        {
            has_timed_out = (pthread_tryjoin_np(thread,
                                                NULL) != 0); /* __thread_return */
        }
        else
        {
            system_time_get_msec_for_timeline_time(timeout,
                                                  &timeout_msec);

            timeout_api.tv_sec  =  timeout_msec / 1000;
            timeout_api.tv_nsec = (timeout_msec % 1000) * NSEC_PER_SEC;

            has_timed_out = (pthread_timedjoin_np(thread,
                                                  NULL, /* __thread_return */
                                                  &timeout_api) != 0);
        }
    }
#endif

    if (out_has_timed_out_ptr != NULL)
    {
        *out_has_timed_out_ptr = has_timed_out;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_thread_id system_threads_spawn(__in  __notnull   PFNSYSTEMTHREADSENTRYPOINTPROC      callback_func,
                                                         __in  __maybenull system_threads_entry_point_argument callback_func_argument,
                                                         __out __maybenull system_event*                       thread_wait_event,
                                                         __out __maybenull system_thread*                      out_thread_ptr)
{
    /* Create a new descriptor */
    system_thread_id result = 0;

    ASSERT_ALWAYS_SYNC(thread_descriptor_pool != NULL,
                       "Thread descriptor pool is null");

    if (thread_descriptor_pool != NULL)
    {
        _system_threads_thread_descriptor* thread_descriptor = (_system_threads_thread_descriptor*) system_resource_pool_get_from_pool(thread_descriptor_pool);

        ASSERT_ALWAYS_SYNC(thread_descriptor != NULL,
                           "Could not allocate thread descriptor");

        if (thread_descriptor != NULL)
        {
            thread_descriptor->callback_func          = callback_func;
            thread_descriptor->callback_func_argument = callback_func_argument;

            /* Spawn the thread. */
#ifdef _WIN32
            thread_descriptor->thread_handle = ::CreateThread(NULL,                                /* no security attribs */
                                                              0,                                   /* default stack size */
                                                             &_system_threads_entry_point_wrapper,
                                                              thread_descriptor,
                                                              0,                                   /* run immediately after creation */
                                                             &thread_descriptor->thread_id);

            ASSERT_ALWAYS_SYNC(thread_descriptor->thread_handle != NULL,
                               "Could not create a new thread");
#else
            /* Instantiate a 'thread started' event we will use to wait until the newly spawned thread
             * submits its thread ID to the descriptor.
             * Since we're using resource pool to manage descriptors and it would require a bit of work
             * to get the event to release at deinit time, we will free the event in just a bit */
            thread_descriptor->thread_id_submitted_event = system_event_create     (true); /* manual_reset */
            thread_descriptor->thread_spawn_index        = system_atomics_increment(&n_threads_spawned);

            int creation_result = pthread_create(&thread_descriptor->thread_handle,
                                                  NULL,
                                                  _system_threads_entry_point_wrapper,
                                                  thread_descriptor);

            ASSERT_ALWAYS_SYNC(creation_result == 0,
                               "Could not create a new thread");

            system_event_wait_single(thread_descriptor->thread_id_submitted_event);

            system_event_release(thread_descriptor->thread_id_submitted_event);
            thread_descriptor->thread_id_submitted_event = NULL;
#endif

            result = thread_descriptor->thread_id;

            if (out_thread_ptr != NULL)
            {
                *out_thread_ptr = thread_descriptor->thread_handle;
            }

            if (thread_wait_event != NULL)
            {
                *thread_wait_event            = system_event_create_from_thread(thread_descriptor->thread_handle);
                thread_descriptor->kill_event = *thread_wait_event;
            }
            else
            {
                thread_descriptor->kill_event = NULL;
            }

            system_critical_section_enter(active_threads_vector_cs);
            {
                system_resizable_vector_push(active_threads_vector,
                                             (void*) thread_descriptor->thread_handle);
            }
            system_critical_section_leave(active_threads_vector_cs);

        } /* if (thread_descriptor != NULL) */
    }

    return result;
}

/** Please see header for specification */
PUBLIC void _system_threads_init()
{
    ASSERT_ALWAYS_SYNC(active_threads_vector == NULL,
                       "Active threads vector already initialized.");
    ASSERT_ALWAYS_SYNC(active_threads_vector_cs == NULL,
                       "Active threads vector CS already initialized.");
    ASSERT_ALWAYS_SYNC(thread_descriptor_pool == NULL,
                       "Thread descriptor pool already initialized.");

    if (active_threads_vector    == NULL &&
        active_threads_vector_cs == NULL &&
        thread_descriptor_pool    == NULL)
    {
        active_threads_vector    = system_resizable_vector_create(BASE_THREADS_CAPACITY);
        active_threads_vector_cs = system_critical_section_create();
        thread_descriptor_pool   = system_resource_pool_create   (sizeof(_system_threads_thread_descriptor),
                                                                  BASE_THREADS_CAPACITY,
                                                                  NULL,  /* init_fn */
                                                                  NULL); /* deinit_fn */

        ASSERT_ALWAYS_SYNC(active_threads_vector != NULL,
                           "Could not initialize active threads vector.");
        ASSERT_ALWAYS_SYNC(active_threads_vector_cs != NULL,
                           "Could not initialize active threads vector CS.");
        ASSERT_ALWAYS_SYNC(thread_descriptor_pool != NULL,
                           "Could not initialize thread descriptor pool.");
    }
}

/** Please see header for specification */
PUBLIC void _system_threads_deinit()
{
    ASSERT_ALWAYS_SYNC(active_threads_vector != NULL,
                       "Active threads vector not initialized.");
    ASSERT_ALWAYS_SYNC(active_threads_vector_cs != NULL,
                       "Active threads vector CS not initialized.");
    ASSERT_ALWAYS_SYNC(thread_descriptor_pool != NULL,
                       "Thread descriptor pool not initialized.");

    if (active_threads_vector    != NULL &&
        active_threads_vector_cs != NULL &&
        thread_descriptor_pool   != NULL)
    {
        /* Wait till all threads quit. */
        system_critical_section_enter(active_threads_vector_cs);
        {
            LOG_INFO("Waiting for all threads to quit..");

            unsigned int n_threads = 0;

            system_resizable_vector_get_property(active_threads_vector,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_threads);

            if (n_threads > 0)
            {
#ifdef _WIN32
                HANDLE* threads = new (std::nothrow) HANDLE[n_threads];

                ASSERT_ALWAYS_SYNC(threads != NULL,
                                   "Could not alloc handle table");

                if (threads != NULL)
#endif
                {
                    for (unsigned int n_thread = 0;
                                      n_thread < n_threads;
                                    ++n_thread)
                    {
                        system_thread thread_handle = (system_thread) NULL;
                        bool          result_get    = system_resizable_vector_get_element_at(active_threads_vector,
                                                                                             n_thread,
                                                                                            &thread_handle);

                        ASSERT_DEBUG_SYNC(result_get,
                                          "Could not retrieve thread handle.");

#ifdef _WIN32
                        memcpy(threads[n_thread],
                              &thread_handle,
                              sizeof(HANDLE) );
#else
                        pthread_join(thread_handle,
                                     NULL); /* _thread_return */
#endif
                    }

#ifdef _WIN32
                    ::WaitForMultipleObjects(n_threads,
                                             threads,
                                             TRUE,
                                             INFINITE);
#endif
                }
            }

            LOG_INFO("All threads quit. Proceeding.");

            /** Okay, release all objects we can release while inside the critical section */
            system_resource_pool_release(thread_descriptor_pool);
            thread_descriptor_pool = NULL;

            system_resizable_vector_release(active_threads_vector);
            active_threads_vector = NULL;
        }
        system_critical_section_leave(active_threads_vector_cs);

        system_critical_section_release(active_threads_vector_cs);
        active_threads_vector_cs = NULL;
    }
}
