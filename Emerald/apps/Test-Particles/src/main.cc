/**
 *
 * Particles test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_ui.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_window.h"
#include "stage_particle.h"

INCLUDE_OPTIMUS_SUPPORT;


ogl_context   _context             = NULL;
ogl_pipeline  _pipeline            = NULL;
system_window _window              = NULL;
system_event  _window_closed_event = system_event_create(true); /* manual_reset */
int           _window_size[2]      = {0};

/* Forward declarations */
PRIVATE void _deinit_gl                     (ogl_context             context);
PRIVATE void _get_decay_value               (void*                   user_arg,
                                             system_variant          result);
PRIVATE void _get_dt_value                  (void*                   user_arg,
                                             system_variant          result);
PRIVATE void _get_gravity_value             (void*                   user_arg,
                                             system_variant          result);
PRIVATE void _get_minimum_mass_value        (void*                   user_arg,
                                             system_variant          result);
PRIVATE void _get_spread_value              (void*                   user_arg,
                                             system_variant          result);
PRIVATE void _init_gl                       (ogl_context             context,
                                             void*                   arg);
PRIVATE void _rendering_handler             (ogl_context             context,
                                             uint32_t                n_frames_rendered,
                                             system_timeline_time    frame_time,
                                             void*                   renderer);
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*);
PRIVATE void _reset_particles               (void*                   not_used,
                                             void*                   not_used2);
PRIVATE void _set_decay_value               (void*                   user_arg,
                                             system_variant          new_value);
PRIVATE void _set_dt_value                  (void*                   user_arg,
                                             system_variant          new_value);
PRIVATE void _set_gravity_value             (void*                   user_arg,
                                             system_variant          new_value);
PRIVATE void _set_minimum_mass_value        (void*                   user_arg,
                                             system_variant          new_value);
PRIVATE void _set_spread_value              (void*                   user_arg,
                                             system_variant          new_value);


/* GL deinitialization */
PRIVATE void _deinit_gl(ogl_context context)
{
    stage_particle_deinit(context,
                          _pipeline);
}

/** TODO */
PRIVATE void _get_decay_value(void*          user_arg,
                              system_variant result)
{
    system_variant_set_float(result,
                             stage_particle_get_decay() );
}

/** TODO */
PRIVATE void _get_dt_value(void*          user_arg,
                           system_variant result)
{
    system_variant_set_float(result,
                             stage_particle_get_dt() );
}

/** TODO */
PRIVATE void _get_gravity_value(void*          user_arg,
                                system_variant result)
{
    system_variant_set_float(result,
                             stage_particle_get_gravity() );
}

/** TODO */
PRIVATE void _get_minimum_mass_value(void*          user_arg,
                                     system_variant result)
{
    system_variant_set_float(result,
                             stage_particle_get_minimum_mass() );
}

/** TODO */
PRIVATE void _get_spread_value(void*          user_arg,
                               system_variant result)
{
    system_variant_set_float(result,
                             stage_particle_get_spread() );
}

