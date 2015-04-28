/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_LERP_H
#define CURVE_EDITOR_PROGRAM_LERP_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


typedef enum
{
    /* float[4], only settable */
    CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS1,

    /* float[4], only settable */
    CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS2,
} curve_editor_program_lerp_property;


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_lerp,
                             curve_editor_program_lerp)


/** TODO */
PUBLIC curve_editor_program_lerp curve_editor_program_lerp_create(__in __notnull ogl_context               context,
                                                                  __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_lerp_set_property(__in __notnull curve_editor_program_lerp          lerp,
                                                   __in           curve_editor_program_lerp_property property,
                                                   __in __notnull const void*                        data);

/** TODO */
PUBLIC void curve_editor_program_lerp_use(__in __notnull ogl_context               context,
                                          __in __notnull curve_editor_program_lerp lerp);

#endif /* CURVE_EDITOR_PROGRAM_LERP_H */
