/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_texture.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_texture_preview.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"
#include <string>

const float _ui_texture_preview_text_color[] = {1, 1, 1, 1.0f};

/** Internal types */
typedef struct
{
    GLfloat blend_color[4];
    GLenum  blend_equation_alpha;
    GLenum  blend_equation_rgb;
    GLenum  blend_function_dst_alpha;
    GLenum  blend_function_dst_rgb;
    GLenum  blend_function_src_alpha;
    GLenum  blend_function_src_rgb;
    bool    is_blending_enabled;

    float                       border_width[2];
    float                       max_size[2];
    ogl_ui_texture_preview_type preview_type;
    float                       x1y1x2y2[4];

    ogl_context               context;
    system_hashed_ansi_string name;
    ogl_program               program;
    GLint                     program_border_width_uniform_location;
    GLint                     program_texture_uniform_location;
    GLint                     program_x1y1x2y2_uniform_location;
    ogl_texture               texture;
    bool                      texture_initialized;

    GLint    text_index;
    ogl_text text_renderer;

    /* Cached func ptrs */
    ogl_context_type                     context_type;
    PFNWRAPPEDGLBINDMULTITEXTUREEXTPROC  gl_pGLBindMultiTextureEXT;
    PFNWRAPPEDGLBINDTEXTUREPROC          gl_pGLBindTexture;
    PFNWRAPPEDGLTEXTUREPARAMETERIEXTPROC gl_pGLTextureParameteriEXT;
    PFNGLACTIVETEXTUREPROC               pGLActiveTexture;
    PFNGLBINDSAMPLERPROC                 pGLBindSampler;
    PFNGLBINDTEXTUREPROC                 pGLBindTexture;
    PFNGLBLENDCOLORPROC                  pGLBlendColor;
    PFNGLBLENDEQUATIONSEPARATEPROC       pGLBlendEquationSeparate;
    PFNGLBLENDFUNCSEPARATEPROC           pGLBlendFuncSeparate;
    PFNGLDISABLEPROC                     pGLDisable;
    PFNGLDRAWARRAYSPROC                  pGLDrawArrays;
    PFNGLENABLEPROC                      pGLEnable;
    PFNGLGETTEXLEVELPARAMETERIVPROC      pGLGetTexLevelParameteriv;
    PFNGLPROGRAMUNIFORM2FVPROC           pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM4FVPROC           pGLProgramUniform4fv;
    PFNGLTEXPARAMETERIPROC               pGLTexParameteri;
    PFNGLUSEPROGRAMPROC                  pGLUseProgram;
} _ogl_ui_texture_preview;

/** Internal variables */
static const char* ui_texture_preview_renderer_alpha_body  = "result = vec4(textureLod(texture, uv, 0).aaa, 1.0);\n";
static const char* ui_texture_preview_renderer_depth_body  = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_red_body    = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_rgb_body    = "result = vec4(textureLod(texture, uv, 0).xyz, 1.0);\n";
static const char* ui_texture_preview_renderer_rgba_body   = "result = textureLod(texture, uv, 0);\n";
static const char* ui_texture_preview_fragment_shader_body = "#version 330\n"
                                                             "\n"
                                                             "in      vec2      uv;\n"
                                                             "out     vec4      result;\n"
                                                             "uniform vec2      border_width;\n"
                                                             "uniform sampler2D texture;\n"
                                                             "\n"
                                                             "void main()\n"
                                                             "{\n"
                                                             "    RENDER_RESULT;\n"
                                                             "\n"
                                                             /* Border? */
                                                             "    if (uv.x < border_width[0]         ||\n"
                                                             "        uv.x > (1.0 - border_width[0]) ||\n"
                                                             "        uv.y < border_width[1]         ||\n"
                                                             "        uv.y > (1.0 - border_width[1]) )\n"
                                                             "    {\n"
                                                             "        result = vec4(0.42f, 0.41f, 0.41f, 1.0);\n"
                                                             "    }\n"
                                                             "}\n";

