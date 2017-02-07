/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_pool.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_read_write_mutex.h"
#include "system/system_resource_pool.h"
#include "system/system_resizable_vector.h"

#define MAX_TEXTURE_IDLE_LIFETIME_MSEC (5000)

typedef struct _ral_texture_pool_texture_item
{
    system_time return_time;
    ral_texture texture;

} _ral_texture_pool_texture_item;

typedef struct _ral_texture_pool
{
    system_read_write_mutex access_mutex;
    system_resizable_vector contexts; /* do NOT release */
    system_hash64map        hash_to_texture_item_vector_map; /* hash -> vector of _ral_texture_pool_texture items for textures currently owned by the texture pool */

    system_resizable_vector active_textures; /* textures which are currently in use */
    bool                    being_released;
    system_resource_pool    texture_item_pool;

    explicit _ral_texture_pool();

    ~_ral_texture_pool();
} _ral_texture_pool;


/** Forward declarations */
PRIVATE system_hash64 _ral_texture_pool_get_texture_hash           (const ral_texture_create_info* info_ptr);
PRIVATE void          _ral_texture_pool_release_all_textures       (_ral_texture_pool*             texture_pool_ptr);
PRIVATE void          _ral_texture_pool_subscribe_for_notifications(_ral_texture_pool*             texture_pool_ptr,
                                                                    ral_context                    context,
                                                                    bool                           should_subscribe);

/** TODO */
_ral_texture_pool::_ral_texture_pool()
{
    access_mutex                    = system_read_write_mutex_create();
    active_textures                 = system_resizable_vector_create(16); /* capacity */
    being_released                  = false;
    contexts                        = system_resizable_vector_create(4,     /* capacity              */
                                                                     true); /* should_be_thread_safe */
    hash_to_texture_item_vector_map = system_hash64map_create       (sizeof(_ral_texture_pool_texture_item*) );
    texture_item_pool               = system_resource_pool_create   (sizeof(_ral_texture_pool_texture_item),
                                                                     16,       /* n_elements_to_preallocate */
                                                                     nullptr,  /* init_fn                   */
                                                                     nullptr); /* deinit_fn                 */
}

/** TODO */
_ral_texture_pool::~_ral_texture_pool()
{
    ASSERT_DEBUG_SYNC(being_released,
                      "Texture pool instance about to get killed, despite not having been informed about the fact in advance");

    if (access_mutex != nullptr)
    {
        system_read_write_mutex_release(access_mutex);

        access_mutex = nullptr;
    }

    if (active_textures != nullptr)
    {
        uint32_t n_active_textures = 0;

        system_resizable_vector_get_property(active_textures,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_active_textures);

        ASSERT_DEBUG_SYNC(n_active_textures == 0,
                          "Active textures detected.");

        system_resizable_vector_release(active_textures);
        active_textures = nullptr;
    }

    if (contexts != nullptr)
    {
        uint32_t n_contexts = 0;

        system_resizable_vector_get_property(contexts,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_contexts);

        for (uint32_t n_context = 0;
                      n_context < n_contexts;
                    ++n_context)
        {
            ral_context current_context;

            system_resizable_vector_get_element_at(contexts,
                                                   n_context,
                                                  &current_context);

            _ral_texture_pool_subscribe_for_notifications(this,
                                                          current_context,
                                                          false); /* should_subscribe */
        }

        system_resizable_vector_release(contexts);
        contexts = nullptr;
    }

    if (hash_to_texture_item_vector_map != nullptr)
    {
        uint32_t n_map_items = 0;

        system_hash64map_get_property(hash_to_texture_item_vector_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_map_items);

        ASSERT_DEBUG_SYNC(n_map_items == 0,
                          "texture_hash_to_items_vector_map has items at destruction time");

        system_hash64map_release(hash_to_texture_item_vector_map);
        hash_to_texture_item_vector_map = nullptr;
    }

    if (texture_item_pool != nullptr)
    {
        system_resource_pool_release(texture_item_pool);
        texture_item_pool = nullptr;
    }
}


