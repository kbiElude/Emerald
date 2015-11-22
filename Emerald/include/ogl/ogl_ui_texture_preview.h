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
PUBLIC void ogl_ui_texture_preview_get_property(const void*              texture_preview,
                                                _ogl_ui_control_property property,
                                                void*                    out_result);

/** TODO */
PUBLIC void* ogl_ui_texture_preview_init(ogl_ui                      instance,
                                         ogl_text                    text_renderer,
                                         system_hashed_ansi_string   name,
                                         const float*                x1y1,
                                         const float*                max_size,
                                         ral_texture                 to,
                                         ogl_ui_texture_preview_type preview_type);

/** TODO */
PUBLIC void ogl_ui_texture_preview_set_property(void*                    texture_preview,
                                                _ogl_ui_control_property property,
                                                const void*              data);

#endif /* OGL_UI_TEXTURE_PREVIEW_H */
