/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
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

    ral_context              context;
    ral_program              program;
    GLint                    program_border_width_ub_offset;
    GLint                    program_brightness_ub_offset;
    GLint                    program_stop_data_ub_offset;
    ral_program_block_buffer program_ub_fs;
    GLuint                   program_ub_fs_bo_size;
    uint32_t                 program_ub_fs_bp;
    ral_program_block_buffer program_ub_vs;
    GLuint                   program_ub_vs_bo_size;
    uint32_t                 program_ub_vs_bp;
    GLint                    program_x1y1x2y2_ub_offset;

    GLint               text_index;
    varia_text_renderer text_renderer;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ui_button;

/** Internal variables */
static const char* ui_button_fragment_shader_body = "#version 430 core\n"
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

/** TODO */
PRIVATE void _ui_button_init_program(ui          ui_instance,
                                     _ui_button* button_ptr)
{
    /* Create all objects */
    ral_context context = ui_get_context(ui_instance);
    ral_shader  fs      = NULL;
    ral_program program = NULL;
    ral_shader  vs      = NULL;

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
                                   fs) ||
        !ral_program_attach_shader(program,
                                   vs) )
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
                               (const void**) shaders_to_delete);
}

/** TODO */
PRIVATE void _ui_button_init_renderer_callback(ogl_context context,
                                               void*       button)
{
    _ui_button*   button_ptr      = (_ui_button*) button;
    raGL_program  program_raGL    = NULL;
    GLuint        program_raGL_id = 0;
    const GLfloat stop_data[]     = {0,     174.0f / 255.0f * 0.5f, 188.0f / 255.0f * 0.5f, 191.0f / 255.0f * 0.5f,
                                     0.5f,  110.0f / 255.0f * 0.5f, 119.0f / 255.0f * 0.5f, 116.0f / 255.0f * 0.5f,
                                     0.51f, 10.0f  / 255.0f * 0.5f, 14.0f  / 255.0f * 0.5f, 10.0f  / 255.0f * 0.5f,
                                     1.0f,  10.0f  / 255.0f * 0.5f, 8.0f   / 255.0f * 0.5f, 9.0f   / 255.0f * 0.5f};
    system_window window          = NULL;

    ral_context_get_property(button_ptr->context,
                             RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                            &window);

    program_raGL = ral_context_get_program_gl(button_ptr->context,
                                              button_ptr->program);

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    raGL_program_get_block_property_by_name(program_raGL,
                                            system_hashed_ansi_string_create("dataFS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &button_ptr->program_ub_fs_bp);
    raGL_program_get_block_property_by_name(program_raGL,
                                            system_hashed_ansi_string_create("dataFV"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &button_ptr->program_ub_vs_bp);

    ASSERT_DEBUG_SYNC(button_ptr->program_ub_fs_bp != button_ptr->program_ub_vs_bp,
                      "BP match detected");

    /* Retrieve uniform UB offsets */
    const ral_program_variable* border_width_uniform_ral_ptr = NULL;
    const ral_program_variable* brightness_uniform_ral_ptr   = NULL;
    const ral_program_variable* stop_data_uniform_ral_ptr    = NULL;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = NULL;

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
}

/** TODO */
PRIVATE void _ui_button_update_text_location(_ui_button* button_ptr)
{
    int           text_height   = 0;
    int           text_xy[2]    = {0};
    int           text_width    = 0;
    system_window window        = NULL;
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
    _ui_button* ui_button_ptr = (_ui_button*) internal_instance;

    varia_text_renderer_set(ui_button_ptr->text_renderer,
                            ui_button_ptr->text_index,
                            "");

    ral_context_delete_objects(ui_button_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               1, /* n_objects */
                               (const void**) &ui_button_ptr->program);

    if (ui_button_ptr->program_ub_fs != NULL)
    {
        ral_program_block_buffer_release(ui_button_ptr->program_ub_fs);

        ui_button_ptr->program_ub_fs = NULL;
    }

    if (ui_button_ptr->program_ub_vs != NULL)
    {
        ral_program_block_buffer_release(ui_button_ptr->program_ub_vs);

        ui_button_ptr->program_ub_vs = NULL;
    }

    delete ui_button_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ui_button_draw(void* internal_instance)
{
    _ui_button* button_ptr  = (_ui_button*) internal_instance;
    system_time time_now    = system_time_now();

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
            } /* if (brightness > FOCUSED_BRIGHTNESS) */
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = FOCUSED_BRIGHTNESS;
        }
    } /* if (button_ptr->is_hovering) */
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
            } /* if (brightness < 0) */
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = NONFOCUSED_BRIGHTNESS;
        }
    }

    /* Update uniforms */
    const float  new_brightness  = brightness * (button_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1);
    raGL_program program_raGL    = ral_context_get_program_gl(button_ptr->context,
                                                              button_ptr->program);
    uint32_t     program_raGL_id = 0;

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

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
        system_window window          = NULL;
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

    /* Draw */
    GLuint      program_ub_fs_bo_id           = 0;
    raGL_buffer program_ub_fs_bo_raGL         = NULL;
    ral_buffer  program_ub_fs_bo_ral          = NULL;
    uint32_t    program_ub_fs_bo_start_offset = -1;
    GLuint      program_ub_vs_bo_id           = 0;
    raGL_buffer program_ub_vs_bo_raGL         = NULL;
    ral_buffer  program_ub_vs_bo_ral          = NULL;
    uint32_t    program_ub_vs_bo_start_offset = -1;

    ral_program_block_buffer_get_property(button_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(button_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_vs_bo_ral);

    program_ub_fs_bo_raGL = ral_context_get_buffer_gl(button_ptr->context,
                                                      program_ub_fs_bo_ral);
    program_ub_vs_bo_raGL = ral_context_get_buffer_gl(button_ptr->context,
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

    button_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                   button_ptr->program_ub_fs_bp,
                                   program_ub_fs_bo_id,
                                   program_ub_fs_bo_start_offset,
                                   button_ptr->program_ub_fs_bo_size);
    button_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                   button_ptr->program_ub_vs_bp,
                                   program_ub_vs_bo_id,
                                   program_ub_vs_bo_start_offset,
                                   button_ptr->program_ub_vs_bo_size);

    ral_program_block_buffer_sync(button_ptr->program_ub_fs);
    ral_program_block_buffer_sync(button_ptr->program_ub_vs);

    button_ptr->pGLUseProgram(program_raGL_id);
    button_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                              0,
                              4);
}

