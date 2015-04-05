/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Uniform buffer data layout is as follows
 *
 * # Horizontal delta (float)
 * # Top-left  U      (float) taken from font table texture.
 * # Top-left  V      (float) taken from font table texture.
 * # Bot-right U      (float) taken from font table texture.
 * # Bot-right V      (float) taken from font table texture.
 * # Font width       (float) in pixels.
 * # Font height      (float) in pixels.
 *
 * Repeated as many times as there are characters in the strng.
 *
 * NOTE: For ES contexts, this implementation requires GL_EXT_texture_buffer support.
 */
#include "shared.h"
#include "gfx/gfx_bfg_font_table.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_texture.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"
#include <map>
#include <sstream>

#define DEFAULT_COLOR_R (1)
#define DEFAULT_COLOR_G (0)
#define DEFAULT_COLOR_B (0)
#define DEFAULT_SCALE   (1)
#define MAX_TEXT_LENGTH "256"

/** Internal types */
typedef struct
{
    ogl_texture to;
} _font_table_descriptor;

typedef std::map<gfx_bfg_font_table, _font_table_descriptor> FontTable;
typedef FontTable::iterator                                  FontTableIterator;
typedef FontTable::const_iterator                            FontTableConstIterator;

typedef struct
{
    void*        data_buffer_contents;
    uint32_t     data_buffer_contents_length;
    uint32_t     data_buffer_contents_size;
    ogl_texture  data_buffer_to;
    bool         dirty;
    unsigned int texture_buffer_offset_alignment;

    GLuint      data_buffer_id; /* managed by ogl_buffers */
    GLuint      data_buffer_offset;

    float default_color[3];
    float default_scale;

    system_critical_section   draw_cs;
    gfx_bfg_font_table        font_table;
    system_hashed_ansi_string name;
    ogl_context               owner_context;
    uint32_t                  screen_height;
    uint32_t                  screen_width;
    system_resizable_vector   strings;

    ogl_buffers buffers;
    ogl_context context;

    /* GL function pointers cache */
    PFNWRAPPEDGLBINDTEXTUREPROC           gl_pGLBindTexture;
    PFNGLNAMEDBUFFERSUBDATAEXTPROC        gl_pGLNamedBufferSubDataEXT;
    PFNGLPOLYGONMODEPROC                  gl_pGLPolygonMode;
    PFNWRAPPEDGLTEXTUREBUFFERRANGEEXTPROC gl_pGLTextureBufferRangeEXT;
    PFNWRAPPEDGLTEXTUREPARAMETERIEXTPROC  gl_pGLTextureParameteriEXT;
    PFNWRAPPEDGLTEXTURESTORAGE2DEXTPROC   gl_pGLTextureStorage2DEXT;
    PFNWRAPPEDGLTEXTURESUBIMAGE2DEXTPROC  gl_pGLTextureSubImage2DEXT;
    PFNGLACTIVETEXTUREPROC                pGLActiveTexture;
    PFNGLBINDBUFFERPROC                   pGLBindBuffer;
    PFNGLBINDTEXTUREPROC                  pGLBindTexture;
    PFNGLBINDVERTEXARRAYPROC              pGLBindVertexArray;
    PFNGLBLENDEQUATIONPROC                pGLBlendEquation;
    PFNGLBLENDFUNCPROC                    pGLBlendFunc;
    PFNGLBUFFERDATAPROC                   pGLBufferData;
    PFNGLBUFFERSUBDATAPROC                pGLBufferSubData;
    PFNGLDELETEVERTEXARRAYSPROC           pGLDeleteVertexArrays;
    PFNGLDISABLEPROC                      pGLDisable;
    PFNGLDRAWARRAYSPROC                   pGLDrawArrays;
    PFNGLENABLEPROC                       pGLEnable;
    PFNGLGENVERTEXARRAYSPROC              pGLGenVertexArrays;
    PFNGLGENERATEMIPMAPPROC               pGLGenerateMipmap;
    PFNGLGETERRORPROC                     pGLGetError;
    PFNGLPROGRAMUNIFORM1FPROC             pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM1IPROC             pGLProgramUniform1i;
    PFNGLPROGRAMUNIFORM3FVPROC            pGLProgramUniform3fv;
    PFNGLSCISSORPROC                      pGLScissor;
    PFNGLTEXBUFFERRANGEPROC               pGLTexBufferRange;
    PFNGLTEXPARAMETERIPROC                pGLTexParameteri;
    PFNGLTEXSTORAGE2DPROC                 pGLTexStorage2D;
    PFNGLTEXSUBIMAGE2DPROC                pGLTexSubImage2D;
    PFNGLUSEPROGRAMPROC                   pGLUseProgram;

    REFCOUNT_INSERT_VARIABLES
} _ogl_text;

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

} _ogl_text_string;

typedef struct
{
    ogl_program draw_text_program;
    ogl_shader  draw_text_fragment_shader;
    ogl_shader  draw_text_vertex_shader;

    GLuint      draw_text_fragment_shader_color_location;
    GLuint      draw_text_fragment_shader_font_table_location;
    GLuint      draw_text_vertex_shader_data_location;
    GLuint      draw_text_vertex_shader_n_origin_character_location;
    GLuint      draw_text_vertex_shader_scale_location;

    FontTable   font_tables;
} _global_variables;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_text, ogl_text, _ogl_text);


/* Private definitions */
system_critical_section _global_cs       = system_critical_section_create();
_global_variables       _global;             /* always enter _global_cs before accessing */
uint32_t                _n_global_owners = 0;/* always enter _global_cs before accessing */


const char* fragment_shader_template = "#ifdef GL_ES\n"
                                       "    precision highp float;\n"
                                       "#endif\n"
                                       "in      vec2      vertex_shader_uv;\n"
                                       "out     vec4      result;\n"
                                       "uniform vec3      color;\n"
                                       "uniform sampler2D font_table;\n"
                                       "\n"
                                       "void main()\n"
                                       "{\n"
                                       "    vec4 texel = texture(font_table, vertex_shader_uv);\n"
                                       "\n"
                                       "    result = vec4(color.xyz * texel.x, (texel.x > 0.9) ? 1 : 0);\n"
                                       "}\n";

