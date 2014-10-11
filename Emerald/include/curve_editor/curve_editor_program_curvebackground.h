/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_H
#define CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_curvebackground, curve_editor_program_curvebackground)


/** TODO */
PUBLIC curve_editor_program_curvebackground curve_editor_program_curvebackground_create(__in __notnull ogl_context context, __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC GLint curve_editor_program_curvebackground_get_colors_uniform_location(__in __notnull curve_editor_program_curvebackground);

/** TODO */
PUBLIC GLuint curve_editor_program_curvebackground_get_id(__in __notnull curve_editor_program_curvebackground);

/** TODO */
PUBLIC GLint curve_editor_program_curvebackground_get_positions_uniform_location(__in __notnull curve_editor_program_curvebackground);

#endif /* CURVE_EDITOR_PROGRAM_QUADSELECTOR_H */
