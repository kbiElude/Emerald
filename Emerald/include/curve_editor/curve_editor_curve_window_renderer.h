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
PUBLIC curve_editor_curve_window_renderer curve_editor_curve_window_renderer_create(system_hashed_ansi_string,
                                                                                    HWND,
                                                                                    ogl_context,
                                                                                    curve_container,
                                                                                    curve_editor_curve_window);

/** TODO */
PUBLIC system_window curve_editor_curve_window_renderer_get_window(curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_redraw(curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_release(curve_editor_curve_window_renderer);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_resize(curve_editor_curve_window_renderer,
                                                      int*                               x1y1x2y2);

/** TODO */
PUBLIC void curve_editor_curve_window_renderer_set_property(curve_editor_curve_window_renderer          renderer,
                                                            curve_editor_curve_window_renderer_property property,
                                                            void*                                       data);

#endif /* CURVE_EDITOR_CURVE_WINDOW_RENDERER_H */
