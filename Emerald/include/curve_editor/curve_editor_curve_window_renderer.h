/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_EDITOR_CURVE_WINDOW_RENDERER_H
#define CURVE_EDITOR_CURVE_WINDOW_RENDERER_H

#include "curve_editor/curve_editor_types.h"
#include "ogl/ogl_types.h"

typedef enum
{
    CURVE_EDITOR_CURVE_WINDOW_RENDERER_PROPERTY_MAX_VISIBLE_TIMELINE_HEIGHT, /* float.                 settable. */
    CURVE_EDITOR_CURVE_WINDOW_RENDERER_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH   /* float, expressed in s. settable. */
} curve_editor_curve_window_renderer_property;

/** TODO */
PUBLIC curve_editor_curve_window_renderer curve_editor_curve_window_renderer_create(__in __notnull system_hashed_ansi_string,
                                                                                    __in __notnull HWND,
                                                                                    __in __notnull ogl_context,
                                                                                    __in           curve_container,
                                                                                    __in __notnull curve_editor_curve_window);

/** TODO */
PUBLIC system_window curve_editor_curve_window_renderer_get_window(__in __notnull curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_redraw(__in __notnull curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_release(__in __post_invalid curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_resize(__in __notnull   curve_editor_curve_window_renderer,
                                                      __in __ecount(4) int*                               x1y1x2y2);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_set_property(__in __notnull curve_editor_curve_window_renderer          renderer,
                                                            __in           curve_editor_curve_window_renderer_property property,
                                                            __in __notnull void*                                       data);

#endif /* CURVE_EDITOR_CURVE_WINDOW_RENDERER_H */
