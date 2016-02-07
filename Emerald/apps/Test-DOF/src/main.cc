/**
 *
 * DOF test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "demo/demo_app.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_ui.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "system/system_variant.h"
#include "stage_step_background.h"
#include "stage_step_dof_scheuermann.h"
#include "stage_step_julia.h"
#include "stage_step_preview.h"


INCLUDE_OPTIMUS_SUPPORT;

float            _blur_radius               = 0.8f;
ral_context      _context                   = NULL;
float            _data[4]                   = {.17995f, -0.66f, -0.239f, -0.210f};
float            _dof_cutoff                = 0.75f;
float            _dof_far_plane_depth       = 5.4f;
float            _dof_focal_plane_depth     = 3.79f;
float            _dof_near_plane_depth      = 4.0f;
float            _epsilon                   = 0.001f;
float            _escape                    = 1.2f * 1.5f;
demo_flyby       _flyby                     = NULL;
float            _fresnel_reflectance       = 0.028f;
float            _light_color[3]            = {1.0f,  1.0f,   1.0f};
float            _light_position[3]         = {2.76f, 1.619f, 0.0f};
float            _max_coc_px                = 5.0f;
int              _max_iterations            = 6;
ogl_pipeline     _pipeline                  = NULL;
uint32_t         _pipeline_stage_id         = -1;
system_matrix4x4 _projection_matrix         = NULL;
float            _raycast_radius_multiplier = 2.65f;
float            _reflectivity              = 0.2f;
bool             _shadows                   = true;
float            _specularity               = 4.4f;
demo_window      _window                    = NULL;
system_event     _window_closed_event       = system_event_create(true); /* manual_reset */
int              _window_resolution[2]      = {1280, 720};

PRIVATE void _get_blur_radius(void*          user_arg,
                              system_variant result)
{
    system_variant_set_float(result,
                             _blur_radius);
}

PRIVATE void _get_dof_cutoff(void*          user_arg,
                             system_variant result)
{
    system_variant_set_float(result,
                             _dof_cutoff);
}

PRIVATE void _get_dof_far_plane_depth(void*          user_arg,
                                      system_variant result)
{
    system_variant_set_float(result,
                             _dof_far_plane_depth);
}

PRIVATE void _get_dof_focal_plane_depth(void*          user_arg,
                                        system_variant result)
{
    system_variant_set_float(result,
                             _dof_focal_plane_depth);
}

PRIVATE void _get_dof_near_plane_depth(void*          user_arg,
                                       system_variant result)
{
    system_variant_set_float(result,
                             _dof_near_plane_depth);
}

PRIVATE void _get_fresnel_reflectance(void*          user_arg,
                                      system_variant result)
{
    system_variant_set_float(result,
                             _fresnel_reflectance);
}

PRIVATE void _get_max_coc_px(void*          user_arg,
                             system_variant result)
{
    system_variant_set_float(result,
                             _max_coc_px);
}

PRIVATE void _get_reflectivity(void*          user_arg,
                               system_variant result)
{
    system_variant_set_float(result,
                             _reflectivity);
}

PRIVATE void _set_blur_radius(void*          user_arg,
                              system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_blur_radius);
}

PRIVATE void _set_dof_cutoff(void*          user_arg,
                             system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_dof_cutoff);
}

PRIVATE void _set_dof_far_plane_depth(void*          user_arg,
                                      system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_dof_far_plane_depth);
}

PRIVATE void _set_dof_focal_plane_depth(void*          user_arg,
                                        system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_dof_focal_plane_depth);
}

PRIVATE void _set_dof_near_plane_depth(void*          user_arg,
                                       system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_dof_near_plane_depth);
}

PRIVATE void _set_fresnel_reflectance(void*          user_arg,
                                      system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_fresnel_reflectance);
}

PRIVATE void _set_max_coc_px(void*          user_arg,
                             system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_max_coc_px);
}

