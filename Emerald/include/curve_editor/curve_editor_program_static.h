/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_STATIC_H
#define CURVE_EDITOR_PROGRAM_STATIC_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


typedef enum
{
    /* GLfloat, only settable */
    CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS1_DATA,

    /* GLfloat[16], only settable */
    CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS2_DATA,
} curve_editor_program_static_property;



REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_static,
                             curve_editor_program_static)


/** TODO */
PUBLIC curve_editor_program_static curve_editor_program_static_create(__in __notnull ogl_context               context,
                                                                      __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_static_set_property(__in __notnull curve_editor_program_static          instance,
                                                     __in           curve_editor_program_static_property property,
                                                     __in __notnull const void*                          data);

/** TODO */
PUBLIC void curve_editor_program_static_use(__in __notnull ogl_context                 context,
                                            __in __notnull curve_editor_program_static instance);

#endif /* CURVE_EDITOR_PROGRAM_STATIC_H */
