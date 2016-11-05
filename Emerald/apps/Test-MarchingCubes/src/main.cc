/**
 *
 * Marching cubes test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "demo/demo_app.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "mesh/mesh.h"
#include "mesh/mesh_marchingcubes.h"
#include "procedural/procedural_uv_generator.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scalar_field/scalar_field_metaballs.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene/scene_material.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_screen_mode.h"
#include "system/system_variant.h"
#include "ui/ui.h"
#include "ui/ui_scrollbar.h"
#include <algorithm>
#include "main.h"

PRIVATE const unsigned int _blob_size[] = {50, 50, 50};

PRIVATE ral_context                       _context                = nullptr;
PRIVATE demo_flyby                        _flyby                  = nullptr;
PRIVATE mesh_marchingcubes                _marching_cubes         = nullptr;
PRIVATE scene_material                    _material               = nullptr;
PRIVATE scalar_field_metaballs            _metaballs              = nullptr;
PRIVATE system_matrix4x4                  _projection_matrix      = nullptr;
PRIVATE ral_texture                       _rt_color               = nullptr;
PRIVATE ral_texture_view                  _rt_color_view          = nullptr;
PRIVATE ral_texture                       _rt_depth               = nullptr;
PRIVATE ral_texture_view                  _rt_depth_view          = nullptr;
PRIVATE scene                             _scene                  = nullptr;
PRIVATE scene_graph_node                  _scene_blob_node        = nullptr;
PRIVATE scene_camera                      _scene_camera           = nullptr;
PRIVATE scene_graph_node                  _scene_camera_node      = nullptr;
PRIVATE scene_light                       _scene_light            = nullptr;
PRIVATE scene_graph_node                  _scene_light_node       = nullptr;
PRIVATE scene_graph                       _scene_graph            = nullptr; /* do not release */
PRIVATE scene_renderer                    _scene_renderer         = nullptr;
PRIVATE procedural_uv_generator           _uv_generator           = nullptr;
PRIVATE procedural_uv_generator_object_id _uv_generator_object_id = -1;
PRIVATE demo_window                       _window                 = nullptr;
PRIVATE system_event                      _window_closed_event    = system_event_create(true); /* manual_reset */
PRIVATE int                               _window_size[2]         = {1280, 720};
PRIVATE system_matrix4x4                  _view_matrix            = nullptr;

const float _start_metaball_configs[] =
{
 /* size     X      Y      Z    */
    0.15f,  0.25f, 0.25f, 0.25f,
    0.2f,   0.5f,  0.3f,  0.7f,
    0.1f,   0.7f,  0.3f,  0.2f,
    0.12f,  0.8f,  0.5f,  0.5f
};
const unsigned int _n_start_metaball_configs = 4;

PRIVATE float _isolevel = float(_n_start_metaball_configs) - 0.5f;

/* Forward declarations */
PRIVATE ral_present_job _render                         (ral_context                                                context,
                                                         void*                                                      user_arg,
                                                         const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr);
PRIVATE bool            _rendering_rbm_callback_handler (system_window                                              window,
                                                         unsigned short                                             x,
                                                         unsigned short                                             y,
                                                         system_window_vk_status                                    new_status,
                                                         void*);
PRIVATE void            _window_closed_callback_handler (system_window                                              window,
                                                         void*                                                      unused);
PRIVATE void            _window_closing_callback_handler(system_window                                              window,
                                                         void*                                                      unused);

PRIVATE void _get_isolevel_value(void*          user_arg,
                                 system_variant result);
PRIVATE void _init              ();
PRIVATE void _init_rt           ();
PRIVATE void _init_scene        ();
PRIVATE void _init_ui           ();
PRIVATE void _rendering_handler (ogl_context    context,
                                 uint32_t       n_frames_rendered,
                                 system_time    frame_time,
                                 const int*     rendering_area_px_topdown,
                                 void*          renderer);
PRIVATE void _set_isolevel_value(void*          user_arg,
                                 system_variant new_value);


/** TODO */
PRIVATE void _get_isolevel_value(void*          user_arg,
                                 system_variant result)
{
    system_variant_set_float(result,
                             _isolevel);
}

