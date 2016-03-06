/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_LABEL_H
#define UI_LABEL_H

#include "ogl/ogl_types.h"
#include "ui/ui_types.h"


/** TODO */
PUBLIC void ui_label_deinit(void* internal_instance);

/** TODO */
PUBLIC void ui_label_get_property(const void*         label,
                                  ui_control_property property,
                                  void*               out_result);

/** TODO */
PUBLIC void* ui_label_init(ui                        instance,
                           varia_text_renderer       text_renderer,
                           system_hashed_ansi_string name,
                           const float*              x1y1);

/** TODO */
PUBLIC void ui_label_set_property(void*               label,
                                  ui_control_property property,
                                  const void*         data);

#endif /* UI_LABEL_H */
