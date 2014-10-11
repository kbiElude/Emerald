/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "sh/sh_projector.h"
#include "sh/sh_rot.h"
#include "sh/sh_samples.h"
#include "system/system_log.h"
#include <sstream>

/* Internal type definitions */
typedef struct
{
    sh_projection_type type;
    GLint              uniform_locations     [SH_PROJECTOR_PROPERTY_LAST]; 
    char               value                 [SH_PROJECTOR_PROPERTY_LAST][sizeof(float)*4]; /* need to keep up to vec4s of floats or ints. fits either way. */
    bool               value_needs_gpu_update[SH_PROJECTOR_PROPERTY_LAST];
} _sh_projector_entry;

typedef struct
{
    GLint       input_data_location;
    GLint       n_sh_coeffs_location;
    ogl_program program;
    GLuint      tbo_bo_id;
    ogl_texture tbo;

} _sh_project_rgbrgb_to_rrggbbXX_program;

typedef struct
{
    sh_components             components;
    ogl_context               context;
    system_hashed_ansi_string name;
    ogl_program               program;
    sh_rot                    rotator;
    sh_samples                samples;

    uint32_t                  rgbrgb_data_bo_offset;
    uint32_t                  rgbrgb_data_bo_size;
    uint32_t                  rrggbb_data_bo_size;
    uint32_t                  rrggbb_data_bo_offset;
    uint32_t                  total_result_bo_size;

    GLuint sh_coeffs_location;
    GLuint theta_phi_location;
    GLuint unit_vecs_location;

    uint32_t             n_projections;
    _sh_projector_entry* projections;  /* has n_projections entries */

    _sh_project_rgbrgb_to_rrggbbXX_program rgbrgb_to_rrggbbXX_converter;

    GLuint      pre_rotation_data_cache_bo_id;
    ogl_texture pre_rotation_data_cache_tbo;
    GLuint      vao_id;
    REFCOUNT_INSERT_VARIABLES

} _sh_projector;

