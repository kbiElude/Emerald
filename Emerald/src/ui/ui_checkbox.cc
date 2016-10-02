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
#include "ral/ral_shader.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_checkbox.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"

#include <algorithm>

#ifdef _WIN32
    #undef max
#endif

#define BORDER_WIDTH_PX                       (1)
#define CHECK_BRIGHTNESS_MODIFIER             (3.0f) /* multiplied */
#define CLICK_BRIGHTNESS_MODIFIER             (1.5f) /* multiplied */
#define CHECKBOX_WIDTH_PX                     (20) /* also modify the vertex shader if you change this field */
#define FOCUSED_BRIGHTNESS                    (0.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (0.1f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(450) )
#define TEXT_DELTA_PX                         (4)

const float _ui_checkbox_text_color[] = {1, 1, 1, 1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_checkbox_program_name = system_hashed_ansi_string_create("UI Checkbox");

/** Internal types */
typedef struct
{
    float base_x1y1[2]; /* bottom-left position */
    float x1y1x2y2[4];

    void*            fire_proc_user_arg;
    PFNUIFIREPROCPTR pfn_fire_proc_ptr;

    float       current_gpu_brightness_level;
    bool        force_gpu_brightness_update;
    bool        is_hovering;
    bool        is_lbm_on;
    float       start_hovering_brightness;
    system_time start_hovering_time;
    bool        status;
    bool        visible;

    ral_command_buffer cached_command_buffer;
    ral_gfx_state      cached_gfx_state;
    ral_present_task   cached_present_task;
    ral_texture_view   cached_present_task_texture_view; /* do NOT release */

    ral_context               context;
    system_hashed_ansi_string name;
    ral_program               program;
    uint32_t                  program_border_width_ub_offset;
    uint32_t                  program_brightness_ub_offset;
    uint32_t                  program_text_brightness_ub_offset;
    ral_program_block_buffer  program_ub_fs;
    uint32_t                  program_ub_fs_bo_size;
    ral_program_block_buffer  program_ub_vs;
    uint32_t                  program_ub_vs_bo_size;
    uint32_t                  program_x1y1x2y2_ub_offset;

    uint32_t            text_index;
    varia_text_renderer text_renderer;
    ui                  ui_instance; /* do NOT release */

} _ui_checkbox;

/** Internal variables */
static const char* ui_checkbox_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "in  vec2 uv;\n"
    "out vec3 result;\n"
    /* stop, rgb */
    "uniform dataFS\n"
    "{\n"
    "    float brightness;\n"
    "    vec2  border_width;\n"
    "    vec4  stop_data[4];\n"
    "    float text_brightness;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    if (uv.x >= border_width[0] * 21.0f)\n"
    "    {\n"
    "        result = vec3(text_brightness);\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        result = vec3(brightness);\n"
    "    }\n"
    /* Border? */
    "if (uv.x <  border_width[0] || uv.x > (1.0 - border_width[0]) ||\n"
    "    uv.y <  border_width[1] || uv.y > (1.0 - border_width[1]) ||\n"
    "    uv.x >= border_width[0] * 20.0f && uv.x <= border_width[0] * 21.0f) result = vec3(0.42f, 0.41f, 0.41f);\n"
    "}\n";


/* Forward declarations */
PRIVATE void _ui_checkbox_init                 (_ui_checkbox* checkbox_ptr);
PRIVATE void _ui_checkbox_init_program         (ui            ui_instance,
                                                _ui_checkbox* checkbox_ptr);
PRIVATE void _ui_checkbox_update_props_cpu_task(void*         checkbox_raw_ptr);
PRIVATE void _ui_checkbox_update_text_location (_ui_checkbox* checkbox_ptr);
PRIVATE void _ui_checkbox_update_x1y1x2y2      (_ui_checkbox* checkbox_ptr);


/** TODO */
PRIVATE void _ui_checkbox_init(_ui_checkbox* checkbox_ptr)
{
    float         border_width[2] = {0};
    system_window window          = nullptr;
    int           window_size[2]  = {0};

    ral_context_get_property  (checkbox_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    border_width[0] = 1.0f / (float)((checkbox_ptr->x1y1x2y2[2] - checkbox_ptr->x1y1x2y2[0]) * window_size[0]);
    border_width[1] = 1.0f / (float)((checkbox_ptr->x1y1x2y2[3] - checkbox_ptr->x1y1x2y2[1]) * window_size[1]);

    /* Retrieve uniform locations */
    const ral_program_variable* border_width_uniform_ral_ptr    = nullptr;
    const ral_program_variable* brightness_uniform_ral_ptr      = nullptr;
    const ral_program_variable* text_brightness_uniform_ral_ptr = nullptr;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr        = nullptr;

    ral_program_get_block_variable_by_name(checkbox_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(checkbox_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("brightness"),
                                          &brightness_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(checkbox_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("text_brightness"),
                                          &text_brightness_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(checkbox_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_uniform_ral_ptr);

    checkbox_ptr->program_border_width_ub_offset    = border_width_uniform_ral_ptr->block_offset;
    checkbox_ptr->program_brightness_ub_offset      = brightness_uniform_ral_ptr->block_offset;
    checkbox_ptr->program_text_brightness_ub_offset = text_brightness_uniform_ral_ptr->block_offset;
    checkbox_ptr->program_x1y1x2y2_ub_offset        = x1y1x2y2_uniform_ral_ptr->block_offset;

    /* Set up uniform blocks */
    ral_buffer ub_fs_bo_ral = nullptr;
    ral_buffer ub_vs_bo_ral = nullptr;

    checkbox_ptr->program_ub_fs = ral_program_block_buffer_create(checkbox_ptr->context,
                                                                  checkbox_ptr->program,
                                                                  system_hashed_ansi_string_create("dataFS") );
    checkbox_ptr->program_ub_vs = ral_program_block_buffer_create(checkbox_ptr->context,
                                                                  checkbox_ptr->program,
                                                                  system_hashed_ansi_string_create("dataVS") );

    ral_program_block_buffer_get_property(checkbox_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_fs_bo_ral);
    ral_program_block_buffer_get_property(checkbox_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_vs_bo_ral);

    ral_buffer_get_property(ub_fs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &checkbox_ptr->program_ub_fs_bo_size);
    ral_buffer_get_property(ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &checkbox_ptr->program_ub_vs_bo_size);

    /* Set them up */
    const float default_brightness = NONFOCUSED_BRIGHTNESS;

    ral_program_block_buffer_set_nonarrayed_variable_value(checkbox_ptr->program_ub_fs,
                                                           border_width_uniform_ral_ptr->block_offset,
                                                          &border_width,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(checkbox_ptr->program_ub_fs,
                                                           text_brightness_uniform_ral_ptr->block_offset,
                                                          &default_brightness,
                                                           sizeof(float) );

    checkbox_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;

    /* Configure the text to be shown on the button */
    varia_text_renderer_set(checkbox_ptr->text_renderer,
                            checkbox_ptr->text_index,
                            system_hashed_ansi_string_get_buffer(checkbox_ptr->name) );

    _ui_checkbox_update_x1y1x2y2     (checkbox_ptr);
    _ui_checkbox_update_text_location(checkbox_ptr);

    varia_text_renderer_set_text_string_property(checkbox_ptr->text_renderer,
                                                 checkbox_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 _ui_checkbox_text_color);

    /* Retrieve the rendering program */
    checkbox_ptr->program = ui_get_registered_program(checkbox_ptr->ui_instance,
                                                      ui_checkbox_program_name);

    if (checkbox_ptr->program == nullptr)
    {
        _ui_checkbox_init_program(checkbox_ptr->ui_instance,
                                  checkbox_ptr);

        ASSERT_DEBUG_SYNC(checkbox_ptr->program != nullptr,
                          "Could not initialize checkbox UI program");
    }
}

/** TODO */
PRIVATE void _ui_checkbox_init_program(ui            ui_instance,
                                       _ui_checkbox* checkbox_ptr)
{
    /* Create all objects */
    ral_shader  fs      = nullptr;
    ral_shader  vs      = nullptr;

    ral_shader_create_info  fs_create_info;
    ral_program_create_info program_create_info;
    ral_shader_create_info  vs_create_info;

    fs_create_info.name   = system_hashed_ansi_string_create("UI checkbox fragment shader");
    fs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    fs_create_info.type   = RAL_SHADER_TYPE_FRAGMENT;

    program_create_info.active_shader_stages = RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX;
    program_create_info.name                 = system_hashed_ansi_string_create("UI checkbox program");

    vs_create_info.name   = system_hashed_ansi_string_create("UI checkbox vertex shader");
    vs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    vs_create_info.type   = RAL_SHADER_TYPE_VERTEX;

    ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader     result_shaders[n_shader_create_info_items];

    if (!ral_context_create_shaders(checkbox_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    if (!ral_context_create_programs(checkbox_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &checkbox_ptr->program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    /* Set up shaders */
    const system_hashed_ansi_string fs_body = system_hashed_ansi_string_create(ui_checkbox_fragment_shader_body);
    const system_hashed_ansi_string vs_body = system_hashed_ansi_string_create(ui_general_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Set up program object */
    if (!ral_program_attach_shader(checkbox_ptr->program,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(checkbox_ptr->program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following button instances will reuse the program */
    ui_register_program(ui_instance,
                        ui_checkbox_program_name,
                        checkbox_ptr->program);

    /* Release shaders we will no longer need */
    ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(checkbox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               reinterpret_cast<void* const*>(shaders_to_release) );
}

/** TODO */
PRIVATE void _ui_checkbox_update_props_cpu_task(void* checkbox_raw_ptr)
{
    _ui_checkbox* checkbox_ptr = reinterpret_cast<_ui_checkbox*>(checkbox_raw_ptr);
    system_time   time_now     = system_time_now();

    /* Update brightness if necessary */
    float brightness = checkbox_ptr->current_gpu_brightness_level;

    if (checkbox_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_time transition_start = checkbox_ptr->start_hovering_time;
        system_time transition_end   = checkbox_ptr->start_hovering_time +
                                       NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) /
                       float(NONFOCUSED_TO_FOCUSED_TRANSITION_TIME);

            brightness = checkbox_ptr->start_hovering_brightness +
                         dt * (FOCUSED_BRIGHTNESS - NONFOCUSED_BRIGHTNESS);

            /* Clamp from above */
            if (brightness > FOCUSED_BRIGHTNESS)
            {
                brightness = FOCUSED_BRIGHTNESS;
            }
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = FOCUSED_BRIGHTNESS;
        }
    }
    else
    {
        /* Are we transiting? */
        system_time transition_start = checkbox_ptr->start_hovering_time;
        system_time transition_end   = checkbox_ptr->start_hovering_time +
                                       FOCUSED_TO_NONFOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) /
                       float(FOCUSED_TO_NONFOCUSED_TRANSITION_TIME);

            brightness = checkbox_ptr->start_hovering_brightness +
                         dt * (NONFOCUSED_BRIGHTNESS - FOCUSED_BRIGHTNESS);

            /* Clamp from below */
            if (brightness < NONFOCUSED_BRIGHTNESS)
            {
                brightness = NONFOCUSED_BRIGHTNESS;
            }
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = NONFOCUSED_BRIGHTNESS;
        }
    }

    /* Update uniforms */
    const float new_brightness = brightness * (checkbox_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER 
                                                                      : 1) * (checkbox_ptr->status ? CHECK_BRIGHTNESS_MODIFIER
                                                                                                   : 1);

    if (checkbox_ptr->current_gpu_brightness_level != brightness ||
        checkbox_ptr->force_gpu_brightness_update)
    {
        checkbox_ptr->current_gpu_brightness_level = brightness;
        checkbox_ptr->force_gpu_brightness_update  = false;
    }

    ral_program_block_buffer_set_nonarrayed_variable_value(checkbox_ptr->program_ub_fs,
                                                           checkbox_ptr->program_brightness_ub_offset,
                                                          &new_brightness,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(checkbox_ptr->program_ub_vs,
                                                           checkbox_ptr->program_x1y1x2y2_ub_offset,
                                                           checkbox_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);

    ral_program_block_buffer_sync_immediately(checkbox_ptr->program_ub_fs);
    ral_program_block_buffer_sync_immediately(checkbox_ptr->program_ub_vs);
}

/** TODO */
PRIVATE void _ui_checkbox_update_text_location(_ui_checkbox* checkbox_ptr)
{
    int           text_height    = 0;
    int           text_xy[2]     = {0};
    int           text_width     = 0;
    system_window window         = nullptr;
    int           window_size[2] = {0};
    const float   x1y1[]         =
    {
        checkbox_ptr->x1y1x2y2[0],
        1.0f - checkbox_ptr->x1y1x2y2[1]
    };
    const float   x2y2[]         =
    {
        checkbox_ptr->x1y1x2y2[2],
        1.0f - checkbox_ptr->x1y1x2y2[3]
    };

    varia_text_renderer_get_text_string_property(checkbox_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 checkbox_ptr->text_index,
                                                &text_height);
    varia_text_renderer_get_text_string_property(checkbox_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 checkbox_ptr->text_index,
                                                &text_width);

    ral_context_get_property(checkbox_ptr->context,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                            &window);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    text_xy[0] = (int) ((x1y1[0] + (x2y2[0] + float(CHECKBOX_WIDTH_PX) / window_size[0] - x1y1[0] - float(text_width)  / window_size[0]) * 0.5f) * (float) window_size[0]);
    text_xy[1] = (int) ((x2y2[1] - (x2y2[1] +                                           - x1y1[1] - float(text_height) / window_size[1]) * 0.5f) * (float) window_size[1]);

    varia_text_renderer_set_text_string_property(checkbox_ptr->text_renderer,
                                                 checkbox_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                 text_xy);
}

/** TODO */
PRIVATE void _ui_checkbox_update_x1y1x2y2(_ui_checkbox* checkbox_ptr)
{
    int           text_height    = 0;
    int           text_width     = 0;
    system_window window;
    int           window_size[2] = {0};

    varia_text_renderer_get_text_string_property(checkbox_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 checkbox_ptr->text_index,
                                                &text_height);
    varia_text_renderer_get_text_string_property(checkbox_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 checkbox_ptr->text_index,
                                                &text_width);

    ral_context_get_property   (checkbox_ptr->context,
                                RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                               &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    checkbox_ptr->x1y1x2y2[0] = checkbox_ptr->base_x1y1[0];
    checkbox_ptr->x1y1x2y2[1] = checkbox_ptr->base_x1y1[1];
    checkbox_ptr->x1y1x2y2[2] = checkbox_ptr->base_x1y1[0] + float(text_width  + 2 * BORDER_WIDTH_PX + CHECKBOX_WIDTH_PX + TEXT_DELTA_PX) / float(window_size[0]);
    checkbox_ptr->x1y1x2y2[3] = checkbox_ptr->base_x1y1[1] + float(std::max(text_height, CHECKBOX_WIDTH_PX))                               / float(window_size[1]);
}


/** Please see header for specification */
PUBLIC void ui_checkbox_deinit(void* internal_instance)
{
    _ui_checkbox* ui_checkbox_ptr = reinterpret_cast<_ui_checkbox*>(internal_instance);

    varia_text_renderer_set(ui_checkbox_ptr->text_renderer,
                            ui_checkbox_ptr->text_index,
                            "");

    ral_context_delete_objects(ui_checkbox_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&ui_checkbox_ptr->program) );

    if (ui_checkbox_ptr->cached_command_buffer != nullptr)
    {
        ral_command_buffer_release(ui_checkbox_ptr->cached_command_buffer);

        ui_checkbox_ptr->cached_command_buffer = nullptr;
    }

    if (ui_checkbox_ptr->cached_gfx_state != nullptr)
    {
        ral_context_delete_objects(ui_checkbox_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&ui_checkbox_ptr->cached_gfx_state) );

        ui_checkbox_ptr->cached_gfx_state = nullptr;
    }

    if (ui_checkbox_ptr->cached_present_task != nullptr)
    {
        ral_present_task_release(ui_checkbox_ptr->cached_present_task);

        ui_checkbox_ptr->cached_present_task = nullptr;
    }

    if (ui_checkbox_ptr->program_ub_fs != nullptr)
    {
        ral_program_block_buffer_release(ui_checkbox_ptr->program_ub_fs);

        ui_checkbox_ptr->program_ub_fs = nullptr;
    }

    if (ui_checkbox_ptr->program_ub_vs != nullptr)
    {
        ral_program_block_buffer_release(ui_checkbox_ptr->program_ub_vs);

        ui_checkbox_ptr->program_ub_vs = nullptr;
    }

    delete ui_checkbox_ptr;
}

/** Please see header for specification */
PUBLIC ral_present_task ui_checkbox_get_present_task(void*            internal_instance,
                                                     ral_texture_view target_texture_view)
{
    _ui_checkbox*    checkbox_ptr = reinterpret_cast<_ui_checkbox*>(internal_instance);
    ral_present_task result       = nullptr;
    uint32_t         target_texture_view_size[2];
    ral_buffer       ub_fs_bo;
    ral_buffer       ub_vs_bo;

    /* Can we re-use the last present task? */
    if (checkbox_ptr->cached_present_task              != nullptr             &&
        checkbox_ptr->cached_present_task_texture_view == target_texture_view)
    {
        result = checkbox_ptr->cached_present_task;

        goto end;
    }

    /* Can we re-use the gfx state instance? */
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                         target_texture_view_size + 0);
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                         target_texture_view_size + 1);

    if (checkbox_ptr->cached_gfx_state != nullptr)
    {
        const ral_command_buffer_set_viewport_command_info* cached_static_viewports = nullptr;

        ral_gfx_state_get_property(checkbox_ptr->cached_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &cached_static_viewports);

        if (!(fabs(cached_static_viewports[0].size[0] - target_texture_view_size[0]) < 1e-5f &&
              fabs(cached_static_viewports[0].size[1] - target_texture_view_size[1]) < 1e-5f) )
        {
            ral_context_delete_objects(checkbox_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&checkbox_ptr->cached_gfx_state) );

            checkbox_ptr->cached_gfx_state = nullptr;
        }
    }

    if (checkbox_ptr->cached_gfx_state == nullptr)
    {
        /* Cache a gfx state needed to render the checkbox */
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_scissor_box_command_info scissor_box;
        ral_command_buffer_set_viewport_command_info    viewport;

        scissor_box.index   = 0;
        scissor_box.size[0] = target_texture_view_size[0];
        scissor_box.size[1] = target_texture_view_size[1];
        scissor_box.xy  [0] = 0;
        scissor_box.xy  [1] = 0;

        viewport.depth_range[0] = 0.0f;
        viewport.depth_range[1] = 1.0f;
        viewport.index          = 0;
        viewport.size[0]        = static_cast<float>(target_texture_view_size[0]);
        viewport.size[1]        = static_cast<float>(target_texture_view_size[1]);
        viewport.xy  [0]        = 0;
        viewport.xy  [1]        = 0;

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &scissor_box;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &viewport;
        gfx_state_create_info.static_viewports_enabled             = true;

        ral_context_create_gfx_states(checkbox_ptr->context,
                                      1, /* n_create_info_items */
                                      &gfx_state_create_info,
                                      &checkbox_ptr->cached_gfx_state);
    }

    /* Record the command buffer */
    if (checkbox_ptr->cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info draw_command_buffer_create_info;

        draw_command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        draw_command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        draw_command_buffer_create_info.is_resettable                           = true;
        draw_command_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(checkbox_ptr->context,
                                           1, /* n_command_buffers */
                                           &draw_command_buffer_create_info,
                                           &checkbox_ptr->cached_command_buffer);
    }

    ral_command_buffer_start_recording(checkbox_ptr->cached_command_buffer);
    {
        ral_command_buffer_draw_call_regular_command_info draw_call_command_info;
        ral_command_buffer_set_binding_command_info       set_binding_command_info_items[2];

        ral_program_block_buffer_get_property(checkbox_ptr->program_ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_fs_bo);
        ral_program_block_buffer_get_property(checkbox_ptr->program_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_vs_bo);

        draw_call_command_info.base_instance = 0;
        draw_call_command_info.base_vertex   = 0;
        draw_call_command_info.n_instances   = 1;
        draw_call_command_info.n_vertices    = 4;

        set_binding_command_info_items[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        set_binding_command_info_items[0].name                          = system_hashed_ansi_string_create("dataFS");
        set_binding_command_info_items[0].uniform_buffer_binding.buffer = ub_fs_bo;
        set_binding_command_info_items[0].uniform_buffer_binding.offset = 0;
        set_binding_command_info_items[0].uniform_buffer_binding.size   = checkbox_ptr->program_ub_fs_bo_size;

        set_binding_command_info_items[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
        set_binding_command_info_items[1].name                          = system_hashed_ansi_string_create("dataVS");
        set_binding_command_info_items[1].uniform_buffer_binding.buffer = ub_vs_bo;
        set_binding_command_info_items[1].uniform_buffer_binding.offset = 0;
        set_binding_command_info_items[1].uniform_buffer_binding.size   = checkbox_ptr->program_ub_fs_bo_size;

        ral_command_buffer_record_set_bindings (checkbox_ptr->cached_command_buffer,
                                                sizeof(set_binding_command_info_items) / sizeof(set_binding_command_info_items[0]),
                                                set_binding_command_info_items);
        ral_command_buffer_record_set_gfx_state(checkbox_ptr->cached_command_buffer,
                                                checkbox_ptr->cached_gfx_state);
        ral_command_buffer_record_set_program  (checkbox_ptr->cached_command_buffer,
                                                checkbox_ptr->program);

        ral_command_buffer_record_draw_call_regular(checkbox_ptr->cached_command_buffer,
                                                    1, /* n_draw_calls */
                                                   &draw_call_command_info);
    }
    ral_command_buffer_stop_recording(checkbox_ptr->cached_command_buffer);

    /* Bake the result present task */
    ral_present_task                    present_task_cpu;
    ral_present_task_cpu_create_info    present_task_cpu_create_info;
    ral_present_task_io                 present_task_cpu_unique_outputs[2];
    ral_present_task                    present_task_gpu;
    ral_present_task_gpu_create_info    present_task_gpu_create_info;
    ral_present_task_io                 present_task_gpu_unique_inputs[3];
    ral_present_task_io                 present_task_gpu_unique_output;
    ral_present_task_group_create_info  present_task_group_create_info;
    ral_present_task                    present_task_group_tasks[2];
    ral_present_task_ingroup_connection present_task_ingroup_connections[2];
    ral_present_task_group_mapping      present_task_unique_input_mapping;
    ral_present_task_group_mapping      present_task_unique_output_mapping;

    present_task_cpu_unique_outputs[0].buffer      = ub_fs_bo;
    present_task_cpu_unique_outputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    present_task_cpu_unique_outputs[1].buffer      = ub_vs_bo;
    present_task_cpu_unique_outputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    present_task_cpu_create_info.cpu_task_callback_user_arg = checkbox_ptr;
    present_task_cpu_create_info.n_unique_inputs            = 0;
    present_task_cpu_create_info.n_unique_outputs           = sizeof(present_task_cpu_unique_outputs) / sizeof(present_task_cpu_unique_outputs[0]);
    present_task_cpu_create_info.pfn_cpu_task_callback_proc = _ui_checkbox_update_props_cpu_task;
    present_task_cpu_create_info.unique_inputs              = 0;
    present_task_cpu_create_info.unique_outputs             = present_task_cpu_unique_outputs;

    present_task_cpu = ral_present_task_create_cpu(system_hashed_ansi_string_create("Checkbox UI control: UB update"),
                                                   &present_task_cpu_create_info);


    present_task_gpu_unique_inputs[0].buffer      = ub_fs_bo;
    present_task_gpu_unique_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    present_task_gpu_unique_inputs[1].buffer      = ub_vs_bo;
    present_task_gpu_unique_inputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    present_task_gpu_unique_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    present_task_gpu_unique_inputs[2].texture_view = target_texture_view;

    present_task_gpu_unique_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    present_task_gpu_unique_output.texture_view = target_texture_view;

    present_task_gpu_create_info.command_buffer   = checkbox_ptr->cached_command_buffer;
    present_task_gpu_create_info.n_unique_inputs  = sizeof(present_task_gpu_unique_inputs) / sizeof(present_task_gpu_unique_inputs[0]);
    present_task_gpu_create_info.n_unique_outputs = 1;
    present_task_gpu_create_info.unique_inputs    = present_task_gpu_unique_inputs;
    present_task_gpu_create_info.unique_outputs   = &present_task_gpu_unique_output;

    present_task_gpu = ral_present_task_create_gpu(system_hashed_ansi_string_create("Checkbox UI control: Rasterization"),
                                                  &present_task_gpu_create_info);


    present_task_group_tasks[0] = present_task_cpu;
    present_task_group_tasks[1] = present_task_gpu;

    present_task_ingroup_connections[0].input_present_task_index     = 1; /* ub_fs */
    present_task_ingroup_connections[0].input_present_task_io_index  = 0;
    present_task_ingroup_connections[0].output_present_task_index    = 1; /* ub_fs */
    present_task_ingroup_connections[0].output_present_task_io_index = 0;

    present_task_ingroup_connections[1].input_present_task_index     = 1; /* ub_vs */
    present_task_ingroup_connections[1].input_present_task_io_index  = 1;
    present_task_ingroup_connections[1].output_present_task_index    = 1; /* ub_vs */
    present_task_ingroup_connections[1].output_present_task_io_index = 1;

    present_task_unique_input_mapping.group_task_io_index   = 0;
    present_task_unique_input_mapping.present_task_io_index = 2; /* texture_view */
    present_task_unique_input_mapping.n_present_task        = 1;

    present_task_unique_output_mapping.group_task_io_index   = 0;
    present_task_unique_output_mapping.present_task_io_index = 0; /* texture_view */
    present_task_unique_output_mapping.n_present_task        = 1;

    present_task_group_create_info.ingroup_connections                      = present_task_ingroup_connections;
    present_task_group_create_info.n_ingroup_connections                    = sizeof(present_task_ingroup_connections) / sizeof(present_task_ingroup_connections[0]);
    present_task_group_create_info.n_present_tasks                          = sizeof(present_task_group_tasks)         / sizeof(present_task_group_tasks        [0]);
    present_task_group_create_info.n_total_unique_inputs                    = 1;
    present_task_group_create_info.n_total_unique_outputs                   = 1;
    present_task_group_create_info.n_unique_input_to_ingroup_task_mappings  = 1;
    present_task_group_create_info.n_unique_output_to_ingroup_task_mappings = 1;
    present_task_group_create_info.present_tasks                            = present_task_group_tasks;
    present_task_group_create_info.unique_input_to_ingroup_task_mapping     = &present_task_unique_input_mapping;
    present_task_group_create_info.unique_output_to_ingroup_task_mapping    = &present_task_unique_output_mapping;

    checkbox_ptr->cached_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("UI check-box: rasterization"),
                                                                      &present_task_group_create_info);
    result                            = checkbox_ptr->cached_present_task;

    ral_present_task_release(present_task_cpu);
    ral_present_task_release(present_task_gpu);

end:
    ral_present_task_retain(result);

    return result;
}

/** Please see header for specification */
PUBLIC void ui_checkbox_get_property(const void*         checkbox,
                                     ui_control_property property,
                                     void*               out_result_ptr)
{
    const _ui_checkbox* checkbox_ptr = reinterpret_cast<const _ui_checkbox*>(checkbox);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS:
        {
            *(bool*) out_result_ptr = checkbox_ptr->status;

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result_ptr = checkbox_ptr->visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property property");
        }
    }
}

/** Please see header for specification */
PUBLIC void* ui_checkbox_init(ui                        instance,
                              varia_text_renderer       text_renderer,
                              system_hashed_ansi_string name,
                              const float*              x1y1,
                              PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                              void*                     fire_proc_user_arg,
                              bool                      default_status)
{
    _ui_checkbox* new_checkbox_ptr = new (std::nothrow) _ui_checkbox;

    ASSERT_ALWAYS_SYNC(new_checkbox_ptr != nullptr,
                       "Out of memory");

    if (new_checkbox_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_checkbox_ptr,
               0,
               sizeof(_ui_checkbox) );

        new_checkbox_ptr->base_x1y1[0] = x1y1[0];
        new_checkbox_ptr->base_x1y1[1] = x1y1[1];

        ui_get_property(instance,
                        UI_PROPERTY_CONTEXT,
                       &new_checkbox_ptr->context);

        new_checkbox_ptr->fire_proc_user_arg = fire_proc_user_arg;
        new_checkbox_ptr->name               = name;
        new_checkbox_ptr->pfn_fire_proc_ptr  = pfn_fire_proc_ptr;
        new_checkbox_ptr->status             = default_status;
        new_checkbox_ptr->text_renderer      = text_renderer;
        new_checkbox_ptr->text_index         = varia_text_renderer_add_string(text_renderer);
        new_checkbox_ptr->visible            = true;

        _ui_checkbox_init(new_checkbox_ptr);
    }

    return (void*) new_checkbox_ptr;
}

/** Please see header for specification */
PUBLIC bool ui_checkbox_is_over(void*        internal_instance,
                                const float* xy)
{
    _ui_checkbox* checkbox_ptr = reinterpret_cast<_ui_checkbox*>(internal_instance);
    float         inversed_y   = 1.0f - xy[1];

    if (xy[0]      >= checkbox_ptr->x1y1x2y2[0] && xy[0]      <= checkbox_ptr->x1y1x2y2[2] &&
        inversed_y >= checkbox_ptr->x1y1x2y2[1] && inversed_y <= checkbox_ptr->x1y1x2y2[3])
    {
        if (!checkbox_ptr->is_hovering)
        {
            checkbox_ptr->start_hovering_brightness = checkbox_ptr->current_gpu_brightness_level;
            checkbox_ptr->start_hovering_time       = system_time_now();
            checkbox_ptr->is_hovering               = true;
        }

        return true;
    }
    else
    {
        if (checkbox_ptr->is_hovering)
        {
            checkbox_ptr->is_hovering               = false;
            checkbox_ptr->start_hovering_brightness = checkbox_ptr->current_gpu_brightness_level;
            checkbox_ptr->start_hovering_time       = system_time_now();
        }

        return false;
    }
}

/** Please see header for specification */
PUBLIC void ui_checkbox_on_lbm_down(void*        internal_instance,
                                    const float* xy)
{
    _ui_checkbox* checkbox_ptr = reinterpret_cast<_ui_checkbox*>(internal_instance);
    float          inversed_y   = 1.0f - xy[1];

    if (xy[0]      >= checkbox_ptr->x1y1x2y2[0] && xy[0]      <= checkbox_ptr->x1y1x2y2[2] &&
        inversed_y >= checkbox_ptr->x1y1x2y2[1] && inversed_y <= checkbox_ptr->x1y1x2y2[3])
    {
        checkbox_ptr->is_lbm_on                   = true;
        checkbox_ptr->force_gpu_brightness_update = true;
    }
}

/** Please see header for specification */
PUBLIC void ui_checkbox_on_lbm_up(void*        internal_instance,
                                  const float* xy)
{
    _ui_checkbox* checkbox_ptr = reinterpret_cast<_ui_checkbox*>(internal_instance);

    checkbox_ptr->is_lbm_on = false;

    if (ui_checkbox_is_over(internal_instance,
                            xy) )
    {
        if (checkbox_ptr->pfn_fire_proc_ptr != nullptr)
        {
            system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                       (PFNSYSTEMTHREADPOOLCALLBACKPROC) checkbox_ptr->pfn_fire_proc_ptr,
                                                                                       checkbox_ptr->fire_proc_user_arg);

            system_thread_pool_submit_single_task(task);
        }

        checkbox_ptr->force_gpu_brightness_update = true;
        checkbox_ptr->status                      = !checkbox_ptr->status;
    }
}

/* Please see header for spec */
PUBLIC void ui_checkbox_set_property(void*               checkbox,
                                     ui_control_property property,
                                     const void*         data)
{
    _ui_checkbox* checkbox_ptr = reinterpret_cast<_ui_checkbox*>(checkbox);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_CHECKBOX_X1Y1:
        {
            memcpy(checkbox_ptr->base_x1y1,
                   data,
                   sizeof(float) * 2);

            checkbox_ptr->base_x1y1[1] = 1.0f - checkbox_ptr->base_x1y1[1];

            _ui_checkbox_update_x1y1x2y2     (checkbox_ptr);
            _ui_checkbox_update_text_location(checkbox_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}
