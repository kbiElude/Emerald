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
    OGL_PROGRAM_BLOCK_PROPERTY_INDEXED_UB_BP,

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
    /* Uniform buffer block */
    OGL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER

} ogl_program_block_type;


/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_program_block ogl_program_block_create(__in __notnull ogl_context               context,
                                                                         __in __notnull ogl_program               owner_program,
                                                                         __in           ogl_program_block_type    block_type,
                                                                         __in __notnull unsigned int              block_index,
                                                                         __in __notnull system_hashed_ansi_string block_name,
                                                                         __in           bool                      support_sync_behavior);

/** TODO */
PUBLIC void ogl_program_block_get_property(__in  __notnull const ogl_program_block    block,
                                           __in            ogl_program_block_property property,
                                           __out __notnull void*                      out_result);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void ogl_program_block_release(__in __notnull ogl_program_block block);

/** TODO */
PUBLIC void ogl_program_block_set_arrayed_variable_value(__in                       __notnull ogl_program_block block,
                                                         __in                                 GLuint            block_variable_offset,
                                                         __in_ecount(src_data_size) __notnull const void*       src_data,
                                                         __in                                 unsigned int      src_data_size,
                                                         __in                                 unsigned int      dst_array_start_index,
                                                         __in                                 unsigned int      dst_array_item_count);

/** TODO */
PUBLIC void ogl_program_block_set_nonarrayed_variable_value(__in                       __notnull ogl_program_block block,
                                                            __in                                 GLuint            block_variable_offset,
                                                            __in_ecount(src_data_size) __notnull const void*       src_data,
                                                            __in                                 unsigned int      src_data_size);

/** TODO */
PUBLIC void ogl_program_block_set_property(__in  __notnull const ogl_program_block    block,
                                           __in            ogl_program_block_property property,
                                           __out __notnull const void*                data);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_program_block_sync(__in __notnull ogl_program_block block);

#endif /* OGL_PROGRAM_BLOCK_H */
