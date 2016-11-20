/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_dpc.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"
#include "system/system_threads.h"
#include <string.h>

/* Private type definitions */
struct _system_callback_manager_callback;

typedef struct _system_callback_manager_callback_subscription
{
    PFNSYSTEMCALLBACKPROC              callback_proc;
    _system_callback_manager_callback* owner_ptr;
    uint32_t                           ref_counter;
    _callback_synchronicity            synchronicity;
    void*                              user_arg;

    _system_callback_manager_callback_subscription()
    {
        callback_proc = nullptr;
        owner_ptr     = nullptr;
        ref_counter   = 1;
        synchronicity = CALLBACK_SYNCHRONICITY_UNKNOWN;
        user_arg      = nullptr;
    }

    ~_system_callback_manager_callback_subscription()
    {
        ASSERT_DEBUG_SYNC(ref_counter == 0,
                          "Removing a subscription descriptor even though reference counter is not 0");
    }
} _system_callback_manager_callback_subscription;

typedef struct _system_callback_manager_callback
{
    uint32_t                callback_proc_data_size;
    system_resource_pool    resource_pool; /* each block is sizeof(_system_callback_manager_callback_subscription) + sizeof(callback_proc_data_size) */
    system_resizable_vector subscriptions; /* stores _system_callback_manager_callback_subscription */
    system_read_write_mutex subscriptions_mutex;

    _system_callback_manager_callback()
    {
        callback_proc_data_size = 0;
        resource_pool           = nullptr;
        subscriptions           = nullptr;
        subscriptions_mutex     = nullptr;
    }

    ~_system_callback_manager_callback()
    {
        if (resource_pool != nullptr)
        {
            system_resource_pool_release(resource_pool);

            resource_pool = nullptr;
        }

        if (subscriptions != nullptr)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = nullptr;

            while (system_resizable_vector_pop(subscriptions,
                                              &subscription_ptr) )
            {
                delete subscription_ptr;

                subscription_ptr = nullptr;
            }
        }

        if (subscriptions_mutex != nullptr)
        {
            system_read_write_mutex_release(subscriptions_mutex);

            subscriptions_mutex = nullptr;
        }
    }
} _system_callback_manager_callback;

typedef struct _system_callback_manager_dpc_callback_arg
{
    int                   callback_id;
    void*                 callback_proc_user_arg;
    PFNSYSTEMCALLBACKPROC pfn_callback_proc;
} _system_callback_manager_dpc_callback_arg;

typedef struct _system_callback_manager
{
    _system_callback_manager_callback* callbacks;
    _callback_id                       max_callback_id;

    _system_callback_manager()
    {
        callbacks       = nullptr;
        max_callback_id = (_callback_id) 0;
    }
} _system_callback_manager;


/** TODO */
system_callback_manager global_callback_manager = nullptr;

/* Forward declarations */
PRIVATE          void _add_callback_support                     (system_callback_manager            callback_manager,
                                                                 _callback_id                       callback_id,
                                                                 uint32_t                           callback_proc_data_size);
PRIVATE          void _deinit_system_callback_manager_callback  (_system_callback_manager_callback& descriptor);
PRIVATE volatile void _system_callback_manager_call_back_handler(void*                              descriptor);


/** TODO */
PRIVATE void _add_callback_support(system_callback_manager callback_manager,
                                   _callback_id            callback_id,
                                   uint32_t                callback_proc_data_size)
{
    _system_callback_manager*          callback_manager_ptr = (_system_callback_manager*) callback_manager;
    _system_callback_manager_callback* descriptor_ptr       = nullptr;

    /* Sanity checks */
    if (callback_id > callback_manager_ptr->max_callback_id)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot add callback support, requested ID is invalid");

        goto end;
    }

    /* Configure the callback descriptor */
    descriptor_ptr = callback_manager_ptr->callbacks + callback_id;

    ASSERT_DEBUG_SYNC(descriptor_ptr->subscriptions == nullptr,
                      "Subscription handler already configured");

    descriptor_ptr->callback_proc_data_size = callback_proc_data_size;
    descriptor_ptr->resource_pool           = system_resource_pool_create   (sizeof(_system_callback_manager_callback_subscription) + callback_proc_data_size,
                                                                             4,     /* n_elements */
                                                                             nullptr,  /* init_fn */
                                                                             nullptr); /* deinit_fn */
    descriptor_ptr->subscriptions           = system_resizable_vector_create(4 /* capacity */);

    descriptor_ptr->subscriptions_mutex = system_read_write_mutex_create();

    ASSERT_ALWAYS_SYNC(descriptor_ptr->resource_pool != nullptr &&
                       descriptor_ptr->subscriptions != nullptr,
                       "Out of memory");

    if (descriptor_ptr->resource_pool == nullptr ||
        descriptor_ptr->subscriptions == nullptr)
    {
        _deinit_system_callback_manager_callback(*descriptor_ptr);

        goto end;
    }