/** TODO */
PRIVATE system_hash64 _ral_texture_pool_get_texture_hash(const ral_texture_create_info* info_ptr)
{
    ASSERT_DEBUG_SYNC(info_ptr->base_mipmap_depth  < (1 << 5)  &&
                      info_ptr->base_mipmap_height < (1 << 13) && 
                      info_ptr->base_mipmap_width  < (1 << 13) &&
                      info_ptr->format             < (1 << 7)  &&
                      info_ptr->n_layers           < (1 << 5)  &&
                      info_ptr->n_samples          < (1 << 5)  &&
                      info_ptr->type               < (1 << 4)  &&
                      info_ptr->usage              < (1 << 8),
                      "Requested texture cannot be handled by the texture pool");

    return (system_hash64) ((((uint64_t) info_ptr->base_mipmap_depth      & ((1 << 5)  - 1)) << 0)  |
                            (((uint64_t) info_ptr->base_mipmap_height     & ((1 << 13) - 1)) << 5)  |
                            (((uint64_t) info_ptr->base_mipmap_width      & ((1 << 13) - 1)) << 18) |
                            (((uint64_t) info_ptr->format                 & ((1 << 7)  - 1)) << 31) |
                            (((uint64_t) info_ptr->n_layers               & ((1 << 5)  - 1)) << 38) |
                            (((uint64_t) info_ptr->n_samples              & ((1 << 5)  - 1)) << 43) |
                            (((uint64_t) info_ptr->type                   & ((1 << 4)  - 1)) << 48) |
                            (((uint64_t) info_ptr->usage                  & ((1 << 8)  - 1)) << 52) |
                            (((uint64_t) info_ptr->fixed_sample_locations                  ) << 60) |
                            (((uint64_t) info_ptr->use_full_mipmap_chain                   ) << 61));
}

/** TODO */
PRIVATE void _ral_texture_pool_on_context_about_to_be_released(const void* callback_data,
                                                               void*       texture_pool_raw_ptr)
{
    const ral_context  context          = (ral_context) (callback_data);
    _ral_texture_pool* texture_pool_ptr = reinterpret_cast<_ral_texture_pool*>(texture_pool_raw_ptr);

    if (!texture_pool_ptr->being_released)
    {
        texture_pool_ptr->being_released = true;

        _ral_texture_pool_release_all_textures(texture_pool_ptr);
    }
}

