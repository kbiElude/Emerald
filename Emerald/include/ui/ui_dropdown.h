/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_DROPDOWN_H
#define UI_DROPDOWN_H

#include "ogl/ogl_types.h"
#include "ui/ui_types.h"

/* Call-backs */
enum
{
    UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,   /* fired when droparea is toggled on or off            / user_arg: not used */
    UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE, /* fired when droparea visibility is toggled on or off / user_arg: not used */
};


/** TODO */
PUBLIC void ui_dropdown_deinit(void* internal_instance);

/** TODO */
PUBLIC ral_present_task ui_dropdown_get_present_task(void*            internal_instance,
                                                     ral_texture_view target_texture_view);


/** TODO */
PUBLIC void ui_dropdown_get_property(const void*         dropdown,
                                     ui_control_property property,
                                     void*               out_result);

/** TODO */
PUBLIC void* ui_dropdown_init(ui                         instance,
                              varia_text_renderer        text_renderer,
                              system_hashed_ansi_string  label_text,
                              uint32_t                   n_entries,
                              system_hashed_ansi_string* entries,
                              void**                     user_args,
                              uint32_t                   n_selected_entry,
                              system_hashed_ansi_string  name,
                              const float*               x1y1,
                              PFNUIFIREPROCPTR           pfn_fire_proc_ptr,
                              void*                      fire_proc_user_arg,
                              ui_control                 owner_control);

/** TODO */
PUBLIC bool ui_dropdown_is_over(      void*  internal_instance,
                                const float* xy);

/** TODO */
PUBLIC void ui_dropdown_on_lbm_down(      void*  internal_instance,
                                    const float* xy);

/** TODO */
PUBLIC void ui_dropdown_on_lbm_up(      void*  internal_instance,
                                  const float* xy);

/** TODO */
PUBLIC void ui_dropdown_on_mouse_move(      void*  internal_instance,
                                      const float* xy);

/** TODO */
PUBLIC void ui_dropdown_on_mouse_wheel(void* internal_instance,
                                       float wheel_delta);

/** TODO */
PUBLIC void ui_dropdown_set_property(void*               dropdown,
                                     ui_control_property property,
                                     const void*         data);

#endif /* UI_DROPDOWN_H */
