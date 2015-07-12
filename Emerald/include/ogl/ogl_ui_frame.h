/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_FRAME_H
#define OGL_UI_FRAME_H


/** TODO */
PUBLIC void ogl_ui_frame_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_frame_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_frame_get_property(const void*              frame,
                                      _ogl_ui_control_property property,
                                      void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_frame_init(ogl_ui       instance,
                               const float* x1y1x2y2);

/** TODO */
PUBLIC void ogl_ui_frame_set_property(void*                    frame,
                                      _ogl_ui_control_property property,
                                      const void*              data);

#endif /* OGL_UI_FRAME_H */