/** TODO */
PRIVATE void _init()
{
    /* Initialize rendertargets */
    _init_rt();

    /* Initialize scene stuff */
    _init_scene();

    /* Initialize the UI */
    _init_ui();

    /* Initialize projection & view matrices */
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(DEG_TO_RAD(34),  /* fov_y */
                                                                               float(_window_size[0]) / float(_window_size[1]),
                                                                               0.1f,  /* z_near */
                                                                               10.0f); /* z_far  */
    _view_matrix       = system_matrix4x4_create                              ();

    system_matrix4x4_set_to_identity(_view_matrix);

    /* Initialize the flyby */
    static const bool is_flyby_active = true;

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_FLYBY,
                            &_flyby);
    demo_flyby_set_property (_flyby,
                             DEMO_FLYBY_PROPERTY_IS_ACTIVE,
                            &is_flyby_active);
}

/** TODO */
PRIVATE void _init_rt()
{
    /* Initialize rendertargets */
    ral_texture_create_info      color_texture_create_info;
    ral_texture_view_create_info color_texture_view_create_info;
    ral_texture_create_info      depth_texture_create_info;
    ral_texture_view_create_info depth_texture_view_create_info;

    color_texture_create_info.base_mipmap_depth      = 1;
    color_texture_create_info.base_mipmap_height     = _window_size[1];
    color_texture_create_info.base_mipmap_width      = _window_size[0];
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
    depth_texture_create_info.base_mipmap_height     = _window_size[1];
    depth_texture_create_info.base_mipmap_width      = _window_size[0];
    depth_texture_create_info.fixed_sample_locations = true;
    depth_texture_create_info.format                 = RAL_FORMAT_DEPTH16_SNORM;
    depth_texture_create_info.name                   = system_hashed_ansi_string_create("Staging depth texture");
    depth_texture_create_info.n_layers               = 1;
    depth_texture_create_info.n_samples              = 1;
    depth_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    depth_texture_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &color_texture_create_info,
                               &_rt_color);
    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &depth_texture_create_info,
                               &_rt_depth);

    color_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_rt_color);
    depth_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_rt_depth);

    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &color_texture_view_create_info,
                                    &_rt_color_view);
    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &depth_texture_view_create_info,
                                    &_rt_depth_view);
}

