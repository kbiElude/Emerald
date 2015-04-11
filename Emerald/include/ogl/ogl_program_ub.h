/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef OGL_PROGRAM_UB_H
#define OGL_PROGRAM_UB_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


typedef enum
{
    /* unsigned int */
    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,

    /* system_hashed_ansi_string */
    OGL_PROGRAM_UB_PROPERTY_NAME,

    /* unsigned int */
    OGL_PROGRAM_UB_PROPERTY_N_MEMBERS,

} ogl_program_ub_property;

/** TODO
 *
 *  Internal use only
 *
 *  @param owner_program TODO. NOT retained.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_program_ub ogl_program_ub_create(__in __notnull ogl_context               context,
                                                                   __in __notnull ogl_program               owner_program,
                                                                   __in __notnull unsigned int              ub_index,
                                                                   __in __notnull system_hashed_ansi_string ub_name);

/** TODO */
PUBLIC void ogl_program_ub_get_property(__in  __notnull const ogl_program_ub    ub,
                                        __in            ogl_program_ub_property property,
                                        __out __notnull void*                   out_result);

/** TODO */
PUBLIC void ogl_program_ub_release(__in __notnull ogl_program_ub ub);


#endif /* OGL_PROGRAM_UB_H */
