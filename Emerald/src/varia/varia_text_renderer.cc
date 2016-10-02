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
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_rendering_handler.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"
#include "varia/varia_text_renderer.h"

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

    ral_program_block_buffer fsdata_ub;
    ral_program_block_buffer vsdata_ub;

    ral_command_buffer last_cached_command_buffer;
    ral_gfx_state      last_cached_gfx_state;
    ral_present_task   last_cached_present_task;
    bool               last_cached_present_task_dirty;
    ral_texture_view   last_cached_present_task_target_texture_view; /* do NOT release */

    float default_color[3];
    float default_scale;

    system_critical_section   draw_cs;
    uint32_t                  draw_text_fragment_shader_color_ub_offset;
    ral_program               draw_text_program;
    ral_program_block_buffer  draw_text_program_data_ssb;
    uint32_t                  draw_text_vertex_shader_n_origin_character_ub_offset;
    uint32_t                  draw_text_vertex_shader_scale_ub_offset;

    gfx_bfg_font_table        font_table;
    ral_texture               font_table_texture;
    ral_texture_view          font_table_texture_view;
    ral_sampler               sampler;

    system_hashed_ansi_string name;
    ral_context               owner_context;
    uint32_t                  screen_height;
    uint32_t                  screen_width;
    system_resizable_vector   strings;

    ral_context context;

    REFCOUNT_INSERT_VARIABLES
} _varia_text_renderer;

