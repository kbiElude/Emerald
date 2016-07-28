/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_texture_preview.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"
#include <string>


const float _ui_texture_preview_text_color[] = {1, 1, 1, 1.0f};

/** Internal types */
typedef struct
{
    float            blend_color[4];
    ral_blend_factor blend_factor_dst_alpha;
    ral_blend_factor blend_factor_dst_rgb;
    ral_blend_factor blend_factor_src_alpha;
    ral_blend_factor blend_factor_src_rgb;
    ral_blend_op     blend_op_alpha;
    ral_blend_op     blend_op_rgb;
    bool             is_blending_enabled;
    uint32_t         layer_shown;
    bool             visible;

    ral_command_buffer last_cached_command_buffer;
    ral_gfx_state      last_cached_gfx_state;
    bool               last_cached_gfx_state_dirty;
    ral_present_task   last_cached_present_task;
    ral_texture_view   last_cached_sampled_texture_view;
    ral_texture_view   last_cached_target_texture_view;

    float                   border_width[2];
    float                   max_size[2];
    ui_texture_preview_type preview_type;
    float                   x1y1x2y2[4];

    ral_context               context;
    system_hashed_ansi_string name;
    ral_program               program;
    uint32_t                  program_border_width_ub_offset;
    uint32_t                  program_layer_ub_offset;
    uint32_t                  program_texture_ub_offset;
    ral_program_block_buffer  program_ub_fs;
    uint32_t                  program_ub_fs_bo_size;
    ral_program_block_buffer  program_ub_vs;
    uint32_t                  program_ub_vs_bo_size;
    uint32_t                  program_x1y1x2y2_ub_offset;
    ral_sampler               sampler;
    ral_texture_view          texture_view;
    bool                      texture_view_initialized;

    uint32_t            text_index;
    varia_text_renderer text_renderer;

} _ui_texture_preview;

