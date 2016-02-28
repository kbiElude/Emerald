/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_scrollbar.h"
#include "ogl/ogl_ui_shared.h"
#include "raGL/raGL_buffer.h"
#include "raGL/raGL_program.h"
#include "raGL/raGL_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"

/** Internal constants */
static system_hashed_ansi_string _ui_scrollbar_slider_program_name = system_hashed_ansi_string_create("UI Scrollbar Slider");
const  float                     _ui_scrollbar_text_color[]        = {1, 1, 1, 1.0f};

/** Internal definitions */
#define UB_DATAFS_BP (0)
#define UB_DATAVS_BP (1)

#define SLIDER_SIZE_PX               (16)
#define TEXT_SCROLLBAR_SEPARATION_PX (10)

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(450) )

/* Loosely based around concepts described in http://thndl.com/?5 */
static const char* ui_scrollbar_fragment_shader_body = "#version 430 core\n"
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
    ral_context                    context;
    system_hashed_ansi_string      name;

    uint32_t                       text_index;
    ogl_ui_scrollbar_text_location text_location;
    ogl_text                       text_renderer;
    ogl_ui                         ui;

    ral_program                    program_slider;
    GLint                          program_slider_border_width_ub_offset;
    GLint                          program_slider_brightness_ub_offset;
    GLint                          program_slider_is_handle_ub_offset;
    GLint                          program_slider_x1y1x2y2_ub_offset;
    ral_program_block_buffer       program_slider_ub_fs;
    GLuint                         program_slider_ub_fs_bo_size;
    ral_program_block_buffer       program_slider_ub_vs;
    GLuint                         program_slider_ub_vs_bo_size;

    system_variant                 min_value_variant;
    system_variant                 max_value_variant;
    system_variant                 temp_variant;

    float                          current_gpu_brightness_level;
    bool                           force_gpu_brightness_update;
    bool                           is_hovering;
    bool                           is_lbm_on;
    bool                           is_visible;
    float                          start_hovering_brightness;
    system_time                    start_hovering_time;

    float                          gpu_slider_handle_position;
    float                          slider_handle_size    [2]; /* scaled to window size */
    float                          slider_handle_x1y1x2y2[4];

    float                          slider_border_width       [2];
    float                          slider_handle_border_width[2];
    float                          slider_x1y1x2y2           [4];

    PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr;
    void*                          get_current_value_ptr_user_arg;
    PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr;
    void*                          set_current_value_ptr_user_arg;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLBLENDEQUATIONPROC       pGLBlendEquation;
    PFNGLBLENDFUNCPROC           pGLBlendFunc;
    PFNGLDISABLEPROC             pGLDisable;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLENABLEPROC              pGLEnable;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ogl_ui_scrollbar;

/* Forward declarations */
PRIVATE void _ogl_ui_scrollbar_update_slider_handle_position(_ogl_ui_scrollbar* scrollbar_ptr);


