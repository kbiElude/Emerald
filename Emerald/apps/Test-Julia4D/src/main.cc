/**
 *
 * 4D Julia set app (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "demo/demo_app.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "system/system_variant.h"
#include "ui/ui.h"
#include "stage_step_julia.h"
#include "stage_step_light.h"

INCLUDE_OPTIMUS_SUPPORT;


ral_context      _context                   = nullptr;
float            _data[4]                   = {.17995f, -0.66f, -0.239f, -0.210f};
float            _epsilon                   = 0.001f;
float            _escape                    = 1.2f * 1.5f;
demo_flyby       _flyby                     = nullptr;
float            _light_color[3]            = {1.0f,  1.0f,   1.0f};
float            _light_position[3]         = {2.76f, 1.619f, 0.0f};
int              _max_iterations            = 10;
uint32_t         _pipeline_stage_id         = -1;
system_matrix4x4 _projection_matrix         = nullptr;
float            _raycast_radius_multiplier = 2.65f;
ral_texture      _rt_color_texture          = nullptr;
ral_texture_view _rt_color_texture_view     = nullptr;
ral_texture      _rt_depth_texture          = nullptr;
ral_texture_view _rt_depth_texture_view     = nullptr;
bool             _shadows                   = true;
float            _specularity               = 4.4f;
demo_window      _window                    = nullptr;
system_event     _window_closed_event       = system_event_create(true); /* manual_reset */
const uint32_t   _window_height             = 480;
const uint32_t   _window_width              = 640;

/* Forward declarations */
PRIVATE void _deinit                     ();
PRIVATE void _fire_shadows               (void*          not_used,
                                          void*          not_used2);
