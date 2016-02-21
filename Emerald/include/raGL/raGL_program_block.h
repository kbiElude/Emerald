/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Module which provides a general abstraction layer for shader block housekeeping duties:
 *
 * a) Enumerates embedded variables and exposes additional info for the
 *    interested parties.
 *
 * b) If requested, allocates buffer memory space and keeps a cache
 *    of values assigned to the embedded variables. Upon a sync request,
 *    ogl_program_block uses available ES/GL functionality to update the changed
 *    regions in an optimal (coalesced) manner.
 *
 *  The module currently supports the following block types:
 *
 *  - Shader storage buffers (upcoming)
 *  - Uniform buffers
 *
 * INTERNAL USAGE ONLY.
 */
#ifndef RAGL_PROGRAM_BLOCK_H
#define RAGL_PROGRAM_BLOCK_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


typedef enum
{
    /* unsigned int. */
    RAGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE,

    /* ral_buffer */
    RAGL_PROGRAM_BLOCK_PROPERTY_BUFFER_RAL,

    /* GLuint */
    RAGL_PROGRAM_BLOCK_PROPERTY_INDEX,

    /* GLuint */
    RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,

    /* system_hashed_ansi_string */
    RAGL_PROGRAM_BLOCK_PROPERTY_NAME,

    /* unsigned int */
    RAGL_PROGRAM_BLOCK_PROPERTY_N_MEMBERS,

} raGL_program_block_property;

/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL raGL_program_block raGL_program_block_create(ral_context               context,
                                                                          raGL_program              owner_program,
                                                                          ral_program_block_type    block_type,
                                                                          unsigned int              block_index,
                                                                          system_hashed_ansi_string block_name,
                                                                          bool                      support_sync_behavior);

/** TODO */
PUBLIC bool raGL_program_block_get_block_variable(raGL_program_block             block,
                                                  unsigned int                   index,
                                                  const _raGL_program_variable** out_variable_ptr);

/** TODO */
PUBLIC bool raGL_program_block_get_block_variable_by_name(raGL_program_block             block,
                                                          system_hashed_ansi_string      name,
                                                          const _raGL_program_variable** out_variable_ptr);

/** TODO */
PUBLIC EMERALD_API void raGL_program_block_get_property(const raGL_program_block     block,
                                                        raGL_program_block_property property,
                                                        void*                       out_result);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void raGL_program_block_release(raGL_program_block block);

/** TODO */
PUBLIC EMERALD_API void raGL_program_block_set_arrayed_variable_value(raGL_program_block block,
                                                                      GLuint             block_variable_offset,
                                                                      const void*        src_data,
                                                                      unsigned int       src_data_size,
                                                                      unsigned int       dst_array_start_index,
                                                                      unsigned int       dst_array_item_count);

/** TODO */
PUBLIC EMERALD_API void raGL_program_block_set_nonarrayed_variable_value(raGL_program_block block,
                                                                         GLuint             block_variable_offset,
                                                                         const void*        src_data,
                                                                         unsigned int       src_data_size);

/** TODO */
PUBLIC void raGL_program_block_set_property(const raGL_program_block    block,
                                            raGL_program_block_property property,
                                            const void*                 data);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void raGL_program_block_sync(raGL_program_block block);

#endif /* OGL_PROGRAM_BLOCK_H */