/** Internal variables */
static const char* ui_texture_preview_renderer_sampler2d_alpha_body      = "result = vec4(textureLod(texture, uv, 0).aaa, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_depth_body      = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_red_body        = "result = vec4(textureLod(texture, uv, 0).xxx, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_rgb_body        = "result = vec4(textureLod(texture, uv, 0).xyz, 1.0);\n";
static const char* ui_texture_preview_renderer_sampler2d_rgba_body       = "result = textureLod(texture, uv, 0);\n";
static const char* ui_texture_preview_renderer_sampler2darray_depth_body = "result = vec4(textureLod(texture, vec3(uv, layer), 0.0).xxx, 1.0);\n";

static const char* ui_texture_preview_fragment_shader_body =
    "#version 430 core\n"
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
PRIVATE void _ui_texture_preview_cpu_task_callback(void* texture_preview_raw_ptr)
{
    _ui_texture_preview* texture_preview_ptr = reinterpret_cast<_ui_texture_preview*>(texture_preview_raw_ptr);

    /* Set up uniforms */
    float layer_index = static_cast<float>(texture_preview_ptr->layer_shown);

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

    ral_program_block_buffer_sync_immediately(texture_preview_ptr->program_ub_fs);
    ral_program_block_buffer_sync_immediately(texture_preview_ptr->program_ub_vs);
}

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
    ral_shader fs = nullptr;
    ral_shader vs = nullptr;

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
    const char* render_result_body           = nullptr;
    const char* sampler_type_body            = nullptr;
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
    }

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
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(texture_preview_ptr->program,
                                   vs,
                                   true /* async */) )
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
PRIVATE void _ui_texture_preview_init_texture(_ui_texture_preview* texture_preview_raw_ptr)
{
    _ui_texture_preview* texture_preview_ptr = reinterpret_cast<_ui_texture_preview*>(texture_preview_raw_ptr);
    system_window        window              = nullptr;
    int                  window_size[2]      = {0};

    ASSERT_DEBUG_SYNC(!texture_preview_ptr->texture_view_initialized,
                      "Texture view already initialized");

    ral_context_get_property  (texture_preview_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    /* Determine preview size */
    float preview_height_ss = 0.0f;
    float preview_width_ss  = 0.0f;
    int   texture_height_px = 0;
    float texture_height_ss = 0.0f;
    float texture_h_ratio   = 0.0f;
    int   texture_width_px  = 0;
    float texture_width_ss  = 0.0f;
    float texture_v_ratio   = 0.0f;

    ral_texture_view_get_mipmap_property(texture_preview_ptr->texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &texture_height_px);
    ral_texture_view_get_mipmap_property(texture_preview_ptr->texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &texture_width_px);

    ASSERT_ALWAYS_SYNC(texture_height_px != 0 &&
                       texture_width_px  != 0,
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
    const ral_program_variable* border_width_uniform_ral_ptr = nullptr;
    const ral_program_variable* layer_uniform_ral_ptr        = nullptr;
    const ral_program_variable* texture_uniform_ral_ptr      = nullptr;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = nullptr;

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

    if (border_width_uniform_ral_ptr != nullptr)
    {
        texture_preview_ptr->program_border_width_ub_offset = border_width_uniform_ral_ptr->block_offset;
    }
    else
    {
        texture_preview_ptr->program_border_width_ub_offset = -1;
    }

    if (layer_uniform_ral_ptr != nullptr)
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
    ral_buffer   ub_fs_bo_ral = nullptr;
    unsigned int ub_fs_index = -1;
    ral_buffer   ub_vs_bo_ral = nullptr;
    unsigned int ub_vs_index = -1;

    if (texture_preview_ptr->program_ub_fs == nullptr)
    {
        texture_preview_ptr->program_ub_fs = ral_program_block_buffer_create(texture_preview_ptr->context,
                                                                             texture_preview_ptr->program,
                                                                             system_hashed_ansi_string_create("dataFS") );

        ral_program_block_buffer_get_property  (texture_preview_ptr->program_ub_fs,
                                                RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                               &ub_fs_bo_ral);
        ral_buffer_get_property                (ub_fs_bo_ral,
                                                RAL_BUFFER_PROPERTY_SIZE,
                                               &texture_preview_ptr->program_ub_fs_bo_size);
    }

    if (texture_preview_ptr->program_ub_vs == nullptr)
    {
        texture_preview_ptr->program_ub_vs = ral_program_block_buffer_create(texture_preview_ptr->context,
                                                                             texture_preview_ptr->program,
                                                                             system_hashed_ansi_string_create("dataVS") );

        ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_vs_bo_ral);
        ral_buffer_get_property              (ub_vs_bo_ral,
                                              RAL_BUFFER_PROPERTY_SIZE,
                                             &texture_preview_ptr->program_ub_vs_bo_size);
    }

    /* All done */
    texture_preview_ptr->texture_view_initialized = true;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_deinit(void* internal_instance)
{
    _ui_texture_preview* ui_texture_preview_ptr = reinterpret_cast<_ui_texture_preview*>(internal_instance);

    ral_context_delete_objects(ui_texture_preview_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &ui_texture_preview_ptr->program);

    varia_text_renderer_set(ui_texture_preview_ptr->text_renderer,
                            ui_texture_preview_ptr->text_index,
                            "");

    if (ui_texture_preview_ptr->last_cached_command_buffer != nullptr)
    {
        ral_command_buffer_release(ui_texture_preview_ptr->last_cached_command_buffer);

        ui_texture_preview_ptr->last_cached_command_buffer = nullptr;
    }

    if (ui_texture_preview_ptr->last_cached_gfx_state != nullptr)
    {
        ral_gfx_state_release(ui_texture_preview_ptr->last_cached_gfx_state);

        ui_texture_preview_ptr->last_cached_gfx_state = nullptr;
    }

    if (ui_texture_preview_ptr->program_ub_fs != nullptr)
    {
        ral_program_block_buffer_release(ui_texture_preview_ptr->program_ub_fs);

        ui_texture_preview_ptr->program_ub_fs = nullptr;
    }

    if (ui_texture_preview_ptr->program_ub_vs != nullptr)
    {
        ral_program_block_buffer_release(ui_texture_preview_ptr->program_ub_vs);

        ui_texture_preview_ptr->program_ub_vs = nullptr;
    }

    if (ui_texture_preview_ptr->sampler != nullptr)
    {
        ral_sampler_release(ui_texture_preview_ptr->sampler);

        ui_texture_preview_ptr->sampler = nullptr;
    }

    delete ui_texture_preview_ptr;
}

/** Please see header for specification */
PUBLIC ral_present_task ui_texture_preview_get_present_task(void*            internal_instance,
                                                            ral_texture_view target_texture_view)
{
    uint32_t             target_texture_view_height = 0;
    uint32_t             target_texture_view_width  = 0;
    _ui_texture_preview* texture_preview_ptr        = reinterpret_cast<_ui_texture_preview*>(internal_instance);
    ral_buffer           ub_fs_bo                   = nullptr;
    ral_buffer           ub_vs_bo                   = nullptr;

    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &target_texture_view_height);
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &target_texture_view_width);

    /* Can we re-use the last cached present task? */
    if ( texture_preview_ptr->last_cached_present_task                                              &&
        !texture_preview_ptr->last_cached_gfx_state_dirty                                           &&
         texture_preview_ptr->last_cached_sampled_texture_view == texture_preview_ptr->texture_view &&
         texture_preview_ptr->last_cached_target_texture_view  == target_texture_view)
    {
        goto end;
    }

    /* If TO was not initialized, set it up now */
    if (!texture_preview_ptr->texture_view_initialized)
    {
        _ui_texture_preview_init_texture(texture_preview_ptr);
    }

    /* Can we re-use the last cached gfx_state instance? */
    if (texture_preview_ptr->last_cached_gfx_state != nullptr)
    {
        if (texture_preview_ptr->last_cached_gfx_state_dirty)
        {
            ral_gfx_state_release(texture_preview_ptr->last_cached_gfx_state);

            texture_preview_ptr->last_cached_gfx_state = nullptr;
        }
        else
        {
            ral_command_buffer_set_viewport_command_info gfx_state_viewport;

            ral_gfx_state_get_property(texture_preview_ptr->last_cached_gfx_state,
                                       RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                      &gfx_state_viewport);

            if (fabs(gfx_state_viewport.size[0] - target_texture_view_width)  > 1e-5f ||
                fabs(gfx_state_viewport.size[1] - target_texture_view_height) > 1e-5f)
            {
                ral_gfx_state_release(texture_preview_ptr->last_cached_gfx_state);

                texture_preview_ptr->last_cached_gfx_state = nullptr;
            }
        }
    }

    if (texture_preview_ptr->last_cached_gfx_state == nullptr)
    {
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_scissor_box_command_info scissor;
        ral_command_buffer_set_viewport_command_info    viewport;

        scissor.index   = 0;
        scissor.size[0] = target_texture_view_width;
        scissor.size[1] = target_texture_view_height;
        scissor.xy  [0] = 0;
        scissor.xy  [1] = 0;

        viewport.depth_range[0] = 0.0f;
        viewport.depth_range[1] = 0.0f;
        viewport.index          = 0;
        viewport.size[0]        = static_cast<float>(target_texture_view_width);
        viewport.size[1]        = static_cast<float>(target_texture_view_height);

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &scissor;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &viewport;
        gfx_state_create_info.static_viewports_enabled             = true;

        texture_preview_ptr->last_cached_gfx_state = ral_gfx_state_create(texture_preview_ptr->context,
                                                                         &gfx_state_create_info);
    }

    /* Start baking a command buffer */
    ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_fs_bo);
    ral_program_block_buffer_get_property(texture_preview_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_vs_bo);

    if (texture_preview_ptr->last_cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info command_buffer_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = true;
        command_buffer_create_info.is_transient                            = false;

        texture_preview_ptr->last_cached_command_buffer = ral_command_buffer_create(texture_preview_ptr->context,
                                                                                   &command_buffer_create_info);
    }

    ral_command_buffer_start_recording(texture_preview_ptr->last_cached_command_buffer);
    {
        if (texture_preview_ptr->texture_view != nullptr)
        {
            ral_command_buffer_set_binding_command_info            bindings[3];
            ral_command_buffer_draw_call_regular_command_info      draw_call_info;
            ral_command_buffer_set_color_rendertarget_command_info rt_info;

            bindings[0].binding_type                       = RAL_BINDING_TYPE_SAMPLED_IMAGE;
            bindings[0].name                               = system_hashed_ansi_string_create("texture");
            bindings[0].sampled_image_binding.sampler      = texture_preview_ptr->sampler;
            bindings[0].sampled_image_binding.texture_view = texture_preview_ptr->texture_view;

            bindings[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            bindings[1].name                          = system_hashed_ansi_string_create("dataFS");
            bindings[1].uniform_buffer_binding.buffer = ub_fs_bo;
            bindings[1].uniform_buffer_binding.offset = 0;
            bindings[1].uniform_buffer_binding.size   = texture_preview_ptr->program_ub_fs_bo_size;

            bindings[2].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            bindings[2].name                          = system_hashed_ansi_string_create("dataVS");
            bindings[2].uniform_buffer_binding.buffer = ub_vs_bo;
            bindings[2].uniform_buffer_binding.offset = 0;
            bindings[2].uniform_buffer_binding.size   = texture_preview_ptr->program_ub_vs_bo_size;

            draw_call_info.base_instance = 0;
            draw_call_info.base_vertex   = 0;
            draw_call_info.n_instances   = 1;
            draw_call_info.n_vertices    = 4;

            rt_info.blend_enabled          = texture_preview_ptr->is_blending_enabled;
            rt_info.blend_op_alpha         = texture_preview_ptr->blend_op_alpha;
            rt_info.blend_op_color         = texture_preview_ptr->blend_op_rgb;
            rt_info.dst_alpha_blend_factor = texture_preview_ptr->blend_factor_dst_alpha;
            rt_info.dst_color_blend_factor = texture_preview_ptr->blend_factor_dst_rgb;
            rt_info.rendertarget_index     = 0;
            rt_info.src_alpha_blend_factor = texture_preview_ptr->blend_factor_src_alpha;
            rt_info.src_color_blend_factor = texture_preview_ptr->blend_factor_src_rgb;
            rt_info.texture_view           = target_texture_view;

            memcpy(rt_info.blend_constant.f32,
                   texture_preview_ptr->blend_color,
                    sizeof(texture_preview_ptr->blend_color) );


            ral_command_buffer_record_set_bindings           (texture_preview_ptr->last_cached_command_buffer,
                                                              sizeof(bindings) / sizeof(bindings[0]),
                                                              bindings);
            ral_command_buffer_record_set_color_rendertargets(texture_preview_ptr->last_cached_command_buffer,
                                                              1, /* n_rendertargets */
                                                             &rt_info);
            ral_command_buffer_record_set_program            (texture_preview_ptr->last_cached_command_buffer,
                                                              texture_preview_ptr->program);

            ral_command_buffer_record_draw_call_regular(texture_preview_ptr->last_cached_command_buffer,
                                                        1, /* n_draw_calls */
                                                       &draw_call_info);
        }
    }
    ral_command_buffer_stop_recording(texture_preview_ptr->last_cached_command_buffer);

    /* Form present task */
    ral_present_task                    cpu_present_task;
    ral_present_task_cpu_create_info    cpu_present_task_create_info;
    ral_present_task_io                 cpu_present_task_outputs[2];
    ral_present_task                    draw_present_task;
    ral_present_task_gpu_create_info    draw_present_task_create_info;
    ral_present_task_io                 draw_present_task_inputs[3];
    ral_present_task_io                 draw_present_task_output;
    ral_present_task                    present_tasks[2];
    ral_present_task_group_create_info  result_present_task_create_info;
    ral_present_task_ingroup_connection result_present_task_ingroup_connections[2];
    ral_present_task_group_mapping      result_present_task_input_mapping;
    ral_present_task_group_mapping      result_present_task_output_mapping;

    cpu_present_task_outputs[0].buffer       = ub_fs_bo;
    cpu_present_task_outputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    cpu_present_task_outputs[1].buffer       = ub_vs_bo;
    cpu_present_task_outputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_present_task_create_info.cpu_task_callback_user_arg = texture_preview_ptr;
    cpu_present_task_create_info.n_unique_inputs            = 0;
    cpu_present_task_create_info.n_unique_outputs           = sizeof(cpu_present_task_outputs) / sizeof(cpu_present_task_outputs[0]);
    cpu_present_task_create_info.pfn_cpu_task_callback_proc = _ui_texture_preview_cpu_task_callback;
    cpu_present_task_create_info.unique_inputs              = nullptr;
    cpu_present_task_create_info.unique_outputs             = cpu_present_task_outputs;

    cpu_present_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Texture preview: UB update"),
                                                  &cpu_present_task_create_info);


    draw_present_task_inputs[0].buffer       = ub_fs_bo;
    draw_present_task_inputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[1].buffer       = ub_vs_bo;
    draw_present_task_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_inputs[2].texture_view = target_texture_view;

    draw_present_task_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_output.texture_view = target_texture_view;

    draw_present_task_create_info.command_buffer   = texture_preview_ptr->last_cached_command_buffer;
    draw_present_task_create_info.n_unique_inputs  = sizeof(draw_present_task_inputs) / sizeof(draw_present_task_inputs[0]);
    draw_present_task_create_info.n_unique_outputs = 1;
    draw_present_task_create_info.unique_inputs    = draw_present_task_inputs;
    draw_present_task_create_info.unique_outputs   = &draw_present_task_output;

    draw_present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("UI texture preview: rasterization"),
                                                   &draw_present_task_create_info);


    present_tasks[0] = cpu_present_task;
    present_tasks[1] = draw_present_task;

    result_present_task_ingroup_connections[0].input_present_task_index     = 1;
    result_present_task_ingroup_connections[0].input_present_task_io_index  = 0;
    result_present_task_ingroup_connections[0].output_present_task_index    = 0;
    result_present_task_ingroup_connections[0].output_present_task_io_index = 0;

    result_present_task_ingroup_connections[1].input_present_task_index     = 1;
    result_present_task_ingroup_connections[1].input_present_task_io_index  = 1;
    result_present_task_ingroup_connections[1].output_present_task_index    = 0;
    result_present_task_ingroup_connections[1].output_present_task_io_index = 1;

    result_present_task_input_mapping.group_task_io_index   = 0;
    result_present_task_input_mapping.n_present_task        = 1;
    result_present_task_input_mapping.present_task_io_index = 2; /* target_texture_view */

    result_present_task_output_mapping.group_task_io_index   = 0;
    result_present_task_output_mapping.present_task_io_index = 0; /* target_texture_view */
    result_present_task_output_mapping.n_present_task        = 1;

    result_present_task_create_info.ingroup_connections                      = result_present_task_ingroup_connections;
    result_present_task_create_info.n_ingroup_connections                    = sizeof(result_present_task_ingroup_connections) / sizeof(result_present_task_ingroup_connections[0]);
    result_present_task_create_info.n_present_tasks                          = sizeof(present_tasks)                           / sizeof(present_tasks                          [0]);
    result_present_task_create_info.n_total_unique_inputs                    = 1;
    result_present_task_create_info.n_total_unique_outputs                   = 1;
    result_present_task_create_info.n_unique_input_to_ingroup_task_mappings  = 1;
    result_present_task_create_info.n_unique_output_to_ingroup_task_mappings = 1;
    result_present_task_create_info.present_tasks                            = present_tasks;
    result_present_task_create_info.unique_input_to_ingroup_task_mapping     = &result_present_task_input_mapping;
    result_present_task_create_info.unique_output_to_ingroup_task_mapping    = &result_present_task_output_mapping;

    texture_preview_ptr->last_cached_present_task         = ral_present_task_create_group(&result_present_task_create_info);
    texture_preview_ptr->last_cached_sampled_texture_view = texture_preview_ptr->texture_view;
    texture_preview_ptr->last_cached_target_texture_view  = target_texture_view;

    ral_present_task_release(cpu_present_task);
    ral_present_task_release(draw_present_task);

