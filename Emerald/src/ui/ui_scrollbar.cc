/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_scrollbar.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"

/** Internal constants */
static system_hashed_ansi_string _ui_scrollbar_slider_program_name = system_hashed_ansi_string_create("UI Scrollbar Slider");
const  float                     _ui_scrollbar_text_color[]        = {1, 1, 1, 1.0f};

/** Internal definitions */
#define SLIDER_SIZE_PX               (16)
#define TEXT_SCROLLBAR_SEPARATION_PX (10)

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(450) )

/* Loosely based around concepts described in http://thndl.com/?5 */
static const char* ui_scrollbar_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "in  vec2  uv;\n"
    "out vec4  result;\n"
    "\n"
    "uniform dataFS\n"
    "{\n"
    "    vec2  border_width;\n"
    "    float brightness;\n"
    "    bool  is_handle;\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    vec2  abs_uv     = abs(uv * 2 - 1)  - .3;\n"
    "    vec2  max_abs_uv = smoothstep(.0, is_handle ? .5 : .7, max(abs_uv, 0.));\n"
    "    float r          = length(max_abs_uv);\n"
    "    \n"
    "    if (is_handle)\n"
    "    {\n"
    "        result = vec4(vec3(max(0.6 + 0.5 * uv.x, r) ), max(0.5, r) );\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        result = vec4(vec3(0.1), 1-r);\n"
    "    }\n"
    "    result *= brightness;\n"
    /* Border? */
    "    if (uv.x < border_width[0] || uv.x > (1.0 - border_width[0]) || uv.y < border_width[1] || uv.y > (1.0 - border_width[1]) )\n"
    "    {\n"
    "        result = vec4(0.42, 0.41, 0.41, 1.0);\n"
    "    }\n"
    "}";

/** Internal types */
typedef struct
{
    ral_context                context;
    system_hashed_ansi_string  name;

    ral_command_buffer last_cached_command_buffer;
    ral_gfx_state      last_cached_gfx_state;
    bool               last_cached_is_visible;
    ral_present_task   last_cached_present_task;
    ral_texture_view   last_cached_target_texture_view;

    uint32_t                   text_index;
    ui_scrollbar_text_location text_location;
    varia_text_renderer        text_renderer;
    ui                         ui_instance;

    ral_program_block_buffer program_handle_ub_fs;
    ral_program              program_slider;
    uint32_t                 program_slider_border_width_ub_offset;
    uint32_t                 program_slider_brightness_ub_offset;
    uint32_t                 program_slider_is_handle_ub_offset;
    uint32_t                 program_slider_x1y1x2y2_ub_offset;
    ral_program_block_buffer program_slider_ub_fs;
    uint32_t                 program_slider_ub_fs_bo_size;
    ral_program_block_buffer program_slider_ub_vs;
    uint32_t                 program_slider_ub_vs_bo_size;

    system_variant min_value_variant;
    system_variant max_value_variant;
    system_variant temp_variant;

    float       current_gpu_brightness_level;
    bool        force_gpu_brightness_update;
    bool        is_hovering;
    bool        is_lbm_on;
    bool        is_visible;
    float       start_hovering_brightness;
    system_time start_hovering_time;

    float gpu_slider_handle_position;
    float slider_handle_size    [2]; /* scaled to window size */
    float slider_handle_x1y1x2y2[4];

    float slider_border_width       [2];
    float slider_handle_border_width[2];
    float slider_x1y1x2y2           [4];

    PFNUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr;
    void*                       get_current_value_ptr_user_arg;
    PFNUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr;
    void*                       set_current_value_ptr_user_arg;
} _ui_scrollbar;

/* Forward declarations */
PRIVATE void _ui_scrollbar_update_slider_handle_position(_ui_scrollbar* scrollbar_ptr);


