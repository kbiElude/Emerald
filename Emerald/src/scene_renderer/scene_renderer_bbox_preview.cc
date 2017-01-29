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
#include "ral/ral_texture_view.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_bbox_preview.h"
#include "system/system_matrix4x4.h"
#include <string.h>

static const char* preview_fragment_shader =
    "#version 430 core\n"
    "\n"
    "out vec4 result;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    result = vec4(1, 1, 1, 1);\n"
    "}\n";
static const char* preview_geometry_shader =
    "#version 430 core\n"
    "\n"
    "#define N_MESHES (__value)\n"
    "\n"
    "layout(points)                       in;\n"
    "layout(line_strip, max_vertices=18) out;\n"
    "\n"
    "layout(location = 0) in uint vertex_id[];\n"
    "\n"
    "layout(std140) uniform data\n"
    "{\n"
    "                      vec4 aabb_max[N_MESHES];\n"
    "                      vec4 aabb_min[N_MESHES];\n"
    "    layout(row_major) mat4 model;\n"
    "    layout(row_major) mat4 vp;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec4 aabb_max = aabb_max[vertex_id[0] ];\n"
    "    vec4 aabb_min = aabb_min[vertex_id[0] ];\n"
    "    mat4 mvp      = vp * model;\n"
    "\n"
    /* Bottom plane */
    "    gl_Position = mvp * vec4(aabb_min.xy, aabb_max.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.xyz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_max.x, aabb_min.yz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_max.x, aabb_min.y, aabb_max.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.xy, aabb_max.z, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    /* Top plane */
    "    gl_Position = mvp * vec4(aabb_min.x, aabb_max.yz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.x, aabb_max.y, aabb_min.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_max.xy, aabb_min.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp* vec4(aabb_max.xyz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.x, aabb_max.yz, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    /* Lines connecting bottom & top planes */
    "    gl_Position = mvp * vec4(aabb_min.xy, aabb_max.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.x, aabb_max.yz, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    "\n"
    "    gl_Position = mvp * vec4(aabb_min.xyz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_min.x, aabb_max.y, aabb_min.z, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    "\n"
    "    gl_Position = mvp * vec4(aabb_max.x, aabb_min.yz, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_max.xy, aabb_min.z, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    "\n"
    "    gl_Position = mvp * vec4(aabb_max.x, aabb_min.y, aabb_max.z, 1.0);\n"
    "    EmitVertex();\n"
    "    gl_Position = mvp * vec4(aabb_max.xyz, 1.0);\n"
    "    EmitVertex();\n"
    "    EndPrimitive();\n"
    "}\n";
static const char* preview_vertex_shader =
    "#version 430 core\n"
    "\n"
    "layout(location = 0) out uint vertex_id;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vertex_id = gl_VertexID;\n"
    "}\n";

/** TODO */
typedef struct _scene_renderer_bbox_preview
{
    /* DO NOT release. */
    ral_context context;

    ral_texture_view         active_color_rendertarget;
    ral_command_buffer       active_command_buffer;
    ral_texture_view         active_depth_rendertarget;
    uint32_t                 data_n_meshes;
    ral_gfx_state            gfx_state;
    scene                    owned_scene;
    scene_renderer           owner;
    ral_program              preview_program;
    ral_program_block_buffer preview_program_data_ub;
    uint32_t                 preview_program_ub_offset_model;
    uint32_t                 preview_program_ub_offset_vp;
} _scene_renderer_bbox_preview;


