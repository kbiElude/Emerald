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
#ifndef OGL_PROGRAM_BLOCK_H
#define OGL_PROGRAM_BLOCK_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


typedef enum
{
    /* unsigned int.
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_BLOCK_DATA_SIZE,

    /* GLuint
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_BO_ID,

    /* GLuint
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_BO_START_OFFSET,

    /* GLuint
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_INDEX,

    /* GLuint
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,

    /* system_hashed_ansi_string
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_NAME,

    /* unsigned int
     *
     * Supported for: UBs.
     */
    OGL_PROGRAM_BLOCK_PROPERTY_N_MEMBERS,

} ogl_program_block_property;

typedef enum
{
    /* Shader storage buffer block */
    OGL_PROGRAM_BLOCK_TYPE_SHADER_STORAGE_BUFFER,

    /* Uniform buffer block */
    OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER

} ogl_program_block_type;


/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_program_block ogl_program_block_create(ogl_context               context,
                                                                         ogl_program               owner_program,
                                                                         ogl_program_block_type    block_type,
                                                                         unsigned int              block_index,
                                                                         system_hashed_ansi_string block_name,
                                                                         bool                      support_sync_behavior);

/** TODO */
PUBLIC bool ogl_program_block_get_block_variable(ogl_program_block            block,
                                                 unsigned int                 index,
                                                 const ogl_program_variable** out_variable_ptr);

/** TODO */
PUBLIC void ogl_program_block_get_property(const ogl_program_block    block,
                                           ogl_program_block_property property,
                                           void*                      out_result);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void ogl_program_block_release(ogl_program_block block);

/** TODO */
PUBLIC void ogl_program_block_set_arrayed_variable_value(ogl_program_block block,
                                                         GLuint            block_variable_offset,
                                                         const void*       src_data,
                                                         unsigned int      src_data_size,
                                                         unsigned int      dst_array_start_index,
                                                         unsigned int      dst_array_item_count);

/** TODO */
PUBLIC void ogl_program_block_set_nonarrayed_variable_value(ogl_program_block block,
                                                            GLuint            block_variable_offset,
                                                            const void*       src_data,
                                                            unsigned int      src_data_size);

/** TODO */
PUBLIC void ogl_program_block_set_property(const ogl_program_block    block,
                                           ogl_program_block_property property,
                                           const void*                data);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_program_block_sync(ogl_program_block block);

#endif /* OGL_PROGRAM_BLOCK_H */
