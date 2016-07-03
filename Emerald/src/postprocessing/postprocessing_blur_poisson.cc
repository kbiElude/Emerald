/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "postprocessing/postprocessing_blur_poisson.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"

/** Internal type definition */
typedef struct
{
    postprocessing_blur_poisson_blur_bluriness_source bluriness_source;
    ral_context                                       context;
    const char*                                       custom_shader_code;

    float            cached_blur_strength;
    ral_texture_view cached_input_texture_view;
    ral_present_task cached_present_task;
    ral_texture_view cached_result_texture_view;

    ral_gfx_state             gfx_state;
    system_hashed_ansi_string name;
    ral_program               program;
    ral_program_block_buffer  program_ub;
    ral_sampler               sampler;

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_blur_poisson;


/** Internal variables */ 
static const char* postprocessing_blur_poisson_uniform_blur_strength_body =
    "vec2 get_blur_strength()\n"
    "{\n"
    "    return vec2(blur_strength);\n"
    "}\n";

static const char* postprocessing_blur_poisson_source_input_alpha_blur_strength_body =
    "vec2 get_blur_strength()\n"
    "{\n"
    "    return textureLod(data, uv, 0.0).aa;\n"
    "}\n";

static const char* postprocessing_blur_poisson_fragment_shader_body_preambule =
    "#version 430 core\n"
    "\n";
static const char* postprocessing_blur_poisson_tap_data_body =
    "const float taps[] = float[](-0.37468f,     0.8566398f,\n"
    "                              0.2497663f,   0.5130413f,\n"
    "                             -0.3814645f,   0.4049693f,\n"
    "                             -0.7627723f,   0.06273784f,\n"
    "                             -0.1496337f,   0.007745424f,\n"
    "                              0.07355442f,  0.984551f,\n"
    "                              0.5004861f,  -0.2253174f,\n"
    "                              0.7175385f,   0.4638414f,\n"
    "                              0.9855714f,   0.01321317f,\n"
    "                             -0.3985322f,  -0.5223456f,\n"
    "                              0.2407384f,  -0.888767f,\n"
    "                              0.07159682f, -0.4352953f,\n"
    "                             -0.2808287f,  -0.9595322f,\n"
    "                             -0.8836895f,  -0.4189175f,\n"
    "                             -0.7953489f,   0.5906183f,\n"
    "                              0.8182191f,  -0.5711964f);\n";

static const char* postprocessing_blur_poisson_fragment_shader_body_declarations =
    "uniform dataFS\n"
    "{\n"
    "    float blur_strength;\n"
    "};\n"
    "\n"
    "uniform sampler2D data;\n"
    "out     vec4      result;\n"
    "in      vec2      uv;\n"
    "\n"
    "vec2 get_blur_strength();\n"
    "\n"
    "\n";
static const char* postprocessing_blur_poisson_fragment_shader_body_main =
    "void main()\n"
    "{\n"
    "\n"
    "   vec4 temp_result = vec4(0.0);\n"
    "\n"
    "   for (int i = 0; i < taps.length(); i += 2)\n"
    "   {\n"
    "      temp_result += textureLod(data, uv + vec2(taps[i], taps[i+1]) / textureSize(data, 0) * get_blur_strength(), 0.0);\n"
    "   }\n"
    "\n"
    "   result = temp_result / taps.length() * 2;\n"
    "}\n";

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_blur_poisson,
                               postprocessing_blur_poisson,
                              _postprocessing_blur_poisson);



/** TODO */
PRIVATE void _postprocessing_blur_poisson_release(void* ptr)
{
    _postprocessing_blur_poisson* data_ptr = (_postprocessing_blur_poisson*) ptr;

    ral_context_delete_objects(data_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &data_ptr->program);

    if (data_ptr->gfx_state != nullptr)
    {
        ral_gfx_state_release(data_ptr->gfx_state);

        data_ptr->gfx_state = nullptr;
    }

    if (data_ptr->program_ub != nullptr)
    {
        ral_program_block_buffer_release(data_ptr->program_ub);

        data_ptr->program_ub = nullptr;
    }

    if (data_ptr->sampler != nullptr)
    {
        ral_context_delete_objects(data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                   1, /* n_objects */
                                   (const void**) &data_ptr->sampler);
    }
}

