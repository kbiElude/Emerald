/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_BUTTON_H
#define OGL_UI_BUTTON_H


/** TODO */
PUBLIC void ogl_ui_button_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_button_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_button_get_property(__in  __notnull const void*              button,
                                       __in            _ogl_ui_control_property property,
                                       __out __notnull void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_button_init(__in           __notnull   ogl_ui                    instance,
                                __in           __notnull   ogl_text                  text_renderer,
                                __in           __notnull   system_hashed_ansi_string name,
                                __in_ecount(2) __notnull   const float*              x1y1,
                                __in_ecount(2) __notnull   const float*              x2y2,
                                __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_proc_ptr,
                                __in           __maybenull void*                     fire_proc_user_arg);

/** TODO */
PUBLIC bool ogl_ui_button_is_over(void* internal_instance, const float* xy);

/** TODO */
PUBLIC void ogl_ui_button_on_lbm_down(void* internal_instance, const float* xy);

/** TODO */
PUBLIC void ogl_ui_button_on_lbm_up(void* internal_instance, const float* xy);

/** TODO */
PUBLIC void ogl_ui_button_set_property(__in __notnull void*                    button,
                                       __in __notnull _ogl_ui_control_property property,
                                       __in __notnull const void*              data);

#endif /* OGL_UI_BUTTON_H */
