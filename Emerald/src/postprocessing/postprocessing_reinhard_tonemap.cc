/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "postprocessing/postprocessing_reinhard_tonemap.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_framebuffer.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "shaders/shaders_fragment_rgb_to_Yxy.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include <algorithm>

#ifdef _WIN32
    #undef max
#endif

/** Internal type definition */
typedef struct
{
    ral_context context;

    ral_texture  downsampled_yxy_texture;
    raGL_texture downsampled_yxy_texture_gl;

    ral_texture  yxy_texture;
    raGL_texture yxy_texture_gl;

    GLuint  dst_framebuffer_id;
    GLuint  src_framebuffer_id;

    shaders_fragment_rgb_to_Yxy rgb_to_Yxy_fragment_shader;
    shaders_vertex_fullscreen   fullscreen_vertex_shader;

    ogl_program rgb_to_Yxy_program;
    GLuint      rgb_to_Yxy_program_tex_uniform_location;

    ogl_program    operator_program;
    GLuint         operator_program_alpha_ub_offset;
    ogl_program_ub operator_program_ub;
    ral_buffer     operator_program_ub_bo;
    GLuint         operator_program_ub_bo_size;
    ogl_shader     operator_fragment_shader;
    GLuint         operator_program_luminance_texture_location;
    GLuint         operator_program_luminance_texture_avg_location;
    GLuint         operator_program_white_level_ub_offset;

    uint32_t texture_width;
    uint32_t texture_height;
    bool     use_crude_downsampled_lum_average_calculation;

    REFCOUNT_INSERT_VARIABLES
} _postprocessing_reinhard_tonemap;

typedef struct
{
    _postprocessing_reinhard_tonemap* data_ptr;

    system_hashed_ansi_string name;

} _create_callback_data;

/** Internal variables */
system_hashed_ansi_string reinhard_tonemap_fragment_shader_body = system_hashed_ansi_string_create("#version 430 core\n"
                                                                                                   "\n"
                                                                                                   "uniform data\n"
                                                                                                   "{\n"
                                                                                                   "    float alpha;\n"
                                                                                                   "    float white_level;\n"
                                                                                                   "};\n"
                                                                                                   "\n"
                                                                                                   "uniform sampler2D luminance_texture;\n"
                                                                                                   "uniform sampler2D luminance_texture_avg;\n"
                                                                                                   "uniform sampler2D rgb_texture;\n"
                                                                                                   "\n"
                                                                                                   "in  vec2 uv;\n"
                                                                                                   "out vec4 result;\n"
                                                                                                   "\n"
                                                                                                   "void main()\n"
                                                                                                   "{\n"
                                                                                                   "    float lum_avg = textureLod(luminance_texture_avg, vec2(0.0, 0.0), 5).x;\n"
                                                                                                   "    vec4  Yxy     = texture   (luminance_texture,     vec2(uv.x, 1-uv.y) );\n"
                                                                                                   "\n"
                                                                                                   "    lum_avg = exp(lum_avg);\n"
                                                                                                   "    Yxy.x   = exp(Yxy.x);\n"
                                                                                                   "\n"
                                                                                                   "    float luminance_scaled = alpha / (lum_avg + 0.001) * Yxy.x;\n"
                                                                                                   "\n"
                                                                                                   "    /* Prepare result XYZA vector */\n"
                                                                                                   "    Yxy.x = luminance_scaled / (1 + luminance_scaled) * (1 + luminance_scaled / pow(white_level, 2.0) );\n"
                                                                                                   "\n"
                                                                                                   "    vec3 XYZ = vec3(Yxy.y * Yxy.x / Yxy.z, Yxy.x, (1.0 - Yxy.y - Yxy.z) * (Yxy.x / Yxy.z));\n"
                                                                                                   "\n"
                                                                                                   "    /* Convert from XYZA to sRGB */\n"
                                                                                                   "    const vec3 X_vector = vec3( 3.2410, -1.5374, -0.4986);\n"
                                                                                                   "    const vec3 Y_vector = vec3(-0.9692,  1.8760,  0.0416);\n"
                                                                                                   "    const vec3 Z_vector = vec3( 0.0556, -0.2040,  1.0570);\n"
                                                                                                   "\n"
                                                                                                   "    result = vec4(dot(X_vector, XYZ), dot(Y_vector, XYZ), dot(Z_vector, XYZ), Yxy.w);\n"
                                                                                                   "}\n");

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_reinhard_tonemap,
                               postprocessing_reinhard_tonemap,
                              _postprocessing_reinhard_tonemap);


