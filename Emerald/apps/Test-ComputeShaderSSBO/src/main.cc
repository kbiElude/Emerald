/**
 *
 * Compute shader + SSBO test app (kbi/elude @2015 - 2016)
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include "../../include/main.h"
#include <algorithm>

#ifdef _WIN32
    #undef min
#endif

ral_buffer       _bo                   = nullptr;
ral_context      _context              = nullptr;
uint32_t         _local_workgroup_size = 0;
ral_program      _program              = nullptr;
ral_texture      _texture              = nullptr;
ral_texture_view _texture_view         = nullptr;
system_event     _window_closed_event  = system_event_create(true); /* manual_reset */
int              _window_size[2]       = {1280, 720};

ral_command_buffer _cmd_buffer_preamble            = nullptr;
uint32_t           _cmd_buffer_preamble_n_commands = 0;


const char* _cs_body_preamble = "#version 430 core\n"
                                "\n"
                                "#extension GL_ARB_compute_shader               : require\n"
                                "#extension GL_ARB_shader_storage_buffer_object : require\n"
                                "\n";

const char* _cs_body_core     = "layout (local_size_x = LOCAL_SIZE, local_size_y = LOCAL_SIZE) in;\n"
                                "\n"
                                "layout(rgba8) restrict writeonly uniform image2D result;\n"
                                "\n"
                                "layout(std140) restrict readonly buffer dataBlock\n"
                                "{\n"
                                "    uint test_counter;"
                                "};\n"
                                "\n"
                                "void main()\n"
                                "{\n"
                                "    const uvec2 current_xy   = gl_GlobalInvocationID.xy;\n"
                                "    const uvec2 image_size   = imageSize(result);\n"
                                "           vec4 result_value = vec4(float (test_counter + current_xy.x                 % image_size.x)                / float(image_size.x),\n"
                                "                                    float (test_counter + current_xy.y                 % image_size.y)                / float(image_size.y),\n"
                                "                                    float((test_counter + current_xy.x + current_xy.y) % image_size.x + image_size.y) / float(image_size.x + image_size.y),\n"
                                "                                    1.0);\n"
                                "\n"
                                "    imageStore(result, ivec2(current_xy), result_value);\n"
                                "}";

/** TODO */
system_hashed_ansi_string _get_cs_body()
{
    const uint32_t*           max_local_work_group_dimensions  = nullptr;
    uint32_t                  max_local_work_group_invocations = 0;
    system_hashed_ansi_string result                           = nullptr;

    ral_context_get_property(_context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                            &max_local_work_group_invocations);
    ral_context_get_property(_context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                            &max_local_work_group_dimensions);

    /* Form the body */
    char               definitions_part[1024];
    const unsigned int max_dimension_size = std::min(max_local_work_group_dimensions[0],
                                                     max_local_work_group_dimensions[1]);

    _local_workgroup_size = (int) sqrt( (float) max_local_work_group_invocations);

    if (_local_workgroup_size > max_dimension_size)
    {
        _local_workgroup_size = max_dimension_size;
    }

    snprintf(definitions_part,
             sizeof(definitions_part),
             "#define LOCAL_SIZE %d\n",
             _local_workgroup_size);

    /* Form the body */
    const char* body_strings[] =
    {
        _cs_body_preamble,
        definitions_part,
        _cs_body_core
    };
    const unsigned int n_body_strings = sizeof(body_strings) /
                                        sizeof(body_strings[0]);

    result = system_hashed_ansi_string_create_by_merging_strings(n_body_strings,
                                                                 body_strings);

    return result;
}

