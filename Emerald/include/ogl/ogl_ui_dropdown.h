/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_DROPDOWN_H
#define OGL_UI_DROPDOWN_H

/* Call-backs */
enum
{
    OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,   /* fired when droparea is toggled on or off            / user_arg: not used */
    OGL_UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE, /* fired when droparea visibility is toggled on or off / user_arg: not used */
};


/** TODO */
PUBLIC void ogl_ui_dropdown_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_dropdown_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_dropdown_get_property(const void*              dropdown,
                                         _ogl_ui_control_property property,
                                         void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_dropdown_init(ogl_ui                     instance,
                                  ogl_text                   text_renderer,
                                  system_hashed_ansi_string  label_text,
                                  uint32_t                   n_entries,
                                  system_hashed_ansi_string* entries,
                                  void**                     user_args,
                                  uint32_t                   n_selected_entry,
                                  system_hashed_ansi_string  name,
                                  const float*               x1y1,
                                  PFNOGLUIFIREPROCPTR        pfn_fire_proc_ptr,
                                  void*                      fire_proc_user_arg,
                                  ogl_ui_control             owner_control);

/** TODO */
PUBLIC bool ogl_ui_dropdown_is_over(      void*  internal_instance,
                                    const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_lbm_down(      void*  internal_instance,
                                        const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_lbm_up(      void*  internal_instance,
                                      const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_mouse_move(      void*  internal_instance,
                                          const float* xy);

/** TODO */
PUBLIC void ogl_ui_dropdown_on_mouse_wheel(void* internal_instance,
                                           float wheel_delta);

/** TODO */
PUBLIC void ogl_ui_dropdown_set_property(void*                    dropdown,
                                         _ogl_ui_control_property property,
                                         const void*              data);

#endif /* OGL_UI_DROPDOWN_H */
