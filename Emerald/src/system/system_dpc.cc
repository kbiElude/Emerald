#include "shared.h"
#include "system/system_dpc.h"
#include "system/system_hash64map.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"

#define MAX_SCHEDULED_CALLBACKS (8)

PRIVATE system_hash64map        _active_dpcs       = nullptr;
PRIVATE system_read_write_mutex _active_dpcs_mutex = nullptr;
PRIVATE system_resource_pool    _dpc_item_pool     = nullptr;

struct _system_dpc_item;

typedef struct
{
    void*                    callback_arg;
    uint32_t                 n_callback_slot;
    void*                    object;
    _system_dpc_item*        owner_ptr;
    PFNSYSTEMDPCCALLBACKPROC pfn_callback_proc;
} _system_dpc_item_callback;

typedef struct _system_dpc_item
{
    uint32_t                  n_scheduled_callbacks;
    _system_dpc_item_callback scheduled_callbacks[MAX_SCHEDULED_CALLBACKS];
} _system_dpc_item;


/** TODO */
PRIVATE volatile void _system_dpc_thread_pool_callback(system_thread_pool_callback_argument arg)
{
    _system_dpc_item_callback* callback_data_ptr = reinterpret_cast<_system_dpc_item_callback*>(arg);
    const system_hash64        object_hash64     = reinterpret_cast<system_hash64>             (callback_data_ptr->object);

    /* Call back, as requested */
    callback_data_ptr->pfn_callback_proc(callback_data_ptr->object,
                                         callback_data_ptr->callback_arg);

    /* Wipe the DPC callback item out of books */
    system_read_write_mutex_lock(_active_dpcs_mutex,
                                 ACCESS_WRITE);
    {
        ASSERT_DEBUG_SYNC(system_hash64map_contains(_active_dpcs,
                                                    object_hash64),
                          "TODO: Add support for multiple active DPCs per object instance");

        if (--callback_data_ptr->owner_ptr->n_scheduled_callbacks == 0)
        {
            system_resource_pool_return_to_pool(_dpc_item_pool,
                                                reinterpret_cast<system_resource_pool_block>(callback_data_ptr->owner_ptr) );

            system_hash64map_remove(_active_dpcs,
                                    object_hash64);
        }
        else
        {
            memset(callback_data_ptr->owner_ptr->scheduled_callbacks + callback_data_ptr->n_callback_slot,
                   0,
                   sizeof(_system_dpc_item_callback) );
        }
    }
    system_read_write_mutex_unlock(_active_dpcs_mutex,
                                   ACCESS_WRITE);
}

/** Please see header for specification */
PUBLIC void system_dpc_deinit()
{
    uint32_t n_pending_dpcs = 0;

    system_read_write_mutex_lock(_active_dpcs_mutex,
                                 ACCESS_WRITE);
    {
        system_hash64map_get_property(_active_dpcs,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_pending_dpcs);

        ASSERT_DEBUG_SYNC(n_pending_dpcs == 0,
                          "Pending DPCs at system_dpc_deinit() call time!");

        system_hash64map_release(_active_dpcs);
        _active_dpcs = nullptr;
    }
    system_read_write_mutex_unlock(_active_dpcs_mutex,
                                   ACCESS_WRITE);

    system_read_write_mutex_release(_active_dpcs_mutex);
    _active_dpcs_mutex = nullptr;

    system_resource_pool_release(_dpc_item_pool);
    _dpc_item_pool = nullptr;
}

/** Please see header for specification */
PUBLIC void system_dpc_init()
{
    _active_dpcs       = system_hash64map_create       (sizeof(_system_dpc_item*) ); /* should_be_thread_safe */
    _active_dpcs_mutex = system_read_write_mutex_create();
    _dpc_item_pool     = system_resource_pool_create   (sizeof(_system_dpc_item),
                                                        64,       /* n_elements_to_preallocate */
                                                        nullptr,  /* init_fn                   */
                                                        nullptr); /* deinit_fn                 */
}

