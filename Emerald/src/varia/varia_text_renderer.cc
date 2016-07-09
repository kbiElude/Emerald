/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * Shader storage buffer data layout is as follows
 *
 * # Horizontal delta (float)
 * # Top-left  U      (float) taken from font table texture.
 * # Top-left  V      (float) taken from font table texture.
 * # Bot-right U      (float) taken from font table texture.
 * # Bot-right V      (float) taken from font table texture.
 * # Font width       (float) in pixels.
 * # Font height      (float) in pixels.
 *
 * Repeated as many times as there are characters in the string.
 *
 */
#include "shared.h"
#include "gfx/gfx_bfg_font_table.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"
#include "varia/varia_text_renderer.h"
#include <map>

#define DEFAULT_COLOR_R (1)
#define DEFAULT_COLOR_G (0)
#define DEFAULT_COLOR_B (0)
#define DEFAULT_SCALE   (1)
#define MAX_TEXT_LENGTH "256"

/** Internal types */
typedef struct
{
    char*        data_buffer_contents;
    uint32_t     data_buffer_contents_length;
    uint32_t     data_buffer_contents_size;
    bool         dirty;
    unsigned int shader_storage_buffer_offset_alignment;

    ral_buffer data_buffer;

    GLuint                   fsdata_index;
    ral_program_block_buffer fsdata_ub;
    GLuint                   vsdata_index;
    ral_program_block_buffer vsdata_ub;

    float default_color[3];
    float default_scale;

    system_critical_section   draw_cs;
    GLuint                    draw_text_fragment_shader_color_ub_offset;
    GLuint                    draw_text_fragment_shader_font_table_location;
    ral_program               draw_text_program;
    ral_program_block_buffer  draw_text_program_data_ssb;
    GLuint                    draw_text_vertex_shader_n_origin_character_ub_offset;
    GLuint                    draw_text_vertex_shader_scale_ub_offset;

    gfx_bfg_font_table        font_table;
    ral_texture               font_table_to;

    system_hashed_ansi_string name;
    ral_context               owner_context;
    uint32_t                  screen_height;
    uint32_t                  screen_width;
    system_resizable_vector   strings;

    ral_context  context;

    /* GL function pointers cache */
    PFNGLPOLYGONMODEPROC           gl_pGLPolygonMode;
    PFNGLTEXTUREPARAMETERIEXTPROC  gl_pGLTextureParameteriEXT;
    PFNGLACTIVETEXTUREPROC         pGLActiveTexture;
    PFNGLBINDBUFFERPROC            pGLBindBuffer;
    PFNGLBINDBUFFERRANGEPROC       pGLBindBufferRange;
    PFNGLBINDSAMPLERPROC           pGLBindSampler;
    PFNGLBINDTEXTUREPROC           pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC       pGLBindVertexArray;
    PFNGLBLENDEQUATIONPROC         pGLBlendEquation;
    PFNGLBLENDFUNCPROC             pGLBlendFunc;
    PFNGLDELETEVERTEXARRAYSPROC    pGLDeleteVertexArrays;
    PFNGLDISABLEPROC               pGLDisable;
    PFNGLDRAWARRAYSPROC            pGLDrawArrays;
    PFNGLENABLEPROC                pGLEnable;
    PFNGLGENVERTEXARRAYSPROC       pGLGenVertexArrays;
    PFNGLGENERATEMIPMAPPROC        pGLGenerateMipmap;
    PFNGLPROGRAMUNIFORM1IPROC      pGLProgramUniform1i;
    PFNGLSCISSORPROC               pGLScissor;
    PFNGLTEXBUFFERRANGEPROC        pGLTexBufferRange;
    PFNGLTEXPARAMETERIPROC         pGLTexParameteri;
    PFNGLUSEPROGRAMPROC            pGLUseProgram;
    PFNGLVIEWPORTPROC              pGLViewport;

    REFCOUNT_INSERT_VARIABLES
} _varia_text_renderer;

typedef struct
{
    bool           visible;
    GLfloat        color[3];
    unsigned int   height_px;
    float          position[2];
    float          scale;
    GLint          scissor_box[4];
    unsigned char* string;
    unsigned int   string_buffer_length;
    unsigned int   string_length;
    unsigned int   width_px;

} _varia_text_renderer_text_string;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(varia_text_renderer,
                               varia_text_renderer,
                              _varia_text_renderer);


/* Private definitions */
static const char* fragment_shader_template = "#ifdef GL_ES\n"
                                              "    precision highp float;\n"
                                              "#endif\n"
                                              "\n"
                                              "in  vec2 vertex_shader_uv;\n"
                                              "out vec4 result;\n"
                                              "\n"
                                              "uniform FSData\n"
                                              "{\n"
                                              "    vec3 color;\n"
                                              "};\n"
                                              "\n"
                                              "uniform sampler2D font_table;\n"
                                              "\n"
                                              "void main()\n"
                                              "{\n"
                                              "    vec4 texel = textureLod(font_table, vertex_shader_uv, 0.0);\n"
                                              "\n"
                                              "    result = vec4(color.xyz * texel.x, (texel.x > 0.9) ? 1 : 0);\n"
                                              "}\n";

static const char* vertex_shader_template = "#ifdef GL_ES\n"
                                            "    precision highp float;\n"
                                            "    precision highp samplerBuffer;\n"
                                            "#else\n"
                                            "    #extension GL_ARB_shader_storage_buffer_object : require\n"
                                            "#endif\n"
                                            "\n"
                                            "buffer dataSSB\n"
                                            "{\n"
                                            "    restrict readonly vec4 data[];\n"
                                            "};\n"
                                            "\n"
                                            "uniform VSData\n"
                                            "{\n"
                                            "    int   n_origin_character;\n"
                                            "    float scale;\n"
                                            "};\n"
                                            "\n"
                                            "out vec2 vertex_shader_uv;\n"
                                            "\n"
                                            "\n"
                                            "void main()\n"
                                            "{\n"
                                            "   int   character_id    = gl_VertexID / 6;\n"
                                            "   vec2  vertex_data;\n"
                                            "\n"
                                            "    switch (gl_VertexID % 6)\n"
                                            "    {\n"
                                            "        case 0: vertex_data = vec2(0.0, 1.0); break;\n"
                                            "        case 1: vertex_data = vec2(0.0, 0.0); break;\n"
                                            "        case 2:\n"
                                            "        case 3: vertex_data = vec2(1.0, 0.0); break;\n"
                                            "        case 4: vertex_data = vec2(1.0, 1.0); break;\n"
                                            "        case 5: vertex_data = vec2(0.0, 1.0); break;\n"
                                            "    }\n"
                                            "\n"
                                            "   vec2  x1_y1_origin = data[n_origin_character * 2].xy;\n"
                                            "   vec4  x1_y1_u1_v1  = data[character_id * 2];\n"
                                            "   vec4  u2_v2_w_h    = data[character_id * 2 + 1];\n"
                                            "   float u_delta      = u2_v2_w_h.x - x1_y1_u1_v1.z;\n"
                                            "   float v_delta      = u2_v2_w_h.y - x1_y1_u1_v1.w;\n"
                                            "\n"
                                            /* Scale x1/y1 to <-1, 1> */
                                            "   x1_y1_u1_v1.xy   = x1_y1_u1_v1.xy * vec2(2.0) - vec2(1.0);\n"
                                            "   x1_y1_origin     = x1_y1_origin   * vec2(2.0) - vec2(1.0);\n"
                                            /* Output */
                                            "   gl_Position      = vec4(x1_y1_u1_v1.x + (1.0 - scale) * (x1_y1_origin.x - x1_y1_u1_v1.x) + scale * (vertex_data.x * u2_v2_w_h.z * 2.0),\n"
                                            "                           x1_y1_u1_v1.y + (1.0 - scale) * (x1_y1_origin.y - x1_y1_u1_v1.y) + scale * (vertex_data.y * u2_v2_w_h.w * 2.0),\n"
                                            "                           0.0, 1.0);\n"
                                            "   vertex_shader_uv = vec2(vertex_data.x * u_delta +       x1_y1_u1_v1.z, \n"
                                            "                          -vertex_data.y * v_delta + 1.0 - x1_y1_u1_v1.w);\n"
                                            "}\n";