/* GL initialization */
PRIVATE void _init_gl(ogl_context context,
                      void*       arg)
{
        /* Create and configure pipeline object */
    _pipeline = ogl_pipeline_create(_context,
                                    true, /* should_overlay_perf_info */
                                    system_hashed_ansi_string_create("pipeline") );

    /* Set up UI */
    const float button_x1y1[]      = {0.8f, 0.1f};
    const float scrollbar_1_x1y1[] = {0.8f, 0.2f};
    const float scrollbar_2_x1y1[] = {0.8f, 0.3f};
    const float scrollbar_3_x1y1[] = {0.8f, 0.4f};
    const float scrollbar_4_x1y1[] = {0.8f, 0.5f};
    const float scrollbar_5_x1y1[] = {0.8f, 0.6f};
    const float scrollbar_6_x1y1[] = {0.8f, 0.7f};
    ogl_ui      pipeline_ui        = ogl_pipeline_get_ui(_pipeline);

    ogl_ui_add_button   (pipeline_ui,
                         system_hashed_ansi_string_create("Reset"),
                         button_x1y1,
                         _reset_particles,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Spread"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.1f),
                         system_variant_create_float(75.0f),
                         scrollbar_1_x1y1,
                         _get_spread_value,
                         NULL,
                         _set_spread_value,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Gravity"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.01f),
                         system_variant_create_float(4.0f),
                         scrollbar_2_x1y1,
                         _get_gravity_value,
                         NULL,
                         _set_gravity_value,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Minimum mass"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0001f),
                         system_variant_create_float(10.0f),
                         scrollbar_3_x1y1,
                         _get_minimum_mass_value,
                         NULL,
                         _set_minimum_mass_value,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Maximum mass delta"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.0001f),
                         system_variant_create_float(40.0f),
                         scrollbar_4_x1y1,
                         _get_minimum_mass_value,
                         NULL,
                         _set_minimum_mass_value,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Decay"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(0.995f),
                         system_variant_create_float(1.005f),
                         scrollbar_5_x1y1,
                         _get_decay_value,
                         NULL,
                         _set_decay_value,
                         NULL);
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("dt"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float(1.0f / 500.0f),
                         system_variant_create_float(1.0f / 30.0f),
                         scrollbar_6_x1y1,
                         _get_dt_value,
                         NULL,
                         _set_dt_value,
                         NULL);

    stage_particle_init(context,
                        _pipeline);
}

/** Rendering handler */
PRIVATE void _rendering_handler(ogl_context          context,
                                uint32_t             n_frames_rendered,
                                system_timeline_time frame_time,
                                void*                renderer)
{
    ogl_pipeline_draw_stage(_pipeline,
                            stage_particle_get_stage_id(),
                            frame_time);
}

PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _reset_particles(void* not_used,
                              void* not_used2)
{
    /* This function is called from a worker thread. */
    stage_particle_reset();
}

/** TODO */
PRIVATE void _set_decay_value(void*          user_arg,
                              system_variant new_value)
{
    float value;

    system_variant_get_float(new_value,
                            &value);
    stage_particle_set_decay(value);
}

/** TODO */
PRIVATE void _set_dt_value(void*          user_arg,
                           system_variant new_value)
{
    float value;

    system_variant_get_float(new_value,
                            &value);
    stage_particle_set_dt   (value);
}

/** TODO */
PRIVATE void _set_gravity_value(void*          user_arg,
                                system_variant new_value)
{
    float value;

    system_variant_get_float  (new_value,
                              &value);
    stage_particle_set_gravity(value);
}

/** TODO */
PRIVATE void _set_minimum_mass_value(void*          user_arg,
                                     system_variant new_value)
{
    float value;

    system_variant_get_float       (new_value,
                                   &value);
    stage_particle_set_minimum_mass(value);
}

/** TODO */
PRIVATE void _set_spread_value(void*          user_arg,
                               system_variant new_value)
{
    float value;

    system_variant_get_float (new_value,
                             &value);
    stage_particle_set_spread(value);
}

/** "Window closed" call-back handler */
PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

/** "Window closing" call-back handler */
PRIVATE void _window_closing_callback_handler(system_window window)
{
    ogl_context context = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &context);

    _deinit_gl          (context);
    ogl_pipeline_release(_pipeline);
}


/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle,
                   HINSTANCE,
                   LPTSTR,
                   int)
{
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    _window_size[0] = 640;
    _window_size[1] = 480;

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_GL,
                                                                            window_x1y1x2y2,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false,
                                                                            0,
                                                                            false,
                                                                            false,
                                                                            true);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL);

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);

    system_window_set_rendering_handler(_window,
                                        window_rendering_handler);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        _rendering_rbm_callback_handler,
                                        NULL); /* callback_func_user_arg */
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        _window_closing_callback_handler,
                                        NULL);

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

    system_window_close (_window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}