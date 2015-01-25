/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_curve_renderer.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_materials.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_uber.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_dropdown.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include <string>
#include <sstream>

uint32_t                   _active_camera_index          = 0;
uint32_t                   _active_camera_path_index     = 0; /* none */
float                      _animation_duration_float     = 0.0f;
system_timeline_time       _animation_duration_time      = 0;
system_critical_section    _camera_cs                    = NULL;
void**                     _camera_indices               = NULL;
system_hashed_ansi_string* _camera_names                 = NULL;
void**                     _camera_path_indices          = NULL;
system_hashed_ansi_string* _camera_path_names            = NULL;
system_resizable_vector    _cameras                      = NULL;
ogl_context                _context                      = NULL;
system_matrix4x4           _current_matrix               = NULL;
ogl_curve_renderer         _curve_renderer               = NULL;
ogl_curve_item_id          _curve_renderer_item_id       = -1;
ogl_pipeline               _pipeline                     = NULL;
uint32_t                   _pipeline_stage_id            = -1;
system_timeline_time       _scene_duration               = 0;
ogl_scene_renderer         _scene_renderer               = NULL;
system_hashed_ansi_string  _selected_scene_data_filename = NULL;
scene                      _test_scene                   = NULL;
system_variant             _temp_variant                 = system_variant_create(SYSTEM_VARIANT_FLOAT);
ogl_text                   _text_renderer                = NULL;
ogl_ui                     _ui                           = NULL;
ogl_ui_control             _ui_active_camera_control     = NULL;
ogl_ui_control             _ui_active_path_control       = NULL;
system_window              _window                       = NULL;
system_event               _window_closed_event          = system_event_create(true, false);

GLuint           _vao_id            = 0;

typedef struct _camera
{
    scene_camera              camera;
    bool                      is_flyby;
    system_hashed_ansi_string name;

    explicit _camera(bool                      in_is_flyby,
                     system_hashed_ansi_string in_name,
                     scene_camera              in_camera)
    {
        camera   = in_camera;
        is_flyby = in_is_flyby;
        name     = in_name;
    }
} _camera;

/* Forward declarations */
void _on_active_camera_changed    (void* fire_proc_user_arg,
                                   void* event_user_arg);
void _on_shown_camera_path_changed(void* fire_proc_user_arg,
                                   void* event_user_arg);
void _update_ui_controls_location ();

/** TODO */
void _callback_on_dropdown_switch(ogl_ui_control control,
                                  int            callback_id,
                                  void*          callback_subscriber_data,
                                  void*          callback_data)
{
    ogl_rendering_handler rendering_handler = NULL;

    system_window_get_rendering_handler(_window,
                                       &rendering_handler);

    ogl_rendering_handler_lock_bound_context(rendering_handler);
    {
        _update_ui_controls_location();
    }
    ogl_rendering_handler_unlock_bound_context(rendering_handler);
}

