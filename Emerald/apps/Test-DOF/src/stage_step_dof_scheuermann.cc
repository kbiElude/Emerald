/**
 *
 * DOF test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "postprocessing/postprocessing_blur_poisson.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "stage_step_background.h"
#include "stage_step_dof_scheuermann.h"
#include "stage_step_julia.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"

#define DOWNSAMPLE_FACTOR (8)

/* General stuff */
PRIVATE postprocessing_blur_poisson _dof_scheuermann_blur_poisson                     = nullptr;
PRIVATE ral_texture                 _dof_scheuermann_combination_texture              = nullptr;
PRIVATE ral_texture_view            _dof_scheuermann_combination_texture_view         = nullptr;
PRIVATE ral_texture                 _dof_scheuermann_downsampled_blurred_texture      = nullptr;
PRIVATE ral_texture_view            _dof_scheuermann_downsampled_blurred_texture_view = nullptr;
PRIVATE ral_texture                 _dof_scheuermann_downsampled_texture              = nullptr;
PRIVATE ral_texture_view            _dof_scheuermann_downsampled_texture_view         = nullptr;
PRIVATE ral_gfx_state               _dof_scheuermann_gfx_state                        = nullptr;
PRIVATE ral_sampler                 _sampler                                          = nullptr;

/* Combination program */
PRIVATE ral_program              _dof_scheuermann_combination_po                         = nullptr;
PRIVATE ral_program_block_buffer _dof_scheuermann_combination_po_ub                      = nullptr;
PRIVATE uint32_t                 _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset = -1;


static const char* _dof_scheuermann_combination_fragment_shader_preambule =
    "#version 430 core\n"
    "\n";
static const char* _dof_scheuermann_combination_fragment_shader_declarations = 
    "layout(binding = 0) uniform sampler2D data_high;\n"
    "layout(binding = 1) uniform sampler2D data_low;\n"
    "layout(binding = 2) uniform sampler2D bg;\n"
    "layout(binding = 3) uniform sampler2D depth_high;\n"
    "\n"
    "layout(binding = 4) uniform data\n"
    "{\n"
    "    float max_coc_px;\n"
    "};\n"
    "\n"
    "layout(location = 0) out vec4 result;\n"
    "layout(location = 0) in  vec2 uv;\n"
    "\n";
static const char* _dof_scheuermann_combination_fragment_shader_main = 
    "void main()\n"
    "{\n"
    "    const float radius_scale = 0.4;\n"
    "\n"
    "    float center_depth     = textureLod(data_high, uv, 0.0).a;\n"
    "    vec2  delta_high       = vec2(1.0f) / textureSize(data_high, 0);\n"
    "    vec2  delta_low        = vec2(1.0f) / textureSize(data_low,  0);\n"
    "    float disc_radius_high = abs(center_depth * (max_coc_px * 2) - max_coc_px);\n"
    "    float disc_radius_low  = disc_radius_high * radius_scale;\n"
    "    int   n_active_taps    = 0;\n"
    "\n"
    "    result = vec4(0.0);\n"
    "\n"
    "    for (int n = 0; n < taps.length(); n += 2)\n"
    "    {\n"
    "        vec2  tap_low_uv  = uv + (delta_low  * vec2(taps[n], taps[n+1]) * disc_radius_low);\n"
    "        vec2  tap_high_uv = uv + (delta_high * vec2(taps[n], taps[n+1]) * disc_radius_high);\n"
    "        vec4  tap_low     = textureLod(data_low,  tap_low_uv,  0.0);\n"
    "        vec4  tap_high    = textureLod(data_high, tap_high_uv, 0.0);\n"
    "        float tap_blur    = abs(tap_high.a * 2 - 1);\n"
    "        vec4  tap         = mix(tap_high, tap_low, tap_blur);\n"
    "\n"
    "        tap.a = (tap.a >= center_depth) ? 1.0 : abs(tap.a * 2 - 1);\n"
    "\n"
    "        result.rgb += tap.rgb * tap.a;\n"
    "        result.a   += tap.a;\n"
    "    }\n"
    "\n"
    "    result.rgb /= result.a;\n"
    "\n"
    "    result += textureLod(bg, uv, 0.0) * ((textureLod(depth_high, uv, 0.0).x < 1) ? 0.0 : 1.0);\n"
    "}\n";