/** TODO */
PRIVATE void _ral_texture_pool_on_texture_created(const void* callback_data,
                                                  void*       texture_pool_raw_ptr)
{
    const ral_context_callback_objects_created_callback_arg* callback_data_ptr = reinterpret_cast<const ral_context_callback_objects_created_callback_arg*>(callback_data);
    _ral_texture_pool*                                       texture_pool_ptr  = reinterpret_cast<_ral_texture_pool*>(texture_pool_raw_ptr);

    system_read_write_mutex_lock(texture_pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        for (uint32_t n_object = 0;
                      n_object < callback_data_ptr->n_objects;
                    ++n_object)
        {
            ral_context                     texture_context = nullptr;
            ral_texture_create_info         texture_create_info;
            system_hash64                   texture_hash;
            _ral_texture_pool_texture_item* texture_item_ptr = nullptr;
            system_resizable_vector         texture_item_vector;

            ral_texture_get_property(reinterpret_cast<ral_texture>(callback_data_ptr->created_objects[n_object]),
                                     RAL_TEXTURE_PROPERTY_CREATE_INFO,
                                    &texture_create_info);

            texture_hash = _ral_texture_pool_get_texture_hash(&texture_create_info);

            if (!system_hash64map_get(texture_pool_ptr->hash_to_texture_item_vector_map,
                                      texture_hash,
                                     &texture_item_vector) )
            {
                texture_item_vector = system_resizable_vector_create(4,     /* capacity              */
                                                                     true); /* should_be_thread_safe */
            }

            ASSERT_DEBUG_SYNC(system_resizable_vector_find(texture_pool_ptr->active_textures,
                                                           callback_data_ptr->created_objects[n_object]) == ITEM_NOT_FOUND,
                              "An attempt to attach a duplicate of an already recognized texture to the texture pool was detected.");

            texture_item_ptr = reinterpret_cast<_ral_texture_pool_texture_item*>(system_resource_pool_get_from_pool(texture_pool_ptr->texture_item_pool) );

            texture_item_ptr->return_time = system_time_now();
            texture_item_ptr->texture     = reinterpret_cast<ral_texture>(callback_data_ptr->created_objects[n_object]);

            system_resizable_vector_push(texture_pool_ptr->active_textures,
                                         texture_item_ptr);

            /* TODO: This call implies RAL texture instances are never released until they are
             *       explicitly purged by the texture pool. "Garbage cleaner" has NOT been
             *       implemented yet, meaning we'll just keep on allocating texture memory.
             *       This needs to be improved when the right time comes.
             */
            ral_texture_get_property(reinterpret_cast<ral_texture>(callback_data_ptr->created_objects[n_object]),
                                     RAL_TEXTURE_PROPERTY_CONTEXT,
                                    &texture_context);

            ral_context_retain_object(texture_context,
                                      RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                      callback_data_ptr->created_objects[n_object]);

            ASSERT_DEBUG_SYNC(system_resizable_vector_find(texture_pool_ptr->contexts,
                                                           texture_context) != ITEM_NOT_FOUND,
                              "Texture pool is unaware of the texture's parent context");
        }
    }
    system_read_write_mutex_unlock(texture_pool_ptr->access_mutex,
                                   ACCESS_WRITE);
}


/** TODO */
PRIVATE void _ral_texture_pool_release_all_textures(_ral_texture_pool* texture_pool_ptr)
{
    uint32_t n_texture_items = 0;
    uint32_t n_textures      = 0;

    system_read_write_mutex_lock(texture_pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        {
            ral_texture current_texture = nullptr;

            system_hash64map_get_property(texture_pool_ptr->hash_to_texture_item_vector_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_textures);

            while (system_resizable_vector_pop(texture_pool_ptr->active_textures,
                                               &current_texture) )
            {
                ral_context current_texture_context;

                ral_texture_get_property(current_texture,
                                         RAL_TEXTURE_PROPERTY_CONTEXT,
                                        &current_texture_context);

                ral_context_delete_objects(current_texture_context,
                                           RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                           1, /* n_objects */
                                           reinterpret_cast<void* const*>(&current_texture) );
            }
        }

        for (uint32_t n_map_item = 0;
                      n_map_item < n_textures;
                    ++n_map_item)
        {
            _ral_texture_pool_texture_item* current_texture_item_ptr = nullptr;
            ral_texture                     texture                  = nullptr;
            system_hash64                   texture_hash             = 0;
            system_resizable_vector         texture_item_vector      = nullptr;

            if (!system_hash64map_get_element_at(texture_pool_ptr->hash_to_texture_item_vector_map,
                                                 n_map_item,
                                                &texture_item_vector,
                                                &texture_hash) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve texture hash->texture item vector map entry at index [%d]",
                                  n_map_item);

                continue;
            }

            /* Release all dangling texture instances.
             *
             * TODO: This could easily be optimized.
             */
            while (system_resizable_vector_pop(texture_item_vector,
                                              &current_texture_item_ptr) )
            {
                ral_context current_texture_context;

                ral_texture_get_property(current_texture_item_ptr->texture,
                                         RAL_TEXTURE_PROPERTY_CONTEXT,
                                        &current_texture_context);

                ral_context_delete_objects(current_texture_context,
                                           RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                           1, /* n_objects */
                                           reinterpret_cast<void* const*>(&current_texture_item_ptr->texture) );

                system_resource_pool_return_to_pool(texture_pool_ptr->texture_item_pool,
                                                    reinterpret_cast<system_resource_pool_block>(current_texture_item_ptr) );
                current_texture_item_ptr = nullptr;
            }

            system_resizable_vector_release(texture_item_vector);
            texture_item_vector = nullptr;
        }

        system_hash64map_clear(texture_pool_ptr->hash_to_texture_item_vector_map);
    }
    system_read_write_mutex_unlock(texture_pool_ptr->access_mutex,
                                   ACCESS_WRITE);
}

