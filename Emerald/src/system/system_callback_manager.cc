/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"
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
        callback_proc = NULL;
        owner_ptr     = NULL;
        ref_counter   = 1;
        synchronicity = CALLBACK_SYNCHRONICITY_UNKNOWN;
        user_arg      = NULL;
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
        resource_pool           = NULL;
        subscriptions           = NULL;
        subscriptions_mutex     = NULL;
    }

    ~_system_callback_manager_callback()
    {
        if (resource_pool != NULL)
        {
            system_resource_pool_release(resource_pool);

            resource_pool = NULL;
        }

        if (subscriptions != NULL)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = NULL;

            while (system_resizable_vector_pop(subscriptions,
                                              &subscription_ptr) )
            {
                delete subscription_ptr;

                subscription_ptr = NULL;
            }
        }

        if (subscriptions_mutex != NULL)
        {
            system_read_write_mutex_release(subscriptions_mutex);

            subscriptions_mutex = NULL;
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
        max_callback_id = (_callback_id) 0;
    }
} _system_callback_manager;


/** TODO */
system_callback_manager global_callback_manager = NULL;

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
    _system_callback_manager_callback* descriptor_ptr       = NULL;

    /* Sanity checks */
    if (callback_id > callback_manager_ptr->max_callback_id)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot add callback support, requested ID is invalid");

        goto end;
    }

    /* Configure the callback descriptor */
    descriptor_ptr = callback_manager_ptr->callbacks + callback_id;

    ASSERT_DEBUG_SYNC(descriptor_ptr->subscriptions == NULL,
                      "Subscription handler already configured");

    descriptor_ptr->callback_proc_data_size = callback_proc_data_size;
    descriptor_ptr->resource_pool           = system_resource_pool_create   (sizeof(_system_callback_manager_callback_subscription) + callback_proc_data_size,
                                                                             4,     /* n_elements */
                                                                             NULL,  /* init_fn */
                                                                             NULL); /* deinit_fn */
    descriptor_ptr->subscriptions           = system_resizable_vector_create(4 /* capacity */);
    descriptor_ptr->subscriptions_mutex     = system_read_write_mutex_create();

    ASSERT_ALWAYS_SYNC(descriptor_ptr->resource_pool != NULL &&
                       descriptor_ptr->subscriptions != NULL,
                       "Out of memory");

    if (descriptor_ptr->resource_pool == NULL ||
        descriptor_ptr->subscriptions == NULL)
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
    if (descriptor.subscriptions_mutex != NULL)
    {
        system_read_write_mutex_release(descriptor.subscriptions_mutex);

        descriptor.subscriptions_mutex = NULL;
    }

    if (descriptor.resource_pool != NULL)
    {
        system_resource_pool_release(descriptor.resource_pool);
        descriptor.resource_pool = NULL;
    }

    if (descriptor.subscriptions != NULL)
    {
        _system_callback_manager_callback_subscription* subscription_ptr = NULL;

        while (system_resizable_vector_pop(descriptor.subscriptions,
                                          &subscription_ptr) )
        {
            delete subscription_ptr;

            subscription_ptr = NULL;
        }

        system_resizable_vector_release(descriptor.subscriptions);

        descriptor.subscriptions = NULL;
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
        if (callback.subscriptions != NULL)
        {
            _system_callback_manager_callback_subscription* subscription_ptr = NULL;
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

                        if (callback_proc_data != NULL)
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
                } /* switch (subscription_ptr->synchronicity) */

                subscription_ptr = NULL;
            } /* for (all subscriptions) */
        } /* if (callback_descriptor.subscriptions != NULL) */
    }
    system_read_write_mutex_unlock(callback.subscriptions_mutex,
                                   ACCESS_READ);
}

/** Please see header for spec */
PUBLIC system_callback_manager system_callback_manager_create(_callback_id max_callback_id)
{
    /* Carry on */
    _system_callback_manager* manager_ptr = new (std::nothrow) _system_callback_manager;

    ASSERT_ALWAYS_SYNC(manager_ptr != NULL,
                       "Out of memory");

    if (manager_ptr != NULL)
    {
        manager_ptr->callbacks       = new (std::nothrow) _system_callback_manager_callback[max_callback_id + 1];
        manager_ptr->max_callback_id = max_callback_id;

        ASSERT_ALWAYS_SYNC(manager_ptr->callbacks != NULL,
                           "Out of memory");

        if (manager_ptr->callbacks == NULL)
        {
            delete manager_ptr;
            manager_ptr = NULL;

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
    } /* ASSERT_ALWAYS_SYNC(manager_ptr != NULL, "Out of memory"); */

end:
    return (system_callback_manager) manager_ptr;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_deinit()
{
    ASSERT_DEBUG_SYNC(global_callback_manager != NULL,
                      "Callback manager not initialized");

    system_callback_manager_release(global_callback_manager);
}

/** Please see header for spec */
PUBLIC EMERALD_API system_callback_manager system_callback_manager_get()
{
    ASSERT_DEBUG_SYNC(global_callback_manager != NULL,
                      "system_callback_manager_get() called without preceding _init()");

    return global_callback_manager;
}

/** Please see header for spec */
PUBLIC void system_callback_manager_init()
{
    ASSERT_DEBUG_SYNC(global_callback_manager == NULL,
                      "Callback manager already initialized");

    global_callback_manager = system_callback_manager_create(CALLBACK_ID_COUNT);
}

/** Please see header for spec */
PUBLIC void system_callback_manager_release(system_callback_manager callback_manager)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

    for (int n_callback  = 0;
             n_callback <= callback_manager_ptr->max_callback_id;
           ++n_callback)
    {
        _system_callback_manager_callback& descriptor = callback_manager_ptr->callbacks[n_callback];

        _deinit_system_callback_manager_callback(descriptor);
    } /* for (all callbacks) */

    if (callback_manager_ptr->callbacks != NULL)
    {
        delete [] callback_manager_ptr->callbacks;

        callback_manager_ptr->callbacks = NULL;
    }

    delete callback_manager_ptr;
    callback_manager_ptr = NULL;
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
            _system_callback_manager_callback_subscription* subscription_ptr = NULL;

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
        } /* for (all added subscriptions) */
    
        /* Create a new descriptor */
        _system_callback_manager_callback_subscription* subscription_ptr = new (std::nothrow) _system_callback_manager_callback_subscription;

        ASSERT_ALWAYS_SYNC(subscription_ptr != NULL,
                           "Out of memory");

        if (subscription_ptr != NULL)
        {
            subscription_ptr->callback_proc = pfn_callback_proc;
            subscription_ptr->owner_ptr     = callback_manager_ptr->callbacks + callback_id;
            subscription_ptr->synchronicity = callback_synchronicity;
            subscription_ptr->user_arg      = callback_proc_user_arg;

            system_resizable_vector_push(callback_manager_ptr->callbacks[callback_id].subscriptions,
                                         subscription_ptr);
        } /* if (subscription_ptr != NULL) */
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
                                                                           void*                   callback_proc_user_arg)
{
    _system_callback_manager* callback_manager_ptr = (_system_callback_manager*) callback_manager;

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
            _system_callback_manager_callback_subscription* subscription_ptr = NULL;

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
                        subscription_ptr = NULL;
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
        } /* for (all callback descriptors) */
    }
    system_read_write_mutex_unlock(callback_manager_ptr->callbacks[callback_id].subscriptions_mutex,
                                   ACCESS_WRITE);
}
