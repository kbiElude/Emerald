/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
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
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_button.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(450) )


const float _ui_button_text_color[] = {1, 1, 1, 1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_button_program_name = system_hashed_ansi_string_create("UI Button");

/** Internal types */
typedef struct
{
    float x1y1x2y2[4];

    void*            fire_proc_user_arg;
    PFNUIFIREPROCPTR pfn_fire_proc_ptr;

    float       current_gpu_brightness_level;
    bool        force_gpu_brightness_update;
    bool        is_hovering;
    bool        is_lbm_on;
    bool        should_update_border_width;
    float       start_hovering_brightness;
    system_time start_hovering_time;
    bool        visible;

    ral_present_task last_present_task;
    ral_texture_view last_present_task_target_texture_view; /* do NOT release */

    ral_command_buffer       cached_command_buffer;
    ral_context              context;
    ral_present_task         cpu_present_task;
    ral_gfx_state            gfx_state;
    ral_program              program;
    uint32_t                 program_border_width_ub_offset;
    uint32_t                 program_brightness_ub_offset;
    uint32_t                 program_stop_data_ub_offset;
    ral_program_block_buffer program_ub_fs;
    uint32_t                 program_ub_fs_bo_size;
    ral_program_block_buffer program_ub_vs;
    GLuint                   program_ub_vs_bo_size;
    uint32_t                 program_x1y1x2y2_ub_offset;

    uint32_t            text_index;
    varia_text_renderer text_renderer;
} _ui_button;

/** Internal variables */
static const char* ui_button_fragment_shader_body =
    "#version 430 core\n"
    "\n"
    "in  vec2 uv;\n"
    "out vec3 result;\n"
    /* stop, rgb */
    "layout(binding = 0) uniform dataFS\n"
    "{\n"
    "    float brightness;\n"
    "    vec2  border_width;\n"
    "    vec4  stop_data[4];\n"
    "};\n"
    "\n"
    "void main()\n"
    "{\n"
    "    if (uv.y >= stop_data[0].x && uv.y <= stop_data[1].x) result = mix(stop_data[0].yzw, stop_data[1].yzw, (uv.y - stop_data[0].x) / (stop_data[1].x - stop_data[0].x));else\n"
    "    if (uv.y >= stop_data[1].x && uv.y <= stop_data[2].x) result = mix(stop_data[1].yzw, stop_data[2].yzw, (uv.y - stop_data[1].x) / (stop_data[2].x - stop_data[1].x));else\n"
    "                                                          result = mix(stop_data[2].yzw, stop_data[3].yzw, (uv.y - stop_data[2].x) / (stop_data[3].x - stop_data[2].x));\n"
    "\n" 
    "    result *= brightness;\n"
    /* Border? */
    "if (uv.x <= border_width[0] || uv.x >= (1.0 - border_width[0]) || uv.y <= border_width[1] || uv.y >= (1.0 - border_width[1]) ) result = vec3(0.42f, 0.41f, 0.41f);\n"
    "}\n";


/* Forward declarations */
PRIVATE void _ui_button_init                          (_ui_button*               button_ptr,
                                                       ui                        ui_instance,
                                                       varia_text_renderer       text_renderer,
                                                       system_hashed_ansi_string name,
                                                       const float*              x1y1,
                                                       const float*              x2y2,
                                                       PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                                                       void*                     fire_proc_user_arg);
PRIVATE void _ui_button_init_program                  (ui                        ui_instance,
                                                       _ui_button*               button_ptr);
PRIVATE void _ui_button_update_props_cpu_task_callback(void*                     button_raw_ptr);
PRIVATE void _ui_button_update_text_location          (_ui_button*               button_ptr);


/** TODO */
PRIVATE void _ui_button_init(_ui_button*               button_ptr,
                             ui                        ui_instance,
                             varia_text_renderer       text_renderer,
                             system_hashed_ansi_string name,
                             const float*              x1y1,
                             const float*              x2y2,
                             PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                             void*                     fire_proc_user_arg)
{
    static const float stop_data[] = {0,     174.0f / 255.0f * 0.5f, 188.0f / 255.0f * 0.5f, 191.0f / 255.0f * 0.5f,
                                      0.5f,  110.0f / 255.0f * 0.5f, 119.0f / 255.0f * 0.5f, 116.0f / 255.0f * 0.5f,
                                      0.51f, 10.0f  / 255.0f * 0.5f, 14.0f  / 255.0f * 0.5f, 10.0f  / 255.0f * 0.5f,
                                      1.0f,  10.0f  / 255.0f * 0.5f, 8.0f   / 255.0f * 0.5f, 9.0f   / 255.0f * 0.5f};

    memset(button_ptr,
           0,
           sizeof(_ui_button) );

    button_ptr->should_update_border_width = true;
    button_ptr->x1y1x2y2[0]                =     x1y1[0];
    button_ptr->x1y1x2y2[1]                = 1 - x2y2[1];
    button_ptr->x1y1x2y2[2]                =     x2y2[0];
    button_ptr->x1y1x2y2[3]                = 1 - x1y1[1];

    ui_get_property(ui_instance,
                    UI_PROPERTY_CONTEXT,
                   &button_ptr->context);

    button_ptr->fire_proc_user_arg = fire_proc_user_arg;
    button_ptr->pfn_fire_proc_ptr  = pfn_fire_proc_ptr;
    button_ptr->text_renderer      = text_renderer;
    button_ptr->text_index         = varia_text_renderer_add_string(text_renderer);
    button_ptr->visible            = true;

    /* Configure the text to be shown on the button */
    varia_text_renderer_set(button_ptr->text_renderer,
                            button_ptr->text_index,
                            system_hashed_ansi_string_get_buffer(name) );

    _ui_button_update_text_location(button_ptr);

    varia_text_renderer_set_text_string_property(button_ptr->text_renderer,
                                                 button_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 _ui_button_text_color);

    /* Retrieve the rendering program */
    button_ptr->program = ui_get_registered_program(ui_instance,
                                                    ui_button_program_name);

    if (button_ptr->program == nullptr)
    {
        _ui_button_init_program(ui_instance,
                                button_ptr);

        ASSERT_DEBUG_SYNC(button_ptr->program != nullptr,
                          "Could not initialize button UI program");
    }

    /* Retrieve uniform UB offsets */
    const ral_program_variable* border_width_uniform_ral_ptr = nullptr;
    const ral_program_variable* brightness_uniform_ral_ptr   = nullptr;
    const ral_program_variable* stop_data_uniform_ral_ptr    = nullptr;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = nullptr;

    ral_program_get_block_variable_by_name(button_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(button_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("brightness"),
                                          &brightness_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(button_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("stop_data[0]"),
                                          &stop_data_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(button_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_uniform_ral_ptr);

    button_ptr->program_border_width_ub_offset = border_width_uniform_ral_ptr->block_offset;
    button_ptr->program_brightness_ub_offset   = brightness_uniform_ral_ptr->block_offset;
    button_ptr->program_stop_data_ub_offset    = stop_data_uniform_ral_ptr->block_offset;
    button_ptr->program_x1y1x2y2_ub_offset     = x1y1x2y2_uniform_ral_ptr->block_offset;

    /* Retrieve uniform block data */
    button_ptr->program_ub_fs = ral_program_block_buffer_create(button_ptr->context,
                                                                button_ptr->program,
                                                                system_hashed_ansi_string_create("dataFS") );
    button_ptr->program_ub_vs = ral_program_block_buffer_create(button_ptr->context,
                                                                button_ptr->program,
                                                                system_hashed_ansi_string_create("dataVS") );

    /* Set it up */
    ral_program_block_buffer_set_arrayed_variable_value(button_ptr->program_ub_fs,
                                                        stop_data_uniform_ral_ptr->block_offset,
                                                        stop_data,
                                                        sizeof(stop_data),
                                                        0, /* dst_array_start_index */
                                                        sizeof(stop_data) / sizeof(float) / 4);

    button_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;

    /* Initialize the CPU present task. The GPU & group one have a render-target dependency, so they
     * optional creation need to be postponed till runtime */
    ral_buffer                       ub_fs_bo = nullptr;
    ral_buffer                       ub_vs_bo = nullptr;
    ral_present_task_cpu_create_info update_present_task_create_info;
    ral_present_task_io              update_present_task_unique_outputs[2];

    ral_program_block_buffer_get_property(button_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_fs_bo);
    ral_program_block_buffer_get_property(button_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &ub_vs_bo);


    update_present_task_unique_outputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    update_present_task_unique_outputs[0].buffer      = ub_fs_bo;

    update_present_task_unique_outputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
    update_present_task_unique_outputs[1].buffer      = ub_vs_bo;

    update_present_task_create_info.cpu_task_callback_user_arg = button_ptr;
    update_present_task_create_info.n_unique_inputs            = 0;
    update_present_task_create_info.n_unique_outputs           = sizeof(update_present_task_unique_outputs) / sizeof(update_present_task_unique_outputs[0]);
    update_present_task_create_info.pfn_cpu_task_callback_proc = _ui_button_update_props_cpu_task_callback;
    update_present_task_create_info.unique_inputs              = nullptr;
    update_present_task_create_info.unique_outputs             = update_present_task_unique_outputs;

    button_ptr->cpu_present_task = ral_present_task_create_cpu(system_hashed_ansi_string_create("Button UI control: UB update"),
                                                              &update_present_task_create_info);

    ral_present_task_retain(button_ptr->cpu_present_task);
}

/** TODO */
PRIVATE void _ui_button_init_program(ui          ui_instance,
                                     _ui_button* button_ptr)
{
    /* Create all objects */
    ral_context context = button_ptr->context;
    ral_shader  fs      = nullptr;
    ral_program program = nullptr;
    ral_shader  vs      = nullptr;

    ral_shader_create_info  fs_create_info;
    ral_program_create_info program_create_info;
    ral_shader_create_info  vs_create_info;

    fs_create_info.name   = system_hashed_ansi_string_create("UI button fragment shader");
    fs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    fs_create_info.type   = RAL_SHADER_TYPE_FRAGMENT;

    program_create_info.active_shader_stages = RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX;
    program_create_info.name                 = system_hashed_ansi_string_create("UI button program");

    vs_create_info.name   = system_hashed_ansi_string_create("UI button vertex shader");
    vs_create_info.source = RAL_SHADER_SOURCE_GLSL;
    vs_create_info.type   = RAL_SHADER_TYPE_VERTEX;

    ral_shader_create_info shader_create_info_items[2] =
    {
        fs_create_info,
        vs_create_info
    };
    const uint32_t         n_shader_create_info_items = sizeof(shader_create_info_items) / sizeof(shader_create_info_items[0]);
    ral_shader             result_shaders[n_shader_create_info_items];

    if (!ral_context_create_shaders(button_ptr->context,
                                    n_shader_create_info_items,
                                    shader_create_info_items,
                                    result_shaders) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    fs = result_shaders[0];
    vs = result_shaders[1];

    if (!ral_context_create_programs(button_ptr->context,
                                     1, /* n_create_info_items */
                                    &program_create_info,
                                    &program) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed.");
    }

    /* Set up shaders */
    const system_hashed_ansi_string fs_body_has = system_hashed_ansi_string_create(ui_button_fragment_shader_body);
    const system_hashed_ansi_string vs_body_has = system_hashed_ansi_string_create(ui_general_vertex_shader_body);


    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body_has);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body_has);

    /* Set up program object */
    if (!ral_program_attach_shader(program,
                                   fs,
                                   true /* async */) ||
        !ral_program_attach_shader(program,
                                   vs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not configure RAL program");
    }

    /* Register the prgoram with UI so following button instances will reuse the program */
    ui_register_program(ui_instance,
                        ui_button_program_name,
                        button_ptr->program);

    /* Release shaders we will no longer need */
    ral_shader shaders_to_delete[] =
    {
        fs,
        vs
    };
    const uint32_t n_shaders_to_delete = sizeof(shaders_to_delete) / sizeof(shaders_to_delete[0]);

    ral_context_delete_objects(button_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_delete,
                               reinterpret_cast<void* const*>(shaders_to_delete) );
}

/** TODO */
PRIVATE void _ui_button_update_props_cpu_task_callback(void* button_raw_ptr)
{
    _ui_button* button_ptr = reinterpret_cast<_ui_button*>(button_raw_ptr);
    system_time time_now   = system_time_now();

    /* Update brightness if necessary */
    float brightness = button_ptr->current_gpu_brightness_level;

    if (button_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_time transition_start = button_ptr->start_hovering_time;
        system_time transition_end   = button_ptr->start_hovering_time + NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) /
                       float(NONFOCUSED_TO_FOCUSED_TRANSITION_TIME);

            brightness = button_ptr->start_hovering_brightness +
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
        system_time transition_start = button_ptr->start_hovering_time;
        system_time transition_end   = button_ptr->start_hovering_time +
                                       FOCUSED_TO_NONFOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) /
                       float(FOCUSED_TO_NONFOCUSED_TRANSITION_TIME);

            brightness = button_ptr->start_hovering_brightness +
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
    const float  new_brightness = brightness * (button_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1);

    if (button_ptr->current_gpu_brightness_level != brightness ||
        button_ptr->force_gpu_brightness_update)
    {
        button_ptr->current_gpu_brightness_level = brightness;
        button_ptr->force_gpu_brightness_update  = false;
    }

    ral_program_block_buffer_set_nonarrayed_variable_value(button_ptr->program_ub_fs,
                                                           button_ptr->program_brightness_ub_offset,
                                                          &new_brightness,
                                                           sizeof(new_brightness) );
    ral_program_block_buffer_set_nonarrayed_variable_value(button_ptr->program_ub_vs,
                                                           button_ptr->program_x1y1x2y2_ub_offset,
                                                           button_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);

    if (button_ptr->should_update_border_width)
    {
        float         border_width[2] = {0};
        system_window window          = nullptr;
        int           window_size[2];

        ral_context_get_property    (button_ptr->context,
                                     RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                    &window);
        system_window_get_property  (window,
                                     SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                     window_size);

        border_width[0] = 1.0f / (float)((button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0]) * (window_size[0]) );
        border_width[1] = 1.0f / (float)((button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1]) * (window_size[1]));

        ral_program_block_buffer_set_nonarrayed_variable_value(button_ptr->program_ub_fs,
                                                               button_ptr->program_border_width_ub_offset,
                                                               border_width,
                                                               sizeof(border_width) );

        button_ptr->should_update_border_width = false;
    }

    ral_program_block_buffer_sync_immediately(button_ptr->program_ub_fs);
    ral_program_block_buffer_sync_immediately(button_ptr->program_ub_vs);
}

/** TODO */
PRIVATE void _ui_button_update_text_location(_ui_button* button_ptr)
{
    int           text_height   = 0;
    int           text_xy[2]    = {0};
    int           text_width    = 0;
    system_window window        = nullptr;
    int           window_size[2];
    const float   x1y1[2]       =
    {
        button_ptr->x1y1x2y2[0],
        1.0f - button_ptr->x1y1x2y2[1]
    };
    const float   x2y2[2]       =
    {
        button_ptr->x1y1x2y2[2],
        1.0f - button_ptr->x1y1x2y2[3]
    };

    varia_text_renderer_get_text_string_property(button_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 button_ptr->text_index,
                                                &text_height);
    varia_text_renderer_get_text_string_property(button_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 button_ptr->text_index,
                                                &text_width);

    ral_context_get_property  (button_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    text_xy[0] = (int) ((x1y1[0] + (x2y2[0] - x1y1[0] - float(text_width)  / window_size[0]) * 0.5f) * (float) window_size[0]);
    text_xy[1] = (int) ((x2y2[1] - (x2y2[1] - x1y1[1] - float(text_height) / window_size[1]) * 0.5f) * (float) window_size[1]);

    varia_text_renderer_set_text_string_property(button_ptr->text_renderer,
                                                 button_ptr->text_index,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                 text_xy);
}


/** Please see header for specification */
PUBLIC void ui_button_deinit(void* internal_instance)
{
    _ui_button* ui_button_ptr = reinterpret_cast<_ui_button*>(internal_instance);

    varia_text_renderer_set(ui_button_ptr->text_renderer,
                            ui_button_ptr->text_index,
                            "");

    ral_context_delete_objects(ui_button_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&ui_button_ptr->program) );

    if (ui_button_ptr->cpu_present_task != nullptr)
    {
        ral_present_task_release(ui_button_ptr->cpu_present_task);

        ui_button_ptr->cpu_present_task = nullptr;
    }

    if (ui_button_ptr->gfx_state != nullptr)
    {
        ral_gfx_state_release(ui_button_ptr->gfx_state);

        ui_button_ptr->gfx_state = nullptr;
    }

    if (ui_button_ptr->last_present_task != nullptr)
    {
        ral_present_task_release(ui_button_ptr->last_present_task);

        ui_button_ptr->last_present_task = nullptr;
    }

    if (ui_button_ptr->program_ub_fs != nullptr)
    {
        ral_program_block_buffer_release(ui_button_ptr->program_ub_fs);

        ui_button_ptr->program_ub_fs = nullptr;
    }

    if (ui_button_ptr->program_ub_vs != nullptr)
    {
        ral_program_block_buffer_release(ui_button_ptr->program_ub_vs);

        ui_button_ptr->program_ub_vs = nullptr;
    }

    delete ui_button_ptr;
}

/** Please see header for specification */
PUBLIC ral_present_task ui_button_get_present_task(void*            internal_instance,
                                                   ral_texture_view target_texture_view)
{
    _ui_button*      button_ptr = reinterpret_cast<_ui_button*>(internal_instance);
    ral_present_task result     = nullptr;
    ral_buffer       ub_fs_bo;
    ral_buffer       ub_vs_bo;

    /* Can we re-use the last present task we created? */
    if (button_ptr->last_present_task                     != nullptr             &&
        button_ptr->last_present_task_target_texture_view == target_texture_view)
    {
        result = button_ptr->last_present_task;
    }
    else
    {
        uint32_t target_texture_view_height;
        uint32_t target_texture_view_width;

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

        /* What about the gfx state? */
        if (button_ptr->gfx_state != nullptr)
        {
            ral_command_buffer_set_viewport_command_info* viewports;

            ral_gfx_state_get_property(button_ptr->gfx_state,
                                       RAL_GFX_STATE_PROPERTY_STATIC_VIEWPORTS,
                                      &viewports);

            if (!(fabs(viewports[0].size[0] - target_texture_view_width)  < 1e-5f &&
                  fabs(viewports[0].size[1] - target_texture_view_height) < 1e-5f))
            {
                ral_gfx_state_release(button_ptr->gfx_state);

                button_ptr->gfx_state = nullptr;
            }
        }

        if (button_ptr->last_present_task != nullptr)
        {
            ral_present_task_release(button_ptr->last_present_task);

            button_ptr->last_present_task = nullptr;
        }

        ral_program_block_buffer_get_property(button_ptr->program_ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_fs_bo);
        ral_program_block_buffer_get_property(button_ptr->program_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &ub_vs_bo);

        /* Set up a GFX state instance, if needed */
        if (button_ptr->gfx_state == nullptr)
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

            button_ptr->gfx_state = ral_gfx_state_create(button_ptr->context,
                                                        &gfx_state_create_info);
        }

        /* Bake the draw command buffer */
        ral_command_buffer draw_command_buffer;

        if (button_ptr->cached_command_buffer == nullptr)
        {
            ral_command_buffer_create_info draw_command_buffer_create_info;

            draw_command_buffer_create_info.compatible_queues                       = RAL_QUEUE_GRAPHICS_BIT;
            draw_command_buffer_create_info.is_invokable_from_other_command_buffers = false;
            draw_command_buffer_create_info.is_resettable                           = true;
            draw_command_buffer_create_info.is_transient                            = false;

            ral_context_create_command_buffers(button_ptr->context,
                                               1, /* n_command_buffers */
                                               &draw_command_buffer_create_info,
                                               &button_ptr->cached_command_buffer);
        }

        draw_command_buffer = button_ptr->cached_command_buffer;

        ral_command_buffer_start_recording(draw_command_buffer);
        {
            ral_command_buffer_draw_call_regular_command_info      draw_call_command_info;
            ral_command_buffer_set_color_rendertarget_command_info set_color_rendertarget_command_info = ral_command_buffer_set_color_rendertarget_command_info::get_preinitialized_instance();
            ral_command_buffer_set_binding_command_info            set_ub_binding_command_info_items[2];

            set_color_rendertarget_command_info.rendertarget_index = 0;
            set_color_rendertarget_command_info.texture_view       = target_texture_view;

            set_ub_binding_command_info_items[0].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            set_ub_binding_command_info_items[0].name                          = system_hashed_ansi_string_create("dataFS");
            set_ub_binding_command_info_items[0].uniform_buffer_binding.buffer = ub_fs_bo;
            set_ub_binding_command_info_items[0].uniform_buffer_binding.offset = 0;
            set_ub_binding_command_info_items[0].uniform_buffer_binding.size   = button_ptr->program_ub_fs_bo_size;

            set_ub_binding_command_info_items[1].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            set_ub_binding_command_info_items[1].name                          = system_hashed_ansi_string_create("dataVS");
            set_ub_binding_command_info_items[1].uniform_buffer_binding.buffer = ub_vs_bo;
            set_ub_binding_command_info_items[1].uniform_buffer_binding.offset = 0;
            set_ub_binding_command_info_items[1].uniform_buffer_binding.size   = button_ptr->program_ub_vs_bo_size;

            ral_command_buffer_record_set_program            (draw_command_buffer,
                                                              button_ptr->program);
            ral_command_buffer_record_set_bindings           (draw_command_buffer,
                                                              sizeof(set_ub_binding_command_info_items) / sizeof(set_ub_binding_command_info_items[0]),
                                                              set_ub_binding_command_info_items);
            ral_command_buffer_record_set_color_rendertargets(draw_command_buffer,
                                                              1, /* n_rendertargets */
                                                             &set_color_rendertarget_command_info);
            ral_command_buffer_record_set_gfx_state          (draw_command_buffer,
                                                              button_ptr->gfx_state);

            draw_call_command_info.base_instance = 0;
            draw_call_command_info.base_vertex   = 0;
            draw_call_command_info.n_instances   = 1;
            draw_call_command_info.n_vertices    = 4;

            ral_command_buffer_record_draw_call_regular(draw_command_buffer,
                                                        1, /* n_draw_calls */
                                                       &draw_call_command_info);
        }
        ral_command_buffer_stop_recording(draw_command_buffer);

        /* Bake the present task */
        ral_present_task                    draw_present_task;
        ral_present_task_gpu_create_info    draw_present_task_create_info;
        ral_present_task_io                 draw_present_task_inputs[3];
        ral_present_task_io                 draw_present_task_output;
        ral_present_task                    present_tasks[2];
        ral_present_task                    result_present_task;
        ral_present_task_group_create_info  result_present_task_create_info;
        ral_present_task_ingroup_connection result_present_task_ingroup_connections[2];
        ral_present_task_group_mapping      result_present_task_input_mapping;
        ral_present_task_group_mapping      result_present_task_output_mapping;

        draw_present_task_inputs[0].buffer      = ub_fs_bo;
        draw_present_task_inputs[0].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
        draw_present_task_inputs[1].buffer      = ub_vs_bo;
        draw_present_task_inputs[1].object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
        draw_present_task_inputs[2].object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        draw_present_task_inputs[2].texture_view = target_texture_view;

        draw_present_task_output.object_type  = RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW;
        draw_present_task_output.texture_view = target_texture_view;

        draw_present_task_create_info.command_buffer   = draw_command_buffer;
        draw_present_task_create_info.n_unique_inputs  = sizeof(draw_present_task_inputs) / sizeof(draw_present_task_inputs[0]);
        draw_present_task_create_info.n_unique_outputs = 1;
        draw_present_task_create_info.unique_inputs    = draw_present_task_inputs;
        draw_present_task_create_info.unique_outputs   = &draw_present_task_output;

        draw_present_task = ral_present_task_create_gpu(system_hashed_ansi_string_create("UI button: draw"),
                                                       &draw_present_task_create_info);


        present_tasks[0] = button_ptr->cpu_present_task;
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
        result_present_task_input_mapping.present_task_io_index = 2; /* target_texture_view */
        result_present_task_input_mapping.n_present_task        = 1;

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

        result_present_task = ral_present_task_create_group(system_hashed_ansi_string_create("UI button: rasterization"),
                                                            &result_present_task_create_info);


        button_ptr->last_present_task                     = result_present_task;
        button_ptr->last_present_task_target_texture_view = target_texture_view;

        ral_present_task_release(draw_present_task);
    }

    /* The caller will release the present task after consumption. Make use it does not cause
     * actual object destruction */
    ral_present_task_retain(result);

    return result;
}

