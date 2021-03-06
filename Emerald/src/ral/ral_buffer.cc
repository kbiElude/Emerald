/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"

typedef struct _ral_buffer
{
    system_callback_manager     callback_manager;
    ral_context                 context;
    ral_buffer_mappability_bits mappability_bits;
    system_hashed_ansi_string   name;
    ral_buffer                  parent_buffer;
    uint32_t                    size;
    uint32_t                    start_offset;
    ral_buffer_property_bits    property_bits;
    ral_buffer_usage_bits       usage_bits;
    ral_queue_bits              user_queue_bits;


    _ral_buffer(ral_context                 in_context,
                system_hashed_ansi_string   in_name,
                ral_buffer                  in_parent_buffer,
                uint32_t                    in_size,
                uint32_t                    in_start_offset,
                ral_buffer_usage_bits       in_usage_bits,
                ral_queue_bits              in_user_queue_bits,
                ral_buffer_property_bits    in_property_bits,
                ral_buffer_mappability_bits in_mappability_bits)
    {
        callback_manager = system_callback_manager_create( (_callback_id) RAL_BUFFER_CALLBACK_ID_COUNT);
        context          = in_context;
        name             = in_name;
        parent_buffer    = in_parent_buffer;
        size             = in_size;
        start_offset     = in_start_offset;

        if (parent_buffer != nullptr)
        {
            _ral_buffer* parent_buffer_ptr = reinterpret_cast<_ral_buffer*>(parent_buffer);

            ral_context_retain_object(context,
                                      RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                      parent_buffer);

            ASSERT_DEBUG_SYNC(in_mappability_bits == 0                                  ||
                              in_mappability_bits == parent_buffer_ptr->mappability_bits,
                              "Parent<->child buffer mappability bits setting mismatch detected at creation time");
            ASSERT_DEBUG_SYNC(in_property_bits == 0                                ||
                              in_property_bits == parent_buffer_ptr->property_bits,
                              "Parent<->child buffer property bits setting mismatch detected at creation time");
            ASSERT_DEBUG_SYNC(in_usage_bits == 0                             ||
                              in_usage_bits == parent_buffer_ptr->usage_bits,
                              "Parent<->child buffer usage bits setting mismatch detected at creation time");
            ASSERT_DEBUG_SYNC(in_user_queue_bits == 0                                  ||
                              in_user_queue_bits == parent_buffer_ptr->user_queue_bits,
                              "Parent<->child buffer usage bits setting mismatch detected at creation time");

            mappability_bits = parent_buffer_ptr->mappability_bits;
            property_bits    = parent_buffer_ptr->property_bits;
            usage_bits       = parent_buffer_ptr->usage_bits;
            user_queue_bits  = parent_buffer_ptr->user_queue_bits;
        }
        else
        {
            mappability_bits = in_mappability_bits;
            property_bits    = in_property_bits;
            usage_bits       = in_usage_bits;
            user_queue_bits  = in_user_queue_bits;
        }
    }

    ~_ral_buffer()
    {
        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }

        if (parent_buffer != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&parent_buffer) );
        }
    }
} _ral_buffer;


