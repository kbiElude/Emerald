/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "sh/sh_samples.h"
#include "shaders/shaders_embeddable_sh.h"
#include "shaders/shaders_embeddable_random_crude.h"
#include "system/system_log.h"

/* Internal type definitions */
typedef struct
{
    ogl_context               context;
    system_hashed_ansi_string name;
    uint32_t                  n_samples;
    uint32_t                  n_sh_bands;

    GLuint      sample_generator_sh_coeffs_BO_id;
    GLuint      sample_generator_theta_phi_BO_id;
    GLuint      sample_generator_unit_vec_BO_id;

    uint32_t    sample_generator_sh_coeffs_BO_size;
    uint32_t    sample_generator_theta_phi_BO_size;
    uint32_t    sample_generator_unit_vec_BO_size;
    
    ogl_program sh_coeffs_program;
    GLuint      sh_coeffs_program_n_samples_square_location;
    ogl_shader  sh_coeffs_vertex_shader;

    ogl_program unit_vec_program;
    GLuint      unit_vec_program_n_samples_square_location;
    ogl_shader  unit_vec_vertex_shader;

    ogl_texture sh_coeffs_tbo;
    ogl_texture theta_phi_tbo;
    GLuint      tfo_id;
    ogl_texture unit_vec_tbo;

    REFCOUNT_INSERT_VARIABLES

} _sh_samples;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(sh_samples,
                               sh_samples,
                              _sh_samples);

/* Internal variables */
const char* shared_body                       = "   float n_samples_square_inv = 1.0 / float(n_samples_square);\n"
                                                "\n"
                                                "   /* Spherical coordinate distribution */\n"
                                                "   float a    = float(vertex_id / n_samples_square);\n"
                                                "   float b    = float(vertex_id % n_samples_square);\n"
                                                "\n"
                                                "   float x     = (a + float(random(1 + vertex_id * 2  )) / 65535.0) * n_samples_square_inv;\n"
                                                "   float y     = (b + float(random(1 + vertex_id * 2+1)) / 65535.0) * n_samples_square_inv;\n"
                                                "   float theta = 2.0 * acos(sqrt(1.0 - x) );\n"
                                                "   float phi   = 2.0 * 3.14152965 * y;\n";

const char* unit_vec_generator_body           = "#version 330 core\n"
                                                "\n"
                                                "#define N_BANDS (%d)\n"
                                                "\n"
                                                /* Crude random generator goes here ==> */
                                                "%s\n"
                                                /* <== ..and ends here */
                                                "out result\n"
                                                "{\n"
                                                "    vec2 theta_phi;\n"
                                                "    vec3 unit_vec;\n"
                                                "\n"
                                                "} Out;\n"
                                                "\n"
                                                "uniform int n_samples_square;\n"
                                                "\n"
                                                "\n"
                                                "void main()\n"
                                                "{\n"
                                                "   int vertex_id = gl_VertexID;\n"
                                                "\n"
                                                /* Shared body goes here ==> */
                                                "%s\n"
                                                /* <== and ends here */
                                                "\n"
                                                "   /* Unit vector generation */\n"
                                                "   Out.theta_phi = vec2(theta, phi);\n"
                                                "   Out.unit_vec  = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta) );\n"
                                                "}\n";