/** Please see header for specification */
PUBLIC void ui_button_get_property(const void*         button,
                                   ui_control_property property,
                                   void*               out_result)
{
    const _ui_button* button_ptr = reinterpret_cast<const _ui_button*>(button);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_BUTTON_HEIGHT_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1];

            break;
        }

        case UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = button_ptr->visible;

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
PUBLIC void* ui_button_init(ui                        instance,
                            varia_text_renderer       text_renderer,
                            system_hashed_ansi_string name,
                            const float*              x1y1,
                            const float*              x2y2,
                            PFNUIFIREPROCPTR          pfn_fire_proc_ptr,
                            void*                     fire_proc_user_arg)
{
    _ui_button* new_button_ptr = new (std::nothrow) _ui_button;

    ASSERT_ALWAYS_SYNC(new_button_ptr != nullptr,
                       "Out of memory");

    if (new_button_ptr != nullptr)
    {
        _ui_button_init(new_button_ptr,
                        instance,
                        text_renderer,
                        name,
                        x1y1,
                        x2y2,
                        pfn_fire_proc_ptr,
                        fire_proc_user_arg);
    }

    return (void*) new_button_ptr;
}

/** Please see header for specification */
PUBLIC bool ui_button_is_over(void*        internal_instance,
                              const float* xy)
{
    _ui_button* button_ptr = reinterpret_cast<_ui_button*>(internal_instance);
    float       inversed_y = 1.0f - xy[1];

    if (xy[0]      >= button_ptr->x1y1x2y2[0] && xy[0]      <= button_ptr->x1y1x2y2[2] &&
        inversed_y >= button_ptr->x1y1x2y2[1] && inversed_y <= button_ptr->x1y1x2y2[3])
    {
        if (!button_ptr->is_hovering)
        {
            button_ptr->start_hovering_brightness = button_ptr->current_gpu_brightness_level;
            button_ptr->start_hovering_time       = system_time_now();
            button_ptr->is_hovering               = true;
        }

        return true;
    }
    else
    {
        if (button_ptr->is_hovering)
        {
            button_ptr->is_hovering               = false;
            button_ptr->start_hovering_brightness = button_ptr->current_gpu_brightness_level;
            button_ptr->start_hovering_time       = system_time_now();
        }

        return false;
    }
}

