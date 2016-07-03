/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "system/system_variant.h"
#include "ui/ui_types.h"
#include "varia/varia_types.h"


REFCOUNT_INSERT_DECLARATIONS(ui,
                             ui);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_button(ui                        ui_instance,
                                            system_hashed_ansi_string name,
                                            const float*              x1y1,
                                            PFNUIFIREPROCPTR          pfn_fire_ptr,
                                            void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_checkbox(ui                        ui_instance,
                                              system_hashed_ansi_string name,
                                              const float*              x1y1,
                                              bool                      default_status,
                                              PFNUIFIREPROCPTR          pfn_fire_ptr,
                                              void*                     fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_dropdown(ui                         ui_instance,
                                              uint32_t                   n_entries,
                                              system_hashed_ansi_string* strings,
                                              void**                     user_args,
                                              uint32_t                   n_selected_entry,
                                              system_hashed_ansi_string  name,
                                              const float*               x1y1,
                                              PFNUIFIREPROCPTR           pfn_fire_ptr,
                                              void*                      fire_user_arg);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_frame(ui           ui_instance,
                                           const float* x1y1x2y2);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_label(ui                        ui_instance,
                                           system_hashed_ansi_string name,
                                           const float*              x1y1);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_scrollbar(ui                          ui_instance,
                                               system_hashed_ansi_string   name,
                                               ui_scrollbar_text_location  text_location,
                                               system_variant              min_value,
                                               system_variant              max_value,
                                               const float*                x1y1,
                                               PFNUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                               void*                       get_current_value_ptr_user_arg,
                                               PFNUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                               void*                       set_current_value_ptr_user_arg);

/** TODO */
PUBLIC EMERALD_API ui_control ui_add_texture_preview(ui                        ui_instance,
                                                     system_hashed_ansi_string name,
                                                     const float*              x1y1,
                                                     const float*              max_size,
                                                     ral_texture               texture,
                                                     ui_texture_preview_type   preview_type);
/** TODO */
PUBLIC ui ui_create(varia_text_renderer       text_renderer,
                    system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ui_get_control_property(ui_control          control,
                                                ui_control_property property,
                                                void*               out_result);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC ral_context ui_get_context(ui ui_instance);

/** TODO
 *
 *  It is caller's responsibility to release the present task when no longer needed.
 */
PUBLIC ral_present_task ui_get_present_task(ui               ui_instance,
                                            ral_texture_view target_texture_view);

/** INTERNAL USAGE ONLY.
 *
 *  TODO: REMOVE
 **/
PUBLIC ral_program ui_get_registered_program(ui                        ui_instance,
                                             system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ui_lock(ui                                  ui_instance,
                                system_read_write_mutex_access_type access_type);

/** INTERNAL USAGE ONLY.
 *
 *  Call-back entry-point to be used by UI control implementations. Will dispatch call-backs
 *  to subscribers by using engine thread pool.
 **/
PUBLIC void ui_receive_control_callback(ui         ui_instance,
                                        ui_control control,
                                        int        callback_id,
                                        void*      callback_user_arg);

/** TODO */
PUBLIC void ui_register_control_callback(ui                        ui_instance,
                                         ui_control                control,
                                         int                       callback_id,
                                         PFNUIEVENTCALLBACKPROCPTR callback_proc_ptr,
                                         void*                     callback_proc_user_arg);

/** INTERNAL USAGE ONLY.
 *
 *  TODO
 **/
PUBLIC bool ui_register_program(ui                        ui_instance,
                                system_hashed_ansi_string name,
                                ral_program               program);

/** TODO */
PUBLIC EMERALD_API void ui_reposition_control(ui_control   control,
                                              unsigned int new_control_index);

/** TODO */
PUBLIC EMERALD_API void ui_set_control_property(ui_control          control,
                                                ui_control_property property,
                                                const void*         data);

/** TODO */
PUBLIC void ui_unlock(ui                                  ui,
                      system_read_write_mutex_access_type access_type);

#endif /* UI_MAIN_H */
