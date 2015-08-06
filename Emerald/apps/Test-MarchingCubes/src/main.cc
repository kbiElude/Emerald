/**
 *
 * Marching cubes test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "procedural/procedural_mesh_box.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <algorithm>
#include "main.h"

PRIVATE procedural_mesh_box _box                          = NULL;
PRIVATE mesh                _box_custom_mesh              = NULL;
PRIVATE unsigned int        _box_n_vertices               = -1;
PRIVATE ogl_context         _context                      = NULL;
PRIVATE ogl_flyby           _context_flyby                = NULL;
PRIVATE ogl_program         _po                           = NULL;
PRIVATE ogl_program_ub      _po_ub_vsdata                 = NULL;
PRIVATE GLint               _po_ub_vsdata_bo_id           = -1;
PRIVATE GLuint              _po_ub_vsdata_bo_start_offset = -1;
PRIVATE system_matrix4x4    _projection_matrix            = NULL;
PRIVATE scene               _scene                        = NULL;
PRIVATE scene_graph_node    _scene_box_node               = NULL;
PRIVATE scene_camera        _scene_camera                 = NULL;
PRIVATE scene_graph_node    _scene_camera_node            = NULL;
PRIVATE scene_graph         _scene_graph                  = NULL; /* do not release */
PRIVATE ogl_scene_renderer  _scene_renderer               = NULL;
PRIVATE system_event        _window_closed_event          = system_event_create(true); /* manual_reset */
PRIVATE int                 _window_size[2]               = {1280, 720};
PRIVATE GLuint              _vao_id                       = 0;
PRIVATE system_matrix4x4    _view_matrix                  = NULL;


/* Forward declarations */
PRIVATE void _get_bounding_box_aabb          (void*                             user_arg,
                                              float**                           out_aabb_model_vec4_min,
                                              float**                           out_aabb_model_vec4_max);
PRIVATE void _init_program_objects           (const ogl_context_gl_entrypoints* entry_points);
PRIVATE void _init_scene                     ();
PRIVATE void _init_vao                       (const ogl_context_gl_entrypoints* entry_points);
PRIVATE void _render_bounding_box            (ogl_context                       context,
                                              const void*                       user_arg,
                                              const system_matrix4x4            model_matrix,
                                              const system_matrix4x4            vp_matrix,
                                              const system_matrix4x4            normal_matrix,
                                              bool                              is_depth_prepass);
PRIVATE void _rendering_handler              (ogl_context                       context,
                                              uint32_t                          n_frames_rendered,
                                              system_time                       frame_time,
                                              const int*                        rendering_area_px_topdown,
                                              void*                             renderer);
PRIVATE bool _rendering_rbm_callback_handler (system_window                     window,
                                              unsigned short                    x,
                                              unsigned short                    y,
                                              system_window_vk_status           new_status,
                                              void*);
PRIVATE void _window_closed_callback_handler (system_window                     window);
PRIVATE void _window_closing_callback_handler(system_window                     window);


/** TODO */
PRIVATE void _get_bounding_box_aabb(const void* user_arg,
                                    float*      out_aabb_model_vec3_min,
                                    float*      out_aabb_model_vec3_max)
{
    out_aabb_model_vec3_max[0] = 1.0f;
    out_aabb_model_vec3_max[1] = 1.0f;
    out_aabb_model_vec3_max[2] = 1.0f;
    out_aabb_model_vec3_min[0] = 0.0f;
    out_aabb_model_vec3_min[1] = 0.0f;
    out_aabb_model_vec3_min[2] = 0.0f;

}

/** TODO */
PRIVATE void _init_program_objects(const ogl_context_gl_entrypoints* entry_points)
{
    const char* fs_body = "#version 430 core\n"
                          "\n"
                          "out vec4 result;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    result = vec4(gl_FragCoord.xy / vec2(1280.0, 720.0f), 0.0, 1.0);\n"
                          "}\n";
    const char* vs_body = "#version 430 core\n"
                          "\n"
                          "uniform data\n"
                          "{\n"
                          "    mat4 vp;\n"
                          "};\n"
                          "\n"
                          "in vec4 vertex;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position = vp * vertex;\n"
                          "}\n";

    ogl_shader  fs = ogl_shader_create (_context,
                                        SHADER_TYPE_FRAGMENT,
                                        system_hashed_ansi_string_create("fs") );
    ogl_shader  vs = ogl_shader_create (_context,
                                        SHADER_TYPE_VERTEX,
                                        system_hashed_ansi_string_create("vs") );

    ogl_shader_set_body(fs,
                        system_hashed_ansi_string_create(fs_body) );
    ogl_shader_set_body(vs,
                        system_hashed_ansi_string_create(vs_body) );

    _po = ogl_program_create(_context,
                             system_hashed_ansi_string_create("po"),
                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_po,
                              fs);
    ogl_program_attach_shader(_po,
                              vs);

    ogl_program_link(_po);

    ogl_program_get_uniform_block_by_name(_po,
                                          system_hashed_ansi_string_create("data"),
                                         &_po_ub_vsdata);
    ogl_program_ub_get_property          (_po_ub_vsdata,
                                          OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                         &_po_ub_vsdata_bo_id);
    ogl_program_ub_get_property          (_po_ub_vsdata,
                                          OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                         &_po_ub_vsdata_bo_start_offset);

    ogl_shader_release(fs);
    ogl_shader_release(vs);
}

