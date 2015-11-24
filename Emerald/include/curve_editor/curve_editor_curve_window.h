/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef CURVE_EDITOR_CURVE_WINDOW_H
#define CURVE_EDITOR_CURVE_WINDOW_H

#include "curve/curve_types.h"
#include "curve_editor/curve_editor_types.h"
#include "ral/ral_types.h"

typedef enum
{
    CURVE_EDITOR_CURVE_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_HEIGHT, /* float, settable */
    CURVE_EDITOR_CURVE_WINDOW_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH,  /* float, settable. expressed in seconds. */
} curve_editor_curve_window_property;

/** TODO */
PUBLIC EMERALD_API void curve_editor_curve_window_hide(curve_editor_curve_window);

/** TODO */
PUBLIC EMERALD_API void curve_editor_curve_window_redraw(curve_editor_curve_window);

/** TODO */
PUBLIC EMERALD_API curve_editor_curve_window curve_editor_curve_window_show(ral_context,
                                                                            curve_container,
                                                                            PFNONCURVEWINDOWRELEASECALLBACKHANDLERPROC,
                                                                            void*,
                                                                            system_critical_section);

/** TODO. Called back with node id by the renderer.
 *
 *  If curve_segment_id or curve_segment_node_id is <= 0, the identifiers are considered to be invalid and
 *  action static and editbox components are hidden
 **/
PUBLIC void curve_editor_select_node(curve_editor_curve_window,
                                     curve_segment_id,
                                     curve_segment_node_id);

/** TODO */
PUBLIC void curve_editor_curve_window_set_property(curve_editor_curve_window          window,
                                                   curve_editor_curve_window_property property,
                                                   void*                              data);

/** TODO. Called back with 2 floats <start, end x> */
PUBLIC void curve_editor_curve_window_create_static_segment_handler(void*);

/** TODO. Called back with 2 floats <start, end x>*/
PUBLIC void curve_editor_curve_window_create_lerp_segment_handler(void*);

/** TODO. Called back with 2 floats <start, end x> */
PUBLIC void curve_editor_curve_window_create_tcb_segment_handler(void*);

#endif /* CURVE_EDITOR_CURVE_WINDOW_H */