/** TODO */
PRIVATE void _stage_step_dof_scheuermann_update_ub_data(void* unused)
{
    const float blur_strength = main_get_blur_radius();
    const float max_coc_px    = main_get_max_coc_px();

    ral_program_block_buffer_set_nonarrayed_variable_value(_dof_scheuermann_combination_po_ub,
                                                           _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset,
                                                          &max_coc_px,
                                                           sizeof(float) );
    ral_program_block_buffer_sync_immediately             (_dof_scheuermann_combination_po_ub);

    postprocessing_blur_poisson_set_property(_dof_scheuermann_blur_poisson,
                                             POSTPROCESSING_BLUR_POISSON_PROPERTY_BLUR_STRENGTH,
                                            &blur_strength);
}


/* Please see header for specification */
PUBLIC void stage_step_dof_scheuermann_deinit(ral_context context)
{
    ral_texture textures_to_release[] = 
    {
        _dof_scheuermann_combination_texture,
        _dof_scheuermann_downsampled_texture,
        _dof_scheuermann_downsampled_blurred_texture
    };
    const uint32_t n_textures_to_release = sizeof(textures_to_release) / sizeof(textures_to_release[0]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_dof_scheuermann_gfx_state) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_dof_scheuermann_combination_po) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&_sampler) );
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               reinterpret_cast<void* const*>(textures_to_release) );

    ral_program_block_buffer_release   (_dof_scheuermann_combination_po_ub);
    postprocessing_blur_poisson_release(_dof_scheuermann_blur_poisson);
}

/* Please see header for specification */
PUBLIC ral_texture_view stage_step_dof_get_blurred_texture_view()
{
    return _dof_scheuermann_downsampled_blurred_texture_view;
}

/** TODO */
PUBLIC ral_present_task stage_step_dof_scheuermann_get_blur_present_task()
{
    return postprocessing_blur_poisson_get_present_task(
        _dof_scheuermann_blur_poisson,
        _dof_scheuermann_downsampled_texture_view,
        _dof_scheuermann_downsampled_blurred_texture_view);
}

/** TODO */
PUBLIC ral_present_task stage_step_dof_scheuermann_get_downsample_present_task(ral_context      context,
                                                                               ral_texture_view julia_color_rt_texture_view)
{
    ral_command_buffer               cmd_buffer;
    ral_command_buffer_create_info   cmd_buffer_create_info;
    ral_present_task_gpu_create_info create_info;
    ral_present_task_io              input;
    ral_present_task_io              output;
    ral_present_task                 result       = nullptr;

    const uint32_t output_resolution[2] =
    {
        main_get_window_width(),
        main_get_window_height()
    };

    /* Set up the command buffer */
    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_executable                           = true;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &cmd_buffer);

    ral_command_buffer_start_recording(cmd_buffer);
    {
        ral_command_buffer_copy_texture_to_texture_command_info copy_op;
        uint32_t                                                downsampled_texture_size[2];

        ral_texture_get_mipmap_property(_dof_scheuermann_downsampled_texture,
                                        0, /* n_layer  */
                                        0, /* n_mipmap */
                                        RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        downsampled_texture_size + 0);
        ral_texture_get_mipmap_property(_dof_scheuermann_downsampled_texture,
                                        0, /* n_layer  */
                                        0, /* n_mipmap */
                                        RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        downsampled_texture_size + 1);

        copy_op.aspect           = RAL_TEXTURE_ASPECT_COLOR_BIT;
        copy_op.dst_size     [0] = downsampled_texture_size[0];
        copy_op.dst_size     [1] = downsampled_texture_size[1];
        copy_op.dst_size     [2] = 1;
        copy_op.dst_start_xyz[0] = 0;
        copy_op.dst_start_xyz[1] = 0;
        copy_op.dst_start_xyz[2] = 0;
        copy_op.dst_texture_view = _dof_scheuermann_downsampled_texture_view;
        copy_op.n_dst_layer      = 0;
        copy_op.n_dst_mipmap     = 0;
        copy_op.n_src_layer      = 0;
        copy_op.n_src_mipmap     = 0;
        copy_op.scaling_filter   = RAL_TEXTURE_FILTER_LINEAR;
        copy_op.src_size     [0] = output_resolution[0];
        copy_op.src_size     [1] = output_resolution[1];
        copy_op.src_size     [2] = 1;
        copy_op.src_start_xyz[0] = 0;
        copy_op.src_start_xyz[1] = 0;
        copy_op.src_start_xyz[2] = 0;
        copy_op.src_texture_view = julia_color_rt_texture_view;

        ral_command_buffer_record_copy_texture_to_texture(cmd_buffer,
                                                          1, /* n_copy_ops */
                                                         &copy_op);
    }
    ral_command_buffer_stop_recording(cmd_buffer);

    /* Set up the present task */
    input.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    input.texture_view = julia_color_rt_texture_view;

    output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    output.texture_view = _dof_scheuermann_downsampled_texture_view;

    create_info.command_buffer   = cmd_buffer;
    create_info.n_unique_inputs  = 1;
    create_info.n_unique_outputs = 1;
    create_info.unique_inputs    = &input;
    create_info.unique_outputs   = &output;

    result = ral_present_task_create_gpu(system_hashed_ansi_string_create("DOF: Downsampling"),
                                        &create_info);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&cmd_buffer) );

    return result;
}

