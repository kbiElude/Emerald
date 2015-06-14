/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_cond_variable.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_time.h"
#include "system/system_threads.h"

#ifdef __linux__
    #include <errno.h>
    #include <pthread.h>
#endif


typedef struct _system_cond_variable
{
#ifdef _WIN32
    CONDITION_VARIABLE condition_variable;
#else
    pthread_cond_t     condition_variable;
#endif

    system_critical_section       cs;
    system_thread_id              owning_thread;
    PFNSYSTEMCONDVARIABLETESTPROC pTestPredicate;
    void*                         test_predicate_user_arg;

    explicit _system_cond_variable(__in PFNSYSTEMCONDVARIABLETESTPROC in_pTestPredicate,
                                   __in void*                         in_test_predicate_user_arg)
    {
        ASSERT_DEBUG_SYNC(in_pTestPredicate != NULL,
                          "Test predicate func ptr is NULL");

        cs                      = NULL;
        owning_thread           = 0;
        pTestPredicate          = in_pTestPredicate;
        test_predicate_user_arg = in_test_predicate_user_arg;
    }

    ~_system_cond_variable()
    {
        ASSERT_DEBUG_SYNC(cs == NULL,
                          "Internally maintained CS != NULL at destruction time.")
    }
} _system_cond_variable;


