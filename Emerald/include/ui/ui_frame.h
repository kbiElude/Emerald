/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_FRAME_H
#define UI_FRAME_H

#include "ui/ui_types.h"


/** TODO */
PUBLIC void ui_frame_deinit(void* internal_instance);

/** TODO */
PUBLIC ral_present_task ui_frame_get_present_task(void*            internal_instance,
                                                  ral_texture_view target_texture_view);

/** TODO */
PUBLIC void ui_frame_get_property(const void*         frame,
                                  ui_control_property property,
                                  void*               out_result);

/** TODO */
PUBLIC void* ui_frame_init(ui           instance,
                           const float* x1y1x2y2);

/** TODO */
PUBLIC void ui_frame_set_property(void*               frame,
                                  ui_control_property property,
                                  const void*         data);

#endif /* UI_FRAME_H */