/** TODO */
void _init_cameras()
{
    unsigned int n_cameras = 0;

    /* How many cameras do we have? */
    scene_get_property(_test_scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_cameras);

    /* Iterate over all cameras and create a descriptor for each entry */
    _cameras = system_resizable_vector_create(4, /* capacity */
                                              sizeof(_camera*) );

    for (unsigned int n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
    {
        scene_camera current_camera = scene_get_camera_by_index(_test_scene, n_camera);

        ASSERT_ALWAYS_ASYNC(current_camera != NULL,
                            "Could not retrieve camera at index [%d]",
                            n_camera);

        if (current_camera != NULL)
        {
            system_hashed_ansi_string current_camera_name = NULL;

            scene_camera_get_property(current_camera,
                                      SCENE_CAMERA_PROPERTY_NAME,
                                      0, /* time - irrelevant for camera name */
                                     &current_camera_name);

            /* Create a new descriptor */
            _camera* new_camera = new (std::nothrow) _camera(false, /* is_flyby */
                                                             current_camera_name,
                                                             current_camera);

            ASSERT_ALWAYS_SYNC(new_camera != NULL, "Out of memory");
            if (new_camera != NULL)
            {
                system_resizable_vector_push(_cameras, new_camera);
            }
        }
    }

    /* Add the 'flyby' camera */
    _camera* flyby_camera = new (std::nothrow) _camera(true,
                                                       system_hashed_ansi_string_create("Flyby camera"),
                                                       NULL);

    ASSERT_ALWAYS_SYNC(flyby_camera != NULL, "Out of memory");
    if (flyby_camera != NULL)
    {
        system_resizable_vector_push(_cameras, flyby_camera);
    }

    /* Create the list of camera names that will be shown under "active camera" dropdown */
    const uint32_t n_total_cameras = system_resizable_vector_get_amount_of_elements(_cameras);

    _camera_indices = new void*                    [n_total_cameras];
    _camera_names   = new system_hashed_ansi_string[n_total_cameras];

    for (uint32_t n_camera = 0;
                  n_camera < n_total_cameras;
                ++n_camera)
    {
        _camera* current_camera = NULL;

        system_resizable_vector_get_element_at(_cameras,
                                               n_camera,
                                              &current_camera);

        _camera_indices[n_camera] = (void*) n_camera;
        _camera_names  [n_camera] = current_camera->name;
    }

    /* Create the list of camera names that will be shown under "show camera path" dropdown */
    _camera_path_indices = new void*                    [n_total_cameras];
    _camera_path_names   = new system_hashed_ansi_string[n_total_cameras];

    for (uint32_t n_camera = 0;
                  n_camera < n_total_cameras;
                ++n_camera)
    {
        if (n_camera == 0)
        {
            _camera_path_indices[n_camera] = (void*) -1;
            _camera_path_names  [n_camera] = system_hashed_ansi_string_create("None");
        }
        else
        {
            _camera* current_camera = NULL;

            system_resizable_vector_get_element_at(_cameras,
                                                   n_camera - 1,
                                                  &current_camera);

            _camera_path_indices[n_camera] = (void*) (n_camera - 1);
            _camera_path_names  [n_camera] = current_camera->name;
        }
    }

    /* Set up the selected camera */
    _on_active_camera_changed(NULL, (void*) _active_camera_index);
}

/** "Active camera" dropdown call-back handler */
void _on_active_camera_changed(void* fire_proc_user_arg,
                               void* event_user_arg)
{
    system_critical_section_enter(_camera_cs);
    {
        _active_camera_index = (unsigned int) event_user_arg;
    }
    system_critical_section_leave(_camera_cs);
}

/* "Show camera path" dropdown call-back handler */
void _on_shown_camera_path_changed(void* fire_proc_user_arg,
                                   void* event_user_arg)
{
    _active_camera_path_index = (uint32_t) event_user_arg;

    if (_curve_renderer_item_id != -1)
    {
        ogl_curve_renderer_delete_curve(_curve_renderer,
                                        _curve_renderer_item_id);

        _curve_renderer_item_id = -1;
    }

    if (_active_camera_path_index != -1) /* None */
    {
        /* Retrieve the camera descriptor */
        _camera* camera_ptr = NULL;

        system_resizable_vector_get_element_at(_cameras,
                                               _active_camera_path_index,
                                              &camera_ptr);

        ASSERT_DEBUG_SYNC(camera_ptr != NULL, "Could not retrieve camera descriptor");
        if (camera_ptr != NULL)
        {
            /* Configure the curve renderer to show the camera path */
            scene_graph_node     camera_node              = NULL;
            const float          curve_color[4]           = {1.0f, 1.0f, 1.0f, 1.0f};
            scene_graph          graph                    = NULL;

            scene_get_property(_test_scene,
                               SCENE_PROPERTY_GRAPH,
                              &graph);

            camera_node = scene_graph_get_node_for_object(graph,
                                                          SCENE_OBJECT_TYPE_CAMERA,
                                                          camera_ptr->camera);
            ASSERT_DEBUG_SYNC(camera_node != NULL,
                              "Could not retrieve owner node for selected camera.");

            _curve_renderer_item_id = ogl_curve_renderer_add_scene_graph_node_curve(_curve_renderer,
                                                                                    graph,
                                                                                    camera_node,
                                                                                    curve_color,
                                                                                    _scene_duration,
                                                                                    15,      /* n_samples_per_second */
                                                                                    10.0f); /* view_vector_length */
       }
    }
}

/** Rendering handler */
void _rendering_handler(ogl_context          context,
                        uint32_t             n_frames_rendered,
                        system_timeline_time frame_time,
                        void*                unused)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLClearColor(0.0f, 0.0f, 0.5f, 0.0f);
    entry_points->pGLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLEnable(GL_DEPTH_TEST);
    entry_points->pGLEnable(GL_CULL_FACE);

    uint32_t temp = 0;
    system_time_get_msec_for_timeline_time(frame_time, &temp);

    /* Render the scene */
    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            frame_time);
}