typedef struct
{
    bool           visible;
    float          color[3];
    unsigned int   height_px;
    float          position[2];
    float          scale;
    int32_t        scissor_box[4];
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
                                              "    result = vec4(1.0, 0.0, 0.0, 0.0) + vec4(color.xyz * texel.x, (texel.x > 0.9) ? 1 : 0);\n"
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


/* Private functions */

/** Please see header for specification */
PRIVATE void _varia_text_renderer_create_font_table_to(_varia_text_renderer* text_ptr)
{
    ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

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

    /* Create a RAL texture for the font table */
    ral_texture_create_info         texture_create_info;
    const system_hashed_ansi_string texture_name = system_hashed_ansi_string_create_by_merging_two_strings("Text renderer ",
                                                                                                          system_hashed_ansi_string_get_buffer(text_ptr->name) );

    std::shared_ptr<ral_texture_mipmap_client_sourced_update_info> texture_update_ptr(new ral_texture_mipmap_client_sourced_update_info() );

    texture_create_info.base_mipmap_depth      = 1;
    texture_create_info.base_mipmap_height     = font_table_height;
    texture_create_info.base_mipmap_width      = font_table_width;
    texture_create_info.fixed_sample_locations = false;
    texture_create_info.format                 = RAL_FORMAT_RGB8_UNORM;
    texture_create_info.n_layers               = 1;
    texture_create_info.n_samples              = 1;
    texture_create_info.type                   = RAL_TEXTURE_TYPE_2D;
    texture_create_info.usage                  = RAL_TEXTURE_USAGE_SAMPLED_BIT;
    texture_create_info.use_full_mipmap_chain  = false;

    texture_update_ptr->data                    = font_table_data_ptr;
    texture_update_ptr->data_row_alignment      = 1;
    texture_update_ptr->data_size               = 3 * font_table_width * font_table_height;
    texture_update_ptr->data_type               = RAL_TEXTURE_DATA_TYPE_UBYTE;
    texture_update_ptr->pfn_delete_handler_proc = nullptr;
    texture_update_ptr->n_layer                 = 0;
    texture_update_ptr->n_mipmap                = 0;
    texture_update_ptr->region_size[0]          = font_table_width;
    texture_update_ptr->region_size[1]          = font_table_height;
    texture_update_ptr->region_size[2]          = 0;
    texture_update_ptr->region_start_offset[0]  = 0;
    texture_update_ptr->region_start_offset[1]  = 0;
    texture_update_ptr->region_start_offset[2]  = 0;

    ral_context_create_textures                   (text_ptr->context,
                                                   1, /* n_textures */
                                                  &texture_create_info,
                                                  &text_ptr->font_table_texture);
    ral_texture_set_mipmap_data_from_client_memory(text_ptr->font_table_texture,
                                                   1, /* n_updates */
                                                  &texture_update_ptr,
                                                   true /* async */);

    /* Create a font table texture view. Note that we need to swizzle R channel with B,
     * because the original data uses BGR layout.
     */
    ral_texture_view_create_info texture_view_create_info;

    texture_view_create_info.aspect             = RAL_TEXTURE_ASPECT_COLOR_BIT;
    texture_view_create_info.component_order[0] = RAL_TEXTURE_COMPONENT_BLUE;
    texture_view_create_info.component_order[1] = RAL_TEXTURE_COMPONENT_GREEN;
    texture_view_create_info.component_order[2] = RAL_TEXTURE_COMPONENT_RED;
    texture_view_create_info.format             = RAL_FORMAT_RGB8_UNORM;
    texture_view_create_info.n_base_layer       = 0;
    texture_view_create_info.n_base_mip         = 0;
    texture_view_create_info.n_layers           = 1;
    texture_view_create_info.n_mips             = 1;
    texture_view_create_info.texture            = text_ptr->font_table_texture;
    texture_view_create_info.type               = RAL_TEXTURE_TYPE_2D;

    ral_context_create_texture_views(text_ptr->context,
                                     1, /* n_texture_views */
                                    &texture_view_create_info,
                                    &text_ptr->font_table_texture_view);

    /* Create a sampler we will use for sampling the texture view */
    ral_sampler_create_info sampler_create_info;

    sampler_create_info.mag_filter  = RAL_TEXTURE_FILTER_LINEAR;
    sampler_create_info.min_filter  = RAL_TEXTURE_FILTER_LINEAR;
    sampler_create_info.mipmap_mode = RAL_TEXTURE_MIPMAP_MODE_NEAREST;
    sampler_create_info.wrap_s      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;
    sampler_create_info.wrap_t      = RAL_TEXTURE_WRAP_MODE_CLAMP_TO_EDGE;

    ral_context_create_samplers(text_ptr->context,
                                1, /* n_create_info_items */
                                &sampler_create_info,
                                &text_ptr->sampler);
}

/** TODO */
PRIVATE void _varia_text_renderer_init_programs(_varia_text_renderer* text_ptr)
{
    const ral_shader_create_info draw_text_fs_create_info =
    {
        system_hashed_ansi_string_create("varia_text_renderer fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info draw_text_vs_create_info =
    {
        system_hashed_ansi_string_create("varia_text_renderer vertex shader"),
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
        system_hashed_ansi_string_create("varia_text_renderer program")
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

    /* Assign bodies to the shaders */
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
                                   false /* async */) ||
        !ral_program_attach_shader(text_ptr->draw_text_program,
                                   result_shaders[1],
                                   false /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not link text drawing program.");
    }

    /* Retrieve uniform locations */
    const ral_program_variable* fragment_shader_color_ral_ptr            = nullptr;
    const ral_program_variable* vertex_shader_n_origin_character_ral_ptr = nullptr;
    const ral_program_variable* vertex_shader_scale_ral_ptr              = nullptr;

    if (!ral_program_get_block_variable_by_name(text_ptr->draw_text_program,
                                                system_hashed_ansi_string_create("FSData"),
                                                system_hashed_ansi_string_create("color"),
                                               &fragment_shader_color_ral_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve color uniform descriptor.");
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
    text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset = vertex_shader_n_origin_character_ral_ptr->block_offset;
    text_ptr->draw_text_vertex_shader_scale_ub_offset              = vertex_shader_scale_ral_ptr->block_offset;

    ASSERT_DEBUG_SYNC(text_ptr->draw_text_fragment_shader_color_ub_offset != -1,
                      "FSData color uniform UB offset is -1");
    ASSERT_DEBUG_SYNC(text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset != -1,
                      "VSData n_origin_character UB offset is -1");
    ASSERT_DEBUG_SYNC(text_ptr->draw_text_vertex_shader_scale_ub_offset != -1,
                      "VSData scale UB offset is -1");

    /* Delete the shader objects */
    ral_context_delete_objects(text_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shader_create_info_items,
                               reinterpret_cast<void* const*>(result_shaders) );

    /* Retrieve uniform block instances. */
    text_ptr->fsdata_ub = ral_program_block_buffer_create(text_ptr->context,
                                                          text_ptr->draw_text_program,
                                                          system_hashed_ansi_string_create("FSData") );
    text_ptr->vsdata_ub = ral_program_block_buffer_create(text_ptr->context,
                                                          text_ptr->draw_text_program,
                                                          system_hashed_ansi_string_create("VSData") );
}

/** TODO */
PRIVATE void _varia_text_renderer_prepare_vram_data_storage(_varia_text_renderer* text_ptr)
{
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
        text_ptr->data_buffer_contents        = reinterpret_cast<char*>(new (std::nothrow) char[text_ptr->data_buffer_contents_size]);

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
                                       reinterpret_cast<void* const*>(&text_ptr->data_buffer) );

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
}

/** Please see header for specification */
PRIVATE void _varia_text_renderer_release(void* text)
{
    _varia_text_renderer* text_ptr = (_varia_text_renderer*) text;

    /* First, free all objects that are not global */
    if (text_ptr->data_buffer != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_buffers */
                                   reinterpret_cast<void* const*>(&text_ptr->data_buffer) );

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
    if (text_ptr->draw_text_program != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&text_ptr->draw_text_program));

        text_ptr->draw_text_program = nullptr;
    }

    if (text_ptr->draw_text_program_data_ssb != nullptr)
    {
        ral_program_block_buffer_release(text_ptr->draw_text_program_data_ssb);

        text_ptr->draw_text_program_data_ssb = nullptr;
    }

    text_ptr->draw_text_fragment_shader_color_ub_offset            = -1;
    text_ptr->draw_text_vertex_shader_n_origin_character_ub_offset = -1;
    text_ptr->draw_text_vertex_shader_scale_ub_offset              = -1;

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

    if (text_ptr->font_table_texture != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                   1, /* n_textures */
                                   reinterpret_cast<void* const*>(&text_ptr->font_table_texture) );

        text_ptr->font_table_texture = nullptr;
    }

    if (text_ptr->font_table_texture_view != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                   1, /* n_textures */
                                   reinterpret_cast<void* const*>(&text_ptr->font_table_texture_view) );

        text_ptr->font_table_texture_view = nullptr;
    }

    if (text_ptr->last_cached_command_buffer != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&text_ptr->last_cached_command_buffer) );

        text_ptr->last_cached_command_buffer = nullptr;
    }

    if (text_ptr->last_cached_present_task != nullptr)
    {
        ral_present_task_release(text_ptr->last_cached_present_task);

        text_ptr->last_cached_present_task = nullptr;
    }

    if (text_ptr->last_cached_gfx_state != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&text_ptr->last_cached_gfx_state) );

        text_ptr->last_cached_gfx_state = nullptr;
    }

    if (text_ptr->sampler != nullptr)
    {
        ral_context_delete_objects(text_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                   1, /* n_textures */
                                   reinterpret_cast<void* const*>(&text_ptr->sampler) );

        text_ptr->sampler = nullptr;
    }

    if (text_ptr->strings != nullptr)
    {
        system_resizable_vector_release(text_ptr->strings);

        text_ptr->strings = nullptr;
    }
}

