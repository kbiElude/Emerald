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
PUBLIC RENDERING_CONTEXT_CALL ogl_program_ssb ogl_program_ssb_create(__in __notnull ogl_context               context,
                                                                     __in __notnull ogl_program               owner_program,
                                                                     __in __notnull unsigned int              ssb_index,
                                                                     __in __notnull system_hashed_ansi_string ssb_name);

/** TODO */
PUBLIC EMERALD_API bool ogl_program_ssb_get_variable_by_index(__in  __notnull const ogl_program_ssb        ssb,
                                                              __in            unsigned int                 n_variable,
                                                              __out __notnull const ogl_program_variable** out_variable_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_program_ssb_get_property(__in  __notnull const ogl_program_ssb    ssb,
                                                     __in            ogl_program_ssb_property property,
                                                     __out __notnull void*                    out_result);

/** TODO
 *
 *  Internal use only.
 *
 */
PUBLIC void ogl_program_ssb_release(__in __notnull ogl_program_ssb ssb);

/** TODO */
PUBLIC void ogl_program_ssb_set_property(__in  __notnull const ogl_program_ssb    ssb,
                                         __in            ogl_program_ssb_property property,
                                         __out __notnull const void*              data);

#endif /* OGL_PROGRAM_SSB_H */
