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
#include "postprocessing/postprocessing_reinhard_tonemap.h"
#include "shaders/shaders_fragment_rgb_to_Yxy.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"


/** Internal type definition */
typedef struct
{
    ogl_context context;

    ogl_texture downsampled_yxy_texture;
    ogl_texture yxy_texture;

    GLuint  dst_framebuffer_id;
    GLuint  src_framebuffer_id;
    GLuint  vao_id;

    shaders_fragment_rgb_to_Yxy rgb_to_Yxy_fragment_shader;
    shaders_vertex_fullscreen   fullscreen_vertex_shader;

    ogl_program rgb_to_Yxy_program;
    GLuint      rgb_to_Yxy_program_tex_uniform_location;

    ogl_program operator_program;
    GLuint      operator_program_alpha_uniform_location;
    ogl_shader  operator_fragment_shader;
    GLuint      operator_program_luminance_texture_location;
    GLuint      operator_program_luminance_texture_avg_location;
    GLuint      operator_program_white_level_uniform_location;

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
system_hashed_ansi_string reinhard_tonemap_fragment_shader_body = system_hashed_ansi_string_create("#version 330\n"
                                                                                                   "\n"
                                                                                                   "uniform float     alpha;\n"
                                                                                                   "uniform sampler2D luminance_texture;\n"
                                                                                                   "uniform sampler2D luminance_texture_avg;\n"
                                                                                                   "uniform sampler2D rgb_texture;\n"
                                                                                                   "uniform float     white_level;\n"
                                                                                                   "\n"
                                                                                                   "in  vec2 uv;\n"
                                                                                                   "out vec4 result;\n"
                                                                                                   "\n"
                                                                                                   "void main()\n"
                                                                                                   "{\n"
                                                                                                   "    float lum_avg = textureLod(luminance_texture_avg, vec2(0.0, 0.0), 32).x;\n"
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
REFCOUNT_INSERT_IMPLEMENTATION(postprocessing_reinhard_tonemap, postprocessing_reinhard_tonemap, _postprocessing_reinhard_tonemap);

/* Forward declarations */
#ifdef _DEBUG
    PRIVATE void _postprocessing_reinhard_tonemap_verify_context_type(__in __notnull ogl_context);
#else
    #define _postprocessing_reinhard_tonemap_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _create_callback(ogl_context context, void* arg)
{
    _create_callback_data*            data         = (_create_callback_data*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* TODO */
    data->data_ptr->context = context;

    /* TODO */
    data->data_ptr->yxy_texture = ogl_texture_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard tonemap [YXY texture] ", system_hashed_ansi_string_get_buffer(data->name) ));

    entry_points->pGLBindTexture(GL_TEXTURE_2D, data->data_ptr->yxy_texture);
    entry_points->pGLTexImage2D (GL_TEXTURE_2D, 0, GL_RGB32F, data->data_ptr->texture_width, data->data_ptr->texture_height, 0, GL_RGB, GL_FLOAT, NULL);
    
    entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_BORDER);
    entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_BORDER);
    entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    entry_points->pGLTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create YXY texture");

    /* TODO */
    if (data->data_ptr->use_crude_downsampled_lum_average_calculation)
    {
        data->data_ptr->downsampled_yxy_texture = ogl_texture_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard tonemap [downsampled YXY texture] ", system_hashed_ansi_string_get_buffer(data->name) ));

        entry_points->pGLBindTexture  (GL_TEXTURE_2D, data->data_ptr->downsampled_yxy_texture);
        entry_points->pGLTexImage2D   (GL_TEXTURE_2D, 0,                     GL_RGB32F, 64, 64, 0, GL_RGB, GL_FLOAT, NULL);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,     GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,     GL_CLAMP_TO_BORDER);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        entry_points->pGLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    
        ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create downsampled YXY texture");
    }
    else
    {
        data->data_ptr->downsampled_yxy_texture = NULL;
    }

    /* Create a source framebuffer we will be using for blitting purposes */
    entry_points->pGLGenFramebuffers(1, &data->data_ptr->src_framebuffer_id);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create source framebuffer");

    /* Create destination framebuffer we will be using for blitting purposes */
    entry_points->pGLGenFramebuffers (1, &data->data_ptr->dst_framebuffer_id);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create and configure destination framebuffer");

    /* Create VAA we will use for full-screen passes */
    entry_points->pGLGenVertexArrays(1, &data->data_ptr->vao_id);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not generate VAO");

    /* Create RGB=>Yxy fragment shader */
    data->data_ptr->rgb_to_Yxy_fragment_shader = shaders_fragment_rgb_to_Yxy_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap RGB2YXY ", system_hashed_ansi_string_get_buffer(data->name) ), true);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create RGB2YXY fragment shader");

    /* Create fullscreen vertex shader */
    data->data_ptr->fullscreen_vertex_shader = shaders_vertex_fullscreen_create(context, true, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap Fullscreen ", system_hashed_ansi_string_get_buffer(data->name) ));

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create fullscreen vertex shader");

    /* Create RGB=>Yxy program */
    data->data_ptr->rgb_to_Yxy_program = ogl_program_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap RGB2YXY ", system_hashed_ansi_string_get_buffer(data->name) ));

    ogl_program_attach_shader(data->data_ptr->rgb_to_Yxy_program, shaders_fragment_rgb_to_Yxy_get_shader(data->data_ptr->rgb_to_Yxy_fragment_shader) );
    ogl_program_attach_shader(data->data_ptr->rgb_to_Yxy_program, shaders_vertex_fullscreen_get_shader  (data->data_ptr->fullscreen_vertex_shader) );
    ogl_program_link         (data->data_ptr->rgb_to_Yxy_program);

    const ogl_program_uniform_descriptor* tex_uniform_descriptor = NULL;

    ogl_program_get_uniform_by_name(data->data_ptr->rgb_to_Yxy_program, system_hashed_ansi_string_create("tex"), &tex_uniform_descriptor);

    data->data_ptr->rgb_to_Yxy_program_tex_uniform_location = tex_uniform_descriptor->location;

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create RGB2YXY program");

    /* Create operator fragment shader */
    data->data_ptr->operator_fragment_shader = ogl_shader_create(context, SHADER_TYPE_FRAGMENT, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap operator ", system_hashed_ansi_string_get_buffer(data->name) ));

    ogl_shader_set_body(data->data_ptr->operator_fragment_shader, reinhard_tonemap_fragment_shader_body);

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create operator shader");

    /* Create operator program */
    data->data_ptr->operator_program = ogl_program_create(context, system_hashed_ansi_string_create_by_merging_two_strings("Reinhard Tonemap operator ", system_hashed_ansi_string_get_buffer(data->name) ));

    ogl_program_attach_shader(data->data_ptr->operator_program, data->data_ptr->operator_fragment_shader);
    ogl_program_attach_shader(data->data_ptr->operator_program, shaders_vertex_fullscreen_get_shader(data->data_ptr->fullscreen_vertex_shader) );
    ogl_program_link         (data->data_ptr->operator_program);

    const ogl_program_uniform_descriptor* alpha_uniform_descriptor                 = NULL;
    const ogl_program_uniform_descriptor* luminance_texture_uniform_descriptor     = NULL;
    const ogl_program_uniform_descriptor* luminance_texture_avg_uniform_descriptor = NULL;
    const ogl_program_uniform_descriptor* white_level_uniform_descriptor           = NULL;

    ogl_program_get_uniform_by_name(data->data_ptr->operator_program, system_hashed_ansi_string_create("alpha"),                 &alpha_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program, system_hashed_ansi_string_create("luminance_texture"),     &luminance_texture_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program, system_hashed_ansi_string_create("luminance_texture_avg"), &luminance_texture_avg_uniform_descriptor);
    ogl_program_get_uniform_by_name(data->data_ptr->operator_program, system_hashed_ansi_string_create("white_level"),           &white_level_uniform_descriptor);

    data->data_ptr->operator_program_alpha_uniform_location         = alpha_uniform_descriptor->location;
    data->data_ptr->operator_program_luminance_texture_location     = luminance_texture_uniform_descriptor->location;
    data->data_ptr->operator_program_luminance_texture_avg_location = luminance_texture_avg_uniform_descriptor->location;
    data->data_ptr->operator_program_white_level_uniform_location   = white_level_uniform_descriptor->location;

    ASSERT_ALWAYS_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create operator program");
}

/** TODO */
PRIVATE void _release_callback(ogl_context context, void* arg)
{
    _postprocessing_reinhard_tonemap* data_ptr     = (_postprocessing_reinhard_tonemap*) arg;
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(data_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLDeleteFramebuffers(1, &data_ptr->dst_framebuffer_id);
    entry_points->pGLDeleteFramebuffers(1, &data_ptr->src_framebuffer_id);
    entry_points->pGLDeleteVertexArrays(1, &data_ptr->vao_id);

    if (data_ptr->downsampled_yxy_texture != NULL)
    {
        ogl_texture_release(data_ptr->downsampled_yxy_texture);
    }

    ogl_texture_release(data_ptr->yxy_texture);

    shaders_fragment_rgb_to_Yxy_release(data_ptr->rgb_to_Yxy_fragment_shader);
    shaders_vertex_fullscreen_release  (data_ptr->fullscreen_vertex_shader);
    ogl_shader_release                 (data_ptr->operator_fragment_shader);

    ogl_program_release                (data_ptr->rgb_to_Yxy_program);
    ogl_program_release                (data_ptr->operator_program);
}

/** TODO */
PRIVATE void _postprocessing_reinhard_tonemap_release(__in __notnull __deallocate(mem) void* ptr)
{
    _postprocessing_reinhard_tonemap* data_ptr = (_postprocessing_reinhard_tonemap*) ptr;
    
    ogl_context_request_callback_from_context_thread(data_ptr->context, _release_callback, data_ptr);
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _postprocessing_reinhard_tonemap_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "postprocessing_reinhard_tonemap is only supported under GL contexts")
    }
#endif

/** Please see header for specification */
PUBLIC EMERALD_API postprocessing_reinhard_tonemap postprocessing_reinhard_tonemap_create(__in __notnull             ogl_context               context, 
                                                                                          __in __notnull             system_hashed_ansi_string name,
                                                                                          __in                       bool                      use_crude_downsampled_lum_average_calculation,
                                                                                          __in __notnull __ecount(2) uint32_t*                 texture_size)
{
    _postprocessing_reinhard_tonemap_verify_context_type(context);

    /* Instantiate the object */
    _postprocessing_reinhard_tonemap* result_object = new (std::nothrow) _postprocessing_reinhard_tonemap;

    ASSERT_DEBUG_SYNC(result_object != NULL, "Out of memory while instantiating _postprocessing_reinhard_tonemap object.");
    if (result_object == NULL)
    {
        LOG_ERROR("Out of memory while creating Reinhard tonemap postprocessor object instance.");

        goto end;
    }

    /* Pass control to rendering thread */
    _create_callback_data data;

    data.data_ptr                                                = result_object;
    data.data_ptr->texture_width                                 = texture_size[0];
    data.data_ptr->texture_height                                = texture_size[1];
    data.name                                                    = name;
    data.data_ptr->use_crude_downsampled_lum_average_calculation = use_crude_downsampled_lum_average_calculation;

    ogl_context_request_callback_from_context_thread(context, _create_callback, &data);

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_object, 
                                                   _postprocessing_reinhard_tonemap_release,
                                                   OBJECT_TYPE_POSTPROCESSING_REINHARD_TONEMAP,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\Post-processing Reinhard tonemap\\", system_hashed_ansi_string_get_buffer(name)) );

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
PUBLIC EMERALD_API void postprocessing_reinhard_tonemap_execute(__in __notnull postprocessing_reinhard_tonemap tonemapper,
                                                                __in __notnull ogl_texture                     in_texture,
                                                                __in           float                           alpha,
                                                                __in           float                           white_level,
                                                                __in __notnull ogl_texture                     out_texture)
{
    _postprocessing_reinhard_tonemap* tonemapper_ptr = (_postprocessing_reinhard_tonemap*) tonemapper;
    const ogl_context_gl_entrypoints* entry_points   = NULL;

    ogl_context_get_property(tonemapper_ptr->context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* 1. Calculate luminance texture */
    GLuint rgb_to_Yxy_program_id = ogl_program_get_id(tonemapper_ptr->rgb_to_Yxy_program);

    entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER, tonemapper_ptr->dst_framebuffer_id);
    entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tonemapper_ptr->yxy_texture, 0);

    entry_points->pGLUseProgram      (rgb_to_Yxy_program_id);
    entry_points->pGLActiveTexture   (GL_TEXTURE0);
    entry_points->pGLBindTexture     (GL_TEXTURE_2D, in_texture);
    entry_points->pGLProgramUniform1i(rgb_to_Yxy_program_id,
                                      tonemapper_ptr->rgb_to_Yxy_program_tex_uniform_location,
                                      0);

    entry_points->pGLBindVertexArray(tonemapper_ptr->vao_id);
    entry_points->pGLDrawArrays     (GL_TRIANGLE_FAN, 0, 4);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not calculate luminance texture");

    /* 2. Downsample Yxy texture IF needed */
    if (tonemapper_ptr->use_crude_downsampled_lum_average_calculation)
    {
        static const GLenum draw_buffers[1] = {GL_COLOR_ATTACHMENT0};

        entry_points->pGLBindFramebuffer     (GL_READ_FRAMEBUFFER, tonemapper_ptr->src_framebuffer_id);
        entry_points->pGLReadBuffer          (GL_COLOR_ATTACHMENT0);
        entry_points->pGLFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tonemapper_ptr->yxy_texture, 0);
        entry_points->pGLBindFramebuffer     (GL_DRAW_FRAMEBUFFER, tonemapper_ptr->dst_framebuffer_id);
        entry_points->pGLDrawBuffers         (1, draw_buffers);
        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tonemapper_ptr->downsampled_yxy_texture, 0);
        entry_points->pGLBlitFramebuffer     (0, 0, tonemapper_ptr->texture_width, tonemapper_ptr->texture_height, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        
        ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not downsample");
    }

    /* 3. Generate mipmaps so that the last level contains average luminance */
    entry_points->pGLBindTexture   (GL_TEXTURE_2D, (tonemapper_ptr->use_crude_downsampled_lum_average_calculation ? tonemapper_ptr->downsampled_yxy_texture :
                                                                                                                    tonemapper_ptr->yxy_texture) );
    entry_points->pGLGenerateMipmap(GL_TEXTURE_2D);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not generate mipmaps for YXY texture");

    /* 4. Process input texture */
    GLuint tonemapper_program_id = ogl_program_get_id(tonemapper_ptr->operator_program);

    entry_points->pGLUseProgram   (tonemapper_program_id);
    entry_points->pGLActiveTexture(GL_TEXTURE0);
    entry_points->pGLBindTexture  (GL_TEXTURE_2D, tonemapper_ptr->yxy_texture); 

    if (tonemapper_ptr->use_crude_downsampled_lum_average_calculation)
    {
        entry_points->pGLActiveTexture(GL_TEXTURE1);
        entry_points->pGLBindTexture  (GL_TEXTURE_2D, tonemapper_ptr->downsampled_yxy_texture);
    }

    entry_points->pGLProgramUniform1f(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_alpha_uniform_location,
                                      alpha);
    entry_points->pGLProgramUniform1i(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_luminance_texture_location,
                                      0);
    entry_points->pGLProgramUniform1i(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_luminance_texture_avg_location,
                                      (tonemapper_ptr->use_crude_downsampled_lum_average_calculation ? 1 : 0) );
    entry_points->pGLProgramUniform1f(tonemapper_program_id,
                                      tonemapper_ptr->operator_program_white_level_uniform_location,
                                      white_level);

    if (out_texture != NULL)
    {
        entry_points->pGLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out_texture, 0);
    }
    else
    {
        entry_points->pGLBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    entry_points->pGLDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not generate output texture");
}