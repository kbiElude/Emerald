/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_SCROLLBAR_H
#define UI_SCROLLBAR_H

#include "ogl/ogl_types.h"
#include "ui/ui_types.h"


/* Call-backs */
enum
{
    /* fired when scrollbar visibility is toggled on or off / user_arg: not used */
    UI_SCROLLBAR_CALLBACK_ID_VISIBILITY_TOGGLE,
};

/** TODO */
PUBLIC void ui_scrollbar_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ui_scrollbar_draw(void* internal_instance);

/** TODO */
PUBLIC void ui_scrollbar_get_property(const void*         scrollbar,
                                      ui_control_property property,
                                      void*               out_result);

/** TODO */
PUBLIC void ui_scrollbar_hover(void*        internal_instance,
                               const float* xy_screen_norm);

/** TODO */
PUBLIC void* ui_scrollbar_init(ui                          instance,
                               varia_text_renderer         text_renderer,
                               ui_scrollbar_text_location  text_location,
                               system_hashed_ansi_string   name,
                               system_variant              min_value,
                               system_variant              max_value,
                               const float*                x1y1,
                               const float*                x2y2,
                               PFNUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                               void*                       get_current_value_ptr_user_arg,
                               PFNUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                               void*                       set_current_value_ptr_user_arg);

/** TODO */
PUBLIC bool ui_scrollbar_is_over(void*        internal_instance,
                                 const float* xy);

/** TODO */
PUBLIC void ui_scrollbar_on_lbm_down(void*        internal_instance,
                                     const float* xy);

/** TODO */
PUBLIC void ui_scrollbar_on_lbm_up(void*        internal_instance,
                                   const float* xy);

/** TODO */
PUBLIC void ui_scrollbar_on_mouse_move(void*        internal_instance,
                                       const float* xy);

/** TODO */
PUBLIC void ui_scrollbar_set_property(void*               scrollbar,
                                      ui_control_property property,
                                      const void*         data);

#endif /* UI_SCROLLBAR_H */
