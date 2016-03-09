/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"


typedef struct _ral_buffer
{
    system_callback_manager     callback_manager;
    ral_buffer_mappability_bits mappability_bits;
    system_hashed_ansi_string   name;
    ral_buffer                  parent_buffer;
    uint32_t                    size;
    uint32_t                    start_offset;
    ral_buffer_property_bits    property_bits;
    ral_buffer_usage_bits       usage_bits;
    ral_queue_bits              user_queue_bits;


    _ral_buffer(system_hashed_ansi_string   in_name,
                ral_buffer                  in_parent_buffer,
                uint32_t                    in_size,
                uint32_t                    in_start_offset,
                ral_buffer_usage_bits       in_usage_bits,
                ral_queue_bits              in_user_queue_bits,
                ral_buffer_property_bits    in_property_bits,
                ral_buffer_mappability_bits in_mappability_bits)
    {
        ASSERT_DEBUG_SYNC(in_start_offset == 0, "!!");

        callback_manager = system_callback_manager_create( (_callback_id) RAL_BUFFER_CALLBACK_ID_COUNT);
        mappability_bits = in_mappability_bits;
        name             = in_name;
        parent_buffer    = in_parent_buffer;
        property_bits    = in_property_bits;
        size             = in_size;
        start_offset     = in_start_offset;
        usage_bits       = in_usage_bits;
        user_queue_bits  = in_user_queue_bits;
    }

    ~_ral_buffer()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        }
    }
} _ral_buffer;


/** Please see header for specification */
PUBLIC ral_buffer ral_buffer_create(system_hashed_ansi_string     name,
                                    const ral_buffer_create_info* create_info_ptr)
{
    _ral_buffer* new_buffer_ptr = NULL;

    /* Sanity checks */
    if (name == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL name specified");

        goto end;
    }

    if (create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Create info pointer is NULL");

        goto end;
    }

    ASSERT_DEBUG_SYNC(create_info_ptr->parent_buffer == NULL,
                      "TODO");

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
    new_buffer_ptr = new (std::nothrow) _ral_buffer(name,
                                                    create_info_ptr->parent_buffer,
                                                    create_info_ptr->size,
                                                    create_info_ptr->start_offset,
                                                    create_info_ptr->usage_bits,
                                                    create_info_ptr->user_queue_bits,
                                                    create_info_ptr->property_bits,
                                                    create_info_ptr->mappability_bits);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != NULL,
                       "Out of memory");

end:
    return (ral_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_buffer_get_property(ral_buffer          buffer,
                                                ral_buffer_property property,
                                                void*               out_result_ptr)
{
    _ral_buffer* buffer_ptr = (_ral_buffer*) buffer;

    /* Sanity checks */
    if (buffer_ptr == NULL)
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
            *(system_callback_manager*) out_result_ptr = buffer_ptr->callback_manager;

            break;
        }

        case RAL_BUFFER_PROPERTY_MAPPABILITY_BITS:
        {
            *(ral_buffer_mappability_bits*) out_result_ptr = buffer_ptr->mappability_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_PARENT_BUFFER:
        {
            *(ral_buffer*) out_result_ptr = buffer_ptr->parent_buffer;

            break;
        }

        case RAL_BUFFER_PROPERTY_PROPERTY_BITS:
        {
            *(ral_buffer_property_bits*) out_result_ptr = buffer_ptr->property_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_START_OFFSET:
        {
            *(uint32_t*) out_result_ptr = buffer_ptr->start_offset;

            break;
        }

        case RAL_BUFFER_PROPERTY_USAGE_BITS:
        {
            *(ral_buffer_usage_bits*) out_result_ptr = buffer_ptr->usage_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_USER_QUEUE_BITS:
        {
            *(ral_queue_bits*) out_result_ptr = buffer_ptr->user_queue_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_SIZE:
        {
            *(uint32_t*) out_result_ptr = buffer_ptr->size;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_buffer_property value");
        }
    } /* switch (property) */
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_buffer_release(ral_buffer& buffer)
{
    delete (_ral_buffer*) buffer;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_buffer_set_data_from_client_memory(ral_buffer                             buffer,
                                                               uint32_t                               n_updates,
                                                               ral_buffer_client_sourced_update_info* updates)
{
    _ral_buffer*                                       buffer_ptr = (_ral_buffer*) buffer;
    ral_buffer_client_sourced_update_info_callback_arg callback_arg;
    bool                                               result     = false;

    /* Sanity checks */
    if (buffer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_buffer instance is NULL");

        goto end;
    }

    if (n_updates == 0)
    {
        result = true;

        goto end;
    }

    if (updates == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input update info array is NULL");

        goto end;
    }

    /* Convert input data to a callback argument and send a notification, so that active rendering contexts
     * can handle the request. */
    callback_arg.buffer    = buffer;
    callback_arg.n_updates = n_updates;
    callback_arg.updates   = updates;

    for (uint32_t n_update = 0;
                  n_update < n_updates;
                ++n_update)
    {
        updates[n_update].start_offset += buffer_ptr->start_offset;
    }

    system_callback_manager_call_back(buffer_ptr->callback_manager,
                                      RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,
                                     &callback_arg);

    /* All done */
    result = true;

end:
    return result;
}