/** TODO */
PRIVATE const char* _ogl_ui_texture_preview_get_program_name(__in ogl_ui_texture_preview_type type)
{
    const char* result = (type == OGL_UI_TEXTURE_PREVIEW_TYPE_ALPHA) ? "UI texture preview [alpha]" :
                         (type == OGL_UI_TEXTURE_PREVIEW_TYPE_DEPTH) ? "UI texture preview [depth]" :
                         (type == OGL_UI_TEXTURE_PREVIEW_TYPE_RED)   ? "UI texture preview [red]"   :
                         (type == OGL_UI_TEXTURE_PREVIEW_TYPE_RGB)   ? "UI texture preview [rgb]"   :
                                                                       "UI texture preview [rgba]";

    return result;
}

/** TODO */
PRIVATE void _ogl_ui_texture_preview_init_program(__in __notnull ogl_ui                   ui,
                                                  __in __notnull _ogl_ui_texture_preview* texture_preview_ptr)
{
    /* Create all objects */
    ogl_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = ogl_shader_create(context,
                                                    SHADER_TYPE_FRAGMENT,
                                                    system_hashed_ansi_string_create_by_merging_two_strings(_ogl_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type),
                                                                                                            " fragment shader") );
    ogl_shader  vertex_shader   = ogl_shader_create(context,
                                                    SHADER_TYPE_VERTEX,
                                                    system_hashed_ansi_string_create_by_merging_two_strings(_ogl_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type),
                                                                                                            " vertex shader") );

    texture_preview_ptr->program = ogl_program_create(context,
                                                      system_hashed_ansi_string_create(_ogl_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type)) );

    /* Set up FS body */
    std::string fs_body            = ui_texture_preview_fragment_shader_body;
    const char* render_result_body = (texture_preview_ptr->preview_type == OGL_UI_TEXTURE_PREVIEW_TYPE_ALPHA) ? ui_texture_preview_renderer_alpha_body :
                                     (texture_preview_ptr->preview_type == OGL_UI_TEXTURE_PREVIEW_TYPE_DEPTH) ? ui_texture_preview_renderer_depth_body :
                                     (texture_preview_ptr->preview_type == OGL_UI_TEXTURE_PREVIEW_TYPE_RED)   ? ui_texture_preview_renderer_red_body   :
                                     (texture_preview_ptr->preview_type == OGL_UI_TEXTURE_PREVIEW_TYPE_RGB)   ? ui_texture_preview_renderer_rgb_body   :
                                                                                                                ui_texture_preview_renderer_rgba_body;
    const char* token              = "RENDER_RESULT";
    std::size_t token_location     = std::string::npos;

    while ( (token_location = fs_body.find(token) ) != std::string::npos)
    {
        fs_body.replace(token_location,
                        strlen(token),
                        render_result_body);
    }

    /* Set up shaders */
    ogl_shader_set_body(fragment_shader,
                        system_hashed_ansi_string_create(fs_body.c_str() ));
    ogl_shader_set_body(vertex_shader,
                        system_hashed_ansi_string_create(ui_general_vertex_shader_body) );

    ogl_shader_compile(fragment_shader);
    ogl_shader_compile(vertex_shader);

    /* Set up program object */
    ogl_program_attach_shader(texture_preview_ptr->program,
                              fragment_shader);
    ogl_program_attach_shader(texture_preview_ptr->program,
                              vertex_shader);

    ogl_program_link(texture_preview_ptr->program);

    /* Register the prgoram with UI so following button instances will reuse the program */
    ogl_ui_register_program(ui,
                            system_hashed_ansi_string_create(_ogl_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type)),
                            texture_preview_ptr->program);

    /* Release shaders we will no longer need */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** TODO */
