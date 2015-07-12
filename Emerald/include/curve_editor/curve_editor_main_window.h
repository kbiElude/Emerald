/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef CURVE_EDITOR_MAIN_WINDOW_H
#define CURVE_EDITOR_MAIN_WINDOW_H

#include "ogl/ogl_types.h"

typedef enum
{
    CURVE_EDITOR_MAIN_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH, /* float, settable. expressed in seconds. */
} curve_editor_main_window_property;

/** TODO */
PUBLIC curve_editor_main_window curve_editor_main_window_create(PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC,
                                                                ogl_context);

/** TODO */
PUBLIC void curve_editor_main_window_release(curve_editor_main_window);

/** TODO */
PUBLIC void curve_editor_main_window_set_property(curve_editor_main_window          window,
                                                  curve_editor_main_window_property property,
                                                  void*                             data);

/** TODO */
PUBLIC void curve_editor_main_window_update_curve_list(curve_editor_main_window window);

#endif /* CURVE_EDITOR_MAIN_WINDOW_H */
