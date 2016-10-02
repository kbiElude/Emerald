/**
 *
 * Julia 4D demo test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "stage_step_light.h"
#include "demo/demo_flyby.h"
#include "demo/demo_window.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "shaders/shaders_fragment_static.h"
#include "system/system_matrix4x4.h"
#include <string.h>

ral_command_buffer       _light_cmd_buffer                = nullptr;
uint32_t                 _light_color_ub_offset           = -1;
ral_shader               _light_fs                        = nullptr;
ral_gfx_state            _light_gfx_state                 = nullptr;
uint32_t                 _light_mvp_ub_offset             = -1;
uint32_t                 _light_position_ub_offset        = -1;
ral_present_task         _light_present_task              = nullptr;
ral_program              _light_program                   =  0;
ral_program_block_buffer _light_program_datafs_ub         = nullptr;
ral_program_block_buffer _light_program_datavs_ub         = nullptr;
uint32_t                 _light_vertex_attribute_location = -1;
system_matrix4x4         _light_view_matrix               = nullptr;

static const char* _light_vs_shader_body =
    "#version 430 core\n"
    "\n"
    "layout(binding = 1) uniform dataVS\n"
    "{\n"
    "    mat4 mvp;\n"
    "    vec4 position;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position  = mvp * position;\n"
    "   gl_PointSize = 16.0;\n"
    "}\n";

/** TODO */
void _stage_step_light_update_buffers(void* unused)
{
    /* Calculate MVP matrix */
    float            camera_location[3];
    system_matrix4x4 mvp             = nullptr;

    if (_light_view_matrix == nullptr)
    {
        _light_view_matrix = system_matrix4x4_create();
    }

    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,
                            camera_location);
    demo_flyby_get_property(_flyby,
                            DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
                           &_light_view_matrix);

    mvp = system_matrix4x4_create_by_mul(main_get_projection_matrix(),
                                         _light_view_matrix);

    /* Set up uniforms */
    static float  gpu_light_position[4] = {0, 0, 0, 1};
    const  float* light_color           = main_get_light_color();
    const  float* light_position        = main_get_light_position();

    ral_program_block_buffer_set_nonarrayed_variable_value(_light_program_datafs_ub,
                                                           _light_color_ub_offset,
                                                           light_color,
                                                           sizeof(float) * 4);

    ral_program_block_buffer_set_nonarrayed_variable_value(_light_program_datavs_ub,
                                                           _light_mvp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(mvp),
                                                           sizeof(float) * 16);
    ral_program_block_buffer_set_nonarrayed_variable_value(_light_program_datavs_ub,
                                                           _light_position_ub_offset,
                                                           light_position,
                                                           sizeof(float) * 4);

    ral_program_block_buffer_sync_immediately(_light_program_datafs_ub);
    ral_program_block_buffer_sync_immediately(_light_program_datavs_ub);

    system_matrix4x4_release(mvp);
}

/* Please see header for specification */
PUBLIC void stage_step_light_deinit(ral_context context)
{
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_light_cmd_buffer) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_light_gfx_state) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_light_program) );

    if (_light_present_task != nullptr)
    {
        ral_present_task_release(_light_present_task);

        _light_present_task = nullptr;
    }

    if (_light_program_datafs_ub != nullptr)
    {
        ral_program_block_buffer_release(_light_program_datafs_ub);

        _light_program_datafs_ub = nullptr;
    }

    if (_light_program_datavs_ub != nullptr)
    {
        ral_program_block_buffer_release(_light_program_datavs_ub);

        _light_program_datavs_ub = nullptr;
    }

    system_matrix4x4_release(_light_view_matrix);
}