/** Please see header for specification */
PRIVATE void _ral_texture_pool_subscribe_for_notifications(_ral_texture_pool* texture_pool_ptr,
                                                           ral_context        context,
                                                           bool               should_subscribe)
{
    system_callback_manager context_callback_manager = nullptr;

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,
                            &context_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _ral_texture_pool_on_context_about_to_be_released,
                                                        texture_pool_ptr);
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _ral_texture_pool_on_texture_created,
                                                        texture_pool_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                           _ral_texture_pool_on_context_about_to_be_released,
                                                           texture_pool_ptr);
        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                           RAL_CONTEXT_CALLBACK_ID_TEXTURES_CREATED,
                                                           _ral_texture_pool_on_texture_created,
                                                           texture_pool_ptr);
    }
}


/** Please see header for specification */
PUBLIC void ral_texture_pool_attach_context(ral_texture_pool pool,
                                            ral_context      context)
{
    _ral_texture_pool* pool_ptr = reinterpret_cast<_ral_texture_pool*>(pool);

    ASSERT_DEBUG_SYNC(context != nullptr,
                      "Null context was specified");
    ASSERT_DEBUG_SYNC(pool != nullptr,
                      "Null texture pool was specified");

    ASSERT_DEBUG_SYNC(system_resizable_vector_find(pool_ptr->contexts,
                                                   context) == ITEM_NOT_FOUND,
                      "The specified context has already been attached to the texture pool");

    system_read_write_mutex_lock(pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        system_resizable_vector_push(pool_ptr->contexts,
                                     context);
    }
    system_read_write_mutex_unlock(pool_ptr->access_mutex,
                                   ACCESS_WRITE);

    _ral_texture_pool_subscribe_for_notifications(pool_ptr,
                                                  context,
                                                  true); /* should_subscribe */
}

/** Please see header for specification */
PUBLIC bool ral_texture_pool_collect_garbage(ral_texture_pool pool)
{
    bool               has_released_texture = false;
    _ral_texture_pool* pool_ptr             = reinterpret_cast<_ral_texture_pool*>(pool);
    const system_time  time_now             = system_time_now();

    system_read_write_mutex_lock(pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        _ral_texture_pool_texture_item* current_texture_item_ptr = nullptr;
        system_resizable_vector         current_texture_item_vector;
        system_hash64                   current_texture_hash;
        uint32_t                        n_texture_vector         = 0;

        while (!has_released_texture                                                       &&
                system_hash64map_get_element_at(pool_ptr->hash_to_texture_item_vector_map,
                                                n_texture_vector,
                                               &current_texture_item_vector,
                                               &current_texture_hash) )
        {
            uint32_t n_current_vector_texture_item = 0;

            while (!has_released_texture                                                  &&
                   system_resizable_vector_get_element_at(current_texture_item_vector,
                                                          n_current_vector_texture_item,
                                                         &current_texture_item_ptr) )
            {
                uint32_t idle_lifetime_msec;

                system_time_get_msec_for_time(time_now - current_texture_item_ptr->return_time,
                                             &idle_lifetime_msec);

                if (idle_lifetime_msec >= MAX_TEXTURE_IDLE_LIFETIME_MSEC)
                {
                    ral_context texture_context = nullptr;

                    pool_ptr->being_released = true;
                    {
                        ral_texture_get_property  (current_texture_item_ptr->texture,
                                                   RAL_TEXTURE_PROPERTY_CONTEXT,
                                                  &texture_context);
                        ral_context_delete_objects(texture_context,
                                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                                   1, /* n_objects */
                                                   reinterpret_cast<void* const*>(&current_texture_item_ptr->texture) );
                    }
                    pool_ptr->being_released = false;

                    system_resource_pool_return_to_pool(pool_ptr->texture_item_pool,
                                                        reinterpret_cast<system_resource_pool_block>(current_texture_item_ptr) );

                    system_resizable_vector_delete_element_at(current_texture_item_vector,
                                                              n_current_vector_texture_item);

                    has_released_texture = true;
                }

                ++n_current_vector_texture_item;
            }

            ++n_texture_vector;
        }
    }
    system_read_write_mutex_unlock(pool_ptr->access_mutex,
                                   ACCESS_WRITE);

    return has_released_texture;
}

