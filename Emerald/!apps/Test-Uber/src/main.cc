/**
 *
 * Kd tree ray casting test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <crtdbg.h>
#include <stdlib.h>
#include "main.h"
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kdtree.h"
#include "ocl/ocl_kernel.h"
#include "ocl/ocl_program.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_skybox.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_uber.h"
#include "scene/scene.h"
#include "sh/sh_projector_combiner.h"
#include "sh/sh_samples.h"
#include "sh_light_editor/sh_light_editor_general.h"
#include "shaders/shaders_fragment_static.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "shaders/shaders_vertex_m_vp_generic.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "system/system_variant.h"

#define ZENITH_LUMINANCE (.0125f)

ocl_context           _cl_context              = NULL;
cl_platform_id        _cl_platform             = NULL;
ogl_context           _context                 = NULL;
mesh*                 _meshes                  = NULL;
uint32_t              _mesh_sh_bands           = 0;
system_matrix4x4      _mvp                     = NULL;
system_matrix4x4*     _mvp_models              = NULL;
system_matrix4x4      _mvp_projection          = NULL;
system_matrix4x4      _mvp_projection_inverted = NULL;
system_matrix4x4      _mvp_view                = NULL;
uint32_t              _n_meshes                = 0;
const float           _plane_near              = 0.05f;
const float           _plane_far               = 750.0f;
const float           _projection_fov_degrees  = 45.0f;
sh_projector_combiner _projector_combiner      = NULL;
sh_samples            _samples                 = NULL;
scene                 _scene                   = NULL;
ogl_skybox            _skybox                  = NULL;
ogl_uber              _uber                    = NULL;
ogl_uber_item_id      _uber_light_item_id      = -1;
system_window         _window                  = NULL;
system_event          _window_closed_event     = system_event_create(true, false);
const uint32_t        _window_height           = 600;
const uint32_t        _window_width            = 800;
GLuint                _vaa_id                  = -1;
system_variant        _variant_float           = NULL;

#define DEG_TO_RAD(x) (x / 360.0f * 2 * 3.14152965f)


/** TODO */
static void _on_right_button_up(void* arg)
{
    system_event_set(_window_closed_event);
}

