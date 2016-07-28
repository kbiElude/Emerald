/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
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
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_normals_preview.h"
#include "system/system_matrix4x4.h"
#include <string.h>

static const char* preview_fragment_shader =
    "#version 430 core\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(0, 1, 0, 1);\n"
    "}\n";
static const char* preview_geometry_shader =
    "#version 430 core\n"
    "\n"
    "layout(points)                     in;\n"
    "layout(line_strip, max_vertices=2) out;\n"
    "\n"
    "in vec3 normal[];\n"
    "in vec3 vertex[];\n"
    "\n"
    "uniform dataGS\n"
    "{\n"
    "    mat4 normal_matrix;\n"
    "    mat4 vp;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    mat4 mvp = vp * normal_matrix;\n"
    "\n"
    "    gl_Position = mvp * vec4(vertex[0], 1);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(vertex[0] + normal[0], 1);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    "}\n";
static const char* preview_vertex_shader =
    "#version 430 core\n"
    "\n"
    "#extension GL_ARB_shader_storage_buffer_object : require\n"
    "\n"
    /* NOTE: We declare the SSBO in vertex shader instead of geometry shader stage,
     *       owing to the fact that GL_ARB_shader_storage_buffer_object requires GL
     *       implementations to support 8 buffers at minimum in VS but 0 in GS.
     */
    "layout(std430, binding = 0) buffer ssb\n"
    "{\n"
    "    restrict readonly float data[];\n"
    "};\n"
    "\n"
    "uniform dataVS\n"
    "{\n"
    "    uvec2 start_offsets;\n"
    "    uint  stride;\n"
    "};\n"
    "\n"
    "out vec3 normal;\n"
    "out vec3 vertex;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    uint base_normal_offset = (start_offsets[0] + stride * gl_VertexID) / 4;\n"
    "    uint base_vertex_offset = (start_offsets[1] + stride * gl_VertexID) / 4;\n"
    "\n"
    "    normal = vec3(data[base_normal_offset],\n"
    "                  data[base_normal_offset + 1],\n"
    "                  data[base_normal_offset + 2]);\n"
    "    vertex = vec3(data[base_vertex_offset],\n"
    "                  data[base_vertex_offset + 1],\n"
    "                  data[base_vertex_offset + 2]);\n"
    "}\n";

