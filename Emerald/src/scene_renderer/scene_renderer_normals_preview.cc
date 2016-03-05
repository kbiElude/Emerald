/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "scene_renderer/scene_renderer.h"
#include "scene_renderer/scene_renderer_normals_preview.h"
#include "system/system_matrix4x4.h"
#include <string.h>

static const char* preview_fragment_shader = "#version 430 core\n"
                                             "\n"
                                             "out vec4 result;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    result = vec4(0, 1, 0, 1);\n"
                                             "}\n";
static const char* preview_geometry_shader = "#version 430 core\n"
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
static const char* preview_vertex_shader   = "#version 430 core\n"
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

    scene                    owned_scene;
    scene_renderer           owner;
    ral_program              preview_program;
    GLint                    preview_program_normal_matrix_ub_offset;
    GLint                    preview_program_start_offsets_ub_offset;
    GLint                    preview_program_stride_ub_offset;
    ral_program_block_buffer preview_program_ub_gs;
    GLuint                   preview_program_ub_gs_ub_bp;
    ral_program_block_buffer preview_program_ub_vs;
    GLuint                   preview_program_ub_vs_ub_bp;
    GLint                    preview_program_vp_ub_offset;
} _scene_renderer_normals_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _scene_renderer_normals_preview_verify_context_type(ogl_context);
#else
    #define _scene_renderer_normals_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _scene_renderer_normals_preview_init_preview_program(_scene_renderer_normals_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    const ogl_context_gl_entrypoints* entrypoints_ptr               = NULL;
    const ral_program_variable*       normal_matrix_uniform_ral_ptr = NULL;
    const ral_program_variable*       start_offsets_uniform_ral_ptr = NULL;
    const ral_program_variable*       stride_uniform_ral_ptr        = NULL;
    const ral_program_variable*       vp_uniform_ral_ptr            = NULL;

    /* Create shaders and set their bodies */
    ral_shader                fs         = NULL;
    ral_shader                gs         = NULL;
    system_hashed_ansi_string scene_name = NULL;
    ral_shader                vs         = NULL;

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
                                   fs) ||
        !ral_program_attach_shader(preview_ptr->preview_program,
                                   gs) ||
        !ral_program_attach_shader(preview_ptr->preview_program,
                                   vs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");

        goto end_fail;
    }

    /* Retrieve uniform locations */
    const raGL_program preview_program_raGL = ral_context_get_program_gl(preview_ptr->context,
                                                                         preview_ptr->preview_program);

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

    ASSERT_DEBUG_SYNC(normal_matrix_uniform_ral_ptr != NULL,
                      "Normal matrix uniform not recognized");
    ASSERT_DEBUG_SYNC(start_offsets_uniform_ral_ptr != NULL,
                      "Start offsets uniform not recognized");
    ASSERT_DEBUG_SYNC(stride_uniform_ral_ptr != NULL,
                      "Stride uniform not recognized");
    ASSERT_DEBUG_SYNC(vp_uniform_ral_ptr != NULL,
                      "VP uniform not recognized");

    if (normal_matrix_uniform_ral_ptr != NULL)
    {
        preview_ptr->preview_program_normal_matrix_ub_offset = normal_matrix_uniform_ral_ptr->block_offset;
    }

    if (start_offsets_uniform_ral_ptr != NULL)
    {
        preview_ptr->preview_program_start_offsets_ub_offset = start_offsets_uniform_ral_ptr->block_offset;
    }

    if (stride_uniform_ral_ptr != NULL)
    {
        preview_ptr->preview_program_stride_ub_offset = stride_uniform_ral_ptr->block_offset;
    }

    if (vp_uniform_ral_ptr != NULL)
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
    preview_ptr->preview_program_ub_vs = NULL;

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub_gs != NULL &&
                      preview_ptr->preview_program_ub_vs != NULL,
                      "Could not create UB block buffers");

    raGL_program_get_block_property_by_name(preview_program_raGL,
                                            system_hashed_ansi_string_create("dataGS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &preview_ptr->preview_program_ub_gs_ub_bp);
    raGL_program_get_block_property_by_name(preview_program_raGL,
                                            system_hashed_ansi_string_create("dataGV"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &preview_ptr->preview_program_ub_vs_ub_bp);

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != NULL)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
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

    ASSERT_ALWAYS_SYNC(new_instance_ptr != NULL,
                       "Out of memory");

    if (new_instance_ptr != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance_ptr->context                                 = context;
        new_instance_ptr->owned_scene                             = scene;
        new_instance_ptr->owner                                   = owner;
        new_instance_ptr->preview_program                         = NULL;
        new_instance_ptr->preview_program_normal_matrix_ub_offset = -1;
        new_instance_ptr->preview_program_start_offsets_ub_offset = -1;
        new_instance_ptr->preview_program_stride_ub_offset        = -1;
        new_instance_ptr->preview_program_vp_ub_offset            = -1;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (scene_renderer_normals_preview) new_instance_ptr;
}

/** Please see header for spec */
PUBLIC void scene_renderer_normals_preview_release(scene_renderer_normals_preview preview)
{
    _scene_renderer_normals_preview* preview_ptr = (_scene_renderer_normals_preview*) preview;

    if (preview_ptr->owned_scene != NULL)
    {
        scene_release(preview_ptr->owned_scene);
    }

    if (preview_ptr->preview_program != NULL)
    {
        ral_context_delete_objects(preview_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   (const void**) &preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

    if (preview_ptr->preview_program_ub_gs != NULL)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_ub_gs);

        preview_ptr->preview_program_ub_gs = NULL;
    }

    if (preview_ptr->preview_program_ub_vs != NULL)
    {
        ral_program_block_buffer_release(preview_ptr->preview_program_ub_vs);

        preview_ptr->preview_program_ub_vs = NULL;
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void scene_renderer_normals_preview_render(scene_renderer_normals_preview preview,
                                                                         uint32_t                       mesh_id)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr          = NULL;
    ral_buffer                        mesh_bo                  = NULL;
    GLuint                            mesh_bo_id               = 0;
    raGL_buffer                       mesh_bo_raGL             = NULL;
    unsigned int                      mesh_bo_size             = 0;
    uint32_t                          mesh_bo_start_offset     = -1;
    mesh                              mesh_instance            = NULL;
    mesh_type                         mesh_instance_type;
    uint32_t                          mesh_start_offset_normal = -1;
    uint32_t                          mesh_start_offset_vertex = -1;
    uint32_t                          mesh_stride              = -1;
    uint32_t                          mesh_total_elements      = 0;
    system_matrix4x4                  normal_matrix            = NULL;
    _scene_renderer_normals_preview*  preview_ptr              = (_scene_renderer_normals_preview*) preview;

    /* Retrieve mesh properties */
    ogl_context_get_property           (ral_context_get_gl_context(preview_ptr->context),
                                        OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                       &entrypoints_ptr);
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
                      MESH_PROPERTY_GL_BO_RAL,
                     &mesh_bo);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,
                     &mesh_bo_size);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_STRIDE,
                     &mesh_stride);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_TOTAL_ELEMENTS,
                     &mesh_total_elements);

    mesh_bo_raGL = ral_context_get_buffer_gl(preview_ptr->context,
                                             mesh_bo);

    raGL_buffer_get_property(mesh_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &mesh_bo_id);
    raGL_buffer_get_property(mesh_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &mesh_bo_start_offset);

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
        mesh_start_offset_normal/* - mesh_bo_start_offset */,
        mesh_start_offset_vertex/* - mesh_bo_start_offset */
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

    ral_program_block_buffer_sync(preview_ptr->preview_program_ub_gs);
    ral_program_block_buffer_sync(preview_ptr->preview_program_ub_vs);

    /* Set up shader storage buffer binding */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        mesh_bo_id,
                                        mesh_bo_start_offset,
                                        mesh_bo_size);

    /* Draw */
    entrypoints_ptr->pGLDrawArrays(GL_POINTS,
                                   0,                    /* first */
                                   mesh_total_elements); /* count */
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void scene_renderer_normals_preview_start(scene_renderer_normals_preview preview,
                                                                        system_matrix4x4               vp)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    _scene_renderer_normals_preview*                          preview_ptr         = (_scene_renderer_normals_preview*) preview;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);

    /* Initialize the program object if one has not been initialized yet */
    if (preview_ptr->preview_program == NULL)
    {
        _scene_renderer_normals_preview_init_preview_program(preview_ptr);

        ASSERT_DEBUG_SYNC(preview_ptr->preview_program != NULL,
                          "Could not initialize preview program");
    }

    /* Set up uniforms that will be shared across subsequent draw calls */
    raGL_program program_raGL    = ral_context_get_program_gl(preview_ptr->context,
                                                              preview_ptr->preview_program);
    GLuint       program_raGL_id = 0;
    GLuint       vao_id          = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);
    ogl_context_get_property (ral_context_get_gl_context(preview_ptr->context),
                              OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                             &vao_id);

    entrypoints_ptr->pGLUseProgram                        (program_raGL_id);
    ral_program_block_buffer_set_nonarrayed_variable_value(preview_ptr->preview_program_ub_gs,
                                                           preview_ptr->preview_program_vp_ub_offset,
                                                           system_matrix4x4_get_column_major_data(vp),
                                                           sizeof(float) * 16);

    GLuint      preview_program_ub_gs_bo_id           = 0;
    raGL_buffer preview_program_ub_gs_bo_raGL         = NULL;
    ral_buffer  preview_program_ub_gs_bo_ral          = NULL;
    uint32_t    preview_program_ub_gs_bo_size         = -1;
    uint32_t    preview_program_ub_gs_bo_start_offset = -1;
    GLuint      preview_program_ub_vs_bo_id           = 0;
    raGL_buffer preview_program_ub_vs_bo_raGL         = NULL;
    ral_buffer  preview_program_ub_vs_bo_ral          = NULL;
    uint32_t    preview_program_ub_vs_bo_size         = -1;
    uint32_t    preview_program_ub_vs_bo_start_offset = -1;

    ral_program_block_buffer_get_property(preview_ptr->preview_program_ub_gs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &preview_program_ub_gs_bo_ral);
    ral_program_block_buffer_get_property(preview_ptr->preview_program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &preview_program_ub_vs_bo_ral);

    preview_program_ub_gs_bo_raGL = ral_context_get_buffer_gl(preview_ptr->context,
                                                              preview_program_ub_gs_bo_ral);
    preview_program_ub_vs_bo_raGL = ral_context_get_buffer_gl(preview_ptr->context,
                                                              preview_program_ub_vs_bo_ral);

    raGL_buffer_get_property(preview_program_ub_gs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_gs_bo_id);
    raGL_buffer_get_property(preview_program_ub_gs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_program_ub_gs_bo_start_offset);
    raGL_buffer_get_property(preview_program_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_vs_bo_id);
    raGL_buffer_get_property(preview_program_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_program_ub_vs_bo_start_offset);

    ral_buffer_get_property(preview_program_ub_gs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &preview_program_ub_gs_bo_size);
    ral_buffer_get_property(preview_program_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &preview_program_ub_vs_bo_size);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        preview_ptr->preview_program_ub_gs_ub_bp,
                                        preview_program_ub_gs_bo_id,
                                        preview_program_ub_gs_bo_start_offset,
                                        preview_program_ub_gs_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        preview_ptr->preview_program_ub_vs_ub_bp,
                                        preview_program_ub_vs_bo_id,
                                        preview_program_ub_vs_bo_start_offset,
                                        preview_program_ub_vs_bo_size);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void scene_renderer_normals_preview_stop(scene_renderer_normals_preview preview)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _scene_renderer_normals_preview*  preview_ptr     = (_scene_renderer_normals_preview*) preview;

    ogl_context_get_property(ral_context_get_gl_context(preview_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLBindVertexArray(0);
}


