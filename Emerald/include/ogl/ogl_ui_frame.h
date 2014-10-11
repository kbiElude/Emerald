/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_FRAME_H
#define OGL_UI_FRAME_H

typedef enum
{
    OGL_UI_FRAME_PROPERTY_X1Y1X2Y2 /* settable, float[4] */
} _ogl_ui_frame_property;

/** TODO */
PUBLIC void ogl_ui_frame_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_frame_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_frame_get_property(__in  __notnull const void* frame,
                                      __in  __notnull int         property_value,
                                      __out __notnull void*       out_result);

/** TODO */
PUBLIC void* ogl_ui_frame_init(__in           __notnull ogl_ui       instance,
                               __in_ecount(4) __notnull const float* x1y1x2y2);

/** TODO */
PUBLIC void ogl_ui_frame_set_property(__in  __notnull void*       frame,
                                      __in  __notnull int         property_value,
                                      __out __notnull const void* data);

#endif /* OGL_UI_FRAME_H */