/** Rendering handler */
void _rendering_handler(ogl_context context, uint32_t n_frames_rendered, system_timeline_time frame_time, void* renderer)
{
    const ocl_11_entrypoints*                                 cl_entry_points            = ocl_get_entrypoints();
    const ocl_khr_gl_sharing_entrypoints*                     cl_gl_sharing_entry_points = ocl_get_khr_gl_sharing_entrypoints();
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points           = NULL;
    const ogl_context_gl_entrypoints*                         entry_points               = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    ogl_flyby_update         (_context);
    ogl_flyby_get_view_matrix(_context, _mvp_view);

    if (_vaa_id == -1)
    {
        entry_points->pGLGenVertexArrays(1, &_vaa_id);

        /* Set up SH */
        if (_mesh_sh_bands != 0)
        {
            /* Initialize SH samples */
            _samples = sh_samples_create(_context, 100 * 100, _mesh_sh_bands, system_hashed_ansi_string_create("SH samples") );

            sh_samples_execute(_samples);

            /* Initialize SH Light projection */
            sh_projection_id projection_id          = 0;
            curve_container  zenith_luminance_curve = NULL;

            _projector_combiner = sh_projector_combiner_create(_context, SH_COMPONENTS_RGB, _samples, system_hashed_ansi_string_create("SH projector"));

            sh_projector_combiner_add_sh_projection                 (_projector_combiner, SH_PROJECTION_TYPE_CIE_OVERCAST, &projection_id);
            sh_projector_combiner_get_sh_projection_property_details(_projector_combiner, projection_id,                   SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE, &zenith_luminance_curve);

            system_variant_set_float         (_variant_float,         ZENITH_LUMINANCE);
            curve_container_set_default_value(zenith_luminance_curve, 0, _variant_float);

            ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Error initializing SH data");
        }

        if (_skybox == NULL)
        {
            _skybox = ogl_skybox_create_light_projection_sh(_context, _samples, system_hashed_ansi_string_create("SH skybox") );
        }

        /* Other */
        entry_points->pGLFrontFace(GL_CCW);
    }

    /* Calculate SH data */
    sh_projector_combiner_execute(_projector_combiner, 0);

    entry_points->pGLBindVertexArray(_vaa_id);

    entry_points->pGLClearColor     (1.0f, 0.25f, 0.0f, 1.0f);
    entry_points->pGLClear          (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLEnable         (GL_DEPTH_TEST);
    //entry_points->pGLEnable         (GL_CULL_FACE);

    /* Get MVP matrix */
    _mvp = system_matrix4x4_create_by_mul(_mvp_projection, _mvp_view);

    /* Render the mesh - initialize SH data */
    const float*             camera_location = ogl_flyby_get_camera_location(_context);
    const float              light_color[]   = {0.85f, 0.85f, 0.85f};
    _ogl_uber_light_sh_data  light_sh_data;
    _ogl_uber_light_sh_data* light_sh_data_ptr = _mesh_sh_bands != 0 ? &light_sh_data : NULL;
    ogl_texture              light_sh_data_tbo = 0;

    sh_projector_combiner_get_sh_projection_result_bo_data_details(_projector_combiner, &light_sh_data.bo_id, &light_sh_data.bo_offset, NULL, &light_sh_data_tbo);

     /* Draw the skybox */
    ogl_skybox_draw(_skybox, light_sh_data_tbo, _mvp_view, _mvp_projection_inverted);

    /* Render the mesh */
    ogl_uber_set_shader_general_property(_uber,                      OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,      camera_location);
    ogl_uber_set_shader_general_property(_uber,                      OGL_UBER_GENERAL_PROPERTY_VP,                   _mvp);
    ogl_uber_set_shader_item_property   (_uber, _uber_light_item_id, OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION, camera_location);
    ogl_uber_set_shader_item_property   (_uber, _uber_light_item_id, OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA,   &light_sh_data);

    ogl_uber_rendering_start(_uber);
    {
        /* Occlusion query-based rendering: phase one */
        entry_points->pGLDepthFunc(GL_LEQUAL);

        entry_points->pGLEnable(GL_RASTERIZER_DISCARD);
        {
            ogl_uber_rendering_render(_uber,
                                      _n_meshes,
                                      _mvp_models,
                                      _meshes,
                                      _scene);
        }
        entry_points->pGLDisable(GL_RASTERIZER_DISCARD);

        /* Occlusion query-based rendering: phase two */
        ogl_uber_rendering_render(_uber,
                                  _n_meshes,
                                  _mvp_models,
                                  _meshes,
                                  _scene);
    }
    ogl_uber_rendering_stop(_uber);

    /* Release MVP matrix */
    system_matrix4x4_release(_mvp);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Rendering error.");
}


/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
{
    ogl_rendering_handler window_rendering_handler;
    int                   window_size    [2] = {_window_width, _window_height};
    int                   window_x1y1x2y2[4] = {0};
    
    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(window_size, window_x1y1x2y2);
    
    _window                  = system_window_create_not_fullscreen         (window_x1y1x2y2, system_hashed_ansi_string_create("Test window"), false, 0, false, false, true);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"), 60, _rendering_handler, NULL);

    bool context_result = system_window_get_context(_window, &_context);
    ASSERT_DEBUG_SYNC(context_result, "Could not retrieve OGL context");

    system_window_add_callback_func    (_window, SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL, SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP, _on_right_button_up, NULL);
    system_window_set_rendering_handler(_window, window_rendering_handler);

    ogl_rendering_handler_set_fps_counter_visibility(window_rendering_handler, true);

    /* Create some helper objects */
    _variant_float = system_variant_create(SYSTEM_VARIANT_FLOAT);

    /* Load the scene */    
    _scene    = scene_load                (_context, system_hashed_ansi_string_create("test.scene") );
    _n_meshes = scene_get_amount_of_meshes(_scene);
    _meshes   = new (std::nothrow) mesh[_n_meshes];

    for (uint32_t n_mesh = 0; n_mesh < _n_meshes; ++n_mesh)
    {
        _meshes[n_mesh] = scene_get_mesh_by_index(_scene, n_mesh);
    }

    ASSERT_DEBUG_SYNC(_meshes != NULL, "Mesh is NULL");

    /* Instantiate uber program */
    mesh_get_property(_meshes[0], MESH_PROPERTY_N_SH_BANDS, &_mesh_sh_bands);

    shaders_fragment_uber_diffuse  diffuse_uber_light  = (_mesh_sh_bands == 0 ? UBER_DIFFUSE_LAMBERT : _mesh_sh_bands == 3 ? UBER_DIFFUSE_LIGHT_PROJECTION_SH3 : UBER_DIFFUSE_LIGHT_PROJECTION_SH4);
    shaders_fragment_uber_specular specular_uber_light = UBER_SPECULAR_PHONG;

    _uber               = ogl_uber_create(_context, system_hashed_ansi_string_create("Uber program") );
    _uber_light_item_id = ogl_uber_add_light_item(_uber, diffuse_uber_light, specular_uber_light);

    /* Matrices */
    _mvp_projection          = system_matrix4x4_create_perspective_projection_matrix(_projection_fov_degrees, float(window_size[0]) / float(window_size[1]), _plane_near, _plane_far);
    _mvp_projection_inverted = system_matrix4x4_create();
    _mvp_view                = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4(_mvp_projection_inverted, _mvp_projection);
    system_matrix4x4_invert            (_mvp_projection_inverted);

    _mvp_models = new (std::nothrow) system_matrix4x4[_n_meshes];
    
    for (uint32_t n = 0; n < _n_meshes; ++n)
    {
        _mvp_models[n] = system_matrix4x4_create();

        system_matrix4x4_set_to_identity(_mvp_models[n]);
    }

    /* Set up fly-by support */
    const float camera_pos[] = {0, 0, -40};

    ogl_flyby_activate          (_context, camera_pos);
    ogl_flyby_set_movement_delta(_context, 0.0125f);

    /* Set up OpenCL */
    ocl_get_platform_id(0, &_cl_platform);

    _cl_context = ocl_context_create_gpu_only_with_gl_sharing(_cl_platform, _context, system_hashed_ansi_string_create("CL context") );    

    /* Show the light editor */
    sh_light_editor_show(_context);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler, 0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    sh_light_editor_hide();

    for (uint32_t n = 0; n < _n_meshes; ++n)
    {
        system_matrix4x4_release(_mvp_models[n]);
    }
    delete [] _mvp_models;

    system_matrix4x4_release(_mvp_projection);
    system_matrix4x4_release(_mvp_view);
    ogl_uber_release        (_uber);
    ogl_skybox_release      (_skybox);

    if (_samples != NULL)
    {
        sh_samples_release(_samples);
    }

    if (_projector_combiner != NULL)
    {
        sh_projector_combiner_release(_projector_combiner);
    }

    ocl_context_release   (_cl_context);
    scene_release         (_scene);
    system_window_close   (_window);
    system_event_release  (_window_closed_event);
    system_variant_release(_variant_float);

    main_force_deinit();

    return 0;
}