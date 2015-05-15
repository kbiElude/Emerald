/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_scene_renderer_bbox_preview.h"
#include "ogl/ogl_shader.h"
#include "scene/scene.h"
#include "scene/scene_mesh.h"
#include "system/system_matrix4x4.h"
#include <string>
#include <sstream>

static const char* preview_fragment_shader = "#version 420 core\n"
                                             "\n"
                                             "out vec4 result;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    result = vec4(1, 1, 1, 1);\n"
                                             "}\n";
static const char* preview_geometry_shader = "#version 420 core\n"
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
static const char* preview_vertex_shader   = "#version 420 core\n"
                                             "\n"
                                             "out uint vertex_id;\n"
                                             "\n"
                                             "void main()\n"
                                             "{\n"
                                             "    vertex_id = gl_VertexID;\n"
                                             "}\n";

/** TODO */
typedef struct _ogl_scene_renderer_bbox_preview
{
    /* Owned by ogl_context. DO NOT release. */
    ogl_buffers buffers;

    /* DO NOT retain/release, as this object is managed by ogl_context and retaining it
     * will cause the rendering context to never release itself.
     */
    ogl_context context;

    GLuint             data_bo_id; /* owned by ogl_buffers - do NOT release with glDeleteBuffers() */
    unsigned int       data_bo_size;
    unsigned int       data_bo_start_offset;
    uint32_t           data_n_meshes;
    ogl_scene_renderer owner;
    ogl_program        preview_program;
    ogl_program_ub     preview_program_data_ub;
    GLuint             preview_program_ub_offset_model;
    GLuint             preview_program_ub_offset_vp;
    scene              scene;

    /* Cached func ptrs */
    PFNGLBINDBUFFERPROC              pGLBindBuffer;
    PFNGLBINDBUFFERRANGEPROC         pGLBindBufferRange;
    PFNGLBINDVERTEXARRAYPROC         pGLBindVertexArray;
    PFNGLBUFFERSUBDATAPROC           pGLBufferSubData;
    PFNGLDELETEVERTEXARRAYSPROC      pGLDeleteVertexArrays;
    PFNGLDRAWARRAYSPROC              pGLDrawArrays;
    PFNGLGENBUFFERSPROC              pGLGenBuffers;
    PFNGLGENVERTEXARRAYSPROC         pGLGenVertexArrays;
    PFNGLUNIFORMBLOCKBINDINGPROC     pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC              pGLUseProgram;
} _ogl_scene_renderer_bbox_preview;


