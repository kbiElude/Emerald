/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_EDITOR_MAIN_WINDOW_H
#define CURVE_EDITOR_MAIN_WINDOW_H

#include "ogl/ogl_types.h"

/** TODO */
PUBLIC curve_editor_main_window curve_editor_main_window_create(PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC, __in __notnull ogl_context);

/** TODO */
PUBLIC void curve_editor_main_window_release(__in __notnull __post_invalid curve_editor_main_window);

#endif /* CURVE_EDITOR_MAIN_WINDOW_H */