/** TODO */
PRIVATE void _init_scene()
{
    scene_mesh blob_scene_mesh = nullptr;

    /* Set up the scene */
    system_hashed_ansi_string blob_has       = system_hashed_ansi_string_create("Test blob");
    system_hashed_ansi_string camera_has     = system_hashed_ansi_string_create("Test camera");
    system_hashed_ansi_string light_has      = system_hashed_ansi_string_create("Test light");
    system_hashed_ansi_string test_scene_has = system_hashed_ansi_string_create("Test scene");

    _scene        = scene_create                  (_context,
                                                   test_scene_has);
    _scene_camera = scene_camera_create           (camera_has,
                                                   test_scene_has);
    _scene_light  = scene_light_create_directional(light_has,
                                                   nullptr); /* object_manager_path */

    /* Set up the metaballs mesh material */
    curve_container* color_curves = nullptr;
    scene_material   material     = scene_material_create      (system_hashed_ansi_string_create("Metaballs material"),
                                                                nullptr, /* object_manager_path */
                                                                _scene);
    system_variant   variant_one  = system_variant_create_float(1.0f);

    scene_material_get_property(material,
                                SCENE_MATERIAL_PROPERTY_COLOR,
                               &color_curves);

    curve_container_set_default_value(color_curves[1],
                                      variant_one);

    system_variant_release(variant_one);
    variant_one = nullptr;

    /* Set up the metaballs mesh instance */
    mesh         marching_cubes_mesh_gpu = nullptr;
    ral_buffer   scalar_field_bo         = nullptr;
    GLuint       scalar_field_bo_id      = 0;
    unsigned int scalar_field_bo_size    = 0;

    _metaballs = scalar_field_metaballs_create(_context,
                                               _blob_size,
                                               system_hashed_ansi_string_create("Metaballs"));

    scalar_field_metaballs_get_property(_metaballs,
                                        SCALAR_FIELD_METABALLS_PROPERTY_DATA_BO_RAL,
                                       &scalar_field_bo);
    ral_buffer_get_property            (scalar_field_bo,
                                        RAL_BUFFER_PROPERTY_SIZE,
                                       &scalar_field_bo_size);

    scalar_field_metaballs_set_property(_metaballs,
                                        SCALAR_FIELD_METABALLS_PROPERTY_N_METABALLS,
                                       &_n_start_metaball_configs);

    for (unsigned int n_metaball = 0;
                      n_metaball < _n_start_metaball_configs;
                    ++n_metaball)
    {
        scalar_field_metaballs_set_metaball_property(_metaballs,
                                                     SCALAR_FIELD_METABALLS_METABALL_PROPERTY_SIZE,
                                                     n_metaball,
                                                     _start_metaball_configs + 4 * n_metaball + 0);
        scalar_field_metaballs_set_metaball_property(_metaballs,
                                                     SCALAR_FIELD_METABALLS_METABALL_PROPERTY_XYZ,
                                                     n_metaball,
                                                     _start_metaball_configs + 4 * n_metaball + 1);
    }

    _marching_cubes = mesh_marchingcubes_create(_context,
                                                _blob_size,
                                                scalar_field_bo,
                                                _isolevel,
                                                material,
                                                system_hashed_ansi_string_create("Marching cubes") );

    mesh_marchingcubes_get_property(_marching_cubes,
                                    MESH_MARCHINGCUBES_PROPERTY_MESH,
                                   &marching_cubes_mesh_gpu);

    scene_add_light        (_scene,
                            _scene_light);
    scene_add_mesh_instance(_scene,
                            marching_cubes_mesh_gpu,
                            blob_has,
                           &blob_scene_mesh);

    /* Set up the procedural UV generator */
    _uv_generator = procedural_uv_generator_create(_context,
                                                   PROCEDURAL_UV_GENERATOR_TYPE_OBJECT_LINEAR,
                                                   system_hashed_ansi_string_create("Procedural UV generator") );

    _uv_generator_object_id = procedural_uv_generator_add_mesh(_uv_generator,
                                                               marching_cubes_mesh_gpu,
                                                               0);      /* in_mesh_layer_id */

    procedural_uv_generator_alloc_result_buffer_memory(_uv_generator,
                                                       _uv_generator_object_id,
                                                       sizeof(float) * 2 /* uv */ * _blob_size[0] * _blob_size[1] * _blob_size[2] * 5 /* max number of triangles per voxel */);

    /* Set up the scene graph */
    system_matrix4x4 identity_matrix = system_matrix4x4_create();

    scene_get_property(_scene,
                      SCENE_PROPERTY_GRAPH,
                     &_scene_graph);

    system_matrix4x4_set_to_identity(identity_matrix);

    _scene_blob_node   = scene_graph_create_general_node                        (_scene_graph);
    _scene_camera_node = scene_graph_create_static_matrix4x4_transformation_node(_scene_graph,
                                                                                 identity_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);
    _scene_light_node  = scene_graph_create_static_matrix4x4_transformation_node(_scene_graph,
                                                                                 identity_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

    system_matrix4x4_release(identity_matrix);
    identity_matrix = nullptr;

    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_blob_node,
                                      SCENE_OBJECT_TYPE_MESH,
                                      blob_scene_mesh);
    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_camera_node,
                                      SCENE_OBJECT_TYPE_CAMERA,
                                      _scene_camera);
    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_light_node,
                                      SCENE_OBJECT_TYPE_LIGHT,
                                      _scene_light);

    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_camera_node);
    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_light_node);
    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_blob_node);

    /* Set up the scene renderer */
    _scene_renderer = scene_renderer_create(_context,
                                            _scene);

    /* Clean up */
    scene_material_release(material);
}

/** TODO */
PRIVATE void _init_ui()
{
    const float           isolevel_scrollbar_x1y1[] = {0.8f, 0.0f};
    ral_rendering_handler rendering_handler         = nullptr;
    ui                    ui_instance               = nullptr;

    ral_context_get_property          (_context,
                                       RAL_CONTEXT_PROPERTY_RENDERING_HANDLER,
                                      &rendering_handler);
    ral_rendering_handler_get_property(rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_UI,
                                      &ui_instance);

    ui_add_scrollbar(ui_instance,
                     system_hashed_ansi_string_create("Isolevel"),
                     UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER,
                     system_variant_create_float(float(_n_start_metaball_configs - 1)),
                     system_variant_create_float(float(_n_start_metaball_configs)    ),
                     isolevel_scrollbar_x1y1,
                     _get_isolevel_value,
                     nullptr,         /* get_current_value_user_arg */
                     _set_isolevel_value,
                     nullptr);        /* set_current_value_user_arg */
}