/** Please see header for specification */
PUBLIC EMERALD_API bool ral_buffer_clear_region(ral_buffer                    buffer,
                                                uint32_t                      n_clear_ops,
                                                ral_buffer_clear_region_info* clear_ops,
                                                bool                          sync_other_contexts)
{
    _ral_buffer*                         buffer_ptr   = reinterpret_cast<_ral_buffer*>(buffer);
    ral_buffer_clear_region_callback_arg callback_arg;
    bool                                 result       = false;

    /* Sanity checks */
    if (buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input buffer is NULL");

        goto end;
    }

    if (n_clear_ops == 0)
    {
        result = true;

        goto end;
    }

    if (clear_ops == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input clear info array is NULL");

        goto end;
    }

    /* Convert input data to a callback argument and send a notification, so that active rendering contexts
     * can handle the request. */
    callback_arg.buffer              = buffer;
    callback_arg.clear_ops           = clear_ops;
    callback_arg.n_clear_ops         = n_clear_ops;
    callback_arg.sync_other_contexts = sync_other_contexts;

    for (uint32_t n_clear_op = 0;
                  n_clear_op < n_clear_ops;
                ++n_clear_op)
    {
        clear_ops[n_clear_op].offset += buffer_ptr->start_offset;
    }

    system_callback_manager_call_back(buffer_ptr->callback_manager,
                                      RAL_BUFFER_CALLBACK_ID_CLEAR_REGION_REQUESTED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_buffer_copy_to_buffer(ral_buffer                      src_buffer,
                                                  ral_buffer                      dst_buffer,
                                                  uint32_t                        n_copy_ops,
                                                  ral_buffer_copy_to_buffer_info* copy_ops,
                                                  bool                            sync_other_contexts)
{
    ral_buffer_copy_to_buffer_callback_arg callback_arg;
    _ral_buffer*                           dst_buffer_ptr = reinterpret_cast<_ral_buffer*>(dst_buffer);
    bool                                   result         = false;
    _ral_buffer*                           src_buffer_ptr = reinterpret_cast<_ral_buffer*>(src_buffer);

    /* Sanity checks */
    if (dst_buffer == nullptr || src_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input destination/source buffer is NULL");

        goto end;
    }

    if (n_copy_ops == 0)
    {
        result = true;

        goto end;
    }

    if (copy_ops == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input copy info array is NULL");

        goto end;
    }

    /* Convert input data to a callback argument and send a notification, so that active rendering backends
     * can handle the request. */
    callback_arg.copy_ops            = copy_ops;
    callback_arg.dst_buffer          = dst_buffer;
    callback_arg.n_copy_ops          = n_copy_ops;
    callback_arg.src_buffer          = src_buffer;
    callback_arg.sync_other_contexts = sync_other_contexts;

    for (uint32_t n_copy_op = 0;
                  n_copy_op < n_copy_ops;
                ++n_copy_op)
    {
        copy_ops[n_copy_op].dst_buffer_region_start_offset += dst_buffer_ptr->start_offset;
        copy_ops[n_copy_op].src_buffer_region_start_offset += src_buffer_ptr->start_offset;
    }

    system_callback_manager_call_back(src_buffer_ptr->callback_manager,
                                      RAL_BUFFER_CALLBACK_ID_BUFFER_TO_BUFFER_COPY_REQUESTED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC ral_buffer ral_buffer_create(ral_context                   context,
                                    system_hashed_ansi_string     name,
                                    const ral_buffer_create_info* create_info_ptr)
{
    _ral_buffer* new_buffer_ptr = nullptr;

    /* Sanity checks */
    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL name specified");

        goto end;
    }

    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Create info pointer is NULL");

        goto end;
    }

    if (create_info_ptr->mappability_bits >= (1 << (RAL_BUFFER_MAPPABILITY_LAST_USED_BIT + 1)) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid buffer mappability bit specified");

        goto end;
    }

    if (create_info_ptr->size == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Zero byte size specified");

        goto end;
    }

    if (create_info_ptr->property_bits >= (1 << (RAL_BUFFER_PROPERTY_LAST_USED_BIT + 1)) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid buffer property bit specified");

        goto end;
    }

    if (create_info_ptr->usage_bits >= (1 << (RAL_BUFFER_USAGE_LAST_USED_BIT + 1)) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid usage bit specified");

        goto end;
    }

    if (create_info_ptr->user_queue_bits >= (1 << (RAL_QUEUE_LAST_USED_BIT + 1) ))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid user queue bit specified");

        goto end;
    }

    /* Create the new descriptor */
    new_buffer_ptr = new (std::nothrow) _ral_buffer(context,
                                                    name,
                                                    create_info_ptr->parent_buffer,
                                                    create_info_ptr->size,
                                                    create_info_ptr->start_offset,
                                                    create_info_ptr->usage_bits,
                                                    create_info_ptr->user_queue_bits,
                                                    create_info_ptr->property_bits,
                                                    create_info_ptr->mappability_bits);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != nullptr,
                       "Out of memory");

end:
    return (ral_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_buffer_get_property(const ral_buffer    buffer,
                                                ral_buffer_property property,
                                                void*               out_result_ptr)
{
    const _ral_buffer* buffer_ptr = reinterpret_cast<const _ral_buffer*>(buffer);

    /* Sanity checks */
    if (buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL buffer is NULL");

        goto end;
    }

    /* Retrieve the requested property value */
    switch (property)
    {
        case RAL_BUFFER_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = buffer_ptr->callback_manager;

            break;
        }

        case RAL_BUFFER_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = buffer_ptr->context;

            break;
        }

        case RAL_BUFFER_PROPERTY_MAPPABILITY_BITS:
        {
            *reinterpret_cast<ral_buffer_mappability_bits*>(out_result_ptr) = buffer_ptr->mappability_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_PARENT_BUFFER:
        {
            *reinterpret_cast<ral_buffer*>(out_result_ptr) = buffer_ptr->parent_buffer;

            break;
        }

        case RAL_BUFFER_PROPERTY_PARENT_BUFFER_TOPMOST:
        {
            if (buffer_ptr->parent_buffer == nullptr)
            {
                *reinterpret_cast<ral_buffer*>(out_result_ptr) = nullptr;
            }
            else
            {
                const _ral_buffer* result_buffer_ptr = reinterpret_cast<_ral_buffer*>(buffer_ptr->parent_buffer);

                while (result_buffer_ptr->parent_buffer != nullptr)
                {
                    result_buffer_ptr = reinterpret_cast<_ral_buffer*>(result_buffer_ptr->parent_buffer);
                }

                *reinterpret_cast<const _ral_buffer**>(out_result_ptr) = result_buffer_ptr;
            }

            break;
        }

        case RAL_BUFFER_PROPERTY_PROPERTY_BITS:
        {
            *reinterpret_cast<ral_buffer_property_bits*>(out_result_ptr) = buffer_ptr->property_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_START_OFFSET:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = buffer_ptr->start_offset;

            break;
        }

        case RAL_BUFFER_PROPERTY_USAGE_BITS:
        {
            *reinterpret_cast<ral_buffer_usage_bits*>(out_result_ptr) = buffer_ptr->usage_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_USER_QUEUE_BITS:
        {
            *reinterpret_cast<ral_queue_bits*>(out_result_ptr) = buffer_ptr->user_queue_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_SIZE:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = buffer_ptr->size;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_buffer_property value");
        }
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_buffer_release(ral_buffer& buffer)
{
    delete reinterpret_cast<_ral_buffer*>(buffer);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_buffer_set_data_from_client_memory(ral_buffer                                                           buffer,
                                                               std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > updates,
                                                               bool                                                                 async,
                                                               bool                                                                 sync_other_contexts)
{
    _ral_buffer*                                       buffer_ptr = reinterpret_cast<_ral_buffer*>(buffer);
    ral_buffer_client_sourced_update_info_callback_arg callback_arg;
    bool                                               result     = false;

    /* Sanity checks */
    if (buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_buffer instance is NULL");

        goto end;
    }

    if (updates.size() == 0)
    {
        result = true;

        goto end;
    }

    /* Convert input data to a callback argument and send a notification, so that active rendering contexts
     * can handle the request. */
    callback_arg.async               = async;
    callback_arg.buffer              = buffer;
    callback_arg.sync_other_contexts = sync_other_contexts;
    callback_arg.updates             = updates;

    for (auto update_ptr : updates)
    {
        update_ptr->start_offset += buffer_ptr->start_offset;
    }

    system_callback_manager_call_back(buffer_ptr->callback_manager,
                                      RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}