/** TODO */
PUBLIC ral_present_task stage_step_dof_scheuermann_get_present_task(ral_context      context,
                                                                    ral_texture_view bg_texture_view,
                                                                    ral_texture_view dof_blurred_texture_view,
                                                                    ral_texture_view julia_color_texture_view,
                                                                    ral_texture_view julia_depth_texture_view)
{
    const int* output_resolution = main_get_output_resolution();

    ral_present_task                   cpu_update_task             = nullptr;
    ral_present_task_cpu_create_info   cpu_update_task_create_info;
    ral_present_task_io                cpu_update_task_output;
    ral_buffer                         po_ub_buffer                = nullptr;
    ral_present_task                   render_task                 = nullptr;
    ral_command_buffer                 render_task_cmd_buffer      = nullptr;
    ral_command_buffer_create_info     render_task_cmd_buffer_create_info;
    ral_present_task_gpu_create_info   render_task_create_info;
    ral_present_task_io                render_task_unique_inputs[5];
    ral_present_task_io                render_task_unique_output;
    ral_present_task                   result_task                 = nullptr;
    ral_present_task_group_create_info result_task_create_info;

    ral_program_block_buffer_get_property(_dof_scheuermann_combination_po_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &po_ub_buffer);

    /* Bake the cpu update task first */
    cpu_update_task_output.buffer       = po_ub_buffer;
    cpu_update_task_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_update_task_create_info.cpu_task_callback_user_arg = nullptr;
    cpu_update_task_create_info.n_unique_inputs            = 0;
    cpu_update_task_create_info.n_unique_outputs           = 1;
    cpu_update_task_create_info.pfn_cpu_task_callback_proc = _stage_step_dof_scheuermann_update_ub_data;
    cpu_update_task_create_info.unique_inputs              = nullptr;
    cpu_update_task_create_info.unique_outputs             = &cpu_update_task_output;

    cpu_update_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("DOF: cpu update"),
                                                 &cpu_update_task_create_info);

    /* Move on to the render task */
    render_task_cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    render_task_cmd_buffer_create_info.is_executable                           = true;
    render_task_cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    render_task_cmd_buffer_create_info.is_resettable                           = false;
    render_task_cmd_buffer_create_info.is_transient                            = false;

    ral_context_create_command_buffers(context,
                                       1, /* n_command_buffers */
                                      &render_task_cmd_buffer_create_info,
                                      &render_task_cmd_buffer);

    /* TODO: This cmd buffer could be cached */
    ral_command_buffer_start_recording(render_task_cmd_buffer);
    {
        ral_command_buffer_set_binding_command_info            bindings[5];
        ral_command_buffer_draw_call_regular_command_info      draw_call;
        ral_command_buffer_set_color_rendertarget_command_info rt          = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();

        bindings[0].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        bindings[0].name                               = system_hashed_ansi_string_create("data_high");
        bindings[0].sampled_image_binding.sampler      = _sampler;
        bindings[0].sampled_image_binding.texture_view = julia_color_texture_view;

        bindings[1].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        bindings[1].name                               = system_hashed_ansi_string_create("data_low");
        bindings[1].sampled_image_binding.sampler      = _sampler;
        bindings[1].sampled_image_binding.texture_view = dof_blurred_texture_view;

        bindings[2].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        bindings[2].name                               = system_hashed_ansi_string_create("bg");
        bindings[2].sampled_image_binding.sampler      = _sampler;
        bindings[2].sampled_image_binding.texture_view = bg_texture_view;

        bindings[3].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        bindings[3].name                               = system_hashed_ansi_string_create("depth_high");
        bindings[3].sampled_image_binding.sampler      = _sampler;
        bindings[3].sampled_image_binding.texture_view = julia_depth_texture_view;

        bindings[4].binding_type                       = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        bindings[4].name                               = system_hashed_ansi_string_create("data");
        bindings[4].uniform_buffer_binding.buffer      = po_ub_buffer;
        bindings[4].uniform_buffer_binding.offset      = 0;
        bindings[4].uniform_buffer_binding.size        = 0;

        draw_call.base_instance = 0;
        draw_call.base_vertex   = 0;
        draw_call.n_instances   = 1;
        draw_call.n_vertices    = 4;

        rt.rendertarget_index = 0;
        rt.texture_view       = _dof_scheuermann_combination_texture_view;

        ral_command_buffer_record_set_program(render_task_cmd_buffer,
                                              _dof_scheuermann_combination_po);

        ral_command_buffer_record_set_bindings           (render_task_cmd_buffer,
                                                          sizeof(bindings) / sizeof(bindings[0]),
                                                          bindings);
        ral_command_buffer_record_set_color_rendertargets(render_task_cmd_buffer,
                                                          1, /* n_rendertargets */
                                                         &rt);
        ral_command_buffer_record_set_gfx_state          (render_task_cmd_buffer,
                                                          _dof_scheuermann_gfx_state);

        ral_command_buffer_record_draw_call_regular(render_task_cmd_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call);
    }
    ral_command_buffer_stop_recording(render_task_cmd_buffer);

    render_task_unique_inputs[0]              = cpu_update_task_output;
    render_task_unique_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    render_task_unique_inputs[1].texture_view = julia_color_texture_view;
    render_task_unique_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    render_task_unique_inputs[2].texture_view = dof_blurred_texture_view;
    render_task_unique_inputs[3].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    render_task_unique_inputs[3].texture_view = bg_texture_view;
    render_task_unique_inputs[4].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    render_task_unique_inputs[4].texture_view = julia_depth_texture_view;

    render_task_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    render_task_unique_output.texture_view = _dof_scheuermann_combination_texture_view;

    render_task_create_info.command_buffer   = render_task_cmd_buffer;
    render_task_create_info.n_unique_inputs  = sizeof(render_task_unique_inputs) / sizeof(render_task_unique_inputs[0]);
    render_task_create_info.n_unique_outputs = 1;
    render_task_create_info.unique_inputs    = render_task_unique_inputs;
    render_task_create_info.unique_outputs   = &render_task_unique_output;

    render_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("DOF: rendering"),
                                             &render_task_create_info);

    /* Form the result present task */
    ral_present_task_ingroup_connection result_task_ingroup_connection;
    ral_present_task_group_mapping      result_task_input_mappings[4];
    ral_present_task_group_mapping      result_task_output_mapping;
    ral_present_task                    result_task_present_tasks[2] =
    {
        cpu_update_task,
        render_task
    };

    result_task_ingroup_connection.input_present_task_index     = 1;
    result_task_ingroup_connection.input_present_task_io_index  = 0;
    result_task_ingroup_connection.output_present_task_index    = 0;
    result_task_ingroup_connection.output_present_task_io_index = 0;

    result_task_input_mappings[0].group_task_io_index   = 0;
    result_task_input_mappings[0].n_present_task        = 1;
    result_task_input_mappings[0].present_task_io_index = 1; /* julia_color_texture_view */
    result_task_input_mappings[1].group_task_io_index   = 1;
    result_task_input_mappings[1].n_present_task        = 1;
    result_task_input_mappings[1].present_task_io_index = 2; /* dof_blurred_texture_view */
    result_task_input_mappings[2].group_task_io_index   = 2;
    result_task_input_mappings[2].n_present_task        = 1;
    result_task_input_mappings[2].present_task_io_index = 3; /* bg_texture_view */
    result_task_input_mappings[3].group_task_io_index   = 3;
    result_task_input_mappings[3].n_present_task        = 1;
    result_task_input_mappings[3].present_task_io_index = 4; /* julia_depth_texture_view */

    result_task_output_mapping.group_task_io_index   = 0; /* result */
    result_task_output_mapping.n_present_task        = 1;
    result_task_output_mapping.present_task_io_index = 0;

    result_task_create_info.ingroup_connections                      = &result_task_ingroup_connection;
    result_task_create_info.n_ingroup_connections                    = 1;
    result_task_create_info.n_present_tasks                          = sizeof(result_task_present_tasks)  / sizeof(result_task_present_tasks[0]);
    result_task_create_info.n_total_unique_inputs                    = sizeof(result_task_input_mappings) / sizeof(result_task_input_mappings[0]);
    result_task_create_info.n_total_unique_outputs                   = 1;
    result_task_create_info.n_unique_input_to_ingroup_task_mappings  = sizeof(result_task_input_mappings) / sizeof(result_task_input_mappings[0]);
    result_task_create_info.n_unique_output_to_ingroup_task_mappings = 1;
    result_task_create_info.present_tasks                            = result_task_present_tasks;
    result_task_create_info.unique_input_to_ingroup_task_mapping     = result_task_input_mappings;
    result_task_create_info.unique_output_to_ingroup_task_mapping    = &result_task_output_mapping;

    result_task = ral_present_task_create_group(system_hashed_ansi_string_create("DOF: Group task"),
                                               &result_task_create_info);

    /* Clean up */
    ral_present_task_release(cpu_update_task);
    ral_present_task_release(render_task);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&render_task_cmd_buffer) );

    /* All done */
    return result_task;
}

