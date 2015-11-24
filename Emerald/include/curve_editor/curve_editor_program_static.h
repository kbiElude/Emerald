/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_STATIC_H
#define CURVE_EDITOR_PROGRAM_STATIC_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ral/ral_types.h"


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
PUBLIC curve_editor_program_static curve_editor_program_static_create(ral_context               context,
                                                                      system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_static_set_property(curve_editor_program_static          instance,
                                                     curve_editor_program_static_property property,
                                                     const void*                          data);

/** TODO */
PUBLIC void curve_editor_program_static_use(ral_context                 context,
                                            curve_editor_program_static instance);

#endif /* CURVE_EDITOR_PROGRAM_STATIC_H */