/** TODO */
PRIVATE void _scene_renderer_bbox_preview_init_preview_program(_scene_renderer_bbox_preview* preview_ptr)
{
    const ral_program_variable* model_uniform_ral_ptr = nullptr;
    const ral_program_variable* vp_uniform_ral_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == nullptr,
                      "Preview program has already been initialized");

    /* Form a string from the number of meshes associated with the scene */
    std::stringstream n_scene_meshes_sstream;
    uint32_t          n_scene_meshes = 0;

    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_N_MESH_INSTANCES,
                      &n_scene_meshes);

    n_scene_meshes_sstream << n_scene_meshes;

    /* Form geometry shader body */
    std::string gs_body      = preview_geometry_shader;
    std::size_t token_offset = std::string::npos;

    while ((token_offset = gs_body.find("__value")) != std::string::npos)
    {
        gs_body = gs_body.replace(token_offset,
                                  strlen("__value"),
                                  n_scene_meshes_sstream.str().c_str() );
    }

    /* Create shaders and set their bodies */
    ral_shader                fs         = nullptr;
    ral_shader                gs         = nullptr;
    system_hashed_ansi_string scene_name = nullptr;
    ral_shader                vs         = nullptr;

    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);


    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview FS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info gs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview GS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_GEOMETRY
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview VS shader ",
                                                                system_hashed_ansi_string_get_buffer(scene_name)),
        RAL_SHADER_TYPE_VERTEX
    };


    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview program ",
                                                                system_hashed_ansi_string_get_buffer(scene_name))
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        gs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items];


    if (!ral_context_create_programs(preview_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &preview_ptr->preview_program))
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    if (!ral_context_create_shaders(preview_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed");
    }


    fs = result_shaders[0];
    gs = result_shaders[1];
    vs = result_shaders[2];


    const system_hashed_ansi_string fs_body_has = system_hashed_ansi_string_create(preview_fragment_shader);
    const system_hashed_ansi_string gs_body_has = system_hashed_ansi_string_create(gs_body.c_str() );
    const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(preview_vertex_shader);

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
    }

    /* Retrieve uniform block manager */
    preview_ptr->preview_program_data_ub = ral_program_block_buffer_create(preview_ptr->context,
                                                                           preview_ptr->preview_program,
                                                                           system_hashed_ansi_string_create("data") );

    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("model"),
                                          &model_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(preview_ptr->preview_program,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("vp"),
                                          &vp_uniform_ral_ptr);

    ASSERT_DEBUG_SYNC(model_uniform_ral_ptr != nullptr,
                      "Model uniform descriptor is nullptr");
    ASSERT_DEBUG_SYNC(vp_uniform_ral_ptr != nullptr,
                      "Vp uniform descriptor is nullptr");

    ASSERT_DEBUG_SYNC(model_uniform_ral_ptr->block_offset != -1,
                      "Model matrix UB offset is -1");
    ASSERT_DEBUG_SYNC(vp_uniform_ral_ptr->block_offset != -1,
                      "View matrix UB offset is -1");

    preview_ptr->preview_program_ub_offset_model = model_uniform_ral_ptr->block_offset;
    preview_ptr->preview_program_ub_offset_vp    = vp_uniform_ral_ptr->block_offset;

    /* All done */
    goto end;

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
                               reinterpret_cast<void* const*>(shaders_to_release) );
}

/** TODO */
PRIVATE void _scene_renderer_bbox_preview_init_ub_data(_scene_renderer_bbox_preview* preview_ptr)
{
    ral_backend_type backend_type                    = RAL_BACKEND_TYPE_UNKNOWN;
    uint32_t         uniform_buffer_offset_alignment = -1;

    /* Fill the buffer with data */
    for (unsigned int n_iteration = 0;
                      n_iteration < 2;
                    ++n_iteration)
    {
        for (unsigned int n_mesh = 0;
                          n_mesh < preview_ptr->data_n_meshes;
                        ++n_mesh)
        {
            float*     aabb_max_ptr              = nullptr;
            float*     aabb_min_ptr              = nullptr;
            uint32_t   mesh_id                   = -1;
            scene_mesh mesh_instance             = scene_get_mesh_instance_by_index(preview_ptr->owned_scene,
                                                                                    n_mesh);
            mesh       mesh_current              = nullptr;
            mesh_type  mesh_instance_type;
            mesh       mesh_instantiation_parent = nullptr;

            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_ID,
                                   &mesh_id);
            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_MESH,
                                   &mesh_current);

            mesh_get_property(mesh_current,
                              MESH_PROPERTY_TYPE,
                             &mesh_instance_type);

            if (mesh_instance_type == MESH_TYPE_REGULAR)
            {
                mesh_get_property(mesh_current,
                                  MESH_PROPERTY_INSTANTIATION_PARENT,
                                 &mesh_instantiation_parent);

                if (mesh_instantiation_parent != nullptr)
                {
                    mesh_current = mesh_instantiation_parent;
                }
            }
            else
            {
                mesh_instantiation_parent = mesh_current;
            }

            mesh_get_property(mesh_current,
                              MESH_PROPERTY_MODEL_AABB_MAX,
                              &aabb_max_ptr);
            mesh_get_property(mesh_current,
                              MESH_PROPERTY_MODEL_AABB_MIN,
                              &aabb_min_ptr);

            ASSERT_DEBUG_SYNC(aabb_max_ptr[0] != aabb_min_ptr[0] &&
                              aabb_max_ptr[1] != aabb_min_ptr[1] &&
                              aabb_max_ptr[2] != aabb_min_ptr[2],
                              "Invalid AABB");

            if (n_iteration == 0)
            {
                ral_program_block_buffer_set_arrayed_variable_value(preview_ptr->preview_program_data_ub,
                                                                    0, /* max AABB data offset */
                                                                    aabb_max_ptr,
                                                                    sizeof(float) * 4, /* src_data_size         */
                                                                    n_mesh,            /* dst_array_start_index */
                                                                    1);
            }
            else
            {
                ral_program_block_buffer_set_arrayed_variable_value(preview_ptr->preview_program_data_ub,
                                                                    preview_ptr->data_n_meshes * sizeof(float) * 4, /* min AABB data offset */
                                                                    aabb_min_ptr,
                                                                    sizeof(float) * 4, /* src_data_size         */
                                                                    n_mesh,            /* dst_array_start_index */
                                                                    1);
            }
        }
    }

    ral_program_block_buffer_sync_immediately(preview_ptr->preview_program_data_ub);
}

