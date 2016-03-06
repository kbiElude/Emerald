/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "raGL/raGL_utils.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_texture_preview.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"
#include <string>

#define UB_FS_BP_INDEX (0)
#define UB_VS_BP_INDEX (1)


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
    GLuint  layer_shown;
    bool    visible;

    float                   border_width[2];
    float                   max_size[2];
    ui_texture_preview_type preview_type;
    float                   x1y1x2y2[4];

    ral_context               context;
    system_hashed_ansi_string name;
    ral_program               program;
    GLint                     program_border_width_ub_offset;
    GLint                     program_layer_ub_offset;
    GLint                     program_texture_ub_offset;
    ral_program_block_buffer  program_ub_fs;
    GLuint                    program_ub_fs_bo_size;
    ral_program_block_buffer  program_ub_vs;
    GLuint                    program_ub_vs_bo_size;
    GLint                     program_x1y1x2y2_ub_offset;
    ral_texture               texture;
    bool                      texture_initialized;

    GLint               text_index;
    varia_text_renderer text_renderer;

    /* Cached func ptrs */
    ral_backend_type                backend_type;
    PFNGLBINDMULTITEXTUREEXTPROC    gl_pGLBindMultiTextureEXT;
    PFNGLTEXTUREPARAMETERIEXTPROC   gl_pGLTextureParameteriEXT;
    PFNGLACTIVETEXTUREPROC          pGLActiveTexture;
    PFNGLBINDBUFFERRANGEPROC        pGLBindBufferRange;
    PFNGLBINDSAMPLERPROC            pGLBindSampler;
    PFNGLBINDTEXTUREPROC            pGLBindTexture;
    PFNGLBLENDCOLORPROC             pGLBlendColor;
    PFNGLBLENDEQUATIONSEPARATEPROC  pGLBlendEquationSeparate;
    PFNGLBLENDFUNCSEPARATEPROC      pGLBlendFuncSeparate;
    PFNGLDISABLEPROC                pGLDisable;
    PFNGLDRAWARRAYSPROC             pGLDrawArrays;
    PFNGLENABLEPROC                 pGLEnable;
    PFNGLGETTEXLEVELPARAMETERIVPROC pGLGetTexLevelParameteriv;
    PFNGLTEXPARAMETERIPROC          pGLTexParameteri;
    PFNGLUNIFORMBLOCKBINDINGPROC    pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC             pGLUseProgram;
} _ui_texture_preview;

