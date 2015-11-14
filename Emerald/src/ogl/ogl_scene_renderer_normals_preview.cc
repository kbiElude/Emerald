/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_normals_preview.h"
#include "ogl/ogl_shader.h"
#include "raGL/raGL_buffer.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
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
typedef struct _ogl_scene_renderer_normals_preview
{
    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    scene              owned_scene;
    ogl_scene_renderer owner;
    ogl_program        preview_program;
    GLint              preview_program_normal_matrix_ub_offset;
    GLint              preview_program_start_offsets_ub_offset;
    GLint              preview_program_stride_ub_offset;
    ogl_program_ub     preview_program_ub_gs;
    raGL_buffer        preview_program_ub_gs_bo;
    unsigned int       preview_program_ub_gs_bo_size;
    GLuint             preview_program_ub_gs_ub_bp;
    ogl_program_ub     preview_program_ub_vs;
    raGL_buffer        preview_program_ub_vs_bo;
    unsigned int       preview_program_ub_vs_bo_size;
    GLuint             preview_program_ub_vs_ub_bp;
    GLint              preview_program_vp_ub_offset;
} _ogl_scene_renderer_normals_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_scene_renderer_normals_preview_verify_context_type(ogl_context);
#else
    #define _ogl_scene_renderer_normals_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_context_scene_renderer_normals_preview_init_preview_program(_ogl_scene_renderer_normals_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    const ogl_context_gl_entrypoints* entrypoints_ptr                      = NULL;
    const ogl_program_variable*       normal_matrix_uniform_descriptor_ptr = NULL;
    const ogl_program_variable*       start_offsets_uniform_descriptor_ptr = NULL;
    const ogl_program_variable*       stride_uniform_descriptor_ptr        = NULL;
    const ogl_program_variable*       vp_uniform_descriptor_ptr            = NULL;

    /* Create shaders and set their bodies */
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(preview_ptr->owned_scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    ogl_shader fs_shader = ogl_shader_create(preview_ptr->context,
                                             RAL_SHADER_TYPE_FRAGMENT,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview FS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader gs_shader = ogl_shader_create(preview_ptr->context,
                                             RAL_SHADER_TYPE_GEOMETRY,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview GS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader vs_shader = ogl_shader_create(preview_ptr->context,
                                             RAL_SHADER_TYPE_VERTEX,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview VS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );

    ogl_shader_set_body(fs_shader,
                        system_hashed_ansi_string_create(preview_fragment_shader) );
    ogl_shader_set_body(gs_shader,
                        system_hashed_ansi_string_create(preview_geometry_shader) );
    ogl_shader_set_body(vs_shader,
                        system_hashed_ansi_string_create(preview_vertex_shader) );

    if (!ogl_shader_compile(fs_shader) ||
        !ogl_shader_compile(gs_shader) ||
        !ogl_shader_compile(vs_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to compile at least one of the shader used by normals preview renderer");

        goto end_fail;
    }

    /* Initialize the program object */
    preview_ptr->preview_program = ogl_program_create(preview_ptr->context,
                                                      system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview program ",
                                                                                                              system_hashed_ansi_string_get_buffer(scene_name)),
                                                      OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(preview_ptr->preview_program,
                              fs_shader);
    ogl_program_attach_shader(preview_ptr->preview_program,
                              gs_shader);
    ogl_program_attach_shader(preview_ptr->preview_program,
                              vs_shader);

    if (!ogl_program_link(preview_ptr->preview_program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to link the program object used by normals preview renderer");

        goto end_fail;
    }

    /* Retrieve uniform locations */
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("normal_matrix"),
                                   &normal_matrix_uniform_descriptor_ptr);
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("start_offsets"),
                                   &start_offsets_uniform_descriptor_ptr);
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("stride"),
                                   &stride_uniform_descriptor_ptr);
    ogl_program_get_uniform_by_name(preview_ptr->preview_program,
                                    system_hashed_ansi_string_create("vp"),
                                   &vp_uniform_descriptor_ptr);

    ASSERT_DEBUG_SYNC(normal_matrix_uniform_descriptor_ptr != NULL,
                      "Normal matrix uniform not recognized");
    ASSERT_DEBUG_SYNC(start_offsets_uniform_descriptor_ptr != NULL,
                      "Start offsets uniform not recognized");
    ASSERT_DEBUG_SYNC(stride_uniform_descriptor_ptr != NULL,
                      "Stride uniform not recognized");
    ASSERT_DEBUG_SYNC(vp_uniform_descriptor_ptr != NULL,
                      "VP uniform not recognized");

    if (normal_matrix_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_normal_matrix_ub_offset = normal_matrix_uniform_descriptor_ptr->block_offset;
    }

    if (start_offsets_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_start_offsets_ub_offset = start_offsets_uniform_descriptor_ptr->block_offset;
    }

    if (stride_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_stride_ub_offset = stride_uniform_descriptor_ptr->block_offset;
    }

    if (vp_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_vp_ub_offset = vp_uniform_descriptor_ptr->block_offset;
    }

    /* Retrieve UB properties */
    preview_ptr->preview_program_ub_gs = NULL;
    preview_ptr->preview_program_ub_vs = NULL;

    ogl_program_get_uniform_block_by_name(preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("dataGS"),
                                         &preview_ptr->preview_program_ub_gs);
    ogl_program_get_uniform_block_by_name(preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &preview_ptr->preview_program_ub_vs);

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_ub_gs != NULL &&
                      preview_ptr->preview_program_ub_vs != NULL,
                      "Could not retrieve UB descriptors");

    ogl_program_ub_get_property(preview_ptr->preview_program_ub_gs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->preview_program_ub_gs_bo_size);
    ogl_program_ub_get_property(preview_ptr->preview_program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->preview_program_ub_vs_bo_size);

    ogl_program_ub_get_property(preview_ptr->preview_program_ub_gs,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &preview_ptr->preview_program_ub_gs_bo);
    ogl_program_ub_get_property(preview_ptr->preview_program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BO,
                               &preview_ptr->preview_program_ub_vs_bo);

    ogl_program_ub_get_property(preview_ptr->preview_program_ub_gs,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &preview_ptr->preview_program_ub_gs_ub_bp);
    ogl_program_ub_get_property(preview_ptr->preview_program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &preview_ptr->preview_program_ub_vs_ub_bp);

    /* All done */
    goto end;

end_fail:
    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

end:
    if (fs_shader != NULL)
    {
        ogl_shader_release(fs_shader);
    }

    if (gs_shader != NULL)
    {
        ogl_shader_release(gs_shader);
    }

    if (vs_shader != NULL)
    {
        ogl_shader_release(vs_shader);
    }
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _ogl_scene_renderer_normals_preview_verify_context_type(ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "ogl_scene_renderer_normals_preview is only supported under GL contexts")
    }
