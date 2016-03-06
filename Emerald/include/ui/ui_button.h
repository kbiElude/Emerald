/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_BUTTON_H
#define UI_BUTTON_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"
#include "ui/ui_types.h"


/** TODO */
PUBLIC void ui_button_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ui_button_draw(void* internal_instance);

/** TODO */
PUBLIC void ui_button_get_property(const void*         button,
                                   ui_control_property property,
                                   void*               out_result);

/** TODO */
PUBLIC void* ui_button_init(ui                        instance,
                            varia_text_renderer       text_renderer,
                            system_hashed_ansi_string name,
                            const float*              x1y1,
                            const float*              x2y2,
                            PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                            void*                     fire_proc_user_arg);

/** TODO */
PUBLIC bool ui_button_is_over(void*        internal_instance,
                              const float* xy);

/** TODO */
PUBLIC void ui_button_on_lbm_down(void*        internal_instance,
                                  const float* xy);

/** TODO */
PUBLIC void ui_button_on_lbm_up(void*        internal_instance,
                                const float* xy);

/** TODO */
PUBLIC void ui_button_set_property(void*               button,
                                   ui_control_property property,
                                   const void*         data);

#endif /* UI_BUTTON_H */