/** Internal variables */
static const char* ui_texture_preview_renderer_sampler2d_alpha_body      = "result = vec4(textureLod(texture, uv, 0).aaa, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_depth_body      = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_red_body        = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_rgb_body        = "result = vec4(textureLod(texture, uv, 0).xyz, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_rgba_body       = "result = textureLod(texture, uv, 0);\n";
static const char* ui_texture_preview_renderer_sampler2darray_depth_body = "result = vec4(textureLod(texture, vec3(uv, layer), 0.0).xxx, 1.0);\n";
static const char* ui_texture_preview_fragment_shader_body               = "#version 430 core\n"
                                                                           "\n"
                                                                           "in      vec2         uv;\n"
                                                                           "out     vec4         result;\n"
                                                                           "uniform SAMPLER_TYPE texture;\n"
                                                                           "\n"
                                                                           "uniform dataFS\n"
                                                                           "{\n"
                                                                           "    vec2  border_width;\n"
                                                                           "    float layer;\n"
                                                                           "};\n"
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

static const char* ui_texture_preview_sampler2d_type      = "sampler2D";
static const char* ui_texture_preview_sampler2darray_type = "sampler2DArray";


/** TODO */
PRIVATE const char* _ui_texture_preview_get_program_name(ui_texture_preview_type type)
{
    const char* result = (type == UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_ALPHA) ? "UI texture preview [sampler2d alpha]" :
                         (type == UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_DEPTH) ? "UI texture preview [sampler2d depth]" :
                         (type == UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RED)   ? "UI texture preview [sampler2d red]"   :
                         (type == UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGB)   ? "UI texture preview [sampler2d rgb]"   :
                         (type == UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGBA)  ? "UI texture preview [sampler2d rgba]"  :
                                                                             "UI texture preview [sampler2DArray depth]";

    return result;
}

/** TODO */
PRIVATE void _ui_texture_preview_init_program(ui                   ui_instance,
                                              _ui_texture_preview* texture_preview_ptr)
{
    /* Create all objects */
    ral_context context = NULL;
    ral_shader  fs      = NULL;
    ral_shader  vs      = NULL;

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings(_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type),
                                                                                                     " fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings(_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type),
                                                                                                     " vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create(_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type) )
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);

    ral_shader     result_shaders[n_shader_create_info_items];


    context = ui_get_context(ui_instance);

    if (!ral_context_create_shaders(texture_preview_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    if (!ral_context_create_programs(texture_preview_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &texture_preview_ptr->program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    /* Set up FS body */
    std::string fs_body                      = ui_texture_preview_fragment_shader_body;
    const char* render_result_body           = NULL;
    const char* sampler_type_body            = NULL;
    const char* token_render_result          = "RENDER_RESULT";
    std::size_t token_render_result_location = std::string::npos;
    const char* token_sampler_type           = "SAMPLER_TYPE";
    std::size_t token_sampler_type_location  = std::string::npos;

    switch (texture_preview_ptr->preview_type)
    {
        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_ALPHA:
        {
            render_result_body = ui_texture_preview_renderer_sampler2d_alpha_body;
            sampler_type_body  = ui_texture_preview_sampler2d_type;

            break;
        }

        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_DEPTH:
        {
            render_result_body = ui_texture_preview_renderer_sampler2d_depth_body;
            sampler_type_body  = ui_texture_preview_sampler2d_type;

            break;
        }

        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RED:
        {
            render_result_body = ui_texture_preview_renderer_sampler2d_red_body;
            sampler_type_body  = ui_texture_preview_sampler2d_type;

            break;
        }

        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGB:
        {
            render_result_body = ui_texture_preview_renderer_sampler2d_rgb_body;
            sampler_type_body  = ui_texture_preview_sampler2d_type;

            break;
        }

        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2D_RGBA:
        {
            render_result_body = ui_texture_preview_renderer_sampler2d_rgba_body;
            sampler_type_body  = ui_texture_preview_sampler2d_type;

            break;
        }

        case UI_TEXTURE_PREVIEW_TYPE_SAMPLER2DARRAY_DEPTH:
        {
            render_result_body = ui_texture_preview_renderer_sampler2darray_depth_body;
            sampler_type_body  = ui_texture_preview_sampler2darray_type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture preview type requested");
        }
    } /* switch (texture_preview_ptr->preview_type) */

    while ( (token_render_result_location = fs_body.find(token_render_result) ) != std::string::npos)
    {
        fs_body.replace(token_render_result_location,
                        strlen(token_render_result),
                        render_result_body);
    }

    while ( (token_sampler_type_location = fs_body.find(token_sampler_type) ) != std::string::npos)
    {
        fs_body.replace(token_sampler_type_location,
                        strlen(token_sampler_type),
                        sampler_type_body);
    }

    /* Set up shaders */
    const system_hashed_ansi_string fs_body_has = system_hashed_ansi_string_create(fs_body.c_str() );
    const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(ui_general_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body_has);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body_has);

    /* Set up program object */
    if (!ral_program_attach_shader(texture_preview_ptr->program,
                                   fs) ||
        !ral_program_attach_shader(texture_preview_ptr->program,
                                   vs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following button instances will reuse the program */
    ui_register_program(ui_instance,
                        system_hashed_ansi_string_create(_ui_texture_preview_get_program_name(texture_preview_ptr->preview_type)),
                        texture_preview_ptr->program);

    /* Release shaders we will no longer need */
    const ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(texture_preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** TODO */
PRIVATE void _ui_texture_preview_init_texture_renderer_callback(ogl_context context,
                                                                void*       texture_preview)
{
    _ui_texture_preview* texture_preview_ptr = (_ui_texture_preview*) texture_preview;
    const raGL_program   program_raGL        = ral_context_get_program_gl(texture_preview_ptr->context,
                                                                          texture_preview_ptr->program);
    GLuint               program_raGL_id     = 0;
    system_window        window              = NULL;
    int                  window_size[2]      = {0};

    ASSERT_DEBUG_SYNC(!texture_preview_ptr->texture_initialized,
                      "TO already initialized");

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ral_context_get_property  (texture_preview_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    /* Determine preview size */
    float            preview_height_ss = 0.0f;
    float            preview_width_ss  = 0.0f;
    int              texture_height_px = 0;
    float            texture_height_ss = 0.0f;
    float            texture_h_ratio   = 0.0f;
    GLenum           texture_target    = GL_NONE;
    ral_texture_type texture_type      = RAL_TEXTURE_TYPE_UNKNOWN;
    int              texture_width_px  = 0;
    float            texture_width_ss  = 0.0f;
    float            texture_v_ratio   = 0.0f;

    ral_texture_get_property(texture_preview_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    texture_target = raGL_utils_get_ogl_texture_target_for_ral_texture_type(texture_type);

    texture_preview_ptr->pGLBindTexture(texture_target,
                                        ral_context_get_texture_gl_id(texture_preview_ptr->context,
                                                                      texture_preview_ptr->texture) );

    texture_preview_ptr->pGLGetTexLevelParameteriv(texture_target,
                                                   0,
                                                   GL_TEXTURE_HEIGHT,
                                                  &texture_height_px);
    texture_preview_ptr->pGLGetTexLevelParameteriv(texture_target,
                                                   0,
                                                   GL_TEXTURE_WIDTH,
                                                  &texture_width_px);

    ASSERT_ALWAYS_SYNC(texture_height_px != 0 && texture_width_px != 0,
                       "Input texture is undefined!");

    texture_h_ratio   = float(texture_width_px)  / float(texture_height_px);
    texture_height_ss = float(texture_height_px) / float(window_size[1]);
    texture_v_ratio   = float(texture_height_px) / float(texture_width_px);
    texture_width_ss  = float(texture_width_px)  / float(window_size[0]);

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

    texture_preview_ptr->border_width[0] = 1.0f / (float)(preview_width_ss  * window_size[0] + 1);
    texture_preview_ptr->border_width[1] = 1.0f / (float)(preview_height_ss * window_size[1] + 1);

    /* Configure the text to be shown below the preview */
    varia_text_renderer_set(texture_preview_ptr->text_renderer,
                            texture_preview_ptr->text_index,
                            system_hashed_ansi_string_get_buffer(texture_preview_ptr->name) );

    int text_height   = 0;
    int text_xy[2]    = {0};
    int text_width    = 0;

    varia_text_renderer_get_text_string_property(texture_preview_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 texture_preview_ptr->text_index,
                                                &text_height);
    varia_text_renderer_get_text_string_property(texture_preview_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 texture_preview_ptr->text_index,
                                                &text_width);

    text_xy[0] = (int) ((texture_preview_ptr->x1y1x2y2[0] + (texture_preview_ptr->x1y1x2y2[2] - texture_preview_ptr->x1y1x2y2[0] - float(text_width)  / window_size[0]) * 0.5f) * (float) window_size[0]);
    text_xy[1] = (int) ((1.0f - (texture_preview_ptr->x1y1x2y2[3] - float(text_height) / window_size[1])) * (float) window_size[1]);

    varia_text_renderer_set_text_string_property(texture_preview_ptr->text_renderer,
                                                 texture_preview_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 _ui_texture_preview_text_color);
    varia_text_renderer_set_text_string_property(texture_preview_ptr->text_renderer,
                                                 texture_preview_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                 text_xy);

    /* Retrieve uniform locations */
    const ral_program_variable* border_width_uniform_ral_ptr = NULL;
    const ral_program_variable* layer_uniform_ral_ptr        = NULL;
    const ral_program_variable* texture_uniform_ral_ptr      = NULL;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = NULL;

    ral_program_get_block_variable_by_name(texture_preview_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(texture_preview_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("layer"),
                                          &layer_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(texture_preview_ptr->program,
                                           system_hashed_ansi_string_create(""),
                                           system_hashed_ansi_string_create("texture"),
                                          &texture_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(texture_preview_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_uniform_ral_ptr);

    if (border_width_uniform_ral_ptr != NULL)
    {
        texture_preview_ptr->program_border_width_ub_offset = border_width_uniform_ral_ptr->block_offset;
    }
    else
    {
        texture_preview_ptr->program_border_width_ub_offset = -1;
    }

    if (layer_uniform_ral_ptr != NULL)
    {
        texture_preview_ptr->program_layer_ub_offset = layer_uniform_ral_ptr->block_offset;
    }
    else
    {
        texture_preview_ptr->program_layer_ub_offset = -1;
    }

    texture_preview_ptr->program_texture_ub_offset  = texture_uniform_ral_ptr->block_offset;
    texture_preview_ptr->program_x1y1x2y2_ub_offset = x1y1x2y2_uniform_ral_ptr->block_offset;

    /* Set up uniform blocks */
    ral_buffer   ub_fs_bo_ral = NULL;
    unsigned int ub_fs_index = -1;
    ral_buffer   ub_vs_bo_ral = NULL;
    unsigned int ub_vs_index = -1;

    if (texture_preview_ptr->program_ub_fs == NULL)
    {
        static const uint32_t datafs_ub_fs_bp_index = UB_FS_BP_INDEX;

        texture_preview_ptr->program_ub_fs = ral_program_block_buffer_create(texture_preview_ptr->context,
                                                                             texture_preview_ptr->program,
                                                                             system_hashed_ansi_string_create("dataFS") );

        ral_program_block_buffer_get_property  (texture_preview_ptr->program_ub_fs,
                                                RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                               &ub_fs_bo_ral);
        ral_buffer_get_property                (ub_fs_bo_ral,
                                                RAL_BUFFER_PROPERTY_SIZE,
                                               &texture_preview_ptr->program_ub_fs_bo_size);
        raGL_program_set_block_property_by_name(program_raGL,
                                                system_hashed_ansi_string_create("dataFS"),
                                                RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                               &datafs_ub_fs_bp_index);
    }

    if (texture_preview_ptr->program_ub_vs == NULL)
    {
        static const uint32_t datafs_ub_vs_bp_index = UB_VS_BP_INDEX;

        texture_preview_ptr->program_ub_vs = ral_program_block_buffer_create(texture_preview_ptr->context,
                                                                             texture_preview_ptr->program,
                                                                             system_hashed_ansi_string_create("dataVS") );

        ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_vs_bo_ral);
        ral_buffer_get_property              (ub_vs_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &texture_preview_ptr->program_ub_vs_bo_size);
        raGL_program_set_block_property_by_name(program_raGL,
                                                system_hashed_ansi_string_create("dataVS"),
                                                RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                               &datafs_ub_vs_bp_index);
    }

    /* All done */
    texture_preview_ptr->texture_initialized = true;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_deinit(void* internal_instance)
{
    _ui_texture_preview* ui_texture_preview_ptr = (_ui_texture_preview*) internal_instance;

    ral_context_delete_objects(ui_texture_preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &ui_texture_preview_ptr->program);

    varia_text_renderer_set(ui_texture_preview_ptr->text_renderer,
                            ui_texture_preview_ptr->text_index,
                            "");

    if (ui_texture_preview_ptr->program_ub_fs != NULL)
    {
        ral_program_block_buffer_release(ui_texture_preview_ptr->program_ub_fs);

        ui_texture_preview_ptr->program_ub_fs = NULL;
    }

    if (ui_texture_preview_ptr->program_ub_vs != NULL)
    {
        ral_program_block_buffer_release(ui_texture_preview_ptr->program_ub_vs);

        ui_texture_preview_ptr->program_ub_vs = NULL;
    }

    delete ui_texture_preview_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ui_texture_preview_draw(void* internal_instance)
{
    float                layer_index         = 0.0f;
    _ui_texture_preview* texture_preview_ptr = (_ui_texture_preview*) internal_instance;
    GLenum               texture_target      = GL_ZERO;
    ral_texture_type     texture_type        = RAL_TEXTURE_TYPE_UNKNOWN;

    const raGL_program   program_raGL    = ral_context_get_program_gl(texture_preview_ptr->context,
                                                                      texture_preview_ptr->program);
    GLuint               program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    /* Nothing to render if no TO is hooked up.. */
    if (texture_preview_ptr->texture == NULL)
    {
        goto end;
    }

    /* If TO was not initialized, set it up now */
    if (!texture_preview_ptr->texture_initialized)
    {
        _ui_texture_preview_init_texture_renderer_callback(ral_context_get_gl_context(texture_preview_ptr->context),
                                                           texture_preview_ptr);
    }

    /* Make sure user-requested texture is bound to zeroth texture unit before we continue */
    texture_preview_ptr->pGLBindSampler(0,  /* unit    */
                                        0); /* sampler */

    if (texture_preview_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        texture_preview_ptr->gl_pGLBindMultiTextureEXT(GL_TEXTURE0,
                                                       GL_TEXTURE_2D,
                                                       ral_context_get_texture_gl_id(texture_preview_ptr->context,
                                                                                     texture_preview_ptr->texture) );
    }
    else
    {
        texture_preview_ptr->pGLActiveTexture(GL_TEXTURE0);
        texture_preview_ptr->pGLBindTexture  (GL_TEXTURE_2D,
                                              ral_context_get_texture_gl_id(texture_preview_ptr->context,
                                                                            texture_preview_ptr->texture) );
    }

    /* For depth textures, make sure the "depth texture comparison mode" is toggled off before
     * we proceed with sampling the mip-map */
    ral_texture_get_property(texture_preview_ptr->texture,
                             RAL_TEXTURE_PROPERTY_TYPE,
                            &texture_type);

    texture_target = raGL_utils_get_ogl_texture_target_for_ral_texture_type(texture_type);

    if (texture_preview_ptr->backend_type == RAL_BACKEND_TYPE_GL)
    {
        texture_preview_ptr->gl_pGLTextureParameteriEXT(ral_context_get_texture_gl_id(texture_preview_ptr->context,
                                                                                      texture_preview_ptr->texture),
                                                        texture_target,
                                                        GL_TEXTURE_COMPARE_MODE,
                                                        GL_NONE);
    }
    else
    {
        texture_preview_ptr->pGLTexParameteri(texture_target,
                                              GL_TEXTURE_COMPARE_MODE,
                                              GL_NONE);
    }

    /* Set up uniforms */
    layer_index = (float) texture_preview_ptr->layer_shown;

    ral_program_block_buffer_set_nonarrayed_variable_value(texture_preview_ptr->program_ub_fs,
                                                           texture_preview_ptr->program_border_width_ub_offset,
                                                           texture_preview_ptr->border_width,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(texture_preview_ptr->program_ub_vs,
                                                           texture_preview_ptr->program_x1y1x2y2_ub_offset,
                                                           texture_preview_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);
    ral_program_block_buffer_set_nonarrayed_variable_value(texture_preview_ptr->program_ub_fs,
                                                           texture_preview_ptr->program_layer_ub_offset,
                                                          &layer_index,
                                                           sizeof(float) );

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
    GLuint      program_ub_fs_bo_id           = 0;
    raGL_buffer program_ub_fs_bo_raGL         = NULL;
    ral_buffer  program_ub_fs_bo_ral          = NULL;
    uint32_t    program_ub_fs_bo_start_offset = -1;
    GLuint      program_ub_vs_bo_id           = 0;
    raGL_buffer program_ub_vs_bo_raGL         = NULL;
    ral_buffer  program_ub_vs_bo_ral          = NULL;
    uint32_t    program_ub_vs_bo_start_offset = -1;

    ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_vs_bo_ral);

    program_ub_fs_bo_raGL = ral_context_get_buffer_gl(texture_preview_ptr->context,
                                                      program_ub_fs_bo_ral);
    program_ub_vs_bo_raGL = ral_context_get_buffer_gl(texture_preview_ptr->context,
                                                      program_ub_vs_bo_ral);

    raGL_buffer_get_property(program_ub_fs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_ub_fs_bo_id);
    raGL_buffer_get_property(program_ub_fs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_fs_bo_start_offset);
    raGL_buffer_get_property(program_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_ub_vs_bo_id);
    raGL_buffer_get_property(program_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_ub_vs_bo_start_offset);

    texture_preview_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            UB_FS_BP_INDEX,
                                            program_ub_fs_bo_id,
                                            program_ub_fs_bo_start_offset,
                                            texture_preview_ptr->program_ub_fs_bo_size);
    texture_preview_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            UB_VS_BP_INDEX,
                                            program_ub_vs_bo_id,
                                            program_ub_vs_bo_start_offset,
                                            texture_preview_ptr->program_ub_vs_bo_size);

    ral_program_block_buffer_sync(texture_preview_ptr->program_ub_fs);
    ral_program_block_buffer_sync(texture_preview_ptr->program_ub_vs);

    texture_preview_ptr->pGLUseProgram(program_raGL_id);
    texture_preview_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                       0,
                                       4);

end:
    ;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_get_property(const void*         texture_preview,
                                            ui_control_property property,
                                            void*               out_result)
{
    _ui_texture_preview* texture_preview_ptr = (_ui_texture_preview*) texture_preview;

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = texture_preview_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR:
        {
            memcpy(out_result,
                   texture_preview_ptr->blend_color,
                   sizeof(texture_preview_ptr->blend_color) );

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_equation_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_equation_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_dst_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_dst_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_ALPHA:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_src_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_RGB:
        {
            *(GLenum*) out_result = texture_preview_ptr->blend_function_src_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED:
        {
            *(bool*) out_result = texture_preview_ptr->is_blending_enabled;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_SHOW_TEXTURE_NAME:
        {
            varia_text_renderer_get_text_string_property(texture_preview_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                         texture_preview_ptr->text_index,
                                                         out_result);

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_RAL:
        {
            *(ral_texture*) out_result = texture_preview_ptr->texture;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ui_texture_preview_init(ui                        instance,
                                     varia_text_renderer       text_renderer,
                                     system_hashed_ansi_string name,
                                     const float*              x1y1,
                                     const float*              max_size,
                                     ral_texture               to,
                                     ui_texture_preview_type   preview_type)
{
    _ui_texture_preview* new_texture_preview_ptr = new (std::nothrow) _ui_texture_preview;

    ASSERT_ALWAYS_SYNC(new_texture_preview_ptr != NULL,
                       "Out of memory");

    if (new_texture_preview_ptr != NULL)
    {
        /* Initialize fields */
        memset(new_texture_preview_ptr,
               0,
               sizeof(_ui_texture_preview) );
        memcpy(new_texture_preview_ptr->max_size,
               max_size,
               sizeof(float) * 2);

        new_texture_preview_ptr->x1y1x2y2[0]         =     x1y1[0];
        new_texture_preview_ptr->x1y1x2y2[1]         = 1 - x1y1[1];

        new_texture_preview_ptr->context             = ui_get_context(instance);
        new_texture_preview_ptr->backend_type        = RAL_BACKEND_TYPE_UNKNOWN;
        new_texture_preview_ptr->layer_shown         = 1;
        new_texture_preview_ptr->name                = name;
        new_texture_preview_ptr->preview_type        = preview_type;
        new_texture_preview_ptr->text_renderer       = text_renderer;
        new_texture_preview_ptr->text_index          = varia_text_renderer_add_string(text_renderer);
        new_texture_preview_ptr->texture             = to;
        new_texture_preview_ptr->texture_initialized = false;
        new_texture_preview_ptr->visible             = true;

        /* Set blending states */
        memset(new_texture_preview_ptr->blend_color,
               0,
               sizeof(new_texture_preview_ptr->blend_color) );

        new_texture_preview_ptr->blend_equation_alpha     = GL_FUNC_ADD;
        new_texture_preview_ptr->blend_equation_rgb       = GL_FUNC_ADD;
        new_texture_preview_ptr->blend_function_dst_alpha = GL_ZERO;
        new_texture_preview_ptr->blend_function_dst_rgb   = GL_ZERO;
        new_texture_preview_ptr->blend_function_src_alpha = GL_ONE;
        new_texture_preview_ptr->blend_function_src_rgb   = GL_ONE;
        new_texture_preview_ptr->is_blending_enabled      = false;

        /* Cache GL func pointers */
        ral_context_get_property(new_texture_preview_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &new_texture_preview_ptr->backend_type);

        if (new_texture_preview_ptr->backend_type == RAL_BACKEND_TYPE_ES)
        {
            ogl_context                       context_es       = NULL;
            const ogl_context_es_entrypoints* entry_points_ptr = NULL;

            context_es = ral_context_get_gl_context(new_texture_preview_ptr->context);

            ogl_context_get_property(context_es,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);

            new_texture_preview_ptr->gl_pGLBindMultiTextureEXT  = NULL;
            new_texture_preview_ptr->gl_pGLTextureParameteriEXT = NULL;
            new_texture_preview_ptr->pGLActiveTexture           = entry_points_ptr->pGLActiveTexture;
            new_texture_preview_ptr->pGLBindBufferRange         = entry_points_ptr->pGLBindBufferRange;
            new_texture_preview_ptr->pGLBindSampler             = entry_points_ptr->pGLBindSampler;
            new_texture_preview_ptr->pGLBindTexture             = entry_points_ptr->pGLBindTexture;
            new_texture_preview_ptr->pGLBlendColor              = entry_points_ptr->pGLBlendColor;
            new_texture_preview_ptr->pGLBlendEquationSeparate   = entry_points_ptr->pGLBlendEquationSeparate;
            new_texture_preview_ptr->pGLBlendFuncSeparate       = entry_points_ptr->pGLBlendFuncSeparate;
            new_texture_preview_ptr->pGLDisable                 = entry_points_ptr->pGLDisable;
            new_texture_preview_ptr->pGLDrawArrays              = entry_points_ptr->pGLDrawArrays;
            new_texture_preview_ptr->pGLEnable                  = entry_points_ptr->pGLEnable;
            new_texture_preview_ptr->pGLGetTexLevelParameteriv  = entry_points_ptr->pGLGetTexLevelParameteriv;
            new_texture_preview_ptr->pGLTexParameteri           = entry_points_ptr->pGLTexParameteri;
            new_texture_preview_ptr->pGLUniformBlockBinding     = entry_points_ptr->pGLUniformBlockBinding;
            new_texture_preview_ptr->pGLUseProgram              = entry_points_ptr->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(new_texture_preview_ptr->backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized context type");

            ogl_context                                               context_gl           = NULL;
            const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points_ptr = NULL;
            const ogl_context_gl_entrypoints*                         entry_points_ptr     = NULL;

            context_gl = ral_context_get_gl_context(new_texture_preview_ptr->context);

            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);
            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                    &dsa_entry_points_ptr);

            new_texture_preview_ptr->gl_pGLBindMultiTextureEXT  = dsa_entry_points_ptr->pGLBindMultiTextureEXT;
            new_texture_preview_ptr->gl_pGLTextureParameteriEXT = dsa_entry_points_ptr->pGLTextureParameteriEXT;
            new_texture_preview_ptr->pGLActiveTexture           = entry_points_ptr->pGLActiveTexture;
            new_texture_preview_ptr->pGLBindBufferRange         = entry_points_ptr->pGLBindBufferRange;
            new_texture_preview_ptr->pGLBindSampler             = entry_points_ptr->pGLBindSampler;
            new_texture_preview_ptr->pGLBindTexture             = entry_points_ptr->pGLBindTexture;
            new_texture_preview_ptr->pGLBlendColor              = entry_points_ptr->pGLBlendColor;
            new_texture_preview_ptr->pGLBlendEquationSeparate   = entry_points_ptr->pGLBlendEquationSeparate;
            new_texture_preview_ptr->pGLBlendFuncSeparate       = entry_points_ptr->pGLBlendFuncSeparate;
            new_texture_preview_ptr->pGLDisable                 = entry_points_ptr->pGLDisable;
            new_texture_preview_ptr->pGLDrawArrays              = entry_points_ptr->pGLDrawArrays;
            new_texture_preview_ptr->pGLEnable                  = entry_points_ptr->pGLEnable;
            new_texture_preview_ptr->pGLGetTexLevelParameteriv  = entry_points_ptr->pGLGetTexLevelParameteriv;
            new_texture_preview_ptr->pGLTexParameteri           = entry_points_ptr->pGLTexParameteri;
            new_texture_preview_ptr->pGLUniformBlockBinding     = entry_points_ptr->pGLUniformBlockBinding;
            new_texture_preview_ptr->pGLUseProgram              = entry_points_ptr->pGLUseProgram;
        }

        /* Retrieve the rendering program */
        new_texture_preview_ptr->program = ui_get_registered_program(instance,
                                                                     system_hashed_ansi_string_create(_ui_texture_preview_get_program_name(preview_type)) );

        if (new_texture_preview_ptr->program == NULL)
        {
            _ui_texture_preview_init_program(instance,
                                             new_texture_preview_ptr);

            ASSERT_DEBUG_SYNC(new_texture_preview_ptr->program != NULL,
                              "Could not initialize texture preview UI program");
        } /* if (new_texture_preview->program == NULL) */

        /* Set up rendering program. This must not be called if the requested TO
         * is NULL.*/
        if (to != NULL)
        {
            ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_texture_preview_ptr->context),
                                                             _ui_texture_preview_init_texture_renderer_callback,
                                                             new_texture_preview_ptr);
        }
    } /* if (new_texture_preview_ptr != NULL) */

    return (void*) new_texture_preview_ptr;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_set_property(void*               texture_preview,
                                            ui_control_property property,
                                            const void*         data)
{
    _ui_texture_preview* texture_preview_ptr = (_ui_texture_preview*) texture_preview;

    switch (property)
    {
        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR:
        {
            memcpy(texture_preview_ptr->blend_color,
                   data,
                   sizeof(texture_preview_ptr->blend_color) );

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_ALPHA:
        {
            texture_preview_ptr->blend_equation_alpha = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_EQUATION_RGB:
        {
            texture_preview_ptr->blend_equation_rgb = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_ALPHA:
        {
            texture_preview_ptr->blend_function_dst_alpha = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_DST_RGB:
        {
            texture_preview_ptr->blend_function_dst_rgb = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_ALPHA:
        {
            texture_preview_ptr->blend_function_src_alpha = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FUNCTION_SRC_RGB:
        {
            texture_preview_ptr->blend_function_src_rgb = *(GLenum*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED:
        {
            texture_preview_ptr->is_blending_enabled = *(bool*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_LAYER_SHOWN:
        {
            texture_preview_ptr->layer_shown = *(GLuint*) data;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_SHOW_TEXTURE_NAME:
        {
            varia_text_renderer_set_text_string_property(texture_preview_ptr->text_renderer,
                                                         texture_preview_ptr->text_index,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                         data);

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_RAL:
        {
            texture_preview_ptr->texture             = *(ral_texture*) data;
            texture_preview_ptr->texture_initialized = false;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value requested");
        }
    } /* switch (property) */
}