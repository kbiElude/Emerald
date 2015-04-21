/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_normals_preview.h"
#include "ogl/ogl_shader.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "system/system_matrix4x4.h"
#include <string>
#include <sstream>

static const char* preview_fragment_shader = "#version 420\n"
                                             "\n"
                                             "out vec4 result;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    result = vec4(0, 1, 0, 1);\n"
                                             "}\n";
static const char* preview_geometry_shader = "#version 420\n"
                                             "\n"
                                             "layout(points)                     in;\n"
                                             "layout(line_strip, max_vertices=2) out;\n"
                                             "\n"
                                             "in vec3 normal[];\n"
                                             "in vec3 vertex[];\n"
                                             "\n"
                                             "uniform mat4 normal_matrix;\n"
                                             "uniform mat4 vp;\n"
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
static const char* preview_vertex_shader   = "#version 420\n"
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
                                             "uniform uvec2 start_offsets;\n"
                                             "uniform uint  stride;\n"
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

    ogl_scene_renderer owner;
    ogl_program        preview_program;
    GLint              preview_program_normal_matrix_location;
    GLint              preview_program_start_offsets_location;
    GLint              preview_program_stride_location;
    GLint              preview_program_vp_location;
    scene              scene;
} _ogl_scene_renderer_normals_preview;

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _ogl_scene_renderer_normals_preview_verify_context_type(__in __notnull ogl_context);
#else
    #define _ogl_scene_renderer_normals_preview_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _ogl_context_scene_renderer_normals_preview_init_preview_program(__in __notnull _ogl_scene_renderer_normals_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Preview program has already been initialized");

    /* Create shaders and set their bodies */
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(preview_ptr->scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    ogl_shader fs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_FRAGMENT,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview FS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader gs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_GEOMETRY,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer normals preview GS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader vs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_VERTEX,
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
                                                                                                              system_hashed_ansi_string_get_buffer(scene_name)) );

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
    const ogl_program_uniform_descriptor* normal_matrix_uniform_descriptor_ptr = NULL;
    const ogl_program_uniform_descriptor* start_offsets_uniform_descriptor_ptr = NULL;
    const ogl_program_uniform_descriptor* stride_uniform_descriptor_ptr        = NULL;
    const ogl_program_uniform_descriptor* vp_uniform_descriptor_ptr            = NULL;

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
        preview_ptr->preview_program_normal_matrix_location = normal_matrix_uniform_descriptor_ptr->location;
    }

    if (start_offsets_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_start_offsets_location = start_offsets_uniform_descriptor_ptr->location;
    }

    if (stride_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_stride_location = stride_uniform_descriptor_ptr->location;
    }

    if (vp_uniform_descriptor_ptr != NULL)
    {
        preview_ptr->preview_program_vp_location = vp_uniform_descriptor_ptr->location;
    }

    /* Set up SSBO bindings */
    const ogl_context_gl_entrypoints_arb_shader_storage_buffer_object* ssbo_entrypoints_ptr = NULL;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_ARB_SHADER_STORAGE_BUFFER_OBJECT,
                            &ssbo_entrypoints_ptr);

    ssbo_entrypoints_ptr->pGLShaderStorageBlockBinding(ogl_program_get_id(preview_ptr->preview_program),
                                                       0,  /* storageBlockIndex */
                                                       0); /* storageBlockBinding */

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
    PRIVATE void _ogl_scene_renderer_normals_preview_verify_context_type(__in __notnull ogl_context context)
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
PUBLIC ogl_scene_renderer_normals_preview ogl_scene_renderer_normals_preview_create(__in __notnull ogl_context        context,
                                                                                    __in __notnull scene              scene,
                                                                                    __in __notnull ogl_scene_renderer owner)
{
    _ogl_scene_renderer_normals_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_normals_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance->context                                = context;
        new_instance->owner                                  = owner;
        new_instance->preview_program                        = NULL;
        new_instance->preview_program_normal_matrix_location = -1;
        new_instance->preview_program_start_offsets_location = -1;
        new_instance->preview_program_stride_location        = -1;
        new_instance->preview_program_vp_location            = -1;
        new_instance->scene                                  = scene;

        scene_retain(scene);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_normals_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_normals_preview_release(__in __notnull __post_invalid ogl_scene_renderer_normals_preview preview)
{
    _ogl_scene_renderer_normals_preview* preview_ptr = (_ogl_scene_renderer_normals_preview*) preview;

    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }

    if (preview_ptr->scene != NULL)
    {
        scene_release(preview_ptr->scene);
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_render(__in __notnull ogl_scene_renderer_normals_preview preview,
                                                                             __in __notnull uint32_t                           mesh_id)
{
    const ogl_context_gl_entrypoints*    entrypoints_ptr          = NULL;
    GLuint                               mesh_bo_id               = 0;
    unsigned int                         mesh_bo_size             = 0;
    unsigned int                         mesh_bo_start_offset     = -1;
    mesh                                 mesh_instance            = NULL;
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

    /* Retrieve start offsets & stride information */
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_BO_ID,
                     &mesh_bo_id);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_BO_START_OFFSET,
                     &mesh_bo_start_offset);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,
                     &mesh_bo_size);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_STRIDE,
                     &mesh_stride);
    mesh_get_property(mesh_instance,
                      MESH_PROPERTY_GL_TOTAL_ELEMENTS,
                     &mesh_total_elements);

    mesh_get_layer_data_stream_property(mesh_instance,
                                        MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &mesh_start_offset_normal);
    mesh_get_layer_data_stream_property(mesh_instance,
                                        MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                        MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
                                       &mesh_start_offset_vertex);

    /* Set the uniforms */
    const uint32_t start_offsets[] =
    {
        mesh_start_offset_normal - mesh_bo_start_offset,
        mesh_start_offset_vertex - mesh_bo_start_offset
    };

    entrypoints_ptr->pGLProgramUniformMatrix4fv(program_id,
                                                preview_ptr->preview_program_normal_matrix_location,
                                                1, /* count */
                                                GL_TRUE,
                                                system_matrix4x4_get_row_major_data(normal_matrix) );
    entrypoints_ptr->pGLProgramUniform2uiv     (program_id,
                                                preview_ptr->preview_program_start_offsets_location,
                                                1, /* count */
                                                start_offsets);
    entrypoints_ptr->pGLProgramUniform1ui      (program_id,
                                                preview_ptr->preview_program_stride_location,
                                                mesh_stride);

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
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_start(__in __notnull ogl_scene_renderer_normals_preview preview,
                                                                            __in __notnull system_matrix4x4                   vp)
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
    entrypoints_ptr->pGLProgramUniformMatrix4fv(program_id,
                                                preview_ptr->preview_program_vp_location,
                                                1, /* count */
                                                GL_TRUE,
                                                system_matrix4x4_get_row_major_data(vp) );

    entrypoints_ptr->pGLBindVertexArray(vao_id);
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_normals_preview_stop(__in __notnull ogl_scene_renderer_normals_preview preview)
{
    const ogl_context_gl_entrypoints*    entrypoints_ptr = NULL;
    _ogl_scene_renderer_normals_preview* preview_ptr     = (_ogl_scene_renderer_normals_preview*) preview;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    entrypoints_ptr->pGLBindVertexArray(0);
}