/** TODO */
typedef struct _scene_renderer_normals_preview
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ral_context context;

    ral_texture_view         cached_color_rt;
    ral_command_buffer       cached_command_buffer;
    ral_texture_view         cached_depth_rt;
    ral_gfx_state            cached_gfx_state;
    scene                    owned_scene;
    scene_renderer           owner;
    ral_program              preview_program;
    uint32_t                 preview_program_normal_matrix_ub_offset;
    uint32_t                 preview_program_start_offsets_ub_offset;
    uint32_t                 preview_program_stride_ub_offset;
    ral_program_block_buffer preview_program_ub_gs;
    ral_program_block_buffer preview_program_ub_vs;
    uint32_t                 preview_program_vp_ub_offset;
} _scene_renderer_normals_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _scene_renderer_normals_preview_verify_context_type(ogl_context);
#else
    #define _scene_renderer_normals_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _scene_renderer_normals_preview_init(_scene_renderer_normals_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == nullptr,
                      "Preview program has already been initialized");

    ral_gfx_state_create_info   gfx_state_create_info;
    const ral_program_variable* normal_matrix_uniform_ral_ptr = nullptr;
    const ral_program_variable* start_offsets_uniform_ral_ptr = nullptr;
    const ral_program_variable* stride_uniform_ral_ptr        = nullptr;
    const ral_program_variable* vp_uniform_ral_ptr            = nullptr;

    /* Create shaders and set their bodies */
    ral_shader                fs         = nullptr;
    ral_shader                gs         = nullptr;
    system_hashed_ansi_string scene_name = nullptr;
    ral_shader                vs         = nullptr;

    const system_hashed_ansi_string fs_body_has    = system_hashed_ansi_string_create(preview_fragment_shader);
    const ral_shader_create_info    fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview FS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const system_hashed_ansi_string gs_body_has    = system_hashed_ansi_string_create(preview_geometry_shader);
    const ral_shader_create_info    gs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview GS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_GEOMETRY
    };
    const system_hashed_ansi_string vs_body_has    = system_hashed_ansi_string_create(preview_vertex_shader);
    const ral_shader_create_info    vs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview VS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        gs_create_info,
        vs_create_info
    };
    const uint32_t               n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader                   result_shaders[n_shader_create_info_items];


    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);


    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview program ",
                                                                system_hashed_ansi_string_get_buffer(scene_name) )
    };


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
    gs = result_shaders[1];
    vs = result_shaders[2];

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body_has);
    ral_shader_set_property(gs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &gs_body_has);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body_has);

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
                                   gs,
                                   true /* async */) ||
        !ral_program_attach_shader(preview_ptr->preview_program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");

        goto end_fail;
    }

    /* Retrieve uniform locations */
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("dataGS"),
                                           system_hashed_ansi_string_create("normal_matrix"),
                                          &normal_matrix_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("start_offsets"),
                                          &start_offsets_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("stride"),
                                          &stride_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("dataGS"),
                                           system_hashed_ansi_string_create("vp"),
                                          &vp_uniform_ral_ptr);

    ASSERT_DEBUG_SYNC(normal_matrix_uniform_ral_ptr != nullptr,
                      "Normal matrix uniform not recognized");
    ASSERT_DEBUG_SYNC(start_offsets_uniform_ral_ptr != nullptr,
                      "Start offsets uniform not recognized");
    ASSERT_DEBUG_SYNC(stride_uniform_ral_ptr != nullptr,
                      "Stride uniform not recognized");
    ASSERT_DEBUG_SYNC(vp_uniform_ral_ptr != nullptr,
                      "VP uniform not recognized");

    if (normal_matrix_uniform_ral_ptr != nullptr)
    {
        preview_ptr->preview_program_normal_matrix_ub_offset = normal_matrix_uniform_ral_ptr->block_offset;
    }

    if (start_offsets_uniform_ral_ptr != nullptr)
    {
        preview_ptr->preview_program_start_offsets_ub_offset = start_offsets_uniform_ral_ptr->block_offset;
    }

    if (stride_uniform_ral_ptr != nullptr)
    {
        preview_ptr->preview_program_stride_ub_offset = stride_uniform_ral_ptr->block_offset;
    }

    if (vp_uniform_ral_ptr != nullptr)
    {
        preview_ptr->preview_program_vp_ub_offset = vp_uniform_ral_ptr->block_offset;
    }

    /* Retrieve UB properties */
    preview_ptr->preview_program_ub_gs = ral_program_block_buffer_create(preview_ptr->context,
                                                                         preview_ptr->preview_program,
                                                                         system_hashed_ansi_string_create("dataGS") );
    preview_ptr->preview_program_ub_vs = ral_program_block_buffer_create(preview_ptr->context,
                                                                         preview_ptr->preview_program,
                                                                         system_hashed_ansi_string_create("dataVS") );
    preview_ptr->preview_program_ub_vs = nullptr;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub_gs != nullptr &&
                      preview_ptr->preview_program_ub_vs != nullptr,
                      "Could not create UB block buffers");

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

        preview_ptr->preview_program = nullptr;
    }