/** TODO */
PRIVATE void _ui_scrollbar_init_program(ui             ui_instance,
                                        _ui_scrollbar* scrollbar_ptr)
{
    /* Create all objects */
    ral_context context = ui_get_context(ui_instance);
    ral_shader  fs      = nullptr;
    ral_shader  vs      = nullptr;

    const ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create("UI scrollbar slider fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    const ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create("UI scrollbar slider vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };

    const ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI scrollbar program")
    };

    const ral_shader_create_info shader_create_info_items[] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);

    ral_shader result_shaders[n_shader_create_info_items];

    if (!ral_context_create_programs(context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &scrollbar_ptr->program_slider) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    if (!ral_context_create_shaders(context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    /* Set up shaders */
    const system_hashed_ansi_string fs_body = system_hashed_ansi_string_create(ui_scrollbar_fragment_shader_body);
    const system_hashed_ansi_string vs_body = system_hashed_ansi_string_create(ui_general_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);

    /* Set up program object */
    if (!ral_program_attach_shader(scrollbar_ptr->program_slider,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(scrollbar_ptr->program_slider,
                                   vs,
                                   true /* async */))
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following button instances will reuse the program */
    ui_register_program(ui_instance,
                        _ui_scrollbar_slider_program_name,
                        scrollbar_ptr->program_slider);

    /* Release shaders we will no longer need */
    ral_shader shaders_to_release[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(scrollbar_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** TODO */
PRIVATE void _ui_scrollbar_update_slider_handle_position(_ui_scrollbar* scrollbar_ptr)
{
    float       max_value  = 0;
    float       min_value  = 0;
    float       new_value  = 0;
    float       slider_pos = 0;
    const float x_delta    = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];
    const float y_delta    = scrollbar_ptr->slider_x1y1x2y2[3] - scrollbar_ptr->slider_x1y1x2y2[1];

    scrollbar_ptr->pfn_get_current_value_ptr(scrollbar_ptr->get_current_value_ptr_user_arg,
                                             scrollbar_ptr->temp_variant);

    system_variant_get_float(scrollbar_ptr->max_value_variant,
                            &max_value);
    system_variant_get_float(scrollbar_ptr->min_value_variant,
                            &min_value);
    system_variant_get_float(scrollbar_ptr->temp_variant,
                            &new_value);

    slider_pos = (new_value - min_value) / (max_value - min_value);

    scrollbar_ptr->slider_handle_x1y1x2y2[0] = scrollbar_ptr->slider_x1y1x2y2[0] +
                                               x_delta * slider_pos - (scrollbar_ptr->slider_handle_size[0] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[1] = scrollbar_ptr->slider_x1y1x2y2[1] +
                                               y_delta * 0.5f       - (scrollbar_ptr->slider_handle_size[1] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[2] = scrollbar_ptr->slider_x1y1x2y2[0] +
                                               x_delta * slider_pos + (scrollbar_ptr->slider_handle_size[0] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[3] = scrollbar_ptr->slider_x1y1x2y2[1] +
                                               y_delta * 0.5f       + (scrollbar_ptr->slider_handle_size[1] * 0.5f);
}

/** TODO */
PRIVATE void _ui_scrollbar_update_text_position(_ui_scrollbar* scrollbar_ptr)
{
    int           text_height    = 0;
    int           text_xy[2]     = {0};
    int           text_width     = 0;
    system_window window         = nullptr;
    int           window_size[2] = {0};

    ral_context_get_property  (scrollbar_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    varia_text_renderer_get_text_string_property(scrollbar_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 scrollbar_ptr->text_index,
                                                &text_height);
    varia_text_renderer_get_text_string_property(scrollbar_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 scrollbar_ptr->text_index,
                                                &text_width);

    if (scrollbar_ptr->text_location == UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
    {
        text_xy[0] = (int) ((scrollbar_ptr->slider_x1y1x2y2[0] + (scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0] - float(text_width)  / window_size[0])  * 0.5f) * (float) window_size[0]);
        text_xy[1] = (int) ((1.0f - scrollbar_ptr->slider_x1y1x2y2[1] - (float(text_height) / window_size[1]) ) * (float) window_size[1]);
    }
    else
    {
        text_xy[0] = (int) ((scrollbar_ptr->slider_x1y1x2y2[0])        * (float) window_size[0]) - text_width - TEXT_SCROLLBAR_SEPARATION_PX;
        text_xy[1] = (int) ((1.0f - scrollbar_ptr->slider_x1y1x2y2[1] + 0.5f * (float(text_height) / window_size[1] - scrollbar_ptr->slider_x1y1x2y2[3] + scrollbar_ptr->slider_x1y1x2y2[1])) * (float) window_size[1]);
    }

    varia_text_renderer_set_text_string_property(scrollbar_ptr->text_renderer,
                                                 scrollbar_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                 text_xy);
}

/** TODO */
PUBLIC void ui_scrollbar_deinit(void* internal_instance)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(internal_instance);

    /* Release block buffers */
    if (scrollbar_ptr->program_handle_ub_fs != nullptr)
    {
        ral_program_block_buffer_release(scrollbar_ptr->program_handle_ub_fs);

        scrollbar_ptr->program_handle_ub_fs = nullptr;
    }

    if (scrollbar_ptr->program_slider_ub_fs != nullptr)
    {
        ral_program_block_buffer_release(scrollbar_ptr->program_slider_ub_fs);

        scrollbar_ptr->program_slider_ub_fs = nullptr;
    }

    if (scrollbar_ptr->program_slider_ub_vs != nullptr)
    {
        ral_program_block_buffer_release(scrollbar_ptr->program_slider_ub_vs);

        scrollbar_ptr->program_slider_ub_vs = nullptr;
    }

    /* Release program */
    ral_context_delete_objects(scrollbar_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &scrollbar_ptr->program_slider);

    /* Release variants */
    system_variant_release(scrollbar_ptr->max_value_variant);
    system_variant_release(scrollbar_ptr->min_value_variant);
    system_variant_release(scrollbar_ptr->temp_variant);

    /* Release other stuff */
    if (scrollbar_ptr->last_cached_command_buffer != nullptr)
    {
        ral_command_buffer_release(scrollbar_ptr->last_cached_command_buffer);

        scrollbar_ptr->last_cached_command_buffer = nullptr;
    }

    if (scrollbar_ptr->last_cached_gfx_state != nullptr)
    {
        ral_gfx_state_release(scrollbar_ptr->last_cached_gfx_state);

        scrollbar_ptr->last_cached_gfx_state = nullptr;
    }

    if (scrollbar_ptr->last_cached_present_task != nullptr)
    {
        ral_present_task_release(scrollbar_ptr->last_cached_present_task);

        scrollbar_ptr->last_cached_present_task = nullptr;
    }
}

/** TODO */
PRIVATE void _ui_scrollbar_update_ub_cpu_task_callback(void* scrollbar_raw_ptr)
{
    static const bool bool_false    = 0;
    static const bool bool_true     = 1;
    float             brightness;
    _ui_scrollbar*    scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(scrollbar_raw_ptr);
    system_time       time_now      = system_time_now();

    /* Update brightness if necessary */
    brightness = scrollbar_ptr->current_gpu_brightness_level;

    if (scrollbar_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_time transition_start = scrollbar_ptr->start_hovering_time;
        system_time transition_end   = scrollbar_ptr->start_hovering_time + NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start && time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) / float(NONFOCUSED_TO_FOCUSED_TRANSITION_TIME);

            brightness = scrollbar_ptr->start_hovering_brightness +
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
        system_time transition_start = scrollbar_ptr->start_hovering_time;
        system_time transition_end   = scrollbar_ptr->start_hovering_time + FOCUSED_TO_NONFOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start && time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) / float(FOCUSED_TO_NONFOCUSED_TRANSITION_TIME);

            brightness = scrollbar_ptr->start_hovering_brightness + dt * (NONFOCUSED_BRIGHTNESS - FOCUSED_BRIGHTNESS);

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

    if (scrollbar_ptr->current_gpu_brightness_level != brightness ||
        scrollbar_ptr->force_gpu_brightness_update)
    {
        const float new_brightness = brightness * (scrollbar_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1);

        ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                               scrollbar_ptr->program_slider_brightness_ub_offset,
                                                              &new_brightness,
                                                               sizeof(float) );

        scrollbar_ptr->current_gpu_brightness_level = brightness;
        scrollbar_ptr->force_gpu_brightness_update  = false;
    }

    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                           scrollbar_ptr->program_slider_border_width_ub_offset,
                                                           scrollbar_ptr->slider_border_width,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                           scrollbar_ptr->program_slider_is_handle_ub_offset,
                                                          &bool_false,
                                                           sizeof(bool) );
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_vs,
                                                           scrollbar_ptr->program_slider_x1y1x2y2_ub_offset,
                                                           scrollbar_ptr->slider_x1y1x2y2,
                                                           sizeof(float) * 4);
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_handle_ub_fs,
                                                           scrollbar_ptr->program_slider_border_width_ub_offset,
                                                           scrollbar_ptr->slider_handle_border_width,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_handle_ub_fs,
                                                           scrollbar_ptr->program_slider_is_handle_ub_offset,
                                                          &bool_true,
                                                           sizeof(bool) );
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_handle_ub_fs,
                                                           scrollbar_ptr->program_slider_x1y1x2y2_ub_offset,
                                                           scrollbar_ptr->slider_handle_x1y1x2y2,
                                                           sizeof(float) * 4);

    ral_program_block_buffer_sync(scrollbar_ptr->program_handle_ub_fs);
    ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_fs);
    ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_vs);
}