/** TODO */
PRIVATE void _ogl_context_scene_renderer_bbox_preview_init_preview_program(__in __notnull _ogl_scene_renderer_bbox_preview* preview_ptr)
{
    ASSERT_DEBUG_SYNC(preview_ptr->preview_program == NULL,
                      "Previe wprogram has already been initialized");

    /* Form a string from the number of meshes associated with the scene */
    std::stringstream n_scene_meshes_sstream;
    uint32_t          n_scene_meshes = 0;

    scene_get_property(preview_ptr->scene,
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
    system_hashed_ansi_string scene_name = NULL;

    scene_get_property(preview_ptr->scene,
                       SCENE_PROPERTY_NAME,
                      &scene_name);

    ogl_shader fs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_FRAGMENT,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview FS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader gs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_GEOMETRY,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview GS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );
    ogl_shader vs_shader = ogl_shader_create(preview_ptr->context,
                                             SHADER_TYPE_VERTEX,
                                             system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview VS shader ",
                                                                                                     system_hashed_ansi_string_get_buffer(scene_name)) );

    ogl_shader_set_body(fs_shader,
                        system_hashed_ansi_string_create(preview_fragment_shader) );
    ogl_shader_set_body(gs_shader,
                        system_hashed_ansi_string_create(gs_body.c_str()) );
    ogl_shader_set_body(vs_shader,
                        system_hashed_ansi_string_create(preview_vertex_shader) );

    if (!ogl_shader_compile(fs_shader) ||
        !ogl_shader_compile(gs_shader) ||
        !ogl_shader_compile(vs_shader) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to compile at least one of the shader used by BBox preview renderer");

        goto end_fail;
    }

    /* Initialize the program object */
    preview_ptr->preview_program = ogl_program_create(preview_ptr->context,
                                                      system_hashed_ansi_string_create_by_merging_two_strings("Scene Renderer BB preview program ",
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
                          "Failed to link the program object used by BBox preview renderer");

        goto end_fail;
    }

    /* Retrieve uniform block manager */
    const ogl_program_variable* model_uniform_ptr = NULL;
    const ogl_program_variable* vp_uniform_ptr    = NULL;

    ogl_program_get_uniform_block_by_name(preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("data"),
                                         &preview_ptr->preview_program_data_ub);
    ogl_program_get_uniform_by_name      (preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("model"),
                                         &model_uniform_ptr);
    ogl_program_get_uniform_by_name      (preview_ptr->preview_program,
                                          system_hashed_ansi_string_create("vp"),
                                         &vp_uniform_ptr);

    ASSERT_DEBUG_SYNC(preview_ptr->preview_program_data_ub != NULL,
                      "Preview program's data uniform block is NULL");
    ASSERT_DEBUG_SYNC(model_uniform_ptr != NULL,
                      "Model uniform descriptor is NULL");
    ASSERT_DEBUG_SYNC(vp_uniform_ptr != NULL,
                      "Vp uniform descriptor is NULL");

    ASSERT_DEBUG_SYNC(model_uniform_ptr->ub_offset != -1,
                      "Model matrix UB offset is -1");
    ASSERT_DEBUG_SYNC(vp_uniform_ptr->ub_offset != -1,
                      "View matrix UB offset is -1");

    preview_ptr->preview_program_ub_offset_model = model_uniform_ptr->ub_offset;
    preview_ptr->preview_program_ub_offset_vp    = vp_uniform_ptr->ub_offset;

    /* Set up UBO bindings */
    preview_ptr->pGLUniformBlockBinding(ogl_program_get_id(preview_ptr->preview_program),
                                        0,  /* uniformBlockIndex */
                                        0); /* uniformBlockBinding */

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
PRIVATE void _ogl_context_scene_renderer_bbox_preview_init_ub_data(__in __notnull _ogl_scene_renderer_bbox_preview* preview_ptr)
{
    float* ub_data = NULL;

    /* Allocate space for AABB data. */
    const uint32_t matrix_data_size = 4 /* vec4 */ * 2 /* max, min */ * preview_ptr->data_n_meshes * sizeof(float);

    ub_data = new (std::nothrow) float[matrix_data_size / sizeof(float)];

    ASSERT_ALWAYS_SYNC(ub_data != NULL,
                       "Out of memory");
    if (ub_data == NULL)
    {
        goto end;
    }

    /* Fill the buffer with data */
    float* traveller_ptr = NULL;

    for (unsigned int n_iteration = 0;
                      n_iteration < 2;
                    ++n_iteration)
    {
        for (unsigned int n_mesh = 0;
                          n_mesh < preview_ptr->data_n_meshes;
                        ++n_mesh)
        {
            float*     aabb_max_ptr              = NULL;
            float*     aabb_min_ptr              = NULL;
            uint32_t   mesh_id                   = -1;
            scene_mesh mesh_instance             = scene_get_mesh_instance_by_index(preview_ptr->scene,
                                                                                    n_mesh);
            mesh       mesh_current              = NULL;
            mesh       mesh_instantiation_parent = NULL;

            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_ID,
                                   &mesh_id);
            scene_mesh_get_property(mesh_instance,
                                    SCENE_MESH_PROPERTY_MESH,
                                   &mesh_current);

            mesh_get_property(mesh_current,
                              MESH_PROPERTY_INSTANTIATION_PARENT,
                             &mesh_instantiation_parent);

            if (mesh_instantiation_parent != NULL)
            {
                mesh_current = mesh_instantiation_parent;
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
        } /* for (all meshes) */
    } /* for (both iterations) */

    /* Retrieve UB offset alignment */
    ogl_context_type context_type                    = OGL_CONTEXT_TYPE_UNDEFINED;
    GLuint           uniform_buffer_offset_alignment = -1;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        ASSERT_DEBUG_SYNC(false,
                          "TODO: ES limits");
    } /* if (context_type == OGL_CONTEXT_TYPE_ES) */
    else
    {
        ogl_context_gl_limits* limits_ptr = NULL;

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized rendering context type.");

        ogl_context_get_property(preview_ptr->context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        uniform_buffer_offset_alignment = limits_ptr->uniform_buffer_offset_alignment;
    }

    /* Initialize UBO storage.
     *
     * NOTE: Since ogl_program_ub does not provide arrayed uniform setters at the time of writing,
     *       simply push the AABB data behind its back. This is OK, since AABB data is set in stone
     *       and will not change later.
     */
    ogl_program_ub_get_property(preview_ptr->preview_program_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &preview_ptr->data_bo_size);
    ogl_program_ub_get_property(preview_ptr->preview_program_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &preview_ptr->data_bo_id);
    ogl_program_ub_get_property(preview_ptr->preview_program_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &preview_ptr->data_bo_start_offset);

    preview_ptr->pGLBindBuffer   (GL_ARRAY_BUFFER,
                                  preview_ptr->data_bo_id);
    preview_ptr->pGLBufferSubData(GL_ARRAY_BUFFER,
                                  preview_ptr->data_bo_start_offset,
                                  matrix_data_size,
                                  ub_data);

    /* All set! */
end:
    if (ub_data != NULL)
    {
        delete [] ub_data;

        ub_data = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_scene_renderer_bbox_preview_release_renderer_callback(__in __notnull ogl_context context,
                                                                        __in __notnull void*       preview)
{
    _ogl_scene_renderer_bbox_preview* preview_ptr = (_ogl_scene_renderer_bbox_preview*) preview;

    if (preview_ptr->preview_program != NULL)
    {
        ogl_program_release(preview_ptr->preview_program);

        preview_ptr->preview_program = NULL;
    }
}

/** Please see header for spec */
PUBLIC ogl_scene_renderer_bbox_preview ogl_scene_renderer_bbox_preview_create(__in __notnull ogl_context        context,
                                                                              __in __notnull scene              scene,
                                                                              __in __notnull ogl_scene_renderer owner)
{
    _ogl_scene_renderer_bbox_preview* new_instance = new (std::nothrow) _ogl_scene_renderer_bbox_preview;

    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        /* Do not allocate any GL objects at this point. We will only create GL objects if we
         * are asked to render the preview.
         */
        new_instance->context                         = context;
        new_instance->data_bo_id                      = 0;
        new_instance->data_bo_size                    = 0;
        new_instance->data_bo_start_offset            = -1;
        new_instance->data_n_meshes                   = 0;
        new_instance->owner                           = owner;
        new_instance->preview_program                 = NULL;
        new_instance->preview_program_data_ub         = NULL;
        new_instance->preview_program_ub_offset_model = -1;
        new_instance->preview_program_ub_offset_vp    = -1;
        new_instance->scene                           = scene;

        ogl_context_get_property(new_instance->context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &new_instance->buffers);

        /* Is buffer_storage supported? */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_instance->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_instance->pGLBindBuffer          = entry_points->pGLBindBuffer;
            new_instance->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_instance->pGLBindVertexArray     = entry_points->pGLBindVertexArray;
            new_instance->pGLBufferSubData       = entry_points->pGLBufferSubData;
            new_instance->pGLDeleteVertexArrays  = entry_points->pGLDeleteVertexArrays;
            new_instance->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_instance->pGLGenBuffers          = entry_points->pGLGenBuffers;
            new_instance->pGLGenVertexArrays     = entry_points->pGLGenVertexArrays;
            new_instance->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_instance->pGLUseProgram          = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_instance->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_instance->pGLBindBuffer          = entry_points->pGLBindBuffer;
            new_instance->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_instance->pGLBindVertexArray     = entry_points->pGLBindVertexArray;
            new_instance->pGLBufferSubData       = entry_points->pGLBufferSubData;
            new_instance->pGLDeleteVertexArrays  = entry_points->pGLDeleteVertexArrays;
            new_instance->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_instance->pGLGenBuffers          = entry_points->pGLGenBuffers;
            new_instance->pGLGenVertexArrays     = entry_points->pGLGenVertexArrays;
            new_instance->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_instance->pGLUseProgram          = entry_points->pGLUseProgram;
        }

        /* Wrap up */
        scene_retain(scene);

        scene_get_property(scene,
                           SCENE_PROPERTY_N_MESH_INSTANCES,
                          &new_instance->data_n_meshes);
    } /* if (new_instance != NULL) */

    return (ogl_scene_renderer_bbox_preview) new_instance;
}

