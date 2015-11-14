/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "system/system_log.h"


typedef struct _ral_buffer
{
    ral_buffer_mappability_bits mappability_bits;
    system_hashed_ansi_string   name;
    uint32_t                    size;
    ral_buffer_property_bits    property_bits;
    ral_buffer_usage_bits       usage_bits;
    ral_queue_bits              user_queue_bits;

    REFCOUNT_INSERT_VARIABLES;


    _ral_buffer(system_hashed_ansi_string   in_name,
                uint32_t                    in_size,
                ral_buffer_usage_bits       in_usage_bits,
                ral_queue_bits              in_user_queue_bits,
                ral_buffer_property_bits    in_property_bits,
                ral_buffer_mappability_bits in_mappability_bits)
    {
        mappability_bits = in_mappability_bits;
        name             = in_name;
        property_bits    = in_property_bits;
        size             = in_size;
        usage_bits       = in_usage_bits;
        user_queue_bits  = in_user_queue_bits;
    }

    ~_ral_buffer()
    {
        // ..
    }
} _ral_buffer;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ral_buffer,
                               ral_buffer,
                              _ral_buffer);


/** Please see header for specification */
PRIVATE void _ral_buffer_release(void* buffer)
{
    /* Stub */
}

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
                                                    create_info_ptr->size,
                                                    create_info_ptr->usage_bits,
                                                    create_info_ptr->user_queue_bits,
                                                    create_info_ptr->property_bits,
                                                    create_info_ptr->mappability_bits);

    ASSERT_ALWAYS_SYNC(new_buffer_ptr != NULL,
                       "Out of memory");

    if (new_buffer_ptr != NULL)
    {
        /* Register in the object manager */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_buffer_ptr,
                                                       _ral_buffer_release,
                                                       OBJECT_TYPE_RAL_BUFFER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\RAL Buffers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_buffer_ptr != NULL) */

end:
    return (ral_buffer) new_buffer_ptr;
}

/** Please see header for specification */
PUBLIC void ral_buffer_get_property(ral_buffer          buffer,
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
        case RAL_BUFFER_PROPERTY_MAPPABILITY_BITS:
        {
            *(ral_buffer_mappability_bits*) out_result_ptr = buffer_ptr->mappability_bits;

            break;
        }

        case RAL_BUFFER_PROPERTY_PROPERTY_BITS:
        {
            *(ral_buffer_property_bits*) out_result_ptr = buffer_ptr->property_bits;

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
