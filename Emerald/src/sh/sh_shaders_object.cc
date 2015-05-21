/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "sh/sh_samples.h"
#include "sh/sh_shaders_object.h"
#include "sh/sh_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include <sstream>

/* Internal variables */

/** Internal type definition */
typedef enum
{
    OBJECT_TYPE_UBO_WITH_BUFFER_TEXTURE
} _sh_shaders_object_type;

typedef struct
{
    /* Shared */
    ogl_shader  fragment_shader; /* user-provided */
    ogl_program program;
    ogl_shader  vertex_shader;

    system_hashed_ansi_string name;
    _sh_shaders_object_type   type;

    /* UBO + buffer texture */
    ogl_context context;
    sh_samples  samples;
    uint32_t    n_vertexes_per_instance;
    uint32_t    n_max_instances;

    GLuint program_light_sh_coefficients_sampler_location;
    GLuint program_matrices_location;
    GLuint program_object_sh_coefficients_sampler_location;
    GLuint program_position_sampler_location;

    REFCOUNT_INSERT_VARIABLES
} _sh_shaders_object;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(sh_shaders_object, sh_shaders_object, _sh_shaders_object);


/* Internal variables */
const char* _multidraw_mvp_per_vertex_ubo_model_buffer_texture_shader_body = "#version 430 core\n"
                                                                             "\n"
                                                                             "#define N_COEFFS                (%d)\n"
                                                                             "#define N_MAX_INSTANCES         (%d)\n"
                                                                             "#define N_VERTEXES_PER_INSTANCE (%d)\n"
                                                                             "\n"
                                                                             "layout(std140) uniform Matrices\n"
                                                                             "{\n"
                                                                             "    mat4 mvp[N_MAX_INSTANCES];\n"
                                                                             "};\n" 
                                                                             "\n"
                                                                             "uniform samplerBuffer light_sh_coefficients_sampler;\n"
                                                                             "uniform samplerBuffer object_sh_coefficients_sampler;\n"
                                                                             "uniform samplerBuffer position_sampler;\n"
                                                                             "\n"
                                                                             "out vec4 color;\n"
                                                                             "\n"
                                                                             "void main()\n"
                                                                             "{\n"
                                                                             "   int  instance_id = gl_VertexID / N_VERTEXES_PER_INSTANCE;\n"
                                                                             "   int  vertex_id   = gl_VertexID %% N_VERTEXES_PER_INSTANCE;\n"
                                                                             "\n"
                                                                             "   vec4 tmp_result  = vec4(0, 0, 0, 1);\n"
                                                                             "\n"
                                                                             "   for (int j = 0; j < N_COEFFS; ++j)\n"
                                                                             "   {\n"
                                                                             "       vec3 light_sh_coefficients  = texelFetch(light_sh_coefficients_sampler,                           j).xyz;\n"
                                                                             "       vec3 object_sh_coefficients = texelFetch(object_sh_coefficients_sampler, N_COEFFS * gl_VertexID + j).xyz;\n"
                                                                             "\n"
                                                                             "       tmp_result.xyz += light_sh_coefficients * object_sh_coefficients;\n"
                                                                             "   }\n"
                                                                             "\n"
                                                                             "   color       = tmp_result;\n"
                                                                             "   gl_Position = mvp[instance_id] * vec4(texelFetch(position_sampler, vertex_id).xyz, 1.0);\n"
                                                                             "}\n";

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _sh_shaders_object_verify_context_type(__in __notnull ogl_context);
#else
    #define _sh_shaders_object_verify_context_type(x)
#endif


/** Function called back when reference counter drops to zero. Releases the static shader object.
 *
 *  @param ptr Pointer to _shaders_fragment_static instance.
 **/
PRIVATE void _sh_shaders_object_release(__in __notnull __deallocate(mem) void* ptr)
{
    _sh_shaders_object* data_ptr = (_sh_shaders_object*) ptr;

    if (data_ptr->fragment_shader != NULL)
    {
        ogl_shader_release(data_ptr->fragment_shader);

        data_ptr->fragment_shader = NULL;
    }

    if (data_ptr->vertex_shader != NULL)
    {
        ogl_shader_release(data_ptr->vertex_shader);

        data_ptr->vertex_shader = NULL;
    }

    if (data_ptr->program != NULL)
    {
        ogl_program_release(data_ptr->program);

        data_ptr->program = NULL;
    }

    ogl_context_release(data_ptr->context);
    sh_samples_release (data_ptr->samples);
}

