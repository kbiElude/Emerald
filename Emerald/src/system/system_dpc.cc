#include "shared.h"
#include "system/system_dpc.h"
#include "system/system_hash64map.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"
#include "system/system_thread_pool.h"


PRIVATE system_hash64map        _active_dpcs            = nullptr;
PRIVATE system_read_write_mutex _active_dpcs_mutex      = nullptr;
PRIVATE system_resource_pool    _dpc_item_callback_pool = nullptr;
PRIVATE system_resource_pool    _dpc_item_pool          = nullptr;

struct _system_dpc_item;

typedef struct
{
    void*                    callback_arg;
    void*                    object;
    _system_dpc_item*        owner_ptr;
    PFNSYSTEMDPCCALLBACKPROC pfn_callback_proc;
} _system_dpc_item_callback;

typedef struct _system_dpc_item
{
    system_resizable_vector scheduled_callbacks_vector;


    static void deinit(system_resource_pool_block item_raw_ptr)
    {
        _system_dpc_item* item_ptr = reinterpret_cast<_system_dpc_item*>(item_raw_ptr);

        system_resizable_vector_release(item_ptr->scheduled_callbacks_vector);
        item_ptr->scheduled_callbacks_vector = nullptr;
    }

    static void init(system_resource_pool_block item_raw_ptr)
    {
        _system_dpc_item* item_ptr = reinterpret_cast<_system_dpc_item*>(item_raw_ptr);

        item_ptr->scheduled_callbacks_vector = system_resizable_vector_create(8); /* capacity */
    }

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
        uint32_t callback_index        = -1;
        uint32_t n_scheduled_callbacks = -1;

        ASSERT_DEBUG_SYNC(system_hash64map_contains(_active_dpcs,
                                                    object_hash64),
                          "TODO: Add support for multiple active DPCs per object instance");

        system_resizable_vector_get_property(callback_data_ptr->owner_ptr->scheduled_callbacks_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_scheduled_callbacks);

        if (n_scheduled_callbacks > 1)
        {
            callback_index = system_resizable_vector_find(callback_data_ptr->owner_ptr->scheduled_callbacks_vector,
                                                          callback_data_ptr);
            ASSERT_DEBUG_SYNC(callback_index != ITEM_NOT_FOUND,
                              "DPC item callback was not found in the DPC item stash?!");

            system_resizable_vector_delete_element_at(callback_data_ptr->owner_ptr->scheduled_callbacks_vector,
                                                      callback_index);
        }
        else
        {
            system_resizable_vector_clear(callback_data_ptr->owner_ptr->scheduled_callbacks_vector);

            system_hash64map_remove(_active_dpcs,
                                    object_hash64);

            system_resource_pool_return_to_pool(_dpc_item_pool,
                                                reinterpret_cast<system_resource_pool_block>(callback_data_ptr->owner_ptr) );
        }

        memset(callback_data_ptr,
               0,
               sizeof(*callback_data_ptr) );

        system_resource_pool_return_to_pool(_dpc_item_callback_pool,
                                            reinterpret_cast<system_resource_pool_block>(callback_data_ptr) );
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

    system_resource_pool_release(_dpc_item_callback_pool);
    _dpc_item_callback_pool = nullptr;

    system_resource_pool_release(_dpc_item_pool);
    _dpc_item_pool = nullptr;
}

/** Please see header for specification */
PUBLIC void system_dpc_init()
{
    _active_dpcs            = system_hash64map_create       (sizeof(_system_dpc_item*) );
    _active_dpcs_mutex      = system_read_write_mutex_create();
    _dpc_item_callback_pool = system_resource_pool_create   (sizeof(_system_dpc_item_callback),
                                                             64,       /* n_elements_to_preallocate */
                                                             nullptr,  /* init_fn                   */
                                                             nullptr); /* deinit_fn                 */
    _dpc_item_pool          = system_resource_pool_create   (sizeof(_system_dpc_item),
                                                             64,       /* n_elements_to_preallocate */
                                                             _system_dpc_item::init,
                                                             _system_dpc_item::deinit);
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
        _system_dpc_item*          dpc_item_ptr          = nullptr;
        _system_dpc_item_callback* dpc_item_callback_ptr = nullptr;

        if (!system_hash64map_get(_active_dpcs,
                                  object_hash64,
                                 &dpc_item_ptr) )
        {
            dpc_item_ptr = reinterpret_cast<_system_dpc_item*>(system_resource_pool_get_from_pool(_dpc_item_pool) );

            system_hash64map_insert(_active_dpcs,
                                    object_hash64,
                                    dpc_item_ptr,
                                    nullptr,  /* on_removal_callback          */
                                    nullptr); /* on_removal_callback_user_arg */
        }

        dpc_item_callback_ptr = reinterpret_cast<_system_dpc_item_callback*>(system_resource_pool_get_from_pool(_dpc_item_callback_pool) );

        dpc_item_callback_ptr->callback_arg      = callback_arg;
        dpc_item_callback_ptr->object            = object;
        dpc_item_callback_ptr->owner_ptr         = dpc_item_ptr;
        dpc_item_callback_ptr->pfn_callback_proc = pfn_callback_proc;

        system_resizable_vector_push(dpc_item_ptr->scheduled_callbacks_vector,
                                     dpc_item_callback_ptr);

        dpc_task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                               _system_dpc_thread_pool_callback,
                                                               dpc_item_callback_ptr);
    }
    system_read_write_mutex_unlock(_active_dpcs_mutex,
                                   ACCESS_WRITE);

    system_thread_pool_submit_single_task(dpc_task);
}