const char* sh_coeffs_generator_template_body = "#version 330 core\n"
                                                "\n"
                                                "#define N_BANDS (%d)\n"
                                                /* Crude random generator goes here ==> */
                                                "%s\n"
                                                /* <== ..and ends here */
                                                /* Spherical harmonics code goes here ==> */
                                                "%s\n"
                                                /* <== ..and ends here */
                                                "out result\n"
                                                "{\n"
                                                "    float sh_coefficient;\n"
                                                "\n"
                                                "} Out;\n"
                                                "\n"
                                                "uniform int n_samples_square;\n"
                                                "\n"
                                                "\n"
                                                "void main()\n"
                                                "{\n"
                                                "\n"
                                                "   int vertex_id = gl_VertexID %% (N_BANDS * N_BANDS);\n"
                                                "\n"
                                                /* Shared body goes here ==> */
                                                "%s\n"
                                                /* <== and ends here */
                                                "   /* SH coefficients for processed sample */\n"
                                                "   float sh_coefficients[N_BANDS * N_BANDS];\n"
                                                "\n"
                                                "   for (int l = 0; l < N_BANDS; ++l)\n"
                                                "   {\n"
                                                "       for (int m = -l; m <= l; ++m)\n"
                                                "       {\n"
                                                "           int index = l * (l+1) + m;\n"
                                                "\n"
                                                "           sh_coefficients[index] = spherical_harmonics(l, m, theta, phi);\n"
                                                "       }\n"
                                                "   }\n"
                                                "\n"
                                                "   Out.sh_coefficient = sh_coefficients[vertex_id];\n"
                                                "}\n";

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _sh_samples_verify_context_type(__in __notnull ogl_context);
#else
    #define _sh_samples_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _sh_samples_create_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_samples*                                              data             = (_sh_samples*) arg;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLGenTransformFeedbacks(1,
                                          &data->tfo_id);

    /* Prepare SH coeffs generator body */
    uint32_t sh_coeffs_body_length = strlen(sh_coeffs_generator_template_body) +
                                     strlen(glsl_embeddable_random_crude)      +
                                     strlen(glsl_embeddable_sh)                +
                                     strlen(shared_body)                       +
                                     10;
    char*    sh_coeffs_body        = new (std::nothrow) char[sh_coeffs_body_length];

    ASSERT_DEBUG_SYNC(sh_coeffs_body != NULL,
                      "Out of memory");
    if (sh_coeffs_body != NULL)
    {
        sprintf_s(sh_coeffs_body,
                  sh_coeffs_body_length,
                  sh_coeffs_generator_template_body,
                  data->n_sh_bands,
                  glsl_embeddable_random_crude,
                  glsl_embeddable_sh,
                  shared_body);
    }

    /* Prepare unit vec generator body */
    uint32_t unit_vec_body_length = strlen(unit_vec_generator_body)      +
                                    strlen(glsl_embeddable_random_crude) +
                                    strlen(shared_body)                  +
                                    10;
    char*    unit_vec_body        = new (std::nothrow) char[unit_vec_body_length];

    ASSERT_DEBUG_SYNC(unit_vec_body != NULL,
                      "Out of memory");
    if (unit_vec_body != NULL)
    {
        sprintf_s(unit_vec_body,
                  unit_vec_body_length,
                  unit_vec_generator_body,
                  data->n_sh_bands,
                  glsl_embeddable_random_crude,
                  shared_body);
    }

    /* Instantiate SH coeffs vertex shader */
    const ogl_program_uniform_descriptor* sh_coeffs_n_samples_square_descriptor = NULL;
    static const char*                    sh_coeffs_tf_output_data[]            = {"result.sh_coefficient"};

    data->sh_coeffs_vertex_shader = ogl_shader_create (data->context,
                                                       SHADER_TYPE_VERTEX,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                               " SH coeffs") );
    data->sh_coeffs_program       = ogl_program_create(data->context,
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                               " SH coeffs") );

    entry_points->pGLShaderSource(ogl_shader_get_id(data->sh_coeffs_vertex_shader),
                                  1,
                                  &sh_coeffs_body,
                                  NULL);

    /* Instantiate unit vec vertex shader */
    const ogl_program_uniform_descriptor* unit_vec_n_samples_square_descriptor = NULL;
    static const char*                    unit_vec_tf_output_data[]            = {"result.unit_vec", "result.theta_phi"};

    data->unit_vec_vertex_shader = ogl_shader_create (data->context,
                                                      SHADER_TYPE_VERTEX,
                                                      system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                              " unit vec") );
    data->unit_vec_program       = ogl_program_create(data->context,
                                                      system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                              " unit vec") );

    entry_points->pGLShaderSource(ogl_shader_get_id(data->unit_vec_vertex_shader),
                                  1,
                                  &unit_vec_body,
                                  NULL);

    /* Configure SH coeffs program */
    ogl_program_attach_shader(data->sh_coeffs_program,
                              data->sh_coeffs_vertex_shader);

    entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data->sh_coeffs_program),
                                               sizeof(sh_coeffs_tf_output_data) / sizeof(sh_coeffs_tf_output_data[0]),
                                               sh_coeffs_tf_output_data,
                                               GL_INTERLEAVED_ATTRIBS);

    ogl_program_link               (data->sh_coeffs_program);
    ogl_program_get_uniform_by_name(data->sh_coeffs_program,
                                    system_hashed_ansi_string_create("n_samples_square"),
                                   &sh_coeffs_n_samples_square_descriptor);

    data->sh_coeffs_program_n_samples_square_location = sh_coeffs_n_samples_square_descriptor->location;

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                       "Could not configure SH coeffs program");

    /* Configure unit vec program */
    ogl_program_attach_shader(data->unit_vec_program,
                              data->unit_vec_vertex_shader);

    entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data->unit_vec_program),
                                               sizeof(unit_vec_tf_output_data) / sizeof(unit_vec_tf_output_data[0]),
                                               unit_vec_tf_output_data,
                                               GL_SEPARATE_ATTRIBS);

    ogl_program_link               (data->unit_vec_program);
    ogl_program_get_uniform_by_name(data->unit_vec_program,
                                    system_hashed_ansi_string_create("n_samples_square"),
                                   &unit_vec_n_samples_square_descriptor);

    data->unit_vec_program_n_samples_square_location = unit_vec_n_samples_square_descriptor->location;

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                       "Could not configure SH coeffs program");

    /* Prepare objects required for transform feedback */
    data->sample_generator_sh_coeffs_BO_size = data->n_samples * sizeof(GLfloat) * (data->n_sh_bands * data->n_sh_bands);
    data->sample_generator_theta_phi_BO_size = data->n_samples * sizeof(GLfloat) * 2;
    data->sample_generator_unit_vec_BO_size  = data->n_samples * sizeof(GLfloat) * 3;

    entry_points->pGLGenBuffers            (1,
                                           &data->sample_generator_sh_coeffs_BO_id);
    entry_points->pGLGenBuffers            (1,
                                           &data->sample_generator_theta_phi_BO_id);
    entry_points->pGLGenBuffers            (1,
                                           &data->sample_generator_unit_vec_BO_id);
    dsa_entry_points->pGLNamedBufferDataEXT(data->sample_generator_sh_coeffs_BO_id,
                                            data->sample_generator_sh_coeffs_BO_size,
                                            NULL,
                                            GL_STATIC_COPY);
    dsa_entry_points->pGLNamedBufferDataEXT(data->sample_generator_theta_phi_BO_id,
                                            data->sample_generator_theta_phi_BO_size,
                                            NULL,
                                            GL_STATIC_COPY);
    dsa_entry_points->pGLNamedBufferDataEXT(data->sample_generator_unit_vec_BO_id,
                                            data->sample_generator_unit_vec_BO_size,
                                            NULL,
                                            GL_STATIC_COPY);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                       "Could not create sample generator storage for %d samples",
                       data->n_samples);

    /* Create texture buffer */
    data->sh_coeffs_tbo = ogl_texture_create(data->context,
                                             system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                     " SH coeffs TBO") );
    data->theta_phi_tbo = ogl_texture_create(data->context,
                                             system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                     " SH theta/phi TBO") );
    data->unit_vec_tbo  = ogl_texture_create(data->context,
                                             system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                     " SH unit vec TBO") );

    dsa_entry_points->pGLTextureBufferEXT(data->sh_coeffs_tbo,
                                          GL_TEXTURE_BUFFER,
                                          GL_R32F,
                                          data->sample_generator_sh_coeffs_BO_id);
    dsa_entry_points->pGLTextureBufferEXT(data->theta_phi_tbo,
                                          GL_TEXTURE_BUFFER,
                                          GL_RG32F,
                                          data->sample_generator_theta_phi_BO_id);
    dsa_entry_points->pGLTextureBufferEXT(data->unit_vec_tbo,
                                          GL_TEXTURE_BUFFER,
                                          GL_RGB32F,
                                          data->sample_generator_unit_vec_BO_id);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                       "Could not generate SH coeffs TBO");

    /* Release bodies */
    if (sh_coeffs_body != NULL)
    {
        delete [] sh_coeffs_body;

        sh_coeffs_body = NULL;
    }

    if (unit_vec_body != NULL)
    {
        delete [] unit_vec_body;

        unit_vec_body = NULL;
    }
}