/** TODO */
PRIVATE void _init_scene()
{
    scene_mesh box_custom_scene_mesh = NULL;

    /* Set up the test mesh data provider */
    _box = procedural_mesh_box_create(_context,
                                      DATA_BO_ARRAYS,
                                      0, /* n_horizontal_patches */
                                      0, /* n_vertical_patches */
                                      system_hashed_ansi_string_create("Bounding box") );

    procedural_mesh_box_get_property(_box,
                                     PROCEDURAL_MESH_BOX_PROPERTY_N_VERTICES,
                                    &_box_n_vertices);

    /* Set up the test mesh instance */
    _box_custom_mesh = mesh_create_custom_mesh(&_render_bounding_box,
                                                NULL, /* render_custom_mesh_proc_user_arg */
                                               &_get_bounding_box_aabb,
                                                NULL, /* get_custom_mesh_aabb_proc_user_arg */
                                                system_hashed_ansi_string_create("Test box") );


    /* Set up the scene */
    system_hashed_ansi_string box_has        = system_hashed_ansi_string_create("Test box");
    system_hashed_ansi_string camera_has     = system_hashed_ansi_string_create("Test camera");
    system_hashed_ansi_string test_scene_has = system_hashed_ansi_string_create("Test scene");

    _scene        = scene_create       (_context,
                                        test_scene_has);
    _scene_camera = scene_camera_create(camera_has,
                                        test_scene_has);

    scene_get_property(_scene,
                       SCENE_PROPERTY_GRAPH,
                      &_scene_graph);

    scene_add_mesh_instance(_scene,
                            _box_custom_mesh,
                            box_has,
                           &box_custom_scene_mesh);

    /* Set up the scene graph */
    system_matrix4x4 identity_matrix = system_matrix4x4_create(); /* TEMP */

    system_matrix4x4_set_to_identity(identity_matrix);

    _scene_box_node    = scene_graph_create_general_node                        (_scene_graph);
    _scene_camera_node = scene_graph_create_static_matrix4x4_transformation_node(_scene_graph,
                                                                                 identity_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

    system_matrix4x4_release(identity_matrix);
    identity_matrix = NULL;

    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_box_node,
                                      SCENE_OBJECT_TYPE_MESH,
                                      box_custom_scene_mesh);
    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_camera_node,
                                      SCENE_OBJECT_TYPE_CAMERA,
                                      _scene_camera);

    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_camera_node);
    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_box_node);

    /* Set up the scene renderer */
    _scene_renderer = ogl_scene_renderer_create(_context,
                                                _scene);
}

/** TODO */
PRIVATE void _init_vao(const ogl_context_gl_entrypoints* entry_points)
{
    GLuint box_custom_mesh_bo_id                          = 0;
    GLuint box_custom_mesh_bo_id_vertex_data_start_offset = 0;

    procedural_mesh_box_get_property(_box,
                                     PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_ID,
                                    &box_custom_mesh_bo_id);
    procedural_mesh_box_get_property(_box,
                                     PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET,
                                    &box_custom_mesh_bo_id_vertex_data_start_offset);

    entry_points->pGLGenVertexArrays(1,
                                    &_vao_id);

    entry_points->pGLBindVertexArray        (_vao_id);
    entry_points->pGLBindBuffer             (GL_ARRAY_BUFFER,
                                             box_custom_mesh_bo_id);
    entry_points->pGLVertexAttribPointer    (0,        /* index */
                                             3,        /* size */
                                             GL_FLOAT,
                                             GL_FALSE, /* normalized */
                                             0,        /* stride */
                                             (GLvoid*) box_custom_mesh_bo_id_vertex_data_start_offset); /* pointer */
    entry_points->pGLEnableVertexAttribArray(0); /* index */
}