/* Forward declarations */
PRIVATE                        void _sh_projector_add_projection_call_pre              (                                 _sh_projector*       data_ptr, sh_projection_type projection_type, std::stringstream& vs_variables, std::stringstream& vs_body, uint32_t n_projection);
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_convert_rgbXrgbX_to_rrggbb           (__in __notnull                   _sh_projector*       projector_ptr, __in uint32_t result_bo_id, __in uint32_t result_bo_offset);
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_create_callback                      (__in __notnull                   ogl_context          context, void* arg);
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_deinit_rgbrgb_to_rrggbbXX_converter(__in __notnull                   _sh_projector*       data);
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_init_rgbrgb_to_rrggbbXX_converter  (                                 _sh_projector*       data);
PRIVATE                        void _sh_projector_init_sh_projector_entry              (                                 _sh_projector_entry* entry, sh_projection_type projection_type);
PRIVATE                        void _sh_projector_replace_general_datatype_defines     (                                 _sh_projector*       data, std::string& in);
PRIVATE                        void _sh_projector_replace_general_loop_define          (__in __notnull                   _sh_projector*       data, std::string& in);
PRIVATE                        void _sh_projector_replace_general_defines              (                                 _sh_projector*       projector_ptr, std::string& in);
PRIVATE                        void _sh_projector_release_callback                     (__in __notnull                   ogl_context          context, void* arg);
PRIVATE                        void _sh_projector_release                              (__in __notnull __deallocate(mem) void*                ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(sh_projector, sh_projector, _sh_projector);

/* Internal variables */
const char* sh_projector_preamble = "#version 330 core\n"
                                    "\n";

const char* sh_projector_static_variables = "uniform DATATYPE static_colorINDEX;\n";
const char* sh_projector_static_code      = "DATATYPE get_value_static(vec3 input, DATATYPE static_color)\n"
                                            "{\n"
                                            "    return static_color;\n"
                                            "}\n";

const char* sh_projector_cie_overcast_variables = "uniform float zenith_luminanceINDEX;\n";
const char* sh_projector_cie_overcast_body      = "DATATYPE get_value_cie_overcast(vec3 input, float zenith_luminance)\n"
                                                  "{\n"
                                                  "    float theta = acos(input.z);\n"
                                                  "\n"
                                                  "    return DATATYPE(zenith_luminance * (1.0 + 2.0 * sin(theta) ) / 3.0);\n"
                                                  "}\n";

const char* sh_projector_synth_light_variables = "uniform float cutoffINDEX;\n";
const char* sh_projector_synth_light_body      = "DATATYPE get_value_synth_light(vec3 input, float cutoff)\n"
                                                 "{\n"
                                                 "    float phi = acos(input.z);\n"
                                                 "\n"
                                                 "    return ((phi - cutoff) > 0.0) ? DATATYPE(0.1) : DATATYPE(0.0);\n"
                                                 "}\n";

const char* sh_projector_texture_variables = "uniform float     linear_exposureINDEX;\n"
                                             "uniform sampler2D spherical_map_samplerINDEX;\n";
const char* sh_projector_texture_body      = "DATATYPE get_value_texture(vec3 input, float linear_exposure, sampler2D spherical_map_sampler)\n"
                                             "{\n"
                                             "    const float pi_inv = 1.0 / 3.14152965;\n"
                                             "    float       l_inv  = 1.0 / sqrt(dot(input.xy, input.xy));\n"
                                             "    float       r      = pi_inv * acos(input.z) * l_inv;\n"
                                             "    float       u      = input.x * r /* <-1, 1> */ * 0.5 + 0.5 /* <0, 1> */;\n"
                                             "    float       v      = input.y * r /* <-1, 1> */ * 0.5 + 0.5 /* <0, 1> */;\n"
                                             "\n"
                                             "    return texture(spherical_map_sampler, vec2(u, v)).DATATYPE_COMPONENTS * linear_exposure;\n"
                                             "}\n";

const char* sh_projector_general_template_loop = "   for (int n = 0; n < N_SAMPLES; ++n)\n"
                                                 "   {\n"
                                                 "       vec3 unit_vector = texelFetch(unit_vecs, n).xyz;\n"
                                                 "       DATATYPE texel   = GET_VALUE_CALL;\n"
                                                 "\n"
                                                 "       float sh_coeff = texelFetch(sh_coeffs, N_COEFFS * n + gl_VertexID).x;\n"
                                                 "\n"
                                                 "       tmp_result += texel * DATATYPE(sh_coeff);\n"
                                                 "   }\n";
const char* sh_projector_general_template_body = "\n"
                                                 "uniform samplerBuffer sh_coeffs;\n"
                                                 "uniform samplerBuffer theta_phi;\n"
                                                 "uniform samplerBuffer unit_vecs;\n"
                                                 "\n"
                                                 "out result\n"
                                                 "{\n"
                                                 "    vec3 result;\n"
                                                 "} Out;\n"
                                                 "\n"
                                                 "void main()\n"
                                                 "{\n"
                                                 "   const float area = 4.0 * 3.14152965;\n"
                                                 "   DATATYPE    tmp_result = DATATYPE(0);\n"
                                                 "\n"
                                                 "   LOOP\n"
                                                 "\n"
                                                 "   float factor = area / float(N_SAMPLES);\n"
                                                 "\n"
                                                 "   Out.result                     = vec3(0);\n"
                                                 "   Out.result.DATATYPE_COMPONENTS = tmp_result * DATATYPE(factor);\n"
                                                 "}\n";

const char* sh_projector_rgbXrgbX_rrggbb_converter_body = "#version 330 core\n"
                                                          "\n"
                                                          "uniform samplerBuffer input_data;\n"
                                                          "uniform int           n_sh_coeffs;\n"
                                                          "\n"
                                                          "out result\n"
                                                          "{\n"
                                                          "    float result;\n"
                                                          "} Out;\n"
                                                          "\n"
                                                          "\n"
                                                          "void main()\n"
                                                          "{\n"
                                                          "    int vertex_id     = (gl_VertexID - gl_VertexID / 4);\n"
                                                          "    int n_component   = vertex_id % n_sh_coeffs;\n" 
                                                          "    int n_coefficient = vertex_id / n_sh_coeffs;\n"
                                                          "    int src_offset    = n_component * 3 + n_coefficient;\n"
                                                          "\n"
                                                          "    Out.result = texelFetch(input_data, src_offset);\n"
                                                          "}\n";

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _sh_projector_verify_context_type(__in __notnull ogl_context);
#else
    #define _sh_projector_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _sh_projector_add_projection_call_pre(_sh_projector* data_ptr, std::stringstream& vs_variables, std::stringstream& vs_functions, uint32_t n_projection)
{
    sh_projection_type projection_type = data_ptr->projections[n_projection].type;
    const char*        projection_body = (projection_type == SH_PROJECTION_TYPE_STATIC_COLOR)      ? sh_projector_static_code       :
                                         (projection_type == SH_PROJECTION_TYPE_CIE_OVERCAST)      ? sh_projector_cie_overcast_body :
                                         (projection_type == SH_PROJECTION_TYPE_SPHERICAL_TEXTURE) ? sh_projector_texture_body      :
                                         (projection_type == SH_PROJECTION_TYPE_SYNTHETIC_LIGHT)   ? sh_projector_synth_light_body  :
                                         "?!";
    const char*        projection_vars = (projection_type == SH_PROJECTION_TYPE_STATIC_COLOR)      ? sh_projector_static_variables       :
                                         (projection_type == SH_PROJECTION_TYPE_CIE_OVERCAST)      ? sh_projector_cie_overcast_variables :
                                         (projection_type == SH_PROJECTION_TYPE_SPHERICAL_TEXTURE) ? sh_projector_texture_variables      :
                                         (projection_type == SH_PROJECTION_TYPE_SYNTHETIC_LIGHT)   ? sh_projector_synth_light_variables  :
                                         "?!";

    std::stringstream  projection_variables_stringstream;
    std::stringstream  projection_body_stringstream;

    /* Add variables */
    std::stringstream n_projection_stringstream;
    std::string       updated_projection_variables_string;

    n_projection_stringstream         << n_projection;
    projection_variables_stringstream << projection_vars;

    updated_projection_variables_string = projection_variables_stringstream.str().replace(projection_variables_stringstream.str().find("INDEX"), strlen("INDEX"), n_projection_stringstream.str().c_str() );

    vs_variables << updated_projection_variables_string;

    /* Add code if not already there */
    if (vs_functions.str().find(projection_body) == std::string::npos)
    {
        vs_functions << projection_body;
    }
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_convert_rgbXrgbX_to_rrggbb(__in __notnull _sh_projector* projector_ptr,
                                                                             __in           uint32_t       result_bo_id,
                                                                             __in           uint32_t       result_bo_offset)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access*  dsa_entry_points  = NULL;
    const ogl_context_gl_entrypoints*                          entry_points      = NULL;

    ogl_context_get_property(projector_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(projector_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* If we haven't generated a TBO for the input BO yet, do it now */
    if (projector_ptr->rgbrgb_to_rrggbbXX_converter.tbo_bo_id == -1)
    {
        /* Throw an assertion if a TBO for a different BO is already available - we should only perform this action once */
        ASSERT_DEBUG_SYNC(projector_ptr->components >= SH_COMPONENTS_RGB, "RGB/RGBA SH components case only implemented");

        projector_ptr->rgbrgb_to_rrggbbXX_converter.tbo = ogl_texture_create(projector_ptr->context, system_hashed_ansi_string_create_by_merging_two_strings("SH Projector [RGBRGB->RRGGBBxx converter] ",
                                                                                                                                                             system_hashed_ansi_string_get_buffer(projector_ptr->name) ));

        dsa_entry_points->pGLTextureBufferRangeEXT(projector_ptr->rgbrgb_to_rrggbbXX_converter.tbo, 
                                                   GL_TEXTURE_BUFFER,
                                                   GL_R32F,
                                                   result_bo_id,
                                                   projector_ptr->rgbrgb_data_bo_offset,
                                                   projector_ptr->rgbrgb_data_bo_size);

        entry_points->pGLProgramUniform1i(ogl_program_get_id(projector_ptr->rgbrgb_to_rrggbbXX_converter.program),
                                          projector_ptr->rgbrgb_to_rrggbbXX_converter.input_data_location, 
                                          0);
    }

    /* At this point RASTERIZER_DISCARD is enabled, so no need to worry about splatting the color attachment */
    uint32_t additional_offset = 0;
    uint32_t size              = 0;

    sh_projector_get_projection_result_details( (sh_projector) projector_ptr, SH_PROJECTION_RESULT_RRGGBB, &additional_offset, &size);

    entry_points->pGLUseProgram             (ogl_program_get_id(projector_ptr->rgbrgb_to_rrggbbXX_converter.program) );
    dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, projector_ptr->rgbrgb_to_rrggbbXX_converter.tbo);
    entry_points->pGLBindBufferRange        (GL_TRANSFORM_FEEDBACK_BUFFER, 
                                             0,
                                             result_bo_id, 
                                             result_bo_offset + additional_offset,
                                             size);

    entry_points->pGLBeginTransformFeedback (GL_POINTS);
    {
        entry_points->pGLDrawArrays(GL_POINTS, 0, sh_samples_get_amount_of_coeffs(projector_ptr->samples) * 4 /* We need RGBA output representation to support up to 4 bands */);
    }
    entry_points->pGLEndTransformFeedback();
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_create_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_projector*                                             data             = (_sh_projector*) arg;
    const ogl_context_gl_entrypoints_ext_direct_state_access*  dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                          entry_points     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Form general body */
    std::stringstream vs_bodystream;
    std::string       vs_body_string;
    std::stringstream vs_functionsstream;
    std::stringstream vs_variablestream;

    vs_body_string = sh_projector_general_template_body;
    
    vs_bodystream << vs_body_string.c_str();

    /* Iterate through requested projections and create the program body */
    for (uint32_t n = 0; n < data->n_projections; ++n)
    {
        _sh_projector_add_projection_call_pre(data,  vs_variablestream, vs_functionsstream, n);
    }

    /* Replace LOOP with actual calls */
    std::string vs_bodystream_string      = vs_bodystream.str();
    std::string vs_functionsstream_string = vs_functionsstream.str();
    std::string vs_variablestream_string  = vs_variablestream.str();

    _sh_projector_replace_general_loop_define     (data, vs_bodystream_string);
    _sh_projector_replace_general_datatype_defines(data, vs_bodystream_string);
    _sh_projector_replace_general_defines         (data, vs_bodystream_string);
    _sh_projector_replace_general_datatype_defines(data, vs_functionsstream_string);
    _sh_projector_replace_general_defines         (data, vs_functionsstream_string);
    _sh_projector_replace_general_datatype_defines(data, vs_variablestream_string);

    /* Create the actual program */
    const char*        body_parts[]             = {sh_projector_preamble,
                                                   vs_variablestream_string.c_str(),
                                                   vs_functionsstream_string.c_str(),
                                                   vs_bodystream_string.c_str()
                                                  };
    ogl_shader         shader                   = NULL;
    static const char* tf_output_data[]         = {"result.result"};
    
    shader        = ogl_shader_create (data->context, SHADER_TYPE_VERTEX, data->name);
    data->program = ogl_program_create(data->context,                     data->name);

    entry_points->pGLShaderSource(ogl_shader_get_id(shader), sizeof(body_parts) / sizeof(body_parts[0]), body_parts, NULL);
    
    ogl_program_attach_shader(data->program, shader);
    
    entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data->program),
                                               sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                               tf_output_data,
                                               GL_INTERLEAVED_ATTRIBS);

    ogl_program_link(data->program);

    /* Retrieve general uniformn locations */
    const ogl_program_uniform_descriptor* sh_coeffs_descriptor = NULL;
    const ogl_program_uniform_descriptor* theta_phi_descriptor = NULL;
    const ogl_program_uniform_descriptor* unit_vecs_descriptor = NULL;

    ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create("sh_coeffs"), &sh_coeffs_descriptor);
    ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create("theta_phi"), &theta_phi_descriptor);
    ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create("unit_vecs"), &unit_vecs_descriptor);

    if (sh_coeffs_descriptor != NULL)
    {
        data->sh_coeffs_location = sh_coeffs_descriptor->location;
    }
    else
    {
        data->sh_coeffs_location = -1;
    }

    if (theta_phi_descriptor != NULL)
    {
        data->theta_phi_location = theta_phi_descriptor->location;
    }
    else
    {
        data->theta_phi_location = -1;
    }

    if (unit_vecs_descriptor != NULL)
    {
        data->unit_vecs_location = unit_vecs_descriptor->location;
    }
    else
    {
        data->unit_vecs_location = -1;
    }

    /* Retrieve uniform locations for each projection */
    for (uint32_t n = 0; n < data->n_projections; ++n)
    {
        std::stringstream                     cutoff_stringstream;
        std::stringstream                     linear_exposure_stringstream;
        std::stringstream                     spherical_map_sampler_stringstream;
        std::stringstream                     static_color_stringstream;
        std::stringstream                     zenith_luminance_stringstream;

        const ogl_program_uniform_descriptor* cutoff_descriptor                = NULL;
        const ogl_program_uniform_descriptor* linear_exposure_descriptor       = NULL;
        const ogl_program_uniform_descriptor* spherical_map_sampler_descriptor = NULL;
        const ogl_program_uniform_descriptor* static_color_descriptor          = NULL;
        const ogl_program_uniform_descriptor* zenith_luminance_descriptor      = NULL;

        cutoff_stringstream                << "cutoff"                << n;
        linear_exposure_stringstream       << "linear_exposure"       << n;
        spherical_map_sampler_stringstream << "spherical_map_sampler" << n;
        static_color_stringstream          << "static_color"          << n;
        zenith_luminance_stringstream      << "zenith_luminance"      << n;

        ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create(cutoff_stringstream.str().c_str()               ), &cutoff_descriptor);
        ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create(linear_exposure_stringstream.str().c_str()      ), &linear_exposure_descriptor);
        ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create(spherical_map_sampler_stringstream.str().c_str()), &spherical_map_sampler_descriptor);
        ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create(static_color_stringstream.str().c_str()         ), &static_color_descriptor);
        ogl_program_get_uniform_by_name(data->program, system_hashed_ansi_string_create(zenith_luminance_stringstream.str().c_str()     ), &zenith_luminance_descriptor);

        if (cutoff_descriptor != NULL)
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_CUTOFF] = cutoff_descriptor->location;
        }
        else
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_CUTOFF] = -1;
        }

        if (linear_exposure_descriptor != NULL)
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE] = linear_exposure_descriptor->location;
        }
        else
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE] = -1;
        }

        if (spherical_map_sampler_descriptor != NULL)
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_TEXTURE_ID] = spherical_map_sampler_descriptor->location;
        }
        else
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_TEXTURE_ID] = -1;
        }

        if (static_color_descriptor != NULL)
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_COLOR] = static_color_descriptor->location;
        }
        else
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_COLOR] = -1;
        }

        if (zenith_luminance_descriptor != NULL)
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE] = zenith_luminance_descriptor->location;
        }
        else
        {
            data->projections[n].uniform_locations[SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE] = -1;
        }
    }

    /* Also initialize rgbXrgbX->rrggbb converter we need for ogl_uber's mental health */
    _sh_projector_init_rgbrgb_to_rrggbbXX_converter(data);

    /* Create VAO */
    entry_points->pGLGenVertexArrays(1, &data->vao_id);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not generate VAO");    

    /* We will also need a buffer object to hold calculated SH data prior to rotating them. 
     * For rotation purposes, we will need a TBO as well, bound to the BO */
    entry_points->pGLGenBuffers            (1, &data->pre_rotation_data_cache_bo_id);
    dsa_entry_points->pGLNamedBufferDataEXT(data->pre_rotation_data_cache_bo_id, data->total_result_bo_size, NULL, GL_DYNAMIC_COPY);

    data->pre_rotation_data_cache_tbo = ogl_texture_create(context, system_hashed_ansi_string_create_by_merging_two_strings("SH Projector [pre-rotation data cache TBO] ",
                                                                                                                            system_hashed_ansi_string_get_buffer(data->name) ));

    dsa_entry_points->pGLTextureBufferRangeEXT(data->pre_rotation_data_cache_tbo,
                                               GL_TEXTURE_BUFFER_EXT,
                                               GL_RGB32F,
                                               data->pre_rotation_data_cache_bo_id,
                                               data->rgbrgb_data_bo_offset,
                                               data->rgbrgb_data_bo_size);

    /* Release the shader instance - we won't be needing it anymore */
    ogl_shader_release(shader);
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_deinit_rgbrgb_to_rrggbbXX_converter(__in __notnull _sh_projector* data)
{
    /* Release TBO */
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(data->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    ogl_texture_release(data->rgbrgb_to_rrggbbXX_converter.tbo);

    /* Release the program instance */
    ogl_program_release(data->rgbrgb_to_rrggbbXX_converter.program);

    /* Reset uniform locations */
    data->rgbrgb_to_rrggbbXX_converter.input_data_location  = -1;
    data->rgbrgb_to_rrggbbXX_converter.n_sh_coeffs_location = -1;
}

/** TODO */
PRIVATE RENDERING_CONTEXT_CALL void _sh_projector_init_rgbrgb_to_rrggbbXX_converter(_sh_projector* data)
{
    const ogl_context_gl_entrypoints* entry_points     = NULL;
    static const char*                tf_output_data[] = {"result.result"};
    ogl_shader                        vertex_shader    = NULL;

    ogl_context_get_property(data->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Already initialized? Get lost */
    if (data->rgbrgb_to_rrggbbXX_converter.program != NULL)
    {
        return;
    }

    /* Set default values */
    data->rgbrgb_to_rrggbbXX_converter.tbo_bo_id = -1;
    data->rgbrgb_to_rrggbbXX_converter.tbo       = NULL;

    /* Link the conversion program */
    vertex_shader                              = ogl_shader_create (data->context, SHADER_TYPE_VERTEX, system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                                                                               " rgbXrgbX to rrggbbXX"));
    data->rgbrgb_to_rrggbbXX_converter.program = ogl_program_create(data->context,                     system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(data->name),
                                                                                                                                                               " rgbXrgbX to rrggbbXX"));

    entry_points->pGLShaderSource(ogl_shader_get_id(vertex_shader), 1, &sh_projector_rgbXrgbX_rrggbb_converter_body, NULL);
    
    ogl_program_attach_shader(data->rgbrgb_to_rrggbbXX_converter.program, vertex_shader);
    ogl_shader_release       (vertex_shader);

    entry_points->pGLTransformFeedbackVaryings(ogl_program_get_id(data->rgbrgb_to_rrggbbXX_converter.program),
                                               sizeof(tf_output_data) / sizeof(tf_output_data[0]),
                                               tf_output_data,
                                               GL_INTERLEAVED_ATTRIBS);

    ogl_program_link(data->rgbrgb_to_rrggbbXX_converter.program);

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* input_data_descriptor   = NULL;
    const ogl_program_uniform_descriptor* n_sh_coeffs_descriptor  = NULL;

    ogl_program_get_uniform_by_name(data->rgbrgb_to_rrggbbXX_converter.program, system_hashed_ansi_string_create("input_data"),   &input_data_descriptor);
    ogl_program_get_uniform_by_name(data->rgbrgb_to_rrggbbXX_converter.program, system_hashed_ansi_string_create("n_sh_coeffs"),  &n_sh_coeffs_descriptor);

    data->rgbrgb_to_rrggbbXX_converter.input_data_location  = input_data_descriptor->location;
    data->rgbrgb_to_rrggbbXX_converter.n_sh_coeffs_location = n_sh_coeffs_descriptor->location;

    /* Set the uniforms */
    GLuint converter_program_id = ogl_program_get_id(data->rgbrgb_to_rrggbbXX_converter.program);

    entry_points->pGLProgramUniform1i(converter_program_id, data->rgbrgb_to_rrggbbXX_converter.n_sh_coeffs_location, sh_samples_get_amount_of_coeffs(data->samples) );

    ASSERT_DEBUG_SYNC(data->components >= SH_COMPONENTS_RGB, "Only RGB/RGBA SH data supported");
}