/** TODO */
PRIVATE void _varia_text_renderer_update_vram_data_storage_cpu_task_callback(void* text_raw_ptr)
{
    ral_backend_type                                                     backend_type   = RAL_BACKEND_TYPE_UNKNOWN;
    uint32_t                                                             n_text_strings = 0;
    ral_buffer_client_sourced_update_info                                data_update;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > data_update_ptrs;
    _varia_text_renderer*                                                text_ptr     = reinterpret_cast<_varia_text_renderer*>(text_raw_ptr);

    if (!text_ptr->dirty)
    {
        goto end;
    }

    ral_context_get_property(text_ptr->context,
                             RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                            &backend_type);

    /* Iterate through each character and prepare the data for uploading */
    float*   character_data_traveller_ptr = reinterpret_cast<float*>(text_ptr->data_buffer_contents);
    uint32_t largest_height               = 0;
    uint32_t summed_width                 = 0;

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
    data_update.data         = text_ptr->data_buffer_contents;
    data_update.data_size    = text_ptr->data_buffer_contents_size;
    data_update.start_offset = 0;

    data_update_ptrs.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&data_update,
                                                                                      NullDeleter<ral_buffer_client_sourced_update_info>() ));

    ral_buffer_set_data_from_client_memory(text_ptr->data_buffer,
                                           data_update_ptrs,
                                           false, /* async */
                                           false  /* sync_other_contexts */);