/** TODO */
PUBLIC ral_present_task ui_scrollbar_get_present_task(void*            internal_instance,
                                                      ral_texture_view target_texture_view)
{
    ral_buffer     handle_ub_fs_bo             = nullptr;
    ral_buffer     slider_ub_fs_bo             = nullptr;
    ral_buffer     slider_ub_vs_bo             = nullptr;
    _ui_scrollbar* scrollbar_ptr               = reinterpret_cast<_ui_scrollbar*>(internal_instance);
    uint32_t       target_texture_view_height  = 0;
    uint32_t       target_texture_view_width   = 0;

    /* Can we re-use the last present task we cached? */
    if (scrollbar_ptr->last_cached_present_task        != nullptr                   &&
        scrollbar_ptr->last_cached_target_texture_view == target_texture_view       &&
        scrollbar_ptr->last_cached_is_visible          == scrollbar_ptr->is_visible)
    {
        goto end;
    }

    /* Can we re-use the last gfx state we cached? */
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                        &target_texture_view_width);
    ral_texture_view_get_mipmap_property(target_texture_view,
                                         0, /* n_layer  */
                                         0, /* n_mipmap */
                                         RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                        &target_texture_view_height);

    if (scrollbar_ptr->last_cached_gfx_state != nullptr)
    {
        ral_command_buffer_set_viewport_command_info viewport;

        ral_gfx_state_get_property(scrollbar_ptr->last_cached_gfx_state,
                                   RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                  &viewport);

        if (fabs(viewport.size[0] - target_texture_view_width)  > 1e-5f ||
            fabs(viewport.size[1] - target_texture_view_height) > 1e-5f)
        {
            ral_gfx_state_release(scrollbar_ptr->last_cached_gfx_state);

            scrollbar_ptr->last_cached_gfx_state = nullptr;
        }
    }

    if (scrollbar_ptr->last_cached_gfx_state == nullptr)
    {
        ral_command_buffer_set_scissor_box_command_info gfx_state_scissor_box;
        ral_gfx_state_create_info                       gfx_state_create_info;
        ral_command_buffer_set_viewport_command_info    gfx_state_viewport;

        gfx_state_scissor_box.index   = 0;
        gfx_state_scissor_box.size[0] = target_texture_view_width;
        gfx_state_scissor_box.size[1] = target_texture_view_height;
        gfx_state_scissor_box.xy  [0] = 0;
        gfx_state_scissor_box.xy  [1] = 0;

        gfx_state_viewport.depth_range[0] = 0.0f;
        gfx_state_viewport.depth_range[1] = 1.0f;
        gfx_state_viewport.index          = 0;
        gfx_state_viewport.size[0]        = static_cast<float>(target_texture_view_width);
        gfx_state_viewport.size[1]        = static_cast<float>(target_texture_view_height);
        gfx_state_viewport.xy  [0]        = 0;
        gfx_state_viewport.xy  [1]        = 0;

        gfx_state_create_info.primitive_type                       = RAL_PRIMITIVE_TYPE_TRIANGLE_FAN;
        gfx_state_create_info.static_n_scissor_boxes_and_viewports = 1;
        gfx_state_create_info.static_scissor_boxes                 = &gfx_state_scissor_box;
        gfx_state_create_info.static_scissor_boxes_enabled         = true;
        gfx_state_create_info.static_viewports                     = &gfx_state_viewport;
        gfx_state_create_info.static_viewports_enabled             = true;

        scrollbar_ptr->last_cached_gfx_state = ral_gfx_state_create(scrollbar_ptr->context,
                                                                   &gfx_state_create_info);
    }

    if (scrollbar_ptr->last_cached_command_buffer == nullptr)
    {
        ral_command_buffer_create_info command_buffer_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = true;
        command_buffer_create_info.is_transient                            = false;

        scrollbar_ptr->last_cached_command_buffer = ral_command_buffer_create(scrollbar_ptr->context,
                                                                             &command_buffer_create_info);
    }

    /* Record commands */
    ral_program_block_buffer_get_property(scrollbar_ptr->program_handle_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &handle_ub_fs_bo);
    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &slider_ub_fs_bo);
    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &slider_ub_vs_bo);

    ral_command_buffer_start_recording(scrollbar_ptr->last_cached_command_buffer);
    {
        if (scrollbar_ptr->is_visible)
        {
            /* Draw stuff otherwise */
            ral_command_buffer_draw_call_regular_command_info      draw_call_info;
            ral_command_buffer_set_color_rendertarget_command_info rt_info;
            ral_command_buffer_set_binding_command_info            ub_handle_binding;
            ral_command_buffer_set_binding_command_info            ub_slider_bindings[2];

            draw_call_info.base_instance = 0;
            draw_call_info.base_vertex   = 0;
            draw_call_info.n_instances   = 1;
            draw_call_info.n_vertices    = 4;

            rt_info.blend_enabled          = true;
            rt_info.blend_op_alpha         = RAL_BLEND_OP_ADD;
            rt_info.blend_op_color         = RAL_BLEND_OP_ADD;
            rt_info.dst_alpha_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            rt_info.dst_color_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            rt_info.rendertarget_index     = 0;
            rt_info.src_alpha_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            rt_info.src_color_blend_factor = RAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            rt_info.texture_view           = target_texture_view;

            ub_handle_binding.binding_type                      = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            ub_handle_binding.name                              = system_hashed_ansi_string_create("dataFS");
            ub_handle_binding.uniform_buffer_binding.buffer     = handle_ub_fs_bo;
            ub_handle_binding.uniform_buffer_binding.offset     = 0;
            ub_handle_binding.uniform_buffer_binding.size       = scrollbar_ptr->program_slider_ub_fs_bo_size;
            ub_slider_bindings[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            ub_slider_bindings[0].name                          = system_hashed_ansi_string_create("dataFS");
            ub_slider_bindings[0].uniform_buffer_binding.buffer = slider_ub_fs_bo;
            ub_slider_bindings[0].uniform_buffer_binding.offset = 0;
            ub_slider_bindings[0].uniform_buffer_binding.size   = scrollbar_ptr->program_slider_ub_fs_bo_size;
            ub_slider_bindings[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            ub_slider_bindings[1].name                          = system_hashed_ansi_string_create("dataVS");
            ub_slider_bindings[1].uniform_buffer_binding.buffer = slider_ub_vs_bo;
            ub_slider_bindings[1].uniform_buffer_binding.offset = 0;
            ub_slider_bindings[1].uniform_buffer_binding.size   = scrollbar_ptr->program_slider_ub_vs_bo_size;


            ral_command_buffer_record_set_bindings           (scrollbar_ptr->last_cached_command_buffer,
                                                              sizeof(ub_slider_bindings) / sizeof(ub_slider_bindings[0]),
                                                              ub_slider_bindings);
            ral_command_buffer_record_set_color_rendertargets(scrollbar_ptr->last_cached_command_buffer,
                                                              1, /* n_rendertargets */
                                                             &rt_info);
            ral_command_buffer_record_set_gfx_state          (scrollbar_ptr->last_cached_command_buffer,
                                                              scrollbar_ptr->last_cached_gfx_state);
            ral_command_buffer_record_set_program            (scrollbar_ptr->last_cached_command_buffer,
                                                              scrollbar_ptr->program_slider);
            ral_command_buffer_record_draw_call_regular      (scrollbar_ptr->last_cached_command_buffer,
                                                              1, /* n_draw_calls */
                                                             &draw_call_info);

            /* Draw the handle */
            ral_command_buffer_record_set_bindings     (scrollbar_ptr->last_cached_command_buffer,
                                                        1,
                                                       &ub_handle_binding);
            ral_command_buffer_record_draw_call_regular(scrollbar_ptr->last_cached_command_buffer,
                                                        1, /* n_draw_calls */
                                                       &draw_call_info);
        }
    }
    ral_command_buffer_stop_recording(scrollbar_ptr->last_cached_command_buffer);

    /* Form the present task */
    ral_present_task                    cpu_present_task;
    ral_present_task_cpu_create_info    cpu_present_task_create_info;
    ral_present_task_io                 cpu_present_task_outputs[3];
    ral_present_task                    draw_present_task;
    ral_present_task_gpu_create_info    draw_present_task_create_info;
    ral_present_task_io                 draw_present_task_inputs[4];
    ral_present_task_io                 draw_present_task_output;
    ral_present_task                    present_tasks[2];
    ral_present_task_group_create_info  result_present_task_create_info;
    ral_present_task_ingroup_connection result_present_task_ingroup_connections[3];
    ral_present_task_group_mapping      result_present_task_input_mapping;
    ral_present_task_group_mapping      result_present_task_output_mapping;

    cpu_present_task_outputs[0].buffer      = handle_ub_fs_bo;
    cpu_present_task_outputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    cpu_present_task_outputs[1].buffer      = slider_ub_fs_bo;
    cpu_present_task_outputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    cpu_present_task_outputs[2].buffer      = slider_ub_vs_bo;
    cpu_present_task_outputs[2].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

    cpu_present_task_create_info.cpu_task_callback_user_arg = scrollbar_ptr;
    cpu_present_task_create_info.n_unique_inputs            = 0;
    cpu_present_task_create_info.n_unique_outputs           = sizeof(cpu_present_task_outputs) / sizeof(cpu_present_task_outputs[0]);
    cpu_present_task_create_info.pfn_cpu_task_callback_proc = _ui_scrollbar_update_ub_cpu_task_callback;
    cpu_present_task_create_info.unique_inputs              = nullptr;
    cpu_present_task_create_info.unique_outputs             = cpu_present_task_outputs;

    cpu_present_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("UI scrollbar: UB update"),
                                                  &cpu_present_task_create_info);


    draw_present_task_inputs[0].buffer       = handle_ub_fs_bo;
    draw_present_task_inputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[1].buffer       = slider_ub_fs_bo;
    draw_present_task_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[2].buffer       = slider_ub_fs_bo;
    draw_present_task_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    draw_present_task_inputs[3].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_inputs[3].texture_view = target_texture_view;

    draw_present_task_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
    draw_present_task_output.texture_view = target_texture_view;

    draw_present_task_create_info.command_buffer   = scrollbar_ptr->last_cached_command_buffer;
    draw_present_task_create_info.n_unique_inputs  = sizeof(draw_present_task_inputs) / sizeof(draw_present_task_inputs[0]);
    draw_present_task_create_info.n_unique_outputs = 1;
    draw_present_task_create_info.unique_inputs    = draw_present_task_inputs;
    draw_present_task_create_info.unique_outputs   = &draw_present_task_output;

    draw_present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("UI scrollbar: draw"),
                                                   &draw_present_task_create_info);


    present_tasks[0] = cpu_present_task;
    present_tasks[1] = draw_present_task;

    for (uint32_t n_connection = 0;
                  n_connection < 3;
                ++n_connection)
    {
        result_present_task_ingroup_connections[n_connection].input_present_task_index     = 1;
        result_present_task_ingroup_connections[n_connection].input_present_task_io_index  = n_connection;
        result_present_task_ingroup_connections[n_connection].output_present_task_index    = 0;
        result_present_task_ingroup_connections[n_connection].output_present_task_io_index = n_connection;
    }

    result_present_task_input_mapping.io_index       = 3; /* target_texture_view */
    result_present_task_input_mapping.n_present_task = 1;

    result_present_task_output_mapping.io_index       = 0; /* target_texture_view */
    result_present_task_output_mapping.n_present_task = 1;

    result_present_task_create_info.ingroup_connections                   = result_present_task_ingroup_connections;
    result_present_task_create_info.n_ingroup_connections                 = sizeof(result_present_task_ingroup_connections) / sizeof(result_present_task_ingroup_connections[0]);
    result_present_task_create_info.n_present_tasks                       = sizeof(present_tasks) / sizeof(present_tasks[0]);
    result_present_task_create_info.n_unique_inputs                       = 1;
    result_present_task_create_info.n_unique_outputs                      = 1;
    result_present_task_create_info.present_tasks                         = present_tasks;
    result_present_task_create_info.unique_input_to_ingroup_task_mapping  = &result_present_task_input_mapping;
    result_present_task_create_info.unique_output_to_ingroup_task_mapping = &result_present_task_output_mapping;

    scrollbar_ptr->last_cached_present_task = ral_present_task_create_group(&result_present_task_create_info);

    ral_present_task_release(cpu_present_task);
    ral_present_task_release(draw_present_task);

