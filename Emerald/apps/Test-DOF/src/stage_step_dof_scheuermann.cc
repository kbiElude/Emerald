/**
 *
 * DOF test app
 *
 */

#include "shared.h"
#include "include/main.h"
#include "postprocessing/postprocessing_blur_poisson.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "stage_step_background.h"
#include "stage_step_dof_scheuermann.h"
#include "stage_step_julia.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"


#define DOWNSAMPLE_FACTOR (8)

/* General stuff */
postprocessing_blur_poisson _dof_scheuermann_blur_poisson           = NULL;
GLuint                      _dof_scheuermann_combination_fbo_id     = 0;
ogl_texture                 _dof_scheuermann_combination_to         = 0;
GLuint                      _dof_scheuermann_downsample_dst_fbo_id  = 0;
GLuint                      _dof_scheuermann_downsample_src_fbo_id  = 0;
ogl_texture                 _dof_scheuermann_downsampled_blurred_to = NULL;
ogl_texture                 _dof_scheuermann_downsampled_to         = NULL;
GLuint                      _dof_scheuermann_vao_id                 = -1;

/* Combination program */
ogl_program                 _dof_scheuermann_combination_po                             = NULL;
ogl_program_ub              _dof_scheuermann_combination_po_ub                          = NULL;
GLuint                      _dof_scheuermann_combination_po_ub_bo_id                    =  0;
GLuint                      _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset     = -1;
GLuint                      _dof_scheuermann_combination_po_ub_bo_size                  =  0;
GLuint                      _dof_scheuermann_combination_po_ub_bo_start_offset          =  0;
GLuint                      _dof_scheuermann_combination_po_bg_uniform_location         = -1;
GLuint                      _dof_scheuermann_combination_po_data_high_uniform_location  = -1;
GLuint                      _dof_scheuermann_combination_po_data_low_uniform_location   = -1;
GLuint                      _dof_scheuermann_combination_po_depth_high_uniform_location = -1;


static const char* _dof_scheuermann_combination_fragment_shader_preambule = "#version 430 core\n"
                                                                            "\n";
static const char* _dof_scheuermann_combination_fragment_shader_declarations = "uniform sampler2D data_high;\n" /* uses texture unit 0 */
                                                                               "uniform sampler2D data_low;\n"  /* uses texture unit 1 */
                                                                               "uniform sampler2D bg;\n"        /* uses texture unit 2 */
                                                                               "uniform sampler2D depth_high;\n"/* uses texture unit 3 */
                                                                               "\n"
                                                                               "uniform data\n"
                                                                               "{\n"
                                                                               "    float max_coc_px;\n"
                                                                               "};\n"
                                                                               "\n"
                                                                               "out     vec4      result;\n"
                                                                               "in      vec2      uv;\n"
                                                                               "\n";
static const char* _dof_scheuermann_combination_fragment_shader_main = "void main()\n"
                                                                       "{\n"
                                                                       "    const float radius_scale = 0.4;\n"
                                                                       "\n"
                                                                       "    float center_depth     = texture(data_high, uv).a;\n"
                                                                       "    vec2  delta_high       = vec2(1.0f) / textureSize(data_high, 0);\n"
                                                                       "    vec2  delta_low        = vec2(1.0f) / textureSize(data_low,  0);\n"
                                                                       "    float disc_radius_high = abs(center_depth * (max_coc_px * 2) - max_coc_px);\n"
                                                                       "    float disc_radius_low  = disc_radius_high * radius_scale;\n"
                                                                       "    int   n_active_taps    = 0;\n"
                                                                       "\n"
                                                                       "    result = vec4(0.0);\n"
                                                                       "\n"
                                                                       "    for (int n = 0; n < taps.length(); n += 2)\n"
                                                                       "    {\n"
                                                                       "        vec2  tap_low_uv  = uv + (delta_low  * vec2(taps[n], taps[n+1]) * disc_radius_low);\n"
                                                                       "        vec2  tap_high_uv = uv + (delta_high * vec2(taps[n], taps[n+1]) * disc_radius_high);\n"
                                                                       "        vec4  tap_low     = texture(data_low,  tap_low_uv);\n"
                                                                       "        vec4  tap_high    = texture(data_high, tap_high_uv);\n"
                                                                       "        float tap_blur    = abs(tap_high.a * 2 - 1);\n"
                                                                       "        vec4  tap         = mix(tap_high, tap_low, tap_blur);\n"
                                                                       "\n"
                                                                       "        tap.a = (tap.a >= center_depth) ? 1.0 : abs(tap.a * 2 - 1);\n"
                                                                       "\n"
                                                                       "        result.rgb += tap.rgb * tap.a;\n"
                                                                       "        result.a   += tap.a;\n"
                                                                       "    }\n"
                                                                       "\n"
                                                                       "    result.rgb /= result.a;\n"
                                                                       "\n"
                                                                       "    result += texture(bg, uv) * ((texture(depth_high, uv).x < 1) ? 0.0 : 1.0);\n"
                                                                       "}\n";