/** TODO */
PRIVATE void _create_callback(ogl_context context,
                              void*       arg)
{
    _create_callback_data*            data         = (_create_callback_data*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* TODO */
    ral_texture_create_info         yxy_texture_create_info;
    raGL_texture                    yxy_texture_gl     = NULL;
    GLuint                          yxy_texture_id     = 0;
    bool                            yxy_texture_is_rbo = false;
    const system_hashed_ansi_string yxy_texture_name   = system_hashed_ansi_string_create_by_merging_two_strings("Reinhard tonemap [YXY texture] ",
                                                                                                                 system_hashed_ansi_string_get_buffer(data->name) );

    yxy_texture_create_info.base_mipmap_depth      = 1;
    yxy_texture_create_info.base_mipmap_height     = data->data_ptr->texture_height;
    yxy_texture_create_info.base_mipmap_width      = data->data_ptr->texture_width;
    yxy_texture_create_info.fixed_sample_locations = false;
    yxy_texture_create_info.format                 = RAL_TEXTURE_FORMAT_RGB32_FLOAT;
    yxy_texture_create_info.name                   = yxy_texture_name;
    yxy_texture_create_info.n_layers               = 1;
    yxy_texture_create_info.n_samples              = 1;
    yxy_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    yxy_texture_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_SRC_BIT         |
                                                     RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     RAL_TEXTURE_USAGE_SAMPLED_BIT;
    yxy_texture_create_info.use_full_mipmap_chain  = true;

    ral_context_create_textures(data->data_ptr->context,
                                1, /* n_textures */
                               &yxy_texture_create_info,
                               &data->data_ptr->yxy_texture);

    data->data_ptr->yxy_texture_gl = ral_context_get_texture_gl(data->data_ptr->context,
                                                                data->data_ptr->yxy_texture);

    raGL_texture_get_property(data->data_ptr->yxy_texture_gl,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &yxy_texture_id);
    raGL_texture_get_property(data->data_ptr->yxy_texture_gl,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &yxy_texture_is_rbo);

    ASSERT_DEBUG_SYNC(!yxy_texture_is_rbo,
                      "TODO");

    entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                   yxy_texture_id);
    entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_WRAP_S,
                                   GL_CLAMP_TO_BORDER);
    entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_WRAP_T,
                                   GL_CLAMP_TO_BORDER);
    entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_MAG_FILTER,
                                   GL_LINEAR);
    entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_MIN_FILTER,
                                   GL_LINEAR);

    /* TODO */
    if (data->data_ptr->use_crude_downsampled_lum_average_calculation)
    {
        ral_texture_create_info         downsampled_yxy_texture_create_info;
        GLuint                          downsampled_yxy_texture_id     = 0;
        bool                            downsampled_yxy_texture_is_rbo = false;
        const system_hashed_ansi_string downsampled_yxy_texture_name   = system_hashed_ansi_string_create_by_merging_two_strings("Reinhard tonemap [downsampled YXY texture] ",
                                                                                                                                 system_hashed_ansi_string_get_buffer(data->name) );

        downsampled_yxy_texture_create_info.base_mipmap_depth      = 1;
        downsampled_yxy_texture_create_info.base_mipmap_height     = 64;
        downsampled_yxy_texture_create_info.base_mipmap_width      = 64;
        downsampled_yxy_texture_create_info.fixed_sample_locations = false;
        downsampled_yxy_texture_create_info.format                 = RAL_TEXTURE_FORMAT_RGB32_FLOAT;
        downsampled_yxy_texture_create_info.name                   = downsampled_yxy_texture_name;
        downsampled_yxy_texture_create_info.n_layers               = 1;
        downsampled_yxy_texture_create_info.n_samples              = 1;
        downsampled_yxy_texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
        downsampled_yxy_texture_create_info.usage                  = RAL_TEXTURE_USAGE_BLIT_DST_BIT |
                                                                     RAL_TEXTURE_USAGE_SAMPLED_BIT;
        downsampled_yxy_texture_create_info.use_full_mipmap_chain  = true;

        ral_context_create_textures(data->data_ptr->context,
                                    1, /* n_textures */
                                   &downsampled_yxy_texture_create_info,
                                   &data->data_ptr->downsampled_yxy_texture);

        data->data_ptr->downsampled_yxy_texture_gl = ral_context_get_texture_gl(data->data_ptr->context,
                                                                                data->data_ptr->downsampled_yxy_texture);

        raGL_texture_get_property(data->data_ptr->downsampled_yxy_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_ID,
                                 &downsampled_yxy_texture_id);
        raGL_texture_get_property(data->data_ptr->downsampled_yxy_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                 &downsampled_yxy_texture_is_rbo);

        ASSERT_DEBUG_SYNC(!downsampled_yxy_texture_is_rbo,
                          "TODO");


        entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                       downsampled_yxy_texture_id);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_S,
                                       GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_WRAP_T,
                                       GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MAG_FILTER,
                                       GL_LINEAR);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D,
                                       GL_TEXTURE_MIN_FILTER,
                                       GL_LINEAR);
    }
    else
    {
        data->data_ptr->downsampled_yxy_texture = NULL;
    }

    /* Create a source framebuffer we will be using for blitting purposes */
    entry_points->pGLGenFramebuffers(1,
                                    &data->data_ptr->src_framebuffer_id);

    /* Create destination framebuffer we will be using for blitting purposes */
    entry_points->pGLGenFramebuffers(1,
                                    &data->data_ptr->dst_framebuffer_id);

    /* Create RGB=>Yxy fragment shader */
    data->data_ptr->rgb_to_Yxy_fragment_shader = shaders_fragment_rgb_to_Yxy_create(data->data_ptr->context,
                                                                                    system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap RGB2YXY ",
                                                                                                                                            system_hashed_ansi_string_get_buffer(data->name) ),
                                                                                    true);

    /* Create fullscreen vertex shader */
    data->data_ptr->fullscreen_vertex_shader = shaders_vertex_fullscreen_create(data->data_ptr->context,
                                                                                true,
                                                                                system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap Fullscreen ",
                                                                                                                                        system_hashed_ansi_string_get_buffer(data->name) ));

    /* Create RGB=>Yxy program */
    data->data_ptr->rgb_to_Yxy_program = ogl_program_create(data->data_ptr->context,
                                                            system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap RGB2YXY ",
                                                                                                                    system_hashed_ansi_string_get_buffer(data->name) ));

    ogl_program_attach_shader(data->data_ptr->rgb_to_Yxy_program,
                              shaders_fragment_rgb_to_Yxy_get_shader(data->data_ptr->rgb_to_Yxy_fragment_shader) );
    ogl_program_attach_shader(data->data_ptr->rgb_to_Yxy_program,
                              shaders_vertex_fullscreen_get_shader  (data->data_ptr->fullscreen_vertex_shader) );
    ogl_program_link         (data->data_ptr->rgb_to_Yxy_program);

    const ogl_program_variable* tex_uniform_descriptor = NULL;

    ogl_program_get_uniform_by_name(data->data_ptr->rgb_to_Yxy_program,
                                    system_hashed_ansi_string_create("tex"),
                                   &tex_uniform_descriptor);

    data->data_ptr->rgb_to_Yxy_program_tex_uniform_location = tex_uniform_descriptor->location;

    /* Create operator fragment shader */
    data->data_ptr->operator_fragment_shader = ogl_shader_create(data->data_ptr->context,
                                                                 RAL_SHADER_TYPE_FRAGMENT,
                                                                 system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap operator ",
                                                                                                                         system_hashed_ansi_string_get_buffer(data->name) ));

    ogl_shader_set_body(data->data_ptr->operator_fragment_shader,
                        reinhard_tonemap_fragment_shader_body);

    /* Create operator program */
    data->data_ptr->operator_program = ogl_program_create(data->data_ptr->context,
                                                          system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap operator ",
                                                                                                                  system_hashed_ansi_string_get_buffer(data->name) ),
                                                          OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(data->data_ptr->operator_program,
                              data->data_ptr->operator_fragment_shader);
    ogl_program_attach_shader(data->data_ptr->operator_program,
                              shaders_vertex_fullscreen_get_shader(data->data_ptr->fullscreen_vertex_shader) );
    ogl_program_link         (data->data_ptr->operator_program);

    /* Retrieve uniform block properties */
    ogl_program_get_uniform_block_by_name(data->data_ptr->operator_program,
                                          system_hashed_ansi_string_create("data"),
                                         &data->data_ptr->operator_program_ub);
    ogl_program_ub_get_property          (data->data_ptr->operator_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                         &data->data_ptr->operator_program_ub_bo_size);
    ogl_program_ub_get_property          (data->data_ptr->operator_program_ub,
                                          OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                                         &data->data_ptr->operator_program_ub_bo);

    /* Retrieve uniform properties */
    const ogl_program_variable* alpha_uniform_descriptor                 = NULL;
    const ogl_program_variable* luminance_texture_uniform_descriptor     = NULL;
    const ogl_program_variable* luminance_texture_avg_uniform_descriptor = NULL;
    const ogl_program_variable* white_level_uniform_descriptor           = NULL;

    ogl_program_get_uniform_by_name(data->data_ptr->operator_program,
                                    system_hashed_ansi_string_create("alpha"),
                                   &alpha_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program,
                                    system_hashed_ansi_string_create("luminance_texture"),
                                   &luminance_texture_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program,
                                    system_hashed_ansi_string_create("luminance_texture_avg"),
                                   &luminance_texture_avg_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program,
                                    system_hashed_ansi_string_create("white_level"),
                                   &white_level_uniform_descriptor);

    data->data_ptr->operator_program_alpha_ub_offset                = alpha_uniform_descriptor->block_offset;
    data->data_ptr->operator_program_luminance_texture_location     = luminance_texture_uniform_descriptor->location;
    data->data_ptr->operator_program_luminance_texture_avg_location = luminance_texture_avg_uniform_descriptor->location;
    data->data_ptr->operator_program_white_level_ub_offset          = white_level_uniform_descriptor->block_offset;
}