#endif


/** Please see header for spec */
PUBLIC ogl_scene_renderer_normals_preview ogl_scene_renderer_normals_preview_create(ogl_context        context,
                                                                                    scene              scene,
                                                                                    ogl_scene_renderer owner)
{
    _ogl_scene_renderer_normals_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_normals_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL,
                       "Out of memory");

    if (new_instance != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance->context                                 = context;
        new_instance->owned_scene                             = scene;
        new_instance->owner                                   = owner;
        new_instance->preview_program                         = NULL;
        new_instance->preview_program_normal_matrix_ub_offset = -1;
        new_instance->preview_program_start_offsets_ub_offset = -1;
        new_instance->preview_program_stride_ub_offset        = -1;
        new_instance->preview_program_vp_ub_offset            = -1;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_normals_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_normals_preview_release(ogl_scene_renderer_normals_preview preview)
{
    _ogl_scene_renderer_normals_preview* preview_ptr = (_ogl_scene_renderer_normals_preview*) preview;

    if (preview_ptr->owned_scene != NULL)
    {
        scene_release(preview_ptr->owned_scene);
    }

    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_render(ogl_scene_renderer_normals_preview preview,
                                                                             uint32_t                           mesh_id)
{
    const ogl_context_gl_entrypoints*    entrypoints_ptr          = NULL;
    raGL_buffer                          mesh_bo                  = NULL;
    GLuint                               mesh_bo_id               = 0;
    unsigned int                         mesh_bo_size             = 0;
    uint32_t                             mesh_bo_start_offset     = -1;
    mesh                                 mesh_instance            = NULL;
    mesh_type                            mesh_instance_type;
    uint32_t                             mesh_start_offset_normal = -1;
    uint32_t                             mesh_start_offset_vertex = -1;
    uint32_t                             mesh_stride              = -1;
    uint32_t                             mesh_total_elements      = 0;
    system_matrix4x4                     normal_matrix            = NULL;
    _ogl_scene_renderer_normals_preview* preview_ptr              = (_ogl_scene_renderer_normals_preview*) preview;
    const GLint                          program_id               = ogl_program_get_id(preview_ptr->preview_program);

    /* Retrieve mesh properties */
    ogl_context_get_property               (preview_ptr->context,
                                            OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                           &entrypoints_ptr);
    ogl_scene_renderer_get_indexed_property(preview_ptr->owner,
                                            OGL_SCENE_RENDERER_PROPERTY_MESH_INSTANCE,
                                            mesh_id,
                                           &mesh_instance);
    ogl_scene_renderer_get_indexed_property(preview_ptr->owner,
                                            OGL_SCENE_RENDERER_PROPERTY_MESH_NORMAL_MATRIX,
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
                      MESH_PROPERTY_GL_BO,
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

    raGL_buffer_get_property(mesh_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &mesh_bo_id);
    raGL_buffer_get_property(mesh_bo,
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
        mesh_start_offset_normal - mesh_bo_start_offset,
        mesh_start_offset_vertex - mesh_bo_start_offset
    };

    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_ub_gs,
                                                preview_ptr->preview_program_normal_matrix_ub_offset,
                                                system_matrix4x4_get_column_major_data(normal_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_ub_vs,
                                                preview_ptr->preview_program_start_offsets_ub_offset,
                                                start_offsets,
                                                0, /* src_data_flags */
                                                sizeof(unsigned int) * 2);
    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_ub_vs,
                                                preview_ptr->preview_program_stride_ub_offset,
                                               &mesh_stride,
                                                0, /* src_data_flags */
                                                sizeof(unsigned int) );

    ogl_program_ub_sync(preview_ptr->preview_program_ub_gs);
    ogl_program_ub_sync(preview_ptr->preview_program_ub_vs);

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
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_start(ogl_scene_renderer_normals_preview preview,
                                                                            system_matrix4x4                   vp)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints_ptr = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr     = NULL;
    _ogl_scene_renderer_normals_preview*                      preview_ptr         = (_ogl_scene_renderer_normals_preview*) preview;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints_ptr);

    /* Initialize the program object if one has not been initialized yet */
    if (preview_ptr->preview_program == NULL)
    {
        _ogl_context_scene_renderer_normals_preview_init_preview_program(preview_ptr);

        ASSERT_DEBUG_SYNC(preview_ptr->preview_program != NULL,
                          "Could not initialize preview program");
    }

    /* Set up uniforms that will be shared across subsequent draw calls */
    const GLint program_id = ogl_program_get_id(preview_ptr->preview_program);
    GLuint      vao_id     = 0;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entrypoints_ptr->pGLUseProgram             (program_id);
    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_ub_gs,
                                                preview_ptr->preview_program_vp_ub_offset,
                                                system_matrix4x4_get_column_major_data(vp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);

    GLuint   preview_program_ub_gs_bo_id           = 0;
    uint32_t preview_program_ub_gs_bo_start_offset = -1;
    GLuint   preview_program_ub_vs_bo_id           = 0;
    uint32_t preview_program_ub_vs_bo_start_offset = -1;

    raGL_buffer_get_property(preview_ptr->preview_program_ub_gs_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_gs_bo_id);
    raGL_buffer_get_property(preview_ptr->preview_program_ub_gs_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_program_ub_gs_bo_start_offset);
    raGL_buffer_get_property(preview_ptr->preview_program_ub_vs_bo,
                             RAGL_BUFFER_PROPERTY_ID,
                            &preview_program_ub_vs_bo_id);
    raGL_buffer_get_property(preview_ptr->preview_program_ub_vs_bo,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &preview_program_ub_vs_bo_start_offset);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        preview_ptr->preview_program_ub_gs_ub_bp,
                                        preview_program_ub_gs_bo_id,
                                        preview_program_ub_gs_bo_start_offset,
                                        preview_ptr->preview_program_ub_gs_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        preview_ptr->preview_program_ub_vs_ub_bp,
                                        preview_program_ub_vs_bo_id,
                                        preview_program_ub_vs_bo_start_offset,
                                        preview_ptr->preview_program_ub_vs_bo_size);

    entrypoints_ptr->pGLBindVertexArray(vao_id);
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_stop(ogl_scene_renderer_normals_preview preview)
{
    const ogl_context_gl_entrypoints*    entrypoints_ptr = NULL;
    _ogl_scene_renderer_normals_preview* preview_ptr     = (_ogl_scene_renderer_normals_preview*) preview;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLBindVertexArray(0);
}


