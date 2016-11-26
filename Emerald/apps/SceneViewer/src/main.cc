/**
 *
 * Emerald scene viewer (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "main.h"
#include "demo/demo_app.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_light.h"
#include "scene_renderer/scene_renderer.h"
#include "system/system_event.h"
#include "system/system_file_enumerator.h"
#include "system/system_matrix4x4.h"
#include "system/system_screen_mode.h"
#include "system/system_variant.h"
#include "varia/varia_curve_renderer.h"
#include <string>
#include <sstream>
#include "app_config.h"
#include "state.h"
#include "ui.h"
#include "ui/ui.h"
#include "ui/ui_dropdown.h"
#include "ui/ui_texture_preview.h"

ral_present_task      _clear_present_task  = nullptr;
ral_texture           _color_texture       = nullptr;
ral_texture_view      _color_texture_view  = nullptr;
ral_context           _context             = nullptr;
ral_texture           _depth_texture       = nullptr;
ral_texture_view      _depth_texture_view  = nullptr;
ral_rendering_handler _rendering_handler   = nullptr;
demo_window           _window              = nullptr;
system_event          _window_closed_event = system_event_create(true); /* manual_reset */
GLuint                _vao_id              = 0;

extern demo_flyby     _flyby;

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
    color_texture_create_info.fixed_sample_locations = true;
    color_texture_create_info.format                 = RAL_FORMAT_RGBA8_UNORM;
    color_texture_create_info.name                   = system_hashed_ansi_string_create("Color texture");
    color_texture_create_info.n_layers               = 1;
    color_texture_create_info.n_samples              = 1;
    color_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    color_texture_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                       RAL_TEXTURE_USAGE_BLIT_SRC_BIT;
    color_texture_create_info.use_full_mipmap_chain  = false;

    depth_texture_create_info.base_mipmap_depth      = 1;
    depth_texture_create_info.base_mipmap_height     = window_size[1];
    depth_texture_create_info.base_mipmap_width      = window_size[0];
    depth_texture_create_info.fixed_sample_locations = true;
    depth_texture_create_info.format                 = RAL_FORMAT_DEPTH16_SNORM;
    depth_texture_create_info.name                   = system_hashed_ansi_string_create("Depth texture");
    depth_texture_create_info.n_layers               = 1;
    depth_texture_create_info.n_samples              = 1;
    depth_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
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

    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &color_texture_view_create_info,
                                    &_color_texture_view);
    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &depth_texture_view_create_info,
                                    &_depth_texture_view);
}