/** TODO */
static void _stage_step_dof_scheuermann_combine_execute(ogl_context context,
                                                        uint32_t    frame_index,
                                                        system_time time,
                                                        const int*  rendering_area_px_topdown,
                                                        void*       not_used)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints   = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints       = NULL;
    const int*                                                output_resolution = main_get_output_resolution();

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Bind required textures */
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_2D,
                                            stage_step_julia_get_color_texture());
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE1,
                                            GL_TEXTURE_2D,
                                            _dof_scheuermann_downsampled_blurred_to);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE2,
                                            GL_TEXTURE_2D,
                                            stage_step_background_get_result_texture() );
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE3,
                                            GL_TEXTURE_2D,
                                            stage_step_julia_get_depth_texture() );

    entrypoints->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                    _dof_scheuermann_combination_fbo_id);
    entrypoints->pGLBindVertexArray(_dof_scheuermann_vao_id);

    /* Uniforms! */
    const  float max_coc_px = main_get_max_coc_px();

    ogl_program_ub_set_nonarrayed_uniform_value(_dof_scheuermann_combination_po_ub,
                                                _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset,
                                               &max_coc_px,
                                                0, /* src_data_flags */
                                                sizeof(float) );

    ogl_program_ub_sync(_dof_scheuermann_combination_po_ub);

    /* Go on */
    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                    _dof_scheuermann_combination_po_ub_bo_id,
                                    _dof_scheuermann_combination_po_ub_bo_start_offset,
                                    _dof_scheuermann_combination_po_ub_bo_size);

    entrypoints->pGLUseProgram(ogl_program_get_id(_dof_scheuermann_combination_po) );
    entrypoints->pGLDrawArrays(GL_TRIANGLE_FAN,
                               0,  /* first */
                               4); /* count */
}

/** TODO */
static void _stage_step_dof_scheuermann_downsample_execute(ogl_context context,
                                                           uint32_t    frame_index,
                                                           system_time time,
                                                           const int*  rendering_area_px_topdown,
                                                           void*       not_used)
{
    const ogl_context_gl_entrypoints* entrypoints                     = NULL;
    const int*                        output_resolution               = main_get_output_resolution();
    const int                         downsampled_output_resolution[] =
    {
        output_resolution[0] / DOWNSAMPLE_FACTOR,
        output_resolution[1] / DOWNSAMPLE_FACTOR
    };

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Downsample the texture */
    entrypoints->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                    _dof_scheuermann_downsample_src_fbo_id);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    _dof_scheuermann_downsample_dst_fbo_id);
    entrypoints->pGLBlitFramebuffer(0, /* srcX0 */
                                    0, /* srcY0 */
                                    output_resolution[0],
                                    output_resolution[1],
                                    0, /* dstX0 */
                                    0, /* dstY0 */
                                    downsampled_output_resolution[0],
                                    downsampled_output_resolution[1],
                                    GL_COLOR_BUFFER_BIT,
                                    GL_LINEAR);
}