/* Private forward declarations */
PRIVATE void _varia_text_renderer_construction_callback_from_renderer        (ogl_context context,
                                                                              void*       text);
PRIVATE void _varia_text_renderer_create_font_table_to_callback_from_renderer(ogl_context context,
                                                                              void*       text);
PRIVATE void _varia_text_renderer_destruction_callback_from_renderer         (ogl_context context,
                                                                              void*       text);
PRIVATE void _varia_text_renderer_draw_callback_from_renderer                (ogl_context context,
                                                                              void*       text);
PRIVATE void _varia_text_renderer_release                                    (void*       text);
PRIVATE void _varia_text_renderer_update_vram_data_storage                   (ogl_context context,
                                                                              void*       text);

/* Private functions */

/** TODO */
PRIVATE void _varia_text_renderer_construction_callback_from_renderer(ogl_context context,
                                                                      void*       text)
{
    raGL_program          draw_text_program_raGL    = nullptr;
    GLuint                draw_text_program_raGL_id = -1;
    _varia_text_renderer* text_ptr                  = (_varia_text_renderer*) text;

    if (context == nullptr)
    {
        context = ral_context_get_gl_context(text_ptr->context);
    }

    const ral_shader_create_info draw_text_fs_create_info =
    {
        system_hashed_ansi_string_create("ogl_text fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info draw_text_vs_create_info =
    {
        system_hashed_ansi_string_create("ogl_text vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };
    const ral_shader_create_info shader_create_info_items[] =
    {
        draw_text_fs_create_info,
        draw_text_vs_create_info
    };
    const uint32_t n_shader_create_info_items                 = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items] = { nullptr };

    const ral_program_create_info draw_text_program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("ogl_text program")
    };


    if (!ral_context_create_shaders(text_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    if (!ral_context_create_programs(text_ptr->context,
                                     1, /* n_create_info_items */
                                    &draw_text_program_create_info,
                                    &text_ptr->draw_text_program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    /* Prepare the bodies */
    ral_backend_type  backend_type = RAL_BACKEND_TYPE_UNKNOWN;
    std::stringstream fs_sstream;
    std::stringstream vs_sstream;

    ral_context_get_property(text_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    if (backend_type == RAL_BACKEND_TYPE_ES)
    {
        fs_sstream << "#version 310 es\n"
                      "\n"
                   << fragment_shader_template;

        vs_sstream << "#version 310 es\n"
                      "\n"
                   << vertex_shader_template;
    }
    else
    {
        ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                          "Unrecognized context type");

        fs_sstream << "#version 430 core\n"
                      "\n"
                   << fragment_shader_template;

        vs_sstream << "#version 430 core\n"
                      "\n"
                   << vertex_shader_template;
    }

    /* Assign the bodies to the shaders */
    const system_hashed_ansi_string fs_body_has = system_hashed_ansi_string_create(fs_sstream.str().c_str() );
    const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(vs_sstream.str().c_str() );

    ral_shader_set_property(result_shaders[0],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body_has);
    ral_shader_set_property(result_shaders[1],
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body_has);

    if (!ral_program_attach_shader(text_ptr->draw_text_program,
                                   result_shaders[0],
                                   true /* async */) ||
        !ral_program_attach_shader(text_ptr->draw_text_program,
                                   result_shaders[1],
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link text drawing program.");
    }

    /* Retrieve uniform locations */
    const ral_program_variable*   fragment_shader_color_ral_ptr            = nullptr;
    const _raGL_program_variable* fragment_shader_font_table_raGL_ptr      = nullptr;
    const ral_program_variable*   vertex_shader_n_origin_character_ral_ptr = nullptr;
    const ral_program_variable*   vertex_shader_scale_ral_ptr              = nullptr;

    draw_text_program_raGL = ral_context_get_program_gl(text_ptr->context,
                                                        text_ptr->draw_text_program);

    raGL_program_get_property(draw_text_program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &draw_text_program_raGL_id);

    if (!ral_program_get_block_variable_by_name(text_ptr->draw_text_program,
                                                system_hashed_ansi_string_create("FSData"),
                                                system_hashed_ansi_string_create("color"),
                                               &fragment_shader_color_ral_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve color uniform descriptor.");
    }

    if (!raGL_program_get_uniform_by_name(draw_text_program_raGL,
                                          system_hashed_ansi_string_create("font_table"),
                                         &fragment_shader_font_table_raGL_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve font table uniform descriptor.");
    }

    if (!ral_program_get_block_variable_by_name(text_ptr->draw_text_program,
                                                system_hashed_ansi_string_create("VSData"),
                                                system_hashed_ansi_string_create("n_origin_character"),
                                               &vertex_shader_n_origin_character_ral_ptr))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve n origin character uniform descriptor.");
    }

    if (!ral_program_get_block_variable_by_name(text_ptr->draw_text_program,
                                                system_hashed_ansi_string_create("VSData"),
                                                system_hashed_ansi_string_create("scale"),
                                               &vertex_shader_scale_ral_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve scale uniform descriptor.");
    }

    text_ptr->draw_text_program_data_ssb = ral_program_block_buffer_create(text_ptr->context,
                                                                           text_ptr->draw_text_program,
                                                                           system_hashed_ansi_string_create("dataSSB") );

    text_ptr->draw_text_fragment_shader_color_ub_offset            = fragment_shader_color_ral_ptr->block_offset;
    text_ptr->draw_text_fragment_shader_font_table_location        = fragment_shader_font_table_raGL_ptr->location;
    text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset = vertex_shader_n_origin_character_ral_ptr->block_offset;
    text_ptr->draw_text_vertex_shader_scale_ub_offset              = vertex_shader_scale_ral_ptr->block_offset;

    ASSERT_DEBUG_SYNC(text_ptr->draw_text_fragment_shader_color_ub_offset != -1,
                      "FSData color uniform UB offset is -1");
    ASSERT_DEBUG_SYNC(text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset != -1,
                      "VSData n_origin_character UB offset is -1");
    ASSERT_DEBUG_SYNC(text_ptr->draw_text_vertex_shader_scale_ub_offset != -1,
                      "VSData scale UB offset is -1");

    /* Set up samplers */
    text_ptr->pGLProgramUniform1i(draw_text_program_raGL_id,
                                  text_ptr->draw_text_fragment_shader_font_table_location,
                                  1);

    /* Delete the shader objects */
    ral_context_delete_objects(text_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shader_create_info_items,
                               (const void**) result_shaders);

    /* Retrieve uniform block instances. */
    text_ptr->fsdata_ub = ral_program_block_buffer_create(text_ptr->context,
                                                          text_ptr->draw_text_program,
                                                          system_hashed_ansi_string_create("FSData") );
    text_ptr->vsdata_ub = ral_program_block_buffer_create(text_ptr->context,
                                                          text_ptr->draw_text_program,
                                                          system_hashed_ansi_string_create("VSData") );

    raGL_program_get_block_property_by_name(draw_text_program_raGL,
                                            system_hashed_ansi_string_create("FSData"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &text_ptr->fsdata_index);
    raGL_program_get_block_property_by_name(draw_text_program_raGL,
                                            system_hashed_ansi_string_create("VSData"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &text_ptr->vsdata_index);

    ASSERT_DEBUG_SYNC(text_ptr->fsdata_index != text_ptr->vsdata_index,
                      "BPs match");
}

/** Please see header for specification */
PRIVATE void _varia_text_renderer_create_font_table_to_callback_from_renderer(ogl_context context,
                                                                              void*       text)
{
    ral_backend_type      backend_type = RAL_BACKEND_TYPE_UNKNOWN;
    _varia_text_renderer* text_ptr     = (_varia_text_renderer*) text;

    ral_context_get_property(text_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    /* Retrieve font table properties */
    const unsigned char* font_table_data_ptr = gfx_bfg_font_table_get_data_pointer(text_ptr->font_table);
    uint32_t             font_table_height   = 0;
    uint32_t             font_table_width    = 0;

    gfx_bfg_font_table_get_data_properties(text_ptr->font_table,
                                          &font_table_width,
                                          &font_table_height);

    /* Create the descriptor */
    ral_texture_create_info         to_create_info;
    const system_hashed_ansi_string to_name = system_hashed_ansi_string_create_by_merging_two_strings("Text renderer ",
                                                                                                     system_hashed_ansi_string_get_buffer(text_ptr->name) );

    std::shared_ptr<ral_texture_mipmap_client_sourced_update_info> to_update_ptr(new ral_texture_mipmap_client_sourced_update_info() );

    to_create_info.base_mipmap_depth      = 1;
    to_create_info.base_mipmap_height     = font_table_height;
    to_create_info.base_mipmap_width      = font_table_width;
    to_create_info.fixed_sample_locations = false;
    to_create_info.format                 = RAL_TEXTURE_FORMAT_RGB8_UNORM;
    to_create_info.n_layers               = 1;
    to_create_info.n_samples              = 1;
    to_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    to_create_info.usage                  = RAL_TEXTURE_USAGE_SAMPLED_BIT;
    to_create_info.use_full_mipmap_chain  = false;

    to_update_ptr->data                    = font_table_data_ptr;
    to_update_ptr->data_row_alignment      = 1;
    to_update_ptr->data_size               = 3 * font_table_width * font_table_height;
    to_update_ptr->data_type               = RAL_TEXTURE_DATA_TYPE_UBYTE;
    to_update_ptr->pfn_delete_handler_proc = nullptr;
    to_update_ptr->n_layer                 = 0;
    to_update_ptr->n_mipmap                = 0;
    to_update_ptr->region_size[0]          = font_table_width;
    to_update_ptr->region_size[1]          = font_table_height;
    to_update_ptr->region_size[2]          = 0;
    to_update_ptr->region_start_offset[0]  = 0;
    to_update_ptr->region_start_offset[1]  = 0;
    to_update_ptr->region_start_offset[2]  = 0;

    ral_context_create_textures                   (text_ptr->context,
                                                   1, /* n_textures */
                                                  &to_create_info,
                                                  &text_ptr->font_table_to);
    ral_texture_set_mipmap_data_from_client_memory(text_ptr->font_table_to,
                                                   1, /* n_updates */
                                                  &to_update_ptr,
                                                   true /* async */);

    text_ptr->pGLBindTexture   (GL_TEXTURE_2D,
                                ral_context_get_texture_gl_id(text_ptr->context,
                                                              text_ptr->font_table_to) );
    text_ptr->pGLTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_S,
                                GL_CLAMP_TO_EDGE);
    text_ptr->pGLTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_WRAP_T,
                                GL_CLAMP_TO_EDGE);
    text_ptr->pGLTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
    text_ptr->pGLTexParameteri (GL_TEXTURE_2D,
                                GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);

    /* Note that texture data is stored in BGR order. We would have just gone with GL_BGR
     * format under GL context, but this is not supported under ES. What we can do, however,
     * is use GL_TEXTURE_SWIZZLE parameters of the texture object. */
    text_ptr->pGLTexParameteri(GL_TEXTURE_2D,
                               GL_TEXTURE_SWIZZLE_R,
                               GL_BLUE);
    text_ptr->pGLTexParameteri(GL_TEXTURE_2D,
                               GL_TEXTURE_SWIZZLE_B,
                               GL_RED);
}

/** Please see header for specification */
PRIVATE void _varia_text_renderer_destruction_callback_from_renderer(ogl_context context,
                                                                     void*       text)
{
    _varia_text_renderer* text_ptr = (_varia_text_renderer*) text;

    /* First, free all objects that are not global */
    if (text_ptr->data_buffer != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   (const void**) &text_ptr->data_buffer);

        text_ptr->data_buffer = nullptr;
    }

    if (text_ptr->fsdata_ub != nullptr)
    {
        ral_program_block_buffer_release(text_ptr->fsdata_ub);

        text_ptr->fsdata_ub = nullptr;
    }

    if (text_ptr->vsdata_ub != nullptr)
    {
        ral_program_block_buffer_release(text_ptr->vsdata_ub);

        text_ptr->vsdata_ub = nullptr;
    }

    /* Get rid of all the shaders and programs */
    ral_context_delete_objects(text_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &text_ptr->draw_text_program);

    ral_context_delete_objects(text_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               1, /* n_textures */
                               (const void**) &text_ptr->font_table_to);

    if (text_ptr->draw_text_program_data_ssb != nullptr)
    {
        ral_program_block_buffer_release(text_ptr->draw_text_program_data_ssb);

        text_ptr->draw_text_program_data_ssb = nullptr;
    }

    text_ptr->draw_text_program                                    = nullptr;
    text_ptr->draw_text_fragment_shader_color_ub_offset            = -1;
    text_ptr->draw_text_fragment_shader_font_table_location        = -1;
    text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset = -1;
    text_ptr->draw_text_vertex_shader_scale_ub_offset              = -1;
}

/** Please see header for specification */
PRIVATE void _varia_text_renderer_draw_callback_from_renderer(ogl_context context,
                                                              void*       text)
{
    ral_backend_type      backend_type = RAL_BACKEND_TYPE_UNKNOWN;
    uint32_t              fb_resolution[2];
    uint32_t              n_strings    = 0;
    _varia_text_renderer* text_ptr     = (_varia_text_renderer*) text;

    const raGL_program program_raGL    = ral_context_get_program_gl(text_ptr->context,
                                                                    text_ptr->draw_text_program);
    GLuint             program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ral_context_get_property            (text_ptr->context,
                                         RAL_CONTEXT_PROPERTY_SYSTEM_FB_SIZE,
                                         fb_resolution);
    ral_context_get_property            (text_ptr->context,
                                         RAL_CONTEXT_PROPERTY_BACKEND,
                                        &backend_type);
    system_resizable_vector_get_property(text_ptr->strings,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_strings);

    system_critical_section_enter(text_ptr->draw_cs);
    {
        text_ptr->pGLViewport(0,
                              0,
                              fb_resolution[0],
                              fb_resolution[1]);

        /* Update underlying helper data buffer contents - only if the "dirty flag" is on */
        if (text_ptr->dirty)
        {
            _varia_text_renderer_update_vram_data_storage(context,
                                                          text);

            text_ptr->dirty = false;
        }

        /* Carry on */
        if (n_strings > 0)
        {
            GLuint vao_id = 0;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                                    &vao_id);

            /* Mark the text drawing program as active */
            if (backend_type == RAL_BACKEND_TYPE_GL)
            {
                text_ptr->gl_pGLPolygonMode(GL_FRONT_AND_BACK,
                                            GL_FILL);
            }

            text_ptr->pGLUseProgram (program_raGL_id);

            /* Bind data BO to the 0-th SSBO BP */
            GLuint      data_buffer_id                = 0;
            raGL_buffer data_buffer_raGL              = nullptr;
            uint32_t    data_buffer_raGL_start_offset = -1;
            uint32_t    data_buffer_ral_start_offset  = -1;

            data_buffer_raGL = ral_context_get_buffer_gl(text_ptr->context,
                                                         text_ptr->data_buffer);

            raGL_buffer_get_property(data_buffer_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &data_buffer_id);
            raGL_buffer_get_property(data_buffer_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &data_buffer_raGL_start_offset);
            ral_buffer_get_property (text_ptr->data_buffer,
                                     RAL_BUFFER_PROPERTY_START_OFFSET,
                                    &data_buffer_ral_start_offset);

            text_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                         0, /* index */
                                         data_buffer_id,
                                         data_buffer_raGL_start_offset + data_buffer_ral_start_offset,
                                         text_ptr->data_buffer_contents_size);

            /* Set up texture units */
            text_ptr->pGLActiveTexture(GL_TEXTURE1);
            text_ptr->pGLBindSampler  (1,  /* unit   */
                                       0); /* sampler*/
            text_ptr->pGLBindTexture  (GL_TEXTURE_2D,
                                       ral_context_get_texture_gl_id(text_ptr->context,
                                                                     text_ptr->font_table_to) );

            /* Draw! */
            GLuint      ub_fsdata_bo_id                =  0;
            raGL_buffer ub_fsdata_bo_raGL              = nullptr;
            uint32_t    ub_fsdata_bo_raGL_start_offset = -1;
            ral_buffer  ub_fsdata_bo_ral               = nullptr;
            uint32_t    ub_fsdata_bo_ral_start_offset  = -1;
            uint32_t    ub_fsdata_bo_size              =  0;
            GLuint      ub_vsdata_bo_id                =  0;
            raGL_buffer ub_vsdata_bo_raGL              = nullptr;
            uint32_t    ub_vsdata_bo_raGL_start_offset = -1;
            ral_buffer  ub_vsdata_bo_ral               = nullptr;
            uint32_t    ub_vsdata_bo_ral_start_offset  = -1;
            uint32_t    ub_vsdata_bo_size              =  0;

            ral_program_block_buffer_get_property(text_ptr->fsdata_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &ub_fsdata_bo_ral);
            ral_program_block_buffer_get_property(text_ptr->vsdata_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &ub_vsdata_bo_ral);

            ub_fsdata_bo_raGL = ral_context_get_buffer_gl(text_ptr->context,
                                                          ub_fsdata_bo_ral);
            ub_vsdata_bo_raGL = ral_context_get_buffer_gl(text_ptr->context,
                                                          ub_vsdata_bo_ral);

            raGL_buffer_get_property(ub_fsdata_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &ub_fsdata_bo_id);
            raGL_buffer_get_property(ub_fsdata_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &ub_fsdata_bo_raGL_start_offset);
            raGL_buffer_get_property(ub_vsdata_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_ID,
                                    &ub_vsdata_bo_id);
            raGL_buffer_get_property(ub_vsdata_bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &ub_vsdata_bo_raGL_start_offset);

            ral_buffer_get_property(ub_fsdata_bo_ral,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &ub_fsdata_bo_size);
            ral_buffer_get_property(ub_fsdata_bo_ral,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &ub_fsdata_bo_ral_start_offset);
            ral_buffer_get_property(ub_vsdata_bo_ral,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &ub_vsdata_bo_size);
            ral_buffer_get_property(ub_vsdata_bo_ral,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &ub_vsdata_bo_ral_start_offset);

            text_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         text_ptr->fsdata_index,
                                         ub_fsdata_bo_id,
                                         ub_fsdata_bo_raGL_start_offset + ub_fsdata_bo_ral_start_offset,
                                         ub_fsdata_bo_size);
            text_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         text_ptr->vsdata_index,
                                         ub_vsdata_bo_id,
                                         ub_vsdata_bo_raGL_start_offset + ub_vsdata_bo_ral_start_offset,
                                         ub_vsdata_bo_size);

            text_ptr->pGLBindVertexArray(vao_id);
            text_ptr->pGLEnable         (GL_BLEND);
            text_ptr->pGLDisable        (GL_DEPTH_TEST);
            text_ptr->pGLBlendEquation  (GL_FUNC_ADD);
            text_ptr->pGLBlendFunc      (GL_ONE,
                                         GL_ONE_MINUS_SRC_ALPHA);
            {
                bool     has_enabled_scissor_test  = false;
                uint32_t n_characters_drawn_so_far = 0;

                for (uint32_t n_string = 0;
                              n_string < n_strings;
                            ++n_string)
                {
                    _varia_text_renderer_text_string* string_ptr = nullptr;

                    system_resizable_vector_get_element_at(text_ptr->strings,
                                                           n_string,
                                                          &string_ptr);

                    if (string_ptr->visible)
                    {
                        if (string_ptr->scissor_box[0] == -1 &&
                            string_ptr->scissor_box[1] == -1 &&
                            string_ptr->scissor_box[2] == -1 &&
                            string_ptr->scissor_box[3] == -1)
                        {
                            text_ptr->pGLDisable(GL_SCISSOR_TEST);

                            has_enabled_scissor_test = false;
                        }
                        else
                        {
                            text_ptr->pGLEnable (GL_SCISSOR_TEST);
                            text_ptr->pGLScissor(string_ptr->scissor_box[0],
                                                 string_ptr->scissor_box[1],
                                                 string_ptr->scissor_box[2],
                                                 string_ptr->scissor_box[3]);

                            has_enabled_scissor_test = true;
                        }

                        ral_program_block_buffer_set_nonarrayed_variable_value(text_ptr->fsdata_ub,
                                                                               text_ptr->draw_text_fragment_shader_color_ub_offset,
                                                                               string_ptr->color,
                                                                               sizeof(float) * 3);
                        ral_program_block_buffer_set_nonarrayed_variable_value(text_ptr->vsdata_ub,
                                                                               text_ptr->draw_text_vertex_shader_scale_ub_offset,
                                                                              &string_ptr->scale,
                                                                               sizeof(float) );
                        ral_program_block_buffer_set_nonarrayed_variable_value(text_ptr->vsdata_ub,
                                                                               text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset,
                                                                              &n_characters_drawn_so_far,
                                                                               sizeof(int) );

                        ral_program_block_buffer_sync(text_ptr->fsdata_ub);
                        ral_program_block_buffer_sync(text_ptr->vsdata_ub);

                        text_ptr->pGLDrawArrays(GL_TRIANGLES,
                                                n_characters_drawn_so_far * 6,
                                                string_ptr->string_length * 6);
                    }

                    n_characters_drawn_so_far += string_ptr->string_length;
                }

                if (has_enabled_scissor_test)
                {
                    text_ptr->pGLDisable(GL_SCISSOR_TEST);
                }
            }
            text_ptr->pGLDisable(GL_BLEND);

            /* Clean up */
            text_ptr->pGLUseProgram (0);
        }
    }
    system_critical_section_leave(text_ptr->draw_cs);
}

/** Please see header for specification */
PRIVATE void _varia_text_renderer_release(void* text)
{
    _varia_text_renderer* text_ptr = (_varia_text_renderer*) text;

    /* First we need to call-back from the rendering thread, in order to free resources owned by GL */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(text_ptr->owner_context),
                                                     _varia_text_renderer_destruction_callback_from_renderer,
                                                     text);

    /* Release other stuff */
    if (text_ptr->data_buffer_contents != nullptr)
    {
        delete [] text_ptr->data_buffer_contents;

        text_ptr->data_buffer_contents = nullptr;
    }

    if (text_ptr->draw_cs != nullptr)
    {
        system_critical_section_release(text_ptr->draw_cs);

        text_ptr->draw_cs = nullptr;
    }

    if (text_ptr->strings != nullptr)
    {
        system_resizable_vector_release(text_ptr->strings);

        text_ptr->strings = nullptr;
    }
}

/** TODO */
PRIVATE void _varia_text_renderer_update_vram_data_storage(ogl_context context,
                                                           void*       text)
{
    ral_backend_type      backend_type = RAL_BACKEND_TYPE_UNKNOWN;
    _varia_text_renderer* text_ptr     = (_varia_text_renderer*) text;

    ral_context_get_property(text_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    /* Prepare UV data to be uploaded to VRAM */
    uint32_t n_text_strings     = 0;
    uint32_t summed_text_length = 0;

    system_resizable_vector_get_property(text_ptr->strings,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_text_strings);

    for (size_t n_text_string = 0;
                n_text_string < n_text_strings;
              ++n_text_string)
    {
        _varia_text_renderer_text_string* string_ptr = nullptr;

        if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                   n_text_string,
                                                  &string_ptr) )
        {
            summed_text_length += string_ptr->string_length;
        }
    }

    if (text_ptr->data_buffer_contents_length < summed_text_length)
    {
        bool                   alloc_result;
        ral_buffer_create_info alloc_info;

        /* Need to reallocate */
        if (text_ptr->data_buffer_contents != nullptr)
        {
            delete [] text_ptr->data_buffer_contents;

            text_ptr->data_buffer_contents = nullptr;
        }

        text_ptr->data_buffer_contents_length = summed_text_length;
        text_ptr->data_buffer_contents_size   = 8 * summed_text_length * sizeof(float);
        text_ptr->data_buffer_contents        = (char*) new (std::nothrow) char[text_ptr->data_buffer_contents_size];

        ASSERT_ALWAYS_SYNC(text_ptr->data_buffer_contents != nullptr,
                           "Out of memory");

        if (text_ptr->data_buffer_contents == nullptr)
        {
            return;
        }

        /* This implies we also need to resize the buffer object */
        if (text_ptr->data_buffer != nullptr)
        {
            /* Free the region first */
            ral_context_delete_objects(text_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_buffers */
                                       (const void**) &text_ptr->data_buffer);

            text_ptr->data_buffer = nullptr;
        }

        alloc_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
        alloc_info.parent_buffer    = nullptr;
        alloc_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
        alloc_info.size             = text_ptr->data_buffer_contents_size;
        alloc_info.start_offset     = 0;
        alloc_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT;
        alloc_info.user_queue_bits  = 0xFFFFFFFF;

        alloc_result = ral_context_create_buffers(text_ptr->context,
                                                  1, /* n_buffers */
                                                 &alloc_info,
                                                 &text_ptr->data_buffer);

        ASSERT_DEBUG_SYNC(alloc_result,
                          "Text data buffer allocation failed.");
    }

    /* Iterate through each character and prepare the data for uploading */
    float*   character_data_traveller_ptr = (float*) text_ptr->data_buffer_contents;
    uint32_t largest_height               = 0;
    uint32_t summed_width                 = 0;

    for (size_t n_text_string = 0;
                n_text_string < n_text_strings;
              ++n_text_string)
    {
        _varia_text_renderer_text_string* string_ptr = nullptr;

        if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                   n_text_string,
                                                  &string_ptr) )
        {
            float summed_x_delta = 0;

            for (uint32_t n_character = 0;
                          n_character < string_ptr->string_length;
                        ++n_character)
            {
                uint8_t font_height = 0;
                uint8_t font_width  = 0;
                float   x_delta     = 0;

                gfx_bfg_font_table_get_character_properties(text_ptr->font_table,
                                                            string_ptr->string[n_character],
                                                            character_data_traveller_ptr+2,  /* U1 */
                                                            character_data_traveller_ptr+3,  /* V1 */
                                                            character_data_traveller_ptr+4,  /* U2 */
                                                            character_data_traveller_ptr+5); /* V2 */

                gfx_bfg_font_table_get_character_size(text_ptr->font_table,
                                                      string_ptr->string[n_character],
                                                     &font_width,
                                                     &font_height);

                x_delta = ((float) font_width) / text_ptr->screen_width;

                *(character_data_traveller_ptr + 0) = string_ptr->position[0]     + summed_x_delta;
                *(character_data_traveller_ptr + 1) = (1-string_ptr->position[1]);
                *(character_data_traveller_ptr + 6) = x_delta;
                *(character_data_traveller_ptr + 7) = ((float) font_height) / text_ptr->screen_height;

                summed_x_delta               += x_delta;
                character_data_traveller_ptr += 8;
            }
        }
    }

    /* Upload the data */
    ral_buffer_client_sourced_update_info                                data_update;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > data_update_ptrs;

    data_update.data         = text_ptr->data_buffer_contents;
    data_update.data_size    = text_ptr->data_buffer_contents_size;
    data_update.start_offset = 0;

    data_update_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&data_update,
                                                                                      NullDeleter<ral_buffer_client_sourced_update_info>() ));

    ral_buffer_set_data_from_client_memory(text_ptr->data_buffer,
                                           data_update_ptrs,
                                           false, /* async */
                                           false  /* sync_other_contexts */);
}