/** Please see header for specification */
PUBLIC bool system_dpc_is_object_pending_callback(void* object)
{
    bool result;

    system_read_write_mutex_lock(_active_dpcs_mutex,
                                 ACCESS_READ);
    {
        result = system_hash64map_contains(_active_dpcs,
                                           reinterpret_cast<system_hash64>(object) );
    }
    system_read_write_mutex_unlock(_active_dpcs_mutex,
                                   ACCESS_READ);

    return result;
}

/** Please see header for specification */
PUBLIC void system_dpc_schedule(void*                    object,
                                PFNSYSTEMDPCCALLBACKPROC pfn_callback_proc,
                                void*                    callback_arg)
{
    system_thread_pool_task dpc_task;
    const system_hash64     object_hash64 = reinterpret_cast<system_hash64>(object);

    system_read_write_mutex_lock(_active_dpcs_mutex,
                                 ACCESS_WRITE);
    {
        _system_dpc_item* dpc_item_ptr = nullptr;

        if (!system_hash64map_get(_active_dpcs,
                                  object_hash64,
                                 &dpc_item_ptr) )
        {
            _system_dpc_item* new_item_ptr = reinterpret_cast<_system_dpc_item*>(system_resource_pool_get_from_pool(_dpc_item_pool) );

            memset(new_item_ptr,
                   0,
                   sizeof(_system_dpc_item) );

            new_item_ptr->scheduled_callbacks[0].callback_arg      = callback_arg;
            new_item_ptr->scheduled_callbacks[0].n_callback_slot   = 0;
            new_item_ptr->scheduled_callbacks[0].object            = object;
            new_item_ptr->scheduled_callbacks[0].owner_ptr         = new_item_ptr;
            new_item_ptr->scheduled_callbacks[0].pfn_callback_proc = pfn_callback_proc;
            new_item_ptr->n_scheduled_callbacks                    = 1;

            system_hash64map_insert(_active_dpcs,
                                    object_hash64,
                                    new_item_ptr,
                                    nullptr,  /* on_removal_callback          */
                                    nullptr); /* on_removal_callback_user_arg */

            dpc_task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                   _system_dpc_thread_pool_callback,
                                                                   new_item_ptr->scheduled_callbacks + 0);
        }
        else
        {
            ASSERT_DEBUG_SYNC(dpc_item_ptr->n_scheduled_callbacks < MAX_SCHEDULED_CALLBACKS,
                              "TODO");

            /* Identify a free slot */
            static const _system_dpc_item_callback dummy_callback = {0};
            uint32_t                               n_callback_slot;

            for (n_callback_slot = 0;
                 n_callback_slot < MAX_SCHEDULED_CALLBACKS;
               ++n_callback_slot)
            {
                if (memcmp(dpc_item_ptr->scheduled_callbacks + n_callback_slot,
                          &dummy_callback,
                           sizeof(dummy_callback)) == 0)
                {
                    break;
                }
            }

            ASSERT_DEBUG_SYNC(n_callback_slot != MAX_SCHEDULED_CALLBACKS,
                              "TODO: Add support for this scenario");

            dpc_item_ptr->scheduled_callbacks[n_callback_slot].callback_arg      = callback_arg;
            dpc_item_ptr->scheduled_callbacks[n_callback_slot].n_callback_slot   = n_callback_slot;
            dpc_item_ptr->scheduled_callbacks[n_callback_slot].object            = object;
            dpc_item_ptr->scheduled_callbacks[n_callback_slot].owner_ptr         = dpc_item_ptr;
            dpc_item_ptr->scheduled_callbacks[n_callback_slot].pfn_callback_proc = pfn_callback_proc;
            dpc_item_ptr->n_scheduled_callbacks++;

            dpc_task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                   _system_dpc_thread_pool_callback,
                                                                   dpc_item_ptr->scheduled_callbacks + n_callback_slot);
        }
    }
    system_read_write_mutex_unlock(_active_dpcs_mutex,
                                   ACCESS_WRITE);

    system_thread_pool_submit_single_task(dpc_task);
}