/** Please see header for spec */
PUBLIC void scene_renderer_bbox_preview_append_mesh(scene_renderer_bbox_preview preview,
                                                    uint32_t                    mesh_id)
{
    system_matrix4x4              model       = nullptr;
    _scene_renderer_bbox_preview* preview_ptr = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);

    ASSERT_DEBUG_SYNC(preview_ptr->active_command_buffer != nullptr,
                      "No rendering in progress");

    scene_renderer_get_indexed_property(preview_ptr->owner,
                                        SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,
                                        mesh_id,
                                       &model);

    /* NOTE: model may be null at this point if the item was culled out. */
    if (model != nullptr)
    {
        ral_buffer                                        data_bo_ral     = nullptr;
        ral_command_buffer_draw_call_regular_command_info draw_call_info;

        ral_program_block_buffer_get_property(preview_ptr->preview_program_data_ub,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &data_bo_ral);

        draw_call_info.base_instance = 0;
        draw_call_info.base_vertex   = mesh_id;
        draw_call_info.n_instances   = 1;
        draw_call_info.n_vertices    = 1;

        ral_command_buffer_record_update_buffer(preview_ptr->active_command_buffer,
                                                data_bo_ral,
                                                preview_ptr->preview_program_ub_offset_model,
                                                sizeof(float) * 16,
                                                system_matrix4x4_get_row_major_data(model));

        ral_command_buffer_record_draw_call_regular(preview_ptr->active_command_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call_info);
    }
}