end:
    ;
}

/** TODO */
PRIVATE void _deinit_system_callback_manager_callback(_system_callback_manager_callback& descriptor)
{
    if (descriptor.subscriptions_mutex != nullptr)
    {
        system_read_write_mutex_release(descriptor.subscriptions_mutex);

        descriptor.subscriptions_mutex = nullptr;
    }

    if (descriptor.resource_pool != nullptr)
    {
        system_resource_pool_release(descriptor.resource_pool);
        descriptor.resource_pool = nullptr;
    }

    if (descriptor.subscriptions != nullptr)
    {
        _system_callback_manager_callback_subscription* subscription_ptr = nullptr;

        while (system_resizable_vector_pop(descriptor.subscriptions,
                                          &subscription_ptr) )
        {
            delete subscription_ptr;

            subscription_ptr = nullptr;
        }

        system_resizable_vector_release(descriptor.subscriptions);

        descriptor.subscriptions = nullptr;
    }
}

/** TODO */
PRIVATE volatile void _system_callback_manager_call_back_handler(void* descriptor)
{
    _system_callback_manager_callback_subscription* subscription_ptr = (_system_callback_manager_callback_subscription*) descriptor;
    const unsigned char*                            callback_data    = (unsigned char*) descriptor + sizeof(_system_callback_manager_callback_subscription);

    subscription_ptr->callback_proc(callback_data,
                                    subscription_ptr->user_arg);

    system_resource_pool_return_to_pool(subscription_ptr->owner_ptr->resource_pool,
                                        (system_resource_pool_block) descriptor);
}

/** TODO */
PRIVATE void _system_callback_manager_handle_dpc_callback(void* object,
                                                          void* callback_arg)
{
    _system_callback_manager_dpc_callback_arg* arg_ptr          = reinterpret_cast<_system_callback_manager_dpc_callback_arg*>(callback_arg);
    system_callback_manager                    callback_manager = reinterpret_cast<system_callback_manager>                   (object);

    system_callback_manager_unsubscribe_from_callbacks(callback_manager,
                                                       arg_ptr->callback_id,
                                                       arg_ptr->pfn_callback_proc,
                                                       arg_ptr->callback_proc_user_arg,
                                                       false); /* allow_deferred_execution */

    delete arg_ptr;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_call_back(system_callback_manager callback_manager,
                                              int                     callback_id,
                                              const void*             callback_proc_data)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(callback_manager_ptr->max_callback_id >= callback_id,
                      "Requested callback ID is invalid");

    /* Issue the call-backs */
    _system_callback_manager_callback& callback = callback_manager_ptr->callbacks[callback_id];

    system_read_write_mutex_lock(callback.subscriptions_mutex,
                                 ACCESS_READ);
    {
        if (callback.subscriptions != nullptr)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = nullptr;
            unsigned int                                    n_subscriptions  = 0;

            system_resizable_vector_get_property(callback.subscriptions,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_subscriptions);

            for (unsigned int n_subscription = 0;
                              n_subscription < n_subscriptions;
                            ++n_subscription)
            {
                system_resizable_vector_get_element_at(callback.subscriptions,
                                                       n_subscription,
                                                      &subscription_ptr);

                switch (subscription_ptr->synchronicity)
                {
                    case CALLBACK_SYNCHRONICITY_ASYNCHRONOUS:
                    {
                        /* Copy caller's data to a memory block from callback-specific resource pool */
                        unsigned char* callback_data = (unsigned char*) system_resource_pool_get_from_pool(callback.resource_pool);

                        memcpy(callback_data,
                               subscription_ptr,
                               sizeof(*subscription_ptr) );

                        if (callback_proc_data != nullptr)
                        {
                            memcpy(callback_data + sizeof(*subscription_ptr),
                                   callback_proc_data,
                                   callback.callback_proc_data_size);
                        }

                        /* Push the task to the thread pool */
                        system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                   _system_callback_manager_call_back_handler,
                                                                                                   callback_data);

                        system_thread_pool_submit_single_task(task);

                        break;
                    }

                    case CALLBACK_SYNCHRONICITY_SYNCHRONOUS:
                    {
                        subscription_ptr->callback_proc(callback_proc_data,
                                                        subscription_ptr->user_arg);

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false, "Unrecognized call-back synchronicity");
                    }
                }

                /* Check if the number of subscriptions have changed. This may happen if a subscription
                 * has been removed by one of the handlers. Handling such use cases remains a TODO */
                uint32_t n_subscriptions_after = 0;

                system_resizable_vector_get_property(callback.subscriptions,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_subscriptions_after);

                if (n_subscriptions_after != 0 &&
                    n_subscriptions_after != n_subscriptions)
                {
                    /* TODO: Need to loop over all subscriptions again and make sure we do not call
                     *       the same callback handlers twice in a row.
                     */
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");
                }

                subscription_ptr = nullptr;
            }
        }
    }
    system_read_write_mutex_unlock(callback.subscriptions_mutex,
                                   ACCESS_READ);
}