/** TODO */
PRIVATE ral_present_job _render(ral_context                                                context,
                                void*                                                      user_arg,
                                const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr)
{
    bool                has_scalar_field_changed         = false;
    system_matrix4x4    light_node_transformation_matrix = nullptr;
    ral_present_task    polygonization_task;
    ral_present_task_id polygonization_task_id;
    ral_present_task    render_task;
    ral_present_task_id render_task_id;
    ral_present_job     result_job;
    bool                scalar_field_update_needed;
    ral_present_task    scalar_field_update_task;
    ral_present_task_id scalar_field_update_task_id;
    ral_present_task    uv_data_update_task;
    ral_present_task_id uv_data_update_task_id;

    /* Update the flyby */
    demo_flyby_update(_flyby);

    /* Update the metaball configuration */
    /* 1. Isolevel */
    mesh_marchingcubes_set_property(_marching_cubes,
                                    MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL,
                                   &_isolevel);

    /* 2. Metaball locations */
    for (unsigned int n_metaball = 0;
                      n_metaball < _n_start_metaball_configs;
                    ++n_metaball)
    {
        float t        = float((frame_data_ptr->frame_time) % 2000) / 2000.0f * 10.0f * 3.14152965f;
        float new_size = _start_metaball_configs[n_metaball * 4 + 0] + sin((t * n_metaball) * 3.14152965f) * 0.0125f;

        float new_xyz[] =
        {
            _start_metaball_configs[n_metaball * 4 + 1] + t                       / float(250 * (n_metaball + 1)),
            _start_metaball_configs[n_metaball * 4 + 2] + t * cos(t * n_metaball) / float(75 * (n_metaball + 1)),
            _start_metaball_configs[n_metaball * 4 + 3] + t * sin(t * n_metaball) / float(120 * (n_metaball + 1))

        };

        scalar_field_metaballs_set_metaball_property(_metaballs,
                                                     SCALAR_FIELD_METABALLS_METABALL_PROPERTY_SIZE,
                                                     n_metaball,
                                                    &new_size);
        scalar_field_metaballs_set_metaball_property(_metaballs,
                                                     SCALAR_FIELD_METABALLS_METABALL_PROPERTY_XYZ,
                                                     n_metaball,
                                                     new_xyz);
    }

    /* Update the scalar field */
    scalar_field_metaballs_get_property(_metaballs,
                                        SCALAR_FIELD_METABALLS_PROPERTY_RESULT_BUFFER_UPDATE_NEEDED,
                                       &scalar_field_update_needed);

    scalar_field_update_task = scalar_field_metaballs_get_present_task(_metaballs);

    /* Polygonize it */
    polygonization_task = mesh_marchingcubes_get_polygonize_present_task(_marching_cubes,
                                                                         scalar_field_update_needed);

    /* Update UV data if needed */
    if (scalar_field_update_needed)
    {
        uv_data_update_task = procedural_uv_generator_get_present_task(_uv_generator,
                                                                       _uv_generator_object_id);
    }

    /* Update the light node's transformation matrix to reflect the camera position */
    demo_flyby_get_property         (_flyby,
                                     DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                                    &_view_matrix);
    scene_graph_node_get_property   (_scene_light_node,
                                     SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                    &light_node_transformation_matrix);
    system_matrix4x4_set_to_identity(light_node_transformation_matrix);

    system_matrix4x4_set_from_matrix4x4(light_node_transformation_matrix,
                                        _view_matrix);
    system_matrix4x4_invert            (light_node_transformation_matrix);

    /* Render the scene */
    render_task = scene_renderer_get_present_task_for_scene_graph(_scene_renderer,
                                                                  _view_matrix,
                                                                  _projection_matrix,
                                                                  _scene_camera,
                                                                  //RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                                                  RENDER_MODE_TEXCOORDS_ONLY,
                                                                  false, /* apply_shadow_mapping */
                                                                  HELPER_VISUALIZATION_BOUNDING_BOXES,
                                                                  frame_data_ptr->frame_time,
                                                                  _rt_color_view,
                                                                  _rt_depth_view);

    /* Form the result job */
    result_job = ral_present_job_create();

    ral_present_job_add_task(result_job,
                             scalar_field_update_task,
                            &scalar_field_update_task_id);
    ral_present_job_add_task(result_job,
                             polygonization_task,
                            &polygonization_task_id);
    ral_present_job_add_task(result_job,
                             render_task,
                            &render_task_id);

    if (uv_data_update_task != nullptr)
    {
        ral_present_job_add_task(result_job,
                                 uv_data_update_task,
                                &uv_data_update_task_id);
    }

    ral_present_job_connect_tasks(result_job,
                                  scalar_field_update_task_id,
                                  0, /* n_src_task_output */
                                  polygonization_task_id,
                                  0,        /* n_dst_task_input          */
                                  nullptr); /* out_opt_connection_id_ptr */

    /* TODO: polygonization -> render task connection */

    ral_present_job_set_presentable_output(result_job,
                                           render_task_id,
                                           false, /* is_input_io */
                                           0);    /* n_io        */

    /* Clean up */
    ral_present_task_release(polygonization_task);
    ral_present_task_release(render_task);
    ral_present_task_release(scalar_field_update_task);

    if (uv_data_update_task != nullptr)
    {
        ral_present_task_release(uv_data_update_task);
    }

    /* All done */
    return result_job;
}