const char* vertex_shader_template = "#ifdef GL_ES\n"
                                     "    #extension GL_EXT_texture_buffer : require\n"
                                     "\n"
                                     "    precision highp float;\n"
                                     "    precision highp samplerBuffer;\n"
                                     "#endif\n"
                                     "\n"
                                     "uniform samplerBuffer data;\n"
                                     "uniform int           n_origin_character;\n"
                                     "uniform float         scale;\n"
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
                                     "   vec2  x1_y1_origin = texelFetch(data, n_origin_character * 2).xy;\n"
                                     "   vec4  x1_y1_u1_v1  = texelFetch(data, character_id * 2);\n"
                                     "   vec4  u2_v2_w_h    = texelFetch(data, character_id * 2 + 1);\n"
                                     "   float u_delta      = u2_v2_w_h.x - x1_y1_u1_v1.z;\n"
                                     "   float v_delta      = u2_v2_w_h.y - x1_y1_u1_v1.w;\n"
                                     "\n"
                                     /* Scale x1/y1 to <-1, 1> */
                                     "   x1_y1_u1_v1.xy   = x1_y1_u1_v1.xy * 2.0 - 1.0;\n"
                                     "   x1_y1_origin     = x1_y1_origin   * 2.0 - 1.0;\n"
                                     /* Output */
                                     "   gl_Position      = vec4(x1_y1_u1_v1.x + (1.0 - scale) * (x1_y1_origin.x - x1_y1_u1_v1.x) + scale * (vertex_data.x * u2_v2_w_h.z * 2.0),\n"
                                     "                           x1_y1_u1_v1.y + (1.0 - scale) * (x1_y1_origin.y - x1_y1_u1_v1.y) + scale * (vertex_data.y * u2_v2_w_h.w * 2.0),\n"
                                     "                           0, 1);\n"
                                     "   vertex_shader_uv = vec2(vertex_data.x * u_delta +       x1_y1_u1_v1.z, \n"
                                     "                          -vertex_data.y * v_delta + 1.0 - x1_y1_u1_v1.w);\n"
                                     "}\n";


/* Private forward declarations */
PRIVATE void _ogl_text_construction_callback_from_renderer        (__in __notnull ogl_context context,
                                                                                  void*       text);
PRIVATE void _ogl_text_create_font_table_to_callback_from_renderer(__in __notnull ogl_context context,
                                                                                  void*       text);
PRIVATE void _ogl_text_destruction_callback_from_renderer         (__in __notnull ogl_context context,
                                                                                  void*       text);
PRIVATE void _ogl_text_draw_callback_from_renderer                (__in __notnull ogl_context context,
                                                                                  void*       text);
PRIVATE void _ogl_text_release                                    (               void*       text);
PRIVATE void _ogl_text_update_vram_data_storage                   (__in __notnull ogl_context context,
                                                                                  void*       text);

/* Private functions */

