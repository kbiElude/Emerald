/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Internal usage only - no functions exported.
 *
 * The functionality can be accessed via ui
 */
#ifndef UI_TEXTURE_PREVIEW_H
#define UI_TEXTURE_PREVIEW_H

#include "ogl/ogl_types.h"
#include "ui/ui_types.h"


/** TODO */
PUBLIC void ui_texture_preview_deinit(void* internal_instance);

/** TODO */
PUBLIC ral_present_task ui_texture_preview_get_present_task(void*            internal_instance,
                                                            ral_texture_view target_texture_view);


/** TODO */
PUBLIC void ui_texture_preview_get_property(const void*         texture_preview,
                                            ui_control_property property,
                                            void*               out_result_ptr);

/** TODO */
PUBLIC void* ui_texture_preview_init(ui                        instance,
                                     varia_text_renderer       text_renderer,
                                     system_hashed_ansi_string name,
                                     const float*              x1y1,
                                     const float*              max_size,
                                     ral_texture_view          texture_view,
                                     ui_texture_preview_type   preview_type);

/** TODO */
PUBLIC void ui_texture_preview_set_property(void*               texture_preview,
                                            ui_control_property property,
                                            const void*         data);

#endif /* UI_TEXTURE_PREVIEW_H */
