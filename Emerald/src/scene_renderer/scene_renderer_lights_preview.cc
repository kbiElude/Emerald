/**
 *
 * Emerald (kbi/elude @2015-2016)
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
#include "scene/scene.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_lights_preview.h"
#include "system/system_matrix4x4.h"
#include <string.h>

/* This is pretty basic stuff to draw white splats in clip space */
static const char* preview_fragment_shader =
    "#version 430 core\n"
    "\n"
    "uniform data\n"
    "{\n"
    "    vec3 color;\n"
    "    vec4 position[2];\n"
    "};\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(color, 1.0);\n"
    "}\n";

static const char* preview_vertex_shader =
    "#version 430 core\n"
    "\n"
    "uniform data\n"
    "{\n"
    "    vec3 color;\n"
    "    vec4 position[2];\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_PointSize = 16.0;\n"
    "    gl_Position  = position[gl_VertexID];\n"
    "}\n";

/** TODO */
typedef struct _scene_renderer_lights_preview
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ral_context context;

    ral_texture_view         cached_color_rt;
    uint32_t                 cached_color_rt_size[2];
    ral_command_buffer       cached_command_buffer;
    ral_gfx_state            cached_gfx_state_lines;
    ral_gfx_state            cached_gfx_state_points;
    scene                    owned_scene;
    ral_program              preview_program;
    ral_program_block_buffer preview_program_ub;
    uint32_t                 preview_program_color_ub_offset;
    uint32_t                 preview_program_position_ub_offset;

    _scene_renderer_lights_preview()
    {
        memset(cached_color_rt_size,
               0,
               sizeof(cached_color_rt_size) );

        cached_color_rt                    = nullptr;
        cached_command_buffer              = nullptr;
        cached_gfx_state_lines             = nullptr;
        cached_gfx_state_points            = nullptr;
        context                            = nullptr;
        owned_scene                        = nullptr;
        preview_program                    = nullptr;
        preview_program_ub                 = nullptr;
        preview_program_color_ub_offset    = -1;
        preview_program_position_ub_offset = -1;
    }

} _scene_renderer_lights_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _scene_renderer_lights_preview_verify_context_type(ogl_context);
#else
    #define _scene_renderer_lights_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _scene_renderer_lights_preview_init(_scene_renderer_lights_preview* preview_ptr)
{
    const ral_program_variable* color_uniform_ral_ptr     = nullptr;
    const ral_program_variable* position_uniform_ral_ptr  = nullptr;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == nullptr,
                      "Preview program has already been initialized");


    /* Create shaders and set their bodies */
    ral_shader                      fs         = nullptr;
    const system_hashed_ansi_string fs_body    = system_hashed_ansi_string_create(preview_fragment_shader);
    system_hashed_ansi_string       scene_name = nullptr;
    ral_shader                      vs         = nullptr;
    const system_hashed_ansi_string vs_body    = system_hashed_ansi_string_create(preview_vertex_shader);

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview FS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview VS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer lights preview program ",
                                                                system_hashed_ansi_string_get_buffer(scene_name))
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);

    ral_shader result_shaders[n_shader_create_info_items];


    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    if (!ral_context_create_shaders(preview_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed");

        goto end_fail;
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Initialize the program object */
    if (!ral_context_create_programs(preview_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &preview_ptr->preview_program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");

        goto end_fail;
    }

    if (!ral_program_attach_shader(preview_ptr->preview_program,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(preview_ptr->preview_program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");

        goto end_fail;
    }

    /* Retrieve UB properties */
    preview_ptr->preview_program_ub = ral_program_block_buffer_create(preview_ptr->context,
                                                                      preview_ptr->preview_program,
                                                                      system_hashed_ansi_string_create("data") );

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub != nullptr,
                      "Data UB is nullptr");

    /* Retrieve uniform properties */
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("color"),
                                          &color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("position[0]"),
                                          &position_uniform_ral_ptr);

    preview_ptr->preview_program_color_ub_offset    = color_uniform_ral_ptr->block_offset;
    preview_ptr->preview_program_position_ub_offset = position_uniform_ral_ptr->block_offset;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_color_ub_offset    != -1 &&
                      preview_ptr->preview_program_position_ub_offset != -1,
                      "At least one uniform UB offset is -1");

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->preview_program) );

        preview_ptr->preview_program = nullptr;
    }