/** Please see header for specification */
PUBLIC EMERALD_API system_cond_variable system_cond_variable_create(__in     PFNSYSTEMCONDVARIABLETESTPROC pTestPredicate,
                                                                    __in_opt void*                         test_predicate_user_arg,
                                                                    __in_opt system_critical_section       in_cs)
{
    _system_cond_variable* new_variable_ptr = new (std::nothrow) _system_cond_variable(pTestPredicate,
                                                                                       test_predicate_user_arg);

    ASSERT_ALWAYS_SYNC(new_variable_ptr != NULL,
                       "Out of memory");

    if (new_variable_ptr != NULL)
    {
#ifdef _WIN32
        ::InitializeConditionVariable(&new_variable_ptr->condition_variable);
#else
        int result = pthread_cond_init(&new_variable_ptr->condition_variable,
                                       NULL); /* cond_attr */

        if (result != 0)
        {
            ASSERT_DEBUG_SYNC(result == 0,
                            "pthread_cond_init() failed");

            delete new_variable_ptr;
            new_variable_ptr = NULL;

            goto end;
        }
#endif
        if (in_cs == NULL)
        {
            new_variable_ptr->cs = system_critical_section_create();
        }
        else
        {
            new_variable_ptr->cs = in_cs;

            system_critical_section_retain(in_cs);
        }
    }

end:
    return (system_cond_variable) new_variable_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_cond_variable_signal(__in __notnull system_cond_variable cond_variable,
                                                    __in           bool                 should_broadcast)
{
    _system_cond_variable* cond_variable_ptr = (_system_cond_variable*) cond_variable;

    ASSERT_DEBUG_SYNC(cond_variable != NULL,
                      "Input condition variable is NULL");

    if (!should_broadcast)
    {
#ifdef _WIN32
        ::WakeConditionVariable(&cond_variable_ptr->condition_variable);
#else
        int result = pthread_cond_signal(&cond_variable_ptr->condition_variable);

        ASSERT_DEBUG_SYNC(result == 0,
                          "pthread_cond_signal() failed");
#endif
    }
    else
    {
#ifdef _WIN32
        ::WakeAllConditionVariable(&cond_variable_ptr->condition_variable);
#else
        int result = pthread_cond_broadcast(&cond_variable_ptr->condition_variable);

        ASSERT_DEBUG_SYNC(result == 0,
                          "pthread_cond_broadcast() failed");
#endif
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_cond_variable_wait_begin(__in __notnull system_cond_variable cond_variable,
                                                        __in           system_timeline_time timeout,
                                                        __out_opt      bool*                out_has_timed_out_ptr)
{
    _system_cond_variable* cond_variable_ptr = (_system_cond_variable*) cond_variable;
    bool                   has_timed_out     = false;
    uint32_t               timeout_msec      = 0;

    ASSERT_DEBUG_SYNC(cond_variable != NULL,
                      "Input condition variable is NULL");
    ASSERT_DEBUG_SYNC(cond_variable_ptr->owning_thread == 0,
                      "CV is already owned by a thread");

    if (timeout == SYSTEM_TIME_INFINITE)
    {
#ifdef _WIN32
        timeout_msec = INFINITE;
#else
        timeout_msec = timeout;
#endif
    }
    else
    {
        system_time_get_msec_for_timeline_time(timeout,
                                              &timeout_msec);
    }

    system_critical_section_enter(cond_variable_ptr->cs);
    {
        while (!cond_variable_ptr->pTestPredicate(cond_variable_ptr->test_predicate_user_arg) &&
               !has_timed_out)
        {
#ifdef _WIN32
            PCRITICAL_SECTION cs_ptr = NULL;
#else
            pthread_mutex_t* cs_ptr = NULL;
#endif

            system_critical_section_get_property(cond_variable_ptr->cs,
                                                 SYSTEM_CRITICAL_SECTION_PROPERTY_HANDLE,
                                                 &cs_ptr);

#ifdef _WIN32
            if (::SleepConditionVariableCS(&cond_variable_ptr->condition_variable,
                                           cs_ptr,
                                           timeout_msec) == 0)
            {
                /* Failure */
                DWORD last_error = ::GetLastError();

                ASSERT_DEBUG_SYNC(last_error == ERROR_TIMEOUT,
                                  "Unrecognized error generated by SleepConditionVariableCS()");

                if (last_error == ERROR_TIMEOUT)
                {
                    has_timed_out = true;
                }
            }
#else
            int wait_result;

            if (timeout_msec == SYSTEM_TIME_INFINITE)
            {
                wait_result = pthread_cond_wait(&cond_variable_ptr->condition_variable,
                                                 cs_ptr);
            }
            else
            {
                struct timespec timeout_api;

                timeout_api.tv_sec  =  timeout_msec / 1000;
                timeout_api.tv_nsec = (timeout_msec % 1000) * NSEC_PER_SEC;

                wait_result = pthread_cond_timedwait(&cond_variable_ptr->condition_variable,
                                                      cs_ptr,
                                                     &timeout_api);
            }

            if (wait_result != 0)
            {
                ASSERT_DEBUG_SYNC(wait_result == ETIMEDOUT,
                                 "Unrecognized error generated by pthread_cond_wait() or pthread_cond_timedwait()");

                has_timed_out = (wait_result == ETIMEDOUT);
            }

#endif
        } /* while (it makes sense to keep spinning) */
    }

    /* Only release the CS if a time-out occurred */
    if (has_timed_out)
    {
        system_critical_section_leave(cond_variable_ptr->cs);
    }
    else
    {
        cond_variable_ptr->owning_thread = system_threads_get_thread_id();
    }

    if (out_has_timed_out_ptr != NULL)
    {
        *out_has_timed_out_ptr = has_timed_out;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_cond_variable_wait_end(__in __notnull system_cond_variable cond_variable)
{
    _system_cond_variable* cond_variable_ptr = (_system_cond_variable*) cond_variable;
    system_thread_id       current_thread_id = system_threads_get_thread_id();

    ASSERT_DEBUG_SYNC(cond_variable != NULL,
                      "Input argument is NULL");
    ASSERT_DEBUG_SYNC(cond_variable_ptr->owning_thread != 0,
                      "CV is not owned by any thread");
    ASSERT_DEBUG_SYNC(cond_variable_ptr->owning_thread == current_thread_id,
                      "CV is owned by another thread");

    if (current_thread_id == cond_variable_ptr->owning_thread)
    {
        system_critical_section_leave(cond_variable_ptr->cs);

        cond_variable_ptr->owning_thread  = 0;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_cond_variable_release(__in __post_invalid system_cond_variable cond_variable)
{
    ASSERT_ALWAYS_SYNC(cond_variable != NULL,
                       "Input argument is NULL");

    if (cond_variable != NULL)
    {
        _system_cond_variable* variable_ptr = (_system_cond_variable*) cond_variable;

        /* There is no release function for CV under Windows */
        memset(&variable_ptr->condition_variable,
               0,
               sizeof(variable_ptr->condition_variable) );

        if (variable_ptr->cs != NULL)
        {
            system_critical_section_release(variable_ptr->cs);

            variable_ptr->cs = NULL;
        }

        delete variable_ptr;
        variable_ptr = NULL;
    } /* if (cond_variable != NULL) */
}