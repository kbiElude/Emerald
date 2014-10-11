/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_LERP_H
#define CURVE_EDITOR_PROGRAM_LERP_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_lerp, curve_editor_program_lerp)


/** TODO */
PUBLIC curve_editor_program_lerp curve_editor_program_lerp_create(__in __notnull ogl_context context, __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC GLuint curve_editor_program_lerp_get_id(__in __notnull curve_editor_program_lerp);

/** TODO */
PUBLIC GLint curve_editor_program_lerp_get_pos1_uniform_location(__in __notnull curve_editor_program_lerp);

/** TODO */
PUBLIC GLint curve_editor_program_lerp_get_pos2_uniform_location(__in __notnull curve_editor_program_lerp);

#endif /* CURVE_EDITOR_PROGRAM_LERP_H */
