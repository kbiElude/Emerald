/**
 *
 * Test demo application @2015-2017
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "postprocessing/postprocessing_blur_gaussian.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene_renderer/scene_renderer.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_resizable_vector.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "app_config.h"
#include "state.h"
#include "ui.h"

ral_present_task      _clear_present_task  = nullptr;
ral_texture           _color_texture       = nullptr;
ral_texture_view      _color_texture_view  = nullptr;
ral_context           _context             = nullptr;
ral_texture           _depth_texture       = nullptr;
ral_texture_view      _depth_texture_view  = nullptr;
ral_rendering_handler _rendering_handler   = nullptr;
demo_window           _window              = nullptr;
system_event          _window_closed_event = system_event_create(true); /* manual_reset */


void _init_textures()
{
    /* Initialize rendertargets */
    ral_texture_create_info      color_texture_create_info;
    ral_texture_view_create_info color_texture_view_create_info;
    ral_texture_create_info      depth_texture_create_info;
    ral_texture_view_create_info depth_texture_view_create_info;
    uint32_t                     window_size[2];

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_size);

    color_texture_create_info.base_mipmap_depth      = 1;
    color_texture_create_info.base_mipmap_height     = window_size[1];
    color_texture_create_info.base_mipmap_width      = window_size[0];
    color_texture_create_info.description            = nullptr;
    color_texture_create_info.fixed_sample_locations = true;
    color_texture_create_info.format                 = RAL_FORMAT_SRGBA8_UNORM;
    color_texture_create_info.n_layers               = 1;
    color_texture_create_info.n_samples              = 1;
    color_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_texture_create_info.unique_name            = system_hashed_ansi_string_create("Color texture");
    color_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                       RAL_TEXTURE_USAGE_BLIT_SRC_BIT;
    color_texture_create_info.use_full_mipmap_chain  = false;

    depth_texture_create_info.base_mipmap_depth      = 1;
    depth_texture_create_info.base_mipmap_height     = window_size[1];
    depth_texture_create_info.base_mipmap_width      = window_size[0];
    depth_texture_create_info.description            = nullptr;
    depth_texture_create_info.fixed_sample_locations = true;
    depth_texture_create_info.format                 = RAL_FORMAT_DEPTH16_SNORM;
    depth_texture_create_info.n_layers               = 1;
    depth_texture_create_info.n_samples              = 1;
    depth_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    depth_texture_create_info.unique_name            = system_hashed_ansi_string_create("Depth texture");
    depth_texture_create_info.usage                  = RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depth_texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &color_texture_create_info,
                               &_color_texture);
    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &depth_texture_create_info,
                               &_depth_texture);

    color_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_color_texture);
    depth_texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_depth_texture);

    _color_texture_view = ral_texture_get_view(&color_texture_view_create_info);
    _depth_texture_view = ral_texture_get_view(&depth_texture_view_create_info);
}