void _render_scene(ogl_context          context,
                   system_timeline_time time,
                   void*                not_used)
{
    static system_timeline_time start_time = system_time_now();
           system_timeline_time frame_time = /* 0; */(system_time_now() - start_time) % _animation_duration_time;

    /* Update view matrix */
    scene_camera     camera             = NULL; /* TODO: this will be incorrectly null for flyby camera - FIXME */
    float            camera_location[4] = {0, 0, 0, 0};
    bool             is_flyby_active    = false;
    system_matrix4x4 projection         = system_matrix4x4_create();
    system_matrix4x4 view               = system_matrix4x4_create();

    system_critical_section_enter(_camera_cs);
    {
        _camera* camera_ptr = NULL;

        /* Retrieve camera descriptor */
        system_resizable_vector_get_element_at(_cameras,
                                               _active_camera_index,
                                              &camera_ptr);

        ASSERT_DEBUG_SYNC(camera_ptr != NULL, "Could not retrieve active camera descriptor");
        if (camera_ptr != NULL)
        {
            /* If there is no projection matrix available, initialize one now */
            /* Retrieve the camera descriptro */
            if (!camera_ptr->is_flyby)
            {
                bool new_visibility = false;

                /* Create projection matrix */
                float           yfov_value;
                float           zfar;
                float           znear;

                scene_camera_get_property(camera_ptr->camera,
                                          SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,
                                          time,
                                          &yfov_value);
                scene_camera_get_property(camera_ptr->camera,
                                          SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                          time,
                                          &zfar);
                scene_camera_get_property(camera_ptr->camera,
                                          SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                          time,
                                          &znear);

                float new_zfar = 0.01f;
                float new_znear = 100.0f;
                scene_camera_set_property(camera_ptr->camera,
                                          SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                         &new_zfar);
                scene_camera_set_property(camera_ptr->camera,
                                          SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                         &new_znear);

                projection = system_matrix4x4_create_perspective_projection_matrix(yfov_value,
                                                                                   1280 / 720.0f,
                                                                                   0.01f,   //znear,
                                                                                   100.0f); //zfar);

                if (_ui_active_path_control != NULL)
                {
                    ogl_ui_set_property(_ui_active_path_control,
                                        OGL_UI_DROPDOWN_PROPERTY_VISIBLE,
                                       &new_visibility);
                }

                camera = camera_ptr->camera;
            }
            else
            {
                bool new_visibility = true;

                projection = system_matrix4x4_create_perspective_projection_matrix(45.0f,
                                                                                   1280 / 720.0f,
                                                                                   1.0f,
                                                                                   10000.0f);

                if (_ui_active_path_control != NULL)
                {
                    ogl_ui_set_property(_ui_active_path_control,
                                        OGL_UI_DROPDOWN_PROPERTY_VISIBLE,
                                       &new_visibility);
                }
            }

            if (camera_ptr->is_flyby)
            {
                is_flyby_active = true;

                ogl_flyby_lock();
                {
                    ogl_flyby_update(_context);

                    /* Retrieve flyby details */
                    const float* flyby_camera_location = ogl_flyby_get_camera_location(_context);

                    memcpy(camera_location,
                           flyby_camera_location,
                           sizeof(float) * 3);

                    ogl_flyby_get_view_matrix(_context, view);
                }
                ogl_flyby_unlock();
            } /* if (camera_ptr->is_flyby) */
            else
            {
                scene_graph scene_renderer_graph = NULL;

                /* Compute matrices for all nodes */
                ogl_scene_renderer_get_property(_scene_renderer,
                                                OGL_SCENE_RENDERER_PROPERTY_GRAPH,
                                               &scene_renderer_graph);

                scene_graph_lock(scene_renderer_graph);
                {
                    scene_graph_compute(scene_renderer_graph,
                                        frame_time);

                    /* Retrieve node that contains the transformation matrix for the camera */
                    scene_graph_node scene_camera_node                  = NULL;
                    system_matrix4x4 scene_camera_transformation_matrix = NULL;

                    scene_camera_get_property(camera_ptr->camera,
                                              SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                              0, /* time - irrelevant for the owner graph node */
                                             &scene_camera_node);

                    scene_graph_node_get_property(scene_camera_node,
                                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                                 &scene_camera_transformation_matrix);

                    /* For the view matrix, we need to take the inverse of the transformation matrix */
                    system_matrix4x4_set_from_matrix4x4(view, scene_camera_transformation_matrix);
                    system_matrix4x4_invert            (view);

                    const float* view_matrix_data = system_matrix4x4_get_row_major_data(view);

                    camera_location[0] = view_matrix_data[0];
                    camera_location[1] = view_matrix_data[1];
                    camera_location[2] = view_matrix_data[2];
                }
                scene_graph_unlock(scene_renderer_graph);
            }
        } /* if (camera_ptr != NULL) */
    }
    system_critical_section_leave(_camera_cs);

    /* Traverse the scene graph */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLEnable(GL_FRAMEBUFFER_SRGB);
    {
        ogl_scene_renderer_render_scene_graph(_scene_renderer,
                                              view,
                                              projection,
                                              camera,
                                              camera_location,
                                              RENDER_MODE_FORWARD,
                                              SHADOW_MAPPING_TYPE_PLAIN,
                                              //(_ogl_scene_renderer_helper_visualization) (HELPER_VISUALIZATION_FRUSTUMS | HELPER_VISUALIZATION_LIGHTS),
                                              HELPER_VISUALIZATION_NONE,
                                              frame_time
                                             );
    }
    entry_points->pGLDisable(GL_FRAMEBUFFER_SRGB);

    /* Draw curves marked as active.
     *
     * NOTE: For some scenes (eg. the introductory one used in "Suxx") the curve renderer
     *       seems to be running into precision issues. These appear to be introduced when
     *       inverting the camera transformation matrix to obtain the view matrix. They
     *       cause the camera path spline to be incorrectly clipped when it gets too close
     *       to the camera. At the same time, the issue does not reproduce when the spline
     *       is viewed from a non-graph camera (the "flyby" one).
     *
     *       This isn't much of an issue for us, so we just show the paths when in flyby
     *       mode. Should this ever become a problem, please investigate.
     **/
    if (_curve_renderer_item_id != -1 && is_flyby_active)
    {
        const ogl_context_gl_entrypoints* entry_points = NULL;
        system_matrix4x4                  vp           = system_matrix4x4_create_by_mul(projection, view);

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);

        ogl_curve_renderer_draw(_curve_renderer,
                                1,
                               &_curve_renderer_item_id,
                                vp);

        system_matrix4x4_release(vp);
    }

    /* Render UI */
    ogl_ui_draw  (_ui);
    ogl_text_draw(_context, _text_renderer);

    /* All done */
    system_matrix4x4_release(view);
    system_matrix4x4_release(projection);
}