/** TODO */
PRIVATE void _ogl_ui_scrollbar_init_program(ogl_ui             ui,
                                            _ogl_ui_scrollbar* scrollbar_ptr)
{
    /* Create all objects */
    ral_context context = ogl_ui_get_context(ui);
    ral_shader  fs      = NULL;
    ral_shader  vs      = NULL;

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
                                   fs) ||
        !ral_program_attach_shader(scrollbar_ptr->program_slider,
                                   vs))
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }

    /* Register the prgoram with UI so following button instances will reuse the program */
    ogl_ui_register_program(ui,
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
PRIVATE void _ogl_ui_scrollbar_init_renderer_callback(ogl_context context,
                                                      void*       scrollbar)
{
    float              border_width[2] = {0};
    _ogl_ui_scrollbar* scrollbar_ptr   = (_ogl_ui_scrollbar*) scrollbar;
    system_window      window          = NULL;
    int                window_size[2]  = {0};

    const raGL_program program_raGL    = ral_context_get_program_gl(scrollbar_ptr->context,
                                                                    scrollbar_ptr->program_slider);
    GLuint             program_raGL_id = 0;
    
    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    /* Retrieve uniform block descriptor */
    ral_buffer   datafs_ub_bo_ral = NULL;
    unsigned int datafs_ub_index  = -1;
    ral_buffer   datavs_ub_bo_ral = NULL;
    unsigned int datavs_ub_index  = -1;

    scrollbar_ptr->program_slider_ub_fs = ral_program_block_buffer_create(scrollbar_ptr->context,
                                                                          scrollbar_ptr->program_slider,
                                                                          system_hashed_ansi_string_create("dataFS") );
    scrollbar_ptr->program_slider_ub_vs = ral_program_block_buffer_create(scrollbar_ptr->context,
                                                                          scrollbar_ptr->program_slider,
                                                                          system_hashed_ansi_string_create("dataVS") );

    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datafs_ub_bo_ral);
    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &datavs_ub_bo_ral);

    ral_buffer_get_property(datafs_ub_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &scrollbar_ptr->program_slider_ub_fs_bo_size);
    ral_buffer_get_property(datavs_ub_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &scrollbar_ptr->program_slider_ub_vs_bo_size);

    const uint32_t ub_datafs_bp = UB_DATAFS_BP;
    const uint32_t ub_datavs_bp = UB_DATAVS_BP;

    raGL_program_set_block_property_by_name(program_raGL,
                                            system_hashed_ansi_string_create("dataFS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &ub_datafs_bp);
    raGL_program_set_block_property_by_name(program_raGL,
                                            system_hashed_ansi_string_create("dataVS"),
                                            RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                           &ub_datavs_bp);

    /* Retrieve uniform locations */
    const ral_program_variable* border_width_uniform_ral_ptr = NULL;
    const ral_program_variable* brightness_uniform_ral_ptr   = NULL;
    const ral_program_variable* is_handle_uniform_ral_ptr    = NULL;
    const ral_program_variable* x1y1x2y2_uniform_ral_ptr     = NULL;

    ral_program_get_block_variable_by_name(scrollbar_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(scrollbar_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("brightness"),
                                          &brightness_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(scrollbar_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("is_handle"),
                                          &is_handle_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(scrollbar_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_uniform_ral_ptr);

    scrollbar_ptr->program_slider_border_width_ub_offset = (border_width_uniform_ral_ptr != NULL ? border_width_uniform_ral_ptr->block_offset : -1);
    scrollbar_ptr->program_slider_brightness_ub_offset   = (brightness_uniform_ral_ptr   != NULL ? brightness_uniform_ral_ptr->block_offset   : -1);
    scrollbar_ptr->program_slider_is_handle_ub_offset    = (is_handle_uniform_ral_ptr    != NULL ? is_handle_uniform_ral_ptr->block_offset    : -1);
    scrollbar_ptr->program_slider_x1y1x2y2_ub_offset     = (x1y1x2y2_uniform_ral_ptr     != NULL ? x1y1x2y2_uniform_ral_ptr->block_offset     : -1);

    /* Set general uniforms */
    ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                           scrollbar_ptr->program_slider_brightness_ub_offset,
                                                          &scrollbar_ptr->current_gpu_brightness_level,
                                                           sizeof(float) );
}

/** TODO */
PRIVATE void _ogl_ui_scrollbar_update_slider_handle_position(_ogl_ui_scrollbar* scrollbar_ptr)
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
PRIVATE void _ogl_ui_scrollbar_update_text_position(_ogl_ui_scrollbar* scrollbar_ptr)
{
    int           text_height    = 0;
    int           text_xy[2]     = {0};
    int           text_width     = 0;
    system_window window         = NULL;
    int           window_size[2] = {0};

    ral_context_get_property  (scrollbar_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    ogl_text_get_text_string_property(scrollbar_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                      scrollbar_ptr->text_index,
                                     &text_height);
    ogl_text_get_text_string_property(scrollbar_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                      scrollbar_ptr->text_index,
                                     &text_width);

    if (scrollbar_ptr->text_location == OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
    {
        text_xy[0] = (int) ((scrollbar_ptr->slider_x1y1x2y2[0] + (scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0] - float(text_width)  / window_size[0])  * 0.5f) * (float) window_size[0]);
        text_xy[1] = (int) ((1.0f - scrollbar_ptr->slider_x1y1x2y2[1] - (float(text_height) / window_size[1]) ) * (float) window_size[1]);
    }
    else
    {
        text_xy[0] = (int) ((scrollbar_ptr->slider_x1y1x2y2[0])        * (float) window_size[0]) - text_width - TEXT_SCROLLBAR_SEPARATION_PX;
        text_xy[1] = (int) ((1.0f - scrollbar_ptr->slider_x1y1x2y2[1] + 0.5f * (float(text_height) / window_size[1] - scrollbar_ptr->slider_x1y1x2y2[3] + scrollbar_ptr->slider_x1y1x2y2[1])) * (float) window_size[1]);
    }

    ogl_text_set_text_string_property(scrollbar_ptr->text_renderer,
                                      scrollbar_ptr->text_index,
                                      OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                      text_xy);
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_deinit(void* internal_instance)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;

    /* Release block buffers */
    if (scrollbar_ptr->program_slider_ub_fs != NULL)
    {
        ral_program_block_buffer_release(scrollbar_ptr->program_slider_ub_fs);

        scrollbar_ptr->program_slider_ub_fs = NULL;
    }

    if (scrollbar_ptr->program_slider_ub_vs != NULL)
    {
        ral_program_block_buffer_release(scrollbar_ptr->program_slider_ub_vs);

        scrollbar_ptr->program_slider_ub_vs = NULL;
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
}

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_scrollbar_draw(void* internal_instance)
{
    float                brightness;
    _ogl_ui_scrollbar*   scrollbar_ptr   = (_ogl_ui_scrollbar*) internal_instance;
    const raGL_program   program_raGL    = ral_context_get_program_gl(scrollbar_ptr->context,
                                                                      scrollbar_ptr->program_slider);
    GLuint               program_raGL_id = 0;
    system_time          time_now        = system_time_now();

    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);

    /* Bail out if invisible */
    if (!scrollbar_ptr->is_visible)
    {
        goto end;
    }

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
            } /* if (brightness < 0) */
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = NONFOCUSED_BRIGHTNESS;
        }
    }

    GLuint      program_slider_ub_fs_bo_id           = 0;
    raGL_buffer program_slider_ub_fs_bo_raGL         = NULL;
    ral_buffer  program_slider_ub_fs_bo_ral          = NULL;
    uint32_t    program_slider_ub_fs_bo_start_offset = -1;
    GLuint      program_slider_ub_vs_bo_id           = 0;
    raGL_buffer program_slider_ub_vs_bo_raGL         = NULL;
    ral_buffer  program_slider_ub_vs_bo_ral          = NULL;
    uint32_t    program_slider_ub_vs_bo_start_offset = -1;

    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_slider_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(scrollbar_ptr->program_slider_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_slider_ub_vs_bo_ral);

    program_slider_ub_fs_bo_raGL = ral_context_get_buffer_gl(scrollbar_ptr->context,
                                                             program_slider_ub_fs_bo_ral);
    program_slider_ub_vs_bo_raGL = ral_context_get_buffer_gl(scrollbar_ptr->context,
                                                             program_slider_ub_vs_bo_ral);

    raGL_buffer_get_property(program_slider_ub_fs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_slider_ub_fs_bo_id);
    raGL_buffer_get_property(program_slider_ub_fs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_slider_ub_fs_bo_start_offset);
    raGL_buffer_get_property(program_slider_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &program_slider_ub_vs_bo_id);
    raGL_buffer_get_property(program_slider_ub_vs_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &program_slider_ub_vs_bo_start_offset);

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

    scrollbar_ptr->pGLDisable        (GL_DEPTH_TEST);
    scrollbar_ptr->pGLEnable         (GL_BLEND);
    scrollbar_ptr->pGLBlendEquation  (GL_FUNC_ADD);
    scrollbar_ptr->pGLBlendFunc      (GL_SRC_ALPHA,
                                      GL_ONE_MINUS_SRC_ALPHA);
    scrollbar_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                      UB_DATAFS_BP,
                                      program_slider_ub_fs_bo_id,
                                      program_slider_ub_fs_bo_start_offset,
                                      scrollbar_ptr->program_slider_ub_fs_bo_size);
    scrollbar_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                      UB_DATAVS_BP,
                                      program_slider_ub_vs_bo_id,
                                      program_slider_ub_vs_bo_start_offset,
                                      scrollbar_ptr->program_slider_ub_vs_bo_size);

    {
        scrollbar_ptr->pGLUseProgram(program_raGL_id);

        /* Draw the slider area */
        static const bool bool_false = 0;
        static const bool bool_true  = 1;

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

        ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_fs);
        ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_vs);

        scrollbar_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */

        /* Draw the handle */
        ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                               scrollbar_ptr->program_slider_border_width_ub_offset,
                                                               scrollbar_ptr->slider_handle_border_width,
                                                               sizeof(float) * 2);
        ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_fs,
                                                               scrollbar_ptr->program_slider_is_handle_ub_offset,
                                                              &bool_true,
                                                               sizeof(bool) );
        ral_program_block_buffer_set_nonarrayed_variable_value(scrollbar_ptr->program_slider_ub_vs,
                                                              scrollbar_ptr->program_slider_x1y1x2y2_ub_offset,
                                                              scrollbar_ptr->slider_handle_x1y1x2y2,
                                                              sizeof(float) * 4);

        ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_fs);
        ral_program_block_buffer_sync(scrollbar_ptr->program_slider_ub_vs);

        scrollbar_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                     0,  /* first */
                                     4); /* count */
    }
    scrollbar_ptr->pGLDisable(GL_BLEND);

