/**
 *
 * Collada->internal Emerald scene format converter, with preview support (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve_editor/curve_editor_general.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
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
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_resources.h"
#include "system/system_window.h"
#include "collada/collada_data.h"
#include "ogl/ogl_ui.h"
#include <string>
#include <sstream>

float                     _animation_duration_float   = 0.0f;
system_time               _animation_duration_time    = 0;
ogl_context               _context                    = NULL;
system_matrix4x4          _current_matrix             = NULL;
ogl_flyby                 _flyby                      = NULL;
ogl_pipeline              _pipeline                   = NULL;
uint32_t                  _pipeline_stage_id          = -1;
ogl_scene_renderer        _scene_renderer             = NULL;
system_hashed_ansi_string _selected_collada_data_file = NULL;
collada_data              _test_collada_data          = NULL;
scene                     _test_scene                 = NULL;
ogl_text                  _text_renderer              = NULL;
ogl_ui                    _ui                         = NULL;
ogl_ui_control            _ui_convert_button          = NULL;
system_window             _window                     = NULL;
system_event              _window_closed_event        = system_event_create(true); /* manual_reset */
ogl_rendering_handler     _window_rendering_handler   = NULL;

system_matrix4x4 _projection_matrix = NULL;
GLuint           _vao_id            = 0;


system_hashed_ansi_string _get_result_file_name()
{
    /* The last four characters should be .dae. We need to replace that sub-string with a
     * new extension name. */
    const char*        input_filename_raw_ptr   = system_hashed_ansi_string_get_buffer(_selected_collada_data_file);
    const unsigned int input_filename_length    = strlen                              (input_filename_raw_ptr);
    const char*        input_filename_extension = input_filename_raw_ptr + (input_filename_length - 4);
    std::string        result_filename          = std::string(input_filename_raw_ptr,
                                                              input_filename_extension - input_filename_raw_ptr);

    ASSERT_ALWAYS_SYNC(input_filename_extension[0] == '.' &&
                       input_filename_extension[1] == 'd' &&
                       input_filename_extension[2] == 'a' &&
                       input_filename_extension[3] == 'e',
                       "Unrecognized input file name!");

    result_filename += ".scene";

    return system_hashed_ansi_string_create(result_filename.c_str() );
}

void _on_callback_convert_clicked(void* not_used,
                                  void* not_used2)
{
    /* Obtain result file name */
    system_hashed_ansi_string result_file_name = _get_result_file_name();

    /* Create the file */
    system_file_serializer serializer = system_file_serializer_create_for_writing(result_file_name);

    ASSERT_ALWAYS_SYNC(serializer != NULL,
                       "Could not create result scene serializer");

    if (serializer == NULL)
    {
        goto end;
    }

    /* Serialize the scene */
    if (!scene_save_with_serializer(_test_scene,
                                    serializer) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Scene serialization failed");
    }

    system_file_serializer_release(serializer);

    /* All done */
end:
    ;
}

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
                        void*       unused)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLClearColor(1.0f,
                                0.0f,
                                0.0f,
                                0.0f);
    entry_points->pGLClear     (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    entry_points->pGLEnable    (GL_DEPTH_TEST);
    entry_points->pGLEnable    (GL_CULL_FACE);

    /* Render the scene */
    ogl_pipeline_draw_stage(_pipeline,
                            _pipeline_stage_id,
                            frame_time,
                            rendering_area_px_topdown);
}