/** TODO */
PRIVATE void _sh_samples_release_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_samples*                      data_ptr     = (_sh_samples*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_shader_release (data_ptr->sh_coeffs_vertex_shader);
    ogl_shader_release (data_ptr->unit_vec_vertex_shader);
    ogl_program_release(data_ptr->sh_coeffs_program);
    ogl_program_release(data_ptr->unit_vec_program);
    ogl_texture_release(data_ptr->sh_coeffs_tbo);
    ogl_texture_release(data_ptr->theta_phi_tbo);
    ogl_texture_release(data_ptr->unit_vec_tbo);

    entry_points->pGLDeleteBuffers           (1,
                                             &data_ptr->sample_generator_sh_coeffs_BO_id);
    entry_points->pGLDeleteBuffers           (1,
                                             &data_ptr->sample_generator_unit_vec_BO_id);
    entry_points->pGLDeleteTransformFeedbacks(1,
                                             &data_ptr->tfo_id);
}

/** TODO */
PRIVATE void _sh_samples_release(__in __notnull __deallocate(mem) void* ptr)
{
    _sh_samples* data_ptr = (_sh_samples*) ptr;

    ogl_context_request_callback_from_context_thread(data_ptr->context,
                                                     _sh_samples_release_callback,
                                                     data_ptr);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _sh_samples_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "sh_samples is only supported under GL contexts")
    }