/** Please see header for specification */
PRIVATE void _ogl_text_release(void* text)
{
    _ogl_text* text_ptr = (_ogl_text*) text;

    /* First we need to call-back from the rendering thread, in order to free resources owned by GL */
    ogl_context_request_callback_from_context_thread(text_ptr->owner_context,
                                                     _ogl_text_destruction_callback_from_renderer,
                                                     text);

    /* Release other stuff */
    if (text_ptr->data_buffer_contents != NULL)
    {
        delete [] text_ptr->data_buffer_contents;

        text_ptr->data_buffer_contents = NULL;
    }

    if (text_ptr->draw_cs != NULL)
    {
        system_critical_section_release(text_ptr->draw_cs);

        text_ptr->draw_cs = NULL;
    }

    if (text_ptr->strings != NULL)
    {
        system_resizable_vector_release(text_ptr->strings);

        text_ptr->strings = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_text_update_vram_data_storage(__in __notnull ogl_context context,
                                                __in __notnull void*       text)
{
    ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;
    _ogl_text*       text_ptr     = (_ogl_text*) text;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    /* Prepare UV data to be uploaded to VRAM */
    uint32_t n_text_strings     = system_resizable_vector_get_amount_of_elements(text_ptr->strings);
    uint32_t summed_text_length = 0;

    for (size_t n_text_string = 0;
                n_text_string < n_text_strings;
              ++n_text_string)
    {
        _ogl_text_string* string_ptr = NULL;

        if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                   n_text_string,
                                                  &string_ptr) )
        {
            summed_text_length += string_ptr->string_length;
        }
    } /* for (uint32_t n_text_string = 0; n_text_string < n_text_strings; ++n_text_string) */

    if (text_ptr->data_buffer_to == NULL)
    {
        /* Generate texture buffer we will use to map the texture object onto the data buffer object*/
        text_ptr->data_buffer_to = ogl_texture_create_empty(context,
                                                            system_hashed_ansi_string_create_by_merging_two_strings("System text renderer ",
                                                                                                                    system_hashed_ansi_string_get_buffer(text_ptr->name) ));

        ASSERT_DEBUG_SYNC(text_ptr->pGLGetError() == GL_NO_ERROR,
                          "Could not create a texture object for texture buffer.");

        /* We need to retain the ogl_texture instance since we will need to manually manage
         * the lifetime of this object.
         */
        ogl_texture_retain(text_ptr->data_buffer_to);
    }

    if (text_ptr->data_buffer_contents_length < summed_text_length)
    {
        /* Need to reallocate */
        if (text_ptr->data_buffer_contents != NULL)
        {
            delete [] text_ptr->data_buffer_contents;

            text_ptr->data_buffer_contents = NULL;
        } /* if (text_ptr->data_buffer_contents != NULL)*/

        text_ptr->data_buffer_contents_length = summed_text_length;
        text_ptr->data_buffer_contents_size   = 8 * summed_text_length * sizeof(float);
        text_ptr->data_buffer_contents        = (void*) new (std::nothrow) char[text_ptr->data_buffer_contents_size];

        ASSERT_ALWAYS_SYNC(text_ptr->data_buffer_contents != NULL,
                           "Out of memory");
        if (text_ptr->data_buffer_contents == NULL)
        {
            return;
        }

        if (context_type == OGL_CONTEXT_TYPE_GL)
        {
            /* This implies we also need to resize the buffer object */
            if (text_ptr->data_buffer_id != 0)
            {
                /* Since we're using ogl_buffers, we need to free the region first */
                ogl_buffers_free_buffer_memory(text_ptr->buffers,
                                               text_ptr->data_buffer_id,
                                               text_ptr->data_buffer_offset);

                text_ptr->data_buffer_id     = 0;
                text_ptr->data_buffer_offset = 0;
            }

            bool alloc_result = ogl_buffers_allocate_buffer_memory(text_ptr->buffers,
                                                                   text_ptr->data_buffer_contents_size,
                                                                   text_ptr->texture_buffer_offset_alignment,
                                                                   OGL_BUFFERS_MAPPABILITY_NONE,
                                                                   OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                                                  &text_ptr->data_buffer_id,
                                                                  &text_ptr->data_buffer_offset);

            LOG_INFO("Alloc: BO id:[%d] offset:[%d]", text_ptr->data_buffer_id, text_ptr->data_buffer_offset);

            ASSERT_DEBUG_SYNC(alloc_result,
                              "Text data buffer allocation failed.");

            /* Set it up */
            text_ptr->gl_pGLTextureBufferRangeEXT(text_ptr->data_buffer_to,
                                                  GL_TEXTURE_BUFFER,
                                                  GL_RGBA32F,
                                                  text_ptr->data_buffer_id,
                                                  text_ptr->data_buffer_offset,
                                                  text_ptr->data_buffer_contents_size);
        }
        else
        {
            text_ptr->pGLBindBuffer    (GL_ARRAY_BUFFER,
                                        text_ptr->data_buffer_id);
            text_ptr->pGLBufferData    (GL_ARRAY_BUFFER,
                                        text_ptr->data_buffer_contents_size,
                                        NULL,
                                        GL_DYNAMIC_DRAW);
            text_ptr->pGLTexBufferRange(GL_TEXTURE_BUFFER,
                                        GL_RGBA32F,
                                        text_ptr->data_buffer_id,
                                        text_ptr->data_buffer_offset,
                                        text_ptr->data_buffer_contents_size);
        }

        ASSERT_DEBUG_SYNC(text_ptr->pGLGetError() == GL_NO_ERROR,
                          "Could not set up texture buffer");
    } /* if (text_ptr->data_buffer_contents_length < summed_text_length) */

    /* Iterate through each character and prepare the data for uploading */
    float*   character_data_traveller_ptr = (float*) text_ptr->data_buffer_contents;
    uint32_t largest_height               = 0;
    uint32_t summed_width                 = 0;

    for (size_t n_text_string = 0;
                n_text_string < n_text_strings;
              ++n_text_string)
    {
        _ogl_text_string* string_ptr = NULL;

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
            } /* for (uint32_t n_character = 0; n_character < string_ptr->string_length; ++n_character) */
        } /* if (system_resizable_vector_get_element_at(text_ptr->strings, n_text_string, &string_ptr) ) */
    } /* for (size_t n_text_string = 0; n_text_string < n_text_strings; ++n_text_string) */

    /* Upload the data */
    if (context_type == OGL_CONTEXT_TYPE_GL)
    {
        text_ptr->gl_pGLNamedBufferSubDataEXT(text_ptr->data_buffer_id,
                                              text_ptr->data_buffer_offset,
                                              text_ptr->data_buffer_contents_size,
                                              text_ptr->data_buffer_contents);
    }
    else
    {
        text_ptr->pGLBufferSubData(GL_ARRAY_BUFFER,
                                   0,
                                   text_ptr->data_buffer_contents_size,
                                   text_ptr->data_buffer_contents);
    }

    ASSERT_DEBUG_SYNC(text_ptr->pGLGetError() == GL_NO_ERROR,
                      "Could not update VRAM text data");
}