/** TODO */
PRIVATE void _sh_projector_init_sh_projector_entry(_sh_projector_entry* entry, sh_projection_type projection_type)
{
    entry->type = projection_type;

    for (uint32_t n = 0; n < SH_PROJECTOR_PROPERTY_LAST; ++n)
    {
        entry->value_needs_gpu_update[n] = true;
    }

    memset(entry->value,             0, sizeof(entry->value) );
    memset(entry->uniform_locations, 0, sizeof(entry->uniform_locations) );
}

/** TODO */
PRIVATE void _sh_projector_replace_general_datatype_defines(_sh_projector* data, std::string& in)
{
    /* Prepare datatype / datatype_components strings */
    const char* datatype            = NULL;
    const char* datatype_components = NULL;    

    switch (data->components)
    {
        case SH_COMPONENTS_RED:
        {
            datatype            = "float";
            datatype_components = "r";

            break;
        }

        case SH_COMPONENTS_RG:
        {
            datatype            = "vec2";
            datatype_components = "rg";

            break;
        }

        case SH_COMPONENTS_RGB:
        {
            datatype            = "vec3";
            datatype_components = "rgb";

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum");
        }
    }

    while (in.find("DATATYPE_COMPONENTS") != std::string::npos)
    {
         in.replace(in.find("DATATYPE_COMPONENTS"), strlen("DATATYPE_COMPONENTS"), datatype_components);
    }

    while (in.find("DATATYPE") != std::string::npos)
    {
        in.replace(in.find("DATATYPE"), strlen("DATATYPE"), datatype);
    }
}