/** TODO */
void _postprocessing_blur_poisson_update_ub_cpu_task_callback(void* poisson_raw_ptr)
{
    _postprocessing_blur_poisson* poisson_ptr = reinterpret_cast<_postprocessing_blur_poisson*>(poisson_raw_ptr);

    ral_program_block_buffer_set_nonarrayed_variable_value(poisson_ptr->program_ub,
                                                           0, /* block_variable_offset */
                                                          &poisson_ptr->cached_blur_strength,
                                                           sizeof(float) );
    ral_program_block_buffer_sync                         (poisson_ptr->program_ub);
}

/** Please see header for specification */
PUBLIC EMERALD_API postprocessing_blur_poisson postprocessing_blur_poisson_create(ral_context                                       context,
                                                                                  system_hashed_ansi_string                         name,
                                                                                  postprocessing_blur_poisson_blur_bluriness_source bluriness_source,
                                                                                  const char*                                       custom_shader_code)
{
    ral_shader                    fragment_shader = nullptr;
    _postprocessing_blur_poisson* result_ptr      = nullptr;
    shaders_vertex_fullscreen     vertex_shader   = nullptr;

    const char* fragment_shader_body_parts[] =
    {
        postprocessing_blur_poisson_fragment_shader_body_preambule,
        postprocessing_blur_poisson_tap_data_body,
        postprocessing_blur_poisson_fragment_shader_body_declarations,
        postprocessing_blur_poisson_fragment_shader_body_main,
        (bluriness_source == POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM)     ? postprocessing_blur_poisson_uniform_blur_strength_body            :
        (bluriness_source == POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_INPUT_ALPHA) ? postprocessing_blur_poisson_source_input_alpha_blur_strength_body :
                                                                                               "?!"
    };
    const uint32_t                  fragment_shader_n_body_parts = sizeof(fragment_shader_body_parts) / sizeof(fragment_shader_body_parts[0]);
    const system_hashed_ansi_string fragment_shader_body         = system_hashed_ansi_string_create_by_merging_strings(fragment_shader_n_body_parts,
                                                                                                                       fragment_shader_body_parts);

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Postprocessing blur poisson fragment shader ", 
                                                                system_hashed_ansi_string_get_buffer(name) ),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_program_create_info po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create_by_merging_two_strings("Postprocessing blur poisson program ",
                                                                system_hashed_ansi_string_get_buffer(name))
    };

    /* Instantiate the object */
    result_ptr = new (std::nothrow) _postprocessing_blur_poisson;

    ASSERT_DEBUG_SYNC(result_ptr != nullptr,
                      "Out of memory");

    if (result_ptr == nullptr)
    {
        LOG_ERROR("Out of memory");

        goto end;
    }

    memset(result_ptr,
           0,
           sizeof(_postprocessing_blur_poisson) );

    result_ptr->bluriness_source   = bluriness_source;
    result_ptr->context            = context;
    result_ptr->custom_shader_code = custom_shader_code;
    result_ptr->name               = name;

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &po_create_info,
                               &result_ptr->program);
    ral_context_create_shaders(context,
                               1, /* n_create_info_items */
                              &fs_create_info,
                              &fragment_shader);

    vertex_shader = shaders_vertex_fullscreen_create(context,
                                                     true, /* export_uv */
                                                     name);


    ral_shader_set_property(fragment_shader,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fragment_shader_body);

    ral_program_attach_shader(result_ptr->program,
                              fragment_shader,
                              true /* async */);
    ral_program_attach_shader(result_ptr->program,
                              shaders_vertex_fullscreen_get_shader(vertex_shader),
                              true /* async */);

    /* Release shaders */
    ral_context_delete_objects(result_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               (const void**) &fragment_shader);

    shaders_vertex_fullscreen_release(vertex_shader);

    /* Retrieve UB info */
    result_ptr->program_ub = ral_program_block_buffer_create(context,
                                                             result_ptr->program,
                                                             system_hashed_ansi_string_create("dataFS") );

    /* All done */
    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                   _postprocessing_blur_poisson_release,
                                                   OBJECT_TYPE_POSTPROCESSING_BLUR_POISSON,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Post-processing Blur Poisson\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (postprocessing_blur_poisson) result_ptr;

