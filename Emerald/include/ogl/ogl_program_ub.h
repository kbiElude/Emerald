/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Module which does a couple of uniform block housekeeping duties:
 *
 * a) Enumerates embedded uniforms and exposes additional info for the
 *    interested parties.
 *
 * b) If requested, allocates buffer memory space and keeps a cache
 *    of values assigned to the embedded uniforms. Upon a sync request,
 *    ogl_program_ub uses available ES/GL functionality to update the changed
 *    regions in an optimal (coalesced) manner.
 *
 * Underneath it re-routes the incoming cals to a more generic ogl_program_block.
 */
#ifndef OGL_PROGRAM_UB_H
#define OGL_PROGRAM_UB_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


typedef enum
{
    /* unsigned int */
    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,

    /* ral_buffer */
    OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,

    /* GLuint */
    OGL_PROGRAM_UB_PROPERTY_INDEX,

    /* GLuint */
    OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,

    /* system_hashed_ansi_string */
    OGL_PROGRAM_UB_PROPERTY_NAME,

    /* unsigned int */
    OGL_PROGRAM_UB_PROPERTY_N_MEMBERS,

} ogl_program_ub_property;

/* Flags used for ogl_program_ub_set_nonarrayed_uniform_value() calls. */
const unsigned int UB_SRC_DATA_FLAG_TRANSPOSED_MATRIX_DATA = 1 << 0;

/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_program_ub ogl_program_ub_create(ogl_context               context,
                                                                   ogl_program               owner_program,
                                                                   unsigned int              ub_index,
                                                                   system_hashed_ansi_string ub_name,
                                                                   bool                      support_sync_behavior);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ub_get_property(const ogl_program_ub    ub,
                                                    ogl_program_ub_property property,
                                                    void*                   out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ub_get_variable_by_name(ogl_program_ub               ub,
                                                            system_hashed_ansi_string    name,
                                                            const ogl_program_variable** out_variable_ptr);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void ogl_program_ub_release(ogl_program_ub ub);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ub_set_arrayed_uniform_value(ogl_program_ub ub,
                                                                 GLuint         ub_uniform_offset,
                                                                 const void*    src_data,
                                                                 int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                 unsigned int   src_data_size,
                                                                 unsigned int   dst_array_start_index,
                                                                 unsigned int   dst_array_item_count);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ub_set_nonarrayed_uniform_value(ogl_program_ub ub,
                                                                    GLuint         ub_uniform_offset,
                                                                    const void*    src_data,
                                                                    int            src_data_flags, /* UB_SRC_DATA_FLAG_* */
                                                                    unsigned int   src_data_size);

/** TODO */
PUBLIC void ogl_program_ub_set_property(const ogl_program_ub    ub,
                                        ogl_program_ub_property property,
                                        const void*             data);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL void ogl_program_ub_sync(ogl_program_ub ub);

#endif /* OGL_PROGRAM_UB_H */