/** TODO */
PRIVATE void _release_callback(ogl_context context,
                               void*       arg)
{
    _postprocessing_reinhard_tonemap* data_ptr     = (_postprocessing_reinhard_tonemap*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(ral_context_get_gl_context(data_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLDeleteFramebuffers(1,
                                       &data_ptr->dst_framebuffer_id);
    entry_points->pGLDeleteFramebuffers(1,
                                       &data_ptr->src_framebuffer_id);

    if (data_ptr->downsampled_yxy_texture != NULL)
    {
        ral_context_delete_textures(data_ptr->context,
                                    1, /* n_textures */
                                   &data_ptr->downsampled_yxy_texture);

        data_ptr->downsampled_yxy_texture_gl = NULL;
    }

    ral_context_delete_textures(data_ptr->context,
                                1, /* n_textures */
                               &data_ptr->yxy_texture);

    data_ptr->yxy_texture = NULL;

    shaders_fragment_rgb_to_Yxy_release(data_ptr->rgb_to_Yxy_fragment_shader);
    shaders_vertex_fullscreen_release  (data_ptr->fullscreen_vertex_shader);
    ogl_shader_release                 (data_ptr->operator_fragment_shader);
    ogl_program_release                (data_ptr->rgb_to_Yxy_program);
    ogl_program_release                (data_ptr->operator_program);
}

/** TODO */
PRIVATE void _postprocessing_reinhard_tonemap_release(void* ptr)
{
    _postprocessing_reinhard_tonemap* data_ptr = (_postprocessing_reinhard_tonemap*) ptr;

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(data_ptr->context),
                                                     _release_callback,
                                                     data_ptr);
}


/** Please see header for specification */
PUBLIC EMERALD_API postprocessing_reinhard_tonemap postprocessing_reinhard_tonemap_create(ral_context               context,
                                                                                          system_hashed_ansi_string name,
                                                                                          bool                      use_crude_downsampled_lum_average_calculation,
                                                                                          uint32_t*                 texture_size)
{
    /* Instantiate the object */
    _postprocessing_reinhard_tonemap* result_object = new (std::nothrow) _postprocessing_reinhard_tonemap;

    ASSERT_DEBUG_SYNC(result_object != NULL,
                      "Out of memory while instantiating _postprocessing_reinhard_tonemap object.");

    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Reinhard tonemap postprocessor object instance.");

        goto end;
    }

    result_object->context = context;

    /* Pass control to rendering thread */
    _create_callback_data data;

    data.data_ptr                                                = result_object;
    data.data_ptr->texture_width                                 = texture_size[0];
    data.data_ptr->texture_height                                = texture_size[1];
    data.name                                                    = name;
    data.data_ptr->use_crude_downsampled_lum_average_calculation = use_crude_downsampled_lum_average_calculation;

    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _create_callback,
                                                    &data);

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object,
                                                   _postprocessing_reinhard_tonemap_release,
                                                   OBJECT_TYPE_POSTPROCESSING_REINHARD_TONEMAP,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Post-processing Reinhard tonemap\\",
                                                                                                           system_hashed_ansi_string_get_buffer(name)) );

    /* Return the object */
    return (postprocessing_reinhard_tonemap) result_object;