/** Please see header for spec */
PUBLIC system_callback_manager system_callback_manager_create(_callback_id max_callback_id)
{
    /* Carry on */
    _system_callback_manager* manager_ptr = new (std::nothrow) _system_callback_manager;

    ASSERT_ALWAYS_SYNC(manager_ptr != nullptr,
                       "Out of memory");

    if (manager_ptr != nullptr)
    {
        manager_ptr->callbacks       = new (std::nothrow) _system_callback_manager_callback[max_callback_id + 1];
        manager_ptr->max_callback_id = max_callback_id;

        ASSERT_ALWAYS_SYNC(manager_ptr->callbacks != nullptr,
                           "Out of memory");

        if (manager_ptr->callbacks == nullptr)
        {
            delete manager_ptr;
            manager_ptr = nullptr;

            goto end;
        }

        /* Initialize all call-back descriptors */
        for (unsigned int n_callback_id  = 0;
                          n_callback_id <= (unsigned int) max_callback_id;
                        ++n_callback_id)
        {
            _add_callback_support( (system_callback_manager) manager_ptr,
                                  (_callback_id) n_callback_id,
                                  sizeof(void*) );
        }
    }

end:
    return (system_callback_manager) manager_ptr;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_deinit()
{
    ASSERT_DEBUG_SYNC(global_callback_manager != nullptr,
                      "Callback manager not initialized");

    system_callback_manager_release(global_callback_manager);
}

/** Please see header for spec */
PUBLIC EMERALD_API system_callback_manager system_callback_manager_get()
{
    ASSERT_DEBUG_SYNC(global_callback_manager != nullptr,
                      "system_callback_manager_get() called without preceding _init()");

    return global_callback_manager;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_init()
{
    ASSERT_DEBUG_SYNC(global_callback_manager == nullptr,
                      "Callback manager already initialized");

    global_callback_manager = system_callback_manager_create(CALLBACK_ID_COUNT);
}

/** Please see header for spec */
PUBLIC void system_callback_manager_release(system_callback_manager callback_manager)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Make sure there are no outstanding DPCs against this callback manager.. */
    while (system_dpc_is_object_pending_callback(callback_manager) )
    {
        system_threads_yield();
    }

    for (int n_callback  = 0;
             n_callback <= callback_manager_ptr->max_callback_id;
           ++n_callback)
    {
        _system_callback_manager_callback& descriptor = callback_manager_ptr->callbacks[n_callback];

        _deinit_system_callback_manager_callback(descriptor);
    }

    if (callback_manager_ptr->callbacks != nullptr)
    {
        delete [] callback_manager_ptr->callbacks;

        callback_manager_ptr->callbacks = nullptr;
    }

    delete callback_manager_ptr;
    callback_manager_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_callback_manager_subscribe_for_callbacks(system_callback_manager callback_manager,
                                                                        int                     callback_id,
                                                                        _callback_synchronicity callback_synchronicity,
                                                                        PFNSYSTEMCALLBACKPROC   pfn_callback_proc,
                                                                        void*                   callback_proc_user_arg)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(callback_manager_ptr->max_callback_id >= callback_id,
                      "Requested callback ID is invalid");

    /* Check if the requested subscription is already stored. If so, bump up its ref counter and leave.
     * Otherwise, we'll spawn a new descriptor and insert it into the map. */
    system_read_write_mutex_lock(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex,
                                 ACCESS_WRITE);
    {
        uint32_t n_subscriptions = 0;

        system_resizable_vector_get_property(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_subscriptions);

        for (uint32_t n_subscription = 0;
                      n_subscription < n_subscriptions;
                    ++n_subscription)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                                        n_subscription,
                                                       &subscription_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve subscription descriptor at index [%d]",
                                  n_subscription);

                continue;
            }

            if (subscription_ptr->callback_proc == pfn_callback_proc                               &&
                subscription_ptr->owner_ptr     == (callback_manager_ptr->callbacks + callback_id) &&
                /* ignore synchronicity setting */
                subscription_ptr->user_arg      == callback_proc_user_arg)
            {
                /* We have a match */
                subscription_ptr->ref_counter++;

                goto end;
            }
        }
    
        /* Create a new descriptor */
        _system_callback_manager_callback_subscription* subscription_ptr = new (std::nothrow) _system_callback_manager_callback_subscription;

        ASSERT_ALWAYS_SYNC(subscription_ptr != nullptr,
                           "Out of memory");

        if (subscription_ptr != nullptr)
        {
            subscription_ptr->callback_proc = pfn_callback_proc;
            subscription_ptr->owner_ptr     = callback_manager_ptr->callbacks + callback_id;
            subscription_ptr->synchronicity = callback_synchronicity;
            subscription_ptr->user_arg      = callback_proc_user_arg;

            system_resizable_vector_push(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                         subscription_ptr);
        }