/** Rendering handler */
ral_present_job _render_frame(ral_context                                                context,
                              void*                                                      user_arg,
                              const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr)
{
    scene_camera        camera                 = nullptr;
    bool                camera_is_flyby_active = false;
    ral_present_task_id clear_present_task_id;
    system_time         frame_time;
    system_matrix4x4    projection             = system_matrix4x4_create();
    ral_present_job     result_present_job     = nullptr;
    ral_present_task    scene_render_task      = nullptr;
    ral_present_task_id scene_render_task_id;
    system_matrix4x4    view                   = system_matrix4x4_create();

    /* Render the scene */
    if (!state_get_playback_status() )
    {
        frame_time = state_get_last_frame_time();
    }
    else
    {
        static system_time start_time = system_time_now();

        #ifdef ENABLE_ANIMATION
           frame_time = (system_time_now() - start_time) %
                        state_get_animation_duration_time();
        #else
           frame_time = 0;
        #endif
    }

    /* Update view matrix */
    state_lock_current_camera(&camera,
                              &camera_is_flyby_active);
    {
        /* If there is no projection matrix available, initialize one now */
        /* Retrieve the camera descriptro */
        ui_control active_path_control = ui_get_active_path_control();
        float      new_zfar            = CAMERA_SETTING_Z_FAR;
        float      new_znear           = 1.0f;

        if (!camera_is_flyby_active)
        {
            bool new_visibility = false;

            /* Create projection matrix */
            float yfov_value;

            scene_camera_get_property(camera,
                                      SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,
                                      frame_time,
                                      &yfov_value);

            scene_camera_set_property(camera,
                                      SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                     &new_zfar);
            scene_camera_set_property(camera,
                                      SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                     &new_znear);

            projection = system_matrix4x4_create_perspective_projection_matrix(yfov_value,
                                                                               float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
                                                                               new_znear,
                                                                               new_zfar);

            if (active_path_control != nullptr)
            {
#if 0
                ui_set_control_property(active_path_control,
                                        UI_CONTROL_PROPERTY_DROPDOWN_VISIBLE,
                                       &new_visibility);
#endif
            }
        }
        else
        {
            bool new_visibility = true;

            projection = system_matrix4x4_create_perspective_projection_matrix(45.0f,
                                                                               float(WINDOW_WIDTH) / float(WINDOW_HEIGHT),
                                                                               1.0f,
                                                                               CAMERA_SETTING_Z_FAR);

            if (active_path_control != nullptr)
            {
                ui_set_control_property(active_path_control,
                                        UI_CONTROL_PROPERTY_DROPDOWN_VISIBLE,
                                       &new_visibility);
            }

            demo_flyby_get_property(_flyby,
                                    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA,
                                   &camera);
            demo_flyby_set_property(_flyby,
                                    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_FAR,
                                   &new_zfar);
            demo_flyby_set_property(_flyby,
                                    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_NEAR,
                                   &new_znear);
        }

        if (camera_is_flyby_active)
        {
            demo_flyby_lock(_flyby);
            {
                demo_flyby_update(_flyby);

                /* Retrieve flyby view matrix */
                demo_flyby_get_property(_flyby,
                                        DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                                       &view);
            }
            demo_flyby_unlock(_flyby);
        }
        else
        {
            scene_graph scene_renderer_graph = nullptr;

            /* Compute matrices for all nodes */
            scene_renderer_get_property(state_get_scene_renderer(),
                                        SCENE_RENDERER_PROPERTY_GRAPH,
                                       &scene_renderer_graph);

            scene_graph_lock(scene_renderer_graph);
            {
                scene_graph_compute(scene_renderer_graph,
                                    frame_time);

                /* Retrieve node that contains the transformation matrix for the camera */
                scene_graph_node scene_camera_node                  = nullptr;
                system_matrix4x4 scene_camera_transformation_matrix = nullptr;

                scene_camera_get_property(camera,
                                          SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,
                                          0, /* time - irrelevant for the owner graph node */
                                         &scene_camera_node);

                scene_graph_node_get_property(scene_camera_node,
                                              SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                             &scene_camera_transformation_matrix);

                /* Extract camera location.
                 *
                 * For the view matrix, we need to take the inverse of the transformation matrix.
                 **/
                system_matrix4x4_set_from_matrix4x4 (view,
                                                     scene_camera_transformation_matrix);
                system_matrix4x4_invert             (view);
            }
            scene_graph_unlock(scene_renderer_graph);
        }
    }
    state_unlock_current_camera();

    /* Traverse the scene graph */
    scene_render_task = scene_renderer_get_present_task_for_scene_graph(
        state_get_scene_renderer(),
        view,
        projection,
        camera,
        RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
#ifdef ENABLE_SM
        true,  /* apply_shadow_mapping */
#else
        false, /* apply_shadow_mapping */
#endif
#ifdef ENABLE_BB_VISUALIZATION
        HELPER_VISUALIZATION_BOUNDING_BOXES,
#else
        HELPER_VISUALIZATION_NONE,
#endif
        frame_time,
        _color_texture_view,
        _depth_texture_view);

    #if 0
        /* Draw curves marked as active. **/
        const varia_curve_item_id current_curve_item_id = state_get_curve_renderer_item_id();
        varia_curve_renderer      curve_renderer        = state_get_curve_renderer();

        if (current_curve_item_id != -1 &&
            camera_is_flyby_active)
        {
            system_matrix4x4 vp = system_matrix4x4_create_by_mul(projection,
                                                                 view);

            varia_curve_renderer_draw(state_get_curve_renderer(),
                                      1,
                                     &current_curve_item_id,
                                      vp);

            system_matrix4x4_release(vp);
        }
    #endif

    #if 0
        TODO

        /* Configure texture preview. Shadow maps are assigned from the texture pool, so we need to
         * refresh the texture assigned to the texture preview control every frame
         */
        scene_light      light              = scene_get_light_by_index(state_get_scene(),
                                                                       0);
        ral_texture_view light_sm           = nullptr;
        ui_control       ui_texture_preview = ui_get_texture_preview_control();

        scene_light_get_property(light,
                                 SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_VIEW_COLOR_RAL,
                                &light_sm);

        if (ui_texture_preview != nullptr)
        {
            ui_set_control_property(ui_texture_preview,
                                    UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_VIEW_RAL,
                                   &light_sm);
        }
    #endif

    /* Construct the result present task */
    result_present_job = ral_present_job_create();

    ral_present_job_add_task(result_present_job,
                             scene_render_task,
                            &scene_render_task_id);
    ral_present_job_add_task(result_present_job,
                             _clear_present_task,
                            &clear_present_task_id);

    ral_present_job_connect_tasks(result_present_job,
                                  clear_present_task_id,
                                  0, /* n_src_task_output */
                                  scene_render_task_id,
                                  0,        /* n_dst_task_input          */
                                  nullptr); /* out_opt_connection_id_ptr */
    ral_present_job_connect_tasks(result_present_job,
                                  clear_present_task_id,
                                  1, /* n_src_task_output */
                                  scene_render_task_id,
                                  1,        /* n_dst_task_input          */
                                  nullptr); /* out_opt_connection_id_ptr */

    ral_present_job_set_presentable_output(result_present_job,
                                           scene_render_task_id,
                                           false, /* is_input_io */
                                           0);    /* n_io        */

    /* All done */
    state_set_last_frame_time(frame_time);

    ral_present_task_release(scene_render_task);
    system_matrix4x4_release(view);
    system_matrix4x4_release(projection);

    return result_present_job;
}

