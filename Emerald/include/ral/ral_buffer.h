#ifndef RAL_BUFFER_H
#define RAL_BUFFER_H

#include "ral/ral_types.h"

typedef enum
{
    /* not settable, uint32_t */
    RAL_BUFFER_PROPERTY_SIZE,

    /* not settable, ral_buffer_usage_bits */
    RAL_BUFFER_PROPERTY_USAGE_BITS,

    /* not settable, ral_queue_bits */
    RAL_BUFFER_PROPERTY_USER_QUEUE_BITS,
} ral_buffer_property;

REFCOUNT_INSERT_DECLARATIONS(ral_buffer,
                             ral_buffer);


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
PUBLIC void ral_buffer_get_property(ral_buffer          buffer,
                                    ral_buffer_property property,
                                    void*               out_result_ptr);

#endif /* RAL_BUFFER_H */