PRIVATE void _ogl_ui_texture_preview_init_texture_renderer_callback(ogl_context context,
                                                                    void*       texture_preview)
{
    _ogl_ui_texture_preview* texture_preview_ptr = (_ogl_ui_texture_preview*) texture_preview;
    const GLuint             program_id          = ogl_program_get_id(texture_preview_ptr->program);
    system_window            window              = NULL;
    int                      window_height       = 0;
    int                      window_width        = 0;

    ASSERT_DEBUG_SYNC(!texture_preview_ptr->texture_initialized,
                      "TO already initialized");

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    system_window_get_dimensions(window,
                                &window_width,
                                &window_height);

    /* Determine preview size */
    float preview_height_ss = 0.0f;
    float preview_width_ss  = 0.0f;
    int   texture_height_px = 0;
    float texture_height_ss = 0.0f;
    float texture_h_ratio   = 0.0f;
    int   texture_width_px  = 0;
    float texture_width_ss  = 0.0f;
    float texture_v_ratio   = 0.0f;

    if (texture_preview_ptr->context_type == OGL_CONTEXT_TYPE_GL)
    {
        texture_preview_ptr->gl_pGLBindTexture(GL_TEXTURE_2D,
                                               texture_preview_ptr->texture);
    }
    else
    {
        GLint texture_id = 0;

        ogl_texture_get_property(texture_preview_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_ID,
                                &texture_id);

        texture_preview_ptr->pGLBindTexture(GL_TEXTURE_2D,
                                            texture_id);
    }

    texture_preview_ptr->pGLGetTexLevelParameteriv(GL_TEXTURE_2D,
                                                   0,
                                                   GL_TEXTURE_HEIGHT,
                                                  &texture_height_px);
    texture_preview_ptr->pGLGetTexLevelParameteriv(GL_TEXTURE_2D,
                                                   0,
                                                   GL_TEXTURE_WIDTH,
                                                  &texture_width_px);

    ASSERT_ALWAYS_SYNC(texture_height_px != 0 && texture_width_px != 0,
                       "Input texture is undefined!");

    texture_h_ratio   = float(texture_width_px)  / float(texture_height_px);
    texture_height_ss = float(texture_height_px) / float(window_height);
    texture_v_ratio   = float(texture_height_px) / float(texture_width_px);
    texture_width_ss  = float(texture_width_px)  / float(window_width);

    if (texture_preview_ptr->max_size[0] > texture_preview_ptr->max_size[1])
    {
        /* Landscape orientation requested */
        preview_width_ss  = texture_preview_ptr->max_size[0];
        preview_height_ss = texture_preview_ptr->max_size[0] / texture_h_ratio;
    }
    else
    {
        /* Vertical orientation requested */
        preview_width_ss  = texture_preview_ptr->max_size[1] / texture_v_ratio;
        preview_height_ss = texture_preview_ptr->max_size[1];
    }

    texture_preview_ptr->x1y1x2y2[2] = texture_preview_ptr->x1y1x2y2[0] + preview_width_ss;
    texture_preview_ptr->x1y1x2y2[3] = texture_preview_ptr->x1y1x2y2[1] - preview_height_ss;

    texture_preview_ptr->border_width[0] = 1.0f / (float)(preview_width_ss  * window_width  + 1);
    texture_preview_ptr->border_width[1] = 1.0f / (float)(preview_height_ss * window_height + 1);

    /* Configure the text to be shown below the preview */
    ogl_text_set(texture_preview_ptr->text_renderer,
                 texture_preview_ptr->text_index,
                 system_hashed_ansi_string_get_buffer(texture_preview_ptr->name) );

    int text_height   = 0;
    int text_xy[2]    = {0};
    int text_width    = 0;

    ogl_text_get_text_string_property(texture_preview_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                      texture_preview_ptr->text_index,
                                     &text_height);
    ogl_text_get_text_string_property(texture_preview_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                      texture_preview_ptr->text_index,
                                     &text_width);

    text_xy[0] = (int) ((texture_preview_ptr->x1y1x2y2[0] + (texture_preview_ptr->x1y1x2y2[2] - texture_preview_ptr->x1y1x2y2[0] - float(text_width)  / window_width) * 0.5f) * (float) window_width);
    text_xy[1] = (int) ((1.0f - (texture_preview_ptr->x1y1x2y2[3] - float(text_height) / window_height)) * (float) window_height);

    ogl_text_set_text_string_property(texture_preview_ptr->text_renderer,
                                      texture_preview_ptr->text_index,
                                      OGL_TEXT_STRING_PROPERTY_COLOR,
                                      _ui_texture_preview_text_color);
    ogl_text_set_text_string_property(texture_preview_ptr->text_renderer,
                                      texture_preview_ptr->text_index,
                                      OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                      text_xy);

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* border_width_uniform = NULL;
    const ogl_program_uniform_descriptor* texture_uniform      = NULL;
    const ogl_program_uniform_descriptor* x1y1x2y2_uniform     = NULL;

    ogl_program_get_uniform_by_name(texture_preview_ptr->program,
                                    system_hashed_ansi_string_create("border_width"),
                                   &border_width_uniform);
    ogl_program_get_uniform_by_name(texture_preview_ptr->program,
                                    system_hashed_ansi_string_create("texture"),
                                   &texture_uniform);
    ogl_program_get_uniform_by_name(texture_preview_ptr->program,
                                    system_hashed_ansi_string_create("x1y1x2y2"),
                                   &x1y1x2y2_uniform);

    if (border_width_uniform != NULL)
    {
        texture_preview_ptr->program_border_width_uniform_location = border_width_uniform->location;
    }
    else
    {
        texture_preview_ptr->program_border_width_uniform_location = -1;
    }

    texture_preview_ptr->program_texture_uniform_location  = texture_uniform->location;
    texture_preview_ptr->program_x1y1x2y2_uniform_location = x1y1x2y2_uniform->location;
    texture_preview_ptr->texture_initialized               = true;
}

