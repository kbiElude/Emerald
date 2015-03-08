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
PUBLIC void ogl_ui_label_get_property(__in  __notnull const void*              label,
                                      __in            _ogl_ui_control_property property,
                                      __out __notnull void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_label_init(__in           __notnull   ogl_ui                    instance,
                               __in           __notnull   ogl_text                  text_renderer,
                               __in           __notnull   system_hashed_ansi_string name,
                               __in_ecount(2) __notnull   const float*              x1y1);

/** TODO */
PUBLIC void ogl_ui_label_set_property(__in __notnull void*                    label,
                                      __in __notnull _ogl_ui_control_property property,
                                      __in __notnull const void*              data);

#endif /* OGL_UI_LABEL_H */