/** Please see header for spec */
PUBLIC scene_renderer_bbox_preview scene_renderer_bbox_preview_create(ral_context    context,
                                                                      scene          scene,
                                                                      scene_renderer owner)
{
    _scene_renderer_bbox_preview* new_instance_ptr = new (std::nothrow) _scene_renderer_bbox_preview;

    ASSERT_ALWAYS_SYNC(new_instance_ptr != nullptr,
                       "Out of memory");

    if (new_instance_ptr != nullptr)
    {
        new_instance_ptr->active_color_rendertarget       = nullptr;
        new_instance_ptr->active_command_buffer           = nullptr;
        new_instance_ptr->active_depth_rendertarget       = nullptr;
        new_instance_ptr->context                         = context;
        new_instance_ptr->data_n_meshes                   = 0;
        new_instance_ptr->gfx_state                       = nullptr;
        new_instance_ptr->owned_scene                     = scene;
        new_instance_ptr->owner                           = owner;
        new_instance_ptr->preview_program                 = nullptr;
        new_instance_ptr->preview_program_data_ub         = nullptr;
        new_instance_ptr->preview_program_ub_offset_model = -1;
        new_instance_ptr->preview_program_ub_offset_vp    = -1;

        /* Set up the gfx state instance */
        ral_gfx_state_create_info gfx_state_create_info;

        gfx_state_create_info.depth_test     = true;
        gfx_state_create_info.depth_writes   = true;
        gfx_state_create_info.primitive_type = RAL_PRIMITIVE_TYPE_POINTS;

        ral_context_create_gfx_states(context,
                                      1, /* n_create_info_items */
                                      &gfx_state_create_info,
                                      &new_instance_ptr->gfx_state);

        /* Initialize the program object */
        _scene_renderer_bbox_preview_init_preview_program(new_instance_ptr);

        ASSERT_DEBUG_SYNC(new_instance_ptr->preview_program != nullptr,
                          "Could not initialize preview program");

        /* Initialize a BO store */
        scene_get_property(scene,
                           SCENE_PROPERTY_N_MESH_INSTANCES,
                          &new_instance_ptr->data_n_meshes);

        _scene_renderer_bbox_preview_init_ub_data(new_instance_ptr);

        /* Wrap up */
        scene_retain(scene);
    }

    return (scene_renderer_bbox_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_bbox_preview_release(scene_renderer_bbox_preview preview)
{
    _scene_renderer_bbox_preview* preview_ptr = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);

    ASSERT_DEBUG_SYNC(preview_ptr->active_command_buffer     == nullptr &&
                      preview_ptr->active_color_rendertarget == nullptr &&
                      preview_ptr->active_depth_rendertarget == nullptr,
                      "Rendering needs to be stopped at scene_renderer_bbox_preview_release() call time.");

    if (preview_ptr->gfx_state != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->gfx_state) );

        preview_ptr->gfx_state = nullptr;
    }

    if (preview_ptr->preview_program_data_ub != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_data_ub);

        preview_ptr->preview_program_data_ub = nullptr;
    }

    if (preview_ptr->preview_program != nullptr)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&preview_ptr->preview_program) );

        preview_ptr->preview_program = nullptr;
    }

    if (preview_ptr->preview_program_data_ub != nullptr)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_data_ub);

        preview_ptr->preview_program_data_ub = nullptr;
    }

    if (preview_ptr->owned_scene != nullptr)
    {
        scene_release(preview_ptr->owned_scene);
    }

    delete preview_ptr;
    preview_ptr = nullptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_bbox_preview_start(scene_renderer_bbox_preview preview,
                                              ral_texture_view            color_rendertarget,
                                              ral_texture_view            depth_rendertarget,
                                              system_matrix4x4            vp)
{
    ral_buffer                    data_bo_ral          = nullptr;
    uint32_t                      data_bo_size         =  0;
    uint32_t                      data_bo_start_offset = -1;
    _scene_renderer_bbox_preview* preview_ptr          = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);
    ral_texture_view              rt;
    uint32_t                      rt_size[2];

    ASSERT_DEBUG_SYNC(color_rendertarget != nullptr &&
                      depth_rendertarget != nullptr,
                      "Null rendertarget was specified");

    ral_program_block_buffer_get_property(preview_ptr->preview_program_data_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &data_bo_ral);

    ral_buffer_get_property(data_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &data_bo_size);

    if (color_rendertarget != nullptr)
    {
        rt = color_rendertarget;
    }
    else
    if (depth_rendertarget != nullptr)
    {
        rt = depth_rendertarget;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Both color and depth render-targets are null");
    }

    ral_texture_view_get_mipmap_property(rt,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         rt_size + 0);
    ral_texture_view_get_mipmap_property(rt,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                         rt_size + 1);

    /* Start recording the result command buffer */
    ral_command_buffer_set_binding_command_info            bindings[2];
    ral_command_buffer_create_info                         cmd_buffer_create_info;
    ral_command_buffer_set_color_rendertarget_command_info color_rt_info           = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
    ral_command_buffer_set_scissor_box_command_info        scissor_box;
    ral_command_buffer_set_viewport_command_info           viewport;

    cmd_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
    cmd_buffer_create_info.is_executable                           = true;
    cmd_buffer_create_info.is_invokable_from_other_command_buffers = false;
    cmd_buffer_create_info.is_resettable                           = false;
    cmd_buffer_create_info.is_transient                            = true;

    bindings[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
    bindings[0].name                          = system_hashed_ansi_string_create("data");
    bindings[0].uniform_buffer_binding.buffer = data_bo_ral;
    bindings[0].uniform_buffer_binding.offset = 0;
    bindings[0].uniform_buffer_binding.size   = data_bo_size;

    bindings[1].binding_type                  = RAL_BINDING_TYPE_RENDERTARGET;
    bindings[1].name                          = system_hashed_ansi_string_create("result");
    bindings[1].rendertarget_binding.rt_index = 0;

    color_rt_info.rendertarget_index = 0;
    color_rt_info.texture_view       = color_rendertarget;

    scissor_box.index   = 0;
    scissor_box.size[0] = rt_size[0];
    scissor_box.size[1] = rt_size[1];
    scissor_box.xy  [0] = 0;
    scissor_box.xy  [1] = 0;

    viewport.depth_range[0] = 0.0f;
    viewport.depth_range[1] = 1.0f;
    viewport.index          = 0;
    viewport.size[0]        = static_cast<float>(rt_size[0]);
    viewport.size[1]        = static_cast<float>(rt_size[1]);
    viewport.xy  [0]        = 0.0f;
    viewport.xy  [1]        = 0.0f;

    ral_context_create_command_buffers(preview_ptr->context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &preview_ptr->active_command_buffer);

    ral_command_buffer_start_recording               (preview_ptr->active_command_buffer);
    ral_command_buffer_record_set_program            (preview_ptr->active_command_buffer,
                                                      preview_ptr->preview_program);
    ral_command_buffer_record_set_bindings           (preview_ptr->active_command_buffer,
                                                      sizeof(bindings) / sizeof(bindings[0]),
                                                      bindings);
    ral_command_buffer_record_set_color_rendertargets(preview_ptr->active_command_buffer,
                                                      1, /* n_rendertargets */
                                                     &color_rt_info);
    ral_command_buffer_record_set_depth_rendertarget (preview_ptr->active_command_buffer,
                                                      depth_rendertarget);
    ral_command_buffer_record_set_gfx_state          (preview_ptr->active_command_buffer,
                                                      preview_ptr->gfx_state);
    ral_command_buffer_record_set_scissor_boxes      (preview_ptr->active_command_buffer,
                                                      1, /* n_scissor_boxes */
                                                     &scissor_box);
    ral_command_buffer_record_set_viewports          (preview_ptr->active_command_buffer,
                                                      1, /* n_viewports */
                                                     &viewport);

    ral_command_buffer_record_update_buffer(preview_ptr->active_command_buffer,
                                            data_bo_ral,
                                            preview_ptr->preview_program_ub_offset_vp,
                                            sizeof(float) * 16,
                                            system_matrix4x4_get_row_major_data(vp));

    preview_ptr->active_color_rendertarget = color_rendertarget;
    preview_ptr->active_depth_rendertarget = depth_rendertarget;
}

