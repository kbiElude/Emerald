/**
 *
 * 4D Julia set app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_ui.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"
#include "system/system_variant.h"
#include "stage_step_julia.h"
#include "stage_step_light.h"

INCLUDE_OPTIMUS_SUPPORT;


ogl_context      _context                   = NULL;
float            _data[4]                   = {.17995f, -0.66f, -0.239f, -0.210f};
float            _epsilon                   = 0.001f;
float            _escape                    = 1.2f * 1.5f;
ogl_flyby        _flyby                     = NULL;
float            _light_color[3]            = {1.0f,  1.0f,   1.0f};
float            _light_position[3]         = {2.76f, 1.619f, 0.0f};
int              _max_iterations            = 10;
ogl_pipeline     _pipeline                  = NULL;
uint32_t         _pipeline_stage_id         = -1;
system_matrix4x4 _projection_matrix         = NULL;
float            _raycast_radius_multiplier = 2.65f;
bool             _shadows                   = true;
float            _specularity               = 4.4f;
system_window    _window                    = NULL;
system_event     _window_closed_event       = system_event_create(true); /* manual_reset */


/* Forward declarations */
PRIVATE void _deinit_gl                              (ogl_context             context,
                                                      void*                   not_used);
PRIVATE void _fire_shadows                           (void*                   not_used,
                                                      void*                   not_used2);