/** Please see header for specification */
PUBLIC ral_texture_pool ral_texture_pool_create()
{
    _ral_texture_pool* new_texture_pool_ptr = new (std::nothrow) _ral_texture_pool();

    ASSERT_ALWAYS_SYNC(new_texture_pool_ptr != nullptr,
                       "Out of memory");

    if (new_texture_pool_ptr != nullptr)
    {
        /* Nothing to do here */
    }

    return (ral_texture_pool) new_texture_pool_ptr;
}

/** Please see header for specification */
PUBLIC void ral_texture_pool_detach_context(ral_texture_pool pool,
                                            ral_context      context)
{
    uint32_t           context_index;
    _ral_texture_pool* pool_ptr = reinterpret_cast<_ral_texture_pool*>(pool);

    ASSERT_DEBUG_SYNC(context != nullptr,
                      "Null context was specified");
    ASSERT_DEBUG_SYNC(pool != nullptr,
                      "Null texture pool was specified");

    system_read_write_mutex_lock(pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        context_index = system_resizable_vector_find(pool_ptr->contexts,
                                                     context);

        ASSERT_DEBUG_SYNC(context_index != ITEM_NOT_FOUND,
                          "The specified context has not been found as attached to the texture pool");

        system_resizable_vector_delete_element_at(pool_ptr->contexts,
                                                  context_index);
    }
    system_read_write_mutex_unlock(pool_ptr->access_mutex,
                                   ACCESS_WRITE);

    _ral_texture_pool_subscribe_for_notifications(pool_ptr,
                                                  context,
                                                  false); /* should_subscribe */
}

/** Please see header for specification */
PUBLIC void ral_texture_pool_empty(ral_texture_pool pool)
{
    _ral_texture_pool* pool_ptr = reinterpret_cast<_ral_texture_pool*>(pool);

    _ral_texture_pool_release_all_textures(pool_ptr);
}