/** Please see header for spec */
PUBLIC ral_present_task scene_renderer_bbox_preview_stop(scene_renderer_bbox_preview preview)
{
    _scene_renderer_bbox_preview* preview_ptr = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);
    ral_present_task              result      = nullptr;

    ASSERT_DEBUG_SYNC(preview_ptr->active_command_buffer != nullptr,
                      "No ongoing rendering process");

    ral_command_buffer_stop_recording(preview_ptr->active_command_buffer);

    /* Stop recording and form a GPU present task */
    ral_present_task_gpu_create_info task_create_info;
    ral_present_task_io              task_unique_ios[2];

    task_unique_ios[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    task_unique_ios[0].texture_view = preview_ptr->active_color_rendertarget;
    task_unique_ios[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    task_unique_ios[1].texture_view = preview_ptr->active_depth_rendertarget;

    task_create_info.command_buffer   = preview_ptr->active_command_buffer;
    task_create_info.n_unique_inputs  = sizeof(task_unique_ios) / sizeof(task_unique_ios[0]);
    task_create_info.n_unique_outputs = sizeof(task_unique_ios) / sizeof(task_unique_ios[0]);
    task_create_info.unique_inputs    = task_unique_ios;
    task_create_info.unique_outputs   = task_unique_ios;

    result = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scene renderre (BBox preview): Rasterization"),
                                        &task_create_info);

    /* Clean up */
    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&preview_ptr->active_command_buffer) );

    preview_ptr->active_color_rendertarget = nullptr;
    preview_ptr->active_command_buffer     = nullptr;
    preview_ptr->active_depth_rendertarget = nullptr;

    /* All done */
    return result;
}


