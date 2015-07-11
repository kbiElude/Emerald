/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_EDITOR_TYPES_H
#define CURVE_EDITOR_TYPES_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/* Curve editor */
DECLARE_HANDLE(curve_editor_curve_window);
DECLARE_HANDLE(curve_editor_curve_window_renderer);
DECLARE_HANDLE(curve_editor_main_window);
DECLARE_HANDLE(curve_editor_program_curvebackground);
DECLARE_HANDLE(curve_editor_program_lerp);
DECLARE_HANDLE(curve_editor_program_quadselector);
DECLARE_HANDLE(curve_editor_program_static);
DECLARE_HANDLE(curve_editor_program_tcb);
DECLARE_HANDLE(curve_editor_watchdog);

/* Internal types */
typedef struct
{

    curve_container                    curve;
    curve_editor_curve_window_renderer renderer;
    system_time                        start_time;
    system_time                        end_time;

} _curve_window_renderer_segment_creation_callback_argument;

/* Callback function pointer definitions */
typedef void (*PFNONMAINWINDOWRELEASECALLBACKHANDLERPROC )();
typedef void (*PFNONCURVEWINDOWRELEASECALLBACKHANDLERPROC)(void*, curve_container);

#endif /* CURVE_EDITOR_TYPES_H */