/** TODO */
PRIVATE void _ogl_text_construction_callback_from_renderer(__in __notnull ogl_context context,
                                                           __in __notnull void*       text)
{
    _ogl_text* text_ptr = (_ogl_text*) text;

    /* If this is the first text object instance we are dealing with, we need to create all the shaders and programs to do
     * the dirty work
     */
    system_critical_section_enter(_global_cs);
    {
        if (_n_global_owners == 0)
        {
            _global.draw_text_fragment_shader = ogl_shader_create (context,
                                                                   SHADER_TYPE_FRAGMENT,
                                                                   system_hashed_ansi_string_create("ogl_text fragment shader"));
            _global.draw_text_program         = ogl_program_create(context,
                                                                   system_hashed_ansi_string_create("ogl_text program") );
            _global.draw_text_vertex_shader   = ogl_shader_create (context,
                                                                   SHADER_TYPE_VERTEX,
                                                                   system_hashed_ansi_string_create("ogl_text vertex shader") );

            /* Prepare the bodies */
            ogl_context_type  context_type = OGL_CONTEXT_TYPE_UNDEFINED;
            std::stringstream fs_sstream;
            std::stringstream vs_sstream;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_TYPE,
                                    &context_type);

            if (context_type == OGL_CONTEXT_TYPE_ES)
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
                ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                                  "Unrecognized context type");

                fs_sstream << "#version 420\n"
                              "\n"
                           << fragment_shader_template;

                vs_sstream << "#version 420\n"
                              "\n"
                           << vertex_shader_template;
            }

            /* Assign the bodies to the shaders */
            ogl_shader_set_body(_global.draw_text_fragment_shader,
                                system_hashed_ansi_string_create(fs_sstream.str().c_str() ));
            ogl_shader_set_body(_global.draw_text_vertex_shader,
                                system_hashed_ansi_string_create(vs_sstream.str().c_str() ));

            ogl_program_attach_shader(_global.draw_text_program,
                                      _global.draw_text_fragment_shader);
            ogl_program_attach_shader(_global.draw_text_program,
                                      _global.draw_text_vertex_shader);

            if (!ogl_program_link(_global.draw_text_program) )
            {
                LOG_ERROR        ("Could not link text drawing program.");
                ASSERT_DEBUG_SYNC(false, "");
            }

            /* Retrieve uniform locations */
            const ogl_program_uniform_descriptor* fragment_shader_color_descriptor            = NULL;
            const ogl_program_uniform_descriptor* fragment_shader_font_table_descriptor       = NULL;
            const ogl_program_uniform_descriptor* vertex_shader_data_descriptor               = NULL;
            const ogl_program_uniform_descriptor* vertex_shader_n_origin_character_descriptor = NULL;
            const ogl_program_uniform_descriptor* vertex_shader_scale_descriptor              = NULL;
            
            if (!ogl_program_get_uniform_by_name(_global.draw_text_program,
                                                 system_hashed_ansi_string_create("color"),
                                                &fragment_shader_color_descriptor) )
            {
                LOG_ERROR        ("Could not retrieve color uniform location.");
                ASSERT_DEBUG_SYNC(false, "");
            }
            else
            if (!ogl_program_get_uniform_by_name(_global.draw_text_program,
                                                 system_hashed_ansi_string_create("font_table"),
                                                &fragment_shader_font_table_descriptor) )
            {
                LOG_ERROR        ("Could not retrieve font table uniform location.");
                ASSERT_DEBUG_SYNC(false, "");
            }
            else
            if (!ogl_program_get_uniform_by_name(_global.draw_text_program,
                                                 system_hashed_ansi_string_create("data"),
                                                &vertex_shader_data_descriptor) )
            {
                LOG_ERROR        ("Could not retrieve data uniform location.");
                ASSERT_DEBUG_SYNC(false, "");
            }
            else
            if (!ogl_program_get_uniform_by_name(_global.draw_text_program,
                                                 system_hashed_ansi_string_create("n_origin_character"),
                                                &vertex_shader_n_origin_character_descriptor))
            {
                LOG_ERROR        ("Could not retrieve n origin character uniform location.");
                ASSERT_DEBUG_SYNC(false, "");
            }
            else
            if (!ogl_program_get_uniform_by_name(_global.draw_text_program,
                                                 system_hashed_ansi_string_create("scale"),
                                                &vertex_shader_scale_descriptor) )
            {
                LOG_ERROR        ("Could not retrieve scale uniform location.");
                ASSERT_DEBUG_SYNC(false, "");
            }
            else
            {
                _global.draw_text_fragment_shader_color_location            = fragment_shader_color_descriptor->location;
                _global.draw_text_fragment_shader_font_table_location       = fragment_shader_font_table_descriptor->location;
                _global.draw_text_vertex_shader_data_location               = vertex_shader_data_descriptor->location;
                _global.draw_text_vertex_shader_n_origin_character_location = vertex_shader_n_origin_character_descriptor->location;
                _global.draw_text_vertex_shader_scale_location              = vertex_shader_scale_descriptor->location;

                /* Set up uniform values */
                GLuint program_gl_id = ogl_program_get_id(_global.draw_text_program);

                text_ptr->pGLProgramUniform1i(program_gl_id,
                                              _global.draw_text_vertex_shader_data_location,
                                              0);
            }
        }

        ++_n_global_owners;
    }
    system_critical_section_leave(_global_cs);
}

/** Please see header for specification */
PRIVATE void _ogl_text_create_font_table_to_callback_from_renderer(__in __notnull ogl_context context,
                                                                   __in __notnull void*       text)
{
    ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;
    _ogl_text*       text_ptr     = (_ogl_text*) text;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (_global.font_tables.find(text_ptr->font_table) == _global.font_tables.end() )
    {
        /* Retrieve font table properties */
        const unsigned char* font_table_data_ptr = gfx_bfg_font_table_get_data_pointer(text_ptr->font_table);
        uint32_t             font_table_height   = 0;
        uint32_t             font_table_width    = 0;

        gfx_bfg_font_table_get_data_properties(text_ptr->font_table,
                                              &font_table_width,
                                              &font_table_height);

        /* Create the descriptor */
        _font_table_descriptor descriptor;

        descriptor.to = ogl_texture_create_empty(context,
                                                 system_hashed_ansi_string_create_by_merging_two_strings("Text renderer ",
                                                                                                         system_hashed_ansi_string_get_buffer(text_ptr->name) ));

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            GLint to_id = 0;

            ogl_texture_get_property(descriptor.to,
                                     OGL_TEXTURE_PROPERTY_ID,
                                    &to_id);

            text_ptr->pGLBindTexture(GL_TEXTURE_2D,
                                     to_id);
        }
        else
        {
            text_ptr->gl_pGLBindTexture(GL_TEXTURE_2D,
                                        descriptor.to);
        }

        text_ptr->pGLTexStorage2D  (GL_TEXTURE_2D,
                                    1 + log2_uint32( (font_table_width > font_table_height) ? font_table_width : font_table_height),
                                    GL_RGB8,
                                    font_table_width,
                                    font_table_height);
        text_ptr->pGLTexSubImage2D (GL_TEXTURE_2D,
                                    0, /* level */
                                    0, /* xoffset */
                                    0, /* yoffset */
                                    font_table_width,
                                    font_table_height,
                                    GL_RGB,
                                    GL_UNSIGNED_BYTE,
                                    font_table_data_ptr);
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
                                    GL_LINEAR_MIPMAP_LINEAR);
        text_ptr->pGLGenerateMipmap(GL_TEXTURE_2D);

        /* Note that texture data is stored in BGR order. We would have just gone with GL_BGR
         * format under GL context, but this is not supported under ES. What we can do, however,
         * is use GL_TEXTURE_SWIZZLE parameters of the texture object. */
        text_ptr->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_SWIZZLE_R,
                                   GL_BLUE);
        text_ptr->pGLTexParameteri(GL_TEXTURE_2D,
                                   GL_TEXTURE_SWIZZLE_B,
                                   GL_RED);

        ASSERT_DEBUG_SYNC(text_ptr->pGLGetError() == GL_NO_ERROR,
                          "Error initializing TO for font table.");

        /* We need to retain the ogl_texture instance since we will need to manually manage
         * the lifetime of this object.
         */
        ogl_texture_retain(descriptor.to);

        _global.font_tables[text_ptr->font_table] = descriptor;
    }
}

