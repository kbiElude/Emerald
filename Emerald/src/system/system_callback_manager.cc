/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"

/* Private type definitions */
struct _system_callback_manager_callback;

typedef struct _system_callback_manager_callback_subscription
{
    PFNSYSTEMCALLBACKPROC              callback_proc;
    _system_callback_manager_callback* owner_ptr;
    _callback_synchronicity            synchronicity;
    void*                              user_arg;

    _system_callback_manager_callback_subscription()
    {
        callback_proc = NULL;
        owner_ptr     = NULL;
        synchronicity = CALLBACK_SYNCHRONICITY_UNKNOWN;
        user_arg      = NULL;
    }
} _system_callback_manager_callback_subscription;

typedef struct _system_callback_manager_callback
{
    system_critical_section callback_in_progress_cs;
    uint32_t                callback_proc_data_size;
    bool                    defined;
    system_resource_pool    resource_pool; /* each block is sizeof(_system_callback_manager_callback_subscription) + sizeof(callback_proc_data_size) */
    system_resizable_vector subscriptions; /* stores _system_callback_manager_callback_subscription */

    _system_callback_manager_callback()
    {
        callback_in_progress_cs = NULL;
        callback_proc_data_size = 0;
        defined                 = false;
        resource_pool           = NULL;
        subscriptions           = NULL;
    }

    ~_system_callback_manager_callback()
    {
        if (callback_in_progress_cs != NULL)
        {
            system_critical_section_release(callback_in_progress_cs);

            callback_in_progress_cs = NULL;
        }

        if (resource_pool != NULL)
        {
            system_resource_pool_release(resource_pool);

            resource_pool = NULL;
        }

        if (subscriptions != NULL)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = NULL;

            while (system_resizable_vector_pop(subscriptions, &subscription_ptr) )
            {
                delete subscription_ptr;

                subscription_ptr = NULL;
            }
        }
    }
} _system_callback_manager_callback;

typedef struct _system_callback_manager
{
    _system_callback_manager_callback* callbacks;
    _callback_id                       max_callback_id;

    _system_callback_manager()
    {
        callbacks       = NULL;
        max_callback_id = 0;
    }
} _system_callback_manager;


/** TODO */
PRIVATE void _deinit_system_callback_manager_callback(_system_callback_manager_callback& descriptor)
{
    if (descriptor.callback_in_progress_cs != NULL)
    {
        system_critical_section_release(descriptor.callback_in_progress_cs);

        descriptor.callback_in_progress_cs = NULL;
    }

    if (descriptor.resource_pool != NULL)
    {
        system_resource_pool_release(descriptor.resource_pool);
        descriptor.resource_pool = NULL;
    }

    if (descriptor.subscriptions != NULL)
    {
        system_resizable_vector_release(descriptor.subscriptions);
        descriptor.subscriptions = NULL;
    }

    descriptor.defined = false;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_add_callback_support(__in __notnull system_callback_manager callback_manager,
                                                         __in           _callback_id            callback_id,
                                                         __in            uint32_t               callback_proc_data_size)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Sanity checks */
    if (callback_id > callback_manager_ptr->max_callback_id)
    {
        ASSERT_DEBUG_SYNC(false, "Cannot add callback support, requested ID is invalid");

        goto end;
    }

    if (callback_manager_ptr->callbacks[callback_id].defined)
    {
        ASSERT_DEBUG_SYNC(false, "Cannot add callback support, requested ID is already configured");

        goto end;
    }

    /* Configure the callback descriptor */
    _system_callback_manager_callback& descriptor = callback_manager_ptr->callbacks[callback_id];

    descriptor.callback_in_progress_cs = system_critical_section_create();
    descriptor.callback_proc_data_size = callback_proc_data_size;
    descriptor.defined                 = true;
    descriptor.resource_pool           = system_resource_pool_create   (sizeof(_system_callback_manager_callback_subscription) + callback_proc_data_size,
                                                                        4,     /* n_elements */
                                                                        NULL,  /* init_fn */
                                                                        NULL); /* deinit_fn */
    descriptor.subscriptions           = system_resizable_vector_create(4 /* capacity */,
                                                                        sizeof(_system_callback_manager_callback_subscription*) );

    ASSERT_ALWAYS_SYNC(descriptor.resource_pool != NULL && descriptor.subscriptions != NULL,
                       "Out of memory");

    if (descriptor.resource_pool == NULL || descriptor.subscriptions == NULL)
    {
        _deinit_system_callback_manager_callback(descriptor);

        goto end;
    }
end:
    ;
}

/** TODO */
PRIVATE volatile void _system_callback_manager_call_back_handler(__in __notnull void* descriptor)
{
    _system_callback_manager_callback_subscription* subscription_ptr = (_system_callback_manager_callback_subscription*) descriptor;
    const unsigned char*                            callback_data    = (unsigned char*) descriptor + sizeof(_system_callback_manager_callback_subscription);

    subscription_ptr->callback_proc(callback_data, subscription_ptr->user_arg);

    system_resource_pool_return_to_pool(subscription_ptr->owner_ptr->resource_pool,
                                        (system_resource_pool_block) descriptor);
}

