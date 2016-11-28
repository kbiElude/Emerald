/**
 *
 * Emerald scene viewer (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "scene/scene_camera.h"
#include "system/system_critical_section.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_dropdown.h"
#include "ui/ui_texture_preview.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

PRIVATE ui         _ui                       = nullptr;
PRIVATE ui_control _ui_active_camera_control = nullptr;
PRIVATE ui_control _ui_active_path_control   = nullptr;
PRIVATE ui_control _ui_texture_preview       = nullptr;


/* Forward declarations */
PRIVATE void _callback_on_dropdown_switch (ui_control control,
                                           int        callback_id,
                                           void*      callback_subscriber_data,
                                           void*      callback_data);
PRIVATE void _on_active_camera_changed    (void*      fire_proc_user_arg,
                                           void*      event_user_arg);
PRIVATE void _on_shown_camera_path_changed(void*      fire_proc_user_arg,
                                           void*      event_user_arg);
PRIVATE void _update_controls_location ();

/** TODO */
PRIVATE void _callback_on_dropdown_switch(ui_control control,
                                          int        callback_id,
                                          void*      callback_subscriber_data,
                                          void*      callback_data)
{
    ral_rendering_handler rendering_handler = nullptr;

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    //ogl_rendering_handler_lock_bound_context(rendering_handler); ?
    {
        _update_controls_location();
    }
    //ogl_rendering_handler_unlock_bound_context(rendering_handler);
}

/** "Active camera" dropdown call-back handler */
PRIVATE void _on_active_camera_changed(void* fire_proc_user_arg,
                                       void* event_user_arg)
{
    state_set_active_camera_index( (unsigned int) (intptr_t) event_user_arg);
}

/* "Show camera path" dropdown call-back handler */
PRIVATE void _on_shown_camera_path_changed(void* fire_proc_user_arg,
                                           void* event_user_arg)
{
    state_set_active_camera_path_index( (unsigned int) (intptr_t) event_user_arg);
}

/** TODO */
PRIVATE void _update_controls_location()
{
    float active_path_dy;
    float new_active_path_x1y1x2y2   [4];
    float prev_active_camera_x1y1x2y2[4];
    float prev_active_path_x1y1x2y2  [4];

    ui_get_control_property(_ui_active_camera_control,
                            UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                            prev_active_camera_x1y1x2y2);
    ui_get_control_property(_ui_active_path_control,
                            UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                            prev_active_path_x1y1x2y2);

    active_path_dy              = fabs(prev_active_path_x1y1x2y2[3] - prev_active_path_x1y1x2y2[1]);
    new_active_path_x1y1x2y2[0] = prev_active_path_x1y1x2y2[0];
    new_active_path_x1y1x2y2[1] = 1.0f - (prev_active_camera_x1y1x2y2[1] - 1.0f / 720.0f);
    new_active_path_x1y1x2y2[2] = prev_active_path_x1y1x2y2[2];
    new_active_path_x1y1x2y2[3] = new_active_path_x1y1x2y2[1] - active_path_dy;

    ui_set_control_property(_ui_active_path_control,
                            UI_CONTROL_PROPERTY_DROPDOWN_X1Y1,
                            new_active_path_x1y1x2y2);
}

/** Please see header for spec */
PUBLIC void ui_deinit()
{
    /* Stub */
}

/** Please see header for spec */
PUBLIC ui_control ui_get_active_path_control()
{
    return _ui_active_path_control;
}

/** Please see header for spec */
PUBLIC ui_control ui_get_texture_preview_control()
{
    return _ui_texture_preview;
}

/** Please see header for spec */
PUBLIC void ui_init()
{
    const float active_camera_dropdown_x1y1[2] = {0.7f, 0.1f};
    int         window_size[2]                 = {0};

    /* Initialize components required to power UI */
    demo_window_get_property          (_window,
                                       DEMO_WINDOW_PROPERTY_RESOLUTION,
                                       window_size);
    ral_rendering_handler_get_property(_rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_UI,
                                       &_ui);

    /* Create camera selector */
    _ui_active_camera_control = ui_add_dropdown(_ui,
                                                state_get_number_of_cameras(),
                                                state_get_camera_names(),
                                                state_get_camera_indices(),
                                                state_get_active_camera_index(),
                                                system_hashed_ansi_string_create("Active camera:"),
                                                active_camera_dropdown_x1y1,
                                                _on_active_camera_changed,
                                                nullptr);

    /* Create camera path selector */
    float next_ui_control_x1y1    [2];
    float prev_ui_control_x1y1x2y2[4];
    float texture_preview_x1y1    [2] = {0.7f, 0.7f};
    float texture_preview_max_size[2] = {0.2f, 0.2f};

    ui_get_control_property(_ui_active_camera_control,
                            UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                            prev_ui_control_x1y1x2y2);

    next_ui_control_x1y1[0] = active_camera_dropdown_x1y1[0];
    next_ui_control_x1y1[1] = 1.0f - (prev_ui_control_x1y1x2y2[1] - 1.0f / 720.0f);

    _ui_active_path_control = ui_add_dropdown(_ui,
                                              state_get_number_of_cameras(),
                                              state_get_camera_path_names(),
                                              state_get_camera_path_indices(),
                                              state_get_active_camera_path_index(),
                                              system_hashed_ansi_string_create("Show camera path for:"),
                                              next_ui_control_x1y1,
                                              _on_shown_camera_path_changed,
                                              nullptr);

    /* Create shadow map preview */
#ifdef SHOW_SM_PREVIEW
    _ui_texture_preview = ui_add_texture_preview(_ui,
                                                 system_hashed_ansi_string_create("Texture preview"),
                                                 texture_preview_x1y1,
                                                 texture_preview_max_size,
                                                 nullptr, /* texture */
                                                 UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGBA);
#endif

    /* Register for call-backs */
    ui_register_control_callback(_ui,
                                 _ui_active_camera_control,
                                 UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                 _callback_on_dropdown_switch,
                                 nullptr); /* callback_proc_user_arg */
}

