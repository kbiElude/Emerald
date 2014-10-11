/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_LABEL_H
#define OGL_UI_LABEL_H

typedef enum
{
    OGL_UI_LABEL_PROPERTY_TEXT,           /* settable,     system_hashed_ansi_string */
    OGL_UI_LABEL_PROPERTY_TEXT_HEIGHT_SS, /* not settable, float */
    OGL_UI_LABEL_PROPERTY_X1Y1            /* settable,     float[2] */
} _ogl_ui_label_property;

/** TODO */
PUBLIC void ogl_ui_label_deinit(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_label_get_property(__in  __notnull const void* label,
                                      __in  __notnull int         property_value,
                                      __out __notnull void*       out_result);

/** TODO */
PUBLIC void* ogl_ui_label_init(__in           __notnull   ogl_ui                    instance,
                               __in           __notnull   ogl_text                  text_renderer,
                               __in           __notnull   system_hashed_ansi_string name,
                               __in_ecount(2) __notnull   const float*              x1y1);

/** TODO */
PUBLIC void ogl_ui_label_set_property(__in  __notnull void*       label,
                                      __in  __notnull int         property_value,
                                      __out __notnull const void* data);

#endif /* OGL_UI_LABEL_H */