/** TODO */
PRIVATE void _sh_projector_replace_general_defines(_sh_projector* projector_ptr, std::string& in)
{
    uint32_t          n_coeffs  = sh_samples_get_amount_of_coeffs (projector_ptr->samples);
    uint32_t          n_samples = sh_samples_get_amount_of_samples(projector_ptr->samples);
    std::stringstream n_coeffs_stringstream;
    std::stringstream n_samples_stringstream;

    n_coeffs_stringstream  << n_coeffs;
    n_samples_stringstream << n_samples;

    while (in.find("N_COEFFS") != std::string::npos)
    {
        in.replace(in.find("N_COEFFS"),  strlen("N_COEFFS"),  n_coeffs_stringstream.str() );
    }

    while (in.find("N_SAMPLES") != std::string::npos)
    {
        in.replace(in.find("N_SAMPLES"), strlen("N_SAMPLES"), n_samples_stringstream.str());
    }
}

/** TODO */
PRIVATE void _sh_projector_replace_general_loop_define(_sh_projector* data, std::string& in)
{
    std::stringstream call_stringstream;
    std::stringstream processed_stringstream;

    if (data->n_projections > 0)
    {
        /* Add the template */
        processed_stringstream << sh_projector_general_template_loop;

        for (uint32_t n = 0; n < data->n_projections; ++n)
        {
            /* Create the call string stream */
            switch (data->projections[n].type)
            {
                case SH_PROJECTION_TYPE_CIE_OVERCAST:
                {
                    call_stringstream << " + get_value_cie_overcast(unit_vector, zenith_luminance" << n << ")";

                    break;
                }

                case SH_PROJECTION_TYPE_STATIC_COLOR:
                {
                    call_stringstream << " + get_value_static(unit_vector, static_color" << n << ")";

                    break;
                }

                case SH_PROJECTION_TYPE_SYNTHETIC_LIGHT:
                {
                    call_stringstream << " + get_value_synth_light(unit_vector, cutoff" << n << ")";

                    break;
                }

                case SH_PROJECTION_TYPE_SPHERICAL_TEXTURE:
                {
                    call_stringstream << " + get_value_texture(unit_vector, linear_exposure" << n << ", spherical_map_sampler" << n <<")";

                    break;
                }

                default: ASSERT_DEBUG_SYNC(false, "Unrecognized projection type");
            } /* switch (projections[n]) */
        }

        /* Replace the template call in the newly inserted template with actual one */
        std::string processed_stringstream_string = processed_stringstream.str();

        processed_stringstream_string.replace(processed_stringstream_string.find("GET_VALUE_CALL"), strlen("GET_VALUE_CALL"), (data->n_projections > 0) ? call_stringstream.str().c_str() : "");
        processed_stringstream.str(processed_stringstream_string);

        in.replace(in.find("LOOP"), strlen("LOOP"), processed_stringstream.str().c_str() );
    }
    else
    {
        in.replace(in.find("LOOP"), strlen("LOOP"), "");
    }
}