end:
    ral_present_task_retain(texture_preview_ptr->last_cached_present_task);

    return texture_preview_ptr->last_cached_present_task;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_get_property(const void*         texture_preview,
                                            ui_control_property property,
                                            void*               out_result_ptr)
{
    _ui_texture_preview* texture_preview_ptr = (_ui_texture_preview*) texture_preview;

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = texture_preview_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR:
        {
            memcpy(out_result_ptr,
                   texture_preview_ptr->blend_color,
                   sizeof(texture_preview_ptr->blend_color) );

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_DST_ALPHA:
        {
            *reinterpret_cast<ral_blend_factor*>(out_result_ptr) = texture_preview_ptr->blend_factor_dst_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_DST_RGB:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = texture_preview_ptr->blend_factor_dst_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_SRC_ALPHA:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = texture_preview_ptr->blend_factor_src_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_SRC_RGB:
        {
            *reinterpret_cast<GLenum*>(out_result_ptr) = texture_preview_ptr->blend_factor_src_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_OP_ALPHA:
        {
            *reinterpret_cast<ral_blend_op*>(out_result_ptr) = texture_preview_ptr->blend_op_alpha;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_OP_RGB:
        {
            *reinterpret_cast<ral_blend_op*>(out_result_ptr) = texture_preview_ptr->blend_op_rgb;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = texture_preview_ptr->is_blending_enabled;

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_SHOW_TEXTURE_NAME:
        {
            varia_text_renderer_get_text_string_property(texture_preview_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                         texture_preview_ptr->text_index,
                                                         out_result_ptr);

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_VIEW_RAL:
        {
            *reinterpret_cast<ral_texture_view*>(out_result_ptr) = texture_preview_ptr->texture_view;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC void* ui_texture_preview_init(ui                        instance,
                                     varia_text_renderer       text_renderer,
                                     system_hashed_ansi_string name,
                                     const float*              x1y1,
                                     const float*              max_size,
                                     ral_texture_view          texture_view,
                                     ui_texture_preview_type   preview_type)
{
    _ui_texture_preview* new_texture_preview_ptr = new (std::nothrow) _ui_texture_preview;

    ASSERT_ALWAYS_SYNC(new_texture_preview_ptr != nullptr,
                       "Out of memory");

    if (new_texture_preview_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_texture_preview_ptr,
               0,
               sizeof(_ui_texture_preview) );
        memcpy(new_texture_preview_ptr->max_size,
               max_size,
               sizeof(float) * 2);

        new_texture_preview_ptr->x1y1x2y2[0] =     x1y1[0];
        new_texture_preview_ptr->x1y1x2y2[1] = 1 - x1y1[1];

        ui_get_property(instance,
                        UI_PROPERTY_CONTEXT,
                       &new_texture_preview_ptr->context);

        new_texture_preview_ptr->layer_shown              = 1;
        new_texture_preview_ptr->name                     = name;
        new_texture_preview_ptr->preview_type             = preview_type;
        new_texture_preview_ptr->text_renderer            = text_renderer;
        new_texture_preview_ptr->text_index               = varia_text_renderer_add_string(text_renderer);
        new_texture_preview_ptr->texture_view             = texture_view;
        new_texture_preview_ptr->texture_view_initialized = false;
        new_texture_preview_ptr->visible                  = true;

        /* Set default blending states */
        memset(new_texture_preview_ptr->blend_color,
               0,
               sizeof(new_texture_preview_ptr->blend_color) );

        new_texture_preview_ptr->blend_factor_dst_alpha = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        new_texture_preview_ptr->blend_factor_dst_rgb   = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        new_texture_preview_ptr->blend_factor_src_alpha = RAL_BLEND_FACTOR_SRC_ALPHA;
        new_texture_preview_ptr->blend_factor_src_rgb   = RAL_BLEND_FACTOR_SRC_ALPHA;
        new_texture_preview_ptr->blend_op_alpha         = RAL_BLEND_OP_ADD;
        new_texture_preview_ptr->blend_op_rgb           = RAL_BLEND_OP_ADD;
        new_texture_preview_ptr->is_blending_enabled    = false;

        /* Set up the sampler */
        ral_sampler_create_info sampler_create_info;

        new_texture_preview_ptr->sampler = ral_sampler_create(new_texture_preview_ptr->context,
                                                              name,
                                                             &sampler_create_info);

        /* Retrieve the rendering program */
        new_texture_preview_ptr->program = ui_get_registered_program(instance,
                                                                     system_hashed_ansi_string_create(_ui_texture_preview_get_program_name(preview_type)) );

        if (new_texture_preview_ptr->program == nullptr)
        {
            _ui_texture_preview_init_program(instance,
                                             new_texture_preview_ptr);

            ASSERT_DEBUG_SYNC(new_texture_preview_ptr->program != nullptr,
                              "Could not initialize texture preview UI program");
        }

        /* Set up rendering program. This must not be called if the specified texture view
         * is NULL.*/
        if (texture_view != nullptr)
        {
            _ui_texture_preview_init_texture(new_texture_preview_ptr);
        }
    }

    return (void*) new_texture_preview_ptr;
}

/** Please see header for specification */
PUBLIC void ui_texture_preview_set_property(void*               texture_preview,
                                            ui_control_property property,
                                            const void*         data)
{
    _ui_texture_preview* texture_preview_ptr = reinterpret_cast<_ui_texture_preview*>(texture_preview);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_COLOR:
        {
            if (memcmp(texture_preview_ptr->blend_color,
                       data,
                       sizeof(texture_preview_ptr->blend_color) ) != 0)
            {
                memcpy(texture_preview_ptr->blend_color,
                       data,
                       sizeof(texture_preview_ptr->blend_color) );

                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_DST_ALPHA:
        {
            const ral_blend_factor in_factor = *reinterpret_cast<const ral_blend_factor*>(data);

            if (texture_preview_ptr->blend_factor_dst_alpha != in_factor)
            {
                texture_preview_ptr->blend_factor_dst_alpha      = in_factor;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_DST_RGB:
        {
            const ral_blend_factor in_factor = *reinterpret_cast<const ral_blend_factor*>(data);

            if (texture_preview_ptr->blend_factor_dst_rgb != in_factor)
            {
                texture_preview_ptr->blend_factor_dst_rgb        = in_factor;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_SRC_ALPHA:
        {
            const ral_blend_factor in_factor = *reinterpret_cast<const ral_blend_factor*>(data);

            if (texture_preview_ptr->blend_factor_src_alpha != in_factor)
            {
                texture_preview_ptr->blend_factor_src_alpha      = in_factor;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_FACTOR_SRC_RGB:
        {
            const ral_blend_factor in_factor = *reinterpret_cast<const ral_blend_factor*>(data);

            if (texture_preview_ptr->blend_factor_src_rgb != in_factor)
            {
                texture_preview_ptr->blend_factor_src_rgb        = in_factor;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_OP_ALPHA:
        {
            const ral_blend_op in_op = *reinterpret_cast<const ral_blend_op*>(data);

            if (texture_preview_ptr->blend_op_alpha != in_op)
            {
                texture_preview_ptr->blend_op_alpha              = in_op;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_BLEND_OP_RGB:
        {
            const ral_blend_op in_op = *reinterpret_cast<const ral_blend_op*>(data);

            if (texture_preview_ptr->blend_op_rgb != in_op)
            {
                texture_preview_ptr->blend_op_rgb                = in_op;
                texture_preview_ptr->last_cached_gfx_state_dirty = true;
            }

            break;
        }


        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_IS_BLENDING_ENABLED:
        {
            texture_preview_ptr->is_blending_enabled = *reinterpret_cast<const bool*>(data);

            break;
        }

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_LAYER_SHOWN:
        {
            texture_preview_ptr->layer_shown = *reinterpret_cast<const GLuint*>(data);

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

        case UI_CONTROL_PROPERTY_TEXTURE_PREVIEW_TEXTURE_VIEW_RAL:
        {
            texture_preview_ptr->texture_view             = *reinterpret_cast<const ral_texture_view*>(data);
            texture_preview_ptr->texture_view_initialized = false;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value requested");
        }
    }
}