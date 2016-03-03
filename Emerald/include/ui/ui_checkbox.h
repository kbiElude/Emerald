/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_CHECKBOX_H
#define UI_CHECKBOX_H

#include "ogl/ogl_types.h"
#include "ui/ui_types.h"


/** TODO */
PUBLIC void ui_checkbox_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ui_checkbox_draw(void* internal_instance);

/** TODO */
PUBLIC void ui_checkbox_get_property(const void*         internal_instance,
                                     ui_control_property property,
                                     void*               out_result);

/** TODO */
PUBLIC void* ui_checkbox_init(ui                        instance,
                              ogl_text                  text_renderer,
                              system_hashed_ansi_string name,
                              const float*              x1y1,
                              PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                              void*                     fire_proc_user_arg,
                              bool                      default_status);

/** TODO */
PUBLIC bool ui_checkbox_is_over(void*        internal_instance,
                                const float* xy);

/** TODO */
PUBLIC void ui_checkbox_on_lbm_down(void*       internal_instance,
                                    const float* xy);

/** TODO */
PUBLIC void ui_checkbox_on_lbm_up(void*        internal_instance,
                                  const float* xy);

/** TODO */
PUBLIC void ui_checkbox_set_property(void*               checkbox,
                                     ui_control_property property,
                                     const void*         data);

#endif /* UI_CHECKBOX_H */