/** Please see header for specification */
PRIVATE void _ogl_text_destruction_callback_from_renderer(__in __notnull ogl_context context,
                                                          __in __notnull void*       text)
{
    _ogl_text* text_ptr = (_ogl_text*) text;

    /* First, free all objects that are not global */
    if (text_ptr->data_buffer_id != 0)
    {
        ogl_buffers_free_buffer_memory(text_ptr->buffers,
                                       text_ptr->data_buffer_id,
                                       text_ptr->data_buffer_offset);

        text_ptr->data_buffer_id     = 0;
        text_ptr->data_buffer_offset = 0;
    }

    if (text_ptr->data_buffer_to != NULL)
    {
        ogl_texture_release(text_ptr->data_buffer_to);
    }

    /* Moving on to global variables, check if we're the only owner remaining */
    system_critical_section_enter(_global_cs);
    {
        if (_n_global_owners == 1)
        {
            /* It's time. Get rid of all the shaders and programs */
            ogl_program_release(_global.draw_text_program);
            ogl_shader_release (_global.draw_text_fragment_shader);
            ogl_shader_release (_global.draw_text_vertex_shader);

            for (FontTableIterator iterator  = _global.font_tables.begin();
                                   iterator != _global.font_tables.end();
                                 ++iterator)
            {
                ogl_texture_release(iterator->second.to);
            }

            _global.draw_text_fragment_shader = NULL;
            _global.draw_text_program         = NULL;
            _global.draw_text_vertex_shader   = NULL;
        }

        --_n_global_owners;
    }
    system_critical_section_leave(_global_cs);
}

/** Please see header for specification */
PRIVATE void _ogl_text_draw_callback_from_renderer(__in __notnull ogl_context context,
                                                   __in __notnull void*       text)
{
    ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;
    GLuint           program_id   = ogl_program_get_id(_global.draw_text_program);
    _ogl_text*       text_ptr     = (_ogl_text*) text;
    uint32_t         n_strings    = system_resizable_vector_get_amount_of_elements(text_ptr->strings);

    system_critical_section_enter(text_ptr->draw_cs);
    {
        /* Update underlying helper data buffer contents - only if the "dirty flag" is on */
        if (text_ptr->dirty)
        {
            _ogl_text_update_vram_data_storage(context, text);

            text_ptr->dirty = false;
        }

        /* Carry on */
        if (n_strings > 0)
        {
            GLuint vao_id = 0;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                                    &vao_id);

            system_critical_section_enter(_global_cs);
            {
                /* Mark the text drawing program as active */
                if (context_type == OGL_CONTEXT_TYPE_GL)
                {
                    text_ptr->gl_pGLPolygonMode(GL_FRONT_AND_BACK,
                                                GL_FILL);
                }

                text_ptr->pGLUseProgram (program_id);

                /* Bind data texture buffer to zeroth active texture unit */
                text_ptr->pGLActiveTexture(GL_TEXTURE0);

                if (context_type == OGL_CONTEXT_TYPE_ES)
                {
                    GLint to_id = 0;

                    ogl_texture_get_property(text_ptr->data_buffer_to,
                                             OGL_TEXTURE_PROPERTY_ID,
                                            &to_id);

                    text_ptr->pGLBindTexture(GL_TEXTURE_BUFFER,
                                             to_id);
                }
                else
                {
                    text_ptr->gl_pGLBindTexture(GL_TEXTURE_BUFFER,
                                                text_ptr->data_buffer_to);
                }


                /* Set up texture units */
                ASSERT_DEBUG_SYNC(_global.font_tables.find(text_ptr->font_table) != _global.font_tables.end(),
                                  "Could not find corresponding texture for a font table!");

                text_ptr->pGLActiveTexture(GL_TEXTURE1);

                if (context_type == OGL_CONTEXT_TYPE_ES)
                {
                    GLint to_id = 0;

                    ogl_texture_get_property(_global.font_tables[text_ptr->font_table].to,
                                             OGL_TEXTURE_PROPERTY_ID,
                                            &to_id);

                    text_ptr->pGLBindTexture(GL_TEXTURE_2D,
                                             to_id);
                }
                else
                {
                    text_ptr->gl_pGLBindTexture(GL_TEXTURE_2D,
                                                _global.font_tables[text_ptr->font_table].to);
                }

                /* Update uniforms */
                text_ptr->pGLProgramUniform1i(program_id,
                                              _global.draw_text_fragment_shader_font_table_location,
                                              1);
                text_ptr->pGLProgramUniform1i(program_id,
                                              _global.draw_text_vertex_shader_data_location,
                                              0);

                /* Draw! */
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
                        _ogl_text_string* string_ptr = NULL;

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

                            text_ptr->pGLProgramUniform3fv(program_id,
                                                           _global.draw_text_fragment_shader_color_location,
                                                           1, /* count */
                                                           string_ptr->color);
                            text_ptr->pGLProgramUniform1f (program_id,
                                                           _global.draw_text_vertex_shader_scale_location,
                                                           string_ptr->scale);
                            text_ptr->pGLProgramUniform1i (program_id,
                                                           _global.draw_text_vertex_shader_n_origin_character_location,
                                                           n_characters_drawn_so_far);

                            text_ptr->pGLDrawArrays(GL_TRIANGLES,
                                                    n_characters_drawn_so_far * 6,
                                                    string_ptr->string_length * 6);
                        }

                        n_characters_drawn_so_far += string_ptr->string_length;
                    } /* for (uint32_t n_string = 0; n_string < n_strings; ++n_string) */

                    if (has_enabled_scissor_test)
                    {
                        text_ptr->pGLDisable(GL_SCISSOR_TEST);
                    }
                }
                text_ptr->pGLDisable(GL_BLEND);

                /* Clean up */
                text_ptr->pGLUseProgram (0);

                ASSERT_DEBUG_SYNC(text_ptr->pGLGetError() == GL_NO_ERROR,
                                  "");
            }
            system_critical_section_leave(_global_cs);
        } /* if (n_strings > 0) */
    }
    system_critical_section_leave(text_ptr->draw_cs);
}