PRIVATE void _get_a_value                            (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_b_value                            (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_c_value                            (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_d_value                            (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_epsilon_value                      (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_escape_threshold_value             (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_color_blue_value             (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_color_green_value            (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_color_red_value              (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_position_x_value             (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_position_y_value             (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_light_position_z_value             (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_max_iterations_value               (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_raycast_radius_multiplier_value    (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _get_specularity_value                  (void*                   user_arg,
                                                      system_variant          result);
PRIVATE void _init_gl                                (ogl_context             context,
                                                      void*                   not_used);
PRIVATE void _init_pipeline_rendering_thread_callback(ogl_context             context,
                                                      void*                   unused);
PRIVATE void _init_ui                                ();
PRIVATE void _rendering_handler                      (ogl_context             context,
                                                      uint32_t                n_frames_rendered,
                                                      system_time             frame_time,
                                                      void*                   renderer);
PRIVATE void _rendering_lbm_callback_handler         (system_window           window,
                                                      unsigned short          x,
                                                      unsigned short          y,
                                                      system_window_vk_status new_status,
                                                      void*                   unused);
PRIVATE void _set_a_value                            (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_b_value                            (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_c_value                            (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_d_value                            (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_epsilon_value                      (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_escape_threshold_value             (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_color_blue_value             (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_color_green_value            (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_color_red_value              (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_position_x_value             (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_position_y_value             (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_light_position_z_value             (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_max_iterations_value               (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_raycast_radius_multiplier_value    (void*                   user_arg,
                                                      system_variant          new_value);
PRIVATE void _set_specularity_value                  (void*                   user_arg,
                                                      system_variant          new_value);


/** TODO */
PRIVATE void _deinit_gl(ogl_context context)
{
    stage_step_julia_deinit(context);
    stage_step_light_deinit(context);
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

PRIVATE void _init_gl(ogl_context context,
                      void*       not_used)
{
    _init_pipeline_rendering_thread_callback(context,
                                             not_used);

    stage_step_julia_init(context,
                          _pipeline,
                          _pipeline_stage_id);
    stage_step_light_init(context,
                          _pipeline,
                          _pipeline_stage_id);
}

/** TODO */
PRIVATE void _init_pipeline_rendering_thread_callback(ogl_context context,
                                                      void*       unused)
{
    /* Set up pipeline */
    _pipeline          = ogl_pipeline_create   (_context,
                                                true,  /* should_overlay_performance_info */
                                                system_hashed_ansi_string_create("pipeline") );
    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    _init_ui();
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
    ogl_ui      pipeline_ui         = ogl_pipeline_get_ui(_pipeline);

    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("A"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-1.5f),
                         system_variant_create_float(1.5f),
                         scrollbar_1_x1y1,
                         _get_a_value,
                         NULL,         /* get_current_value_user_arg */
                         _set_a_value,
                         NULL);        /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("B"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-1.5f),
                         system_variant_create_float(1.5f),
                         scrollbar_2_x1y1,
                         _get_b_value,
                         NULL,         /* get_current_value_user_arg */
                         _set_b_value,
                         NULL);        /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("C"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-1.5f),
                         system_variant_create_float(1.5f),
                         scrollbar_3_x1y1,
                         _get_c_value,
                         NULL,         /* get_current_value_user_arg */
                         _set_c_value,
                         NULL);        /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("D"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-1.5f),
                         system_variant_create_float(1.5f),
                         scrollbar_4_x1y1,
                         _get_d_value,
                         NULL,         /* get_current_value_user_arg */
                         _set_d_value,
                         NULL);        /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Epsilon"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.00001f),
                         system_variant_create_float(0.01f),
                         scrollbar_5_x1y1,
                         _get_epsilon_value,
                         NULL,               /* get_current_value_user_arg */
                         _set_epsilon_value,
                         NULL);              /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Escape threshold"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.01f),
                         system_variant_create_float(8.0f),
                         scrollbar_6_x1y1,
                         _get_escape_threshold_value,
                         NULL,                        /* get_current_value_user_arg */
                         _set_escape_threshold_value,
                         NULL);                       /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Max iterations"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(1),
                         system_variant_create_float(10),
                         scrollbar_7_x1y1,
                         _get_max_iterations_value,
                         NULL,                      /* get_current_value_user_arg */
                         _set_max_iterations_value,
                         NULL);                     /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Raycast radius"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(1),
                         system_variant_create_float(4.0f),
                         scrollbar_8_x1y1,
                         _get_raycast_radius_multiplier_value,
                         NULL,                                 /* get_current_value_user_arg */
                         _set_raycast_radius_multiplier_value,
                         NULL);                                /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Specularity"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0001f),
                         system_variant_create_float(40.0f),
                         scrollbar_9_x1y1,
                         _get_specularity_value,
                         NULL,                   /* get_current_value_user_arg */
                         _set_specularity_value,
                         NULL);                  /* set_current_value_user_arg */
    ogl_ui_add_checkbox (pipeline_ui,
                         system_hashed_ansi_string_create("Shadows"),
                         checkbox_1_x1y1,
                         _shadows,
                         _fire_shadows,
                         NULL);         /* fire_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Color R"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0f),
                         system_variant_create_float(1.0f),
                         scrollbar_10_x1y1,
                         _get_light_color_red_value,
                         NULL,                       /* get_current_value_user_arg */
                         _set_light_color_red_value,
                         NULL);                      /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Color G"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0f),
                         system_variant_create_float(1.0f),
                         scrollbar_11_x1y1,
                         _get_light_color_green_value,
                         NULL,                         /* get_current_value_user_arg */
                         _set_light_color_green_value,
                         NULL);                        /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Color B"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0f),
                         system_variant_create_float(1.0f),
                         scrollbar_12_x1y1,
                         _get_light_color_blue_value,
                         NULL,                        /* get_current_value_user_arg */
                         _set_light_color_blue_value,
                         NULL);                       /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Position X"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-3.0f),
                         system_variant_create_float(3.0f),
                         scrollbar_13_x1y1,
                         _get_light_position_x_value,
                         NULL,                        /* get_current_value_user_arg */
                         _set_light_position_x_value,
                         NULL);                       /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Position Y"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-3.0f),
                         system_variant_create_float(3.0f),
                         scrollbar_14_x1y1,
                         _get_light_position_y_value,
                         NULL,                        /* get_current_value_user_arg */
                         _set_light_position_y_value,
                         NULL);                       /* set_current_value_user_arg */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Light Position Z"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(-3.0f),
                         system_variant_create_float(3.0f),
                         scrollbar_15_x1y1,
                         _get_light_position_z_value,
                         NULL,                        /* get_current_value_user_arg */
                         _set_light_position_z_value,
                         NULL);                       /* set_current_value_user_arg */
}

/** Rendering handler */
PRIVATE void _rendering_handler(ogl_context context,
                                uint32_t    n_frames_rendered,
                                system_time frame_time,
                                void*       renderer)
{
    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            frame_time);
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
PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

/** TODO */
PRIVATE void _window_closing_callback_handler(system_window window)
{
    ogl_context context = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &context);

    _deinit_gl          (context);
    ogl_pipeline_release(_pipeline);

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
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_size    [2]       = {640, 480};
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               8,  /* depth_buffer_bits       */
                                                               1,  /* n_samples               */
                                                               0); /* stencil_buffer_bits     */

    system_window_get_centered_window_position_for_primary_monitor(window_size,
                                                                   window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_GL,
                                                                            window_x1y1x2y2,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false,
                                                                            false, /* vsync_enabled */
                                                                            true,  /* visible */
                                                                            window_pf);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            30, /* desired_fps */
                                                                            _rendering_handler,
                                                                            NULL);

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                               &_context);
    ogl_context_get_property  (_context,
                               OGL_CONTEXT_PROPERTY_FLYBY,
                              &_flyby);
    system_window_set_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Set up matrices */
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,           /* fov_y */
                                                                               640.0f / 480.0f, /* ar */
                                                                               0.001f,          /* z_near */
                                                                               500.0f);         /* z_far */

    /* Set up callbacks */
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                    (void*) _rendering_lbm_callback_handler,
                                    NULL); /* callback_func_user_arg */
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                    (void*) _window_closed_callback_handler,
                                    NULL);
    system_window_add_callback_func(_window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                    (void*) _window_closing_callback_handler,
                                    NULL);

    /* Initialize flyby */
    const float camera_pos[]          = {-0.1611f, 4.5528f, -6.0926f};
    const float camera_movement_delta = 0.025f;
    const float camera_pitch          = -0.6f;
    const float camera_yaw            = 0.024f;
    const bool  flyby_active          = true;

    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                           camera_pos);
    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_IS_ACTIVE,
                          &flyby_active);
    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_MOVEMENT_DELTA,
                          &camera_movement_delta);
    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_PITCH,
                          &camera_pitch);
    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_YAW,
                          &camera_yaw);

    /* Initialize GL objects */
    ogl_rendering_handler_request_callback_from_context_thread(window_rendering_handler,
                                                               _init_gl,
                                                               NULL); /* user_arg */

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler,
                               0); /* time */

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    /* Deinitialize GL objects */

    system_matrix4x4_release(_projection_matrix);
    system_window_close     (_window);
    system_event_release    (_window_closed_event);

    main_force_deinit();

    return 0;
}