void _rendering_rbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

void _setup_ui()
{
    const float  active_camera_dropdown_x1y1[2] = {0.7f, 0.1f};
    const float  text_default_size              = 0.5f;
    int          window_height                  = 0;
    int          window_width                   = 0;

    system_window_get_dimensions(_window,
                                &window_width,
                                &window_height);

    _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"),
                                     _context,
                                     system_resources_get_meiryo_font_table(),
                                     window_width,
                                     window_height);

    ogl_text_set_text_string_property(_text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_default_size);

    _ui = ogl_ui_create(_text_renderer,
                        system_hashed_ansi_string_create("UI") );

    /* Create camera selector */
    _ui_active_camera_control = ogl_ui_add_dropdown(_ui,
                                                    system_resizable_vector_get_amount_of_elements(_cameras),
                                                    _camera_names,
                                                    _camera_indices,
                                                    _active_camera_index,
                                                    system_hashed_ansi_string_create("Active camera:"),
                                                    active_camera_dropdown_x1y1,
                                                    _on_active_camera_changed,
                                                    NULL);

    /* Create camera path selector */
    float next_ui_control_x1y1[2];
    float prev_ui_control_x1y1x2y2[4];

    ogl_ui_get_property(_ui_active_camera_control,
                        OGL_UI_DROPDOWN_PROPERTY_X1Y1X2Y2,
                        prev_ui_control_x1y1x2y2);

    next_ui_control_x1y1[0] = active_camera_dropdown_x1y1[0];
    next_ui_control_x1y1[1] = 1.0f - (prev_ui_control_x1y1x2y2[1] - 1.0f / 720.0f);

    _ui_active_path_control = ogl_ui_add_dropdown(_ui,
                                                  system_resizable_vector_get_amount_of_elements(_cameras),
                                                  _camera_path_names,
                                                  _camera_path_indices,
                                                  _active_camera_path_index,
                                                  system_hashed_ansi_string_create("Show camera path for:"),
                                                  next_ui_control_x1y1,
                                                  _on_shown_camera_path_changed,
                                                  NULL);

    /* Update the visibility of the UI controls */
    _on_active_camera_changed(NULL,
                              (void*) _active_camera_index);

    /* Register for call-backs */
    ogl_ui_register_control_callback(_ui,
                                     _ui_active_camera_control,
                                     OGL_UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                     _callback_on_dropdown_switch,
                                     NULL); /* callback_proc_user_arg */
}