end:
    if (result_object != NULL)
    {
        delete result_object;

        result_object = NULL;
    }

    return NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API void postprocessing_reinhard_tonemap_execute(postprocessing_reinhard_tonemap tonemapper,
                                                                ral_texture                     in_texture,
                                                                float                           alpha,
                                                                float                           white_level,
                                                                ral_texture                     out_texture)
{
    ral_framebuffer                   context_default_fb             = NULL;
    raGL_framebuffer                  context_default_fb_raGL        = NULL;
    GLuint                            context_default_fb_raGL_id     = -1;
    GLuint                            downsampled_yxy_texture_id     = 0;
    bool                              downsampled_yxy_texture_is_rbo = false;
    const ogl_context_gl_entrypoints* entry_points                   = NULL;
    raGL_texture                      in_texture_gl                  = NULL;
    GLuint                            in_texture_id                  = 0;
    bool                              in_texture_is_rbo              = false;
    raGL_texture                      out_texture_gl                 = NULL;
    GLuint                            out_texture_id                 = 0;
    bool                              out_texture_is_rbo             = false;
    _postprocessing_reinhard_tonemap* tonemapper_ptr                 = (_postprocessing_reinhard_tonemap*) tonemapper;
    GLuint                            yxy_texture_id                 = 0;
    bool                              yxy_texture_is_rbo             = false;
    GLuint                            vao_id                         = 0;

    in_texture_gl  = ral_context_get_texture_gl(tonemapper_ptr->context,
                                                in_texture);
    out_texture_gl = ral_context_get_texture_gl(tonemapper_ptr->context,
                                                out_texture);

    raGL_texture_get_property(in_texture_gl,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &in_texture_id);
    raGL_texture_get_property(tonemapper_ptr->yxy_texture_gl,
                              RAGL_TEXTURE_PROPERTY_ID,
                             &yxy_texture_id);

    raGL_texture_get_property(in_texture_gl,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &in_texture_is_rbo);
    raGL_texture_get_property(tonemapper_ptr->yxy_texture_gl,
                              RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                             &yxy_texture_is_rbo);

    ASSERT_DEBUG_SYNC(!downsampled_yxy_texture_is_rbo &&
                      !in_texture_is_rbo              &&
                      !out_texture_is_rbo             &&
                      !yxy_texture_is_rbo,
                      "TODO");

    ogl_context_get_property(ral_context_get_gl_context(tonemapper_ptr->context),
                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO,
                            &context_default_fb);
    ogl_context_get_property(ral_context_get_gl_context(tonemapper_ptr->context),
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);
    ogl_context_get_property(ral_context_get_gl_context(tonemapper_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    context_default_fb_raGL = ral_context_get_framebuffer_gl(tonemapper_ptr->context,
                                                             context_default_fb);

    raGL_framebuffer_get_property(context_default_fb_raGL,
                                  RAGL_FRAMEBUFFER_PROPERTY_ID,
                                 &context_default_fb_raGL_id);

    entry_points->pGLViewport(0, /* x */
                              0, /* y */
                              tonemapper_ptr->texture_width,
                              tonemapper_ptr->texture_height);

    /* 1. Calculate luminance texture */
    GLuint rgb_to_Yxy_program_id = ogl_program_get_id(tonemapper_ptr->rgb_to_Yxy_program);

    entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER,
                                          tonemapper_ptr->dst_framebuffer_id);
    entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,                      /* TODO: WTF is happening here? Why isn't this FBO pre-configured? */
                                          GL_COLOR_ATTACHMENT0,
                                          GL_TEXTURE_2D,
                                          yxy_texture_id,
                                          0);

    entry_points->pGLUseProgram      (rgb_to_Yxy_program_id);
    entry_points->pGLActiveTexture   (GL_TEXTURE0);
    entry_points->pGLBindTexture     (GL_TEXTURE_2D,
                                      in_texture_id);
    entry_points->pGLProgramUniform1i(rgb_to_Yxy_program_id,
                                      tonemapper_ptr->rgb_to_Yxy_program_tex_uniform_location,
                                      0);

    entry_points->pGLBindVertexArray(vao_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0,
                                     4);

    /* 2. Downsample Yxy texture IF needed */
    static const GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};

    if (tonemapper_ptr->use_crude_downsampled_lum_average_calculation)
    {
        raGL_texture_get_property(tonemapper_ptr->downsampled_yxy_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_ID,
                                 &downsampled_yxy_texture_id);
        raGL_texture_get_property(tonemapper_ptr->downsampled_yxy_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                 &downsampled_yxy_texture_is_rbo);

        entry_points->pGLBindFramebuffer     (GL_READ_FRAMEBUFFER,
                                              tonemapper_ptr->src_framebuffer_id);  /* TODO: as above */
        entry_points->pGLReadBuffer          (GL_COLOR_ATTACHMENT0);
        entry_points->pGLFramebufferTexture2D(GL_READ_FRAMEBUFFER,
                                              GL_COLOR_ATTACHMENT0,
                                              GL_TEXTURE_2D,
                                              yxy_texture_id,
                                              0);
        entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER,
                                              tonemapper_ptr->dst_framebuffer_id);
        entry_points->pGLDrawBuffers         (1,
                                              draw_buffers);
        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                              GL_COLOR_ATTACHMENT0,
                                              GL_TEXTURE_2D,
                                              downsampled_yxy_texture_id,
                                              0);

        entry_points->pGLBlitFramebuffer     (0, /* srcX0 */
                                              0, /* srcYo */
                                              tonemapper_ptr->texture_width,
                                              tonemapper_ptr->texture_height,
                                              0,  /* dstX0 */
                                              0,  /* dstY0 */
                                              64, /* dstX1 */
                                              64, /* dstY1 */
                                              GL_COLOR_BUFFER_BIT,
                                              GL_LINEAR);

        entry_points->pGLViewport(0,  /* x */
                                  0,  /* y */
                                  64, /* width */
                                  64);/* height */
    }

    /* 3. Generate mipmaps so that the last level contains average luminance */
    entry_points->pGLBindTexture   (GL_TEXTURE_2D,
                                    (tonemapper_ptr->use_crude_downsampled_lum_average_calculation ? downsampled_yxy_texture_id :
                                                                                                     yxy_texture_id) );
    entry_points->pGLGenerateMipmap(GL_TEXTURE_2D);

    /* 4. Process input texture */
    GLuint tonemapper_program_id = ogl_program_get_id(tonemapper_ptr->operator_program);

    entry_points->pGLUseProgram   (tonemapper_program_id);
    entry_points->pGLActiveTexture(GL_TEXTURE0);
    entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                   yxy_texture_id);

    if (tonemapper_ptr->use_crude_downsampled_lum_average_calculation)
    {
        entry_points->pGLActiveTexture(GL_TEXTURE1);
        entry_points->pGLBindTexture  (GL_TEXTURE_2D,
                                       downsampled_yxy_texture_id);
    }

    entry_points->pGLProgramUniform1i(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_luminance_texture_location,
                                      0);
    entry_points->pGLProgramUniform1i(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_luminance_texture_avg_location,
                                      (tonemapper_ptr->use_crude_downsampled_lum_average_calculation ? 1 : 0) );

    ogl_program_ub_set_nonarrayed_uniform_value(tonemapper_ptr->operator_program_ub,
                                                tonemapper_ptr->operator_program_alpha_ub_offset,
                                               &alpha,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(tonemapper_ptr->operator_program_ub,
                                                tonemapper_ptr->operator_program_white_level_ub_offset,
                                               &white_level,
                                                0, /* src_data_flags */
                                                sizeof(float) );

    ogl_program_ub_sync(tonemapper_ptr->operator_program_ub);

    if (out_texture != NULL)
    {
        raGL_texture_get_property(out_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_ID,
                                 &out_texture_id);
        raGL_texture_get_property(out_texture_gl,
                                  RAGL_TEXTURE_PROPERTY_IS_RENDERBUFFER,
                                 &out_texture_is_rbo);

        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                              GL_COLOR_ATTACHMENT0,
                                              GL_TEXTURE_2D,
                                              out_texture_id,
                                              0);
    }
    else
    {
        entry_points->pGLBindFramebuffer(GL_FRAMEBUFFER,
                                         context_default_fb_raGL_id);
    }

    entry_points->pGLViewport(0, /* x */
                              0, /* y */
                              tonemapper_ptr->texture_width,
                              tonemapper_ptr->texture_height);

    GLuint      operator_program_ub_bo_id           = 0;
    raGL_buffer operator_program_ub_bo_raGL         = NULL;
    uint32_t    operator_program_ub_bo_start_offset = -1;

    operator_program_ub_bo_raGL = ral_context_get_buffer_gl(tonemapper_ptr->context,
                                                            tonemapper_ptr->operator_program_ub_bo);

    raGL_buffer_get_property(operator_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &operator_program_ub_bo_id);
    raGL_buffer_get_property(operator_program_ub_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &operator_program_ub_bo_start_offset);

    entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     0, /* index */
                                     operator_program_ub_bo_id,
                                     operator_program_ub_bo_start_offset,
                                     tonemapper_ptr->operator_program_ub_bo_size);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */
}