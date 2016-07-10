#ifndef RAL_PROGRAM_BLOCK_BUFFER_H
#define RAL_PROGRAM_BLOCK_BUFFER_H

#include "system/system_types.h"
#include "ral/ral_types.h"

typedef enum
{
    /* not settable; ral_buffer */
    RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
} ral_program_block_buffer_property;

/** TODO */
PUBLIC EMERALD_API ral_program_block_buffer ral_program_block_buffer_create(ral_context               context,
                                                                            ral_program               program,
                                                                            system_hashed_ansi_string block_name);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_get_property(ral_program_block_buffer          block_buffer,
                                                              ral_program_block_buffer_property property,
                                                              void*                             out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_release(ral_program_block_buffer block_buffer);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_set_arrayed_variable_value(ral_program_block_buffer block_buffer,
                                                                            unsigned int             block_variable_offset,
                                                                            const void*              src_data,
                                                                            unsigned int             src_data_size,
                                                                            unsigned int             dst_array_start_index,
                                                                            unsigned int             dst_array_item_count);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_set_nonarrayed_variable_value(ral_program_block_buffer block_buffer,
                                                                               unsigned int             block_variable_offset,
                                                                               const void*              src_data,
                                                                               unsigned int             src_data_size);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_sync_immediately(ral_program_block_buffer block_buffer);

/** TODO */
PUBLIC EMERALD_API void ral_program_block_buffer_sync_via_command_buffer(ral_program_block_buffer block_buffer,
                                                                         ral_command_buffer       command_buffer);

#endif /* RAL_PROGRAM_BLOCK_BUFFER_H */