end:
    ral_shader shaders_to_release[] =
    {
        fs,
        gs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** Please see header for spec */
PUBLIC scene_renderer_normals_preview scene_renderer_normals_preview_create(ral_context    context,
                                                                            scene          scene,
                                                                            scene_renderer owner)
{
    _scene_renderer_normals_preview* new_instance_ptr = new (std::nothrow) _scene_renderer_normals_preview;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != nullptr,
                       "Out of memory");

    if (new_instance_ptr != nullptr)
    {
        new_instance_ptr->cached_command_buffer                   = nullptr;
        new_instance_ptr->cached_gfx_state                        = nullptr;
        new_instance_ptr->context                                 = context;
        new_instance_ptr->owned_scene                             = scene;
        new_instance_ptr->owner                                   = owner;
        new_instance_ptr->preview_program                         = nullptr;
        new_instance_ptr->preview_program_normal_matrix_ub_offset = -1;
        new_instance_ptr->preview_program_start_offsets_ub_offset = -1;
        new_instance_ptr->preview_program_stride_ub_offset        = -1;
        new_instance_ptr->preview_program_vp_ub_offset            = -1;

        scene_retain(scene);

        _scene_renderer_normals_preview_init(new_instance_ptr);
    }

    return (scene_renderer_normals_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_normals_preview_release(scene_renderer_normals_preview preview)
{
    _scene_renderer_normals_preview* preview_ptr = reinterpret_cast<_scene_renderer_normals_preview*>(preview);

    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                               1, /* n_objects */
                               (const void**) &preview_ptr->cached_gfx_state);
    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               (const void**) &preview_ptr->cached_command_buffer);

    if (preview_ptr->owned_scene != nullptr)
    {
        scene_release(preview_ptr->owned_scene);
    }

    if (preview_ptr->preview_program != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

        preview_ptr->preview_program = nullptr;
    }

    if (preview_ptr->preview_program_ub_gs != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_ub_gs);

        preview_ptr->preview_program_ub_gs = nullptr;
    }

    if (preview_ptr->preview_program_ub_vs != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_ub_vs);

        preview_ptr->preview_program_ub_vs = nullptr;
    }

    delete preview_ptr;
    preview_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_normals_preview_render(scene_renderer_normals_preview preview,
                                                  uint32_t                       mesh_id)
{
    ral_buffer                       mesh_bo                  = nullptr;
    unsigned int                     mesh_bo_size             = 0;
    mesh                             mesh_instance            = nullptr;
    mesh_type                        mesh_instance_type;
    uint32_t                         mesh_start_offset_normal = -1;
    uint32_t                         mesh_start_offset_vertex = -1;
    uint32_t                         mesh_stride              = -1;
    uint32_t                         mesh_total_elements      = 0;
    system_matrix4x4                 normal_matrix            = nullptr;
    _scene_renderer_normals_preview* preview_ptr              = reinterpret_cast<_scene_renderer_normals_preview*>(preview);

    /* Retrieve mesh properties */
    scene_renderer_get_indexed_property(preview_ptr->owner,
                                        SCENE_RENDERER_PROPERTY_MESH_INSTANCE,
                                        mesh_id,
                                       &mesh_instance);
    scene_renderer_get_indexed_property(preview_ptr->owner,
                                        SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX,
                                        mesh_id,
                                       &normal_matrix);

    /* Only regular meshes are supported at the moment. Throw an assertion failure for other
     * mesh types. */
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_TYPE,
                     &mesh_instance_type);

    ASSERT_DEBUG_SYNC(mesh_instance_type == MESH_TYPE_REGULAR,
                      "Only regular meshes are supported for the normals preview.");

    /* Retrieve start offsets & stride information */
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_BO_RAL,
                     &mesh_bo);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_BO_PROCESSED_DATA_SIZE,
                     &mesh_bo_size);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_BO_STRIDE,
                     &mesh_stride);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_BO_TOTAL_ELEMENTS,
                     &mesh_total_elements);

    mesh_get_layer_data_stream_property(mesh_instance,
                                        0, /* layer_id - irrelevant for regular meshes */
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &mesh_start_offset_normal);
    mesh_get_layer_data_stream_property(mesh_instance,
                                        0, /* layer_id - irrelevant for regular meshes */
                                        MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &mesh_start_offset_vertex);

    /* Set the uniforms */
    const uint32_t start_offsets[] =
    {
        mesh_start_offset_normal,
        mesh_start_offset_vertex
    };

    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub_gs,
                                                           preview_ptr->preview_program_normal_matrix_ub_offset,
                                                           system_matrix4x4_get_column_major_data(normal_matrix),
                                                           sizeof(float) * 16);
    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub_vs,
                                                           preview_ptr->preview_program_start_offsets_ub_offset,
                                                           start_offsets,
                                                           sizeof(unsigned int) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub_vs,
                                                           preview_ptr->preview_program_stride_ub_offset,
                                                          &mesh_stride,
                                                           sizeof(unsigned int) );

    ral_program_block_buffer_sync_via_command_buffer(preview_ptr->preview_program_ub_gs,
                                                     preview_ptr->cached_command_buffer);
    ral_program_block_buffer_sync_via_command_buffer(preview_ptr->preview_program_ub_vs,
                                                     preview_ptr->cached_command_buffer);

    /* Set up shader storage buffer binding */
    ral_command_buffer_set_binding_command_info ssb_binding;

    ssb_binding.binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
    ssb_binding.storage_buffer_binding.buffer = mesh_bo;
    ssb_binding.storage_buffer_binding.offset = 0;
    ssb_binding.storage_buffer_binding.size   = mesh_bo_size;

    ral_command_buffer_record_set_bindings(preview_ptr->cached_command_buffer,
                                           1, /* n_bindings */
                                          &ssb_binding);

    /* Draw */
    ral_command_buffer_draw_call_regular_command_info draw_call_info;

    draw_call_info.base_instance = 0;
    draw_call_info.base_vertex   = 0;
    draw_call_info.n_instances   = 1;
    draw_call_info.n_vertices    = mesh_total_elements;

    ral_command_buffer_record_draw_call_regular(preview_ptr->cached_command_buffer,
                                                1, /* n_draw_calls */
                                               &draw_call_info);
}