end:
    const ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               reinterpret_cast<void* const*>(shaders_to_release) );
}


/** Please see header for spec */
PUBLIC scene_renderer_lights_preview scene_renderer_lights_preview_create(ral_context context,
                                                                          scene       scene)
{
    _scene_renderer_lights_preview* new_instance_ptr = new (std::nothrow) _scene_renderer_lights_preview;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != nullptr,
                       "Out of memory");

    if (new_instance_ptr != nullptr)
    {
        new_instance_ptr->context         = context;
        new_instance_ptr->owned_scene     = scene;
        new_instance_ptr->preview_program = nullptr;

        _scene_renderer_lights_preview_init(new_instance_ptr);

        scene_retain(scene);
    }

    return reinterpret_cast<scene_renderer_lights_preview>(new_instance_ptr);
}

/** Please see header for spec */
PUBLIC void scene_renderer_lights_preview_release(scene_renderer_lights_preview preview)
{
    _scene_renderer_lights_preview* preview_ptr = reinterpret_cast<_scene_renderer_lights_preview*>(preview);

    ral_gfx_state gfx_states[] =
    {
        preview_ptr->cached_gfx_state_lines,
        preview_ptr->cached_gfx_state_points
    };

    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&preview_ptr->cached_command_buffer) );
    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               sizeof(gfx_states) / sizeof(gfx_states[0]),
                               reinterpret_cast<void* const*>(gfx_states) );


    preview_ptr->cached_command_buffer   = nullptr;
    preview_ptr->cached_gfx_state_lines  = nullptr;
    preview_ptr->cached_gfx_state_points = nullptr;


    if (preview_ptr->preview_program != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->preview_program) );

        preview_ptr->preview_program = nullptr;
    }

    if (preview_ptr->preview_program_ub != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_ub);

        preview_ptr->preview_program_ub = nullptr;
    }

    if (preview_ptr->owned_scene != nullptr)
    {
        scene_release(preview_ptr->owned_scene);

        preview_ptr->owned_scene = nullptr;
    }

    delete preview_ptr;
    preview_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_lights_preview_render(scene_renderer_lights_preview preview,
                                                 float*                        light_position,
                                                 float*                        light_color,
                                                 float*                        light_pos_plus_direction)
{
    _scene_renderer_lights_preview* preview_ptr = reinterpret_cast<_scene_renderer_lights_preview*>(preview);

    /* Set the uniforms */
    float merged_light_positions[8];

    memcpy(merged_light_positions,
           light_position,
           sizeof(float) * 4);

    if (light_pos_plus_direction != nullptr)
    {
        memcpy(merged_light_positions + 4,
               light_pos_plus_direction,
               sizeof(float) * 4);
    }

    ral_program_block_buffer_set_arrayed_variable_value   (preview_ptr->preview_program_ub,
                                                           preview_ptr->preview_program_position_ub_offset,
                                                           merged_light_positions,
                                                           sizeof(float) * 4,
                                                           0, /* dst_array_start_index */
                                                           ((light_pos_plus_direction != nullptr) ? 2 : 1) );
    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub,
                                                           preview_ptr->preview_program_color_ub_offset,
                                                           light_color,
                                                           sizeof(float) * 3);

    ral_program_block_buffer_sync_via_command_buffer(preview_ptr->preview_program_ub,
                                                     preview_ptr->cached_command_buffer);

    /* Draw. This probably beats the world's worst way of achieving this functionality,
     * but light preview is only used for debugging purposes, so not much sense
     * in writing an overbloated implementation.
     */
    ral_command_buffer_draw_call_regular_command_info draw_call_info;

    draw_call_info.base_instance = 0;
    draw_call_info.base_vertex   = 0;
    draw_call_info.n_instances   = 1;
    draw_call_info.n_vertices    = 1;

    ral_command_buffer_record_set_gfx_state    (preview_ptr->cached_command_buffer,
                                                preview_ptr->cached_gfx_state_points);
    ral_command_buffer_record_draw_call_regular(preview_ptr->cached_command_buffer,
                                                1, /* n_draw_calls */
                                               &draw_call_info);


    if (light_pos_plus_direction != nullptr)
    {
        draw_call_info.n_vertices = 2;

        ral_command_buffer_record_set_gfx_state    (preview_ptr->cached_command_buffer,
                                                    preview_ptr->cached_gfx_state_lines);
        ral_command_buffer_record_draw_call_regular(preview_ptr->cached_command_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call_info);
    }
}

