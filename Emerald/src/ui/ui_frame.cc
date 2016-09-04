/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_frame.h"
#include "ui/ui_shared.h"

/** Internal types */
typedef struct
{
    ral_context   context;
    ral_program   program;
    bool          visible;
    float         x1y1x2y2[4];

    ral_command_buffer last_cached_command_buffer;
    ral_gfx_state      last_cached_gfx_state;
    ral_present_task   last_cached_present_task;
    ral_texture_view   last_cached_present_task_target_texture_view;

    ral_program_block_buffer program_ub;
    uint32_t                 program_ub_bo_size;
    uint32_t                 program_x1y1x2y2_ub_offset;
} _ui_frame;

/** Internal variables */
static const char* ui_frame_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(0.1, 0.1, 0.1f, 0.7);\n"
    "}\n";

static system_hashed_ansi_string ui_frame_program_name = system_hashed_ansi_string_create("UI Frame");


/** TODO */
PRIVATE void _ui_frame_init_program(ui         ui_instance,
                                    _ui_frame* frame_ptr)
{
    /* Create all objects */
    ral_shader fs = nullptr;
    ral_shader vs = nullptr;

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create("UI frame fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create("UI frame vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI frame program")
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items];

    if (!ral_context_create_shaders(frame_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    if (!ral_context_create_programs(frame_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &frame_ptr->program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    /* Set up shaders */
    const system_hashed_ansi_string fs_body = system_hashed_ansi_string_create(ui_frame_fragment_shader_body);
    const system_hashed_ansi_string vs_body = system_hashed_ansi_string_create(ui_general_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Set up program object */
    if (!ral_program_attach_shader(frame_ptr->program,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(frame_ptr->program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following frame instances will reuse the program */
    ui_register_program(ui_instance,
                        ui_frame_program_name,
                        frame_ptr->program);

    /* Release shaders we will no longer need */
    ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(frame_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               reinterpret_cast<void* const*>(shaders_to_release) );
}

/** TODO */
PRIVATE void _ui_frame_update_ub_props_cpu_task_callback(void* frame_raw_ptr)
{
    _ui_frame* frame_ptr = reinterpret_cast<_ui_frame*>(frame_raw_ptr);

    /* Update uniforms */
    ral_program_block_buffer_set_nonarrayed_variable_value(frame_ptr->program_ub,
                                                           frame_ptr->program_x1y1x2y2_ub_offset,
                                                           frame_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);

    ral_program_block_buffer_sync_immediately(frame_ptr->program_ub);

}


/** Please see header for specification */
PUBLIC void ui_frame_deinit(void* internal_instance)
{
    _ui_frame* ui_frame_ptr = reinterpret_cast<_ui_frame*>(internal_instance);

    ral_context_delete_objects(ui_frame_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&ui_frame_ptr->program) );

    if (ui_frame_ptr->last_cached_command_buffer != nullptr)
    {
        ral_command_buffer_release(ui_frame_ptr->last_cached_command_buffer);

        ui_frame_ptr->last_cached_command_buffer = nullptr;
    }

    if (ui_frame_ptr->last_cached_gfx_state != nullptr)
    {
        ral_gfx_state_release(ui_frame_ptr->last_cached_gfx_state);

        ui_frame_ptr->last_cached_gfx_state = nullptr;
    }

    if (ui_frame_ptr->program_ub != nullptr)
    {
        ral_program_block_buffer_release(ui_frame_ptr->program_ub);

        ui_frame_ptr->program_ub = nullptr;
    }

    delete ui_frame_ptr;
}

/** Please see header for specification */
PUBLIC ral_present_task ui_frame_get_present_task(void*            internal_instance,
                                                  ral_texture_view target_texture_view)
{
    _ui_frame* frame_ptr                  = reinterpret_cast<_ui_frame*>(internal_instance);
    uint32_t   target_texture_view_height = 0;
    uint32_t   target_texture_view_width  = 0;
    ral_buffer ub_bo                      = nullptr;

    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &target_texture_view_width);
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &target_texture_view_height);

    /* Can we re-use last present task? */
    if (frame_ptr->last_cached_present_task                     != nullptr             &&
        frame_ptr->last_cached_present_task_target_texture_view == target_texture_view)
    {
        goto end;
    }

    /* Can we re-use last gfx_state instance? */
    if (frame_ptr->last_cached_gfx_state != nullptr)
    {
        ral_command_buffer_set_viewport_command_info gfx_state_viewport;

        ral_gfx_state_get_property(frame_ptr->last_cached_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &gfx_state_viewport);

        if (fabs(gfx_state_viewport.size[0] - target_texture_view_width) > 1e-5f ||
            fabs(gfx_state_viewport.size[1] - target_texture_view_height) > 1e-5f)
        {
            ral_gfx_state_release(frame_ptr->last_cached_gfx_state);

            frame_ptr->last_cached_gfx_state = nullptr;
        }
    }

    if (frame_ptr->last_cached_gfx_state == nullptr)
    {
        /* Bake a gfx_state instance */
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_scissor_box_command_info scissor;
        ral_command_buffer_set_viewport_command_info    viewport;

        scissor.index   = 0;
        scissor.size[0] = target_texture_view_height;
        scissor.size[1] = target_texture_view_width;
        scissor.xy  [0] = 0;
        scissor.xy  [1] = 0;

        viewport.depth_range[0] = 0.0f;
        viewport.depth_range[1] = 1.0f;
        viewport.index          = 0;
        viewport.size[0]        = static_cast<float>(target_texture_view_width);
        viewport.size[1]        = static_cast<float>(target_texture_view_height);

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &scissor;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &viewport;
        gfx_state_create_info.static_viewports_enabled             = true;

        frame_ptr->last_cached_gfx_state = ral_gfx_state_create(frame_ptr->context,
                                                               &gfx_state_create_info);
    }

    /* Draw */
    ral_program_block_buffer_get_property(frame_ptr->program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_bo);

    if (frame_ptr->last_cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info cmd_buffer_create_info;

        cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
        cmd_buffer_create_info.is_resettable                           = true;
        cmd_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(frame_ptr->context,
                                           1, /* n_command_buffers */
                                          &cmd_buffer_create_info,
                                          &frame_ptr->last_cached_command_buffer);
    }

    ral_command_buffer_start_recording(frame_ptr->last_cached_command_buffer);
    {
        ral_command_buffer_draw_call_regular_command_info      draw_call_info;
        ral_command_buffer_set_color_rendertarget_command_info rt_info          = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
        ral_command_buffer_set_binding_command_info            ub_binding_info;

        draw_call_info.base_instance = 0;
        draw_call_info.base_vertex   = 0;
        draw_call_info.n_instances   = 1;
        draw_call_info.n_vertices    = 4;

        rt_info.blend_enabled          = true;
        rt_info.blend_op_alpha         = RAL_BLEND_OP_ADD;
        rt_info.blend_op_color         = RAL_BLEND_OP_ADD;
        rt_info.dst_alpha_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        rt_info.dst_color_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        rt_info.rendertarget_index     = 0;
        rt_info.src_alpha_blend_factor = RAL_BLEND_FACTOR_SRC_ALPHA;
        rt_info.src_color_blend_factor = RAL_BLEND_FACTOR_SRC_ALPHA;
        rt_info.texture_view           = target_texture_view;

        ub_binding_info.binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_binding_info.name                          = system_hashed_ansi_string_create("dataVS");
        ub_binding_info.uniform_buffer_binding.buffer = ub_bo;
        ub_binding_info.uniform_buffer_binding.offset = 0;
        ub_binding_info.uniform_buffer_binding.size   = frame_ptr->program_ub_bo_size;

        ral_command_buffer_record_set_bindings           (frame_ptr->last_cached_command_buffer,
                                                          1,
                                                         &ub_binding_info);
        ral_command_buffer_record_set_color_rendertargets(frame_ptr->last_cached_command_buffer,
                                                          1, /* n_rendertargets */
                                                         &rt_info);
        ral_command_buffer_record_set_gfx_state          (frame_ptr->last_cached_command_buffer,
                                                          frame_ptr->last_cached_gfx_state);
        ral_command_buffer_record_set_program            (frame_ptr->last_cached_command_buffer,
                                                          frame_ptr->program);

        ral_command_buffer_record_draw_call_regular(frame_ptr->last_cached_command_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call_info);
    }
    ral_command_buffer_stop_recording(frame_ptr->last_cached_command_buffer);

    /* Form the present tasks */
    ral_present_task                    cpu_present_task;
    ral_present_task_cpu_create_info    cpu_present_task_create_info;
    ral_present_task_io                 cpu_present_task_output;
    ral_present_task                    draw_present_task;
    ral_present_task_gpu_create_info    draw_present_task_create_info;
    ral_present_task_io                 draw_present_task_inputs[2];
    ral_present_task_io                 draw_present_task_output;
    ral_present_task                    present_tasks[2];
    ral_present_task                    result_present_task;
    ral_present_task_group_create_info  result_present_task_create_info;
    ral_present_task_ingroup_connection result_present_task_ingroup_connection;
    ral_present_task_group_mapping      result_present_task_input_mapping;
    ral_present_task_group_mapping      result_present_task_output_mapping;

    cpu_present_task_output.buffer      = ub_bo;
    cpu_present_task_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_present_task_create_info.cpu_task_callback_user_arg = frame_ptr;
    cpu_present_task_create_info.n_unique_inputs            = 0;
    cpu_present_task_create_info.n_unique_outputs           = 1;
    cpu_present_task_create_info.pfn_cpu_task_callback_proc = _ui_frame_update_ub_props_cpu_task_callback;
    cpu_present_task_create_info.unique_inputs              = nullptr;
    cpu_present_task_create_info.unique_outputs             = &cpu_present_task_output;

    cpu_present_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("UI frame: UB update"),
                                                  &cpu_present_task_create_info);


    draw_present_task_inputs[0].buffer      = ub_bo;
    draw_present_task_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_inputs[1].texture_view = target_texture_view;

    draw_present_task_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_output.texture_view = target_texture_view;

    draw_present_task_create_info.command_buffer   = frame_ptr->last_cached_command_buffer;
    draw_present_task_create_info.n_unique_inputs  = sizeof(draw_present_task_inputs) / sizeof(draw_present_task_inputs[0]);
    draw_present_task_create_info.n_unique_outputs = 1;
    draw_present_task_create_info.unique_inputs    = draw_present_task_inputs;
    draw_present_task_create_info.unique_outputs   = &draw_present_task_output;

    draw_present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("UI frame: draw"),
                                                   &draw_present_task_create_info);


    present_tasks[0] = cpu_present_task;
    present_tasks[1] = draw_present_task;

    result_present_task_ingroup_connection.input_present_task_index     = 1;
    result_present_task_ingroup_connection.input_present_task_io_index  = 0;
    result_present_task_ingroup_connection.output_present_task_index    = 0;
    result_present_task_ingroup_connection.output_present_task_io_index = 0;


    result_present_task_input_mapping.group_task_io_index   = 0;
    result_present_task_input_mapping.present_task_io_index = 1; /* target_texture_view */
    result_present_task_input_mapping.n_present_task        = 1;

    result_present_task_output_mapping.group_task_io_index   = 0;
    result_present_task_output_mapping.present_task_io_index = 0; /* target_texture_view */
    result_present_task_output_mapping.n_present_task        = 1;

    result_present_task_create_info.ingroup_connections                      = &result_present_task_ingroup_connection;
    result_present_task_create_info.n_ingroup_connections                    = 1;
    result_present_task_create_info.n_present_tasks                          = sizeof(present_tasks) / sizeof(present_tasks[0]);
    result_present_task_create_info.n_total_unique_inputs                    = 1;
    result_present_task_create_info.n_total_unique_outputs                   = 1;
    result_present_task_create_info.n_unique_input_to_ingroup_task_mappings  = result_present_task_create_info.n_total_unique_inputs;
    result_present_task_create_info.n_unique_output_to_ingroup_task_mappings = result_present_task_create_info.n_total_unique_outputs;
    result_present_task_create_info.present_tasks                            = present_tasks;
    result_present_task_create_info.unique_input_to_ingroup_task_mapping     = &result_present_task_input_mapping;
    result_present_task_create_info.unique_output_to_ingroup_task_mapping    = &result_present_task_output_mapping;

    result_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("UI frame: rasterization"),
                                                        &result_present_task_create_info);


    frame_ptr->last_cached_present_task                     = result_present_task;
    frame_ptr->last_cached_present_task_target_texture_view = target_texture_view;

    ral_present_task_release(cpu_present_task);
    ral_present_task_release(draw_present_task);
end:
    ral_present_task_retain(frame_ptr->last_cached_present_task);

    return frame_ptr->last_cached_present_task;
}

/** Please see header for specification */
PUBLIC void ui_frame_get_property(const void*         frame,
                                  ui_control_property property,
                                  void*               out_result)
{
    const _ui_frame* frame_ptr = reinterpret_cast<const _ui_frame*>(frame);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            *(float*) out_result = frame_ptr->x1y1x2y2[3] - frame_ptr->x1y1x2y2[1];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = frame_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
        {
            ((float*) out_result)[0] =        frame_ptr->x1y1x2y2[0];
            ((float*) out_result)[1] = 1.0f - frame_ptr->x1y1x2y2[1];
            ((float*) out_result)[2] =        frame_ptr->x1y1x2y2[2];
            ((float*) out_result)[3] = 1.0f - frame_ptr->x1y1x2y2[3];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC void* ui_frame_init(ui           instance,
                           const float* x1y1x2y2)
{
    _ui_frame* new_frame_ptr = new (std::nothrow) _ui_frame;

    ASSERT_ALWAYS_SYNC(new_frame_ptr != nullptr,
                       "Out of memory");

    if (new_frame_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_frame_ptr,
               0,
               sizeof(_ui_frame) );

        ui_get_property(instance,
                        UI_PROPERTY_CONTEXT,
                       &new_frame_ptr->context);

        new_frame_ptr->visible     = true;
        new_frame_ptr->x1y1x2y2[0] = x1y1x2y2[0];
        new_frame_ptr->x1y1x2y2[1] = 1.0f - x1y1x2y2[1];
        new_frame_ptr->x1y1x2y2[2] = x1y1x2y2[2];
        new_frame_ptr->x1y1x2y2[3] = 1.0f - x1y1x2y2[3];

        /* Retrieve the rendering program */
        new_frame_ptr->program = ui_get_registered_program(instance,
                                                           ui_frame_program_name);

        if (new_frame_ptr->program == nullptr)
        {
            _ui_frame_init_program(instance,
                                   new_frame_ptr);

            ASSERT_DEBUG_SYNC(new_frame_ptr->program != nullptr,
                              "Could not initialize frame UI program");
        }

        /* Retrieve the uniform block properties */
        ral_buffer program_ub_bo_ral = nullptr;

        new_frame_ptr->program_ub = ral_program_block_buffer_create(new_frame_ptr->context,
                                                                    new_frame_ptr->program,
                                                                    system_hashed_ansi_string_create("dataVS") );

        ral_program_block_buffer_get_property(new_frame_ptr->program_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_ub_bo_ral);
        ral_buffer_get_property              (program_ub_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &new_frame_ptr->program_ub_bo_size);

        /* Retrieve the uniforms */
        const ral_program_variable* x1y1x2y2_uniform_ral_ptr = nullptr;

        ral_program_get_block_variable_by_name(new_frame_ptr->program,
                                               system_hashed_ansi_string_create("dataVS"),
                                               system_hashed_ansi_string_create("x1y1x2y2"),
                                              &x1y1x2y2_uniform_ral_ptr);

        new_frame_ptr->program_x1y1x2y2_ub_offset = x1y1x2y2_uniform_ral_ptr->block_offset;
    }

    return reinterpret_cast<void*>(new_frame_ptr);
}

/** Please see header for specification */
PUBLIC void ui_frame_set_property(void*               frame,
                                  ui_control_property property,
                                  const void*         data)
{
    _ui_frame* frame_ptr = reinterpret_cast<_ui_frame*>(frame);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_FRAME_X1Y1X2Y2:
        {
            const float* x1y1x2y2 = (const float*) data;

            memcpy(frame_ptr->x1y1x2y2,
                   x1y1x2y2,
                   sizeof(float) * 4);

            frame_ptr->x1y1x2y2[1] = 1.0f - frame_ptr->x1y1x2y2[1];
            frame_ptr->x1y1x2y2[3] = 1.0f - frame_ptr->x1y1x2y2[3];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}