void _init()
{
    /* Set up the data buffer object */
    ral_buffer_create_info bo_create_info;
    const int              data_bo_contents = 0;

    bo_create_info.mappability_bits = 0;
    bo_create_info.parent_buffer    = nullptr;
    bo_create_info.property_bits    = 0;
    bo_create_info.size             = sizeof(data_bo_contents);
    bo_create_info.start_offset     = 0;
    bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT;
    bo_create_info.user_queue_bits  = RAL_QUEUE_COMPUTE_BIT | RAL_QUEUE_GRAPHICS_BIT;

    ral_context_create_buffers(_context,
                               1, /* n_buffers */
                              &bo_create_info,
                              &_bo);

    /* Set up the compute shader object */
    ral_shader                      cs             = nullptr;
    const system_hashed_ansi_string cs_body        = _get_cs_body();
    const ral_shader_create_info    cs_create_info =
    {
        system_hashed_ansi_string_create("Compute shader object"),
        RAL_SHADER_TYPE_COMPUTE
    };

    ral_context_create_shaders(_context,
                               1, /* n_create_info_items */
                              &cs_create_info,
                              &cs);

    ral_shader_set_property(cs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &cs_body);

    /* Set up the compute program object */
    const ral_program_create_info po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
        system_hashed_ansi_string_create("Program object")
    };

    ral_context_create_programs(_context,
                                1, /* n_create_info_items */
                               &po_create_info,
                               &_program);

    ral_program_attach_shader(_program,
                              cs,
                              true /* async */);

    ral_context_delete_objects(_context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&cs) );

    /* Set up the texture object we will have the CS write to. The texture mip-map will later be
     * used as a source for the blit operation which is going to fill the back buffer with data.
     */
    ral_texture_create_info      texture_create_info;
    ral_texture_view_create_info texture_view_create_info;

    texture_create_info.base_mipmap_depth      = 1;
    texture_create_info.base_mipmap_height     = _window_size[1];
    texture_create_info.base_mipmap_width      = _window_size[0];
    texture_create_info.fixed_sample_locations = true;
    texture_create_info.format                 = RAL_FORMAT_RGBA8_UNORM;
    texture_create_info.name                   = system_hashed_ansi_string_create("Staging texture");
    texture_create_info.n_layers               = 1;
    texture_create_info.n_samples              = 1;
    texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    texture_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_SRC_BIT |
                                                 RAL_TEXTURE_USAGE_IMAGE_STORE_OPS_BIT;
    texture_create_info.use_full_mipmap_chain  = false;

    ral_context_create_textures(_context,
                                1, /* n_textures */
                               &texture_create_info,
                               &_texture);

    texture_view_create_info = ral_texture_view_create_info::ral_texture_view_create_info(_texture);

    ral_context_create_texture_views(_context,
                                     1, /* n_texture_views */
                                    &texture_view_create_info,
                                    &_texture_view);

    /* Set up preamble command buffer */
    ral_command_buffer_create_info cmd_buffer_create_info;

    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT;
    cmd_buffer_create_info.is_executable                           = false;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(_context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &_cmd_buffer_preamble);

    ral_command_buffer_start_recording(_cmd_buffer_preamble);
    {
        ral_command_buffer_set_binding_command_info bindings[2];

        bindings[0].binding_type                       = RAL_BINDING_TYPE_STORAGE_IMAGE;
        bindings[0].name                               = system_hashed_ansi_string_create("result");
        bindings[0].storage_image_binding.access_bits  = RAL_IMAGE_ACCESS_WRITE;
        bindings[0].storage_image_binding.texture_view = _texture_view;

        bindings[1].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
        bindings[1].name                          = system_hashed_ansi_string_create("dataBlock");
        bindings[1].storage_buffer_binding.buffer = _bo;
        bindings[1].storage_buffer_binding.offset = 0;
        bindings[1].storage_buffer_binding.size   = sizeof(uint32_t);

        ral_command_buffer_record_set_program (_cmd_buffer_preamble,
                                               _program);
        ral_command_buffer_record_set_bindings(_cmd_buffer_preamble,
                                               sizeof(bindings) / sizeof(bindings[0]),
                                               bindings);
    }
    ral_command_buffer_stop_recording(_cmd_buffer_preamble);

    ral_command_buffer_get_property(_cmd_buffer_preamble,
                                    RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS,
                                   &_cmd_buffer_preamble_n_commands);
}