#endif


/** Please see header for specification */
PUBLIC EMERALD_API sh_samples sh_samples_create(__in __notnull ogl_context               context,
                                                __in           uint32_t                  n_samples_squareable,
                                                __in           uint32_t                  n_sh_bands,
                                                __in __notnull system_hashed_ansi_string name)
{
    _sh_samples_verify_context_type(context);

    /* Instantiate the object */
    _sh_samples* result = new (std::nothrow) _sh_samples;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not instantiate result object.");
    if (result != NULL)
    {
        result->context    = context;
        result->name       = name;
        result->n_samples  = n_samples_squareable;
        result->n_sh_bands = n_sh_bands;
    }

    if (context != NULL)
    {
        /* Request renderer callback */
        ogl_context_request_callback_from_context_thread(context,
                                                         _sh_samples_create_callback,
                                                         result);
    }
    else
    {
        /* Software only mode! */
    }

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                   _sh_samples_release,
                                                   OBJECT_TYPE_SH_SAMPLES,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\SH Sample Generator\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the result */
    return (sh_samples) result;
}

/** Please see header for specification */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_samples_execute(__in __notnull sh_samples samples)
{
    const ogl_context_gl_entrypoints* entry_points     = NULL;
    _sh_samples*                      samples_ptr      = (_sh_samples*) samples;
    GLuint                            vao_id           = 0;

    ogl_context_get_property(samples_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(samples_ptr->context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entry_points->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                           samples_ptr->tfo_id);
    entry_points->pGLBindVertexArray      (vao_id);
    {
        entry_points->pGLEnable(GL_RASTERIZER_DISCARD);
        {
            /* 1. Generate unit vectors using instanced draw call */
            GLuint unit_vec_program_id = ogl_program_get_id(samples_ptr->unit_vec_program);

            entry_points->pGLProgramUniform1i(unit_vec_program_id,
                                              samples_ptr->unit_vec_program_n_samples_square_location,
                                              (int) sqrt(float(samples_ptr->n_samples)) );

            entry_points->pGLUseProgram            (unit_vec_program_id);
            entry_points->pGLBindBufferBase        (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                    0,
                                                    samples_ptr->sample_generator_unit_vec_BO_id);
            entry_points->pGLBindBufferBase        (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                    1,
                                                    samples_ptr->sample_generator_theta_phi_BO_id);
            entry_points->pGLBeginTransformFeedback(GL_POINTS);
            {
                entry_points->pGLDrawArrays(GL_POINTS,
                                            0,
                                            samples_ptr->n_samples);
            }
            entry_points->pGLEndTransformFeedback();

            /* 2. Generate SH coefficients using another instanced draw call */
            GLuint sh_coeffs_program_id = ogl_program_get_id(samples_ptr->sh_coeffs_program);

            entry_points->pGLProgramUniform1i(sh_coeffs_program_id,
                                              samples_ptr->sh_coeffs_program_n_samples_square_location,
                                              (int) sqrt(float(samples_ptr->n_samples)) );

            entry_points->pGLUseProgram            (sh_coeffs_program_id);
            entry_points->pGLBindBufferBase        (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                    0,
                                                    samples_ptr->sample_generator_sh_coeffs_BO_id);
            entry_points->pGLBeginTransformFeedback(GL_POINTS);
            {
                entry_points->pGLDrawArrays(GL_POINTS,
                                            0,
                                            samples_ptr->n_sh_bands * samples_ptr->n_sh_bands * samples_ptr->n_samples);
            }
            entry_points->pGLEndTransformFeedback();

            entry_points->pGLBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER,
                                            0,
                                            0);
            entry_points->pGLUseProgram    (0);
        }
        entry_points->pGLDisable(GL_RASTERIZER_DISCARD);
    }
    entry_points->pGLBindVertexArray      (0);
    entry_points->pGLBindTransformFeedback(GL_TRANSFORM_FEEDBACK,
                                           0);
}

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_bands(__in __notnull sh_samples samples)
{
    return ((_sh_samples*)samples)->n_sh_bands;
}

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_coeffs(__in __notnull sh_samples samples)
{
    return ((_sh_samples*)samples)->n_sh_bands * ((_sh_samples*)samples)->n_sh_bands;
}

/** TODO */
PUBLIC EMERALD_API uint32_t sh_samples_get_amount_of_samples(__in __notnull sh_samples samples)
{
    return ((_sh_samples*)samples)->n_samples;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_sh_coeffs_bo_id(sh_samples samples)
{
    return ((_sh_samples*)samples)->sample_generator_sh_coeffs_BO_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_sh_coeffs_tbo(sh_samples samples)
{
    return ((_sh_samples*)samples)->sh_coeffs_tbo;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_theta_phi_bo_id(sh_samples samples)
{
    return ((_sh_samples*)samples)->sample_generator_theta_phi_BO_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_theta_phi_tbo(sh_samples samples)
{
    return ((_sh_samples*)samples)->theta_phi_tbo;
}

/* Please see header for specification */
PUBLIC EMERALD_API GLuint sh_samples_get_sample_unit_vec_bo_id(sh_samples samples)
{
    return ((_sh_samples*)samples)->sample_generator_unit_vec_BO_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture sh_samples_get_sample_unit_vec_tbo(sh_samples samples)
{
    return ((_sh_samples*)samples)->unit_vec_tbo;
}