PRIVATE void _get_a_value                (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_b_value                (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_c_value                (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_d_value                (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_epsilon_value          (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_escape_threshold_value (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_color_blue_value (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_color_green_value(void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_color_red_value  (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_position_x_value (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_position_y_value (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_light_position_z_value (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_max_iterations_value   (void*          user_arg,
                                          system_variant result);
PRIVATE void _get_specularity_value      (void*          user_arg,
                                          system_variant result);
PRIVATE void _init                       ();
PRIVATE void _init_ui                    ();
PRIVATE void _set_a_value                (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_b_value                (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_c_value                (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_d_value                (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_epsilon_value          (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_escape_threshold_value (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_color_blue_value (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_color_green_value(void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_color_red_value  (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_position_x_value (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_position_y_value (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_light_position_z_value (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_max_iterations_value   (void*          user_arg,
                                          system_variant new_value);
PRIVATE void _set_specularity_value      (void*          user_arg,
                                          system_variant new_value);

PRIVATE void _get_raycast_radius_multiplier_value(void*                   user_arg,
                                                  system_variant          result);
PRIVATE void _rendering_lbm_callback_handler     (system_window           window,
                                                  unsigned short          x,
                                                  unsigned short          y,
                                                  system_window_vk_status new_status,
                                                  void*                   unused);
PRIVATE void _set_raycast_radius_multiplier_value(void*                   user_arg,
                                                  system_variant          new_value);


/** TODO */
PRIVATE void _deinit()
{
    ral_texture textures_to_delete[] =
    {
        _rt_color_texture,
        _rt_depth_texture
    };
    ral_texture_view texture_views_to_delete[] =
    {
        _rt_color_texture_view,
        _rt_depth_texture_view
    };
    const uint32_t n_textures_to_delete      = sizeof(textures_to_delete)      / sizeof(textures_to_delete     [0]);
    const uint32_t n_texture_views_to_delete = sizeof(texture_views_to_delete) / sizeof(texture_views_to_delete[0]);

    ral_context_delete_objects(_context,
                              RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                              n_textures_to_delete,
                              reinterpret_cast<void* const*>(textures_to_delete) );
    ral_context_delete_objects(_context,
                              RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                              n_texture_views_to_delete,
                              reinterpret_cast<void* const*>(texture_views_to_delete) );

    stage_step_julia_deinit(_context);
    stage_step_light_deinit(_context);
}

/** TODO */
PRIVATE ral_present_job _draw_frame(ral_context                                                context,
                                    void*                                                      user_arg,
                                    const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr)
{
    ral_present_task_id julia_present_task_id = -1;
    ral_present_task_id light_present_task_id = -1;
    ral_present_job     present_job           = nullptr;

    present_job = ral_present_job_create();

    ral_present_job_add_task(present_job,
                             stage_step_light_get_present_task(),
                            &light_present_task_id);
    ral_present_job_add_task(present_job,
                             stage_step_julia_get_present_task(),
                            &julia_present_task_id);

    ral_present_job_connect_tasks(present_job,
                                  julia_present_task_id,
                                  0, /* n_src_task_output */
                                  light_present_task_id,
                                  0,        /* n_dst_task_input          */
                                  nullptr); /* out_opt_connection_id_ptr */
    ral_present_job_connect_tasks(present_job,
                                  julia_present_task_id,
                                  1, /* n_src_task_output */
                                  light_present_task_id,
                                  1,        /* n_dst_task_input          */
                                  nullptr); /* out_opt_connection_id_ptr */

    ral_present_job_set_presentable_output(present_job,
                                           light_present_task_id,
                                           false, /* is_input_io */
                                           0);    /* n_io        */

    return present_job;
}

/** TODO */
PRIVATE void _fire_shadows(void* not_used,
                           void* not_used2)
{
    _shadows = !_shadows;
}

/** TODO */
PRIVATE void _get_a_value(void*          user_arg,
                          system_variant result)
{
    system_variant_set_float(result,
                             _data[0]);
}

/** TODO */
PRIVATE void _get_b_value(void*          user_arg,
                          system_variant result)
{
    system_variant_set_float(result,
                             _data[1]);
}

/** TODO */
PRIVATE void _get_c_value(void*          user_arg,
                          system_variant result)
{
    system_variant_set_float(result,
                             _data[2]);
}

/** TODO */
PRIVATE void _get_d_value(void*          user_arg,
                          system_variant result)
{
    system_variant_set_float(result,
                             _data[3]);
}

/** TODO */
PRIVATE void _get_epsilon_value(void*          user_arg,
                                system_variant result)
{
    system_variant_set_float(result, _epsilon);
}

/** TODO */
PRIVATE void _get_escape_threshold_value(void*          user_arg,
                                         system_variant result)
{
    system_variant_set_float(result,
                             _escape);
}

/** TODO */
PRIVATE void _get_light_color_blue_value(void*          user_arg,
                                         system_variant result)
{
    system_variant_set_float(result,
                             _light_color[2]);
}

/** TODO */
PRIVATE void _get_light_color_green_value(void*          user_arg,
                                          system_variant result)
{
    system_variant_set_float(result,
                             _light_color[1]);
}

/** TODO */
PRIVATE void _get_light_color_red_value(void*          user_arg,
                                        system_variant result)
{
    system_variant_set_float(result,
                             _light_color[0]);
}

/** TODO */
PRIVATE void _get_light_position_x_value(void*          user_arg,
                                         system_variant result)
{
    system_variant_set_float(result,
                             _light_position[0]);
}

/** TODO */
PRIVATE void _get_light_position_y_value(void*          user_arg,
                                         system_variant result)
{
    system_variant_set_float(result,
                             _light_position[1]);
}

/** TODO */
PRIVATE void _get_light_position_z_value(void*          user_arg,
                                         system_variant result)
{
    system_variant_set_float(result,
                             _light_position[2]);
}

/** TODO */
PRIVATE void _get_max_iterations_value(void*          user_arg,
                                       system_variant result)
{
    system_variant_set_float(result,
                             (float) _max_iterations);
}

/** TODO */
PRIVATE void _get_raycast_radius_multiplier_value(void*          user_arg,
                                                  system_variant result)
{
    system_variant_set_float(result,
                             _raycast_radius_multiplier);
}

/** TODO */
PRIVATE void _get_specularity_value(void*          user_arg,
                                    system_variant result)
{
    system_variant_set_float(result,
                             _specularity);
}

PRIVATE void _init()
{
    /* Initialize rendertargets */
    ral_texture_create_info      color_texture_create_info;
    ral_texture_view_create_info color_texture_view_create_info;
    ral_texture_create_info      depth_texture_create_info;
    ral_texture_view_create_info depth_texture_view_create_info;

    color_texture_create_info.base_mipmap_depth      = 1;
    color_texture_create_info.base_mipmap_height     = _window_height;
    color_texture_create_info.base_mipmap_width      = _window_width;
    color_texture_create_info.fixed_sample_locations = true;
    color_texture_create_info.format                 = RAL_FORMAT_RGBA8_UNORM;
    color_texture_create_info.name                   = system_hashed_ansi_string_create("Staging color texture");
    color_texture_create_info.n_layers               = 1;
    color_texture_create_info.n_samples              = 1;
    color_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                       RAL_TEXTURE_USAGE_BLIT_SRC_BIT;
    color_texture_create_info.use_full_mipmap_chain  = false;

    depth_texture_create_info.base_mipmap_depth      = 1;
    depth_texture_create_info.base_mipmap_height     = _window_height;
    depth_texture_create_info.base_mipmap_width      = _window_width;
    depth_texture_create_info.fixed_sample_locations = true;
    depth_texture_create_info.format                 = RAL_FORMAT_DEPTH32_FLOAT;
    depth_texture_create_info.name                   = system_hashed_ansi_string_create("Staging depth texture");
    depth_texture_create_info.n_layers               = 1;
    depth_texture_create_info.n_samples              = 1;
    depth_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    depth_texture_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &color_texture_create_info,
                               &_rt_color_texture);
    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &depth_texture_create_info,
                               &_rt_depth_texture);

    color_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_rt_color_texture);
    depth_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_rt_depth_texture);

    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &color_texture_view_create_info,
                                    &_rt_color_texture_view);
    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &depth_texture_view_create_info,
                                    &_rt_depth_texture_view);

    /* Initialize workers */
    stage_step_julia_init(_context,
                          _rt_color_texture_view,
                          _rt_depth_texture_view);
    stage_step_light_init(_context,
                          _rt_color_texture_view,
                          _rt_depth_texture_view);

    /* Initialize the UI */
    _init_ui();

    /* Initialize flyby */
    const float camera_pos[]          = {-0.1611f, 4.5528f, -6.0926f};
    const float camera_movement_delta = 0.025f;
    const float camera_pitch          = -0.6f;
    const float camera_yaw            = 0.024f;
    const bool  flyby_active          = true;

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_FLYBY,
                             &_flyby);

    demo_flyby_set_property(_flyby,
                            DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,
                            camera_pos);
    demo_flyby_set_property(_flyby,
                            DEMO_FLYBY_PROPERTY_IS_ACTIVE,
                           &flyby_active);
    demo_flyby_set_property(_flyby,
                            DEMO_FLYBY_PROPERTY_MOVEMENT_DELTA,
                           &camera_movement_delta);
    demo_flyby_set_property(_flyby,
                            DEMO_FLYBY_PROPERTY_PITCH,
                           &camera_pitch);
    demo_flyby_set_property(_flyby,
                            DEMO_FLYBY_PROPERTY_YAW,
                           &camera_yaw);
}

/** TODO */
PRIVATE void _init_ui()
{
    /* Initialize UI */
    const float scrollbar_1_x1y1[]  = {0.8f, 0.0f};
    const float scrollbar_2_x1y1[]  = {0.8f, 0.1f};
    const float scrollbar_3_x1y1[]  = {0.8f, 0.2f};
    const float scrollbar_4_x1y1[]  = {0.8f, 0.3f};
    const float scrollbar_5_x1y1[]  = {0.8f, 0.4f};
    const float scrollbar_6_x1y1[]  = {0.8f, 0.5f};
    const float scrollbar_7_x1y1[]  = {0.8f, 0.6f};
    const float scrollbar_8_x1y1[]  = {0.8f, 0.7f};
    const float scrollbar_9_x1y1[]  = {0.8f, 0.8f};
    const float scrollbar_10_x1y1[] = {0.0f, 0.2f};
    const float scrollbar_11_x1y1[] = {0.0f, 0.3f};
    const float scrollbar_12_x1y1[] = {0.0f, 0.4f};
    const float scrollbar_13_x1y1[] = {0.0f, 0.5f};
    const float scrollbar_14_x1y1[] = {0.0f, 0.6f};
    const float scrollbar_15_x1y1[] = {0.0f, 0.7f};
    const float checkbox_1_x1y1[]   = {0.0f, 0.1f};

    ral_rendering_handler rh;
    ui                    rh_ui;

    demo_window_get_property          (_window,
                                       DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                                      &rh);
    ral_rendering_handler_get_property(rh,
                                       RAL_RENDERING_HANDLER_PROPERTY_UI,
                                      &rh_ui);

#if 0
    TODO

    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("A"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-1.5f),
                     system_variant_create_float(1.5f),
                     scrollbar_1_x1y1,
                     _get_a_value,
                     NULL,         /* get_current_value_user_arg */
                     _set_a_value,
                     NULL);        /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("B"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-1.5f),
                     system_variant_create_float(1.5f),
                     scrollbar_2_x1y1,
                     _get_b_value,
                     NULL,         /* get_current_value_user_arg */
                     _set_b_value,
                     NULL);        /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("C"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-1.5f),
                     system_variant_create_float(1.5f),
                     scrollbar_3_x1y1,
                     _get_c_value,
                     NULL,         /* get_current_value_user_arg */
                     _set_c_value,
                     NULL);        /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("D"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-1.5f),
                     system_variant_create_float(1.5f),
                     scrollbar_4_x1y1,
                     _get_d_value,
                     NULL,         /* get_current_value_user_arg */
                     _set_d_value,
                     NULL);        /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Epsilon"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.00001f),
                     system_variant_create_float(0.01f),
                     scrollbar_5_x1y1,
                     _get_epsilon_value,
                     NULL,               /* get_current_value_user_arg */
                     _set_epsilon_value,
                     NULL);              /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Escape threshold"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.01f),
                     system_variant_create_float(8.0f),
                     scrollbar_6_x1y1,
                     _get_escape_threshold_value,
                     NULL,                        /* get_current_value_user_arg */
                     _set_escape_threshold_value,
                     NULL);                       /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Max iterations"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(1),
                     system_variant_create_float(10),
                     scrollbar_7_x1y1,
                     _get_max_iterations_value,
                     NULL,                      /* get_current_value_user_arg */
                     _set_max_iterations_value,
                     NULL);                     /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Raycast radius"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(1),
                     system_variant_create_float(4.0f),
                     scrollbar_8_x1y1,
                     _get_raycast_radius_multiplier_value,
                     NULL,                                 /* get_current_value_user_arg */
                     _set_raycast_radius_multiplier_value,
                     NULL);                                /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Specularity"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.0001f),
                     system_variant_create_float(40.0f),
                     scrollbar_9_x1y1,
                     _get_specularity_value,
                     NULL,                   /* get_current_value_user_arg */
                     _set_specularity_value,
                     NULL);                  /* set_current_value_user_arg */
    ui_add_checkbox (rh_ui,
                     system_hashed_ansi_string_create("Shadows"),
                     checkbox_1_x1y1,
                     _shadows,
                     _fire_shadows,
                     NULL);         /* fire_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Color R"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.0f),
                     system_variant_create_float(1.0f),
                     scrollbar_10_x1y1,
                     _get_light_color_red_value,
                     NULL,                       /* get_current_value_user_arg */
                     _set_light_color_red_value,
                     NULL);                      /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Color G"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.0f),
                     system_variant_create_float(1.0f),
                     scrollbar_11_x1y1,
                     _get_light_color_green_value,
                     NULL,                         /* get_current_value_user_arg */
                     _set_light_color_green_value,
                     NULL);                        /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Color B"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(0.0f),
                     system_variant_create_float(1.0f),
                     scrollbar_12_x1y1,
                     _get_light_color_blue_value,
                     NULL,                        /* get_current_value_user_arg */
                     _set_light_color_blue_value,
                     NULL);                       /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Position X"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-3.0f),
                     system_variant_create_float(3.0f),
                     scrollbar_13_x1y1,
                     _get_light_position_x_value,
                     NULL,                        /* get_current_value_user_arg */
                     _set_light_position_x_value,
                     NULL);                       /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Position Y"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-3.0f),
                     system_variant_create_float(3.0f),
                     scrollbar_14_x1y1,
                     _get_light_position_y_value,
                     NULL,                        /* get_current_value_user_arg */
                     _set_light_position_y_value,
                     NULL);                       /* set_current_value_user_arg */
    ui_add_scrollbar(rh_ui,
                     system_hashed_ansi_string_create("Light Position Z"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(-3.0f),
                     system_variant_create_float(3.0f),
                     scrollbar_15_x1y1,
                     _get_light_position_z_value,
                     NULL,                        /* get_current_value_user_arg */
                     _set_light_position_z_value,
                     NULL);                       /* set_current_value_user_arg */
#endif
}

PRIVATE void _rendering_lbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*                   unused)
{
    system_event_set(_window_closed_event);
}

/** TODO */
PRIVATE void _set_a_value(void*          user_arg,
                          system_variant new_value)
{
    system_variant_get_float(new_value,
                             _data + 0);
}

/** TODO */
PRIVATE void _set_b_value(void*          user_arg,
                          system_variant new_value)
{
    system_variant_get_float(new_value,
                             _data + 1);
}

/** TODO */
PRIVATE void _set_c_value(void*          user_arg,
                          system_variant new_value)
{
    system_variant_get_float(new_value,
                             _data + 2);
}

/** TODO */
PRIVATE void _set_d_value(void*          user_arg,
                          system_variant new_value)
{
    system_variant_get_float(new_value,
                             _data + 3);
}

/** TODO */
PRIVATE void _set_epsilon_value(void*          user_arg,
                                system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_epsilon);
}

/** TODO */
PRIVATE void _set_escape_threshold_value(void*          user_arg,
                                         system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_escape);
}

/** TODO */
PRIVATE void _set_light_color_blue_value(void*          user_arg,
                                         system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_color + 2);
}