/** TODO */
PRIVATE void _render_bounding_box(ogl_context             context,
                                  const void*             user_arg,
                                  const system_matrix4x4  model_matrix,
                                  const system_matrix4x4  vp_matrix,
                                  const system_matrix4x4  normal_matrix,
                                  bool                    is_depth_prepass)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

#if 0
    /* Set up the unifrom block contents */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    ogl_program_ub_set_nonarrayed_uniform_value(_po_ub_vsdata,
                                                0, /* ub_uniform_offset */
                                                system_matrix4x4_get_column_major_data(vp_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_sync                        (_po_ub_vsdata);

    /* Set up the uniform buffer binding */
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        0, /* index */
                                        _po_ub_vsdata_bo_id,
                                        _po_ub_vsdata_bo_start_offset,
                                        sizeof(float) * 16);

    /* Draw stuff */
    entrypoints_ptr->pGLDisable        (GL_CULL_FACE);
    entrypoints_ptr->pGLPolygonMode    (GL_FRONT_AND_BACK,
                                        GL_LINE);

    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_po) );
    entrypoints_ptr->pGLBindVertexArray(_vao_id);
    entrypoints_ptr->pGLDrawArrays     (GL_TRIANGLES,
                                        0, /* first */
                                        _box_n_vertices);

    entrypoints_ptr->pGLPolygonMode    (GL_FRONT_AND_BACK,
                                        GL_FILL);
#endif
}

/** TODO */
PRIVATE void _rendering_handler(ogl_context context,
                                uint32_t    n_frames_rendered,
                                system_time frame_time,
                                const int*  rendering_area_px_topdown,
                                void*       renderer)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    static bool                       is_initialized  = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    /* If this is the first frame we're rendering, initialize various stuff. */
    if (!is_initialized)
    {
        /* Initialize scene stuff */
        _init_scene();

        /* Initialize projection & view matrices */
        _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,  /* fov_y */
                                                                                   float(_window_size[0]) / float(_window_size[1]),
                                                                                   0.01f,  /* z_near */
                                                                                   10.0f); /* z_far  */
        _view_matrix       = system_matrix4x4_create                              ();

        system_matrix4x4_set_to_identity(_view_matrix);

        /* Initialize the flyby */
        static const bool is_flyby_active = true;

        ogl_context_get_property(_context,
                                 OGL_CONTEXT_PROPERTY_FLYBY,
                                &_context_flyby);
        ogl_flyby_set_property  (_context_flyby,
                                 OGL_FLYBY_PROPERTY_IS_ACTIVE,
                                &is_flyby_active);

        /* Initialize program objects */
        _init_program_objects(entrypoints_ptr);

        /* Initialize VAO */
        _init_vao(entrypoints_ptr);

        /* All done */
        is_initialized = true;
    } /* if (!is_initialized) */

    /* Update the flyby */
    ogl_flyby_update(_context_flyby);

    /* Render the scene */
    ogl_flyby_get_property(_context_flyby,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_view_matrix);

    ogl_scene_renderer_render_scene_graph(_scene_renderer,
                                          _view_matrix,
                                          _projection_matrix,
                                          _scene_camera,
                                          RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                          false, /* apply_shadow_mapping */
                                          (_ogl_scene_renderer_helper_visualization) 0,
                                          //HELPER_VISUALIZATION_BOUNDING_BOXES,
                                          frame_time);
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

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window)
{
    if (_box != NULL)
    {
        procedural_mesh_box_release(_box);

        _box = NULL;
    }

    if (_box_custom_mesh != NULL)
    {
        mesh_release(_box_custom_mesh);

        _box_custom_mesh = NULL;
    }

    if (_po != NULL)
    {
        ogl_program_release(_po);

        _po = NULL;
    }

    if (_projection_matrix != NULL)
    {
        system_matrix4x4_release(_projection_matrix);

        _projection_matrix = NULL;
    }

    if (_scene != NULL)
    {
        scene_release(_scene);

        _scene = NULL;
    }

    if (_scene_camera != NULL)
    {
        scene_camera_release(_scene_camera);

        _scene_camera = NULL;
    }

    if (_scene_renderer != NULL)
    {
        ogl_scene_renderer_release(_scene_renderer);

        _scene_renderer = NULL;
    }

    if (_view_matrix != NULL)
    {
        system_matrix4x4_release(_view_matrix);

        _view_matrix = NULL;
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
    system_screen_mode    screen_mode              = NULL;
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    _window_size[0] /= 2;
    _window_size[1] /= 2;

    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 true,  /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

    /* Set up the rendering contxt */
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL); /* user_arg */

    /* Kick off the rendering process */
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Launch the rendering process and wait until the window is closed. */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    system_window_close (window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}