/** Please see header for spec */
PUBLIC void system_callback_manager_call_back(__in __notnull system_callback_manager callback_manager,
                                              __in           _callback_id            callback_id,
                                              __in __notnull void*                   callback_proc_data)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(callback_manager_ptr->max_callback_id >= callback_id, "Requested callback ID is invalid");
    ASSERT_DEBUG_SYNC(callback_manager_ptr->callbacks[callback_id].defined, "Requested callback ID is not defined");

    /* Issue the call-backs */
    _system_callback_manager_callback& callback_descriptor = callback_manager_ptr->callbacks[callback_id];

    system_critical_section_enter(callback_descriptor.callback_in_progress_cs);
    {
        _system_callback_manager_callback_subscription* subscription_ptr = NULL;
        const unsigned int                              n_subscriptions  = system_resizable_vector_get_amount_of_elements(callback_descriptor.subscriptions);

        for (unsigned int n_subscription = 0; n_subscription < n_subscriptions; ++n_subscription)
        {
            system_resizable_vector_get_element_at(callback_descriptor.subscriptions, n_subscriptions, &subscription_ptr);

            switch (subscription_ptr->synchronicity)
            {
                case CALLBACK_SYNCHRONICITY_ASYNCHRONOUS:
                {
                    /* Copy caller's data to a memory block from callback-specific resource pool */
                    unsigned char* callback_data = (unsigned char*) system_resource_pool_get_from_pool(callback_descriptor.resource_pool);

                    memcpy(callback_data,                             subscription_ptr,   sizeof(*subscription_ptr) );
                    memcpy(callback_data + sizeof(*subscription_ptr), callback_proc_data, callback_descriptor.callback_proc_data_size);

                    /* Push the task to the thread pool */
                    system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                                     _system_callback_manager_call_back_handler,
                                                                                                                     callback_data);

                    system_thread_pool_submit_single_task(task);

                    break;
                }

                case CALLBACK_SYNCHRONICITY_SYNCHRONOUS:
                {
                    subscription_ptr->callback_proc(callback_proc_data, subscription_ptr->user_arg);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false, "Unrecognized call-back synchronicity");
                }
            } /* switch (subscription_ptr->synchronicity) */

            subscription_ptr = NULL;
        } /* for (all subscriptions) */
    }
    system_critical_section_leave(callback_descriptor.callback_in_progress_cs);
}

/** Please see header for spec */
PUBLIC system_callback_manager system_callback_manager_create(__in _callback_id max_callback_id)
{
    /* Carry on */
    _system_callback_manager* manager_ptr = new (std::nothrow) _system_callback_manager;

    ASSERT_ALWAYS_SYNC(manager_ptr != NULL, "Out of memory");
    if (manager_ptr != NULL)
    {
        manager_ptr->callbacks       = new (std::nothrow) _system_callback_manager_callback[max_callback_id + 1];
        manager_ptr->max_callback_id = max_callback_id;

        ASSERT_ALWAYS_SYNC(manager_ptr->callbacks != NULL, "Out of memory");
        if (manager_ptr->callbacks == NULL)
        {
            delete manager_ptr;
            manager_ptr = NULL;

            goto end;
        }
    } /* ASSERT_ALWAYS_SYNC(manager_ptr != NULL, "Out of memory"); */

end:
    return (system_callback_manager) manager_ptr;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_subscribe_for_callbacks(__in __notnull system_callback_manager callback_manager,
                                                            __in           _callback_id            callback_id,
                                                            __in __notnull _callback_synchronicity callback_synchronicity,
                                                            __in __notnull PFNSYSTEMCALLBACKPROC   pfn_callback_proc,
                                                            __in __notnull void*                   callback_proc_user_arg)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(callback_manager_ptr->max_callback_id >= callback_id, "Requested callback ID is invalid");
    ASSERT_DEBUG_SYNC(callback_manager_ptr->callbacks[callback_id].defined, "Requested callback ID is not defined");

    /* Create a new descriptor */
    _system_callback_manager_callback_subscription* subscription_ptr = new (std::nothrow) _system_callback_manager_callback_subscription;

    ASSERT_ALWAYS_SYNC(subscription_ptr != NULL, "Out of memory");
    if (subscription_ptr != NULL)
    {
        subscription_ptr->callback_proc = pfn_callback_proc;
        subscription_ptr->owner_ptr     = callback_manager_ptr->callbacks + callback_id;
        subscription_ptr->synchronicity = callback_synchronicity;
        subscription_ptr->user_arg      = callback_proc_user_arg;

        system_critical_section_enter(callback_manager_ptr->callbacks[callback_id].callback_in_progress_cs);
        {
            system_resizable_vector_push(callback_manager_ptr->callbacks[callback_id].subscriptions, subscription_ptr);
        }
        system_critical_section_leave(callback_manager_ptr->callbacks[callback_id].callback_in_progress_cs);
    }
}

/** Please see header for spec */
PUBLIC void system_callback_manager_release(__in __notnull system_callback_manager callback_manager)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    for (unsigned int n_callback  = 0;
                      n_callback <= callback_manager_ptr->max_callback_id;
                    ++n_callback)
    {
        _system_callback_manager_callback& descriptor = callback_manager_ptr->callbacks[n_callback];

        if (descriptor.defined)
        {
            _deinit_system_callback_manager_callback(descriptor);
        } /* if (descriptor.defined) */
    } /* for (all callbacks) */

    if (callback_manager_ptr->callbacks != NULL)
    {
        delete [] callback_manager_ptr->callbacks;

        callback_manager_ptr->callbacks = NULL;
    }

    delete callback_manager_ptr;
    callback_manager_ptr = NULL;
}