/** TODO */
PRIVATE void _set_light_color_green_value(void*          user_arg,
                                          system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_color + 1);
}

/** TODO */
PRIVATE void _set_light_color_red_value(void*          user_arg,
                                        system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_color + 0);
}

/** TODO */
PRIVATE void _set_light_position_x_value(void*          user_arg,
                                         system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_position + 0);
}

/** TODO */
PRIVATE void _set_light_position_y_value(void*          user_arg,
                                         system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_position + 1);
}

/** TODO */
PRIVATE void _set_light_position_z_value(void*          user_arg,
                                         system_variant new_value)
{
    system_variant_get_float(new_value,
                             _light_position + 2);
}

/** TODO */
PRIVATE void _set_max_iterations_value(void*          user_arg,
                                       system_variant new_value)
{
    float new_max_iterations = 0;

    system_variant_get_float(new_value,
                            &new_max_iterations);

    _max_iterations = (int) new_max_iterations;
}

/** TODO */
PRIVATE void _set_raycast_radius_multiplier_value(void*          user_arg,
                                                  system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_raycast_radius_multiplier);
}

/** TODO */
PRIVATE void _set_specularity_value(void*          user_arg,
                                    system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_specularity);
}

/** TODO */
PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

/** TODO */
PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    _deinit();
}

