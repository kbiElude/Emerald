/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_H
#define CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


typedef enum
{
    /* GLfloat[4], only settable */
    CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_COLORS_DATA,

    /* GLfloat[20], only settable */
    CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_POSITIONS_DATA,
} curve_editor_program_curvebackground_property;


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_curvebackground,
                             curve_editor_program_curvebackground)


/** TODO */
PUBLIC curve_editor_program_curvebackground curve_editor_program_curvebackground_create(__in __notnull ogl_context               context,
                                                                                        __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_curvebackground_set_property(__in __notnull curve_editor_program_curvebackground          program,
                                                              __in           curve_editor_program_curvebackground_property property,
                                                              __in __notnull const void*                                   data);

/** TODO */
PUBLIC void curve_editor_program_curvebackground_use(__in __notnull ogl_context                          context,
                                                     __in __notnull curve_editor_program_curvebackground curvebackground);

#endif /* CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_H */
