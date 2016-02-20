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
#include "ogl/ogl_program_ub.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_framebuffer.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"

#define DOWNSAMPLE_FACTOR (8)

/* General stuff */
postprocessing_blur_poisson _dof_scheuermann_blur_poisson           = NULL;
ral_framebuffer             _dof_scheuermann_combination_fbo        = NULL;
ral_texture                 _dof_scheuermann_combination_to         = NULL;
ral_framebuffer             _dof_scheuermann_downsample_dst_fbo     = NULL;
ral_framebuffer             _dof_scheuermann_downsample_src_fbo     = NULL;
ral_texture                 _dof_scheuermann_downsampled_blurred_to = NULL;
ral_texture                 _dof_scheuermann_downsampled_to         = NULL;
GLuint                      _dof_scheuermann_vao_id                 = -1;

/* Combination program */
ral_program                 _dof_scheuermann_combination_po                             = NULL;
ogl_program_ub              _dof_scheuermann_combination_po_ub                          = NULL;
ral_buffer                  _dof_scheuermann_combination_po_ub_bo                       = NULL;
GLuint                      _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset     = -1;
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
static void _stage_step_dof_scheuermann_combine_execute(ral_context context,
                                                        uint32_t    frame_index,
                                                        system_time time,
                                                        const int*  rendering_area_px_topdown,
                                                        void*       not_used)
{
    ral_texture                                               background_result_texture               = NULL;
    raGL_texture                                              background_result_texture_raGL          = NULL;
    GLuint                                                    background_result_texture_raGL_id       = 0;
    ogl_context                                               context_gl                              = NULL;
    raGL_framebuffer                                          dof_scheuermann_combination_fbo_raGL    = NULL;
    GLuint                                                    dof_scheuermann_combination_fbo_raGL_id = 0;
    raGL_texture                                              downsampled_blurred_to_raGL             = NULL;
    GLuint                                                    downsampled_blurred_to_raGL_id          = 0;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints                         = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints                             = NULL;
    raGL_texture                                              julia_color_texture_raGL                = NULL;
    GLuint                                                    julia_color_texture_raGL_id             = 0;
    ral_texture                                               julia_depth_texture                     = NULL;
    raGL_texture                                              julia_depth_texture_raGL                = NULL;
    GLuint                                                    julia_depth_texture_raGL_id             = 0;
    const int*                                                output_resolution                       = main_get_output_resolution();

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Bind required textures */
    background_result_texture_raGL = ral_context_get_texture_gl(context,
                                                                stage_step_background_get_result_texture() );
    downsampled_blurred_to_raGL    = ral_context_get_texture_gl(context,
                                                                _dof_scheuermann_downsampled_blurred_to);
    julia_color_texture_raGL       = ral_context_get_texture_gl(context,
                                                                stage_step_julia_get_color_texture() );
    julia_depth_texture_raGL       = ral_context_get_texture_gl(context,
                                                                stage_step_julia_get_depth_texture() );

    dof_scheuermann_combination_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                                          _dof_scheuermann_combination_fbo);

    raGL_texture_get_property(background_result_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &background_result_texture_raGL_id);
    raGL_texture_get_property(downsampled_blurred_to_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &downsampled_blurred_to_raGL_id);
    raGL_texture_get_property(julia_color_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &julia_color_texture_raGL_id);
    raGL_texture_get_property(julia_depth_texture_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &julia_depth_texture_raGL_id);

    raGL_framebuffer_get_property(dof_scheuermann_combination_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &dof_scheuermann_combination_fbo_raGL_id);

    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE0,
                                            GL_TEXTURE_2D,
                                            julia_color_texture_raGL_id);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE1,
                                            GL_TEXTURE_2D,
                                            downsampled_blurred_to_raGL_id);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE2,
                                            GL_TEXTURE_2D,
                                            background_result_texture_raGL_id);
    dsa_entrypoints->pGLBindMultiTextureEXT(GL_TEXTURE3,
                                            GL_TEXTURE_2D,
                                            julia_depth_texture_raGL_id);


    entrypoints->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                    dof_scheuermann_combination_fbo_raGL_id);
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
    raGL_program dof_scheuermann_combination_po_raGL                    = NULL;
    GLuint       dof_scheuermann_combination_po_raGL_id                 = 0;
    raGL_buffer  dof_scheuermann_combination_po_ub_bo_raGL              = NULL;
    GLuint       dof_scheuermann_combination_po_ub_bo_raGL_id           = 0;
    uint32_t     dof_scheuermann_combination_po_ub_bo_raGL_start_offset = -1;
    uint32_t     dof_scheuermann_combination_po_ub_bo_ral_start_offset  = -1;
    uint32_t     dof_scheuermann_combination_po_ub_bo_size              = 0;

    dof_scheuermann_combination_po_raGL       = ral_context_get_program_gl(context,
                                                                           _dof_scheuermann_combination_po);
    dof_scheuermann_combination_po_ub_bo_raGL = ral_context_get_buffer_gl (context,
                                                                           _dof_scheuermann_combination_po_ub_bo);

    raGL_program_get_property(dof_scheuermann_combination_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &dof_scheuermann_combination_po_raGL_id);

    ral_buffer_get_property (_dof_scheuermann_combination_po_ub_bo,
                             RAL_BUFFER_PROPERTY_SIZE,
                            &dof_scheuermann_combination_po_ub_bo_size);
    ral_buffer_get_property (_dof_scheuermann_combination_po_ub_bo,
                             RAL_BUFFER_PROPERTY_START_OFFSET,
                            &dof_scheuermann_combination_po_ub_bo_ral_start_offset);
    raGL_buffer_get_property(dof_scheuermann_combination_po_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                             &dof_scheuermann_combination_po_ub_bo_raGL_id);
    raGL_buffer_get_property(dof_scheuermann_combination_po_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                             &dof_scheuermann_combination_po_ub_bo_raGL_start_offset);

    entrypoints->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                    0, /* index */
                                    dof_scheuermann_combination_po_ub_bo_raGL_id,
                                    dof_scheuermann_combination_po_ub_bo_raGL_start_offset + dof_scheuermann_combination_po_ub_bo_ral_start_offset,
                                    dof_scheuermann_combination_po_ub_bo_size);

    entrypoints->pGLUseProgram(dof_scheuermann_combination_po_raGL_id);
    entrypoints->pGLDrawArrays(GL_TRIANGLE_FAN,
                               0,  /* first */
                               4); /* count */
}

