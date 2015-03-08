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


/** TODO */
PUBLIC void ogl_ui_texture_preview_deinit(void* internal_instance);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_texture_preview_draw(void* internal_instance);

/** TODO */
PUBLIC void ogl_ui_texture_preview_get_property(__in  __notnull const void*              texture_preview,
                                                __in            _ogl_ui_control_property property,
                                                __out __notnull void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_texture_preview_init(__in           __notnull ogl_ui                      instance,
                                         __in           __notnull ogl_text                    text_renderer,
                                         __in           __notnull system_hashed_ansi_string   name,
                                         __in_ecount(2) __notnull const float*                x1y1,
                                         __in_ecount(2) __notnull const float*                max_size,
                                         __in                     ogl_texture                 to,
                                         __in                     ogl_ui_texture_preview_type preview_type);

/** TODO */
PUBLIC void ogl_ui_texture_preview_set_property(__in __notnull void*                    texture_preview,
                                                __in __notnull _ogl_ui_control_property property,
                                                __in __notnull const void*              data);

#endif /* OGL_UI_TEXTURE_PREVIEW_H */
