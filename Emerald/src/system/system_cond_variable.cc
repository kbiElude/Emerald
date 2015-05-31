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


typedef struct _system_cond_variable
{
    CONDITION_VARIABLE            condition_variable;
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

        memset(&condition_variable,
               0,
               sizeof(condition_variable) );
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
        ::InitializeConditionVariable(&new_variable_ptr->condition_variable);

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
        ::WakeConditionVariable(&cond_variable_ptr->condition_variable);
    }
    else
    {
        ::WakeAllConditionVariable(&cond_variable_ptr->condition_variable);
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
        timeout_msec = INFINITE;
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
            PCRITICAL_SECTION cs_ptr = NULL;

            system_critical_section_get_property(cond_variable_ptr->cs,
                                                 SYSTEM_CRITICAL_SECTION_PROPERTY_HANDLE,
                                                 &cs_ptr);

            if (::SleepConditionVariableCS(&cond_variable_ptr->condition_variable,
                                           cs_ptr,
                                           timeout_msec) == 0)
            {
                /* Failure */
                DWORD last_error = ::GetLastError();

                ASSERT_DEBUG_SYNC(last_error == WAIT_TIMEOUT,
                                  "Unrecognized error generated by SleepConditionVariableCS()");

                if (last_error == WAIT_TIMEOUT)
                {
                    has_timed_out = true;
                }
            }
        }
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