/** Please see header for specification */
PUBLIC void ogl_ui_texture_preview_deinit(void* internal_instance)
{
    _ogl_ui_texture_preview* ui_texture_preview_ptr = (_ogl_ui_texture_preview*) internal_instance;

    ogl_context_release(ui_texture_preview_ptr->context);
    ogl_program_release(ui_texture_preview_ptr->program);
    ogl_text_set       (ui_texture_preview_ptr->text_renderer,
                        ui_texture_preview_ptr->text_index,
                        "");

    delete internal_instance;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_texture_preview_draw(void* internal_instance)
{
    _ogl_ui_texture_preview* texture_preview_ptr = (_ogl_ui_texture_preview*) internal_instance;

    /* Nothing to render if no TO is hooked up.. */
    if (texture_preview_ptr->texture == NULL)
    {
        goto end;
    }

    /* If TO was not initialized, set it up now */
    if (!texture_preview_ptr->texture_initialized)
    {
        _ogl_ui_texture_preview_init_texture_renderer_callback(texture_preview_ptr->context,
                                                               texture_preview_ptr);
    }

    /* Make sure user-requested texture is bound to zeroth texture unit before we continue */
    texture_preview_ptr->pGLBindSampler(0,  /* unit    */
                                        0); /* sampler */

    if (texture_preview_ptr->context_type == OGL_CONTEXT_TYPE_GL)
    {
        texture_preview_ptr->gl_pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                       GL_TEXTURE_2D,
                                                       texture_preview_ptr->texture);
    }
    else
    {
        GLint texture_id = 0;

        ogl_texture_get_property(texture_preview_ptr->texture,
                                 OGL_TEXTURE_PROPERTY_ID,
                                &texture_id);

        texture_preview_ptr->pGLActiveTexture(GL_TEXTURE0);
        texture_preview_ptr->pGLBindTexture  (GL_TEXTURE_2D,
                                              texture_id);
    }

    /* For depth textures, make sure the "depth texture comparison mode" is toggled off before
     * we proceed with sampling the mip-map */
    if (texture_preview_ptr->context_type == OGL_CONTEXT_TYPE_GL)
    {
        texture_preview_ptr->gl_pGLTextureParameteriEXT(texture_preview_ptr->texture,
                                                        GL_TEXTURE_2D,
                                                        GL_TEXTURE_COMPARE_MODE,
                                                        GL_NONE);
    }
    else
    {
        texture_preview_ptr->pGLTexParameteri(GL_TEXTURE_2D,
                                              GL_TEXTURE_COMPARE_MODE,
                                              GL_NONE);
    }

    /* Set up uniforms */
    GLuint program_id = ogl_program_get_id(texture_preview_ptr->program);

    texture_preview_ptr->pGLProgramUniform2fv(program_id,
                                              texture_preview_ptr->program_border_width_uniform_location,
                                              1,
                                              texture_preview_ptr->border_width);
    texture_preview_ptr->pGLProgramUniform4fv(program_id,
                                              texture_preview_ptr->program_x1y1x2y2_uniform_location,
                                              1,
                                              texture_preview_ptr->x1y1x2y2);

    /* Configure blending.
     *
     * NOTE: Since we're using a state cache, only those calls that modify current
     *       context configuration will reach the driver */
    if (texture_preview_ptr->is_blending_enabled)
    {
        texture_preview_ptr->pGLEnable(GL_BLEND);
    }
    else
    {
        texture_preview_ptr->pGLDisable(GL_BLEND);
    }

    texture_preview_ptr->pGLBlendColor           (texture_preview_ptr->blend_color[0],
                                                  texture_preview_ptr->blend_color[1],
                                                  texture_preview_ptr->blend_color[2],
                                                  texture_preview_ptr->blend_color[3]);
    texture_preview_ptr->pGLBlendEquationSeparate(texture_preview_ptr->blend_equation_rgb,
                                                  texture_preview_ptr->blend_equation_alpha);
    texture_preview_ptr->pGLBlendFuncSeparate    (texture_preview_ptr->blend_function_src_rgb,
                                                  texture_preview_ptr->blend_function_dst_rgb,
                                                  texture_preview_ptr->blend_function_src_alpha,
                                                  texture_preview_ptr->blend_function_dst_alpha);

    /* Draw */
    texture_preview_ptr->pGLUseProgram(ogl_program_get_id(texture_preview_ptr->program) );
    texture_preview_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                       0,
                                       4);