end:
        ;
    }
    system_read_write_mutex_unlock(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex,
                                   ACCESS_WRITE);
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_callback_manager_unsubscribe_from_callbacks(system_callback_manager callback_manager,
                                                                           int                     callback_id,
                                                                           PFNSYSTEMCALLBACKPROC   pfn_callback_proc,
                                                                           void*                   callback_proc_user_arg,
                                                                           bool                    allow_deferred_execution)
{
    _system_callback_manager* callback_manager_ptr    = (_system_callback_manager*) callback_manager;
    bool                      subscription_removal_ok = true;

    if (allow_deferred_execution                                                                            &&
        system_read_write_mutex_is_locked(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex))
    {
        _system_callback_manager_dpc_callback_arg* arg_ptr = new (std::nothrow) _system_callback_manager_dpc_callback_arg;

        ASSERT_DEBUG_SYNC(arg_ptr != nullptr,
                          "Out of memory");

        arg_ptr->callback_id            = callback_id;
        arg_ptr->callback_proc_user_arg = callback_proc_user_arg;
        arg_ptr->pfn_callback_proc      = pfn_callback_proc;

        system_dpc_schedule(callback_manager,
                            _system_callback_manager_handle_dpc_callback,
                            arg_ptr);
    }
    else
    {
        system_read_write_mutex_lock(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex,
                                     ACCESS_WRITE);
        {
            /* Find the callback descriptor */
            uint32_t n_callbacks = 0;

            system_resizable_vector_get_property(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_callbacks);

            for (uint32_t n_callback = 0;
                          n_callback < n_callbacks;
                        ++n_callback)
            {
                _system_callback_manager_callback_subscription* subscription_ptr = nullptr;

                if (system_resizable_vector_get_element_at(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                                           n_callback,
                                                          &subscription_ptr) )
                {
                    if (subscription_ptr->callback_proc == pfn_callback_proc      &&
                        subscription_ptr->user_arg      == callback_proc_user_arg)
                    {
                        --subscription_ptr->ref_counter;

                        if (subscription_ptr->ref_counter == 0)
                        {
                            system_resizable_vector_delete_element_at(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                                                      n_callback);

                            delete subscription_ptr;
                            subscription_ptr = nullptr;
                        }

                        break;
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve callback descriptor at index [%d]",
                                      n_callback);
                }
            }
        }
        system_read_write_mutex_unlock(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex,
                                       ACCESS_WRITE);
    }
}
