/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_types.h"
#include "sh/sh_rot.h"
#include "sh/sh_rot_precalc.h"
#include "sh/sh_types.h"
#include "shaders/shaders_embeddable_sh.h"
#include "system/system_log.h"


/** Internal type definition */
typedef struct
{
    ogl_context               context;
    system_hashed_ansi_string name;
    uint32_t                  n_bands;

    #ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
        uint32_t    rot_y_krivanek_ddy_diagonal_buffer_id;
        uint32_t    rot_y_krivanek_ddy_diagonal_tbo_id;
        uint32_t    rot_y_krivanek_dy_subdiagonal_buffer_id;
        uint32_t    rot_y_krivanek_dy_subdiagonal_tbo_id;
        float*      rot_y_krivanek_diagonal_subdiagonal_cache;
        ogl_program rot_y_krivanek_program;
        GLuint      rot_y_krivanek_program_input_location;
        GLuint      rot_y_krivanek_program_dy_subdiagonal_location;
        GLuint      rot_y_krivanek_program_ddy_diagonal_location;
        GLuint      rot_y_krivanek_program_y_angle_location;
        ogl_shader  rot_y_krivanek_vertex_shader;

        sh_derivative_rotation_matrices rotation_matrices_derivative;
    #endif

    ogl_program rot_xyz_program;
    GLuint      rot_xyz_program_input_location;
    GLuint      rot_xyz_program_x_angle_location;
    GLuint      rot_xyz_program_y_angle_location;
    GLuint      rot_xyz_program_z_angle_location;
    ogl_shader  rot_xyz_vertex_shader;

    ogl_program rot_z_program;
    GLuint      rot_z_program_input_location;
    GLuint      rot_z_program_z_angle_location;
    ogl_shader  rot_z_vertex_shader;

    GLuint vaa_id;

    REFCOUNT_INSERT_VARIABLES
} _sh_rot;

/* Internal variables */
#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    const char* sh_rotate_krivanek_y_vp_template_body = "#version 420 core\n"
                                                        "\n"
                                                        "#define N_BANDS (%d)\n"
                                                        "\n"
                                                        /** Here goes Y rotation body */
                                                        "%s\n"
                                                        /* And here it ends */
                                                        "\n"
                                                        "uniform float         y_angle;\n"
                                                        "uniform samplerBuffer input;\n"
                                                        "out     result\n"
                                                        "{\n"
                                                        /** !!! */ DO NOT output so much data in one go. This will not work correctly with NVIDIA driver. Fix if needed. /* !!! */
                                                        "    vec3 rotated_sh[N_BANDS * N_BANDS];\n"
                                                        "} Result;\n"
                                                        "\n"
                                                        "void main()\n"
                                                        "{\n"
                                                        "    for (int n = 0; n < N_BANDS * N_BANDS; n++)\n"
                                                        "    {\n"
                                                        "        Result.rotated_sh[n] = texelFetch(input, n).xyz;\n"
                                                        "    }\n"
                                                        "    \n"
                                                        "    sh_rot_krivanek_y(Result.rotated_sh, y_angle);\n"
                                                        "}\n";
#endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

const char* sh_rotate_xyz_vp_body = "#version 420 core\n"
                                    "\n"
                                    "#define N_BANDS (%d)\n"
                                    "\n"
                                    glsl_embeddable_rot_xyz
                                    "\n"
                                    "uniform float         x_angle;\n"
                                    "uniform float         y_angle;\n"
                                    "uniform float         z_angle;\n"
                                    "uniform samplerBuffer input;\n"
                                    "out     result\n"
                                    "{\n"
                                    "    vec3 rotated_sh_coeff;\n"
                                    "} Result;\n"
                                    "\n"
                                    "void main()\n"
                                    "{\n"
                                    "    vec4 input_data [N_BANDS * N_BANDS];\n"
                                    "    vec4 output_data[N_BANDS * N_BANDS];\n"
                                    "\n"
                                    "    for (int n = 0; n < N_BANDS * N_BANDS; n++)\n"
                                    "    {\n"
                                    "        input_data[n] = texelFetch(input, n);\n"
                                    "    }\n"
                                    "    \n"
                                    "    sh_rot_xyz(input_data, output_data, x_angle, y_angle, z_angle);\n"
                                    "\n"
                                    "    Result.rotated_sh_coeff = output_data[gl_VertexID].xyz;\n"
                                    "}\n";

