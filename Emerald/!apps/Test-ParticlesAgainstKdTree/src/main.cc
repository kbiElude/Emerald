/**
 *
 * Pipeline test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "mesh/mesh.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kdtree.h"
#include "ocl/ocl_types.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_checkbox.h"
#include "scene/scene.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "stage_step_mesh.h"
#include "stage_step_particles.h"

INCLUDE_OPTIMUS_SUPPORT;

ocl_context            _cl_context                      = NULL;
cl_platform_id         _cl_platform_id                  = (cl_platform_id) NULL;
ogl_context            _gl_context                      = NULL;
ogl_pipeline           _pipeline                        = NULL;
system_matrix4x4       _projection_matrix               = NULL;
scene                  _scene                           = NULL;
ocl_kdtree             _scene_kdtree                    = NULL;
mesh                   _scene_mesh                      = NULL;
uint32_t               _stage_id                        = -1;
ogl_ui_control         _ui_toggle_debug_checkbox        = NULL;
system_window          _window                          = NULL;
system_event           _window_closed_event             = system_event_create(true, false);
const int              _window_size[2]                  = {640, 480};

/* GL deinitialization */
PRIVATE void _deinit_gl(ogl_context context, void* arg)
{
    stage_step_mesh_deinit     (context, _pipeline);
    stage_step_particles_deinit(context, _pipeline);
}

/** TODO */
PRIVATE void _get_decay_value(void* user_arg, system_variant result)
{
    system_variant_set_float(result, stage_step_particles_get_decay() );
}

/** TODO */
PRIVATE void _get_dt_value(void* user_arg, system_variant result)
{
    system_variant_set_float(result, stage_step_particles_get_dt() );
}

/** TODO */
PRIVATE void _get_gravity_value(void* user_arg, system_variant result)
{
    system_variant_set_float(result, stage_step_particles_get_gravity() );
}

/** TODO */
PRIVATE void _get_minimum_mass_value(void* user_arg, system_variant result)
{
    system_variant_set_float(result, stage_step_particles_get_minimum_mass() );
}

/** TODO */
PRIVATE void _get_spread_value(void* user_arg, system_variant result)
{
    system_variant_set_float(result, stage_step_particles_get_spread() );
}

/* GL initialization */
PRIVATE void _init_gl(ogl_context context, void* arg)
{
    stage_step_mesh_init     (context, _pipeline, _stage_id);
    stage_step_particles_init(context, _pipeline, _stage_id);
}

/** TODO */
PRIVATE void _iterate_frame(void* not_used)
{
    /* This function is called from a worker thread */
    stage_step_particles_iterate_frame();
}

/** Rendering handler */
void _rendering_handler(ogl_context context, uint32_t n_frames_rendered, system_timeline_time frame_time, void* renderer)
{
    ogl_pipeline_draw_stage(_pipeline, _stage_id, frame_time);
}

void _rendering_lbm_callback_handler(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void*)
{
    system_event_set(_window_closed_event);
}

