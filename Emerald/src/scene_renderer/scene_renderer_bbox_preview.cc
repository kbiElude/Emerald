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
    "in uint vertex_id[];\n"
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
    "out uint vertex_id;\n"
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

    ral_command_buffer       active_command_buffer;
    ral_texture_view         active_rendertarget;
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
    float*           traveller_ptr                   = nullptr;
    float*           ub_data                         = nullptr;
    uint32_t         uniform_buffer_offset_alignment = -1;

    /* Allocate space for AABB data. */
    const uint32_t min_bbox_ub_offset = 4 /* vec4 */ *                    preview_ptr->data_n_meshes * sizeof(float);
    const uint32_t matrix_data_size   = 4 /* vec4 */ * 2 /* max, min */ * preview_ptr->data_n_meshes * sizeof(float);

    ub_data = new (std::nothrow) float[matrix_data_size / sizeof(float)];

    ASSERT_ALWAYS_SYNC(ub_data != nullptr,
                       "Out of memory");

    if (ub_data == nullptr)
    {
        goto end;
    }

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
                traveller_ptr = ub_data + 4 /* vec4 */ * mesh_id;

                memcpy(traveller_ptr,
                       aabb_max_ptr,
                       sizeof(float) * 3);
            }
            else
            {
                traveller_ptr = ub_data + 4 /* vec4 */ * (preview_ptr->data_n_meshes + mesh_id);

                memcpy(traveller_ptr,
                       aabb_min_ptr,
                       sizeof(float) * 3);
            }
        }
    }

    /* Retrieve UB offset alignment */
    ral_context_get_property(preview_ptr->context,
                             RAL_CONTEXT_PROPERTY_UNIFORM_BUFFER_ALIGNMENT,
                            &uniform_buffer_offset_alignment);

    /* Initialize UBO storage. */
    uint32_t data_bo_size = 0;

    ral_program_block_buffer_set_arrayed_variable_value(preview_ptr->preview_program_data_ub,
                                                        0, /* max AABB data offset */
                                                        ub_data,
                                                        matrix_data_size / 2, /* src_data_size         */
                                                        0,                    /* dst_array_start_index */
                                                        preview_ptr->data_n_meshes);
    ral_program_block_buffer_set_arrayed_variable_value(preview_ptr->preview_program_data_ub,
                                                        min_bbox_ub_offset,
                                                        ub_data,
                                                        matrix_data_size / 2, /* src_data_size         */
                                                        0,                    /* dst_array_start_index */
                                                        preview_ptr->data_n_meshes);

    ral_program_block_buffer_sync_immediately(preview_ptr->preview_program_data_ub);
end:
    if (ub_data != nullptr)
    {
        delete [] ub_data;

        ub_data = nullptr;
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
        new_instance_ptr->active_command_buffer           = nullptr;
        new_instance_ptr->active_rendertarget             = nullptr;
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
        if (new_instance_ptr->preview_program_data_ub == nullptr)
        {
            _scene_renderer_bbox_preview_init_ub_data(new_instance_ptr);
        }

        /* Wrap up */
        scene_retain(scene);

        scene_get_property(scene,
                           SCENE_PROPERTY_N_MESH_INSTANCES,
                          &new_instance_ptr->data_n_meshes);
    }

    return (scene_renderer_bbox_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_bbox_preview_release(scene_renderer_bbox_preview preview)
{
    _scene_renderer_bbox_preview* preview_ptr = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);

    ASSERT_DEBUG_SYNC(preview_ptr->active_command_buffer == nullptr &&
                      preview_ptr->active_rendertarget   == nullptr,
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
PUBLIC void scene_renderer_bbox_preview_render(scene_renderer_bbox_preview preview,
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
PUBLIC void scene_renderer_bbox_preview_start(scene_renderer_bbox_preview preview,
                                              ral_texture_view            rendertarget,
                                              system_matrix4x4            vp)
{
    ral_buffer                    data_bo_ral          = nullptr;
    uint32_t                      data_bo_size         =  0;
    uint32_t                      data_bo_start_offset = -1;
    _scene_renderer_bbox_preview* preview_ptr          = reinterpret_cast<_scene_renderer_bbox_preview*>(preview);

    ASSERT_DEBUG_SYNC(rendertarget != nullptr,
                      "Null rendertarget was specified");

    ral_program_block_buffer_get_property(preview_ptr->preview_program_data_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &data_bo_ral);

    ral_buffer_get_property(data_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &data_bo_size);

    /* Start recording the result command buffer */
    ral_command_buffer_set_binding_command_info            bindings[2];
    ral_command_buffer_create_info                         cmd_buffer_create_info;
    ral_command_buffer_set_color_rendertarget_command_info rt_info                = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();

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

    rt_info.rendertarget_index = 0;
    rt_info.texture_view       = rendertarget;

    ral_context_create_command_buffers(preview_ptr->context,
                                       1, /* n_command_buffers */
                                      &cmd_buffer_create_info,
                                      &preview_ptr->active_command_buffer);

    ral_command_buffer_start_recording               (preview_ptr->active_command_buffer);
    ral_command_buffer_record_set_bindings           (preview_ptr->active_command_buffer,
                                                      sizeof(bindings) / sizeof(bindings[0]),
                                                      bindings);
    ral_command_buffer_record_set_color_rendertargets(preview_ptr->active_command_buffer,
                                                      1, /* n_rendertargets */
                                                     &rt_info);
    ral_command_buffer_record_set_program            (preview_ptr->active_command_buffer,
                                                      preview_ptr->preview_program);

    ral_command_buffer_record_update_buffer(preview_ptr->active_command_buffer,
                                            data_bo_ral,
                                            preview_ptr->preview_program_ub_offset_vp,
                                            sizeof(float) * 16,
                                            system_matrix4x4_get_row_major_data(vp));

    preview_ptr->active_rendertarget = rendertarget;
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
    ral_present_task_io              task_unique_output;

    task_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    task_unique_output.texture_view = preview_ptr->active_rendertarget;

    task_create_info.command_buffer   = preview_ptr->active_command_buffer;
    task_create_info.n_unique_inputs  = 0;
    task_create_info.n_unique_outputs = 1;
    task_create_info.unique_inputs    = nullptr;
    task_create_info.unique_outputs   = &task_unique_output;

    result = ral_present_task_create_gpu(system_hashed_ansi_string_create("Scene renderre (BBox preview): Rasterization"),
                                        &task_create_info);

    /* Clean up */
    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&preview_ptr->active_command_buffer) );
    ral_context_delete_objects(preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&preview_ptr->active_rendertarget) );

    preview_ptr->active_command_buffer = nullptr;
    preview_ptr->active_rendertarget   = nullptr;

    /* All done */
    return result;
}