/** Please see header for specification */
PUBLIC EMERALD_API varia_text_renderer_text_string_id varia_text_renderer_add_string(varia_text_renderer text)
{
    varia_text_renderer_text_string_id result          = 0;
    _varia_text_renderer*              text_ptr        = (_varia_text_renderer*) text;
    _varia_text_renderer_text_string*  text_string_ptr = nullptr;

    text_string_ptr = new (std::nothrow) _varia_text_renderer_text_string;

    ASSERT_DEBUG_SYNC(text_string_ptr != nullptr,
                      "Out of memory");

    if (text_string_ptr != nullptr)
    {
        memcpy(text_string_ptr->color,
               text_ptr->default_color,
               sizeof(text_string_ptr->color) );

        text_string_ptr->height_px            = 0;
        text_string_ptr->position[0]          = 0;
        text_string_ptr->position[1]          = 0;
        text_string_ptr->scale                = text_ptr->default_scale;
        text_string_ptr->scissor_box[0]       = -1;
        text_string_ptr->scissor_box[1]       = -1;
        text_string_ptr->scissor_box[2]       = -1;
        text_string_ptr->scissor_box[3]       = -1;
        text_string_ptr->string               = nullptr;
        text_string_ptr->string_buffer_length = 0;
        text_string_ptr->string_length        = 0;
        text_string_ptr->width_px             = 0;
        text_string_ptr->visible              = true;
        text_ptr->dirty                       = true;

        system_resizable_vector_get_property(text_ptr->strings,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &result);

        system_critical_section_enter(text_ptr->draw_cs);
        {
            system_resizable_vector_push(text_ptr->strings,
                                         text_string_ptr);
        }
        system_critical_section_leave(text_ptr->draw_cs);
    }

    return result;
}