/** Event callback handlers */
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    system_event_set(_window_closed_event);

    return true;
}

/** TODO */
PRIVATE void _set_isolevel_value(void*          user_arg,
                                 system_variant new_value)
{
    system_variant_get_float(new_value,
                            &_isolevel);
}

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    ral_texture textures_to_release[] =
    {
        _rt_color,
        _rt_depth
    };
    ral_texture_view texture_views_to_release[] =
    {
        _rt_color_view,
        _rt_depth_view
    };
    const uint32_t n_texture_views_to_release = sizeof(texture_views_to_release) / sizeof(texture_views_to_release[0]);
    const uint32_t n_textures_to_release      = sizeof(textures_to_release)      / sizeof(textures_to_release     [0]);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               reinterpret_cast<void* const*>(textures_to_release) );
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               n_texture_views_to_release,
                               reinterpret_cast<void* const*>(texture_views_to_release) );

    if (_marching_cubes != nullptr)
    {
        mesh_marchingcubes_release(_marching_cubes);

        _marching_cubes = nullptr;
    }

    if (_metaballs != nullptr)
    {
        scalar_field_metaballs_release(_metaballs);

        _metaballs = nullptr;
    }

    if (_projection_matrix != nullptr)
    {
        system_matrix4x4_release(_projection_matrix);

        _projection_matrix = nullptr;
    }

    if (_scene != nullptr)
    {
        scene_release(_scene);

        _scene = nullptr;
    }

    if (_scene_camera != nullptr)
    {
        scene_camera_release(_scene_camera);

        _scene_camera = nullptr;
    }

    if (_scene_light != nullptr)
    {
        scene_light_release(_scene_light);

        _scene_light = nullptr;
    }

    if (_scene_renderer != nullptr)
    {
        scene_renderer_release(_scene_renderer);

        _scene_renderer = nullptr;
    }

    if (_uv_generator != nullptr)
    {
        procedural_uv_generator_release(_uv_generator);

        _uv_generator = nullptr;
    }

    if (_view_matrix != nullptr)
    {
        system_matrix4x4_release(_view_matrix);

        _view_matrix = nullptr;
    }

    /* All done */
    system_event_set(_window_closed_event);
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
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _render;
    ral_rendering_handler                   rendering_handler  = NULL;
    system_screen_mode                      screen_mode        = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("DOF test app");
    int                                     window_x1y1x2y2[4] = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    _window_size[0] /= 2;
    _window_size[1] /= 2;


    window_create_info.resolution[0] = _window_size[0];
    window_create_info.resolution[1] = _window_size[1];

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
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f, /* fov_y */
                                                                               float(_window_size[0]) / float(_window_size[1]),
                                                                               0.01f,  /* z_near */
                                                                               100.0f);/* z_far */

    /* Set up callbacks */
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_rbm_callback_handler,
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

    /* Set up rendering-related stuff */
    _init();

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