/** Please see header for specification */
PUBLIC void ui_button_get_property(const void*         button,
                                   ui_control_property property,
                                   void*               out_result)
{
    const _ui_button* button_ptr = (const _ui_button*) button;

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
    } /* switch (property_value) */
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

    ASSERT_ALWAYS_SYNC(new_button_ptr != NULL,
                       "Out of memory");

    if (new_button_ptr != NULL)
    {
        /* Initialize fields */
        memset(new_button_ptr,
               0,
               sizeof(_ui_button) );

        new_button_ptr->should_update_border_width = true;
        new_button_ptr->x1y1x2y2[0]                =     x1y1[0];
        new_button_ptr->x1y1x2y2[1]                = 1 - x2y2[1];
        new_button_ptr->x1y1x2y2[2]                =     x2y2[0];
        new_button_ptr->x1y1x2y2[3]                = 1 - x1y1[1];

        new_button_ptr->context            = ui_get_context(instance);
        new_button_ptr->fire_proc_user_arg = fire_proc_user_arg;
        new_button_ptr->pfn_fire_proc_ptr  = pfn_fire_proc_ptr;
        new_button_ptr->text_renderer      = text_renderer;
        new_button_ptr->text_index         = varia_text_renderer_add_string(text_renderer);
        new_button_ptr->visible            = true;

        /* Cache GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(new_button_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_button_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_button_ptr->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_button_ptr->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_button_ptr->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_button_ptr->pGLUseProgram          = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backend type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_button_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_button_ptr->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_button_ptr->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_button_ptr->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_button_ptr->pGLUseProgram          = entry_points->pGLUseProgram;
        }

        /* Configure the text to be shown on the button */
        varia_text_renderer_set(new_button_ptr->text_renderer,
                                new_button_ptr->text_index,
                                system_hashed_ansi_string_get_buffer(name) );

        _ui_button_update_text_location(new_button_ptr);

        varia_text_renderer_set_text_string_property(new_button_ptr->text_renderer,
                                                     new_button_ptr->text_index,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                     _ui_button_text_color);

        /* Retrieve the rendering program */
        new_button_ptr->program = ui_get_registered_program(instance,
                                                            ui_button_program_name);

        if (new_button_ptr->program == NULL)
        {
            _ui_button_init_program(instance,
                                    new_button_ptr);

            ASSERT_DEBUG_SYNC(new_button_ptr->program != NULL,
                              "Could not initialize button UI program");
        } /* if (new_button->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_button_ptr->context),
                                                         _ui_button_init_renderer_callback,
                                                         new_button_ptr);
    } /* if (new_button != NULL) */

    return (void*) new_button_ptr;
}

/** Please see header for specification */
PUBLIC bool ui_button_is_over(void* internal_instance, const float* xy)
{
    _ui_button* button_ptr = (_ui_button*) internal_instance;
    float       inversed_y = 1.0f - xy[1];

    if (xy[0]      >= button_ptr->x1y1x2y2[0] && xy[0]      <= button_ptr->x1y1x2y2[2] &&
        inversed_y >= button_ptr->x1y1x2y2[1] && inversed_y <= button_ptr->x1y1x2y2[3])
    {
        if (!button_ptr->is_hovering)
        {
            button_ptr->start_hovering_brightness = button_ptr->current_gpu_brightness_level;
            button_ptr->start_hovering_time       = system_time_now();
            button_ptr->is_hovering               = true;
        } /* if (!button_ptr->is_hovering) */

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
PUBLIC void ui_button_on_lbm_down(void* internal_instance, const float* xy)
{
    _ui_button* button_ptr = (_ui_button*) internal_instance;
    float       inversed_y = 1.0f - xy[1];

    if (xy[0]      >= button_ptr->x1y1x2y2[0] && xy[0]      <= button_ptr->x1y1x2y2[2] &&
        inversed_y >= button_ptr->x1y1x2y2[1] && inversed_y <= button_ptr->x1y1x2y2[3])
    {
        button_ptr->is_lbm_on                   = true;
        button_ptr->force_gpu_brightness_update = true;
    }
}

/** Please see header for specification */
PUBLIC void ui_button_on_lbm_up(void* internal_instance, const float* xy)
{
    _ui_button* button_ptr = (_ui_button*) internal_instance;

    button_ptr->is_lbm_on                   = false;
    button_ptr->force_gpu_brightness_update = true;

    if (ui_button_is_over(internal_instance, xy) &&
        button_ptr->pfn_fire_proc_ptr != NULL)
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
    _ui_button* button_ptr = (_ui_button*) button;

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
    } /* switch (property_value) */
}