end:
    ;
}

/** Please see header for specification */
PUBLIC void ogl_ui_texture_preview_get_property(__in  __notnull const void* texture_preview,
                                                __in  __notnull int         property_value,
                                                __out __notnull void*       out_result)
{
    _ogl_ui_texture_preview* texture_preview_ptr = (_ogl_ui_texture_preview*) texture_preview;

    switch (property_value)
    {
        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_COLOR:
        {
            memcpy(out_result,
                   texture_preview_ptr->blend_color,
                   sizeof(texture_preview_ptr->blend_color) );

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_equation_alpha;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_equation_rgb;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_dst_alpha;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_dst_rgb;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_src_alpha;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_src_rgb;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_IS_BLENDING_ENABLED:
        {
            *(bool*) out_result = texture_preview_ptr->is_blending_enabled;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_SHOW_TEXTURE_NAME:
        {
            ogl_text_get_text_string_property(texture_preview_ptr->text_renderer,
                                              OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                              texture_preview_ptr->text_index,
                                              out_result);

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_TEXTURE:
        {
            *(ogl_texture*) out_result = texture_preview_ptr->texture;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_ui_texture_preview property requested");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_texture_preview_init(__in           __notnull ogl_ui                      instance,
                                         __in           __notnull ogl_text                    text_renderer,
                                         __in           __notnull system_hashed_ansi_string   name,
                                         __in_ecount(2) __notnull const float*                x1y1,
                                         __in_ecount(2) __notnull const float*                max_size,
                                         __in                     ogl_texture                 to,
                                         __in                     ogl_ui_texture_preview_type preview_type)
{
    _ogl_ui_texture_preview* new_texture_preview = new (std::nothrow) _ogl_ui_texture_preview;

    ASSERT_ALWAYS_SYNC(new_texture_preview != NULL,
                       "Out of memory");

    if (new_texture_preview != NULL)
    {
        /* Initialize fields */
        memset(new_texture_preview->border_width,
               0,
               sizeof(new_texture_preview->border_width) );
        memset(new_texture_preview,
               0,
               sizeof(_ogl_ui_texture_preview) );
        memcpy(new_texture_preview->max_size,
               max_size,
               sizeof(float) * 2);

        new_texture_preview->x1y1x2y2[0]         =     x1y1[0];
        new_texture_preview->x1y1x2y2[1]         = 1 - x1y1[1];

        new_texture_preview->context             = ogl_ui_get_context(instance);
        new_texture_preview->context_type        = OGL_CONTEXT_TYPE_UNDEFINED;
        new_texture_preview->name                = name;
        new_texture_preview->preview_type        = preview_type;
        new_texture_preview->text_renderer       = text_renderer;
        new_texture_preview->text_index          = ogl_text_add_string(text_renderer);
        new_texture_preview->texture             = to;
        new_texture_preview->texture_initialized = false;

        if (to != NULL)
        {
            ogl_texture_retain(to);
        }

        /* Set blending states */
        memset(new_texture_preview->blend_color,
               0,
               sizeof(new_texture_preview->blend_color) );

        new_texture_preview->blend_equation_alpha     = GL_FUNC_ADD;
        new_texture_preview->blend_equation_rgb       = GL_FUNC_ADD;
        new_texture_preview->blend_function_dst_alpha = GL_ZERO;
        new_texture_preview->blend_function_dst_rgb   = GL_ZERO;
        new_texture_preview->blend_function_src_alpha = GL_ONE;
        new_texture_preview->blend_function_src_rgb   = GL_ONE;
        new_texture_preview->is_blending_enabled      = false;

        ogl_context_retain(new_texture_preview->context);

        /* Cache GL func pointers */
        ogl_context_get_property(new_texture_preview->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &new_texture_preview->context_type);

        if (new_texture_preview->context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_texture_preview->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_texture_preview->gl_pGLBindMultiTextureEXT  = NULL;
            new_texture_preview->gl_pGLBindTexture          = NULL;
            new_texture_preview->gl_pGLTextureParameteriEXT = NULL;
            new_texture_preview->pGLActiveTexture           = entry_points->pGLActiveTexture;
            new_texture_preview->pGLBindSampler             = entry_points->pGLBindSampler;
            new_texture_preview->pGLBindTexture             = entry_points->pGLBindTexture;
            new_texture_preview->pGLBlendColor              = entry_points->pGLBlendColor;
            new_texture_preview->pGLBlendEquationSeparate   = entry_points->pGLBlendEquationSeparate;
            new_texture_preview->pGLBlendFuncSeparate       = entry_points->pGLBlendFuncSeparate;
            new_texture_preview->pGLDisable                 = entry_points->pGLDisable;
            new_texture_preview->pGLDrawArrays              = entry_points->pGLDrawArrays;
            new_texture_preview->pGLEnable                  = entry_points->pGLEnable;
            new_texture_preview->pGLGetTexLevelParameteriv  = entry_points->pGLGetTexLevelParameteriv;
            new_texture_preview->pGLProgramUniform2fv       = entry_points->pGLProgramUniform2fv;
            new_texture_preview->pGLProgramUniform4fv       = entry_points->pGLProgramUniform4fv;
            new_texture_preview->pGLTexParameteri           = entry_points->pGLTexParameteri;
            new_texture_preview->pGLUseProgram              = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(new_texture_preview->context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
            const ogl_context_gl_entrypoints*                         entry_points     = NULL;

            ogl_context_get_property(new_texture_preview->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            ogl_context_get_property(new_texture_preview->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                    &dsa_entry_points);

            new_texture_preview->gl_pGLBindMultiTextureEXT  = dsa_entry_points->pGLBindMultiTextureEXT;
            new_texture_preview->gl_pGLBindTexture          = entry_points->pGLBindTexture;
            new_texture_preview->gl_pGLTextureParameteriEXT = dsa_entry_points->pGLTextureParameteriEXT;
            new_texture_preview->pGLActiveTexture           = entry_points->pGLActiveTexture;
            new_texture_preview->pGLBindSampler             = entry_points->pGLBindSampler;
            new_texture_preview->pGLBlendColor              = entry_points->pGLBlendColor;
            new_texture_preview->pGLBlendEquationSeparate   = entry_points->pGLBlendEquationSeparate;
            new_texture_preview->pGLBlendFuncSeparate       = entry_points->pGLBlendFuncSeparate;
            new_texture_preview->pGLDisable                 = entry_points->pGLDisable;
            new_texture_preview->pGLDrawArrays              = entry_points->pGLDrawArrays;
            new_texture_preview->pGLEnable                  = entry_points->pGLEnable;
            new_texture_preview->pGLGetTexLevelParameteriv  = entry_points->pGLGetTexLevelParameteriv;
            new_texture_preview->pGLProgramUniform2fv       = entry_points->pGLProgramUniform2fv;
            new_texture_preview->pGLProgramUniform4fv       = entry_points->pGLProgramUniform4fv;
            new_texture_preview->pGLTexParameteri           = entry_points->pGLTexParameteri;
            new_texture_preview->pGLUseProgram              = entry_points->pGLUseProgram;
        }

        /* Retrieve the rendering program */
        new_texture_preview->program = ogl_ui_get_registered_program(instance,
                                                                     system_hashed_ansi_string_create(_ogl_ui_texture_preview_get_program_name(preview_type)) );

        if (new_texture_preview->program == NULL)
        {
            _ogl_ui_texture_preview_init_program(instance,
                                                 new_texture_preview);

            ASSERT_DEBUG_SYNC(new_texture_preview->program != NULL,
                              "Could not initialize texture preview UI program");
        } /* if (new_texture_preview->program == NULL) */

        /* Set up rendering program. This must not be called if the requested TO
         * is NULL.*/
        if (to != NULL)
        {
            ogl_context_request_callback_from_context_thread(new_texture_preview->context,
                                                             _ogl_ui_texture_preview_init_texture_renderer_callback,
                                                             new_texture_preview);
        }
    } /* if (new_button != NULL) */

    return (void*) new_texture_preview;
}

/** Please see header for specification */
PUBLIC void ogl_ui_texture_preview_set_property(__in __notnull void*       texture_preview,
                                                __in __notnull int         property_value,
                                                __in __notnull const void* data)
{
    _ogl_ui_texture_preview* texture_preview_ptr = (_ogl_ui_texture_preview*) texture_preview;

    switch (property_value)
    {
        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_COLOR:
        {
            memcpy(texture_preview_ptr->blend_color,
                   data,
                   sizeof(texture_preview_ptr->blend_color) );

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_ALPHA:
        {
            texture_preview_ptr->blend_equation_alpha = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_EQUATION_RGB:
        {
            texture_preview_ptr->blend_equation_rgb = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_ALPHA:
        {
            texture_preview_ptr->blend_function_dst_alpha = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_DST_RGB:
        {
            texture_preview_ptr->blend_function_dst_rgb = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_ALPHA:
        {
            texture_preview_ptr->blend_function_src_alpha = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_BLEND_FUNCTION_SRC_RGB:
        {
            texture_preview_ptr->blend_function_src_rgb = *(GLenum*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_IS_BLENDING_ENABLED:
        {
            texture_preview_ptr->is_blending_enabled = *(bool*) data;

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_SHOW_TEXTURE_NAME:
        {
            ogl_text_set_text_string_property(texture_preview_ptr->text_renderer,
                                              texture_preview_ptr->text_index,
                                              OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                              data);

            break;
        }

        case OGL_UI_TEXTURE_PREVIEW_PROPERTY_TEXTURE:
        {
            if (texture_preview_ptr->texture != NULL)
            {
                ogl_texture_release(texture_preview_ptr->texture);
            }

            texture_preview_ptr->texture             = *(ogl_texture*) data;
            texture_preview_ptr->texture_initialized = false;

            if (texture_preview_ptr->texture != NULL)
            {
                ogl_texture_retain(texture_preview_ptr->texture);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_ui_texture_preview property requested");
        }
    } /* switch (property_value) */
}