end:
    text_ptr->dirty = false;
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API varia_text_renderer_text_string_id varia_text_renderer_add_string(varia_text_renderer text)
{
    varia_text_renderer_text_string_id result          = 0;
    _varia_text_renderer*              text_ptr        = reinterpret_cast<_varia_text_renderer*>(text);
    _varia_text_renderer_text_string*  text_string_ptr = nullptr;

    text_string_ptr = new (std::nothrow) _varia_text_renderer_text_string;

    ASSERT_DEBUG_SYNC(text_string_ptr != nullptr,
                      "Out of memory");

    if (text_string_ptr != nullptr)
    {
        memcpy(text_string_ptr->color,
               text_ptr->default_color,
               sizeof(text_string_ptr->color) );

        text_string_ptr->height_px               = 0;
        text_string_ptr->position[0]             = 0;
        text_string_ptr->position[1]             = 0;
        text_string_ptr->scale                   = text_ptr->default_scale;
        text_string_ptr->scissor_box[0]          = -1;
        text_string_ptr->scissor_box[1]          = -1;
        text_string_ptr->scissor_box[2]          = -1;
        text_string_ptr->scissor_box[3]          = -1;
        text_string_ptr->string                  = nullptr;
        text_string_ptr->string_buffer_length    = 0;
        text_string_ptr->string_length           = 0;
        text_string_ptr->width_px                = 0;
        text_string_ptr->visible                 = true;
        text_ptr->dirty                          = true;
        text_ptr->last_cached_present_task_dirty = true;

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
PUBLIC varia_text_renderer varia_text_renderer_create(system_hashed_ansi_string name,
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
        result_ptr->fsdata_ub        = nullptr;
        result_ptr->name             = name;
        result_ptr->owner_context    = context;
        result_ptr->screen_width     = screen_width;
        result_ptr->screen_height    = screen_height;
        result_ptr->strings          = system_resizable_vector_create(4 /* default capacity */);
        result_ptr->vsdata_ub        = nullptr;

        ral_context_get_property(context,
                                 RAL_CONTEXT_PROPERTY_STORAGE_BUFFER_ALIGNMENT,
                                &result_ptr->shader_storage_buffer_offset_alignment);

        /* Make sure the font table has been assigned a texture object */
        _varia_text_renderer_create_font_table_to(result_ptr);

        /* Initialize rendering programs */
        _varia_text_renderer_init_programs(result_ptr);

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
PUBLIC EMERALD_API const unsigned char* varia_text_renderer_get(varia_text_renderer                text,
                                                                varia_text_renderer_text_string_id text_string_id)
{
    const unsigned char*              result          = nullptr;
    _varia_text_renderer*             text_ptr        = reinterpret_cast<_varia_text_renderer*>(text);
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
PUBLIC ral_present_task varia_text_renderer_get_present_task(varia_text_renderer text,
                                                             ral_texture_view    target_texture_view)
{
    /* TODO: This function should be rewritten to cache text string data with a single update buffer call
     *       and draw text strings with instanced draw calls.
     */
    uint32_t              n_strings                  = 0;
    ral_present_task      result                     = nullptr;
    uint32_t              target_texture_view_height = 0;
    uint32_t              target_texture_view_width  = 0;
    _varia_text_renderer* text_ptr                   = reinterpret_cast<_varia_text_renderer*>(text);

    /* Can we re-use the last present task we returned? */
    if ( text_ptr->last_cached_present_task                     != nullptr             &&
         text_ptr->last_cached_present_task_target_texture_view == target_texture_view &&
        !text_ptr->last_cached_present_task_dirty)
    {
        result = text_ptr->last_cached_present_task;

        goto end;
    }
    else
    if (text_ptr->last_cached_present_task != nullptr)
    {
        ral_present_task_release(text_ptr->last_cached_present_task);

        text_ptr->last_cached_present_task = nullptr;
    }

    /* Can we re-use the cached gfx_state instance? */
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &target_texture_view_height);
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &target_texture_view_width);

    if (text_ptr->last_cached_gfx_state != nullptr)
    {
        ral_command_buffer_set_viewport_command_info* gfx_state_viewport_ptr = nullptr;

        ral_gfx_state_get_property(text_ptr->last_cached_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &gfx_state_viewport_ptr);

        if (fabs(gfx_state_viewport_ptr->size[0] - target_texture_view_width)  > 1e-5f ||
            fabs(gfx_state_viewport_ptr->size[1] - target_texture_view_height) > 1e-5f)
        {
            ral_context_delete_objects(text_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&text_ptr->last_cached_gfx_state) );

            text_ptr->last_cached_gfx_state = nullptr;
        }
    }

    if (text_ptr->last_cached_gfx_state == nullptr)
    {
        ral_gfx_state_create_info                    gfx_state_create_info;
        ral_command_buffer_set_viewport_command_info gfx_state_viewport;

        gfx_state_viewport.depth_range[0] = 1e-5f;
        gfx_state_viewport.depth_range[1] = 1.0f;
        gfx_state_viewport.index          = 0;
        gfx_state_viewport.size[0]        = static_cast<float>(target_texture_view_width);
        gfx_state_viewport.size[1]        = static_cast<float>(target_texture_view_height);
        gfx_state_viewport.xy  [0]        = 0.0f;
        gfx_state_viewport.xy  [1]        = 0.0f;

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLES;
        gfx_state_create_info.scissor_test                         = true;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes_enabled         = false;
        gfx_state_create_info.static_viewports_enabled             = true;
        gfx_state_create_info.static_viewports                     = &gfx_state_viewport;

        ral_context_create_gfx_states(text_ptr->context,
                                      1, /* n_create_info_items */
                                     &gfx_state_create_info,
                                     &text_ptr->last_cached_gfx_state);
    }

    /* If necessary, create the data buffer */
    _varia_text_renderer_prepare_vram_data_storage(text_ptr);

    /* Start recording the commands .. */
    if (text_ptr->last_cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info command_buffer_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = true;
        command_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(text_ptr->context,
                                           1, /* n_command_buffers */
                                           &command_buffer_create_info,
                                           &text_ptr->last_cached_command_buffer);
    }

    ral_command_buffer_start_recording(text_ptr->last_cached_command_buffer);
    {
        ral_command_buffer_record_set_gfx_state(text_ptr->last_cached_command_buffer,
                                                text_ptr->last_cached_gfx_state);

        system_critical_section_enter(text_ptr->draw_cs);
        {
            system_resizable_vector_get_property(text_ptr->strings,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_strings);

            if (n_strings > 0)
            {
                ral_command_buffer_set_binding_command_info            binding_info[4];
                ral_command_buffer_draw_call_regular_command_info      draw_call_info;
                uint32_t                                               n_characters_drawn_so_far = 0;
                ral_command_buffer_set_color_rendertarget_command_info rt_info                   = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
                ral_command_buffer_set_scissor_box_command_info        rt_wide_scissor_box;
                ral_buffer                                             ub_fsdata_bo              = nullptr;
                uint32_t                                               ub_fsdata_bo_size         =  0;
                ral_buffer                                             ub_vsdata_bo              = nullptr;
                uint32_t                                               ub_vsdata_bo_size         =  0;

                ral_program_block_buffer_get_property(text_ptr->fsdata_ub,
                                                      RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                     &ub_fsdata_bo);
                ral_program_block_buffer_get_property(text_ptr->vsdata_ub,
                                                      RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                     &ub_vsdata_bo);

                ral_buffer_get_property(ub_fsdata_bo,
                                        RAL_BUFFER_PROPERTY_SIZE,
                                       &ub_fsdata_bo_size);
                ral_buffer_get_property(ub_vsdata_bo,
                                        RAL_BUFFER_PROPERTY_SIZE,
                                       &ub_vsdata_bo_size);


                ASSERT_DEBUG_SYNC(text_ptr->data_buffer != nullptr &&
                                  text_ptr->sampler     != nullptr &&
                                  ub_fsdata_bo          != nullptr &&
                                  ub_vsdata_bo          != nullptr,
                                  "Cannot bind a null object");

                binding_info[0].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
                binding_info[0].name                          = system_hashed_ansi_string_create("dataSSB");
                binding_info[0].storage_buffer_binding.buffer = text_ptr->data_buffer;
                binding_info[0].storage_buffer_binding.offset = 0;
                binding_info[0].storage_buffer_binding.size   = text_ptr->data_buffer_contents_size;

                binding_info[1].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
                binding_info[1].name                               = system_hashed_ansi_string_create("font_table");
                binding_info[1].sampled_image_binding.sampler      = text_ptr->sampler;
                binding_info[1].sampled_image_binding.texture_view = text_ptr->font_table_texture_view;

                binding_info[2].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                binding_info[2].name                          = system_hashed_ansi_string_create("FSData");
                binding_info[2].uniform_buffer_binding.buffer = ub_fsdata_bo;
                binding_info[2].uniform_buffer_binding.offset = 0;
                binding_info[2].uniform_buffer_binding.size   = ub_fsdata_bo_size;

                binding_info[3].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
                binding_info[3].name                          = system_hashed_ansi_string_create("VSData");
                binding_info[3].uniform_buffer_binding.buffer = ub_vsdata_bo;
                binding_info[3].uniform_buffer_binding.offset = 0;
                binding_info[3].uniform_buffer_binding.size   = ub_vsdata_bo_size;

                rt_info.blend_enabled          = true;
                rt_info.blend_op_alpha         = RAL_BLEND_OP_ADD;
                rt_info.blend_op_color         = RAL_BLEND_OP_ADD;
                rt_info.dst_alpha_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                rt_info.dst_color_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                rt_info.rendertarget_index     = 0;
                rt_info.src_alpha_blend_factor = RAL_BLEND_FACTOR_SRC_ALPHA;
                rt_info.src_color_blend_factor = RAL_BLEND_FACTOR_SRC_ALPHA;
                rt_info.texture_view           = target_texture_view;

                rt_wide_scissor_box.index   = 0;
                rt_wide_scissor_box.size[0] = target_texture_view_width;
                rt_wide_scissor_box.size[1] = target_texture_view_height;
                rt_wide_scissor_box.xy  [0] = 0;
                rt_wide_scissor_box.xy  [1] = 0;


                ral_command_buffer_record_set_program            (text_ptr->last_cached_command_buffer,
                                                                  text_ptr->draw_text_program);
                ral_command_buffer_record_set_bindings           (text_ptr->last_cached_command_buffer,
                                                                  sizeof(binding_info) / sizeof(binding_info[0]),
                                                                  binding_info);
                ral_command_buffer_record_set_color_rendertargets(text_ptr->last_cached_command_buffer,
                                                                  1, /* n_rendertargets */
                                                                 &rt_info);

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
                            ral_command_buffer_record_set_scissor_boxes(text_ptr->last_cached_command_buffer,
                                                                        1, /* n_scissor_boxes */
                                                                       &rt_wide_scissor_box);
                        }
                        else
                        {
                            ral_command_buffer_set_scissor_box_command_info string_scissor_box;

                            string_scissor_box.index   = 0;
                            string_scissor_box.size[0] = string_ptr->scissor_box[2];
                            string_scissor_box.size[1] = string_ptr->scissor_box[3];
                            string_scissor_box.xy  [0] = string_ptr->scissor_box[0];
                            string_scissor_box.xy  [1] = string_ptr->scissor_box[1];

                            ral_command_buffer_record_set_scissor_boxes(text_ptr->last_cached_command_buffer,
                                                                        1, /* n_scissor_boxes */
                                                                       &string_scissor_box);
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

                        ral_program_block_buffer_sync_via_command_buffer(text_ptr->fsdata_ub,
                                                                         text_ptr->last_cached_command_buffer);
                        ral_program_block_buffer_sync_via_command_buffer(text_ptr->vsdata_ub,
                                                                         text_ptr->last_cached_command_buffer);

                        draw_call_info.base_instance = 0;
                        draw_call_info.base_vertex   = n_characters_drawn_so_far * 6;
                        draw_call_info.n_instances   = 1;
                        draw_call_info.n_vertices    = string_ptr->string_length * 6;

                        ral_command_buffer_record_draw_call_regular(text_ptr->last_cached_command_buffer,
                                                                    1, /* n_draw_calls */
                                                                   &draw_call_info);
                    }

                    n_characters_drawn_so_far += string_ptr->string_length;
                }
            }
        }
        system_critical_section_leave(text_ptr->draw_cs);
    }
    ral_command_buffer_stop_recording(text_ptr->last_cached_command_buffer);

    /* Form present task */
    ral_present_task                    present_task_cpu;
    ral_present_task_cpu_create_info    present_task_cpu_create_info;
    ral_present_task_io                 present_task_cpu_unique_output;
    ral_present_task                    present_task_gpu;
    ral_present_task_gpu_create_info    present_task_gpu_create_info;
    ral_present_task_io                 present_task_gpu_unique_inputs[2];
    ral_present_task_io                 present_task_gpu_unique_output;
    ral_present_task_group_create_info  result_present_task_create_info;
    ral_present_task_ingroup_connection result_present_task_ingroup_connection;
    ral_present_task                    result_present_task_present_tasks[2];
    ral_present_task_group_mapping      result_present_task_unique_input;
    ral_present_task_group_mapping      result_present_task_unique_output;

    present_task_cpu_unique_output.buffer      = text_ptr->data_buffer;
    present_task_cpu_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    present_task_cpu_create_info.cpu_task_callback_user_arg = text_ptr;
    present_task_cpu_create_info.n_unique_inputs            = 0;
    present_task_cpu_create_info.n_unique_outputs           = 1;
    present_task_cpu_create_info.pfn_cpu_task_callback_proc = _varia_text_renderer_update_vram_data_storage_cpu_task_callback;
    present_task_cpu_create_info.unique_inputs              = nullptr;
    present_task_cpu_create_info.unique_outputs             = &present_task_cpu_unique_output;

    present_task_cpu = ral_present_task_create_cpu(system_hashed_ansi_string_create("Text renderer: Data buffer update"),
                                                  &present_task_cpu_create_info);


    present_task_gpu_unique_inputs[0].buffer       = text_ptr->data_buffer;
    present_task_gpu_unique_inputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    present_task_gpu_unique_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    present_task_gpu_unique_inputs[1].texture_view = target_texture_view;

    present_task_gpu_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    present_task_gpu_unique_output.texture_view = target_texture_view;

    present_task_gpu_create_info.command_buffer   = text_ptr->last_cached_command_buffer;
    present_task_gpu_create_info.n_unique_inputs  = sizeof(present_task_gpu_unique_inputs) / sizeof(present_task_gpu_unique_inputs[0]);
    present_task_gpu_create_info.n_unique_outputs = 1;
    present_task_gpu_create_info.unique_inputs    =  present_task_gpu_unique_inputs;
    present_task_gpu_create_info.unique_outputs   = &present_task_gpu_unique_output;

    present_task_gpu = ral_present_task_create_gpu(system_hashed_ansi_string_create("Text renderer: Rasterization"),
                                                  &present_task_gpu_create_info);


    result_present_task_ingroup_connection.input_present_task_index     = 1;
    result_present_task_ingroup_connection.input_present_task_io_index  = 0; /* data_buffer */
    result_present_task_ingroup_connection.output_present_task_index    = 0;
    result_present_task_ingroup_connection.output_present_task_io_index = 0; /* data_buffer */

    result_present_task_present_tasks[0] = present_task_cpu;
    result_present_task_present_tasks[1] = present_task_gpu;

    result_present_task_unique_input.group_task_io_index   = 0;
    result_present_task_unique_input.n_present_task        = 1;
    result_present_task_unique_input.present_task_io_index = 1; /* target_texture_view */

    result_present_task_unique_output.group_task_io_index   = 0;
    result_present_task_unique_output.present_task_io_index = 0; /* target_texture_view */
    result_present_task_unique_output.n_present_task        = 1;

    result_present_task_create_info.ingroup_connections                      = &result_present_task_ingroup_connection;
    result_present_task_create_info.n_ingroup_connections                    = 1;
    result_present_task_create_info.n_present_tasks                          = sizeof(result_present_task_present_tasks) / sizeof(result_present_task_present_tasks[0]);
    result_present_task_create_info.n_total_unique_inputs                    = 1;
    result_present_task_create_info.n_total_unique_outputs                   = 1;
    result_present_task_create_info.n_unique_input_to_ingroup_task_mappings  = 1;
    result_present_task_create_info.n_unique_output_to_ingroup_task_mappings = 1;
    result_present_task_create_info.present_tasks                            =  result_present_task_present_tasks;
    result_present_task_create_info.unique_input_to_ingroup_task_mapping     = &result_present_task_unique_input;
    result_present_task_create_info.unique_output_to_ingroup_task_mapping    = &result_present_task_unique_output;

    text_ptr->last_cached_present_task                     = ral_present_task_create_group(system_hashed_ansi_string_create("Text renderer: rasterization"),
                                                                                           &result_present_task_create_info);
    text_ptr->last_cached_present_task_target_texture_view = target_texture_view;

    result = text_ptr->last_cached_present_task;


    ral_present_task_release(present_task_cpu);
    ral_present_task_release(present_task_gpu);
    ral_present_task_retain(result);

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_get_property(varia_text_renderer          text,
                                                         varia_text_renderer_property property,
                                                         void*                        out_result_ptr)
{
    _varia_text_renderer* text_ptr = reinterpret_cast<_varia_text_renderer*>(text);

    switch (property)
    {
        case VARIA_TEXT_RENDERER_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = text_ptr->context;

            break;
        }

        case VARIA_TEXT_RENDERER_PROPERTY_N_ADDED_TEXT_STRINGS:
        {
            system_resizable_vector_get_property(text_ptr->strings,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized varia_text_renderer_property value specified.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_text_renderer_get_text_string_property(varia_text_renderer                      text,
                                                                     varia_text_renderer_text_string_property property,
                                                                     varia_text_renderer_text_string_id       text_string_id,
                                                                     void*                                    out_result_ptr)
{
    _varia_text_renderer* text_ptr = reinterpret_cast<_varia_text_renderer*>(text);

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
                    int* position = reinterpret_cast<int*>(out_result_ptr);

                    position[0] = (int) ((text_string_ptr->position[0]) * (float) text_ptr->screen_width);
                    position[1] = (int) ((text_string_ptr->position[1]) * (float) text_ptr->screen_height);
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    float* position = reinterpret_cast<float*>(out_result_ptr);

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
                    *reinterpret_cast<int*>(out_result_ptr) = int( float(text_string_ptr->height_px) * (1-text_string_ptr->scale));
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS)
                {
                    unsigned int window_height = 0;

                    *reinterpret_cast<float*>(out_result_ptr) = (float(text_string_ptr->height_px) * (1 - text_string_ptr->scale)) / float(text_ptr->screen_height);
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX)
                {
                    *reinterpret_cast<int*>(out_result_ptr) = int( float(text_string_ptr->width_px) * (1-text_string_ptr->scale));
                }
                else
                {
                    *reinterpret_cast<bool*>(out_result_ptr) = text_string_ptr->visible;
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
                *reinterpret_cast<float*>(out_result_ptr) = text_ptr->default_scale;
            }
            else
            if (system_resizable_vector_get_element_at(text_ptr->strings,
                                                       text_string_id,
                                                      &text_string_ptr) )
            {
                *reinterpret_cast<float*>(out_result_ptr) = text_string_ptr->scale;
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
    _varia_text_renderer*             text_ptr        = reinterpret_cast<_varia_text_renderer*>(text);
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
    raw_text_length = strlen(reinterpret_cast<const char*>(raw_text_ptr) );

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

        memcpy(reinterpret_cast<void*>(text_string_ptr->string),
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
    _varia_text_renderer*             text_ptr        = reinterpret_cast<_varia_text_renderer*>(text);
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
            float new_scale = *reinterpret_cast<const float*>(data);

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
                    const int* position = reinterpret_cast<const int*>(data);

                    text_string_ptr->position[0] = (float) (position[0]) / (float) text_ptr->screen_width;
                    text_string_ptr->position[1] = (float) (position[1]) / (float) text_ptr->screen_height;
                }
                else
                if (property == VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_SS)
                {
                    const float* position = reinterpret_cast<const float*>(data);

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
                    text_string_ptr->visible = *reinterpret_cast<const bool*>(data);
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