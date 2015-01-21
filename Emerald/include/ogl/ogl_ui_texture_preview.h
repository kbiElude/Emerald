/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ogl_ui
 */
#ifndef OGL_UI_TEXTURE_PREVIEW_H
#define OGL_UI_TEXTURE_PREVIEW_H

/** Properties */
typedef enum
{
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_COLOR,              /* settable, GLfloat[4] */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_ALPHA,     /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_RGB,       /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_ALPHA, /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_RGB,   /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_ALPHA, /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_RGB,   /* settable, GLenum     */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_IS_BLENDING_ENABLED,      /* settable, bool       */
    OGL_UI_TEXTURE_PREVIEW_PROPERTY_SHOW_TEXTURE_NAME,        /* settable, bool       */
};

/** TODO */
PUBLIC void ogl_ui_texture_preview_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_texture_preview_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_texture_preview_get_property(__in  __notnull const void* texture_preview,
                                                __in  __notnull int         property_value,
                                                __out __notnull void*       out_result);

/** TODO */
PUBLIC void* ogl_ui_texture_preview_init(__in           __notnull ogl_ui                      instance,
                                         __in           __notnull ogl_text                    text_renderer,
                                         __in           __notnull system_hashed_ansi_string   name,
                                         __in_ecount(2) __notnull const float*                x1y1,
                                         __in_ecount(2) __notnull const float*                max_size,
                                         __in                     ogl_texture                 to,
                                         __in                     ogl_ui_texture_preview_type preview_type);

/** TODO */
PUBLIC void ogl_ui_texture_preview_set_property(__in __notnull void*       texture_preview,
                                                __in __notnull int         property_value,
                                                __in __notnull const void* data);

#endif /* OGL_UI_TEXTURE_PREVIEW_H */