void _render_scene(ogl_context context,
                   system_time time,
                   const int*  rendering_area_px_topdown,
                   void*       not_used)
{
    /* Update view matrix */
    float            camera_location[4] = {0, 0, 0, 0};
    ogl_flyby        flyby              = NULL;
    system_matrix4x4 view               = system_matrix4x4_create();

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_FLYBY,
                            &flyby);

    ogl_flyby_lock(flyby);
    {
        ogl_flyby_update(flyby);

        /* Retrieve flyby details */
        ogl_flyby_get_property(flyby,
                               OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                               camera_location);
        ogl_flyby_get_property(flyby,
                               OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                              &view);
    }
    ogl_flyby_unlock(flyby);

    /* Retrieve scene camera */
    scene_camera camera = scene_get_camera_by_index(_test_scene,
                                                    0);

    /* Traverse the scene graph */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_scene_renderer_render_scene_graph(_scene_renderer,
                                          view,
                                          _projection_matrix,
                                          camera,
                                          RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                          true, /* apply_shadow_mapping */
                                          HELPER_VISUALIZATION_NONE,
                                          system_time_now() % _animation_duration_time
                                         );

    /* Render UI */
    ogl_ui_draw  (_ui);
    ogl_text_draw(_context,
                  _text_renderer);

    /* All done */
    system_matrix4x4_release(view);
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
    const float  convert_button_x1y1[2]            = {0.7f, 0.1f};
    const float  load_curve_dataset_button_x1y1[2] = {0.7f, 0.2f};
    const float  text_default_size                 = 0.5f;
    int          window_dimensions[2]              = {0};

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_dimensions);

    _text_renderer = ogl_text_create(system_hashed_ansi_string_create("Text renderer"),
                                     _context,
                                     system_resources_get_meiryo_font_table(),
                                     window_dimensions[0],
                                     window_dimensions[1]);

    ogl_text_set_text_string_property(_text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                     &text_default_size);

    _ui = ogl_ui_create(_text_renderer,
                        system_hashed_ansi_string_create("UI") );

    _ui_convert_button = ogl_ui_add_button(_ui,
                                           system_hashed_ansi_string_create("Convert"),
                                           convert_button_x1y1,
                                           _on_callback_convert_clicked,
                                           NULL);

}

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    if (_pipeline != NULL)
    {
        ogl_pipeline_release(_pipeline);
    }

    if (_scene_renderer != NULL)
    {
        ogl_scene_renderer_release(_scene_renderer);
    }
}

#ifdef _WIN32
/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle,
                   HINSTANCE,
                   LPTSTR,
                   int)
#else
int main()
#endif
{
    float       camera_position[3]   = {0, 0, 0};
    const bool  flyby_active         = true;
    const float flyby_movement_delta = 10.25f;
    int         window_size    [2]   = {1280, 720};
    int         window_x1y1x2y2[4]   = {0};

    const system_hashed_ansi_string filter_descriptions[] =
    {
        system_hashed_ansi_string_create("COLLADA files")
    };
    const system_hashed_ansi_string filter_extensions[] =
    {
        system_hashed_ansi_string_create("*.dae")
    };

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

    _window                   = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_GL,
                                                                             window_x1y1x2y2,
                                                                             system_hashed_ansi_string_create("Test window"),
                                                                             false,
                                                                             false, /* vsync_enabled */
                                                                             true,  /* visible */
                                                                             window_pf);
    _window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                             60,
                                                                             _rendering_handler,
                                                                             NULL);

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    ogl_context_get_property  (_context,
                               OGL_CONTEXT_PROPERTY_FLYBY,
                              &_flyby);

    system_window_set_property         (_window,
                                        SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                                       &_window_rendering_handler);

    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Let the user select the DAE file */
    _selected_collada_data_file = system_file_enumerator_choose_file_via_ui(SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD,
                                                                            1, /* n_filters */
                                                                            filter_descriptions,
                                                                            filter_extensions,
                                                                            system_hashed_ansi_string_create("Select COLLADA file") );

    if (_selected_collada_data_file == NULL)
    {
        goto end;
    }

    _test_collada_data = collada_data_load(_selected_collada_data_file,
                                           system_hashed_ansi_string_create("COLLADA scene"),
                                           true);

    /* Determine the animation duration. */
    collada_data_get_property(_test_collada_data,
                              COLLADA_DATA_PROPERTY_MAX_ANIMATION_DURATION,
                             &_animation_duration_float);

    _animation_duration_time = system_time_get_time_for_msec( uint32_t(_animation_duration_float * 1000.0f) );

    /* Carry on initializing */
    _test_scene = collada_data_get_emerald_scene(_test_collada_data,
                                                 _context,
                                                 0); /* n_scene */

    _current_matrix    = system_matrix4x4_create                              ();
    _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,        /* fov_y */
                                                                               1280 / 720.0f,/* ar */
                                                                               1.0f,         /* z_near */
                                                                               100000.0f);   /* z_far */

    _scene_renderer = ogl_scene_renderer_create(_context,
                                                _test_scene);

    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_IS_ACTIVE,
                          &flyby_active);
    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_CAMERA_LOCATION,
                           camera_position);

    ogl_flyby_set_property(_flyby,
                           OGL_FLYBY_PROPERTY_MOVEMENT_DELTA,
                          &flyby_movement_delta);

    /* Show the curve editor */
#if 0
    curve_editor_show        (_context);
    curve_editor_set_property(_context,
                              CURVE_EDITOR_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH,
                              &_animation_duration_float);
#endif

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
    ogl_rendering_handler_play(_window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(_window_rendering_handler);

end:
    if (_test_collada_data != NULL)
    {
        collada_data_release(_test_collada_data);
    }

    system_window_close (_window);
    system_event_release(_window_closed_event);

    return 0;
}