/** Please see header for spec */
PUBLIC void scene_renderer_normals_preview_start(scene_renderer_normals_preview preview,
                                                 system_matrix4x4               vp,
                                                 ral_texture_view               color_rt,
                                                 ral_texture_view               depth_rt)
{
    uint32_t                         color_rt_size[2];
    _scene_renderer_normals_preview* preview_ptr = reinterpret_cast<_scene_renderer_normals_preview*>(preview);

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

    /* Can we re-use the gfx_state instance we may have already initialized? */
    if (preview_ptr->cached_gfx_state != nullptr)
    {
        ral_command_buffer_set_viewport_command_info* cached_gfx_state_viewport_ptr = nullptr;

        ral_gfx_state_get_property(preview_ptr->cached_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &cached_gfx_state_viewport_ptr);

        if (cached_gfx_state_viewport_ptr[0].size[0] != color_rt_size[0] ||
            cached_gfx_state_viewport_ptr[0].size[1] != color_rt_size[1])
        {
            ral_context_delete_objects(preview_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                       1, /* n_objects */
                                       (const void**) &preview_ptr->cached_gfx_state);

            preview_ptr->cached_gfx_state = nullptr;
        }
    }

    if (preview_ptr->cached_gfx_state == nullptr)
    {
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_scissor_box_command_info scissor_box;
        ral_command_buffer_set_viewport_command_info    viewport;

        /* Set up a gfx_state instance */
        scissor_box.index   = 0;
        scissor_box.size[0] = color_rt_size[0];
        scissor_box.size[1] = color_rt_size[1];
        scissor_box.xy  [0] = 0;
        scissor_box.xy  [1] = 0;

        viewport.depth_range[0] = 0.0f;
        viewport.depth_range[1] = 1.0f;
        viewport.index          = 0;
        viewport.size[0]        = static_cast<float>(color_rt_size[0]);
        viewport.size[1]        = static_cast<float>(color_rt_size[1]);
        viewport.xy  [0]        = 0;
        viewport.xy  [1]        = 0;

        gfx_state_create_info.depth_test                           = true;
        gfx_state_create_info.depth_test_compare_op                = RAL_COMPARE_OP_LESS;
        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_POINTS;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &scissor_box;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &viewport;
        gfx_state_create_info.static_viewports_enabled             = true;

        preview_ptr->cached_gfx_state = ral_gfx_state_create(preview_ptr->context,
                                                            &gfx_state_create_info);
    }

    /* Start recording commands */
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
        ral_buffer preview_program_ub_gs_bo_ral  = nullptr;
        uint32_t   preview_program_ub_gs_bo_size = -1;
        ral_buffer preview_program_ub_vs_bo_ral  = nullptr;
        uint32_t   preview_program_ub_vs_bo_size = -1;

        ral_program_block_buffer_get_property(preview_ptr->preview_program_ub_gs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &preview_program_ub_gs_bo_ral);
        ral_program_block_buffer_get_property(preview_ptr->preview_program_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &preview_program_ub_vs_bo_ral);

        ral_buffer_get_property(preview_program_ub_gs_bo_ral,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &preview_program_ub_gs_bo_size);
        ral_buffer_get_property(preview_program_ub_vs_bo_ral,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &preview_program_ub_vs_bo_size);

        /* Set up contents of uniform blocks that will be shared across subsequent draw calls */
        ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub_gs,
                                                               preview_ptr->preview_program_vp_ub_offset,
                                                               system_matrix4x4_get_column_major_data(vp),
                                                               sizeof(float) * 16);

        /* Start recording actual commands .. */
        ral_command_buffer_set_binding_command_info ub_bindings[2];

        ub_bindings[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_bindings[0].name                          = system_hashed_ansi_string_create("dataGS");
        ub_bindings[0].uniform_buffer_binding.buffer = preview_program_ub_gs_bo_ral;
        ub_bindings[0].uniform_buffer_binding.offset = 0;
        ub_bindings[0].uniform_buffer_binding.size   = preview_program_ub_gs_bo_size;

        ub_bindings[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        ub_bindings[1].name                          = system_hashed_ansi_string_create("dataVS");
        ub_bindings[1].uniform_buffer_binding.buffer = preview_program_ub_vs_bo_ral;
        ub_bindings[1].uniform_buffer_binding.offset = 0;
        ub_bindings[1].uniform_buffer_binding.size   = preview_program_ub_vs_bo_size;

        ral_command_buffer_record_set_bindings(preview_ptr->cached_command_buffer,
                                               sizeof(ub_bindings) / sizeof(ub_bindings[0]),
                                               ub_bindings);
        ral_command_buffer_record_set_program (preview_ptr->cached_command_buffer,
                                               preview_ptr->preview_program);
    }

    preview_ptr->cached_color_rt = color_rt;
    preview_ptr->cached_depth_rt = depth_rt;
}

/** Please see header for spec */
PUBLIC ral_present_task scene_renderer_normals_preview_stop(scene_renderer_normals_preview preview)
{
    _scene_renderer_normals_preview* preview_ptr = reinterpret_cast<_scene_renderer_normals_preview*>(preview);

    ral_command_buffer_stop_recording(preview_ptr->cached_command_buffer);

    /* Form the result present task */
    ral_present_task                 result_task;
    ral_present_task_gpu_create_info result_task_create_info;
    ral_present_task_io              result_task_ios[2];

    result_task_ios[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    result_task_ios[0].texture_view = preview_ptr->cached_color_rt;

    result_task_ios[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    result_task_ios[1].texture_view = preview_ptr->cached_depth_rt;

    result_task_create_info.command_buffer   = preview_ptr->cached_command_buffer;
    result_task_create_info.n_unique_inputs  = sizeof(result_task_ios) / sizeof(result_task_ios[0]);
    result_task_create_info.n_unique_outputs = sizeof(result_task_ios) / sizeof(result_task_ios[0]);
    result_task_create_info.unique_inputs    = result_task_ios;
    result_task_create_info.unique_outputs   = result_task_ios;

    result_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scene renderer: Normals preview"),
                                             &result_task_create_info);

    return result_task;
}