const char* sh_rotate_z_vp_template_body = "#version 420 core\n"
                                           "\n"
                                           "#define N_BANDS (%d)\n"
                                           "\n"
                                           /** Here goes Z rotation body */
                                           "%s\n"
                                           /* And here it ends */
                                           "\n"
                                           "uniform float         z_angle;\n"
                                           "uniform samplerBuffer input;\n"
                                           "out     result\n"
                                           "{\n"
                                           /** !!! DO NOT output so many data in one go. This will not work correctly with NVIDIA driver. Fix if needed. !!! */
                                           "    vec4 rotated_sh_coeff[N_BANDS * N_BANDS];\n"
                                           "} Result;\n"
                                           "\n"
                                           "void main()\n"
                                           "{\n"
                                           "    for (int n = 0; n < N_BANDS * N_BANDS; n++)\n"
                                           "    {\n"
                                           "        Result.rotated_sh_coeff[n] = texelFetch(input, n);\n"
                                           "    }\n"
                                           "    \n"
                                           "    sh_rot_z(Result.rotated_sh_coeff, z_angle);\n"
                                           "}\n";


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(sh_rot, sh_rot, _sh_rot);

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _sh_rot_verify_context_type(__in __notnull ogl_context);
#else
    #define _sh_rot_verify_context_type(x)
#endif