/** TODO */
static void _stage_step_dof_scheuermann_downsample_execute(ral_context context,
                                                           uint32_t    frame_index,
                                                           system_time time,
                                                           const int*  rendering_area_px_topdown,
                                                           void*       not_used)
{
    ogl_context                       context_gl                      = NULL;
    raGL_framebuffer                  downsample_dst_fbo_raGL         = NULL;
    GLuint                            downsample_dst_fbo_raGL_id      = 0;
    raGL_framebuffer                  downsample_src_fbo_raGL         = NULL;
    GLuint                            downsample_src_fbo_raGL_id      = 0;
    const ogl_context_gl_entrypoints* entrypoints                     = NULL;
    const int*                        output_resolution               = main_get_output_resolution();

    const int                         downsampled_output_resolution[] =
    {
        output_resolution[0] / DOWNSAMPLE_FACTOR,
        output_resolution[1] / DOWNSAMPLE_FACTOR
    };

    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* Downsample the texture */
    downsample_dst_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                             _dof_scheuermann_downsample_dst_fbo);
    downsample_src_fbo_raGL = ral_context_get_framebuffer_gl(context,
                                                             _dof_scheuermann_downsample_src_fbo);

    raGL_framebuffer_get_property(downsample_dst_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &downsample_dst_fbo_raGL_id);
    raGL_framebuffer_get_property(downsample_src_fbo_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &downsample_src_fbo_raGL_id);

    entrypoints->pGLBindFramebuffer(GL_READ_FRAMEBUFFER,
                                    downsample_src_fbo_raGL_id);
    entrypoints->pGLBindFramebuffer(GL_DRAW_FRAMEBUFFER,
                                    downsample_dst_fbo_raGL_id);
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
static void _stage_step_dof_scheuermann_preblur_execute(ral_context context,
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
PUBLIC void stage_step_dof_scheuermann_deinit(ral_context context)
{
    ral_framebuffer framebuffers_to_release[] =
    {
        _dof_scheuermann_combination_fbo,
        _dof_scheuermann_downsample_dst_fbo,
        _dof_scheuermann_downsample_src_fbo
    };
    ral_texture textures_to_release[] = 
    {
        _dof_scheuermann_combination_to,
        _dof_scheuermann_downsampled_to,
        _dof_scheuermann_downsampled_blurred_to
    };
    const uint32_t n_framebuffers_to_release = sizeof(framebuffers_to_release) / sizeof(framebuffers_to_release[0]);
    const uint32_t n_textures_to_release     = sizeof(textures_to_release)     / sizeof(textures_to_release    [0]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &_dof_scheuermann_combination_po);
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_FRAMEBUFFER,
                               n_framebuffers_to_release,
                               (const void**) framebuffers_to_release);
    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               n_textures_to_release,
                               (const void**) textures_to_release);

    postprocessing_blur_poisson_release(_dof_scheuermann_blur_poisson);
}