/* Please see header for specification */
PUBLIC void stage_step_dof_scheuermann_init(ral_context context)
{
    const int* output_resolution               = main_get_output_resolution();
    const int  downsampled_output_resolution[] =
    {
        output_resolution[0] / DOWNSAMPLE_FACTOR,
        output_resolution[1] / DOWNSAMPLE_FACTOR
    };

    /* Set up TOs */
    ral_texture_create_info combination_to_create_info;
    ral_texture_create_info downsampled_blurred_to_create_info;
    ral_texture_create_info downsampled_to_create_info;

    combination_to_create_info.base_mipmap_depth      = 1;
    combination_to_create_info.base_mipmap_height     = output_resolution[1];
    combination_to_create_info.base_mipmap_width      = output_resolution[0];
    combination_to_create_info.description            = nullptr;
    combination_to_create_info.fixed_sample_locations = true;
    combination_to_create_info.format                 = RAL_FORMAT_RGBA8_UNORM;
    combination_to_create_info.n_layers               = 1;
    combination_to_create_info.n_samples              = 1;
    combination_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    combination_to_create_info.unique_name            = system_hashed_ansi_string_create("DOF Scheuermann combination TO");
    combination_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    combination_to_create_info.use_full_mipmap_chain  = false;

    downsampled_blurred_to_create_info.base_mipmap_depth      = 1;
    downsampled_blurred_to_create_info.base_mipmap_height     = downsampled_output_resolution[1];
    downsampled_blurred_to_create_info.base_mipmap_width      = downsampled_output_resolution[0];
    downsampled_blurred_to_create_info.description            = nullptr;
    downsampled_blurred_to_create_info.fixed_sample_locations = true;
    downsampled_blurred_to_create_info.format                 = RAL_FORMAT_RGBA16_FLOAT;
    downsampled_blurred_to_create_info.n_layers               = 1;
    downsampled_blurred_to_create_info.n_samples              = 1;
    downsampled_blurred_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    downsampled_blurred_to_create_info.unique_name            = system_hashed_ansi_string_create("DOF Scheuermann downsampled TO");
    downsampled_blurred_to_create_info.usage                  = RAL_TEXTURE_USAGE_SAMPLED_BIT;
    downsampled_blurred_to_create_info.use_full_mipmap_chain  = false;

    downsampled_to_create_info.base_mipmap_depth      = 1;
    downsampled_to_create_info.base_mipmap_height     = downsampled_output_resolution[1];
    downsampled_to_create_info.base_mipmap_width      = downsampled_output_resolution[0];
    downsampled_to_create_info.description            = nullptr;
    downsampled_to_create_info.fixed_sample_locations = true;
    downsampled_to_create_info.format                 = RAL_FORMAT_RGBA16_FLOAT;
    downsampled_to_create_info.n_layers               = 1;
    downsampled_to_create_info.n_samples              = 1;
    downsampled_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    downsampled_to_create_info.unique_name            = system_hashed_ansi_string_create("DOF Scheuermann downsampled blurred TO");
    downsampled_to_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_DST_BIT |
                                                        RAL_TEXTURE_USAGE_SAMPLED_BIT;
    downsampled_to_create_info.use_full_mipmap_chain  = false;


    ral_texture_create_info to_create_info_items[] =
    {
        combination_to_create_info,
        downsampled_blurred_to_create_info,
        downsampled_to_create_info
    };
    const uint32_t n_to_create_info_items = sizeof(to_create_info_items) / sizeof(to_create_info_items[0]);
    ral_texture    result_tos[n_to_create_info_items];

    ral_context_create_textures(context,
                                n_to_create_info_items,
                                to_create_info_items,
                                result_tos);

    _dof_scheuermann_combination_texture         = result_tos[0];
    _dof_scheuermann_downsampled_blurred_texture = result_tos[1];
    _dof_scheuermann_downsampled_texture         = result_tos[2];

    /* Set up texture views */
    ral_texture_view_create_info texture_view_create_info_items[] =
    {
        ral_texture_view_create_info(_dof_scheuermann_combination_texture),
        ral_texture_view_create_info(_dof_scheuermann_downsampled_texture),
        ral_texture_view_create_info(_dof_scheuermann_downsampled_blurred_texture)
    };
    const uint32_t n_texture_view_create_info_items = sizeof(texture_view_create_info_items) / sizeof(texture_view_create_info_items[0]);

    _dof_scheuermann_combination_texture_view         = ral_texture_get_view(texture_view_create_info_items + 0);
    _dof_scheuermann_downsampled_texture_view         = ral_texture_get_view(texture_view_create_info_items + 1);
    _dof_scheuermann_downsampled_blurred_texture_view = ral_texture_get_view(texture_view_create_info_items + 2);

    /* Bake a sampler */
    ral_sampler_create_info sampler_create_info; 

    sampler_create_info.min_filter = RAL_TEXTURE_FILTER_LINEAR;
    sampler_create_info.wrap_s     = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
    sampler_create_info.wrap_t     = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;

    ral_context_create_samplers(context,
                                1, /* n_create_info_items */
                               &sampler_create_info,
                               &_sampler);

    /* Set up gfx state */
    ral_gfx_state_create_info                       gfx_state_create_info;
    ral_command_buffer_set_scissor_box_command_info scissor_box;
    ral_command_buffer_set_viewport_command_info    viewport;

    scissor_box.index   = 0;
    scissor_box.size[0] = output_resolution[0];
    scissor_box.size[1] = output_resolution[1];
    scissor_box.xy  [0] = 0;
    scissor_box.xy  [1] = 0;

    viewport.depth_range[0] = 0.0f;
    viewport.depth_range[1] = 1.0f;
    viewport.index          = 0;
    viewport.size[0]        = static_cast<float>(output_resolution[0]);
    viewport.size[1]        = static_cast<float>(output_resolution[1]);
    viewport.xy  [0]        = 0;
    viewport.xy  [1]        = 0;

    gfx_state_create_info.depth_test                           = false;
    gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
    gfx_state_create_info.scissor_test                         = true;
    gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
    gfx_state_create_info.static_scissor_boxes                 = &scissor_box;
    gfx_state_create_info.static_scissor_boxes_enabled         = true;
    gfx_state_create_info.static_viewports                     = &viewport;
    gfx_state_create_info.static_viewports_enabled             = true;

    ral_context_create_gfx_states(context,
                                  1, /* n_create_info_items */
                                 &gfx_state_create_info,
                                 &_dof_scheuermann_gfx_state);

    /* Set up postprocessor */
    _dof_scheuermann_blur_poisson = postprocessing_blur_poisson_create(context,
                                                                       system_hashed_ansi_string_create("Poison blurrer"),
                                                                       POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM);

    /* Set up combination program */
    ral_shader  combination_fs              = NULL;
    const char* combination_fs_body_parts[] =
    {
        _dof_scheuermann_combination_fragment_shader_preambule,
        _dof_scheuermann_combination_fragment_shader_declarations,
        postprocessing_blur_poisson_get_tap_data_shader_code(),
        _dof_scheuermann_combination_fragment_shader_main
    };
    const uint32_t                  combination_fs_n_body_parts = sizeof(combination_fs_body_parts) /
                                                                  sizeof(combination_fs_body_parts[0]);
    const system_hashed_ansi_string combination_fs_body_has     = system_hashed_ansi_string_create_by_merging_strings(combination_fs_n_body_parts,
                                                                                                                      combination_fs_body_parts);
    shaders_vertex_fullscreen       combination_vs              = shaders_vertex_fullscreen_create(context,
                                                                                                   true, /* export_uv */
                                                                                                   system_hashed_ansi_string_create("DOF Scheuermann combination VS") );

    const ral_shader_create_info combination_fs_create_info =
    {
        system_hashed_ansi_string_create("DOF Scheuermann combination FS"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_program_create_info combination_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("DOF Scheuermann combination PO")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &combination_po_create_info,
                               &_dof_scheuermann_combination_po);
    ral_context_create_shaders (context,
                                1, /* n_create_info_items */
                               &combination_fs_create_info,
                               &combination_fs);

    ral_shader_set_property(combination_fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            &combination_fs_body_has);

    ral_program_attach_shader(_dof_scheuermann_combination_po,
                              combination_fs,
                              true /* async */);
    ral_program_attach_shader(_dof_scheuermann_combination_po,
                              shaders_vertex_fullscreen_get_shader(combination_vs),
                              true /* async */);

    /* Create a combination program uniform block buffer */
    _dof_scheuermann_combination_po_ub = ral_program_block_buffer_create(context,
                                                                         _dof_scheuermann_combination_po,
                                                                         system_hashed_ansi_string_create("data"));

    /* Retrieve combination program uniform locations */
    const ral_program_variable* max_coc_px_uniform_ral_ptr = nullptr;

    ral_program_get_block_variable_by_name(_dof_scheuermann_combination_po,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("max_coc_px"),
                                          &max_coc_px_uniform_ral_ptr);

    _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset = (max_coc_px_uniform_ral_ptr != nullptr) ? max_coc_px_uniform_ral_ptr->block_offset : -1;

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&combination_fs) );

    shaders_vertex_fullscreen_release(combination_vs);
}
