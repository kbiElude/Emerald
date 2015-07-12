/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_CHECKBOX_H
#define OGL_UI_CHECKBOX_H


/** TODO */
PUBLIC void ogl_ui_checkbox_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_checkbox_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_checkbox_get_property(const void*              internal_instance,
                                         _ogl_ui_control_property property,
                                         void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_checkbox_init(ogl_ui                    instance,
                                  ogl_text                  text_renderer,
                                  system_hashed_ansi_string name,
                                  const float*              x1y1,
                                  PFNOGLUIFIREPROCPTR       pfn_fire_proc_ptr,
                                  void*                     fire_proc_user_arg,
                                  bool                      default_status);

/** TODO */
PUBLIC bool ogl_ui_checkbox_is_over(void*        internal_instance,
                                    const float* xy);

/** TODO */
PUBLIC void ogl_ui_checkbox_on_lbm_down(void*       internal_instance,
                                        const float* xy);

/** TODO */
PUBLIC void ogl_ui_checkbox_on_lbm_up(void*        internal_instance,
                                      const float* xy);

/** TODO */
PUBLIC void ogl_ui_checkbox_set_property(void*                    checkbox,
                                         _ogl_ui_control_property property,
                                         const void*              data);

#endif /* OGL_UI_CHECKBOX_H */