/* Please see header for specification */
PUBLIC ral_framebuffer stage_step_dof_scheuermann_get_combination_fbo()
{
    return _dof_scheuermann_combination_fbo;
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_dof_scheuermann_get_combined_texture()
{
    return _dof_scheuermann_combination_to;
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_dof_scheuermann_get_downsampled_texture()
{
    return _dof_scheuermann_downsampled_to;
}

/* Please see header for specification */
PUBLIC ral_texture stage_step_dof_scheuermann_get_downsampled_blurred_texture()
{
    return _dof_scheuermann_downsampled_blurred_to;
}

/* Please see header for specification */
PUBLIC void stage_step_dof_scheuermann_init(ral_context  context,
                                            ogl_pipeline pipeline,
                                            uint32_t     stage_id)
{
    ogl_context                                               context_gl                      = NULL;
    const ogl_context_gl_entrypoints*                         entrypoints_ptr                 = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* entrypoints_dsa_ptr             = NULL;
    const int*                                                output_resolution               = main_get_output_resolution();
    const int                                                 downsampled_output_resolution[] =
    {
        output_resolution[0] / DOWNSAMPLE_FACTOR,
        output_resolution[1] / DOWNSAMPLE_FACTOR
    };

    /* Set up VAO */
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_BACKEND_CONTEXT,
                            &context_gl);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &entrypoints_dsa_ptr);

    entrypoints_ptr->pGLGenVertexArrays(1, /* n */
                                       &_dof_scheuermann_vao_id);

    /* Set up FBOs */
    ral_framebuffer result_fbos[3];

    ral_context_create_framebuffers(context,
                                    sizeof(result_fbos) / sizeof(result_fbos[0]),
                                    result_fbos);

    _dof_scheuermann_combination_fbo    = result_fbos[0];
    _dof_scheuermann_downsample_dst_fbo = result_fbos[1];
    _dof_scheuermann_downsample_src_fbo = result_fbos[2];

    /* Set up TOs */
    ral_texture_create_info combination_to_create_info;
    ral_texture_create_info downsampled_blurred_to_create_info;
    ral_texture_create_info downsampled_to_create_info;

    combination_to_create_info.base_mipmap_depth      = 1;
    combination_to_create_info.base_mipmap_height     = output_resolution[1];
    combination_to_create_info.base_mipmap_width      = output_resolution[0];
    combination_to_create_info.fixed_sample_locations = true;
    combination_to_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA16_FLOAT;
    combination_to_create_info.name                   = system_hashed_ansi_string_create("DOF Scheuermann combination TO");
    combination_to_create_info.n_layers               = 1;
    combination_to_create_info.n_samples              = 1;
    combination_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    combination_to_create_info.usage                  = RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
    combination_to_create_info.use_full_mipmap_chain  = false;

    downsampled_blurred_to_create_info.base_mipmap_depth      = 1;
    downsampled_blurred_to_create_info.base_mipmap_height     = downsampled_output_resolution[1];
    downsampled_blurred_to_create_info.base_mipmap_width      = downsampled_output_resolution[0];
    downsampled_blurred_to_create_info.fixed_sample_locations = true;
    downsampled_blurred_to_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA16_FLOAT;
    downsampled_blurred_to_create_info.name                   = system_hashed_ansi_string_create("DOF Scheuermann downsampled TO");
    downsampled_blurred_to_create_info.n_layers               = 1;
    downsampled_blurred_to_create_info.n_samples              = 1;
    downsampled_blurred_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    downsampled_blurred_to_create_info.usage                  = RAL_TEXTURE_USAGE_SAMPLED_BIT;
    downsampled_blurred_to_create_info.use_full_mipmap_chain  = false;

    downsampled_to_create_info.base_mipmap_depth      = 1;
    downsampled_to_create_info.base_mipmap_height     = downsampled_output_resolution[1];
    downsampled_to_create_info.base_mipmap_width      = downsampled_output_resolution[0];
    downsampled_to_create_info.fixed_sample_locations = true;
    downsampled_to_create_info.format                 = RAL_TEXTURE_FORMAT_RGBA16_FLOAT;
    downsampled_to_create_info.name                   = system_hashed_ansi_string_create("DOF Scheuermann downsampled blurred TO");
    downsampled_to_create_info.n_layers               = 1;
    downsampled_to_create_info.n_samples              = 1;
    downsampled_to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    downsampled_to_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_DST_BIT |
                                                        RAL_TEXTURE_USAGE_SAMPLED_BIT;
    downsampled_to_create_info.use_full_mipmap_chain  = false;


    ral_texture_create_info to_create_info_items[] =
    {
        combination_to_create_info,
        downsampled_blurred_to_create_info,
        downsampled_to_create_info
    };
    const uint32_t n_to_create_info_items = sizeof(to_create_info_items) / sizeof(to_create_info_items[0]);
    ral_texture    result_tos[n_to_create_info_items];

    ral_context_create_textures(context,
                                n_to_create_info_items,
                                to_create_info_items,
                                result_tos);

    _dof_scheuermann_combination_to         = result_tos[0];
    _dof_scheuermann_downsampled_blurred_to = result_tos[1];
    _dof_scheuermann_downsampled_to         = result_tos[2];


    raGL_texture combination_to_raGL            = NULL;
    GLuint       combination_to_raGL_id         = 0;
    raGL_texture downsampled_blurred_to_raGL    = NULL;
    GLuint       downsampled_blurred_to_raGL_id = 0;
    raGL_texture downsampled_to_raGL            = NULL;
    GLuint       downsampled_to_raGL_id         = 0;

    combination_to_raGL         = ral_context_get_texture_gl(context,
                                                             _dof_scheuermann_combination_to);
    downsampled_blurred_to_raGL = ral_context_get_texture_gl(context,
                                                             _dof_scheuermann_downsampled_blurred_to);
    downsampled_to_raGL         = ral_context_get_texture_gl(context,
                                                             _dof_scheuermann_downsampled_to);

    raGL_texture_get_property(combination_to_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &combination_to_raGL_id);
    raGL_texture_get_property(downsampled_blurred_to_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &downsampled_blurred_to_raGL_id);
    raGL_texture_get_property(downsampled_to_raGL,
                              RAGL_TEXTURE_PROPERTY_ID,
                              &downsampled_to_raGL_id);


    entrypoints_dsa_ptr->pGLTextureParameteriEXT(combination_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_MIN_FILTER,
                                                 GL_LINEAR);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(combination_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_S,
                                                 GL_CLAMP_TO_EDGE);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(combination_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_T,
                                                 GL_CLAMP_TO_EDGE);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_MIN_FILTER,
                                                 GL_LINEAR);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_S,
                                                 GL_CLAMP_TO_EDGE);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_T,
                                                 GL_CLAMP_TO_EDGE);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_blurred_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_MIN_FILTER,
                                                 GL_LINEAR);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_blurred_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_S,
                                                 GL_CLAMP_TO_EDGE);
    entrypoints_dsa_ptr->pGLTextureParameteriEXT(downsampled_blurred_to_raGL_id,
                                                 GL_TEXTURE_2D,
                                                 GL_TEXTURE_WRAP_T,
                                                 GL_CLAMP_TO_EDGE);

    ral_framebuffer_set_attachment_2D(_dof_scheuermann_combination_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0 /* index */,
                                      _dof_scheuermann_combination_to,
                                      0 /* n_mipmap */);
    ral_framebuffer_set_attachment_2D(_dof_scheuermann_downsample_dst_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      _dof_scheuermann_downsampled_to,
                                      0 /* n_mipmap */);
    ral_framebuffer_set_attachment_2D(_dof_scheuermann_downsample_src_fbo,
                                      RAL_FRAMEBUFFER_ATTACHMENT_TYPE_COLOR,
                                      0, /* index */
                                      stage_step_julia_get_color_texture(),
                                      0 /* n_mipmap */);

    /* Set up postprocessor */
    _dof_scheuermann_blur_poisson = postprocessing_blur_poisson_create(context,
                                                                       system_hashed_ansi_string_create("Poison blurrer"),
                                                                       POSTPROCESSING_BLUR_POISSON_BLUR_BLURRINESS_SOURCE_UNIFORM
                                                                       );

    /* Set up combination program */
    ral_shader  combination_fs              = NULL;
    const char* combination_fs_body_parts[] =
    {
        _dof_scheuermann_combination_fragment_shader_preambule,
        _dof_scheuermann_combination_fragment_shader_declarations,
        postprocessing_blur_poisson_get_tap_data_shader_code(),
        _dof_scheuermann_combination_fragment_shader_main
    };
    const uint32_t                  combination_fs_n_body_parts = sizeof(combination_fs_body_parts) /
                                                                  sizeof(combination_fs_body_parts[0]);
    const system_hashed_ansi_string combination_fs_body_has     = system_hashed_ansi_string_create_by_merging_strings(combination_fs_n_body_parts,
                                                                                                                      combination_fs_body_parts);
    shaders_vertex_fullscreen       combination_vs              = shaders_vertex_fullscreen_create(context,
                                                                                                   true, /* export_uv */
                                                                                                   system_hashed_ansi_string_create("DOF Scheuermann combination VS") );

    const ral_shader_create_info combination_fs_create_info =
    {
        system_hashed_ansi_string_create("DOF Scheuermann combination FS"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_program_create_info combination_po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("DOF Scheuermann combination PO")
    };

    ral_context_create_programs(context,
                                1, /* n_create_info_items */
                               &combination_po_create_info,
                               &_dof_scheuermann_combination_po);
    ral_context_create_shaders (context,
                                1, /* n_create_info_items */
                               &combination_fs_create_info,
                               &combination_fs);

    ral_shader_set_property(combination_fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                            &combination_fs_body_has);

    ral_program_attach_shader(_dof_scheuermann_combination_po,
                              combination_fs);
    ral_program_attach_shader(_dof_scheuermann_combination_po,
                              shaders_vertex_fullscreen_get_shader(combination_vs));

    /* Retrieve combination program uniform block data */
    const raGL_program dof_scheuermann_combination_po_raGL    = ral_context_get_program_gl(context,
                                                                                           _dof_scheuermann_combination_po);
    GLuint             dof_scheuermann_combination_po_raGL_id = 0;

    raGL_program_get_property(dof_scheuermann_combination_po_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &dof_scheuermann_combination_po_raGL_id);

    raGL_program_get_uniform_block_by_name(dof_scheuermann_combination_po_raGL,
                                           system_hashed_ansi_string_create("data"),
                                          &_dof_scheuermann_combination_po_ub);
    ogl_program_ub_get_property           (_dof_scheuermann_combination_po_ub,
                                           OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                          &_dof_scheuermann_combination_po_ub_bo);

    /* Retrieve combination program uniform locations */
    const _raGL_program_variable* bg_uniform_raGL_ptr         = NULL;
    const _raGL_program_variable* data_high_uniform_raGL_ptr  = NULL;
    const _raGL_program_variable* data_low_uniform_raGL_ptr   = NULL;
    const _raGL_program_variable* depth_high_uniform_raGL_ptr = NULL;
    const ral_program_variable*   max_coc_px_uniform_ral_ptr = NULL;

    raGL_program_get_uniform_by_name      (dof_scheuermann_combination_po_raGL,
                                           system_hashed_ansi_string_create("bg"),
                                          &bg_uniform_raGL_ptr);
    raGL_program_get_uniform_by_name      (dof_scheuermann_combination_po_raGL,
                                           system_hashed_ansi_string_create("data_high"),
                                          &data_high_uniform_raGL_ptr);
    raGL_program_get_uniform_by_name      (dof_scheuermann_combination_po_raGL,
                                           system_hashed_ansi_string_create("data_low"),
                                          &data_low_uniform_raGL_ptr);
    raGL_program_get_uniform_by_name      (dof_scheuermann_combination_po_raGL,
                                           system_hashed_ansi_string_create("depth_high"),
                                          &depth_high_uniform_raGL_ptr);
    ral_program_get_block_variable_by_name(_dof_scheuermann_combination_po,
                                           system_hashed_ansi_string_create("data"),
                                           system_hashed_ansi_string_create("max_coc_px"),
                                          &max_coc_px_uniform_ral_ptr);

    _dof_scheuermann_combination_po_bg_uniform_location         = (bg_uniform_raGL_ptr         != NULL) ? bg_uniform_raGL_ptr->location            : -1;
    _dof_scheuermann_combination_po_data_high_uniform_location  = (data_high_uniform_raGL_ptr  != NULL) ? data_high_uniform_raGL_ptr->location     : -1;
    _dof_scheuermann_combination_po_data_low_uniform_location   = (data_low_uniform_raGL_ptr   != NULL) ? data_low_uniform_raGL_ptr->location      : -1;
    _dof_scheuermann_combination_po_depth_high_uniform_location = (depth_high_uniform_raGL_ptr != NULL) ? depth_high_uniform_raGL_ptr->location    : -1;
    _dof_scheuermann_combination_po_ub_max_coc_px_ub_offset     = (max_coc_px_uniform_ral_ptr  != NULL) ? max_coc_px_uniform_ral_ptr->block_offset : -1;

    entrypoints_ptr->pGLProgramUniform1i(dof_scheuermann_combination_po_raGL_id,
                                         data_high_uniform_raGL_ptr->location,
                                         0);
    entrypoints_ptr->pGLProgramUniform1i(dof_scheuermann_combination_po_raGL_id,
                                         data_low_uniform_raGL_ptr->location,
                                         1);
    entrypoints_ptr->pGLProgramUniform1i(dof_scheuermann_combination_po_raGL_id,
                                         bg_uniform_raGL_ptr->location,
                                         2);
    entrypoints_ptr->pGLProgramUniform1i(dof_scheuermann_combination_po_raGL_id,
                                         depth_high_uniform_raGL_ptr->location,
                                         3);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               (const void**) &combination_fs);

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