PRIVATE bool _rendering_rbm_callback_handler(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _reset_particles(void* not_used)
{
    /* This function is called from a worker thread. */
    stage_step_particles_reset();
}

/** TODO */
PRIVATE void _set_decay_value(void* user_arg, system_variant new_value)
{
    float value;

    system_variant_get_float      (new_value, &value);
    stage_step_particles_set_decay(value);
}

/** TODO */
PRIVATE void _set_dt_value(void* user_arg, system_variant new_value)
{
    float value;

    system_variant_get_float   (new_value, &value);
    stage_step_particles_set_dt(value);
}

/** TODO */
PRIVATE void _set_gravity_value(void* user_arg, system_variant new_value)
{
    float value;

    system_variant_get_float        (new_value, &value);
    stage_step_particles_set_gravity(value);
}

/** TODO */
PRIVATE void _set_minimum_mass_value(void* user_arg, system_variant new_value)
{
    float value;

    system_variant_get_float             (new_value, &value);
    stage_step_particles_set_minimum_mass(value);
}

/** TODO */
PRIVATE void _set_spread_value(void* user_arg, system_variant new_value)
{
    float value;

    system_variant_get_float       (new_value, &value);
    stage_step_particles_set_spread(value);
}

PRIVATE void _toggle_debug(void* not_used)
{
    /* This function is called from a worker thread. */
    bool check_status = false;

    ogl_ui_get_property(_ui_toggle_debug_checkbox,
                        OGL_UI_CHECKBOX_PROPERTY_CHECK_STATUS,
                       &check_status);

    stage_step_particles_set_debug(check_status);
}

/** Please see header for specification */
PUBLIC ocl_context main_get_ocl_context()
{
    return _cl_context;
}

/** Please see header for specification */
PUBLIC system_matrix4x4 main_get_projection_matrix()
{
    return _projection_matrix;
}

/** Please see header for specification */
PUBLIC scene main_get_scene()
{
    return _scene;
}

/** Please see header for specification */
PUBLIC ocl_kdtree main_get_scene_kdtree()
{
    return _scene_kdtree;
}

/** Please see header for specification */
PUBLIC mesh main_get_scene_mesh()
{
    return _scene_mesh;
}

/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
{
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(_window_size, window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (window_x1y1x2y2, system_hashed_ansi_string_create("Test window"), false, 0, false, false, true);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"), 60, _rendering_handler, NULL);
    context_result           = system_window_get_context(_window, &_gl_context);

    ASSERT_DEBUG_SYNC(context_result, "Could not retrieve OGL context");

    system_window_set_rendering_handler(_window, window_rendering_handler);
    system_window_add_callback_func    (_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,  _rendering_rbm_callback_handler, NULL);

    /* Initialize OpenCL - required for intersections */
    if (!ocl_get_platform_id(0, &_cl_platform_id) )
    {
        LOG_FATAL("Cannot retrieve OpenCL platform ID - crash imminent");
    }

    _cl_context = ocl_context_create_gpu_only_with_gl_sharing(_cl_platform_id, _gl_context, system_hashed_ansi_string_create("CL context") );

    if (_cl_context == NULL)
    {
        LOG_FATAL("Could not create OpenCL context - crash imminent");
    }

    /* Create and configure pipeline object */
    _pipeline = ogl_pipeline_create   (_gl_context, true, system_hashed_ansi_string_create("pipeline") );
    _stage_id = ogl_pipeline_add_stage(_pipeline);

    /* Set up UI */
    const float button1_x1y1[]     = {0.8f, 0.1f};
    const float button3_x1y1[]     = {0.8f, 0.2f};
    const float scrollbar_1_x1y1[] = {0.8f, 0.3f};
    const float scrollbar_2_x1y1[] = {0.8f, 0.4f};
    const float scrollbar_3_x1y1[] = {0.8f, 0.5f};
    const float scrollbar_4_x1y1[] = {0.8f, 0.6f};
    const float scrollbar_5_x1y1[] = {0.8f, 0.7f};
    const float scrollbar_6_x1y1[] = {0.8f, 0.8f};

    ogl_ui      pipeline_ui    = ogl_pipeline_get_ui(_pipeline);

                                ogl_ui_add_button  (pipeline_ui, system_hashed_ansi_string_create("Reset"),      button1_x1y1,                                   _reset_particles, NULL);
    _ui_toggle_debug_checkbox = ogl_ui_add_checkbox(pipeline_ui, system_hashed_ansi_string_create("Debug"),      button3_x1y1, stage_step_particles_get_debug(), _toggle_debug,    NULL);

    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("Spread"),             system_variant_create_float(0.1f),          system_variant_create_float(4.0f),        scrollbar_1_x1y1, _get_spread_value,       NULL, _set_spread_value,       NULL);
    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("Gravity"),            system_variant_create_float(0.01f),         system_variant_create_float(4.0f),         scrollbar_2_x1y1, _get_gravity_value,      NULL, _set_gravity_value,      NULL);
    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("Minimum mass"),       system_variant_create_float(0.0001f),       system_variant_create_float(50.0f),        scrollbar_3_x1y1, _get_minimum_mass_value, NULL, _set_minimum_mass_value, NULL);
    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("Maximum mass delta"), system_variant_create_float(0.0001f),       system_variant_create_float(40.0f),        scrollbar_4_x1y1, _get_minimum_mass_value, NULL, _set_minimum_mass_value, NULL);
    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("Decay"),              system_variant_create_float(0.995f),        system_variant_create_float(1.005f),       scrollbar_5_x1y1, _get_decay_value,        NULL, _set_decay_value,        NULL);
    ogl_ui_add_scrollbar(pipeline_ui, system_hashed_ansi_string_create("dt"),                 system_variant_create_float(1.0f / 500.0f), system_variant_create_float(1.0f / 30.0f), scrollbar_6_x1y1, _get_dt_value,           NULL, _set_dt_value,           NULL);

    /* Initialize flyby */
    const float camera_pos[] = {-0.1611f, 4.5528f, -6.0926f};

    ogl_flyby_activate          (_gl_context, camera_pos);
    ogl_flyby_set_movement_delta(_gl_context,  0.025f);
    ogl_flyby_set_pitch_yaw     (_gl_context, -0.6f, 0.024f);

    /* Set up projection matrix */
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f, float(_window_size[0]) / float(_window_size[1]), 0.001f, 500.0f);

    /* Load the scene */
    _scene        = scene_load             (_gl_context, system_hashed_ansi_string_create("sphere_on_plane") );
    _scene_kdtree = ocl_kdtree_load        (_cl_context, system_hashed_ansi_string_create("sphere_on_plane_kdtree"), OCL_KDTREE_USAGE_MANY_LOCATIONS_ONE_RAY);
    _scene_mesh   = scene_get_mesh_by_index(_scene, 0);

    /* Initialize GL objects */
    ogl_rendering_handler_request_callback_from_context_thread(window_rendering_handler,
                                                               _init_gl,
                                                               NULL);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler, 0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    ogl_rendering_handler_request_callback_from_context_thread(window_rendering_handler,
                                                               _deinit_gl,
                                                               NULL);

    ocl_context_release (_cl_context);
    scene_release       (_scene);
    ocl_kdtree_release  (_scene_kdtree);
    ogl_pipeline_release(_pipeline);
    system_window_close (_window);
    system_event_release(_window_closed_event);

    /* Avoid leaking system resources */
    main_force_deinit();

    return 0;
}