/** Private functions */
/** TODO */
PRIVATE void _sh_rot_create_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_rot*                                                  data_ptr         = (_sh_rot*) arg;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Plain Z axis rotation */
    {
        data_ptr->rot_z_program       = ogl_program_create(data_ptr->context,                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot z") );
        data_ptr->rot_z_vertex_shader = ogl_shader_create (data_ptr->context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot z") );

        /* Create "z rotation" vertex shader body */
        uint32_t rot_z_body_length = strlen(sh_rotate_z_vp_template_body) + strlen(glsl_embeddable_rot_z) + 10;
        char*    rot_z_body        = new (std::nothrow) char[rot_z_body_length];

        ASSERT_DEBUG_SYNC(rot_z_body != NULL, "Out of memory");
        if (rot_z_body != NULL)
        {
            sprintf_s(rot_z_body, rot_z_body_length, sh_rotate_z_vp_template_body, data_ptr->n_bands, glsl_embeddable_rot_z);
        }

        /* Create program */
        const char* tf_output_data[] = {"result.rotated_sh_coeff"};

        entry_points->pGLShaderSource(ogl_shader_get_id(data_ptr->rot_z_vertex_shader),
                                      1,
                                      &rot_z_body,
                                      NULL);

        ogl_program_attach_shader(data_ptr->rot_z_program, data_ptr->rot_z_vertex_shader);

        entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data_ptr->rot_z_program),
                                                   sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                                   tf_output_data,
                                                   GL_INTERLEAVED_ATTRIBS);

        ogl_program_link(data_ptr->rot_z_program);

        /* Retrieve program uniforms */
        const ogl_program_uniform_descriptor* input_descriptor   = NULL;
        const ogl_program_uniform_descriptor* z_angle_descriptor = NULL;

        ogl_program_get_uniform_by_name(data_ptr->rot_z_program, system_hashed_ansi_string_create("input"),   &input_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_z_program, system_hashed_ansi_string_create("z_angle"), &z_angle_descriptor);

        data_ptr->rot_z_program_input_location   = input_descriptor->location;
        data_ptr->rot_z_program_z_angle_location = z_angle_descriptor->location;

        /* Release the body */
        delete [] rot_z_body;
    }

    /* XYZ rotation */
    {
        data_ptr->rot_xyz_program       = ogl_program_create(data_ptr->context,                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot xyz") );
        data_ptr->rot_xyz_vertex_shader = ogl_shader_create (data_ptr->context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot xyz") );

        /* Create "z rotation" vertex shader body */
        uint32_t rot_xyz_body_length = strlen(sh_rotate_xyz_vp_body) + 10;
        char*    rot_xyz_body        = new (std::nothrow) char[rot_xyz_body_length];

        ASSERT_DEBUG_SYNC(rot_xyz_body != NULL, "Out of memory");
        if (rot_xyz_body != NULL)
        {
            sprintf_s(rot_xyz_body, rot_xyz_body_length, sh_rotate_xyz_vp_body, data_ptr->n_bands);
        }

        /* Create program */
        const char* tf_output_data[] = {"result.rotated_sh_coeff"};

        entry_points->pGLShaderSource(ogl_shader_get_id(data_ptr->rot_xyz_vertex_shader),
                                      1,
                                      &rot_xyz_body,
                                      NULL);

        ogl_program_attach_shader(data_ptr->rot_xyz_program, data_ptr->rot_xyz_vertex_shader);

        entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data_ptr->rot_xyz_program),
                                                   sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                                   tf_output_data,
                                                   GL_INTERLEAVED_ATTRIBS);

        ogl_program_link(data_ptr->rot_xyz_program);

        /* Retrieve program uniforms */
        const ogl_program_uniform_descriptor* input_descriptor   = NULL;
        const ogl_program_uniform_descriptor* x_angle_descriptor = NULL;
        const ogl_program_uniform_descriptor* y_angle_descriptor = NULL;
        const ogl_program_uniform_descriptor* z_angle_descriptor = NULL;

        ogl_program_get_uniform_by_name(data_ptr->rot_xyz_program, system_hashed_ansi_string_create("input"),   &input_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_xyz_program, system_hashed_ansi_string_create("x_angle"), &x_angle_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_xyz_program, system_hashed_ansi_string_create("y_angle"), &y_angle_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_xyz_program, system_hashed_ansi_string_create("z_angle"), &z_angle_descriptor);

        data_ptr->rot_xyz_program_input_location   = input_descriptor->location;
        data_ptr->rot_xyz_program_x_angle_location = x_angle_descriptor->location;
        data_ptr->rot_xyz_program_y_angle_location = y_angle_descriptor->location;
        data_ptr->rot_xyz_program_z_angle_location = z_angle_descriptor->location;

        /* Release the body */
        delete [] rot_xyz_body;
    }

    #ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    {
        data_ptr->rot_y_krivanek_program       = ogl_program_create(data_ptr->context,                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot y krivanek") );
        data_ptr->rot_y_krivanek_vertex_shader = ogl_shader_create (data_ptr->context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data_ptr->name), " rot y krivanek") );

        /* Create "y rotation" vertex shader body */
        uint32_t rot_y_body_length = strlen(sh_rotate_krivanek_y_vp_template_body) + strlen(glsl_embeddable_rot_krivanek_y) + 10;
        char*    rot_y_body        = new (std::nothrow) char[rot_y_body_length];

        ASSERT_DEBUG_SYNC(rot_y_body != NULL, "Out of memory");
        if (rot_y_body != NULL)
        {
            sprintf_s(rot_y_body, rot_y_body_length, sh_rotate_krivanek_y_vp_template_body, data_ptr->n_bands, glsl_embeddable_rot_krivanek_y);
        }

        /* Create program */
        const char* tf_output_data[] = {"result.rotated_sh"};

        entry_points->pGLShaderSource(ogl_shader_get_id(data_ptr->rot_y_krivanek_vertex_shader),
                                      1,
                                      &rot_y_body,
                                      NULL);

        ogl_program_attach_shader(data_ptr->rot_y_krivanek_program, data_ptr->rot_y_krivanek_vertex_shader);

        entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data_ptr->rot_y_krivanek_program),
                                                   sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                                   tf_output_data,
                                                   GL_INTERLEAVED_ATTRIBS);

        ogl_program_link(data_ptr->rot_y_krivanek_program);

        /* Retrieve program uniforms */
        const ogl_program_uniform_descriptor* ddy_diagonal_descriptor   = NULL;
        const ogl_program_uniform_descriptor* dy_subdiagonal_descriptor = NULL;
        const ogl_program_uniform_descriptor* input_descriptor          = NULL;
        const ogl_program_uniform_descriptor* y_angle_descriptor        = NULL;

        ogl_program_get_uniform_by_name(data_ptr->rot_y_krivanek_program, system_hashed_ansi_string_create("dy_subdiagonal"), &dy_subdiagonal_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_y_krivanek_program, system_hashed_ansi_string_create("ddy_diagonal"),   &ddy_diagonal_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_y_krivanek_program, system_hashed_ansi_string_create("input"),          &input_descriptor);
        ogl_program_get_uniform_by_name(data_ptr->rot_y_krivanek_program, system_hashed_ansi_string_create("y_angle"),        &y_angle_descriptor);

        data_ptr->rot_y_krivanek_program_ddy_diagonal_location   = ddy_diagonal_descriptor->location;
        data_ptr->rot_y_krivanek_program_dy_subdiagonal_location = dy_subdiagonal_descriptor->location;
        data_ptr->rot_y_krivanek_program_input_location          = input_descriptor->location;
        data_ptr->rot_y_krivanek_program_y_angle_location        = y_angle_descriptor->location;

        /* Release the body */
        delete [] rot_y_body;

        /* Create diagonal+subdiagonal buffer object id. Don't use any content - we will be pushing it per-frame */
        uint32_t n_coeffs = data_ptr->n_bands * data_ptr->n_bands;

        entry_points->pGLGenBuffers            (1,                                                 &data_ptr->rot_y_krivanek_ddy_diagonal_buffer_id);
        entry_points->pGLGenBuffers            (1,                                                 &data_ptr->rot_y_krivanek_dy_subdiagonal_buffer_id);
        dsa_entry_points->pGLNamedBufferDataEXT(data_ptr->rot_y_krivanek_ddy_diagonal_buffer_id,   sizeof(float) * n_coeffs, NULL, GL_DYNAMIC_COPY);
        dsa_entry_points->pGLNamedBufferDataEXT(data_ptr->rot_y_krivanek_dy_subdiagonal_buffer_id, sizeof(float) * n_coeffs, NULL, GL_DYNAMIC_COPY);

        /* TBOs */
        entry_points->pGLGenTextures(1, &data_ptr->rot_y_krivanek_ddy_diagonal_tbo_id);
        entry_points->pGLGenTextures(1, &data_ptr->rot_y_krivanek_dy_subdiagonal_tbo_id);

        dsa_entry_points->pGLTextureBufferEXT(data_ptr->rot_y_krivanek_ddy_diagonal_tbo_id,   GL_TEXTURE_BUFFER, GL_R32F, data_ptr->rot_y_krivanek_ddy_diagonal_buffer_id);
        dsa_entry_points->pGLTextureBufferEXT(data_ptr->rot_y_krivanek_dy_subdiagonal_tbo_id, GL_TEXTURE_BUFFER, GL_R32F, data_ptr->rot_y_krivanek_dy_subdiagonal_buffer_id);
    }
    #endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

    /* generate vaa */
    entry_points->pGLGenVertexArrays(1, &data_ptr->vaa_id);
}