/** Please see header for spec */
PUBLIC void ogl_scene_renderer_bbox_preview_release(__in __notnull __post_invalid ogl_scene_renderer_bbox_preview preview)
{
    _ogl_scene_renderer_bbox_preview* preview_ptr = (_ogl_scene_renderer_bbox_preview*) preview;

    if (preview_ptr->data_bo_id != 0)
    {
        ogl_context_request_callback_from_context_thread(preview_ptr->context,
                                                         _ogl_scene_renderer_bbox_preview_release_renderer_callback,
                                                         preview_ptr);
    }

    if (preview_ptr->scene != NULL)
    {
        scene_release(preview_ptr->scene);
    }

    delete preview_ptr;
    preview_ptr = NULL;
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_render(__in __notnull ogl_scene_renderer_bbox_preview preview,
                                                                          __in __notnull uint32_t                        mesh_id)
{
    system_matrix4x4                  model       = NULL;
    _ogl_scene_renderer_bbox_preview* preview_ptr = (_ogl_scene_renderer_bbox_preview*) preview;
    const GLint                       program_id  = ogl_program_get_id(preview_ptr->preview_program);

    ogl_scene_renderer_get_indexed_property(preview_ptr->owner,
                                            OGL_SCENE_RENDERER_PROPERTY_MESH_MODEL_MATRIX,
                                            mesh_id,
                                           &model);

    /* NOTE: model may be null at this point if the item was culled out. */
    if (model != NULL)
    {
        ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_data_ub,
                                                    preview_ptr->preview_program_ub_offset_model,
                                                    system_matrix4x4_get_row_major_data(model),
                                                    0, /* src_data_flags */
                                                    sizeof(float) * 16);

        ogl_program_ub_sync(preview_ptr->preview_program_data_ub);

        preview_ptr->pGLDrawArrays(GL_POINTS,
                                   mesh_id, /* first */
                                   1);      /* count */
    }
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_start(__in __notnull ogl_scene_renderer_bbox_preview preview,
                                                                         __in __notnull system_matrix4x4                vp)
{
    _ogl_scene_renderer_bbox_preview* preview_ptr  = (_ogl_scene_renderer_bbox_preview*) preview;

    /* Initialize the program object if one has not been initialized yet */
    if (preview_ptr->preview_program == NULL)
    {
        _ogl_context_scene_renderer_bbox_preview_init_preview_program(preview_ptr);

        ASSERT_DEBUG_SYNC(preview_ptr->preview_program != NULL,
                          "Could not initialize preview program");
    }

    /* Initialize a BO store if one has not been created yet */
    if (preview_ptr->data_bo_id == 0)
    {
        _ogl_context_scene_renderer_bbox_preview_init_ub_data(preview_ptr);
    }

    /* Issue the draw call */
    const GLint program_id = ogl_program_get_id(preview_ptr->preview_program);
    GLuint      vao_id     = 0;

    ogl_context_get_property(preview_ptr->context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    preview_ptr->pGLUseProgram(program_id);

    ogl_program_ub_set_nonarrayed_uniform_value(preview_ptr->preview_program_data_ub,
                                                preview_ptr->preview_program_ub_offset_vp,
                                                system_matrix4x4_get_row_major_data(vp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);

    preview_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                    preview_ptr->data_bo_id,
                                    preview_ptr->data_bo_start_offset,
                                    preview_ptr->data_bo_size);

    preview_ptr->pGLBindVertexArray(vao_id);
}

/** Please see header for spec */
PUBLIC RENDERING_CONTEXT_CALL void ogl_scene_renderer_bbox_preview_stop(__in __notnull ogl_scene_renderer_bbox_preview preview)
{
    _ogl_scene_renderer_bbox_preview* preview_ptr = (_ogl_scene_renderer_bbox_preview*) preview;

    preview_ptr->pGLBindVertexArray(0);
}