/** Please see header for specification */
PUBLIC EMERALD_API varia_text_renderer varia_text_renderer_create(system_hashed_ansi_string name,
                                                                  ral_context               context,
                                                                  gfx_bfg_font_table        font_table,
                                                                  uint32_t                  screen_width,
                                                                  uint32_t                  screen_height)
{
    _varia_text_renderer* result_ptr = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != nullptr,
                      "Input context is null!");

    if (context == nullptr)
    {
        LOG_ERROR("Input context is null - cannot create text instance.");

        goto end;
    }

    /* Instantiate the new object */
    result_ptr = new (std::nothrow) _varia_text_renderer;

    ASSERT_ALWAYS_SYNC(result_ptr != nullptr,
                       "Could not allocate memory for text instance.");

    if (result_ptr == nullptr)
    {
        LOG_FATAL("Could not allocate memory for text instance.");

        goto end;
    }
    else
    {
        /* Now that we have place to store OGL-specific ids, we can request a call-back from context thread to fill them up.
         * Before that, however, fill the structure with data that is necessary for that to happen. */
        ogl_context context_gl = ral_context_get_gl_context(context);

        memset(result_ptr,
               0,
               sizeof(_varia_text_renderer) );

        result_ptr->context          = context;
        result_ptr->default_color[0] = DEFAULT_COLOR_R;
        result_ptr->default_color[1] = DEFAULT_COLOR_G;
        result_ptr->default_color[2] = DEFAULT_COLOR_B;
        result_ptr->default_scale    = DEFAULT_SCALE;
        result_ptr->draw_cs          = system_critical_section_create();
        result_ptr->font_table       = font_table;
        result_ptr->fsdata_index     = -1;
        result_ptr->fsdata_ub        = nullptr;
        result_ptr->name             = name;
        result_ptr->owner_context    = context;
        result_ptr->screen_width     = screen_width;
        result_ptr->screen_height    = screen_height;
        result_ptr->strings          = system_resizable_vector_create(4 /* default capacity */);
        result_ptr->vsdata_index     = -1;
        result_ptr->vsdata_ub        = nullptr;

        /* Retrieve TBO alignment requirement */
        const ogl_context_gl_limits* limits_ptr = nullptr;

        ogl_context_get_property(context_gl,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        result_ptr->shader_storage_buffer_offset_alignment = limits_ptr->shader_storage_buffer_offset_alignment;

        /* Initialize GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints*                    entry_points_ptr    = nullptr;
            const ogl_context_es_entrypoints_ext_texture_buffer* ts_entry_points_ptr = nullptr;

            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);
            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER,
                                    &ts_entry_points_ptr);

            result_ptr->pGLActiveTexture      = entry_points_ptr->pGLActiveTexture;
            result_ptr->pGLBindBuffer         = entry_points_ptr->pGLBindBuffer;
            result_ptr->pGLBindBufferRange    = entry_points_ptr->pGLBindBufferRange;
            result_ptr->pGLBindSampler        = entry_points_ptr->pGLBindSampler;
            result_ptr->pGLBindTexture        = entry_points_ptr->pGLBindTexture;
            result_ptr->pGLBindVertexArray    = entry_points_ptr->pGLBindVertexArray;
            result_ptr->pGLBlendEquation      = entry_points_ptr->pGLBlendEquation;
            result_ptr->pGLBlendFunc          = entry_points_ptr->pGLBlendFunc;
            result_ptr->pGLDeleteVertexArrays = entry_points_ptr->pGLDeleteVertexArrays;
            result_ptr->pGLDisable            = entry_points_ptr->pGLDisable;
            result_ptr->pGLDrawArrays         = entry_points_ptr->pGLDrawArrays;
            result_ptr->pGLEnable             = entry_points_ptr->pGLEnable;
            result_ptr->pGLGenVertexArrays    = entry_points_ptr->pGLGenVertexArrays;
            result_ptr->pGLGenerateMipmap     = entry_points_ptr->pGLGenerateMipmap;
            result_ptr->pGLProgramUniform1i   = entry_points_ptr->pGLProgramUniform1i;
            result_ptr->pGLScissor            = entry_points_ptr->pGLScissor;
            result_ptr->pGLTexBufferRange     = ts_entry_points_ptr->pGLTexBufferRangeEXT;
            result_ptr->pGLTexParameteri      = entry_points_ptr->pGLTexParameteri;
            result_ptr->pGLUseProgram         = entry_points_ptr->pGLUseProgram;
            result_ptr->pGLViewport           = entry_points_ptr->pGLViewport;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backend type");

            const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points_ptr = nullptr;
            const ogl_context_gl_entrypoints*                         entry_points_ptr     = nullptr;

            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);
            ogl_context_get_property(context_gl,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                    &dsa_entry_points_ptr);

            result_ptr->gl_pGLPolygonMode           = entry_points_ptr->pGLPolygonMode;
            result_ptr->gl_pGLTextureParameteriEXT  = dsa_entry_points_ptr->pGLTextureParameteriEXT;
            result_ptr->pGLActiveTexture            = entry_points_ptr->pGLActiveTexture;
            result_ptr->pGLBindBuffer               = entry_points_ptr->pGLBindBuffer;
            result_ptr->pGLBindBufferRange          = entry_points_ptr->pGLBindBufferRange;
            result_ptr->pGLBindSampler              = entry_points_ptr->pGLBindSampler;
            result_ptr->pGLBindTexture              = entry_points_ptr->pGLBindTexture;
            result_ptr->pGLBindVertexArray          = entry_points_ptr->pGLBindVertexArray;
            result_ptr->pGLBlendEquation            = entry_points_ptr->pGLBlendEquation;
            result_ptr->pGLBlendFunc                = entry_points_ptr->pGLBlendFunc;
            result_ptr->pGLDeleteVertexArrays       = entry_points_ptr->pGLDeleteVertexArrays;
            result_ptr->pGLDisable                  = entry_points_ptr->pGLDisable;
            result_ptr->pGLDrawArrays               = entry_points_ptr->pGLDrawArrays;
            result_ptr->pGLEnable                   = entry_points_ptr->pGLEnable;
            result_ptr->pGLGenVertexArrays          = entry_points_ptr->pGLGenVertexArrays;
            result_ptr->pGLGenerateMipmap           = entry_points_ptr->pGLGenerateMipmap;
            result_ptr->pGLProgramUniform1i         = entry_points_ptr->pGLProgramUniform1i;
            result_ptr->pGLScissor                  = entry_points_ptr->pGLScissor;
            result_ptr->pGLTexBufferRange           = entry_points_ptr->pGLTexBufferRange;
            result_ptr->pGLTexParameteri            = entry_points_ptr->pGLTexParameteri;
            result_ptr->pGLUseProgram               = entry_points_ptr->pGLUseProgram;
            result_ptr->pGLViewport                 = entry_points_ptr->pGLViewport;
        }

        /* Make sure the font table has been assigned a texture object */
        ogl_context_request_callback_from_context_thread(context_gl,
                                                         _varia_text_renderer_create_font_table_to_callback_from_renderer,
                                                         result_ptr);

        /* We need a call-back, now */
        ogl_context_request_callback_from_context_thread(context_gl,
                                                         _varia_text_renderer_construction_callback_from_renderer,
                                                         result_ptr);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result_ptr,
                                                       _varia_text_renderer_release,
                                                       OBJECT_TYPE_VARIA_TEXT_RENDERER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Text Renderers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        goto end;
    }

end:
    return (varia_text_renderer) result_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_delete_string(varia_text_renderer                text,
                                                          varia_text_renderer_text_string_id text_id)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_draw(ral_context         context,
                                                 varia_text_renderer text)
{
    _varia_text_renderer* text_ptr = (_varia_text_renderer*) text;

    /* Make sure the request is handled from rendering thread */
    ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(context),
                                                     _varia_text_renderer_draw_callback_from_renderer,
                                                     text);
}