/** TODO */
PRIVATE void _sh_rot_release_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_rot*                          rot_ptr      = (_sh_rot*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    #ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    {
        entry_points->pGLDeleteBuffers(1, &rot_ptr->rot_y_krivanek_ddy_diagonal_buffer_id);
        entry_points->pGLDeleteBuffers(1, &rot_ptr->rot_y_krivanek_dy_subdiagonal_buffer_id);

        ogl_shader_release (rot_ptr->rot_y_krivanek_vertex_shader);
        ogl_program_release(rot_ptr->rot_y_krivanek_program);
    }
    #endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

    ogl_shader_release (rot_ptr->rot_xyz_vertex_shader);
    ogl_shader_release (rot_ptr->rot_z_vertex_shader);
    ogl_program_release(rot_ptr->rot_xyz_program);
    ogl_program_release(rot_ptr->rot_z_program);
}

/** TODO */
PRIVATE void _sh_rot_release(__in __notnull __deallocate(mem) void* ptr)
{
    _sh_rot* data_ptr = (_sh_rot*) ptr;
    
    #ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    {
        delete [] data_ptr->rot_y_krivanek_diagonal_subdiagonal_cache;
        data_ptr->rot_y_krivanek_diagonal_subdiagonal_cache = NULL;

        sh_derivative_rotation_matrices_release(data_ptr->rotation_matrices_derivative);
    }
    #endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

    ogl_context_request_callback_from_context_thread(data_ptr->context, _sh_rot_release_callback, data_ptr);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _sh_rot_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "sh_rot is only supported under GL contexts")
    }
#endif


