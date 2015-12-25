#ifndef RAL_BUFFER_H
#define RAL_BUFFER_H

#include "ral/ral_types.h"

typedef enum
{
    /* One or more updates are pending for a ral_buffer instance.
     *
     * arg: ral_buffer_client_sourced_update_info_callback_arg
     */
    RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,

    /* Always last */
    RAL_BUFFER_CALLBACK_ID_COUNT,
};

typedef struct ral_buffer_client_sourced_update_info_callback_arg
{
    ral_buffer                                   buffer;
    uint32_t                                     n_updates;
    const ral_buffer_client_sourced_update_info* updates;

    ral_buffer_client_sourced_update_info_callback_arg()
    {
        buffer    = NULL;
        n_updates = 0;
        updates   = NULL;
    }
} ral_buffer_client_sourced_update_info_callback_arg;


typedef enum
{
    /* not settable, system_callback_manager */
    RAL_BUFFER_PROPERTY_CALLBACK_MANAGER,

    /* not settable, ral_buffer_mappability_bits */
    RAL_BUFFER_PROPERTY_MAPPABILITY_BITS,

    /* not settable, ral_buffer
     *
     * Parent buffer. This is used for cases when the ral_buffer instance describes a sub-region of another ral_buffer.
     */
    RAL_BUFFER_PROPERTY_PARENT_BUFFER,

    /* not settable, ral_buffer_property_bits */
    RAL_BUFFER_PROPERTY_PROPERTY_BITS,

    /* not settable, uint32_t */
    RAL_BUFFER_PROPERTY_SIZE,

    /* not settable, uint32_t. */
    RAL_BUFFER_PROPERTY_START_OFFSET,

    /* not settable, ral_buffer_usage_bits */
    RAL_BUFFER_PROPERTY_USAGE_BITS,

    /* not settable, ral_queue_bits */
    RAL_BUFFER_PROPERTY_USER_QUEUE_BITS,
} ral_buffer_property;


/** TODO */
PUBLIC ral_buffer ral_buffer_create(system_hashed_ansi_string     name,
                                    const ral_buffer_create_info* create_info_ptr);

#if 0
    PUBLIC ral_buffer ral_buffer_create_from_sparse(ral_buffer            parent_sparse_buffer,
                                                    uint32_t              size,
                                                    ral_buffer_usage_bits usage_bits,
                                                    ral_queue_bits        user_queue_bits);
#endif

/** TODO */
PUBLIC EMERALD_API void ral_buffer_get_property(ral_buffer          buffer,
                                                ral_buffer_property property,
                                                void*               out_result_ptr);

/** TODO */
PUBLIC void ral_buffer_release(ral_buffer& buffer);

/** TODO */
PUBLIC bool ral_buffer_set_data_from_client_memory(ral_buffer                                   buffer,
                                                   uint32_t                                     n_updates,
                                                   const ral_buffer_client_sourced_update_info* updates);
#endif /* RAL_BUFFER_H */