/** Please see header for specification */
PUBLIC EMERALD_API ogl_text_string_id ogl_text_add_string(__in __notnull ogl_text text)
{
    ogl_text_string_id result          = 0;
    _ogl_text*         text_ptr        = (_ogl_text*) text;
    _ogl_text_string*  text_string_ptr = NULL;

    text_string_ptr = new (std::nothrow) _ogl_text_string;

    ASSERT_DEBUG_SYNC(text_string_ptr != NULL, "Out of memory");
    if (text_string_ptr != NULL)
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
        text_string_ptr->string               = NULL;
        text_string_ptr->string_buffer_length = 0;
        text_string_ptr->string_length        = 0;
        text_string_ptr->width_px             = 0;
        text_string_ptr->visible              = true;
        text_ptr->dirty                       = true;

        result = system_resizable_vector_get_amount_of_elements(text_ptr->strings);

        system_critical_section_enter(text_ptr->draw_cs);
        {
            system_resizable_vector_push(text_ptr->strings,
                                         text_string_ptr);
        }
        system_critical_section_leave(text_ptr->draw_cs);
    } /* if (text_string_ptr != NULL) */

    return result;
}


/** Please see header for specification */
PUBLIC EMERALD_API ogl_text ogl_text_create(__in __notnull system_hashed_ansi_string name,
                                            __in __notnull ogl_context               context,
                                            __in __notnull gfx_bfg_font_table        font_table,
                                            __in           uint32_t                  screen_width,
                                            __in           uint32_t                  screen_height)
{
    _ogl_text* result = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != NULL,
                      "Input context is null!");
    if (context == NULL)
    {
        LOG_ERROR("Input context is null - cannot create text instance.");

        goto end;
    }

    /* Instantiate the new object */
    result = new (std::nothrow) _ogl_text;

    ASSERT_ALWAYS_SYNC(result != NULL,
                       "Could not allocate memory for text instance.");
    if (result == NULL)
    {
        LOG_FATAL("Could not allocate memory for text instance.");

        goto end;
    }
    else
    {
        /* Now that we have place to store OGL-specific ids, we can request a call-back from context thread to fill them up.
         * Before that, however, fill the structure with data that is necessary for that to happen. */
        memset(result,
               0,
               sizeof(_ogl_text) );

        result->context          = context;
        result->default_color[0] = DEFAULT_COLOR_R;
        result->default_color[1] = DEFAULT_COLOR_G;
        result->default_color[2] = DEFAULT_COLOR_B;
        result->default_scale    = DEFAULT_SCALE;
        result->draw_cs          = system_critical_section_create();
        result->font_table       = font_table;
        result->name             = name;
        result->owner_context    = context;
        result->screen_width     = screen_width;
        result->screen_height    = screen_height;
        result->strings          = system_resizable_vector_create(4 /* default capacity */,
                                                                  sizeof(_ogl_text_string*) );

        /* Cache the buffer manager */
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_BUFFERS,
                                &result->buffers);

        /* Retrieve TBO alignment requirement */
        const ogl_context_gl_limits* limits_ptr = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LIMITS,
                                &limits_ptr);

        result->texture_buffer_offset_alignment = limits_ptr->texture_buffer_offset_alignment;

        /* Initialize GL func pointers */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints*                    entry_points    = NULL;
            const ogl_context_es_entrypoints_ext_texture_buffer* ts_entry_points = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES_EXT_TEXTURE_BUFFER,
                                    &ts_entry_points);

            result->pGLActiveTexture      = entry_points->pGLActiveTexture;
            result->pGLBindBuffer         = entry_points->pGLBindBuffer;
            result->pGLBindTexture        = entry_points->pGLBindTexture;
            result->pGLBindVertexArray    = entry_points->pGLBindVertexArray;
            result->pGLBlendEquation      = entry_points->pGLBlendEquation;
            result->pGLBlendFunc          = entry_points->pGLBlendFunc;
            result->pGLBufferData         = entry_points->pGLBufferData;
            result->pGLBufferSubData      = entry_points->pGLBufferSubData;
            result->pGLDeleteVertexArrays = entry_points->pGLDeleteVertexArrays;
            result->pGLDisable            = entry_points->pGLDisable;
            result->pGLDrawArrays         = entry_points->pGLDrawArrays;
            result->pGLEnable             = entry_points->pGLEnable;
            result->pGLGenVertexArrays    = entry_points->pGLGenVertexArrays;
            result->pGLGenerateMipmap     = entry_points->pGLGenerateMipmap;
            result->pGLGetError           = entry_points->pGLGetError;
            result->pGLProgramUniform1f   = entry_points->pGLProgramUniform1f;
            result->pGLProgramUniform1i   = entry_points->pGLProgramUniform1i;
            result->pGLProgramUniform3fv  = entry_points->pGLProgramUniform3fv;
            result->pGLScissor            = entry_points->pGLScissor;
            result->pGLTexBufferRange     = ts_entry_points->pGLTexBufferRangeEXT;
            result->pGLTexParameteri      = entry_points->pGLTexParameteri;
            result->pGLTexStorage2D       = entry_points->pGLTexStorage2D;
            result->pGLTexSubImage2D      = entry_points->pGLTexSubImage2D;
            result->pGLUseProgram         = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
            const ogl_context_gl_entrypoints*                         entry_points     = NULL;

            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);
            ogl_context_get_property(context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                    &dsa_entry_points);

            result->gl_pGLBindTexture              = entry_points->pGLBindTexture;
            result->gl_pGLNamedBufferSubDataEXT    = dsa_entry_points->pGLNamedBufferSubDataEXT;
            result->gl_pGLPolygonMode              = entry_points->pGLPolygonMode;
            result->gl_pGLTextureBufferRangeEXT    = dsa_entry_points->pGLTextureBufferRangeEXT;
            result->gl_pGLTextureParameteriEXT     = dsa_entry_points->pGLTextureParameteriEXT;
            result->gl_pGLTextureStorage2DEXT      = dsa_entry_points->pGLTextureStorage2DEXT;
            result->gl_pGLTextureSubImage2DEXT     = dsa_entry_points->pGLTextureSubImage2DEXT;
            result->pGLActiveTexture               = entry_points->pGLActiveTexture;
            result->pGLBindBuffer                  = entry_points->pGLBindBuffer;
            result->pGLBindVertexArray             = entry_points->pGLBindVertexArray;
            result->pGLBlendEquation               = entry_points->pGLBlendEquation;
            result->pGLBlendFunc                   = entry_points->pGLBlendFunc;
            result->pGLBufferData                  = entry_points->pGLBufferData;
            result->pGLBufferSubData               = entry_points->pGLBufferSubData;
            result->pGLDeleteVertexArrays          = entry_points->pGLDeleteVertexArrays;
            result->pGLDisable                     = entry_points->pGLDisable;
            result->pGLDrawArrays                  = entry_points->pGLDrawArrays;
            result->pGLEnable                      = entry_points->pGLEnable;
            result->pGLGenVertexArrays             = entry_points->pGLGenVertexArrays;
            result->pGLGenerateMipmap              = entry_points->pGLGenerateMipmap;
            result->pGLGetError                    = entry_points->pGLGetError;
            result->pGLProgramUniform1f            = entry_points->pGLProgramUniform1f;
            result->pGLProgramUniform1i            = entry_points->pGLProgramUniform1i;
            result->pGLProgramUniform3fv           = entry_points->pGLProgramUniform3fv;
            result->pGLScissor                     = entry_points->pGLScissor;
            result->pGLTexBufferRange              = entry_points->pGLTexBufferRange;
            result->pGLTexParameteri               = entry_points->pGLTexParameteri;
            result->pGLTexStorage2D                = entry_points->pGLTexStorage2D;
            result->pGLTexSubImage2D               = entry_points->pGLTexSubImage2D;
            result->pGLUseProgram                  = entry_points->pGLUseProgram;
        }

        /* Make sure the font table has been assigned a texture object */
        if (_global.font_tables.find(font_table) == _global.font_tables.end() )
        {
            ogl_context_request_callback_from_context_thread(context,
                                                             _ogl_text_create_font_table_to_callback_from_renderer,
                                                             result);
        }

        /* We need a call-back, now */
        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_text_construction_callback_from_renderer,
                                                         result);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _ogl_text_release,
                                                       OBJECT_TYPE_OGL_TEXT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Text Renderers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        goto end;
    }