end:
    ral_present_task_retain(scrollbar_ptr->last_cached_present_task);

    return scrollbar_ptr->last_cached_present_task;
}

/** TODO */
PUBLIC void ui_scrollbar_get_property(const void*         scrollbar,
                                      ui_control_property property,
                                      void*               out_result)
{
    const _ui_scrollbar* scrollbar_ptr = reinterpret_cast<const _ui_scrollbar*>(scrollbar);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            if (scrollbar_ptr->is_visible)
            {
                float text_height = 0.0f;

                varia_text_renderer_get_text_string_property(scrollbar_ptr->text_renderer,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                                             scrollbar_ptr->text_index,
                                                            &text_height);

                *(float*) out_result = scrollbar_ptr->slider_x1y1x2y2[3] -
                                       scrollbar_ptr->slider_x1y1x2y2[1];

                if (scrollbar_ptr->text_location == UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
                {
                    *(float*) out_result += text_height;
                }
            }
            else
            {
                *(float*) out_result = 0.0f;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED:
        {
            *(float*) out_result = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        {
            ((float*) out_result)[0] = scrollbar_ptr->slider_x1y1x2y2[0];
            ((float*) out_result)[1] = scrollbar_ptr->slider_x1y1x2y2[1];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        case UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE:
        {
            *(bool*) out_result = scrollbar_ptr->is_visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}

/** TODO */
PUBLIC void ui_scrollbar_hover(void*        internal_instance,
                               const float* xy_screen_norm)
{
}

/** TODO */
PUBLIC void* ui_scrollbar_init(ui                          ui_instance,
                               varia_text_renderer         text_renderer,
                               ui_scrollbar_text_location  text_location,
                               system_hashed_ansi_string   name,
                               system_variant              min_value,
                               system_variant              max_value,
                               const float*                x1y1,
                               const float*                x2y2,
                               PFNUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                               void*                       get_current_value_ptr_user_arg,
                               PFNUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                               void*                       set_current_value_ptr_user_arg)
{
    _ui_scrollbar* new_scrollbar_ptr = new (std::nothrow) _ui_scrollbar;

    ASSERT_ALWAYS_SYNC(new_scrollbar_ptr != nullptr,
                       "Out of memory");

    if (new_scrollbar_ptr != nullptr)
    {
        /* Initialize fields */
        memset(new_scrollbar_ptr,
               0,
               sizeof(_ui_scrollbar) );

        new_scrollbar_ptr->gpu_slider_handle_position = 0;
        new_scrollbar_ptr->is_visible                 = true;

        new_scrollbar_ptr->slider_x1y1x2y2[0] =     x1y1[0];
        new_scrollbar_ptr->slider_x1y1x2y2[1] = 1 - x2y2[1];
        new_scrollbar_ptr->slider_x1y1x2y2[2] =     x2y2[0];
        new_scrollbar_ptr->slider_x1y1x2y2[3] = 1 - x1y1[1];

        new_scrollbar_ptr->current_gpu_brightness_level   = NONFOCUSED_BRIGHTNESS;
        new_scrollbar_ptr->context                        = ui_get_context(ui_instance);
        new_scrollbar_ptr->pfn_get_current_value_ptr      = pfn_get_current_value_ptr;
        new_scrollbar_ptr->get_current_value_ptr_user_arg = get_current_value_ptr_user_arg;
        new_scrollbar_ptr->pfn_set_current_value_ptr      = pfn_set_current_value_ptr;
        new_scrollbar_ptr->set_current_value_ptr_user_arg = set_current_value_ptr_user_arg;

        new_scrollbar_ptr->text_renderer                  = text_renderer;
        new_scrollbar_ptr->text_index                     = varia_text_renderer_add_string(text_renderer);
        new_scrollbar_ptr->text_location                  = text_location;
        new_scrollbar_ptr->ui_instance                    = ui_instance;

        new_scrollbar_ptr->max_value_variant              = system_variant_create(system_variant_get_type(max_value) );
        new_scrollbar_ptr->min_value_variant              = system_variant_create(system_variant_get_type(min_value) );
        new_scrollbar_ptr->temp_variant                   = system_variant_create(system_variant_get_type(max_value) );

        system_variant_set(new_scrollbar_ptr->max_value_variant,
                           max_value,
                           false);
        system_variant_set(new_scrollbar_ptr->min_value_variant,
                           min_value,
                           false);

        /* Configure the text to be shown on the button */
        varia_text_renderer_set(new_scrollbar_ptr->text_renderer,
                                new_scrollbar_ptr->text_index,
                                system_hashed_ansi_string_get_buffer(name) );

        int           text_height    = 0;
        system_window window         = nullptr;
        int           window_size[2] = {0};

        varia_text_renderer_get_text_string_property(new_scrollbar_ptr->text_renderer,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                     new_scrollbar_ptr->text_index,
                                                    &text_height);

        ral_context_get_property  (new_scrollbar_ptr->context,
                                   RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                  &window);
        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        new_scrollbar_ptr->slider_x1y1x2y2[3] -= float(text_height + TEXT_SCROLLBAR_SEPARATION_PX) / window_size[1];

        ASSERT_DEBUG_SYNC(new_scrollbar_ptr->slider_x1y1x2y2[1] < new_scrollbar_ptr->slider_x1y1x2y2[3],
                          "Scrollbar has illegal height");

        varia_text_renderer_set_text_string_property(new_scrollbar_ptr->text_renderer,
                                                     new_scrollbar_ptr->text_index,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                     _ui_scrollbar_text_color);

        _ui_scrollbar_update_text_position(new_scrollbar_ptr);

        /* Calculate slider sizes */
        new_scrollbar_ptr->slider_border_width       [0] = 1.0f / (float)((new_scrollbar_ptr->slider_x1y1x2y2[2] - new_scrollbar_ptr->slider_x1y1x2y2[0]) * window_size[0]);
        new_scrollbar_ptr->slider_border_width       [1] = 1.0f / (float)((new_scrollbar_ptr->slider_x1y1x2y2[3] - new_scrollbar_ptr->slider_x1y1x2y2[1]) * window_size[1]);
        new_scrollbar_ptr->slider_handle_border_width[0] = 1.0f / (float)(SLIDER_SIZE_PX);
        new_scrollbar_ptr->slider_handle_border_width[1] = 1.0f / (float)(SLIDER_SIZE_PX);

        /* Calculate slider handle size */
        new_scrollbar_ptr->slider_handle_size[0] = float(SLIDER_SIZE_PX) / float(window_size[0]);
        new_scrollbar_ptr->slider_handle_size[1] = float(SLIDER_SIZE_PX) / float(window_size[1]);

        /* Update slider handle position */
        _ui_scrollbar_update_slider_handle_position(new_scrollbar_ptr);

        /* Retrieve the rendering program */
        new_scrollbar_ptr->program_slider = ui_get_registered_program(ui_instance,
                                                                      _ui_scrollbar_slider_program_name);

        if (new_scrollbar_ptr->program_slider == nullptr)
        {
            _ui_scrollbar_init_program(ui_instance,
                                       new_scrollbar_ptr);

            ASSERT_DEBUG_SYNC(new_scrollbar_ptr->program_slider != nullptr,
                              "Could not initialize scrollbar slider UI program");
        }

        /* Set up predefined values */
        float        border_width[2]  = {0};
        ral_buffer   datafs_ub_bo_ral = nullptr;
        unsigned int datafs_ub_index  = -1;
        ral_buffer   datavs_ub_bo_ral = nullptr;
        unsigned int datavs_ub_index  = -1;

        new_scrollbar_ptr->program_handle_ub_fs = ral_program_block_buffer_create(new_scrollbar_ptr->context,
                                                                                  new_scrollbar_ptr->program_slider,
                                                                                  system_hashed_ansi_string_create("dataFS") );
        new_scrollbar_ptr->program_slider_ub_fs = ral_program_block_buffer_create(new_scrollbar_ptr->context,
                                                                                  new_scrollbar_ptr->program_slider,
                                                                                  system_hashed_ansi_string_create("dataFS") );
        new_scrollbar_ptr->program_slider_ub_vs = ral_program_block_buffer_create(new_scrollbar_ptr->context,
                                                                                  new_scrollbar_ptr->program_slider,
                                                                                  system_hashed_ansi_string_create("dataVS") );

        ral_program_block_buffer_get_property(new_scrollbar_ptr->program_slider_ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &datafs_ub_bo_ral);
        ral_program_block_buffer_get_property(new_scrollbar_ptr->program_slider_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &datavs_ub_bo_ral);

        ral_buffer_get_property(datafs_ub_bo_ral,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &new_scrollbar_ptr->program_slider_ub_fs_bo_size);
        ral_buffer_get_property(datavs_ub_bo_ral,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &new_scrollbar_ptr->program_slider_ub_vs_bo_size);

        /* Retrieve uniform locations */
        const ral_program_variable* border_width_uniform_ral_ptr = nullptr;
        const ral_program_variable* brightness_uniform_ral_ptr   = nullptr;
        const ral_program_variable* is_handle_uniform_ral_ptr    = nullptr;
        const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = nullptr;

        ral_program_get_block_variable_by_name(new_scrollbar_ptr->program_slider,
                                               system_hashed_ansi_string_create("dataFS"),
                                               system_hashed_ansi_string_create("border_width"),
                                              &border_width_uniform_ral_ptr);
        ral_program_get_block_variable_by_name(new_scrollbar_ptr->program_slider,
                                               system_hashed_ansi_string_create("dataFS"),
                                               system_hashed_ansi_string_create("brightness"),
                                              &brightness_uniform_ral_ptr);
        ral_program_get_block_variable_by_name(new_scrollbar_ptr->program_slider,
                                               system_hashed_ansi_string_create("dataFS"),
                                               system_hashed_ansi_string_create("is_handle"),
                                              &is_handle_uniform_ral_ptr);
        ral_program_get_block_variable_by_name(new_scrollbar_ptr->program_slider,
                                               system_hashed_ansi_string_create("dataVS"),
                                               system_hashed_ansi_string_create("x1y1x2y2"),
                                              &x1y1x2y2_uniform_ral_ptr);

        new_scrollbar_ptr->program_slider_border_width_ub_offset = (border_width_uniform_ral_ptr != nullptr ? border_width_uniform_ral_ptr->block_offset : -1);
        new_scrollbar_ptr->program_slider_brightness_ub_offset   = (brightness_uniform_ral_ptr   != nullptr ? brightness_uniform_ral_ptr->block_offset   : -1);
        new_scrollbar_ptr->program_slider_is_handle_ub_offset    = (is_handle_uniform_ral_ptr    != nullptr ? is_handle_uniform_ral_ptr->block_offset    : -1);
        new_scrollbar_ptr->program_slider_x1y1x2y2_ub_offset     = (x1y1x2y2_uniform_ral_ptr     != nullptr ? x1y1x2y2_uniform_ral_ptr->block_offset     : -1);

        /* Set general uniforms */
        ral_program_block_buffer_set_nonarrayed_variable_value(new_scrollbar_ptr->program_slider_ub_fs,
                                                               new_scrollbar_ptr->program_slider_brightness_ub_offset,
                                                              &new_scrollbar_ptr->current_gpu_brightness_level,
                                                               sizeof(float) );
    }

    return (void*) new_scrollbar_ptr;
}

/** TODO */
PUBLIC bool ui_scrollbar_is_over(void*        internal_instance,
                                 const float* xy)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(internal_instance);
    float          inversed_y    = 1.0f - xy[1];

    if (scrollbar_ptr->is_lbm_on)
    {
        return true;
    }

    if (xy[0]      >= scrollbar_ptr->slider_handle_x1y1x2y2[0] && xy[0]      <= scrollbar_ptr->slider_handle_x1y1x2y2[2] &&
        inversed_y >= scrollbar_ptr->slider_handle_x1y1x2y2[1] && inversed_y <= scrollbar_ptr->slider_handle_x1y1x2y2[3])
    {
        if (!scrollbar_ptr->is_hovering)
        {
            scrollbar_ptr->start_hovering_brightness = scrollbar_ptr->current_gpu_brightness_level;
            scrollbar_ptr->start_hovering_time       = system_time_now();
            scrollbar_ptr->is_hovering               = true;
        }

        return true;
    }
    else
    {
        if (scrollbar_ptr->is_hovering)
        {
            scrollbar_ptr->is_hovering               = false;
            scrollbar_ptr->start_hovering_brightness = scrollbar_ptr->current_gpu_brightness_level;
            scrollbar_ptr->start_hovering_time       = system_time_now();
        }

        return false;
    }
}

/** TODO */
PUBLIC void ui_scrollbar_on_lbm_down(void*        internal_instance,
                                     const float* xy)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(internal_instance);
    float          inversed_y    = 1.0f - xy[1];

    if (xy[0]      >= scrollbar_ptr->slider_handle_x1y1x2y2[0] &&
        xy[0]      <= scrollbar_ptr->slider_handle_x1y1x2y2[2] &&
        inversed_y >= scrollbar_ptr->slider_handle_x1y1x2y2[1] &&
        inversed_y <= scrollbar_ptr->slider_handle_x1y1x2y2[3])
    {
        scrollbar_ptr->is_lbm_on                   = true;
        scrollbar_ptr->force_gpu_brightness_update = true;
    }
}

/** TODO */
PUBLIC void ui_scrollbar_on_lbm_up(void*        internal_instance,
                                   const float* xy)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(internal_instance);

    scrollbar_ptr->is_lbm_on                   = false;
    scrollbar_ptr->force_gpu_brightness_update = true;
}

/** TODO */
PUBLIC void ui_scrollbar_on_mouse_move(void*        internal_instance,
                                       const float* xy)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(internal_instance);
    float          x_clicked     = xy[0];

    if (scrollbar_ptr->is_lbm_on)
    {
        /* Clamp */
        if (x_clicked < scrollbar_ptr->slider_x1y1x2y2[0])
        {
            x_clicked = scrollbar_ptr->slider_x1y1x2y2[0];
        }
        else
        if (x_clicked > scrollbar_ptr->slider_x1y1x2y2[2])
        {
            x_clicked = scrollbar_ptr->slider_x1y1x2y2[2];
        }

        /* Set the value */
        float max_value, min_value, new_value;

        system_variant_get_float(scrollbar_ptr->max_value_variant,
                                &max_value);
        system_variant_get_float(scrollbar_ptr->min_value_variant,
                                &min_value);

        new_value = min_value                                                               +
                    (x_clicked - scrollbar_ptr->slider_x1y1x2y2[0])                         /
                    (scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0]) *
                    (max_value - min_value);

        system_variant_set_float                (scrollbar_ptr->temp_variant,
                                                 new_value);
        scrollbar_ptr->pfn_set_current_value_ptr(scrollbar_ptr->set_current_value_ptr_user_arg,
                                                 scrollbar_ptr->temp_variant);

        /* Update UI */
        _ui_scrollbar_update_slider_handle_position(scrollbar_ptr);
    }
}