PRIVATE void _set_reflectivity(void*          user_arg,
                               system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_reflectivity);
}


void _deinit_gl(ral_context context)
{
    stage_step_background_deinit     (_context);
    stage_step_julia_deinit          (_context);
    stage_step_dof_scheuermann_deinit(_context);
    stage_step_preview_deinit        (_context);
}

void _init_gl(ogl_context context,
              void*       not_used)
{
    _pipeline          = ogl_pipeline_create   (_context,
                                                true, /* should_overlay_performance_info */
                                                system_hashed_ansi_string_create("pipeline") );
    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    stage_step_background_init     (_context,
                                    _pipeline,
                                    _pipeline_stage_id);
    stage_step_julia_init          (_context,
                                    _pipeline,
                                    _pipeline_stage_id);
    stage_step_dof_scheuermann_init(_context,
                                    _pipeline,
                                    _pipeline_stage_id);
    stage_step_preview_init        (_context,
                                    _pipeline,
                                    _pipeline_stage_id);

    /* Initialize flyby */
    const float camera_movement_delta =  0.025f;
    const float camera_pitch          = -0.6f;
    const float camera_pos[]          = {-0.1611f, 4.5528f, -6.0926f};
    const float camera_yaw            =  0.024f;
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

    /* Initialize UI */
    const float scrollbar_1_x1y1[]         = {0.8f, 0.0f};
    const float scrollbar_2_x1y1[]         = {0.8f, 0.1f};
    const float scrollbar_3_x1y1[]         = {0.8f, 0.2f};
    const float scrollbar_4_x1y1[]         = {0.8f, 0.3f};
    const float scrollbar_5_x1y1[]         = {0.8f, 0.4f};
    const float scrollbar_6_x1y1[]         = {0.8f, 0.5f};
    const float scrollbar_7_x1y1[]         = {0.8f, 0.6f};
    const float scrollbar_8_x1y1[]         = {0.8f, 0.7f};
    const float texture_preview_max_size[] = {0.3f, 0.05f};
    const float texture_preview_x1y1[]     = {0.6f, 0.1f};
    ogl_ui      pipeline_ui                = ogl_pipeline_get_ui(_pipeline);

    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("DOF cut-off"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (1.0f),
                         scrollbar_1_x1y1,
                         _get_dof_cutoff,
                         NULL,             /* pfn_get_current_value_ptr */
                         _set_dof_cutoff,
                         NULL);            /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui, 
                         system_hashed_ansi_string_create("DOF far plane depth"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (10.0f),
                         scrollbar_2_x1y1,
                         _get_dof_far_plane_depth,
                         NULL,                    /* pfn_get_current_value_ptr */
                         _set_dof_far_plane_depth,
                         NULL);                   /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui, 
                         system_hashed_ansi_string_create("DOF focal plane depth"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (10.0f),
                         scrollbar_3_x1y1,
                         _get_dof_focal_plane_depth,
                         NULL,                       /* pfn_get_current_value_ptr */
                         _set_dof_focal_plane_depth,
                         NULL);                      /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("DOF near plane depth"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (10.0f),
                         scrollbar_4_x1y1,
                         _get_dof_near_plane_depth,
                         NULL,                     /* pfn_get_current_value_ptr */
                         _set_dof_near_plane_depth,
                         NULL);                    /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Max CoC size (px)"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (1.0f),
                         system_variant_create_float     (20.0f),
                         scrollbar_5_x1y1,
                         _get_max_coc_px,
                         NULL,            /* pfn_get_current_value_ptr */
                         _set_max_coc_px,
                         NULL);           /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Blur radius"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.01f),
                         system_variant_create_float     (2.0f),
                         scrollbar_6_x1y1,
                         _get_blur_radius,
                         NULL,            /* pfn_get_current_value_ptr */
                         _set_blur_radius,
                         NULL);           /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Reflectivity"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (1.0f),
                         scrollbar_7_x1y1,
                         _get_reflectivity,
                         NULL,             /* pfn_get_current_value_ptr */
                         _set_reflectivity,
                         NULL);            /* pfn_set_current_value_ptr */
    ogl_ui_add_scrollbar(pipeline_ui,
                         system_hashed_ansi_string_create("Fresnel reflectance"),
                         OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                         system_variant_create_float     (0.0f),
                         system_variant_create_float     (1.0f),
                         scrollbar_8_x1y1,
                         _get_fresnel_reflectance,
                         NULL,                     /* pfn_get_current_value_ptr */
                         _set_fresnel_reflectance,
                         NULL);                    /* pfn_set_current_value_ptr */
}

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       renderer)
{
    ral_framebuffer                   default_fbo         = NULL;
    raGL_framebuffer                  default_fbo_raGL    = NULL;
    GLuint                            default_fbo_raGL_id = 0;
    const ogl_context_gl_entrypoints* entry_points        = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                            &default_fbo);

    default_fbo_raGL = ral_context_get_framebuffer_gl(_context,
                                                      default_fbo);

    raGL_framebuffer_get_property(default_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &default_fbo_raGL_id);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLClearColor     (0, /* red */
                                     0, /* green */
                                     0, /* blue */
                                     1); /* alpha */
    entry_points->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                     default_fbo_raGL_id);
    entry_points->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            n_frames_rendered,
                            frame_time,
                            rendering_area_px_topdown);
}