void _init_present_tasks()
{
    ral_command_buffer               clear_cmd_buffer;
    ral_command_buffer_create_info   clear_task_cmd_buffer_create_info;
    ral_present_task_gpu_create_info clear_task_create_info;
    ral_present_task_io              clear_task_ios[2];

    clear_task_ios[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    clear_task_ios[0].texture_view = _color_texture_view;
    clear_task_ios[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    clear_task_ios[1].texture_view = _depth_texture_view;

    clear_task_cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT | RAL_QUEUE_GRAPHICS_BIT;
    clear_task_cmd_buffer_create_info.is_executable                           = true;
    clear_task_cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    clear_task_cmd_buffer_create_info.is_resettable                           = false;
    clear_task_cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(_context,
                                       1, /* n_command_buffers */
                                      &clear_task_cmd_buffer_create_info,
                                      &clear_cmd_buffer);

    ral_command_buffer_start_recording(clear_cmd_buffer);
    {
        ral_command_buffer_clear_rt_binding_command_info       clear_op;
        ral_command_buffer_set_color_rendertarget_command_info color_rt       = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        uint32_t                                               window_size[2];

        demo_window_get_property(_window,
                                 DEMO_WINDOW_PROPERTY_RESOLUTION,
                                 window_size);

        clear_op.clear_regions[0].n_base_layer             = 0;
        clear_op.clear_regions[0].n_layers                 = 1;
        clear_op.clear_regions[0].size[0]                  = window_size[0];
        clear_op.clear_regions[0].size[1]                  = window_size[1];
        clear_op.clear_regions[0].xy[0]                    = 0;
        clear_op.clear_regions[0].xy[1]                    = 0;
        clear_op.n_clear_regions                           = 1;
        clear_op.n_rendertargets                           = 2;
        clear_op.rendertargets[0].aspect                   = RAL_TEXTURE_ASPECT_COLOR_BIT;
        clear_op.rendertargets[0].clear_value.color.f32[0] = 0.0f;
        clear_op.rendertargets[0].clear_value.color.f32[1] = 0.0f;
        clear_op.rendertargets[0].clear_value.color.f32[2] = 0.5f;
        clear_op.rendertargets[0].clear_value.color.f32[3] = 0.0f;
        clear_op.rendertargets[0].rt_index                 = 0;
        clear_op.rendertargets[1].aspect                   = RAL_TEXTURE_ASPECT_DEPTH_BIT;
        clear_op.rendertargets[1].clear_value.depth        = 1.0f;
        clear_op.rendertargets[1].rt_index                 = -1;

        color_rt.rendertarget_index = 0;
        color_rt.texture_view       = _color_texture_view;

        ral_command_buffer_record_set_color_rendertargets   (clear_cmd_buffer,
                                                             1, /* n_rendertargets */
                                                            &color_rt);
        ral_command_buffer_record_set_depth_rendertarget    (clear_cmd_buffer,
                                                             _depth_texture_view);
        ral_command_buffer_record_clear_rendertarget_binding(clear_cmd_buffer,
                                                             1, /* n_clear_ops */
                                                            &clear_op);
    }
    ral_command_buffer_stop_recording(clear_cmd_buffer);

    clear_task_create_info.command_buffer   = clear_cmd_buffer;
    clear_task_create_info.n_unique_inputs  = sizeof(clear_task_ios) / sizeof(clear_task_ios[0]);
    clear_task_create_info.n_unique_outputs = sizeof(clear_task_ios) / sizeof(clear_task_ios[0]);
    clear_task_create_info.unique_inputs    = clear_task_ios;
    clear_task_create_info.unique_outputs   = clear_task_ios;

    _clear_present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Clear rendertargets"),
                                                     &clear_task_create_info);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&clear_cmd_buffer) );
}