/** TODO */
PRIVATE void _sh_projector_release_callback(__in __notnull ogl_context context, void* arg)
{
    _sh_projector*                    data_ptr     = (_sh_projector*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* Release samples instance */
    sh_samples_release (data_ptr->samples);

    /* Release projector program */
    ogl_program_release(data_ptr->program);

    /* Release conversion programs */
    _sh_projector_deinit_rgbrgb_to_rrggbbXX_converter(data_ptr);

    /* Release rotator */
    if (data_ptr->rotator != NULL)
    {
        sh_rot_release(data_ptr->rotator);
    }

    /* Release buffer and texture objects */
    entry_points->pGLDeleteBuffers (1, &data_ptr->pre_rotation_data_cache_bo_id);

    ogl_texture_release(data_ptr->pre_rotation_data_cache_tbo);

    data_ptr->pre_rotation_data_cache_bo_id  = 0;

    /* Release VAO */
    entry_points->pGLDeleteVertexArrays(1, &data_ptr->vao_id);
}

/** TODO */
PRIVATE void _sh_projector_release(__in __notnull __deallocate(mem) void* ptr)
{
    _sh_projector* data_ptr = (_sh_projector*) ptr;
    
    ogl_context_request_callback_from_context_thread(data_ptr->context, _sh_projector_release_callback, data_ptr);
}

/** TODO */
PRIVATE void _sh_projector_store_rgbrgb_result_as_rrggbb(_sh_projector* projector_ptr, float* src_ptr, float* result_ptr, uint32_t n_coeffs)
{
    /* NOTE: Internally, we always output vec3, so that the result data can be directly fed to uber program. */
    switch (projector_ptr->components)
    {
        case SH_COMPONENTS_RED:
        {
            memcpy(result_ptr, src_ptr, 3 * sizeof(float) * n_coeffs);

            break;
        }

        case SH_COMPONENTS_RG:
        case SH_COMPONENTS_RGB:
        {
            uint32_t n_bands = sh_samples_get_amount_of_bands(projector_ptr->samples);

            ASSERT_DEBUG_SYNC(n_bands <= 4, "Up to 4 SH bands are supported");

            memset(result_ptr, 0, 3 * sizeof(float) * n_coeffs);

            for (uint32_t n = 0; n < 3 * n_coeffs; ++n)
            {
                uint32_t n_channel       = n % 3;       /* vec3 */
                uint32_t n_coeff         = n / 3;       /* vec3 */
                uint32_t rearranged_n    = n_channel * n_coeffs + n_coeff;
                uint32_t n_padding_bytes = (n_bands != 4) ? (rearranged_n / n_bands) : (0);

                result_ptr[rearranged_n + n_padding_bytes] = src_ptr[n];
            }

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized SH components enum value");
        }
    }
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _sh_projector_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "sh_projector is only supported under GL contexts")
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API sh_projector sh_projector_create(__in __notnull ogl_context               context,
                                                    __in __notnull sh_samples                samples,
                                                    __in           sh_components             components,
                                                    __in __notnull system_hashed_ansi_string name,
                                                    __in           uint32_t                  n_projections,
                                                    __in __notnull const sh_projection_type* projections)
{
    _sh_projector_verify_context_type(context);

    /* Instantiate the object */
    _sh_projector* result = new (std::nothrow) _sh_projector;

    ASSERT_DEBUG_SYNC(result != NULL, "Could not instantiate result object.");
    if (result != NULL)
    {
        const uint32_t               rrggbb_data_set_size     = 4 /* rrggbb layout is vec3-based */ * sh_samples_get_amount_of_coeffs(samples) * sizeof(GLfloat);
        const uint32_t               rgbrgb_data_set_size     = 3 /* rgbrgb layout is vec4-based */ * sh_samples_get_amount_of_coeffs(samples) * sizeof(GLfloat);
        const ogl_context_gl_limits* limits_ptr               = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        const GLuint tbo_offset_alignment     = limits_ptr->texture_buffer_offset_alignment;
        uint32_t     rgbrgb_alignment_padding = (tbo_offset_alignment - (rrggbb_data_set_size % tbo_offset_alignment) );

        if (rgbrgb_alignment_padding == tbo_offset_alignment)
        {
            rgbrgb_alignment_padding = 0;
        }

        memset(&result->rgbrgb_to_rrggbbXX_converter, 0, sizeof(result->rgbrgb_to_rrggbbXX_converter) );

        result->components             = components;
        result->context                = context;
        result->name                   = name;
        result->n_projections          = n_projections;
        result->projections            = new (std::nothrow) _sh_projector_entry[n_projections];
        result->rotator                = sh_rot_create(context,
                                                       sh_samples_get_amount_of_bands(samples),
                                                       system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(name),
                                                                                                               " projector rotator") );
        result->samples                = samples;
        
        result->rrggbb_data_bo_offset  = 0;
        result->rrggbb_data_bo_size    = rrggbb_data_set_size;
        result->rgbrgb_data_bo_offset  = rrggbb_data_set_size + rgbrgb_alignment_padding;
        result->rgbrgb_data_bo_size    = rgbrgb_data_set_size;
        result->total_result_bo_size   = rrggbb_data_set_size + rgbrgb_data_set_size;
        result->total_result_bo_size  +=                        rgbrgb_alignment_padding;

        ASSERT_ALWAYS_SYNC(result->projections != NULL, "Out of memory");
        if (result->projections != NULL)
        {
            for (uint32_t n_projection = 0; n_projection < n_projections; ++n_projection)
            {
                _sh_projector_init_sh_projector_entry(result->projections + n_projection, projections[n_projection]);
            }
        }

        sh_samples_retain(samples);
    }

    /* Request renderer callback */
    ogl_context_request_callback_from_context_thread(context, _sh_projector_create_callback, result);

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result, 
                                                   _sh_projector_release,
                                                   OBJECT_TYPE_SH_PROJECTOR,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\SH Projector\\", system_hashed_ansi_string_get_buffer(name)) );

    /* Return the result */
    return (sh_projector) result;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void sh_projector_execute(__in           __notnull   sh_projector projector,
                                                                    __in_ecount(3) __notnull   float*       rotation_angles_radians,
                                                                    __in           __notnull   GLuint       result_bo_id,
                                                                    __in           __notnull   GLuint       result_bo_offset,
                                                                    __in           __maybenull float*       result_bo_ptr)
{
    _sh_projector*                                            projector_ptr    = (_sh_projector*) projector;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const uint32_t                                            n_coeffs         = sh_samples_get_amount_of_coeffs(projector_ptr->samples);

    ogl_context_get_property(projector_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);
    ogl_context_get_property(projector_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLBindVertexArray(projector_ptr->vao_id);
    entry_points->pGLEnable         (GL_RASTERIZER_DISCARD);
    {
        /* Make sure all uniforms are configured correctly */
        GLuint program_id = ogl_program_get_id(projector_ptr->program);

        for (uint32_t n = 0; n < projector_ptr->n_projections; ++n)
        {
            _sh_projector_entry* projection_ptr = projector_ptr->projections + n;

            for (uint32_t n_entry = 0; n_entry < SH_PROJECTOR_PROPERTY_LAST; ++n_entry)
            {
                if (projection_ptr->value_needs_gpu_update[n_entry])
                {
                    switch(n_entry)
                    {
                        case SH_PROJECTOR_PROPERTY_TYPE:
                        {
                            /* Ignore */
                            break;
                        }

                        case SH_PROJECTOR_PROPERTY_CUTOFF:
                        case SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE:
                        case SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE:
                        {
                            /* float1 */
                            entry_points->pGLProgramUniform1f(program_id, projection_ptr->uniform_locations[n_entry], *((GLfloat*) projection_ptr->value[n_entry]));

                            break;
                        }

                        case SH_PROJECTOR_PROPERTY_ROTATION:
                        {
                            /* Used in pre-processing phase - ignore. */
                            break;
                        }

                        case SH_PROJECTOR_PROPERTY_COLOR:
                        {
                            /* datatype-dependent */
                            switch (projector_ptr->components)
                            {
                                case 1:  entry_points->pGLProgramUniform1fv(program_id, projection_ptr->uniform_locations[n_entry], 1, (GLfloat*) projection_ptr->value[n_entry]); break;
                                case 2:  entry_points->pGLProgramUniform2fv(program_id, projection_ptr->uniform_locations[n_entry], 1, (GLfloat*) projection_ptr->value[n_entry]); break;
                                case 3:  entry_points->pGLProgramUniform3fv(program_id, projection_ptr->uniform_locations[n_entry], 1, (GLfloat*) projection_ptr->value[n_entry]); break;
                                case 4:  entry_points->pGLProgramUniform4fv(program_id, projection_ptr->uniform_locations[n_entry], 1, (GLfloat*) projection_ptr->value[n_entry]); break;

                                default: ASSERT_DEBUG_SYNC(false, "Unrecognized components value");
                            }

                            break;
                        }

                        case SH_PROJECTOR_PROPERTY_TEXTURE_ID:
                        {
                            /* sampler2d */
                            entry_points->pGLProgramUniform1i(program_id, projection_ptr->uniform_locations[n_entry], *((GLint*) projection_ptr->value[n_entry]));

                            break;
                        }

                        default: ASSERT_DEBUG_SYNC(false, "Unrecognized property");
                    }

                    projection_ptr->value_needs_gpu_update[n_entry] = false;
                }
            }
        }

        /* Don't forget about the generic uniforms! */
        const GLuint projector_program_id = ogl_program_get_id(projector_ptr->program);

        entry_points->pGLUseProgram             (projector_program_id);
        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                 GL_TEXTURE_BUFFER,
                                                 sh_samples_get_sample_sh_coeffs_tbo(projector_ptr->samples) );
        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE1,
                                                 GL_TEXTURE_BUFFER,
                                                 sh_samples_get_sample_theta_phi_tbo(projector_ptr->samples) );
        dsa_entry_points->pGLBindMultiTextureEXT(GL_TEXTURE2,
                                                 GL_TEXTURE_BUFFER,
                                                 sh_samples_get_sample_unit_vec_tbo (projector_ptr->samples) );
        entry_points->pGLProgramUniform1i       (projector_program_id,
                                                 projector_ptr->sh_coeffs_location,
                                                 0);
        entry_points->pGLProgramUniform1i       (projector_program_id,
                                                 projector_ptr->theta_phi_location,
                                                 1);
        entry_points->pGLProgramUniform1i       (projector_program_id,
                                                 projector_ptr->unit_vecs_location,
                                                 2);
        entry_points->pGLBindBufferRange        (GL_TRANSFORM_FEEDBACK_BUFFER,
                                                 0,
                                                 projector_ptr->pre_rotation_data_cache_bo_id,
                                                 projector_ptr->rgbrgb_data_bo_offset,
                                                 projector_ptr->rgbrgb_data_bo_size);

        /* Glory be */
        entry_points->pGLBeginTransformFeedback (GL_POINTS);
        {
            entry_points->pGLDrawArrays(GL_POINTS, 0, n_coeffs);
        }
        entry_points->pGLEndTransformFeedback();
        entry_points->pGLFinish();

        /* Rotate the data set */
        ASSERT_DEBUG_SYNC(sh_samples_get_amount_of_bands(projector_ptr->samples) == 3, "Only 3 bands saupported for now");

        sh_rot_rotate_xyz(projector_ptr->rotator,
                          rotation_angles_radians[0],
                          rotation_angles_radians[1],
                          rotation_angles_radians[2],
                          projector_ptr->pre_rotation_data_cache_tbo,
                          result_bo_id,
                          projector_ptr->rgbrgb_data_bo_offset, /* keep RGBRGB data offset */
                          projector_ptr->rgbrgb_data_bo_size);  /* keep RGBRGB data size limit */

        /* Output data is in rgbXrgbXrgbX format. ogl_uber implementation expects rrggbb layout so make sure the converted form is also available in our result buffer object */
        _sh_projector_convert_rgbXrgbX_to_rrggbb(projector_ptr, result_bo_id, result_bo_offset);

        /* We're done. */
    }
    entry_points->pGLDisable(GL_RASTERIZER_DISCARD);
    entry_points->pGLBindVertexArray(0);
}

