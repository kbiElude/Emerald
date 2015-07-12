/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_LABEL_H
#define OGL_UI_LABEL_H


/** TODO */
PUBLIC void ogl_ui_label_deinit(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_label_get_property(const void*              label,
                                      _ogl_ui_control_property property,
                                      void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_label_init(ogl_ui                    instance,
                               ogl_text                  text_renderer,
                               system_hashed_ansi_string name,
                               const float*              x1y1);

/** TODO */
PUBLIC void ogl_ui_label_set_property(void*                    label,
                                      _ogl_ui_control_property property,
                                      const void*              data);

#endif /* OGL_UI_LABEL_H */