/** Please see header for spec */
PUBLIC void scene_renderer_lights_preview_start(scene_renderer_lights_preview preview,
                                                ral_texture_view              color_rt)
{
    uint32_t                          color_rt_size[2];
    const ogl_context_gl_entrypoints* entrypoints_ptr = nullptr;
    _scene_renderer_lights_preview*   preview_ptr     = reinterpret_cast<_scene_renderer_lights_preview*>(preview);
    ral_buffer                        ub_bo_ral       = nullptr;
    uint32_t                          ub_bo_size      = -1;

    ral_program_block_buffer_get_property(preview_ptr->preview_program_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_bo_ral);
    ral_buffer_get_property              (ub_bo_ral,
                                          RAL_BUFFER_PROPERTY_START_OFFSET,
                                         &ub_bo_size);

    /* If a gfx_state instance is already available, make sure it can be reused */
    ral_texture_view_get_mipmap_property(color_rt,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         color_rt_size + 0);
    ral_texture_view_get_mipmap_property(color_rt,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                         color_rt_size + 1);

    if ( preview_ptr->cached_gfx_state_lines  != nullptr          &&
        (preview_ptr->cached_color_rt_size[0] != color_rt_size[0] ||
         preview_ptr->cached_color_rt_size[1] != color_rt_size[1]) )
    {
        const ral_gfx_state gfx_states_to_release[] =
        {
            preview_ptr->cached_gfx_state_lines,
            preview_ptr->cached_gfx_state_points
        };
        const uint32_t n_gfx_states_to_release = sizeof(gfx_states_to_release) / sizeof(gfx_states_to_release[0]);

        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   n_gfx_states_to_release,
                                   reinterpret_cast<void* const*>(gfx_states_to_release) );

        preview_ptr->cached_gfx_state_lines = nullptr;
        preview_ptr->cached_gfx_state_points = nullptr;
    }

    if (preview_ptr->cached_gfx_state_lines == nullptr)
    {
        ral_gfx_state_create_info                       gfx_state_create_info_items[2];
        ral_gfx_state                                   result_gfx_states[2];
        ral_command_buffer_set_scissor_box_command_info scissor_box;
        ral_command_buffer_set_viewport_command_info    viewport;

        scissor_box.index   = 0;
        scissor_box.size[0] = color_rt_size[0];
        scissor_box.size[1] = color_rt_size[1];
        scissor_box.xy  [0] = 0;
        scissor_box.xy  [0] = 1;

        viewport.depth_range[0] = 0.0f;
        viewport.depth_range[1] = 1.0f;
        viewport.index          = 0;
        viewport.size[0]        = static_cast<float>(color_rt_size[0]);
        viewport.size[1]        = static_cast<float>(color_rt_size[1]);
        viewport.xy  [0]        = 0;
        viewport.xy  [1]        = 0;

        gfx_state_create_info_items[0].line_width                           = 4.0f;
        gfx_state_create_info_items[0].primitive_type                       = RAL_PRIMITIVE_TYPE_LINES;
        gfx_state_create_info_items[0].static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info_items[0].static_scissor_boxes                 = &scissor_box;
        gfx_state_create_info_items[0].static_scissor_boxes_enabled         = true;
        gfx_state_create_info_items[0].static_viewports                     = &viewport;
        gfx_state_create_info_items[0].static_viewports_enabled             = true;

        gfx_state_create_info_items[1]                = gfx_state_create_info_items[0];
        gfx_state_create_info_items[1].primitive_type = RAL_PRIMITIVE_TYPE_POINTS;

        ral_context_create_gfx_states(preview_ptr->context,
                                      sizeof(gfx_state_create_info_items) / sizeof(gfx_state_create_info_items[0]),
                                      gfx_state_create_info_items,
                                      result_gfx_states);

        preview_ptr->cached_gfx_state_lines  = result_gfx_states[0];
        preview_ptr->cached_gfx_state_points = result_gfx_states[1];
    }

    /* Start recording the command buffer */
    if (preview_ptr->cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info cmd_buffer_create_info;

        cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        cmd_buffer_create_info.is_executable                           = true;
        cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
        cmd_buffer_create_info.is_resettable                           = true;
        cmd_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(preview_ptr->context,
                                           1, /* n_command_buffers */
                                          &cmd_buffer_create_info,
                                          &preview_ptr->cached_command_buffer);
    }

    ral_command_buffer_start_recording(preview_ptr->cached_command_buffer);
    {
        ral_command_buffer_set_binding_command_info            binding_info_items[2];
        ral_command_buffer_set_color_rendertarget_command_info color_rt_info = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();

        color_rt_info.rendertarget_index = 0;
        color_rt_info.texture_view       = color_rt;

        binding_info_items[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        binding_info_items[0].name                          = system_hashed_ansi_string_create("data");
        binding_info_items[0].uniform_buffer_binding.buffer = ub_bo_ral;
        binding_info_items[0].uniform_buffer_binding.offset = 0;
        binding_info_items[0].uniform_buffer_binding.size   = ub_bo_size;

        binding_info_items[1].binding_type                  = RAL_BINDING_TYPE_RENDERTARGET;
        binding_info_items[1].name                          = system_hashed_ansi_string_create("result");
        binding_info_items[1].rendertarget_binding.rt_index = 0;

        ral_command_buffer_record_set_bindings           (preview_ptr->cached_command_buffer,
                                                          sizeof(binding_info_items) / sizeof(binding_info_items[0]),
                                                          binding_info_items);
        ral_command_buffer_record_set_color_rendertargets(preview_ptr->cached_command_buffer,
                                                          1, /* n_rendertargets */
                                                         &color_rt_info);
        ral_command_buffer_record_set_program            (preview_ptr->cached_command_buffer,
                                                          preview_ptr->preview_program);
    }

    preview_ptr->cached_color_rt = color_rt;
}

/** Please see header for spec */
PUBLIC ral_present_task scene_renderer_lights_preview_stop(scene_renderer_lights_preview preview)
{
    _scene_renderer_lights_preview*  preview_ptr             = reinterpret_cast<_scene_renderer_lights_preview*>(preview);
    ral_present_task                 result_task             = nullptr;
    ral_present_task_gpu_create_info result_task_create_info;
    ral_present_task_io              result_task_io;

    /* First, stop recording the command buffer we're going to use for the result present task */
    ral_command_buffer_stop_recording(preview_ptr->cached_command_buffer);

    /* Instantiate the requested present task */
    result_task_io.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    result_task_io.texture_view = preview_ptr->cached_color_rt;

    result_task_create_info.command_buffer   = preview_ptr->cached_command_buffer;
    result_task_create_info.n_unique_inputs  = 1;
    result_task_create_info.n_unique_outputs = 1;
    result_task_create_info.unique_inputs    = &result_task_io;
    result_task_create_info.unique_outputs   = &result_task_io;

    result_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scene renderer: Lights preview"),
                                             &result_task_create_info);

    return result_task;
}