end:
    return (ogl_text) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_text_delete_string(__in __notnull ogl_text           text,
                                               __in           ogl_text_string_id text_id)
{
    ASSERT_DEBUG_SYNC(false, "TODO");
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_text_draw(__in __notnull ogl_context context,
                                      __in __notnull ogl_text    text)
{
    _ogl_text* text_ptr = (_ogl_text*) text;

    /* Make sure the request is handled from rendering thread */
    ogl_context_request_callback_from_context_thread(context,
                                                     _ogl_text_draw_callback_from_renderer,
                                                     text);
}

/** Please see header for specification */
PUBLIC EMERALD_API const unsigned char* ogl_text_get(__in __notnull ogl_text           text,
                                                     __in           ogl_text_string_id text_string_id)
{
    const unsigned char* result          = NULL;
    _ogl_text*           text_ptr        = (_ogl_text*) text;
    _ogl_text_string*    text_string_ptr = NULL;

    if (system_resizable_vector_get_element_at(text_ptr->strings,
                                               text_string_id,
                                              &text_string_ptr) )
    {
        result = text_string_ptr->string;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t ogl_text_get_added_strings_counter(__in __notnull ogl_text instance)
{
    return system_resizable_vector_get_amount_of_elements(((_ogl_text*) instance)->strings);
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_context ogl_text_get_context(__in __notnull ogl_text text)
{
    return ((_ogl_text*) text)->context;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_text_get_text_string_property(__in  __notnull ogl_text                 text,
                                                          __in            ogl_text_string_property property,
                                                          __in            ogl_text_string_id       text_string_id,
                                                          __out __notnull void*                    out_result)
{
    _ogl_text* text_ptr = (_ogl_text*) text;

    switch(property)
    {
        case OGL_TEXT_STRING_PROPERTY_COLOR:
        {
            _ogl_text_string* text_string_ptr = NULL;

            if (text_string_id == TEXT_STRING_ID_DEFAULT)
            {
                memcpy(out_result,
                        text_ptr->default_color,
                       sizeof(text_ptr->default_color) );
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                memcpy(out_result,
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

        case OGL_TEXT_STRING_PROPERTY_POSITION_PX:
        case OGL_TEXT_STRING_PROPERTY_POSITION_SS:
        case OGL_TEXT_STRING_PROPERTY_SCISSOR_BOX:
        case OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX:
        case OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS:
        case OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX:
        case OGL_TEXT_STRING_PROPERTY_VISIBILITY:
        {
            _ogl_text_string* text_string_ptr = NULL;

            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                if (property == OGL_TEXT_STRING_PROPERTY_POSITION_PX)
                {
                    int* position = (int*) out_result;

                    position[0] = (int) ((text_string_ptr->position[0]) * (float) text_ptr->screen_width);
                    position[1] = (int) ((text_string_ptr->position[1]) * (float) text_ptr->screen_height);
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    float* position = (float*) out_result;

                    position[0] = text_string_ptr->position[0];
                    position[1] = text_string_ptr->position[1];
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_SCISSOR_BOX)
                {
                    memcpy(out_result,
                           text_string_ptr->scissor_box,
                           sizeof(text_string_ptr->scissor_box) );
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX)
                {
                    *(int*) out_result = int( float(text_string_ptr->height_px) * (1-text_string_ptr->scale));
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS)
                {
                    unsigned int window_height = 0;

                    *(float*) out_result = (float(text_string_ptr->height_px) * (1 - text_string_ptr->scale)) / float(text_ptr->screen_height);
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX)
                {
                    *(int*) out_result = int( float(text_string_ptr->width_px) * (1-text_string_ptr->scale));
                }
                else
                {
                    *(bool*) out_result = text_string_ptr->visible;
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

        case OGL_TEXT_STRING_PROPERTY_SCALE:
        {
            _ogl_text_string* text_string_ptr = NULL;

            if (text_string_id == TEXT_STRING_ID_DEFAULT)
            {
                *(float*) out_result = text_ptr->default_scale;
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                *(float*) out_result = text_string_ptr->scale;
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
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_text_set(__in __notnull ogl_text           text,
                                     __in           ogl_text_string_id text_string_id,
                                     __in __notnull const char*        raw_text_ptr)
{
    _ogl_text*        text_ptr        = (_ogl_text*) text;
    _ogl_text_string* text_string_ptr = NULL;

    system_critical_section_enter(text_ptr->draw_cs);

    /* Make sure input arguments are okay */
    ASSERT_DEBUG_SYNC(raw_text_ptr != NULL,
                      "Input text pointer is NULL!");

    if (raw_text_ptr == NULL)
    {
        LOG_ERROR("Input text pointer is NULL.");

        goto end;
    }

    system_resizable_vector_get_element_at(text_ptr->strings,
                                           text_string_id,
                                          &text_string_ptr);

    if (text_string_ptr == NULL)
    {
        LOG_ERROR("Could not retrieve text string descriptor");

        goto end;
    }

    /* Update text width and height properties */
    size_t raw_text_length = strlen((const char*) raw_text_ptr);
    
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
    } /* for (uint32_t n_character = 0; n_character < raw_text_length; ++n_character) */

    /* Excellent. Update underlying structure's data and we're done here */
    if (text_string_ptr->string_buffer_length <= raw_text_length)
    {
        if (text_string_ptr->string != NULL)
        {
            delete [] text_string_ptr->string;

            text_string_ptr->string = NULL;
        }

        if (raw_text_length == 0)
        {
            raw_text_length = 1;
        }

        text_string_ptr->string_buffer_length = (unsigned int) raw_text_length * 2;
        text_string_ptr->string               = new (std::nothrow) unsigned char[text_string_ptr->string_buffer_length];

        ASSERT_ALWAYS_SYNC(text_string_ptr->string != NULL,
                           "Could not reallocate memory block used for raw text storage!");
        if (text_string_ptr->string == NULL)
        {
            LOG_ERROR("Could not reallocate memory block used for raw text storage");

            goto end;
        }

        LOG_INFO("Performance degradation: text string reallocation");
    }

    if (text_string_ptr->string != NULL)
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

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_text_set_screen_properties(__in __notnull ogl_text instance,
                                                       __in           uint32_t screen_width,
                                                       __in           uint32_t screen_height)
{
    _ogl_text* text_ptr = (_ogl_text*) instance;

    text_ptr->screen_height = screen_height;
    text_ptr->screen_width  = screen_width;
    text_ptr->dirty         = true;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_text_set_text_string_property(__in __notnull ogl_text                 text,
                                                          __in           ogl_text_string_id       text_string_id,
                                                          __in           ogl_text_string_property property,
                                                          __in __notnull const void*              data)
{
    _ogl_text*        text_ptr        = (_ogl_text*) text;
    _ogl_text_string* text_string_ptr = NULL;

    switch (property)
    {
        case OGL_TEXT_STRING_PROPERTY_COLOR:
        {
            if (text_string_id == TEXT_STRING_ID_DEFAULT)
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

        case OGL_TEXT_STRING_PROPERTY_SCALE:
        {
            float new_scale = *(float*) data;

            ASSERT_DEBUG_SYNC(new_scale >= 0 && new_scale <= 1,
                              "Scale out of supported range - adapt the shaders");

            if (text_string_id == TEXT_STRING_ID_DEFAULT)
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
        } /* case OGL_TEXT_STRING_PROPERTY_SCALE: */

        case OGL_TEXT_STRING_PROPERTY_POSITION_PX:
        case OGL_TEXT_STRING_PROPERTY_POSITION_SS:
        case OGL_TEXT_STRING_PROPERTY_SCISSOR_BOX:
        case OGL_TEXT_STRING_PROPERTY_VISIBILITY:
        {
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                if (property == OGL_TEXT_STRING_PROPERTY_POSITION_PX)
                {
                    const int* position = (const int*) data;

                    text_string_ptr->position[0] = (float) (position[0]) / (float) text_ptr->screen_width;
                    text_string_ptr->position[1] = (float) (position[1]) / (float) text_ptr->screen_height;
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    const float* position = (const float*) data;

                    text_string_ptr->position[0] = position[0];
                    text_string_ptr->position[1] = position[1];
                }
                else
                if (property == OGL_TEXT_STRING_PROPERTY_SCISSOR_BOX)
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
    } /* switch (property) */

    text_ptr->dirty = true;
}