/** Please see header for specification */
PUBLIC EMERALD_API sh_components sh_projector_get_components(__in __notnull sh_projector instance)
{
    return ((_sh_projector*)instance)->components;
}

/* Please see header for specification */
PUBLIC EMERALD_API void sh_projector_get_projection_result_details(__in  __notnull   sh_projector          instance,
                                                                   __in              _sh_projection_result type,
                                                                   __out __maybenull uint32_t*             result_bo_offset,
                                                                   __out __maybenull uint32_t*             result_bo_size)
{
    _sh_projector* projector_ptr = (_sh_projector*) instance;
    uint32_t       offset        = 0;
    uint32_t       size          = 0;

    switch(type)
    {
        case SH_PROJECTION_RESULT_RGBRGB:
        {
            offset = projector_ptr->rgbrgb_data_bo_offset;
            size   = projector_ptr->rgbrgb_data_bo_size;

            break;
        }

        case SH_PROJECTION_RESULT_RRGGBB:
        {
            offset = projector_ptr->rrggbb_data_bo_offset;
            size   = projector_ptr->rrggbb_data_bo_size;

            break;
        }

        default: ASSERT_DEBUG_SYNC(false, "Unrecognized projection result type");
    }

    if (result_bo_offset != NULL)
    {
        *result_bo_offset = offset;
    }

    if (result_bo_size != NULL)
    {
        *result_bo_size = size;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t sh_projector_get_projection_result_total_size(__in __notnull sh_projector instance)
{
    return ((_sh_projector*)instance)->total_result_bo_size;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool sh_projector_set_projection_property(__in __notnull sh_projector          instance,
                                                             __in           uint32_t              n_projection,
                                                             __in           sh_projector_property property,
                                                             __in           const void*           data)
{
    _sh_projector* projector_ptr = (_sh_projector*) instance;
    bool           result        = false;
    uint32_t       n_bytes_taken = 0;

    switch (property)
    {
        case SH_PROJECTOR_PROPERTY_LINEAR_EXPOSURE:
        case SH_PROJECTOR_PROPERTY_CUTOFF:
        case SH_PROJECTOR_PROPERTY_SKY_ZENITH_LUMINANCE:
        {
            n_bytes_taken = sizeof(float);

            break;
        }

        case SH_PROJECTOR_PROPERTY_COLOR:
        {
            n_bytes_taken = sizeof(float) * projector_ptr->components;

            break;
        }

        case SH_PROJECTOR_PROPERTY_TEXTURE_ID:
        {
            n_bytes_taken = sizeof(int);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized SH projection property");

            break;
        }
    }

    if (n_bytes_taken != 0)
    {
        if (memcmp(projector_ptr->projections[n_projection].value[property], data, n_bytes_taken) != 0)
        {
            memcpy(projector_ptr->projections[n_projection].value[property], data, n_bytes_taken);

            projector_ptr->projections[n_projection].value_needs_gpu_update[property] = true;
            result                                                                    = true;
        }
    }

    return result;
}