end:
    ;
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_get_property(const void*              scrollbar,
                                          _ogl_ui_control_property property,
                                          void*                    out_result)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) scrollbar;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            if (scrollbar_ptr->is_visible)
            {
                float text_height = 0.0f;

                ogl_text_get_text_string_property(scrollbar_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                                  scrollbar_ptr->text_index,
                                                 &text_height);

                *(float*) out_result = scrollbar_ptr->slider_x1y1x2y2[3] -
                                       scrollbar_ptr->slider_x1y1x2y2[1];

                if (scrollbar_ptr->text_location == OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
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

        case OGL_UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED:
        {
            *(float*) out_result = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        {
            ((float*) out_result)[0] = scrollbar_ptr->slider_x1y1x2y2[0];
            ((float*) out_result)[1] = scrollbar_ptr->slider_x1y1x2y2[1];

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        case OGL_UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE:
        {
            *(bool*) out_result = scrollbar_ptr->is_visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_hover(void*        internal_instance,
                                   const float* xy_screen_norm)
{
}

/** TODO */
PUBLIC void* ogl_ui_scrollbar_init(ogl_ui                         instance,
                                   ogl_text                       text_renderer,
                                   ogl_ui_scrollbar_text_location text_location,
                                   system_hashed_ansi_string      name,
                                   system_variant                 min_value,
                                   system_variant                 max_value,
                                   const float*                   x1y1,
                                   const float*                   x2y2,
                                   PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                   void*                          get_current_value_ptr_user_arg,
                                   PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                   void*                          set_current_value_ptr_user_arg)
{
    _ogl_ui_scrollbar* new_scrollbar_ptr = new (std::nothrow) _ogl_ui_scrollbar;

    ASSERT_ALWAYS_SYNC(new_scrollbar_ptr != NULL,
                       "Out of memory");

    if (new_scrollbar_ptr != NULL)
    {
        /* Initialize fields */
        memset(new_scrollbar_ptr,
               0,
               sizeof(_ogl_ui_scrollbar) );

        new_scrollbar_ptr->gpu_slider_handle_position = 0;
        new_scrollbar_ptr->is_visible                 = true;

        new_scrollbar_ptr->slider_x1y1x2y2[0] =     x1y1[0];
        new_scrollbar_ptr->slider_x1y1x2y2[1] = 1 - x2y2[1];
        new_scrollbar_ptr->slider_x1y1x2y2[2] =     x2y2[0];
        new_scrollbar_ptr->slider_x1y1x2y2[3] = 1 - x1y1[1];

        new_scrollbar_ptr->current_gpu_brightness_level   = NONFOCUSED_BRIGHTNESS;
        new_scrollbar_ptr->context                        = ogl_ui_get_context(instance);
        new_scrollbar_ptr->pfn_get_current_value_ptr      = pfn_get_current_value_ptr;
        new_scrollbar_ptr->get_current_value_ptr_user_arg = get_current_value_ptr_user_arg;
        new_scrollbar_ptr->pfn_set_current_value_ptr      = pfn_set_current_value_ptr;
        new_scrollbar_ptr->set_current_value_ptr_user_arg = set_current_value_ptr_user_arg;

        new_scrollbar_ptr->text_renderer                  = text_renderer;
        new_scrollbar_ptr->text_index                     = ogl_text_add_string(text_renderer);
        new_scrollbar_ptr->text_location                  = text_location;
        new_scrollbar_ptr->ui                             = instance;

        new_scrollbar_ptr->max_value_variant              = system_variant_create(system_variant_get_type(max_value) );
        new_scrollbar_ptr->min_value_variant              = system_variant_create(system_variant_get_type(min_value) );
        new_scrollbar_ptr->temp_variant                   = system_variant_create(system_variant_get_type(max_value) );

        system_variant_set(new_scrollbar_ptr->max_value_variant,
                           max_value,
                           false);
        system_variant_set(new_scrollbar_ptr->min_value_variant,
                           min_value,
                           false);

        /* Cache GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(new_scrollbar_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points_ptr = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_scrollbar_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);

            new_scrollbar_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_scrollbar_ptr->pGLBlendEquation       = entry_points_ptr->pGLBlendEquation;
            new_scrollbar_ptr->pGLBlendFunc           = entry_points_ptr->pGLBlendFunc;
            new_scrollbar_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_scrollbar_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_scrollbar_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_scrollbar_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_scrollbar_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backend type");

            const ogl_context_gl_entrypoints* entry_points_ptr = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_scrollbar_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);

            new_scrollbar_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_scrollbar_ptr->pGLBlendEquation       = entry_points_ptr->pGLBlendEquation;
            new_scrollbar_ptr->pGLBlendFunc           = entry_points_ptr->pGLBlendFunc;
            new_scrollbar_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_scrollbar_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_scrollbar_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_scrollbar_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_scrollbar_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }

        /* Configure the text to be shown on the button */
        ogl_text_set(new_scrollbar_ptr->text_renderer,
                     new_scrollbar_ptr->text_index,
                     system_hashed_ansi_string_get_buffer(name) );

        int           text_height    = 0;
        system_window window         = NULL;
        int           window_size[2] = {0};

        ogl_text_get_text_string_property(new_scrollbar_ptr->text_renderer,
                                          OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
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

        ogl_text_set_text_string_property(new_scrollbar_ptr->text_renderer,
                                          new_scrollbar_ptr->text_index,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          _ui_scrollbar_text_color);

        _ogl_ui_scrollbar_update_text_position(new_scrollbar_ptr);

        /* Calculate slider sizes */
        new_scrollbar_ptr->slider_border_width       [0] = 1.0f / (float)((new_scrollbar_ptr->slider_x1y1x2y2[2] - new_scrollbar_ptr->slider_x1y1x2y2[0]) * window_size[0]);
        new_scrollbar_ptr->slider_border_width       [1] = 1.0f / (float)((new_scrollbar_ptr->slider_x1y1x2y2[3] - new_scrollbar_ptr->slider_x1y1x2y2[1]) * window_size[1]);
        new_scrollbar_ptr->slider_handle_border_width[0] = 1.0f / (float)(SLIDER_SIZE_PX);
        new_scrollbar_ptr->slider_handle_border_width[1] = 1.0f / (float)(SLIDER_SIZE_PX);

        /* Calculate slider handle size */
        new_scrollbar_ptr->slider_handle_size[0] = float(SLIDER_SIZE_PX) / float(window_size[0]);
        new_scrollbar_ptr->slider_handle_size[1] = float(SLIDER_SIZE_PX) / float(window_size[1]);

        /* Update slider handle position */
        _ogl_ui_scrollbar_update_slider_handle_position(new_scrollbar_ptr);

        /* Retrieve the rendering program */
        new_scrollbar_ptr->program_slider = ogl_ui_get_registered_program(instance,
                                                                          _ui_scrollbar_slider_program_name);

        if (new_scrollbar_ptr->program_slider == NULL)
        {
            _ogl_ui_scrollbar_init_program(instance,
                                           new_scrollbar_ptr);

            ASSERT_DEBUG_SYNC(new_scrollbar_ptr->program_slider != NULL,
                              "Could not initialize scrollbar slider UI program");
        } /* if (new_scrollbar->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_scrollbar_ptr->context),
                                                         _ogl_ui_scrollbar_init_renderer_callback,
                                                         new_scrollbar_ptr);
    } /* if (new_scrollbar_ptr != NULL) */

    return (void*) new_scrollbar_ptr;
}

/** TODO */
PUBLIC bool ogl_ui_scrollbar_is_over(void*        internal_instance,
                                     const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;
    float              inversed_y = 1.0f - xy[1];

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
        } /* if (!scrollbar_ptr->is_hovering) */

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
PUBLIC void ogl_ui_scrollbar_on_lbm_down(void*        internal_instance,
                                         const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;
    float              inversed_y    = 1.0f - xy[1];

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
PUBLIC void ogl_ui_scrollbar_on_lbm_up(void*        internal_instance,
                                       const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;

    scrollbar_ptr->is_lbm_on                   = false;
    scrollbar_ptr->force_gpu_brightness_update = true;
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_mouse_move(void*        internal_instance,
                                           const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;
    float              x_clicked     = xy[0];

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
        _ogl_ui_scrollbar_update_slider_handle_position(scrollbar_ptr);
    } /* if (scrollbar_ptr->is_lbm_on) */
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_set_property(void*                    scrollbar,
                                          _ogl_ui_control_property property,
                                          const void*              data)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) scrollbar;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        {
            const float dx             = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];
            const float dy             = scrollbar_ptr->slider_x1y1x2y2[3] - scrollbar_ptr->slider_x1y1x2y2[1];
                  float text_height_ss = 0.0f;

            if (scrollbar_ptr->text_location == OGL_UI_SCROLLBAR_TEXT_LOCATION_ABOVE_SLIDER)
            {
                ogl_text_get_text_string_property(scrollbar_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_SS,
                                                  scrollbar_ptr->text_index,
                                                 &text_height_ss);
            }

            scrollbar_ptr->slider_x1y1x2y2[0] =        ((float*) data)[0];
            scrollbar_ptr->slider_x1y1x2y2[1] = 1.0f - ((float*) data)[1] - dy - text_height_ss;
            scrollbar_ptr->slider_x1y1x2y2[2] = scrollbar_ptr->slider_x1y1x2y2[0] + dx;
            scrollbar_ptr->slider_x1y1x2y2[3] = scrollbar_ptr->slider_x1y1x2y2[1] + dy;

            _ogl_ui_scrollbar_update_slider_handle_position(scrollbar_ptr);
            _ogl_ui_scrollbar_update_text_position         (scrollbar_ptr);

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        case OGL_UI_CONTROL_PROPERTY_SCROLLBAR_VISIBLE:
        {
            scrollbar_ptr->is_visible = *(bool*) data;

            ogl_text_set_text_string_property(scrollbar_ptr->text_renderer,
                                              scrollbar_ptr->text_index,
                                              OGL_TEXT_STRING_PROPERTY_VISIBILITY,
                                              data);

            ogl_ui_receive_control_callback(scrollbar_ptr->ui,
                                            (ogl_ui_control) scrollbar_ptr,
                                            OGL_UI_SCROLLBAR_CALLBACK_ID_VISIBILITY_TOGGLE,
                                            NULL); /* callback_user_arg */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property property");
        }
    } /* switch (property_value) */
}