/** TODO */
void _update_ui_controls_location()
{
    float active_path_dy;
    float new_active_path_x1y1x2y2   [4];
    float prev_active_camera_x1y1x2y2[4];
    float prev_active_path_x1y1x2y2  [4];

    ogl_ui_get_property(_ui_active_camera_control,
                        OGL_UI_DROPDOWN_PROPERTY_X1Y1X2Y2,
                        prev_active_camera_x1y1x2y2);
    ogl_ui_get_property(_ui_active_path_control,
                        OGL_UI_DROPDOWN_PROPERTY_X1Y1X2Y2,
                        prev_active_path_x1y1x2y2);

    active_path_dy              = fabs(prev_active_path_x1y1x2y2[3] - prev_active_path_x1y1x2y2[1]);
    new_active_path_x1y1x2y2[0] = prev_active_path_x1y1x2y2[0];
    new_active_path_x1y1x2y2[1] = 1.0f - (prev_active_camera_x1y1x2y2[1] - 1.0f / 720.0f);
    new_active_path_x1y1x2y2[2] = prev_active_path_x1y1x2y2[2];
    new_active_path_x1y1x2y2[3] = new_active_path_x1y1x2y2[1] - active_path_dy;

    ogl_ui_set_property(_ui_active_path_control,
                        OGL_UI_DROPDOWN_PROPERTY_X1Y1,
                        new_active_path_x1y1x2y2);
}

