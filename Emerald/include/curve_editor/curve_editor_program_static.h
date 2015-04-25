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


REFCOUNT_INSERT_DECLARATIONS(curve_editor_program_static,
                             curve_editor_program_static)


/** TODO */
PUBLIC curve_editor_program_static curve_editor_program_static_create(__in __notnull ogl_context               context,
                                                                      __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC GLuint curve_editor_program_static_get_id(__in __notnull curve_editor_program_static);

/** TODO */
PUBLIC GLint curve_editor_program_static_get_pos1_uniform_location(__in __notnull curve_editor_program_static);

/** TODO */
PUBLIC GLint curve_editor_program_static_get_pos2_uniform_location(__in __notnull curve_editor_program_static);

#endif /* CURVE_EDITOR_PROGRAM_STATIC_H */