/* Please see header for specification */
PUBLIC void stage_step_light_init(ral_context      context,
                                  ral_texture_view color_rt_texture_view,
                                  ral_texture_view depth_rt_texture_view)
{
    shaders_fragment_static fragment_shader = nullptr;
    ral_shader              vertex_shader   = nullptr;

    /* Prepare a fragment shader */
    fragment_shader = shaders_fragment_static_create(context,
                                                 system_hashed_ansi_string_create("light fragment") );

    /* Prepare a vertex shader */
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create("light vs"),
        RAL_SHADER_TYPE_VERTEX,
    };

    vertex_shader = ral_shader_create(&vs_create_info);

    ral_shader_set_property(vertex_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &_light_vs_shader_body);

    /* Prepare a program */
    const ral_program_create_info light_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("light program")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &light_po_create_info,
                               &_light_program);

    ral_program_attach_shader(_light_program,
                              shaders_fragment_static_get_shader(fragment_shader),
                              true /* async */);
    ral_program_attach_shader(_light_program,
                              vertex_shader,
                              true /* async */);

    shaders_fragment_static_release(fragment_shader);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                              &vertex_shader);

    /* Retrieve attribute/uniform locations */
    const ral_program_variable* color_uniform_ral_ptr    = nullptr;
    const ral_program_variable* mvp_uniform_ral_ptr      = nullptr;
    const ral_program_variable* position_uniform_ral_ptr = nullptr;

    ral_program_get_block_variable_by_name(_light_program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("color"),
                                          &color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_light_program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("mvp"),
                                          &mvp_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(_light_program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("position"),
                                          &position_uniform_ral_ptr);

    _light_color_ub_offset    = (color_uniform_ral_ptr    != nullptr) ? color_uniform_ral_ptr->block_offset    : -1;
    _light_mvp_ub_offset      = (mvp_uniform_ral_ptr      != nullptr) ? mvp_uniform_ral_ptr->block_offset      : -1;
    _light_position_ub_offset = (position_uniform_ral_ptr != nullptr) ? position_uniform_ral_ptr->block_offset : -1;

    /* Retrieve uniform block properties */
    ral_buffer datafs_ub_bo_ral = nullptr;
    ral_buffer datavs_ub_bo_ral = nullptr;

    _light_program_datafs_ub = ral_program_block_buffer_create(context,
                                                               _light_program,
                                                               system_hashed_ansi_string_create("dataFS") );
    _light_program_datavs_ub = ral_program_block_buffer_create(context,
                                                               _light_program,
                                                               system_hashed_ansi_string_create("dataVS") );

    ral_program_block_buffer_get_property(_light_program_datafs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datafs_ub_bo_ral);
    ral_program_block_buffer_get_property(_light_program_datavs_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datavs_ub_bo_ral);

    /* Set up gfx state */
    ral_gfx_state_create_info gfx_state_create_info;

    gfx_state_create_info.depth_test            = true;
    gfx_state_create_info.depth_test_compare_op = RAL_COMPARE_OP_LESS;
    gfx_state_create_info.primitive_type        = RAL_PRIMITIVE_TYPE_POINTS;

    ral_context_create_gfx_states(context,
                                  1, /* n_create_info_items */
                                 &gfx_state_create_info,
                                 &_light_gfx_state);

    /* Set up matrices */
    _light_view_matrix = system_matrix4x4_create();

    /* Prepare a command buffer which draws the light */
    ral_command_buffer_create_info cmd_buffer_create_info;

    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_executable                           = false;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &_light_cmd_buffer);

    ral_command_buffer_start_recording(_light_cmd_buffer);
    {
        ral_command_buffer_set_color_rendertarget_command_info color_rt      = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        ral_command_buffer_draw_call_regular_command_info      draw_call;
        ral_command_buffer_set_binding_command_info            ub_bindings[2];
        const uint32_t                                         n_ub_bindings = sizeof(ub_bindings) / sizeof(ub_bindings[0]);

        draw_call.base_instance = 0;
        draw_call.base_vertex   = 0;
        draw_call.n_instances   = 1;
        draw_call.n_vertices    = 1;

        color_rt.texture_view = color_rt_texture_view;

        ub_bindings[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_bindings[0].name                          = system_hashed_ansi_string_create("dataFS");
        ub_bindings[0].uniform_buffer_binding.buffer = datafs_ub_bo_ral;
        ub_bindings[0].uniform_buffer_binding.offset = 0;
        ub_bindings[0].uniform_buffer_binding.size   = 0;

        ub_bindings[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_bindings[1].name                          = system_hashed_ansi_string_create("dataVS");
        ub_bindings[1].uniform_buffer_binding.buffer = datavs_ub_bo_ral;
        ub_bindings[1].uniform_buffer_binding.offset = 0;
        ub_bindings[1].uniform_buffer_binding.size   = 0;

        ral_command_buffer_record_set_program            (_light_cmd_buffer,
                                                          _light_program);
        ral_command_buffer_record_set_bindings           (_light_cmd_buffer,
                                                          n_ub_bindings,
                                                          ub_bindings);
        ral_command_buffer_record_set_color_rendertargets(_light_cmd_buffer,
                                                          1, /* n_rendertargets */
                                                         &color_rt);
        ral_command_buffer_record_set_depth_rendertarget (_light_cmd_buffer,
                                                          depth_rt_texture_view);
        ral_command_buffer_record_set_gfx_state          (_light_cmd_buffer,
                                                          _light_gfx_state);

        ral_command_buffer_record_draw_call_regular(_light_cmd_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call);
    }
    ral_command_buffer_stop_recording(_light_cmd_buffer);

    /* Prepare a present task which uses the cmd buffer above to draw the light */
    ral_present_task                    cpu_task;
    ral_present_task_cpu_create_info    cpu_task_create_info;
    ral_present_task_io                 cpu_task_outputs[2];
    ral_present_task                    gpu_task;
    ral_present_task_gpu_create_info    gpu_task_create_info;
    ral_present_task_io                 gpu_task_inputs[4];
    ral_present_task_io                 gpu_task_outputs[2];
    ral_present_task_group_create_info  group_task_create_info;
    ral_present_task_ingroup_connection group_task_connections    [2];
    ral_present_task_group_mapping      group_task_input_mappings [2];
    ral_present_task_group_mapping      group_task_output_mappings[2];
    ral_present_task                    group_task_present_tasks  [2];

    cpu_task_outputs[0].buffer      = datafs_ub_bo_ral;
    cpu_task_outputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    cpu_task_outputs[1].buffer      = datavs_ub_bo_ral;
    cpu_task_outputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_task_create_info.cpu_task_callback_user_arg = nullptr;
    cpu_task_create_info.n_unique_inputs            = 0;
    cpu_task_create_info.n_unique_outputs           = sizeof(cpu_task_outputs) / sizeof(cpu_task_outputs[0]);
    cpu_task_create_info.pfn_cpu_task_callback_proc = _stage_step_light_update_buffers;
    cpu_task_create_info.unique_inputs              = nullptr;
    cpu_task_create_info.unique_outputs             = cpu_task_outputs;


    gpu_task_inputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    gpu_task_inputs[0].texture_view = color_rt_texture_view;
    gpu_task_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    gpu_task_inputs[1].texture_view = depth_rt_texture_view;
    gpu_task_inputs[2].buffer       = datafs_ub_bo_ral;
    gpu_task_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    gpu_task_inputs[3].buffer       = datavs_ub_bo_ral;
    gpu_task_inputs[3].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    gpu_task_outputs[0]             = gpu_task_inputs[0];
    gpu_task_outputs[1]             = gpu_task_inputs[1];

    gpu_task_create_info.command_buffer   = _light_cmd_buffer;
    gpu_task_create_info.n_unique_inputs  = sizeof(gpu_task_inputs)  / sizeof(gpu_task_inputs [0]);
    gpu_task_create_info.n_unique_outputs = sizeof(gpu_task_outputs) / sizeof(gpu_task_outputs[0]);
    gpu_task_create_info.unique_inputs    = gpu_task_inputs;
    gpu_task_create_info.unique_outputs   = gpu_task_outputs;

    cpu_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Light: buffer update"),
                                          &cpu_task_create_info);
    gpu_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Light: rendering"),
                                          &gpu_task_create_info);


    group_task_connections[0].input_present_task_index     = 1; /* gpu_present_task */
    group_task_connections[0].input_present_task_io_index  = 2; /* datafs_ub_bo_ral */
    group_task_connections[0].output_present_task_index    = 0; /* cpu_present_task */
    group_task_connections[0].output_present_task_io_index = 0; /* datafs_ub_bo_ral */
    group_task_connections[1].input_present_task_index     = 1; /* gpu_present_task */
    group_task_connections[1].input_present_task_io_index  = 3; /* datavs_ub_bo_ral */
    group_task_connections[1].output_present_task_index    = 0; /* cpu_present_task */
    group_task_connections[1].output_present_task_io_index = 1; /* datavs_ub_bo_ral */

    group_task_present_tasks[0] = cpu_task;
    group_task_present_tasks[1] = gpu_task;

    group_task_input_mappings[0].group_task_io_index   = 0;
    group_task_input_mappings[0].n_present_task        = 1; /* gpu_task              */
    group_task_input_mappings[0].present_task_io_index = 0; /* color_rt_texture_view */
    group_task_input_mappings[1].group_task_io_index   = 0;
    group_task_input_mappings[1].n_present_task        = 1; /* gpu_task              */
    group_task_input_mappings[1].present_task_io_index = 1; /* depth_rt_texture_view */
    group_task_output_mappings[0]                      = group_task_input_mappings[0];
    group_task_output_mappings[1]                      = group_task_input_mappings[1];

    group_task_create_info.ingroup_connections                      = group_task_connections;
    group_task_create_info.n_ingroup_connections                    = sizeof(group_task_connections)   / sizeof(group_task_connections  [0]);
    group_task_create_info.n_present_tasks                          = sizeof(group_task_present_tasks) / sizeof(group_task_present_tasks[0]);
    group_task_create_info.n_total_unique_inputs                    = 0;
    group_task_create_info.n_total_unique_outputs                   = 0;
    group_task_create_info.n_unique_input_to_ingroup_task_mappings  = sizeof(group_task_input_mappings)  / sizeof(group_task_input_mappings [0]);
    group_task_create_info.n_unique_output_to_ingroup_task_mappings = sizeof(group_task_output_mappings) / sizeof(group_task_output_mappings[0]);
    group_task_create_info.present_tasks                            = group_task_present_tasks;
    group_task_create_info.unique_input_to_ingroup_task_mapping     = group_task_input_mappings;
    group_task_create_info.unique_output_to_ingroup_task_mapping    = group_task_output_mappings;

    _light_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("Light: render light preview"),
                                                       &group_task_create_info);

    /* Clean up */
    ral_present_task_release(cpu_task);
    ral_present_task_release(gpu_task);
}