/** TODO */
PRIVATE void _sh_shaders_object_create_per_vertex_ubo_buffertexture_rendering_callback(ogl_context context, void* arg)
{
    _sh_shaders_object* ptr = (_sh_shaders_object*) arg;

    /* Do the job. */
    uint32_t preview_vp_body_length = strlen(_multidraw_mvp_per_vertex_ubo_model_buffer_texture_shader_body) + 10 + 1;
    char*    preview_vp_body        = new (std::nothrow) char[preview_vp_body_length];

    if (preview_vp_body != NULL)
    {
        uint32_t n_sh_coefficients = sh_samples_get_amount_of_coeffs(ptr->samples);

        sprintf_s(preview_vp_body, 
                  preview_vp_body_length,
                  _multidraw_mvp_per_vertex_ubo_model_buffer_texture_shader_body, 
                  n_sh_coefficients,
                  ptr->n_max_instances,
                  ptr->n_vertexes_per_instance);

        ptr->vertex_shader = ogl_shader_create(ptr->context, 
                                               SHADER_TYPE_VERTEX,
                                               system_hashed_ansi_string_create_by_merging_two_strings("SH object UBO+buffertexture ",
                                                                                                system_hashed_ansi_string_get_buffer(ptr->name) ));

        ogl_shader_set_body(ptr->vertex_shader, system_hashed_ansi_string_create(preview_vp_body) );

        delete [] preview_vp_body;
        preview_vp_body = NULL;
    }

    ptr->program = ogl_program_create(ptr->context, 
                                      system_hashed_ansi_string_create_by_merging_two_strings("SH object UBO+buffertexture ",
                                                                                              system_hashed_ansi_string_get_buffer(ptr->name) ));

    ogl_program_attach_shader(ptr->program, ptr->fragment_shader);
    ogl_program_attach_shader(ptr->program, ptr->vertex_shader);
    ogl_program_link         (ptr->program);

    /* Set up preview UBO */
    const ogl_program_variable* program_light_sh_coefficients_descriptor   = NULL;
    const ogl_program_variable* program_object_sh_coefficients_descriptor  = NULL;
    const ogl_program_variable* program_position_descriptor                = NULL;
    GLuint                      program_id                                 = ogl_program_get_id(ptr->program);

    ogl_program_get_uniform_by_name(ptr->program, system_hashed_ansi_string_create("light_sh_coefficients_sampler"),  &program_light_sh_coefficients_descriptor);
    ogl_program_get_uniform_by_name(ptr->program, system_hashed_ansi_string_create("object_sh_coefficients_sampler"), &program_object_sh_coefficients_descriptor);
    ogl_program_get_uniform_by_name(ptr->program, system_hashed_ansi_string_create("position_sampler"),               &program_position_descriptor);

    ptr->program_light_sh_coefficients_sampler_location  = program_light_sh_coefficients_descriptor->location;
    ptr->program_object_sh_coefficients_sampler_location = program_object_sh_coefficients_descriptor->location;
    ptr->program_position_sampler_location               = program_position_descriptor->location;

    /* Set up program stuff */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Set up UBO bindings */
    ptr->program_matrices_location = entry_points->pGLGetUniformBlockIndex(program_id, "Matrices");

    entry_points->pGLUniformBlockBinding(program_id, ptr->program_matrices_location, 0);

    /* Set up sampler uniforms */
    entry_points->pGLProgramUniform1i(program_id, ptr->program_object_sh_coefficients_sampler_location, 0);
    entry_points->pGLProgramUniform1i(program_id, ptr->program_light_sh_coefficients_sampler_location,  1);
    entry_points->pGLProgramUniform1i(program_id, ptr->program_position_sampler_location,               2);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _sh_shaders_object_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "sh_shaders_object is only supported under GL contexts")
    }
#endif


/** Please see header for specification */
PUBLIC EMERALD_API sh_shaders_object sh_shaders_object_create_per_vertex_ubo_buffertexture(__in __notnull ogl_context               context,
                                                                                            __in           sh_samples                samples,
                                                                                            __in           uint32_t                  n_vertexes_per_instance,
                                                                                            __in           uint32_t                  n_max_instances,
                                                                                            __in __notnull ogl_shader                fragment_shader,
                                                                                            __in __notnull system_hashed_ansi_string name)
{
    _sh_shaders_object_verify_context_type(context);

    /* Create new instance */
    _sh_shaders_object* new_instance = new (std::nothrow) _sh_shaders_object;

    ASSERT_DEBUG_SYNC(new_instance != NULL, "Could not create _sh_shaders_object instance.");
    if (new_instance != NULL)
    {
        new_instance->context                 = context;
        new_instance->fragment_shader         = fragment_shader;
        new_instance->name                    = name;
        new_instance->n_max_instances         = n_max_instances;
        new_instance->n_vertexes_per_instance = n_vertexes_per_instance;
        new_instance->samples                 = samples;
        new_instance->type                    = OBJECT_TYPE_UBO_WITH_BUFFER_TEXTURE;

        ogl_context_retain(new_instance->context);
        ogl_shader_retain (new_instance->fragment_shader);
        sh_samples_retain (new_instance->samples);

        /* Move to rendering thread to create actual shader, since we need to use UBOs */
        ogl_context_request_callback_from_context_thread(context, _sh_shaders_object_create_per_vertex_ubo_buffertexture_rendering_callback, new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _sh_shaders_object_release,
                                                       OBJECT_TYPE_SH_SHADERS_OBJECT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\SH Shader Objects\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (sh_shaders_object) new_instance;
}

/** Please see header for specification */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_shaders_object_draw_per_vertex_ubo_buffertexture(__in __notnull sh_shaders_object instance,
                                                                                                   __in           uint32_t          mvp_storage_bo_id,
                                                                                                   __in           ogl_texture       object_sh_storage_tbo,
                                                                                                   __in           ogl_texture       light_sh_storage_tbo,
                                                                                                   __in           ogl_texture       vertex_data_tbo,
                                                                                                   __in           uint32_t          n_instances)
{
    _sh_shaders_object*                                       data_ptr         = (_sh_shaders_object*) instance;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;

    ogl_context_get_property(data_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(data_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ASSERT_DEBUG_SYNC(data_ptr->type == OBJECT_TYPE_UBO_WITH_BUFFER_TEXTURE, "Incorrect draw request!");

    /* Let's go */
    entry_points->pGLUseProgram(ogl_program_get_id(data_ptr->program) );
    entry_points->pGLBindBufferBase(GL_UNIFORM_BUFFER, 0, mvp_storage_bo_id);

    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, object_sh_storage_tbo);
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE1, GL_TEXTURE_BUFFER, light_sh_storage_tbo);
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE2, GL_TEXTURE_BUFFER, vertex_data_tbo);
    
    entry_points->pGLDrawArrays(GL_TRIANGLES, 0, data_ptr->n_vertexes_per_instance * n_instances);
}