/** TODO */
static void _stage_step_dof_scheuermann_preblur_execute(ogl_context context,
                                                        uint32_t    frame_index,
                                                        system_time time,
                                                        const int*  rendering_area_px_topdown,
                                                        void*       not_used)
{
    postprocessing_blur_poisson_execute(_dof_scheuermann_blur_poisson,
                                        _dof_scheuermann_downsampled_to,
                                        main_get_blur_radius(),
                                        _dof_scheuermann_downsampled_blurred_to);
}

/* Please see header for specification */
PUBLIC void stage_step_dof_scheuermann_deinit(ogl_context context)
{
    ogl_program_release                (_dof_scheuermann_combination_po);
    ogl_texture_release                (_dof_scheuermann_combination_to);
    ogl_texture_release                (_dof_scheuermann_downsampled_to);
    ogl_texture_release                (_dof_scheuermann_downsampled_blurred_to);
    postprocessing_blur_poisson_release(_dof_scheuermann_blur_poisson);
}

/* Please see header for specification */
PUBLIC GLuint stage_step_dof_scheuermann_get_combination_fbo_id()
{
    return _dof_scheuermann_combination_fbo_id;
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_combined_texture()
{
    return _dof_scheuermann_combination_to;
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_downsampled_texture()
{
    return _dof_scheuermann_downsampled_to;
}

/* Please see header for specification */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_downsampled_blurred_texture()
{
    return _dof_scheuermann_downsampled_blurred_to;
}

/* Please see header for specification */
PUBLIC void stage_step_dof_scheuermann_init(ogl_context  context,
                                            ogl_pipeline pipeline,
                                            uint32_t     stage_id)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints                 = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints                     = NULL;
    const int*                                                output_resolution               = main_get_output_resolution();
    const int                                                 downsampled_output_resolution[] =
    {
        output_resolution[0] / DOWNSAMPLE_FACTOR,
        output_resolution[1] / DOWNSAMPLE_FACTOR
    };

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Set up VAO */
    entrypoints->pGLGenVertexArrays(1, /* n */
                                   &_dof_scheuermann_vao_id);

    /* Set up FBOs */
    entrypoints->pGLGenFramebuffers(1, /* n */
                                   &_dof_scheuermann_combination_fbo_id);
    entrypoints->pGLGenFramebuffers(1, /* n */
                                   &_dof_scheuermann_downsample_dst_fbo_id);
    entrypoints->pGLGenFramebuffers(1, /* n */
                                   &_dof_scheuermann_downsample_src_fbo_id);

    _dof_scheuermann_combination_to         = ogl_texture_create_empty(context,
                                                                       system_hashed_ansi_string_create("DOF Scheuermann combination TO"));
    _dof_scheuermann_downsampled_to         = ogl_texture_create_empty(context,
                                                                       system_hashed_ansi_string_create("DOF Scheuermann downsampled TO"));
    _dof_scheuermann_downsampled_blurred_to = ogl_texture_create_empty(context,
                                                                       system_hashed_ansi_string_create("DOF Scheuermann downsampled blurred TO"));

    dsa_entrypoints->pGLTextureStorage2DEXT(_dof_scheuermann_combination_to,
                                            GL_TEXTURE_2D,
                                            1,  /* levels */
                                            GL_RGBA16F,
                                            output_resolution[0],
                                            output_resolution[1]);
    dsa_entrypoints->pGLTextureStorage2DEXT(_dof_scheuermann_downsampled_to,
                                            GL_TEXTURE_2D,
                                            1, /* levels */
                                            GL_RGBA16F,
                                            downsampled_output_resolution[0],
                                            downsampled_output_resolution[1]);
    dsa_entrypoints->pGLTextureStorage2DEXT(_dof_scheuermann_downsampled_blurred_to,
                                            GL_TEXTURE_2D,
                                            1, /* levels */
                                            GL_RGBA16F,
                                            downsampled_output_resolution[0],
                                            downsampled_output_resolution[1]);

    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_combination_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_LINEAR);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_combination_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_S,
                                             GL_CLAMP_TO_EDGE);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_combination_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_T,
                                             GL_CLAMP_TO_EDGE);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_LINEAR);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_S,
                                             GL_CLAMP_TO_EDGE);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_T,
                                             GL_CLAMP_TO_EDGE);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_blurred_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_LINEAR);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_blurred_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_S,
                                             GL_CLAMP_TO_EDGE);
    dsa_entrypoints->pGLTextureParameteriEXT(_dof_scheuermann_downsampled_blurred_to,
                                             GL_TEXTURE_2D,
                                             GL_TEXTURE_WRAP_T,
                                             GL_CLAMP_TO_EDGE);

    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_dof_scheuermann_combination_fbo_id,
                                                     GL_COLOR_ATTACHMENT0,
                                                     GL_TEXTURE_2D,
                                                     _dof_scheuermann_combination_to,
                                                     0); /* level */
    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_dof_scheuermann_downsample_dst_fbo_id,
                                                     GL_COLOR_ATTACHMENT0,
                                                     GL_TEXTURE_2D,
                                                     _dof_scheuermann_downsampled_to,
                                                     0); /* level */
    dsa_entrypoints->pGLNamedFramebufferTexture2DEXT(_dof_scheuermann_downsample_src_fbo_id,
                                                     GL_COLOR_ATTACHMENT0,
                                                     GL_TEXTURE_2D,
                                                     stage_step_julia_get_color_texture(),
                                                     0); /* level */

    /* Set up postprocessor */
    _dof_scheuermann_blur_poisson = postprocessing_blur_poisson_create(context,
                                                                       system_hashed_ansi_string_create("Poison blurrer"),
                                                                       POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM
                                                                       );

    /* Set up combination program */
    ogl_shader                combination_fs = ogl_shader_create               (context,
                                                                                RAL_SHADER_TYPE_FRAGMENT,
                                                                                system_hashed_ansi_string_create("DOF Scheuermann combination FS") );
    shaders_vertex_fullscreen combination_vs = shaders_vertex_fullscreen_create(context,
                                                                                true, /* export_uv */
                                                                                system_hashed_ansi_string_create("DOF Scheuermann combination VS") );

    const char* combination_fs_body_parts[] =
    {
        _dof_scheuermann_combination_fragment_shader_preambule,
        _dof_scheuermann_combination_fragment_shader_declarations,
        postprocessing_blur_poisson_get_tap_data_shader_code(),
        _dof_scheuermann_combination_fragment_shader_main
    };
    const uint32_t combination_fs_n_body_parts = sizeof(combination_fs_body_parts) /
                                                 sizeof(combination_fs_body_parts[0]);

    _dof_scheuermann_combination_po = ogl_program_create(context,
                                                         system_hashed_ansi_string_create("DOF Scheuermann combination PO"),
                                                         OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_shader_set_body(combination_fs,
                        system_hashed_ansi_string_create_by_merging_strings(combination_fs_n_body_parts,
                                                                            combination_fs_body_parts) );

    ogl_shader_compile(combination_fs);

    ogl_program_attach_shader(_dof_scheuermann_combination_po,
                              combination_fs);
    ogl_program_attach_shader(_dof_scheuermann_combination_po,
                              shaders_vertex_fullscreen_get_shader(combination_vs) );

    ogl_program_link(_dof_scheuermann_combination_po);

    /* Retrieve combination program uniform block data */
    ogl_program_get_uniform_block_by_name(_dof_scheuermann_combination_po,
                                          system_hashed_ansi_string_create("data"),
                                         &_dof_scheuermann_combination_po_ub);

    ogl_program_ub_get_property(_dof_scheuermann_combination_po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_dof_scheuermann_combination_po_ub_bo_size);
    ogl_program_ub_get_property(_dof_scheuermann_combination_po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &_dof_scheuermann_combination_po_ub_bo_id);
    ogl_program_ub_get_property(_dof_scheuermann_combination_po_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &_dof_scheuermann_combination_po_ub_bo_start_offset);

    /* Retrieve combination program uniform locations */
    const ogl_program_variable* bg_uniform_descriptor         = NULL;
    const ogl_program_variable* data_high_uniform_descriptor  = NULL;
    const ogl_program_variable* data_low_uniform_descriptor   = NULL;
    const ogl_program_variable* depth_high_uniform_descriptor = NULL;
    const ogl_program_variable* max_coc_px_uniform_descriptor = NULL;

    ogl_program_get_uniform_by_name(_dof_scheuermann_combination_po,
                                    system_hashed_ansi_string_create("bg"),
                                   &bg_uniform_descriptor);
    ogl_program_get_uniform_by_name(_dof_scheuermann_combination_po,
                                    system_hashed_ansi_string_create("data_high"),
                                   &data_high_uniform_descriptor);
    ogl_program_get_uniform_by_name(_dof_scheuermann_combination_po,
                                    system_hashed_ansi_string_create("data_low"),
                                   &data_low_uniform_descriptor);
    ogl_program_get_uniform_by_name(_dof_scheuermann_combination_po,
                                    system_hashed_ansi_string_create("depth_high"),
                                   &depth_high_uniform_descriptor);
    ogl_program_get_uniform_by_name(_dof_scheuermann_combination_po,
                                    system_hashed_ansi_string_create("max_coc_px"),
                                   &max_coc_px_uniform_descriptor);

    _dof_scheuermann_combination_po_bg_uniform_location         = (bg_uniform_descriptor         != NULL) ? bg_uniform_descriptor->location             : -1;
    _dof_scheuermann_combination_po_data_high_uniform_location  = (data_high_uniform_descriptor  != NULL) ? data_high_uniform_descriptor->location      : -1;
    _dof_scheuermann_combination_po_data_low_uniform_location   = (data_low_uniform_descriptor   != NULL) ? data_low_uniform_descriptor->location       : -1;
    _dof_scheuermann_combination_po_depth_high_uniform_location = (depth_high_uniform_descriptor != NULL) ? depth_high_uniform_descriptor->location     : -1;
    _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset     = (max_coc_px_uniform_descriptor != NULL) ? max_coc_px_uniform_descriptor->block_offset : -1;

    entrypoints->pGLProgramUniform1i(ogl_program_get_id(_dof_scheuermann_combination_po),
                                     data_high_uniform_descriptor->location,
                                     0);
    entrypoints->pGLProgramUniform1i(ogl_program_get_id(_dof_scheuermann_combination_po),
                                     data_low_uniform_descriptor->location,
                                     1);
    entrypoints->pGLProgramUniform1i(ogl_program_get_id(_dof_scheuermann_combination_po),
                                     bg_uniform_descriptor->location,
                                     2);
    entrypoints->pGLProgramUniform1i(ogl_program_get_id(_dof_scheuermann_combination_po),
                                     depth_high_uniform_descriptor->location,
                                     3);

    ogl_shader_release               (combination_fs);
    shaders_vertex_fullscreen_release(combination_vs);

    /* Add ourselves to the pipeline */
    ogl_pipeline_stage_step_declaration combine_stage_step;
    ogl_pipeline_stage_step_declaration downsampling_stage_step;
    ogl_pipeline_stage_step_declaration preblur_stage_step;

    combine_stage_step.name              = system_hashed_ansi_string_create("DOF: Combine");
    combine_stage_step.pfn_callback_proc = _stage_step_dof_scheuermann_combine_execute;
    combine_stage_step.user_arg          = NULL;

    downsampling_stage_step.name              = system_hashed_ansi_string_create("DOF: Downsampling");
    downsampling_stage_step.pfn_callback_proc = _stage_step_dof_scheuermann_downsample_execute;
    downsampling_stage_step.user_arg          = NULL;

    preblur_stage_step.name              = system_hashed_ansi_string_create("DOF: Pre-blurring");
    preblur_stage_step.pfn_callback_proc = _stage_step_dof_scheuermann_preblur_execute;
    preblur_stage_step.user_arg          = NULL;

    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                                &downsampling_stage_step);
    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &preblur_stage_step);
    ogl_pipeline_add_stage_step(pipeline,
                                stage_id,
                               &combine_stage_step);
}
