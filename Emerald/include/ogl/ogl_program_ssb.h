/**
 *
 * Emerald (kbi/elude @2015)
 *
 * Module which does a couple of shader storage block housekeeping duties:
 *
 * a) Enumerates embedded variables and exposes additional info for the
 *    interested parties.
 * b) TODO: In future, we may be interested in including the sync behavior,
 *    already available for ogl_program_ub instances, in ogl_program_ssb.
 *
 * Underneath it re-routes the incoming cals to a more generic ogl_program_block.
 */
#ifndef OGL_PROGRAM_SSB_H
#define OGL_PROGRAM_SSB_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


typedef enum
{
    /* unsigned int */
    OGL_PROGRAM_SSB_PROPERTY_BLOCK_DATA_SIZE,

    /* GLuint */
    OGL_PROGRAM_SSB_PROPERTY_INDEX,

    /* GLuint */
    OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,

    /* system_hashed_ansi_string */
    OGL_PROGRAM_SSB_PROPERTY_NAME,

    /* unsigned int */
    OGL_PROGRAM_SSB_PROPERTY_N_MEMBERS,

} ogl_program_ssb_property;


/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_program_ssb ogl_program_ssb_create(ral_context               context,
                                                                     raGL_program              owner_program,
                                                                     unsigned int              ssb_index,
                                                                     system_hashed_ansi_string ssb_name);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_ssb_get_variable_by_index(const ogl_program_ssb        ssb,
                                                              unsigned int                 n_variable,
                                                              const ral_program_variable** out_variable_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ssb_get_property(const ogl_program_ssb    ssb,
                                                     ogl_program_ssb_property property,
                                                     void*                    out_result);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void ogl_program_ssb_release(ogl_program_ssb ssb);

/** TODO */
PUBLIC void ogl_program_ssb_set_property(const ogl_program_ssb    ssb,
                                         ogl_program_ssb_property property,
                                         const void*              data);

#endif /* OGL_PROGRAM_SSB_H */