/** Please see header for specification */
PUBLIC void ui_button_on_lbm_down(void*        internal_instance,
                                  const float* xy)
{
    _ui_button* button_ptr = reinterpret_cast<_ui_button*>(internal_instance);
    float       inversed_y = 1.0f - xy[1];

    if (xy[0]      >= button_ptr->x1y1x2y2[0] && xy[0]      <= button_ptr->x1y1x2y2[2] &&
        inversed_y >= button_ptr->x1y1x2y2[1] && inversed_y <= button_ptr->x1y1x2y2[3])
    {
        button_ptr->is_lbm_on                   = true;
        button_ptr->force_gpu_brightness_update = true;
    }
}

/** Please see header for specification */
PUBLIC void ui_button_on_lbm_up(void*        internal_instance,
                                const float* xy)
{
    _ui_button* button_ptr = reinterpret_cast<_ui_button*>(internal_instance);

    button_ptr->is_lbm_on                   = false;
    button_ptr->force_gpu_brightness_update = true;

    if (ui_button_is_over(internal_instance,
                          xy)                     &&
        button_ptr->pfn_fire_proc_ptr != nullptr)
    {
        system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                   (PFNSYSTEMTHREADPOOLCALLBACKPROC) button_ptr->pfn_fire_proc_ptr,
                                                                                   button_ptr->fire_proc_user_arg);

        system_thread_pool_submit_single_task(task);
    }
}