/** Rendering handler */
ral_present_job _render_frame(ral_context                                                context,
                              void*                                                      user_arg,
                              const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr)
{
    scene_camera        current_camera         = NULL;
    scene_renderer      current_renderer       = NULL;
    scene               current_scene          = NULL;
    system_time         frame_time             = 0;
    uint32_t            result_io_index        = -1;
    ral_present_job     result_present_job     = nullptr;
    ral_present_task    scene_render_task      = nullptr;
    ral_present_task_id scene_render_task_id;

    state_set_current_frame_time      (frame_data_ptr->frame_time);
    state_get_current_frame_properties(&current_scene,
                                       &current_camera,
                                       &current_renderer,
                                       &frame_time);

    /* Update projection matrix */
    float            new_zfar   = CAMERA_SETTING_Z_FAR;
    float            new_znear  = 0.1f;
    system_matrix4x4 projection = NULL;
    float            yfov_value;

    scene_camera_get_property(current_camera,
                              SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,
                              frame_time,
                              &yfov_value);

    scene_camera_set_property(current_camera,
                              SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                             &new_zfar);
    scene_camera_set_property(current_camera,
                              SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                             &new_znear);

    projection = system_matrix4x4_create_perspective_projection_matrix(yfov_value,
                                                                       float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
                                                                       new_znear,
                                                                       new_zfar);

    /* Proceed with frame rendering.
     *
     * First, traverse the scene graph to compute node transformation matrices.
     */
    scene_graph scene_renderer_graph = NULL;

    scene_renderer_get_property(current_renderer,
                                SCENE_RENDERER_PROPERTY_GRAPH,
                               &scene_renderer_graph);

    scene_graph_lock(scene_renderer_graph);
    {
        scene_graph_compute(scene_renderer_graph,
                            frame_time);
    }
    scene_graph_unlock(scene_renderer_graph);

    /* Retrieve node that contains the transformation matrix for the camera */
    scene_graph_node scene_camera_node                  = NULL;
    system_matrix4x4 scene_camera_transformation_matrix = NULL;

    scene_camera_get_property(current_camera,
                              SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                              0, /* time - irrelevant for the owner graph node */
                             &scene_camera_node);

    scene_graph_node_get_property(scene_camera_node,
                                  SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                 &scene_camera_transformation_matrix);

    /* Configure view matrix, using the selected camera's transformation matrix */
    system_matrix4x4 view = system_matrix4x4_create();

    system_matrix4x4_set_from_matrix4x4 (view,
                                         scene_camera_transformation_matrix);
    system_matrix4x4_invert             (view);

    /* Render the scene graph */
    scene_render_task = scene_renderer_get_present_task_for_scene_graph(
        current_renderer,
        view,
        projection,
        current_camera,
        RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
        true,  /* apply_shadow_mapping */
        HELPER_VISUALIZATION_BOUNDING_BOXES,
        frame_time,
        _color_texture_view,
        _depth_texture_view);

    /* Construct the result present job */
    result_present_job = ral_present_job_create();

    ral_present_task_add_subtask_to_group_task(scene_render_task,
                                               _clear_present_task,
                                               RAL_PRESENT_TASK_SUBTASK_ROLE_PRODUCER);
    ral_present_job_add_task                  (result_present_job,
                                               scene_render_task,
                                              &scene_render_task_id);

    ral_present_task_get_io_index(scene_render_task,
                                  RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                  _color_texture_view,
                                  &result_io_index);

    ral_present_job_set_presentable_output(result_present_job,
                                           scene_render_task_id,
                                           false,            /* is_input_io */
                                           result_io_index); /* n_io        */

    /* All done */
    ral_present_task_release(scene_render_task);
    system_matrix4x4_release(view);
    system_matrix4x4_release(projection);

    return result_present_job;
}

void _rendering_rbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

bool _rendering_window_closed_callback_handler(system_window window,
                                               void*         unused)
{
    system_event_set(_window_closed_event);

    return true;
}

void _rendering_window_closing_callback_handler(system_window window,
                                                void*         unused)
{
    ui_deinit   ();
    state_deinit();
}

/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
#else
    int main()
#endif
{
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _render_frame;
    system_screen_mode                      screen_mode        = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Demo test app");
    int                                     window_x1y1x2y2[4] = {0};

    window_create_info.resolution[0] = WINDOW_WIDTH;
    window_create_info.resolution[1] = WINDOW_HEIGHT;

    _window = demo_app_create_window(window_name,
                                     window_create_info,
                                     RAL_BACKEND_TYPE_GL,
                                     false /* use_timeline */);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &_rendering_handler);

    ral_rendering_handler_set_property(_rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _rendering_window_closed_callback_handler,
                                        NULL);
    demo_window_add_callback_func      (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _rendering_window_closing_callback_handler,
                                        NULL);

    /* Initialize data required to run the demo */
    state_init         ();
    ui_init            ();
    _init_textures     ();
    _init_present_tasks();

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    return 0;
}