/** Please see header for specification */
const float* main_get_data_vector()
{
    return _data;
}

/** Please see header for specification */
float main_get_epsilon()
{
    return _epsilon;
}

/** Please see header for specification */
float main_get_escape_threshold()
{
    return _escape;
}

/** Please see header for specification */
const float* main_get_light_color()
{
    return _light_color;
}

/** Please see header for specification */
const float* main_get_light_position()
{
    return _light_position;
}

/** Please see header for specification */
system_matrix4x4 main_get_projection_matrix()
{
    return _projection_matrix;
}

/** Please see header for specification */
int main_get_max_iterations()
{
    return _max_iterations;
}

/** Please see header for specification */
float main_get_raycast_radius_multiplier()
{
    return _raycast_radius_multiplier;
}

/** Please see header for specification */
bool main_get_shadows_status()
{
    return _shadows;
}

/** Please see header for specification */
float main_get_specularity()
{
    return _specularity;
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc    = _draw_frame;
    ral_rendering_handler                   rendering_handler    = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name          = system_hashed_ansi_string_create("Julia 4D test app");

    window_create_info.resolution[0] = _window_width;
    window_create_info.resolution[1] = _window_height;

    _window = demo_app_create_window(window_name,
                                     window_create_info,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ral_rendering_handler_set_property(rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    /* Set up matrices */
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,                                        /* fov_y  */
                                                                               float(_window_width) / float(_window_height), /* ar     */
                                                                               0.001f,                                       /* z_near */
                                                                               500.0f);                                      /* z_far  */

    /* Set up callbacks */
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL); /* callback_func_user_arg */
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  NULL);

    /* Set up rendering-related stuff */
    _init();

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_app_destroy_window(window_name);

    /* Deinitialize GL objects */
    system_matrix4x4_release(_projection_matrix);
    system_event_release    (_window_closed_event);

    main_force_deinit();

    return 0;
}