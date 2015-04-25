/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_TCB_H
#define CURVE_EDITOR_PROGRAM_TCB_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_tcb,
                             curve_editor_program_tcb)


/** TODO */
PUBLIC curve_editor_program_tcb curve_editor_program_tcb_create(__in __notnull ogl_context               context,
                                                                __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_nodes_uniform_buffer_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_delta_time_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_delta_x_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLuint curve_editor_program_tcb_get_id(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_node_indexes_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_should_round_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_start_time_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_start_x_uniform_location(__in __notnull curve_editor_program_tcb);

/** TODO */
PUBLIC GLint curve_editor_program_tcb_get_val_range_uniform_location(__in __notnull curve_editor_program_tcb);

#endif /* CURVE_EDITOR_PROGRAM_TCB_H */