/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
{
    float                 camera_position[3]       = {0, 0, 0};
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_size    [2]       = {1280, 720};
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(window_size, window_x1y1x2y2);

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
    context_result           = system_window_get_context                   (_window,
                                                                           &_context);

    ASSERT_DEBUG_SYNC(context_result, "Could not retrieve OGL context");

    system_window_set_rendering_handler(_window,
                                        window_rendering_handler);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        _rendering_rbm_callback_handler,
                                        NULL);

    /* Spawn curve renderer */
    _curve_renderer = ogl_curve_renderer_create(_context, system_hashed_ansi_string_create("Curve renderer") );

    /* Let the user select the scene file */
    _selected_scene_data_filename = system_file_enumerator_choose_file_via_ui(SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD,
                                                                              system_hashed_ansi_string_create("*.scene"),
                                                                              system_hashed_ansi_string_create("Emerald Scene files"),
                                                                              system_hashed_ansi_string_create("Select Emerald Scene file") );

    if (_selected_scene_data_filename == NULL)
    {
        goto end;
    }

    _test_scene = scene_load(_context,
                             _selected_scene_data_filename);

    /* Determine animation duration */
    float                animation_duration_float = 0.0f;
    system_timeline_time animation_duration       = 0;

    scene_get_property(_test_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &animation_duration_float);

    _scene_duration = system_time_get_timeline_time_for_msec( uint32_t(animation_duration_float * 1000.0f) );

    /* Enumerate all cameras */
    _camera_cs = system_critical_section_create();

    _init_cameras();

    /* Determine the animation duration. */
    scene_get_property(_test_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &_animation_duration_float);

    _animation_duration_time = system_time_get_timeline_time_for_msec( uint32_t(_animation_duration_float * 1000.0f) );

    /* Carry on initializing */
    _current_matrix = system_matrix4x4_create();
    _scene_renderer = ogl_scene_renderer_create(_context, _test_scene);

    ogl_flyby_activate(_context, camera_position);
    ogl_flyby_set_movement_delta(_context, 1.25f);

    /* Construct the pipeline object */
    _pipeline = ogl_pipeline_create(_context,
                                    true, /* should_overlay_performance_info */
                                    system_hashed_ansi_string_create("Pipeline") );

    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                                system_hashed_ansi_string_create("Scene rendering"),
                                _render_scene,
                                NULL);

    /* Set up UI */
    _setup_ui();

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler, 0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

end:
    ogl_curve_renderer_release(_curve_renderer);
    scene_release             (_test_scene);
    ogl_scene_renderer_release(_scene_renderer);
    ogl_ui_release            (_ui);
    ogl_text_release          (_text_renderer);
    ogl_pipeline_release      (_pipeline);
    system_window_close       (_window);
    system_event_release      (_window_closed_event);

    if (_cameras != NULL)
    {
        while (system_resizable_vector_get_amount_of_elements(_cameras) > 0)
        {
            _camera* current_camera = NULL;

            system_resizable_vector_pop(_cameras, &current_camera);
            delete current_camera;
        }
        system_resizable_vector_release(_cameras);
    }

    if (_camera_cs != NULL)
    {
        system_critical_section_release(_camera_cs);

        _camera_cs = NULL;
    }

    if (_camera_indices != NULL)
    {
        delete [] _camera_indices;

        _camera_indices = NULL;
    }

    if (_camera_names != NULL)
    {
        delete [] _camera_names;

        _camera_names = NULL;
    }

    if (_camera_path_indices != NULL)
    {
        delete [] _camera_path_indices;

        _camera_path_indices = NULL;
    }

    if (_camera_path_names != NULL)
    {
        delete [] _camera_path_names;

        _camera_names = NULL;
    }

    if (_temp_variant != NULL)
    {
        system_variant_release(_temp_variant);
    }
    return 0;
}