/** Rendering handler */
ral_present_job _rendering_handler(ral_context                                                context,
                                   void*                                                      user_arg,
                                   const ral_rendering_handler_rendering_callback_frame_data* frame_data_ptr)
{
    ral_command_buffer command_buffer = nullptr;
    ral_present_job    result_job     = ral_present_job_create();

    /* Instantiate the command buffer */
    ral_command_buffer_create_info command_buffer_create_info;

    command_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT;
    command_buffer_create_info.is_executable                           = true;
    command_buffer_create_info.is_invokable_from_other_command_buffers = false;
    command_buffer_create_info.is_resettable                           = false;
    command_buffer_create_info.is_transient                            = true;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                      &command_buffer_create_info,
                                      &command_buffer);

    /* Record the command buffer */
    ral_command_buffer_start_recording(command_buffer);
    {
        ral_command_buffer_append_commands_from_command_buffer(command_buffer,
                                                               _cmd_buffer_preamble,
                                                               0, /* n_start_commands */
                                                               _cmd_buffer_preamble_n_commands);

        /* Update the test counter value */
        ral_buffer_client_sourced_update_info update;

        update.data         = &frame_data_ptr->n_frame;
        update.data_size    = sizeof(uint32_t);
        update.start_offset = 0;

        ral_command_buffer_record_update_buffer(command_buffer,
                                                _bo,
                                                0, /* start_offset */
                                                sizeof(uint32_t),
                                               &frame_data_ptr->n_frame);

        /* Run the compute shader invocations.
         *
         * NOTE: We're doing a ceiling division here to ensure the whole texture mipmap
         *       is filled with data.
         */
        const unsigned int n_global_invocations[] =
        {
            (_window_size[0] + _local_workgroup_size - 1) / _local_workgroup_size,
            (_window_size[1] + _local_workgroup_size - 1) / _local_workgroup_size,
            1
        };

        ral_command_buffer_record_dispatch(command_buffer,
                                           n_global_invocations);

        /* TODO: Make sure RAL injects a GL_FRAMEBUFFER_BARRIER_BIT before automatic buffer swap ! */
    }
    ral_command_buffer_stop_recording(command_buffer);

    /* Create a present task from our command buffer */
    ral_present_task                 present_task;
    ral_present_task_gpu_create_info present_task_create_info;
    ral_present_task_id              present_task_id;
    ral_present_task_io              present_task_unique_output;

    present_task_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    present_task_unique_output.texture_view = _texture_view;

    present_task_create_info.command_buffer   = command_buffer;
    present_task_create_info.n_unique_inputs  = 0;
    present_task_create_info.n_unique_outputs = 1;
    present_task_create_info.unique_inputs    = nullptr;
    present_task_create_info.unique_outputs   = &present_task_unique_output;

    present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Visuals"),
                                              &present_task_create_info);

    /* Configure the result present job */
    ral_present_job_add_task(result_job,
                             present_task,
                            &present_task_id);
    ral_present_task_release(present_task);

    ral_present_job_set_presentable_output(result_job,
                                           present_task_id,
                                           false, /* is_input_io */
                                           0);    /* n_io        */

    /* Release the command buffer */
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_oobjects */
                               reinterpret_cast<void**>(&command_buffer) );

    /* All done */
    return result_job;
}

/** Event callback handlers */
bool _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window,
                                              void*         unused)
{
    if (_bo != nullptr)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   reinterpret_cast<void* const*>(&_bo) );

        _bo = nullptr;
    }

    if (_cmd_buffer_preamble != nullptr)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&_cmd_buffer_preamble) );

        _cmd_buffer_preamble = nullptr;
    }

    if (_program != nullptr)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&_program) );

        _program = nullptr;
    }

    if (_texture != nullptr)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_textures */
                                   reinterpret_cast<void* const*>(&_texture) );

        _texture = nullptr;
    }

    if (_texture_view != nullptr)
    {
        ral_context_delete_objects(_context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                   1, /* n_texture_views */
                                   reinterpret_cast<void* const*>(&_texture_view) );

        _texture_view = nullptr;
    }

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
    PFNRALRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ral_rendering_handler                   rendering_handler  = nullptr;
    system_screen_mode                      screen_mode        = nullptr;
    demo_window                             window             = nullptr;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Compute shader SSBO test app");
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

    window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ral_rendering_handler_set_property(rendering_handler,
                                       RAL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  nullptr);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  nullptr);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                  (void*) _window_closing_callback_handler,
                                  nullptr);

    _init();

    demo_window_start_rendering(window,
                                0 /* rendering_start_time */);
    system_event_wait_single   (_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    demo_app_destroy_window(window_name);

    system_event_release(_window_closed_event);

    main_force_deinit();
    return 0;
}