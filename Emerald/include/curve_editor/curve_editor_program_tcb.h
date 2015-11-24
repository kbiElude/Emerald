/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Nodes buffer UB is exposed under UB BP 0.
 * It is caller's responsibility to bind it & set its contents.
 */
#ifndef CURVE_EDITOR_PROGRAM_TCB_H
#define CURVE_EDITOR_PROGRAM_TCB_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"


typedef enum
{
    /* GLfloat, only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_TIME_DATA,

    /* GLfloat, only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_X_DATA,

    /* GLint[4], only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_NODE_INDEXES_DATA,

    /* GLboolean, only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_SHOULD_ROUND_DATA,

    /* GLfloat, only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_TIME_DATA,

    /* GLfloat, only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_X_DATA,

    /* GLfloat[2], only settable */
    CURVE_EDITOR_PROGRAM_TCB_PROPERTY_VAL_RANGE_DATA,

} curve_editor_program_tcb_property;


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_tcb,
                             curve_editor_program_tcb)


/** TODO */
PUBLIC curve_editor_program_tcb curve_editor_program_tcb_create(ral_context               context,
                                                                system_hashed_ansi_string name);

/** TODO */
PUBLIC void curve_editor_program_tcb_set_property(curve_editor_program_tcb          tcb,
                                                  curve_editor_program_tcb_property property,
                                                  const void*                       data);

/** TODO */
PUBLIC void curve_editor_program_tcb_use(ral_context              context,
                                         curve_editor_program_tcb tcb);

#endif /* CURVE_EDITOR_PROGRAM_TCB_H */
