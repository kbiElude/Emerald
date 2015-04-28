/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_QUADSELECTOR_H
#define CURVE_EDITOR_PROGRAM_QUADSELECTOR_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


typedef enum
{
    /* GLfloat */
    CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_ALPHA_DATA,

    /* GLfloat[16] */
    CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_POSITIONS_DATA,
} curve_editor_program_quadselector_property;



REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_quadselector,
                             curve_editor_program_quadselector)


/** TODO */
PUBLIC curve_editor_program_quadselector curve_editor_program_quadselector_create(__in __notnull ogl_context               context,
                                                                                  __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_quadselector_set_property(__in __notnull curve_editor_program_quadselector          program,
                                                           __in           curve_editor_program_quadselector_property property,
                                                           __in __notnull const void*                                data);

/** TODO */
PUBLIC void curve_editor_program_quadselector_use(__in __notnull ogl_context                       context,
                                                  __in __notnull curve_editor_program_quadselector quadselector);

#endif /* CURVE_EDITOR_PROGRAM_QUADSELECTOR_H */