/** TODO */
PUBLIC EMERALD_API sh_rot sh_rot_create(__in __notnull ogl_context               context,
                                        __in           uint32_t                  n_bands,
                                        __in __notnull system_hashed_ansi_string name)
{
    _sh_rot_verify_context_type(context);

    _sh_rot* new_instance = new (std::nothrow) _sh_rot;

    ASSERT_DEBUG_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        new_instance->context = context;
        new_instance->name    = name;
        new_instance->n_bands = n_bands;

        #ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
        {
            /* Precalculate Y rotation derivative matrices */
            uint32_t n_coeffs = new_instance->n_bands * new_instance->n_bands;

            new_instance->rotation_matrices_derivative              = sh_derivative_rotation_matrices_create(new_instance->n_bands, 3);
            new_instance->rot_y_krivanek_diagonal_subdiagonal_cache = new (std::nothrow) float[n_coeffs];
        }
        #endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

        ogl_context_request_callback_from_context_thread(new_instance->context, _sh_rot_create_callback, new_instance);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _sh_rot_release,
                                                       OBJECT_TYPE_SH_ROT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\SH Rotations\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (sh_rot) new_instance;
}

#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    /** TODO */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_krivanek_y(__in __notnull sh_rot rotations,
                                                                            __in           float  angle_radians,
                                                                            __in __notnull GLuint sh_data_tbo_id,
                                                                            __in __notnull GLuint out_rotated_sh_data_bo_id)
    {
        _sh_rot*                                                  rot_ptr          = (_sh_rot*) rotations;
        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
        const ogl_context_gl_entrypoints*                         entry_points     = NULL;

        ogl_context_get_property(rot_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entry_points);
        ogl_context_get_property(rot_ptr->context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                                &entry_points);

        entry_points->pGLBindVertexArray(rot_ptr->vaa_id);

        entry_points->pGLEnable(GL_RASTERIZER_DISCARD);
        {
            uint32_t n_coeffs = rot_ptr->n_bands * rot_ptr->n_bands;

            sh_derivative_rotation_matrices_get_krivanek_ddy_diagonal(rot_ptr->rotation_matrices_derivative,          angle_radians, rot_ptr->rot_y_krivanek_diagonal_subdiagonal_cache);
            dsa_entry_points->pGLNamedBufferSubDataEXT               (rot_ptr->rot_y_krivanek_ddy_diagonal_buffer_id, 0,             sizeof(float) * n_coeffs, rot_ptr->rot_y_krivanek_diagonal_subdiagonal_cache);

            sh_derivative_rotation_matrices_get_krivanek_dy_subdiagonal(rot_ptr->rotation_matrices_derivative,            angle_radians, rot_ptr->rot_y_krivanek_diagonal_subdiagonal_cache);
            dsa_entry_points->pGLNamedBufferSubDataEXT                 (rot_ptr->rot_y_krivanek_dy_subdiagonal_buffer_id, 0,             sizeof(float) * n_coeffs, rot_ptr->rot_y_krivanek_diagonal_subdiagonal_cache);

            entry_points->pGLUseProgram             (ogl_program_get_id(rot_ptr->rot_y_krivanek_program) );
            dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,                                             GL_TEXTURE_BUFFER, rot_ptr->rot_y_krivanek_dy_subdiagonal_tbo_id);
            dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE1,                                             GL_TEXTURE_BUFFER, rot_ptr->rot_y_krivanek_ddy_diagonal_tbo_id);
            dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE2,                                             GL_TEXTURE_BUFFER, sh_data_tbo_id);
            entry_points->pGLUniform1i              (rot_ptr->rot_y_krivanek_program_dy_subdiagonal_location, 0);
            entry_points->pGLUniform1i              (rot_ptr->rot_y_krivanek_program_ddy_diagonal_location,   1);
            entry_points->pGLUniform1i              (rot_ptr->rot_y_krivanek_program_input_location,          2);
            entry_points->pGLUniform1f              (rot_ptr->rot_y_krivanek_program_y_angle_location,        angle_radians);
            entry_points->pGLBindBufferBase         (GL_TRANSFORM_FEEDBACK_BUFFER,                            0, out_rotated_sh_data_bo_id);
            entry_points->pGLBeginTransformFeedback (GL_POINTS);
            {
                entry_points->pGLDrawArrays(GL_POINTS, 0, 1);
            }
            entry_points->pGLEndTransformFeedback();

            entry_points->pGLBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
        }
        entry_points->pGLDisable(GL_RASTERIZER_DISCARD);
    }
#endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

/* Please see header for specifiaction */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_xyz(__in __notnull sh_rot      rotations,
                                                                 __in           float       x_angle_radians,
                                                                 __in           float       y_angle_radians,
                                                                 __in           float       z_angle_radians,
                                                                 __in __notnull ogl_texture sh_data_tbo,
                                                                 __in __notnull GLuint      out_rotated_sh_data_bo_id,
                                                                 __in           GLuint      out_rotated_sh_data_bo_offset,
                                                                 __in           GLuint      out_rotated_sh_data_bo_size)
{
    _sh_rot*                          rot_ptr      = (_sh_rot*) rotations;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(rot_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLBindVertexArray(rot_ptr->vaa_id);

    entry_points->pGLEnable(GL_RASTERIZER_DISCARD);
    {
        const GLuint rot_xyz_program_id = ogl_program_get_id(rot_ptr->rot_xyz_program);

        entry_points->pGLUseProgram             (rot_xyz_program_id);
        entry_points->pGLActiveTexture          (GL_TEXTURE0);
        entry_points->pGLBindTexture            (GL_TEXTURE_BUFFER,
                                                 sh_data_tbo);
        entry_points->pGLProgramUniform1i       (rot_xyz_program_id,
                                                 rot_ptr->rot_xyz_program_input_location,
                                                 0);
        entry_points->pGLProgramUniform1f       (rot_xyz_program_id,
                                                 rot_ptr->rot_xyz_program_x_angle_location,
                                                 x_angle_radians);
        entry_points->pGLProgramUniform1f       (rot_xyz_program_id,
                                                 rot_ptr->rot_xyz_program_y_angle_location,
                                                 y_angle_radians);
        entry_points->pGLProgramUniform1f       (rot_xyz_program_id,
                                                 rot_ptr->rot_xyz_program_z_angle_location,
                                                 z_angle_radians);
        entry_points->pGLBindBufferRange        (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                 0,
                                                 out_rotated_sh_data_bo_id,
                                                 out_rotated_sh_data_bo_offset,
                                                 out_rotated_sh_data_bo_size);
        entry_points->pGLBeginTransformFeedback (GL_POINTS);
        {
            entry_points->pGLDrawArrays(GL_POINTS, 0, 9);
        }
        entry_points->pGLEndTransformFeedback();

        entry_points->pGLBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    }
    entry_points->pGLDisable(GL_RASTERIZER_DISCARD);
}

/** TODO */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_z(__in __notnull sh_rot      rotations,
                                                               __in           float       angle_radians,
                                                               __in __notnull ogl_texture sh_data_tbo,
                                                               __in __notnull GLuint      out_rotated_sh_data_bo_id)
{
    _sh_rot*                          rot_ptr      = (_sh_rot*) rotations;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(rot_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLBindVertexArray(rot_ptr->vaa_id);

    entry_points->pGLEnable(GL_RASTERIZER_DISCARD);
    {
        const GLuint rot_z_program_id = ogl_program_get_id(rot_ptr->rot_z_program);

        entry_points->pGLUseProgram             (rot_z_program_id);
        entry_points->pGLActiveTexture          (GL_TEXTURE0);
        entry_points->pGLBindTexture            (GL_TEXTURE_BUFFER,
                                                 sh_data_tbo);
        entry_points->pGLProgramUniform1i       (rot_z_program_id,
                                                 rot_ptr->rot_z_program_input_location,
                                                 0);
        entry_points->pGLProgramUniform1f       (rot_z_program_id,
                                                 rot_ptr->rot_z_program_z_angle_location,
                                                 angle_radians);
        entry_points->pGLBindBufferBase         (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                 0,
                                                 out_rotated_sh_data_bo_id);
        entry_points->pGLBeginTransformFeedback (GL_POINTS);
        {
            entry_points->pGLDrawArrays(GL_POINTS, 0, 1);
        }
        entry_points->pGLEndTransformFeedback();

        entry_points->pGLBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
    }
    entry_points->pGLDisable(GL_RASTERIZER_DISCARD);
}