bool _rendering_key_down_callback_handler(system_window  window,
                                          unsigned short key,
                                          void*          unused)
{
    bool result = true;

    if (key == ' ')
    {
        state_set_playback_status(!state_get_playback_status() );

        result = false;
    }

    return result;
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
    ral_texture textures_to_release[] =
    {
        _color_texture,
        _depth_texture
    };
    ral_texture_view texture_views_to_release[] =
    {
        _color_texture_view,
        _depth_texture_view
    };
    const uint32_t n_textures_to_release      = sizeof(textures_to_release)      / sizeof(textures_to_release     [0]);
    const uint32_t n_texture_views_to_release = sizeof(texture_views_to_release) / sizeof(texture_views_to_release[0]);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               reinterpret_cast<void* const*>(textures_to_release) );
    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               n_texture_views_to_release,
                               reinterpret_cast<void* const*>(texture_views_to_release) );

    ral_present_task_release(_clear_present_task);
    _clear_present_task = nullptr;

    ui_deinit   ();
    state_deinit();
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
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _render_frame;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Scene viewer");
    int                                     window_x1y1x2y2[4] = {0};

    /* Let the user select the scene file */
    const system_hashed_ansi_string filters[] =
    {
        system_hashed_ansi_string_create("Packed Blob Files"),
        system_hashed_ansi_string_create("Scene Blob Files")
    };
    const system_hashed_ansi_string filter_extensions[] =
    {
        system_hashed_ansi_string_create("*.packed"),
        system_hashed_ansi_string_create("*.scene")
    };

    system_hashed_ansi_string scene_filename = system_file_enumerator_choose_file_via_ui(SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD,
                                                                                         2, /* n_filters */
                                                                                         filters,
                                                                                         filter_extensions,
                                                                                         system_hashed_ansi_string_create("Select Emerald Scene file") );

    if (scene_filename == nullptr)
    {
        goto end;
    }

    /* Carry on */
    system_screen_mode screen_mode = nullptr;

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    window_create_info.resolution + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    window_create_info.resolution + 1);

    window_create_info.resolution[0] /= 2;
    window_create_info.resolution[1] /= 2;

   _window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_FLYBY,
                            &_flyby);
    demo_window_get_property(_window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &_rendering_handler);

    ral_rendering_handler_set_property(_rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_rbm_callback_handler,
                                  nullptr);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_KEY_DOWN,
                                  (void*) _rendering_key_down_callback_handler,
                                  nullptr);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _rendering_window_closed_callback_handler,
                                  nullptr);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _rendering_window_closing_callback_handler,
                                  nullptr);

    /* Initialize various states required to run the demo */
    if (!state_init(scene_filename) )
    {
        goto end;
    }

    /* Set up UI */
    ui_init();

    /* Set up stuff */
    _init_textures     ();
    _init_present_tasks();

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* time */);
    system_event_wait_single   (_window_closed_event);

end:
    /* Clean up */
    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    main_force_deinit();
    return 0;
}