end:
    if (result_ptr != nullptr)
    {
        _postprocessing_blur_poisson_release(result_ptr);

        delete result_ptr; 
    }

    return nullptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API ral_present_task postprocessing_blur_poisson_get_present_task(postprocessing_blur_poisson blur_poisson,
                                                                                 ral_texture_view            input_texture_view,
                                                                                 float                       blur_strength,
                                                                                 ral_texture_view            result_texture_view)
{
    ral_command_buffer             cmd_buffer             = nullptr;
    ral_command_buffer_create_info cmd_buffer_create_info;
    _postprocessing_blur_poisson*  poisson_ptr            = reinterpret_cast<_postprocessing_blur_poisson*>(blur_poisson);
    ral_present_task               result                 = nullptr;
    unsigned int                   texture_height         = 0;
    unsigned int                   texture_width          = 0;
    system_window                  window                 = nullptr;
    int                            window_size[2]         = {0};

    ral_context_get_property(poisson_ptr->context,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                            &window);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    if (poisson_ptr->cached_present_task        != nullptr             &&
        poisson_ptr->cached_input_texture_view  == input_texture_view  &&
        // ignore blur_strength here - the setting is adjusted at run-time
        poisson_ptr->cached_result_texture_view == result_texture_view)
    {
        result = poisson_ptr->cached_present_task;

        goto end;
    }

    if (poisson_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(poisson_ptr->cached_present_task);

        poisson_ptr->cached_present_task = nullptr;
    }

    /* Cache input texture view's properties */
    ral_texture_view_get_mipmap_property(result_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &texture_height);
    ral_texture_view_get_mipmap_property(result_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &texture_width);

    /* Instantiate a new gfx state */
    ral_gfx_state_create_info                       gfx_state_create_info;
    ral_command_buffer_set_scissor_box_command_info gfx_state_scissor_box;
    ral_command_buffer_set_viewport_command_info    gfx_state_viewport;

    gfx_state_scissor_box.index   = 0;
    gfx_state_scissor_box.size[0] = texture_width;
    gfx_state_scissor_box.size[1] = texture_height;
    gfx_state_scissor_box.xy  [0] = 0;
    gfx_state_scissor_box.xy  [1] = 0;

    gfx_state_viewport.depth_range[0] = 0.0f;
    gfx_state_viewport.depth_range[1] = 1.0f;
    gfx_state_viewport.index          = 0;
    gfx_state_viewport.size[0]        = texture_width;
    gfx_state_viewport.size[1]        = texture_height;
    gfx_state_viewport.xy  [0]        = 0;
    gfx_state_viewport.xy  [1]        = 0;

    if (poisson_ptr->gfx_state != nullptr)
    {
        ral_gfx_state_release(poisson_ptr->gfx_state);

        poisson_ptr->gfx_state = nullptr;
    }

    gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
    gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
    gfx_state_create_info.static_scissor_boxes                 = &gfx_state_scissor_box;
    gfx_state_create_info.static_scissor_boxes_and_viewports   = true;
    gfx_state_create_info.static_viewports                     = &gfx_state_viewport;

    poisson_ptr->gfx_state = ral_gfx_state_create(poisson_ptr->context,
                                                 &gfx_state_create_info);

    /* Cache UB properties */
    ral_buffer program_ub_bo      = nullptr;
    uint32_t   program_ub_bo_size = 0;

    ral_program_block_buffer_get_property(poisson_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_bo);

    ral_buffer_get_property(program_ub_bo,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &program_ub_bo_size);

    /* Create & record the command buffer */
    ral_command_buffer_set_binding_command_info set_ub_binding_command_info;

    set_ub_binding_command_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
    set_ub_binding_command_info.name                          = system_hashed_ansi_string_create("dataFS");
    set_ub_binding_command_info.uniform_buffer_binding.buffer = program_ub_bo;
    set_ub_binding_command_info.uniform_buffer_binding.offset = 0;
    set_ub_binding_command_info.uniform_buffer_binding.size   = program_ub_bo_size;

    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = false;

    cmd_buffer = ral_command_buffer_create(poisson_ptr->context,
                                          &cmd_buffer_create_info);

    ASSERT_DEBUG_SYNC(cmd_buffer != nullptr,
                      "Could not create a command buffer");

    ral_command_buffer_start_recording(cmd_buffer);
    {
        ral_command_buffer_draw_call_regular_command_info      draw_call_command_info;
        ral_command_buffer_set_binding_command_info            set_input_texture_binding_command_info;
        ral_command_buffer_set_color_rendertarget_command_info set_color_rt_binding_command_info = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        ral_command_buffer_set_viewport_command_info           set_viewport_command_info;

        draw_call_command_info.base_instance = 0;
        draw_call_command_info.base_vertex   = 0;
        draw_call_command_info.n_instances   = 1;
        draw_call_command_info.n_vertices    = 4;

        set_color_rt_binding_command_info.rendertarget_index = 0;
        set_color_rt_binding_command_info.texture_view       = poisson_ptr->cached_result_texture_view;

        set_input_texture_binding_command_info.binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
        set_input_texture_binding_command_info.name                               = system_hashed_ansi_string_create("data");
        set_input_texture_binding_command_info.sampled_image_binding.sampler      = poisson_ptr->sampler;
        set_input_texture_binding_command_info.sampled_image_binding.texture_view = input_texture_view;

        set_viewport_command_info.depth_range[0] = 0.0f;
        set_viewport_command_info.depth_range[1] = 1.0f;
        set_viewport_command_info.index          = 0;
        set_viewport_command_info.size[0]        = static_cast<float>(texture_width);
        set_viewport_command_info.size[1]        = static_cast<float>(texture_height);
        set_viewport_command_info.xy  [0]        = 0;
        set_viewport_command_info.xy  [1]        = 0;

        ral_command_buffer_record_set_program            (cmd_buffer,
                                                          poisson_ptr->program);
        ral_command_buffer_record_set_bindings           (cmd_buffer,
                                                          1, /* n_bindings */
                                                         &set_input_texture_binding_command_info);
        ral_command_buffer_record_set_color_rendertargets(cmd_buffer,
                                                          1, /* n_rendertargets */
                                                         &set_color_rt_binding_command_info);
        ral_command_buffer_record_set_gfx_state          (cmd_buffer,
                                                          poisson_ptr->gfx_state);
        ral_command_buffer_record_set_bindings           (cmd_buffer,
                                                          1, /* n_bindings */
                                                         &set_ub_binding_command_info);
        ral_command_buffer_record_set_viewports          (cmd_buffer,
                                                          1, /* n_viewports */
                                                         &set_viewport_command_info);

        ral_command_buffer_record_draw_call_regular(cmd_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call_command_info);
    }
    ral_command_buffer_stop_recording(cmd_buffer);

    /* Form the presentation tasks */
    ral_present_task                    group_task = nullptr;
    ral_present_task_group_create_info  group_task_create_info;
    ral_present_task_group_mapping      group_task_input_to_ingroup_in_mapping;
    ral_present_task_group_mapping      group_task_output_to_ingroup_out_mapping;
    ral_present_task_ingroup_connection group_task_ub_update_to_pp_connection;
    ral_present_task                    group_task_present_tasks[2];
    ral_present_task_gpu_create_info    pp_create_info;
    ral_present_task_io                 pp_inputs[2];
    ral_present_task_io                 pp_output;
    ral_present_task                    pp_task        = nullptr;
    ral_present_task                    ub_update_task = nullptr;
    ral_present_task_io                 ub_update_task_output;
    ral_present_task_cpu_create_info    ub_update_task_create_info;

    ub_update_task_output.buffer      = program_ub_bo;
    ub_update_task_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    ub_update_task_create_info.cpu_task_callback_user_arg = poisson_ptr;
    ub_update_task_create_info.n_unique_inputs            = 0;
    ub_update_task_create_info.n_unique_outputs           = 1;
    ub_update_task_create_info.pfn_cpu_task_callback_proc = _postprocessing_blur_poisson_update_ub_cpu_task_callback;
    ub_update_task_create_info.unique_inputs              = nullptr;
    ub_update_task_create_info.unique_outputs             = &ub_update_task_output;

    ub_update_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Poisson blur: UB update task"),
                                                &ub_update_task_create_info);


    pp_inputs[0].buffer      = program_ub_bo;
    pp_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    pp_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    pp_inputs[1].texture_view = nullptr;

    pp_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    pp_output.texture_view = result_texture_view;

    pp_create_info.command_buffer   = cmd_buffer;
    pp_create_info.n_unique_inputs  = sizeof(pp_inputs) / sizeof(pp_inputs[0]);
    pp_create_info.n_unique_outputs = 1;
    pp_create_info.unique_inputs    = pp_inputs;
    pp_create_info.unique_outputs   = &pp_output;

    pp_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Poisson blur: post-processing"),
                                                                          &pp_create_info);


    group_task_present_tasks[0] = ub_update_task;
    group_task_present_tasks[1] = pp_task;

    group_task_input_to_ingroup_in_mapping.io_index       = 1; /* pp_inputs[1] */
    group_task_input_to_ingroup_in_mapping.n_present_task = 1;

    group_task_output_to_ingroup_out_mapping.io_index       = 0;
    group_task_output_to_ingroup_out_mapping.n_present_task = 1; /* pp_output */

    group_task_ub_update_to_pp_connection.input_present_task_index     = 1;
    group_task_ub_update_to_pp_connection.input_present_task_io_index  = 0; /* pp_inputs[0] */
    group_task_ub_update_to_pp_connection.output_present_task_index    = 0;
    group_task_ub_update_to_pp_connection.output_present_task_io_index = 0; /* ub_update_task_output */

    group_task_create_info.ingroup_connections                   = &group_task_ub_update_to_pp_connection;
    group_task_create_info.n_ingroup_connections                 = 1;
    group_task_create_info.n_present_tasks                       = sizeof(group_task_present_tasks) / sizeof(group_task_present_tasks[0]);
    group_task_create_info.n_unique_inputs                       = 1;
    group_task_create_info.n_unique_outputs                      = 1;
    group_task_create_info.present_tasks                         = group_task_present_tasks;
    group_task_create_info.unique_input_to_ingroup_task_mapping  = &group_task_input_to_ingroup_in_mapping;
    group_task_create_info.unique_output_to_ingroup_task_mapping = &group_task_output_to_ingroup_out_mapping;

    group_task = ral_present_task_create_group(&group_task_create_info);

    ral_present_task_release(pp_task);
    pp_task = nullptr;

    ral_present_task_release(ub_update_task);
    ub_update_task = nullptr;


    poisson_ptr->cached_blur_strength       = blur_strength;
    poisson_ptr->cached_input_texture_view  = input_texture_view;
    poisson_ptr->cached_present_task        = group_task;
    poisson_ptr->cached_result_texture_view = result_texture_view;

    ral_present_task_retain(poisson_ptr->cached_present_task);

    result = poisson_ptr->cached_present_task;
end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API const char* postprocessing_blur_poisson_get_tap_data_shader_code()
{
    return postprocessing_blur_poisson_tap_data_body;
}