/** Please see header for specification */
PUBLIC bool ral_texture_pool_get(ral_texture_pool               texture_pool,
                                 const ral_texture_create_info* texture_create_info_ptr,
                                 ral_texture*                   out_result_texture_ptr)
{
    bool                    result               = false;
    system_hash64           texture_hash;
    system_resizable_vector texture_item_vector  = nullptr;
    _ral_texture_pool*      texture_pool_ptr     = (_ral_texture_pool*) texture_pool;

    if (texture_pool == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture_pool != nullptr,
                          "Input ral_texture_pool instance is NULL");

        goto end;
    }

    if (texture_create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture_create_info_ptr != nullptr,
                          "Input ral_texture_create_info instance is NULL");

        goto end;
    }

    /* Is there an unused texture instance we may spare? */
    texture_hash = _ral_texture_pool_get_texture_hash(texture_create_info_ptr);

    system_read_write_mutex_lock(texture_pool_ptr->access_mutex,
                                 ACCESS_READ);
    {
        if (system_hash64map_get(texture_pool_ptr->hash_to_texture_item_vector_map,
                                 texture_hash,
                                &texture_item_vector) )
        {
            _ral_texture_pool_texture_item* texture_item_ptr = nullptr;

            result = system_resizable_vector_pop(texture_item_vector,
                                                &texture_item_ptr);

            if (result)
            {
                *out_result_texture_ptr = texture_item_ptr->texture;

                system_resource_pool_return_to_pool(texture_pool_ptr->texture_item_pool,
                                                    reinterpret_cast<system_resource_pool_block>(texture_item_ptr) );
            }
        }
    }
    system_read_write_mutex_unlock(texture_pool_ptr->access_mutex,
                                   ACCESS_READ);

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC void ral_texture_pool_get_property(ral_texture_pool          pool,
                                          ral_texture_pool_property property,
                                          void*                     out_result)
{
    _ral_texture_pool* pool_ptr = (_ral_texture_pool*) pool;

    if (pool == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_texture_pool instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_TEXTURE_POOL_PROPERTY_IS_BEING_RELEASED:
        {
            *reinterpret_cast<bool*>(out_result) = pool_ptr->being_released;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_pool_property value requested.");

            goto end;
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_texture_pool_release(ral_texture_pool pool)
{
    _ral_texture_pool* pool_ptr = reinterpret_cast<_ral_texture_pool*>(pool);

    ASSERT_DEBUG_SYNC(pool != nullptr,
                      "Input ral_texture_pool instance is NULL");

    ral_texture_pool_empty(pool);

    delete pool_ptr;
}

/** Please see header for specification */
PUBLIC bool ral_texture_pool_return_texture(ral_texture_pool pool,
                                            ral_texture      texture)
{
    size_t                          active_texture_index = -1;
    bool                            mutex_locked         = false;
    _ral_texture_pool*              pool_ptr             = (_ral_texture_pool*) pool;
    bool                            result               = false;
    ral_texture_create_info         texture_create_info;
    system_hash64                   texture_hash;
    _ral_texture_pool_texture_item* texture_item_ptr     = nullptr;
    system_resizable_vector         texture_item_vector  = nullptr;

    if (pool_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(pool_ptr != nullptr,
                          "Input ral_texture_pool instance is NULL");

        goto end;
    }

    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture != nullptr,
                          "Input ral_texture instance is NULL");

        goto end;
    }

    if (pool_ptr->being_released)
    {
        goto end;
    }

    /* Check if the texture hash is already assigned a vector. If not, spawn it before continuing */
    ral_texture_get_property(texture,
                             RAL_TEXTURE_PROPERTY_CREATE_INFO,
                            &texture_create_info);

    texture_hash = _ral_texture_pool_get_texture_hash(&texture_create_info);

    system_read_write_mutex_lock(pool_ptr->access_mutex,
                                 ACCESS_WRITE);
    {
        mutex_locked = true;

        if (!system_hash64map_get(pool_ptr->hash_to_texture_item_vector_map,
                                  texture_hash,
                                 &texture_item_vector) )
        {
            texture_item_vector = system_resizable_vector_create(4 /* capacity */);

            if (texture_item_vector == nullptr)
            {
                ASSERT_DEBUG_SYNC(texture_item_vector != nullptr,
                                  "Could not instantiate a texture vector");

                goto end;
            }

            system_hash64map_insert(pool_ptr->hash_to_texture_item_vector_map,
                                    texture_hash,
                                    texture_item_vector,
                                    nullptr,  /* callback          */
                                    nullptr); /* callback_argument */
        }

        /* Stash the texture */
        texture_item_ptr = reinterpret_cast<_ral_texture_pool_texture_item*>(system_resource_pool_get_from_pool(pool_ptr->texture_item_pool) );

        texture_item_ptr->return_time = system_time_now();
        texture_item_ptr->texture     = texture;

        system_resizable_vector_push(texture_item_vector,
                                     texture_item_ptr);
    }
    /* Unlock at end: */

    uint32_t active_textures_index = system_resizable_vector_find(pool_ptr->active_textures,
                                                                  texture);

    if (active_textures_index != ITEM_NOT_FOUND)
    {
        system_resizable_vector_delete_element_at(pool_ptr->active_textures,
                                                  active_textures_index);
    }

    /* All done */
    result = true;
end:
    if (mutex_locked)
    {
        system_read_write_mutex_unlock(pool_ptr->access_mutex,
                                       ACCESS_WRITE);
    }

    return result;
}