/** Please see header for specification */
PUBLIC void ui_button_set_property(void*               button,
                                   ui_control_property property,
                                   const void*         data)
{
    _ui_button* button_ptr = reinterpret_cast<_ui_button*>(button);

    switch (property)
    {
        case UI_CONTROL_PROPERTY_BUTTON_X1Y1:
        {
            float dx = button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0];
            float dy = button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1];

            memcpy(button_ptr->x1y1x2y2,
                   data,
                   sizeof(float) * 2);

            button_ptr->x1y1x2y2[2] = button_ptr->x1y1x2y2[0] + dx;
            button_ptr->x1y1x2y2[3] = button_ptr->x1y1x2y2[1] + dy;

            button_ptr->x1y1x2y2[1] = 1.0f - button_ptr->x1y1x2y2[1];
            button_ptr->x1y1x2y2[3] = 1.0f - button_ptr->x1y1x2y2[3];

            button_ptr->should_update_border_width = true;

            _ui_button_update_text_location(button_ptr);

            break;
        }

        case UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2:
        {
            memcpy(button_ptr->x1y1x2y2,
                   data,
                   sizeof(float) * 4);

            button_ptr->x1y1x2y2[1]                = 1.0f - button_ptr->x1y1x2y2[1];
            button_ptr->x1y1x2y2[3]                = 1.0f - button_ptr->x1y1x2y2[3];
            button_ptr->should_update_border_width = true;

            _ui_button_update_text_location(button_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    }
}