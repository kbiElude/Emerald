/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_dropdown.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui_texture_preview.h"
#include "scene/scene_camera.h"
#include "system/system_critical_section.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

PRIVATE ogl_text       _text_renderer                = NULL;
PRIVATE ogl_ui         _ui                           = NULL;
PRIVATE ogl_ui_control _ui_active_camera_control     = NULL;
PRIVATE ogl_ui_control _ui_active_path_control       = NULL;
PRIVATE ogl_ui_control _ui_texture_preview           = NULL;


/* Forward declarations */
PRIVATE void _callback_on_dropdown_switch (ogl_ui_control control,
                                           int            callback_id,
                                           void*          callback_subscriber_data,
                                           void*          callback_data);
PRIVATE void _on_active_camera_changed    (void*          fire_proc_user_arg,
                                           void*          event_user_arg);
PRIVATE void _on_shown_camera_path_changed(void*          fire_proc_user_arg,
                                           void*          event_user_arg);
PRIVATE void _update_controls_location ();

/** TODO */
PRIVATE void _callback_on_dropdown_switch(ogl_ui_control control,
                                          int            callback_id,
                                          void*          callback_subscriber_data,
                                          void*          callback_data)
{
    ogl_rendering_handler rendering_handler = NULL;

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    ogl_rendering_handler_lock_bound_context(rendering_handler);
    {
        _update_controls_location();
    }
    ogl_rendering_handler_unlock_bound_context(rendering_handler);
}

/** "Active camera" dropdown call-back handler */
PRIVATE void _on_active_camera_changed(void* fire_proc_user_arg,
                                       void* event_user_arg)
{
    state_set_active_camera_index( (unsigned int) event_user_arg);
}

/* "Show camera path" dropdown call-back handler */
PRIVATE void _on_shown_camera_path_changed(void* fire_proc_user_arg,
                                           void* event_user_arg)
{
    state_set_active_camera_path_index( (unsigned int) event_user_arg);
}

/** TODO */
PRIVATE void _update_controls_location()
{
    float active_path_dy;
    float new_active_path_x1y1x2y2   [4];
    float prev_active_camera_x1y1x2y2[4];
    float prev_active_path_x1y1x2y2  [4];

    ogl_ui_get_control_property(_ui_active_camera_control,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                                prev_active_camera_x1y1x2y2);
    ogl_ui_get_control_property(_ui_active_path_control,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                                prev_active_path_x1y1x2y2);

    active_path_dy              = fabs(prev_active_path_x1y1x2y2[3] - prev_active_path_x1y1x2y2[1]);
    new_active_path_x1y1x2y2[0] = prev_active_path_x1y1x2y2[0];
    new_active_path_x1y1x2y2[1] = 1.0f - (prev_active_camera_x1y1x2y2[1] - 1.0f / 720.0f);
    new_active_path_x1y1x2y2[2] = prev_active_path_x1y1x2y2[2];
    new_active_path_x1y1x2y2[3] = new_active_path_x1y1x2y2[1] - active_path_dy;

    ogl_ui_set_control_property(_ui_active_path_control,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1,
                                new_active_path_x1y1x2y2);
}

/** Please see header for spec */
PUBLIC void ui_deinit()
{
    if (_ui != NULL)
    {
        ogl_ui_release(_ui);

        _ui = NULL;
    }

    if (_text_renderer != NULL)
    {
        ogl_text_release(_text_renderer);

        _text_renderer = NULL;
    }
}

/** Please see header for spec */
PUBLIC void ui_draw()
{
    ogl_ui_draw  (_ui);
    ogl_text_draw(_context,
                  _text_renderer);
}

/** Please see header for spec */
PUBLIC ogl_ui_control ui_get_active_path_control()
{
    return _ui_active_path_control;
}

/** Please see header for spec */
PUBLIC ogl_ui_control ui_get_texture_preview_control()
{
    return _ui_texture_preview;
}

/** Please see header for spec */
PUBLIC void ui_init()
{
    const float  active_camera_dropdown_x1y1[2] = {0.7f, 0.1f};
    const float  text_default_size              = 0.5f;
    int          window_size[2]                 = {0};

    /* Initialize components required to power UI */
    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_TEXT_RENDERER,
                            &_text_renderer);
    ogl_text_retain         (_text_renderer);

    ogl_text_set_text_string_property(_text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_default_size);

    _ui = ogl_ui_create(_text_renderer,
                        system_hashed_ansi_string_create("UI") );

    /* Create camera selector */
    _ui_active_camera_control = ogl_ui_add_dropdown(_ui,
                                                    state_get_number_of_cameras(),
                                                    state_get_camera_names(),
                                                    state_get_camera_indices(),
                                                    state_get_active_camera_index(),
                                                    system_hashed_ansi_string_create("Active camera:"),
                                                    active_camera_dropdown_x1y1,
                                                    _on_active_camera_changed,
                                                    NULL);

    /* Create camera path selector */
    float next_ui_control_x1y1    [2];
    float prev_ui_control_x1y1x2y2[4];
    float texture_preview_x1y1    [2] = {0.7f, 0.7f};
    float texture_preview_max_size[2] = {0.2f, 0.2f};

    ogl_ui_get_control_property(_ui_active_camera_control,
                                OGL_UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2,
                                prev_ui_control_x1y1x2y2);

    next_ui_control_x1y1[0] = active_camera_dropdown_x1y1[0];
    next_ui_control_x1y1[1] = 1.0f - (prev_ui_control_x1y1x2y2[1] - 1.0f / 720.0f);

    _ui_active_path_control = ogl_ui_add_dropdown(_ui,
                                                  state_get_number_of_cameras(),
                                                  state_get_camera_path_names(),
                                                  state_get_camera_path_indices(),
                                                  state_get_active_camera_path_index(),
                                                  system_hashed_ansi_string_create("Show camera path for:"),
                                                  next_ui_control_x1y1,
                                                  _on_shown_camera_path_changed,
                                                  NULL);

    /* Create shadow map preview */
#ifdef SHOW_SM_PREVIEW
    _ui_texture_preview = ogl_ui_add_texture_preview(_ui,
                                                     system_hashed_ansi_string_create("Texture preview"),
                                                     texture_preview_x1y1,
                                                     texture_preview_max_size,
                                                     NULL, /* texture */
                                                     OGL_UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGBA);
#endif

    /* Register for call-backs */
    ogl_ui_register_control_callback(_ui,
                                     _ui_active_camera_control,
                                     OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                     _callback_on_dropdown_switch,
                                     NULL); /* callback_proc_user_arg */
}

/** TODO */

