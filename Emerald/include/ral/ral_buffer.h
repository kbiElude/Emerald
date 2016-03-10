#ifndef RAL_BUFFER_H
#define RAL_BUFFER_H

#include "ral/ral_types.h"

typedef enum
{
    /* One or more buffer->buffer copy ops are pending for a ral_buffer instance.
     *
     * arg: ral_buffer_copy_to_buffer_callback_arg
     **/
    RAL_BUFFER_CALLBACK_ID_BUFFER_TO_BUFFER_COPY_REQUESTED,

    /* One or more region clears are pending for a ral_buffer instance.
     *
     * arg: ral_buffer_clear_region_callback_arg
     */
    RAL_BUFFER_CALLBACK_ID_CLEAR_REGION_REQUESTED,

    /* One or more updates are pending for a ral_buffer instance.
     *
     * arg: ral_buffer_client_sourced_update_info_callback_arg
     */
    RAL_BUFFER_CALLBACK_ID_CLIENT_MEMORY_SOURCED_UPDATES_REQUESTED,

    /* Always last */
    RAL_BUFFER_CALLBACK_ID_COUNT,
};

typedef struct ral_buffer_clear_region_callback_arg
{
    ral_buffer                          buffer;
    const ral_buffer_clear_region_info* clear_ops;
    uint32_t                            n_clear_ops;

    ral_buffer_clear_region_callback_arg()
    {
        buffer      = NULL;
        clear_ops   = NULL;
        n_clear_ops = 0;
    }
} ral_buffer_clear_region_callback_arg;

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

typedef struct ral_buffer_copy_to_buffer_callback_arg
{
    ral_buffer_copy_to_buffer_info* copy_ops;
    ral_buffer                      dst_buffer;
    uint32_t                        n_copy_ops;
    ral_buffer                      src_buffer;

    ral_buffer_copy_to_buffer_callback_arg()
    {
        copy_ops   = NULL;
        dst_buffer = NULL;
        n_copy_ops = 0;
        src_buffer = NULL;
    }
} ral_buffer_copy_to_buffer_callback_arg;

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
PUBLIC EMERALD_API bool ral_buffer_clear_region(ral_buffer                    buffer,
                                                uint32_t                      n_clear_ops,
                                                ral_buffer_clear_region_info* clear_ops);

/** TODO */
PUBLIC EMERALD_API bool ral_buffer_copy_to_buffer(ral_buffer                      src_buffer,
                                                  ral_buffer                      dst_buffer,
                                                  uint32_t                        n_copy_ops,
                                                  ral_buffer_copy_to_buffer_info* copy_ops);

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
PUBLIC EMERALD_API bool ral_buffer_set_data_from_client_memory(ral_buffer                             buffer,
                                                               uint32_t                               n_updates,
                                                               ral_buffer_client_sourced_update_info* updates);
#endif /* RAL_BUFFER_H */