/** TODO */
PUBLIC void ui_scrollbar_set_property(void*               scrollbar,
                                      ui_control_property property,
                                      const void*         data)
{
    _ui_scrollbar* scrollbar_ptr = reinterpret_cast<_ui_scrollbar*>(scrollbar);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        {
            const float dx             = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];
            const float dy             = scrollbar_ptr->slider_x1y1x2y2[3] - scrollbar_ptr->slider_x1y1x2y2[1];
                  float text_height_ss = 0.0f;

            if (scrollbar_ptr->text_location == UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
            {
                varia_text_renderer_get_text_string_property(scrollbar_ptr->text_renderer,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                                             scrollbar_ptr->text_index,
                                                            &text_height_ss);
            }

            scrollbar_ptr->slider_x1y1x2y2[0] =        ((float*) data)[0];
            scrollbar_ptr->slider_x1y1x2y2[1] = 1.0f - ((float*) data)[1] - dy - text_height_ss;
            scrollbar_ptr->slider_x1y1x2y2[2] = scrollbar_ptr->slider_x1y1x2y2[0] + dx;
            scrollbar_ptr->slider_x1y1x2y2[3] = scrollbar_ptr->slider_x1y1x2y2[1] + dy;

            _ui_scrollbar_update_slider_handle_position(scrollbar_ptr);
            _ui_scrollbar_update_text_position         (scrollbar_ptr);

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        case UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE:
        {
            scrollbar_ptr->is_visible = *reinterpret_cast<const bool*>(data);

            varia_text_renderer_set_text_string_property(scrollbar_ptr->text_renderer,
                                                         scrollbar_ptr->text_index,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                         data);

            ui_receive_control_callback(scrollbar_ptr->ui_instance,
                                        (ui_control) scrollbar_ptr,
                                        UI_SCROLLBAR_CALLBACK_ID_VISIBILITY_TOGGLE,
                                        nullptr); /* callback_user_arg */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property property");
        }
    }
}