void _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

/** "Window closed" call-back handler */
PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

/** "Window closing" call-back handler */
PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    ral_context context = NULL;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT_RAL,
                              &context);

    _deinit_gl          (context);
    ogl_pipeline_release(_pipeline);
}

/** Please see header for specification */
float main_get_blur_radius()
{
    return _blur_radius;
}

/** Please see header for specification */
const float* main_get_data_vector()
{
    return _data;
}

/** Please see header for specification */
float main_get_dof_cutoff()
{
    return _dof_cutoff;
}

/** Please see header for specification */
float main_get_dof_far_plane_depth()
{
    return _dof_far_plane_depth;
}

/** Please see header for specification */
float main_get_dof_focal_plane_depth()
{
    return _dof_focal_plane_depth;
}

/** Please see header for specification */
float main_get_dof_near_plane_depth()
{
    return _dof_near_plane_depth;
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
float main_get_fresnel_reflectance()
{
    return _fresnel_reflectance;
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
float main_get_max_coc_px()
{
    return _max_coc_px;
}

/** Please see header for specification */
const int* main_get_output_resolution()
{
    return _window_resolution;
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
float main_get_reflectivity()
{
    return _reflectivity;
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

/** Please see header for specification */
unsigned int main_get_window_height()
{
    return _window_resolution[1];
}

/** Please see header for specification */
unsigned int main_get_window_width()
{
    return _window_resolution[0];
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
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ogl_rendering_handler                   rendering_handler  = NULL;
    system_screen_mode                      screen_mode        = NULL;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Compute shader SSBO test app");
    int                                     window_x1y1x2y2[4] = {0};

    _window = demo_app_create_window(window_name,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_set_property(_window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             _window_resolution);

    demo_window_show(_window);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    /* Set up matrices */
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f, /* fov_y */
                                                                               float(_window_resolution[0]) / float(_window_resolution[1]),
                                                                               0.01f,  /* z_near */
                                                                               100.0f);/* z_far */

    /* Set up callbacks */
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
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

    /* Initialize GL objects */
    ogl_rendering_handler_request_callback_from_context_thread(rendering_handler,
                                                               _init_gl,
                                                               NULL); /* user_arg */

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    system_matrix4x4_release(_projection_matrix);
    demo_app_destroy_window (window_name);
    system_event_release    (_window_closed_event);

    /* Avoid leaking system resources */
    main_force_deinit();

    return 0;
}