/** Please see header for specification */
PUBLIC EMERALD_API const unsigned char* varia_text_renderer_get(varia_text_renderer                text,
                                                                varia_text_renderer_text_string_id text_string_id)
{
    const unsigned char*              result          = nullptr;
    _varia_text_renderer*             text_ptr        = (_varia_text_renderer*) text;
    _varia_text_renderer_text_string* text_string_ptr = nullptr;

    if (system_resizable_vector_get_element_at(text_ptr->strings,
                                               text_string_id,
                                              &text_string_ptr) )
    {
        result = text_string_ptr->string;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t varia_text_renderer_get_added_strings_counter(varia_text_renderer instance)
{
    uint32_t result = 0;

    system_resizable_vector_get_property(((_varia_text_renderer*) instance)->strings,
                                          SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_context varia_text_renderer_get_context(varia_text_renderer text)
{
    return ((_varia_text_renderer*) text)->context;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_get_text_string_property(varia_text_renderer                      text,
                                                                     varia_text_renderer_text_string_property property,
                                                                     varia_text_renderer_text_string_id       text_string_id,
                                                                     void*                                    out_result_ptr)
{
    _varia_text_renderer* text_ptr = (_varia_text_renderer*) text;

    switch(property)
    {
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR:
        {
            _varia_text_renderer_text_string* text_string_ptr = nullptr;

            if (text_string_id == VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT)
            {
                memcpy(out_result_ptr,
                        text_ptr->default_color,
                       sizeof(text_ptr->default_color) );
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                memcpy(out_result_ptr,
                       text_string_ptr->color,
                       sizeof(float) * 3);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized text string id");
            }

            break;
        }

        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY:
        {
            _varia_text_renderer_text_string* text_string_ptr = nullptr;

            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX)
                {
                    int* position = (int*) out_result_ptr;

                    position[0] = (int) ((text_string_ptr->position[0]) * (float) text_ptr->screen_width);
                    position[1] = (int) ((text_string_ptr->position[1]) * (float) text_ptr->screen_height);
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    float* position = (float*) out_result_ptr;

                    position[0] = text_string_ptr->position[0];
                    position[1] = text_string_ptr->position[1];
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX)
                {
                    memcpy(out_result_ptr,
                           text_string_ptr->scissor_box,
                           sizeof(text_string_ptr->scissor_box) );
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX)
                {
                    *(int*) out_result_ptr = int( float(text_string_ptr->height_px) * (1-text_string_ptr->scale));
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS)
                {
                    unsigned int window_height = 0;

                    *(float*) out_result_ptr = (float(text_string_ptr->height_px) * (1 - text_string_ptr->scale)) / float(text_ptr->screen_height);
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX)
                {
                    *(int*) out_result_ptr = int( float(text_string_ptr->width_px) * (1-text_string_ptr->scale));
                }
                else
                {
                    *(bool*) out_result_ptr = text_string_ptr->visible;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized string id [%d]",
                                  text_string_id);
            }

            break;
        }

        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE:
        {
            _varia_text_renderer_text_string* text_string_ptr = nullptr;

            if (text_string_id == VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT)
            {
                *(float*) out_result_ptr = text_ptr->default_scale;
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                *(float*) out_result_ptr = text_string_ptr->scale;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized text string id");
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized text string property");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_set(varia_text_renderer                text,
                                                varia_text_renderer_text_string_id text_string_id,
                                                const char*                        raw_text_ptr)
{
    size_t                            raw_text_length = 0;
    _varia_text_renderer*             text_ptr        = (_varia_text_renderer*) text;
    _varia_text_renderer_text_string* text_string_ptr = nullptr;

    system_critical_section_enter(text_ptr->draw_cs);

    /* Make sure input arguments are okay */
    ASSERT_DEBUG_SYNC(raw_text_ptr != nullptr,
                      "Input text pointer is NULL!");

    if (raw_text_ptr == nullptr)
    {
        LOG_ERROR("Input text pointer is NULL.");

        goto end;
    }

    system_resizable_vector_get_element_at(text_ptr->strings,
                                           text_string_id,
                                          &text_string_ptr);

    if (text_string_ptr == nullptr)
    {
        LOG_ERROR("Could not retrieve text string descriptor");

        goto end;
    }

    /* Update text width and height properties */
    raw_text_length = strlen((const char*) raw_text_ptr);

    text_string_ptr->height_px = 0;
    text_string_ptr->width_px  = 0;

    for (uint32_t n_character = 0;
                  n_character < raw_text_length;
                ++n_character)
    {
        uint8_t font_height = 0;
        uint8_t font_width  = 0;

        gfx_bfg_font_table_get_character_size(text_ptr->font_table,
                                              raw_text_ptr[n_character],
                                             &font_width,
                                             &font_height);

        if (font_height > text_string_ptr->height_px)
        {
            text_string_ptr->height_px = font_height;
        }

        text_string_ptr->width_px += font_width;
    }

    /* Excellent. Update underlying structure's data and we're done here */
    if (text_string_ptr->string_buffer_length <= raw_text_length)
    {
        if (text_string_ptr->string != nullptr)
        {
            delete [] text_string_ptr->string;

            text_string_ptr->string = nullptr;
        }

        if (raw_text_length == 0)
        {
            raw_text_length = 1;
        }

        text_string_ptr->string_buffer_length = (unsigned int) raw_text_length * 2;
        text_string_ptr->string               = new (std::nothrow) unsigned char[text_string_ptr->string_buffer_length];

        ASSERT_ALWAYS_SYNC(text_string_ptr->string != nullptr,
                           "Could not reallocate memory block used for raw text storage!");

        if (text_string_ptr->string == nullptr)
        {
            LOG_ERROR("Could not reallocate memory block used for raw text storage");

            goto end;
        }
    }

    if (text_string_ptr->string != nullptr)
    {
        memset(text_string_ptr->string,
               text_string_ptr->string_buffer_length,
               0);

        text_string_ptr->string[raw_text_length] = 0;

        memcpy((void*) text_string_ptr->string,
               raw_text_ptr,
               raw_text_length);

        text_string_ptr->string_length = raw_text_length;
    }

    /* We'll need to rebuild the internal data structure at next draw call. Update the dirty flag */
    text_ptr->dirty = true;

end:
    system_critical_section_leave(text_ptr->draw_cs);
}

/* Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_set_text_string_property(varia_text_renderer                      text,
                                                                     varia_text_renderer_text_string_id       text_string_id,
                                                                     varia_text_renderer_text_string_property property,
                                                                     const void*                              data)
{
    _varia_text_renderer*             text_ptr        = (_varia_text_renderer*) text;
    _varia_text_renderer_text_string* text_string_ptr = nullptr;

    switch (property)
    {
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR:
        {
            if (text_string_id == VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT)
            {
                memcpy(text_ptr->default_color,
                       data,
                       sizeof(text_ptr->default_color) );
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                memcpy(text_string_ptr->color,
                       data,
                       sizeof(float) * 3);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized text string id");
            }

            break;
        }

        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCALE:
        {
            float new_scale = *(float*) data;

            ASSERT_DEBUG_SYNC(new_scale >= 0 && new_scale <= 1,
                              "Scale out of supported range - adapt the shaders");

            if (text_string_id == VARIA_TEXT_RENDERER_TEXT_STRING_ID_DEFAULT)
            {
                text_ptr->default_scale = new_scale;
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                text_string_ptr->scale = new_scale;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized text string id");
            }

            break;
        }

        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX:
        case VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY:
        {
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX)
                {
                    const int* position = (const int*) data;

                    text_string_ptr->position[0] = (float) (position[0]) / (float) text_ptr->screen_width;
                    text_string_ptr->position[1] = (float) (position[1]) / (float) text_ptr->screen_height;
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    const float* position = (const float*) data;

                    text_string_ptr->position[0] = position[0];
                    text_string_ptr->position[1] = position[1];
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX)
                {
                    memcpy(text_string_ptr->scissor_box,
                           data,
                           sizeof(text_string_ptr->scissor_box) );
                }
                else
                {
                    text_string_ptr->visible = *(bool*) data;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized string id [%d]",
                                  text_string_id);
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized text string property");
        }
    }

    text_ptr->dirty = true;
}