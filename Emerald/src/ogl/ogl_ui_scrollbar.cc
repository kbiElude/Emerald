/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_scrollbar.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_window.h"

/** Internal constants */
static system_hashed_ansi_string _ui_scrollbar_slider_program_name = system_hashed_ansi_string_create("UI Scrollbar Slider");
const  float                     _ui_scrollbar_text_color[]        = {1, 1, 1, 1.0f};

/** Internal definitions */
#define SLIDER_SIZE_PX               (16)
#define TEXT_SCROLLBAR_SEPARATION_PX (10)

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(450) )

/* Loosely based around concepts described in http://thndl.com/?5 */
static const char* ui_scrollbar_fragment_shader_body = "#version 330\n"
                                                       "\n"
                                                       "in      vec2  uv;\n"
                                                       "out     vec4  result;\n"
                                                       "uniform vec2  border_width;\n"
                                                       "uniform float brightness;\n"
                                                       "uniform bool  is_handle;\n"
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
    ogl_context                    context;
    system_hashed_ansi_string      name;

    uint32_t                       text_index;
    ogl_text                       text_renderer;

    ogl_program                    program_slider;
    GLint                          program_slider_border_width_uniform_location;
    GLint                          program_slider_brightness_uniform_location;
    GLint                          program_slider_is_handle_uniform_location;
    GLint                          program_slider_x1y1x2y2_uniform_location;

    system_variant                 min_value_variant;
    system_variant                 max_value_variant;
    system_variant                 temp_variant;

    float                          current_gpu_brightness_level;
    bool                           force_gpu_brightness_update;
    bool                           is_hovering;
    bool                           is_lbm_on;
    float                          start_hovering_brightness;
    system_timeline_time           start_hovering_time;

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
    PFNGLBLENDFUNCPROC         pGLBlendFunc;
    PFNGLDISABLEPROC           pGLDisable;
    PFNGLDRAWARRAYSPROC        pGLDrawArrays;
    PFNGLENABLEPROC            pGLEnable;
    PFNGLPROGRAMUNIFORM1FPROC  pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM1IPROC  pGLProgramUniform1i;
    PFNGLPROGRAMUNIFORM2FVPROC pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM4FVPROC pGLProgramUniform4fv;
    PFNGLUSEPROGRAMPROC        pGLUseProgram;
} _ogl_ui_scrollbar;

/* Forward declarations */
PRIVATE void _ogl_ui_scrollbar_update_slider_handle_position(__in __notnull _ogl_ui_scrollbar* scrollbar_ptr);


/** TODO */
PRIVATE void _ogl_ui_scrollbar_init_program(__in __notnull ogl_ui             ui,
                                            __in __notnull _ogl_ui_scrollbar* scrollbar_ptr)
{
    /* Create all objects */
    ogl_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = ogl_shader_create(context, SHADER_TYPE_FRAGMENT, system_hashed_ansi_string_create("UI scrollbar slider fragment shader") );
    ogl_shader  vertex_shader   = ogl_shader_create(context, SHADER_TYPE_VERTEX,   system_hashed_ansi_string_create("UI scrollbar slider vertex shader") );

    scrollbar_ptr->program_slider = ogl_program_create(context, system_hashed_ansi_string_create("UI scrollbar program") );

    /* Set up shaders */
    ogl_shader_set_body(fragment_shader, system_hashed_ansi_string_create(ui_scrollbar_fragment_shader_body) );
    ogl_shader_set_body(vertex_shader,   system_hashed_ansi_string_create(ui_general_vertex_shader_body) );

    ogl_shader_compile(fragment_shader);
    ogl_shader_compile(vertex_shader);

    /* Set up program object */
    ogl_program_attach_shader(scrollbar_ptr->program_slider, fragment_shader);
    ogl_program_attach_shader(scrollbar_ptr->program_slider, vertex_shader);

    ogl_program_link(scrollbar_ptr->program_slider);

    /* Register the prgoram with UI so following button instances will reuse the program */
    ogl_ui_register_program(ui, _ui_scrollbar_slider_program_name, scrollbar_ptr->program_slider);

    /* Release shaders we will no longer need */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** TODO */
PRIVATE void _ogl_ui_scrollbar_init_renderer_callback(ogl_context context, void* scrollbar)
{
    float              border_width[2] = {0};
    _ogl_ui_scrollbar* scrollbar_ptr   = (_ogl_ui_scrollbar*) scrollbar;
    const GLuint       program_id      = ogl_program_get_id(scrollbar_ptr->program_slider);
    system_window      window          = NULL;
    int                window_height   = 0;
    int                window_width    = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    system_window_get_dimensions(window,
                                &window_width,
                                &window_height);

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* border_width_uniform = NULL;
    const ogl_program_uniform_descriptor* brightness_uniform   = NULL;
    const ogl_program_uniform_descriptor* is_handle_uniform    = NULL;
    const ogl_program_uniform_descriptor* x1y1x2y2_uniform     = NULL;

    ogl_program_get_uniform_by_name(scrollbar_ptr->program_slider, system_hashed_ansi_string_create("border_width"), &border_width_uniform);
    ogl_program_get_uniform_by_name(scrollbar_ptr->program_slider, system_hashed_ansi_string_create("brightness"),   &brightness_uniform);
    ogl_program_get_uniform_by_name(scrollbar_ptr->program_slider, system_hashed_ansi_string_create("is_handle"),    &is_handle_uniform);
    ogl_program_get_uniform_by_name(scrollbar_ptr->program_slider, system_hashed_ansi_string_create("x1y1x2y2"),     &x1y1x2y2_uniform);

    scrollbar_ptr->program_slider_border_width_uniform_location = (border_width_uniform != NULL ? border_width_uniform->location : -1);
    scrollbar_ptr->program_slider_brightness_uniform_location   = (brightness_uniform   != NULL ? brightness_uniform->location   : -1);
    scrollbar_ptr->program_slider_is_handle_uniform_location    = (is_handle_uniform    != NULL ? is_handle_uniform->location    : -1);
    scrollbar_ptr->program_slider_x1y1x2y2_uniform_location     = x1y1x2y2_uniform->location;

    /* Set general uniforms */
    scrollbar_ptr->pGLProgramUniform1f(program_id, scrollbar_ptr->program_slider_brightness_uniform_location, scrollbar_ptr->current_gpu_brightness_level);
}

/** TODO */
PRIVATE void _ogl_ui_scrollbar_update_slider_handle_position(__in __notnull _ogl_ui_scrollbar* scrollbar_ptr)
{
    float       max_value  = 0;
    float       min_value  = 0;
    float       new_value  = 0;
    float       slider_pos = 0;
    const float x_delta    = scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0];
    const float y_delta    = scrollbar_ptr->slider_x1y1x2y2[3] - scrollbar_ptr->slider_x1y1x2y2[1];

    scrollbar_ptr->pfn_get_current_value_ptr(scrollbar_ptr->get_current_value_ptr_user_arg, scrollbar_ptr->temp_variant);
    system_variant_get_float                (scrollbar_ptr->max_value_variant,             &max_value);
    system_variant_get_float                (scrollbar_ptr->min_value_variant,             &min_value);
    system_variant_get_float                (scrollbar_ptr->temp_variant,                  &new_value);

    slider_pos = (new_value - min_value) / (max_value - min_value);

    scrollbar_ptr->slider_handle_x1y1x2y2[0] = scrollbar_ptr->slider_x1y1x2y2[0] + x_delta * slider_pos - (scrollbar_ptr->slider_handle_size[0] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[1] = scrollbar_ptr->slider_x1y1x2y2[1] + y_delta * 0.5f       - (scrollbar_ptr->slider_handle_size[1] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[2] = scrollbar_ptr->slider_x1y1x2y2[0] + x_delta * slider_pos + (scrollbar_ptr->slider_handle_size[0] * 0.5f);
    scrollbar_ptr->slider_handle_x1y1x2y2[3] = scrollbar_ptr->slider_x1y1x2y2[1] + y_delta * 0.5f       + (scrollbar_ptr->slider_handle_size[1] * 0.5f);
}


/** TODO */
PUBLIC void ogl_ui_scrollbar_deinit(void* internal_instance)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;

    /* Release program */
    ogl_program_release(scrollbar_ptr->program_slider);

    /* Release variants */
    system_variant_release(scrollbar_ptr->max_value_variant);
    system_variant_release(scrollbar_ptr->min_value_variant);
    system_variant_release(scrollbar_ptr->temp_variant);

    /* Release context */
    ogl_context_release(scrollbar_ptr->context);
}

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_scrollbar_draw(void* internal_instance)
{
    _ogl_ui_scrollbar*   scrollbar_ptr   = (_ogl_ui_scrollbar*) internal_instance;
    GLuint               program_id      = ogl_program_get_id(scrollbar_ptr->program_slider);
    system_timeline_time time_now        = system_time_now();


    /* Update brightness if necessary */
    float brightness = scrollbar_ptr->current_gpu_brightness_level;
    
    if (scrollbar_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_timeline_time transition_start = scrollbar_ptr->start_hovering_time;
        system_timeline_time transition_end   = scrollbar_ptr->start_hovering_time + NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start && time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) / float(NONFOCUSED_TO_FOCUSED_TRANSITION_TIME);

            brightness = scrollbar_ptr->start_hovering_brightness + dt * (FOCUSED_BRIGHTNESS - NONFOCUSED_BRIGHTNESS);

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
        system_timeline_time transition_start = scrollbar_ptr->start_hovering_time;
        system_timeline_time transition_end   = scrollbar_ptr->start_hovering_time + FOCUSED_TO_NONFOCUSED_TRANSITION_TIME;

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

    if (scrollbar_ptr->current_gpu_brightness_level != brightness || scrollbar_ptr->force_gpu_brightness_update)
    {
        scrollbar_ptr->pGLProgramUniform1f(ogl_program_get_id(scrollbar_ptr->program_slider),
                                           scrollbar_ptr->program_slider_brightness_uniform_location,
                                           brightness * (scrollbar_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1)
                                          );

        scrollbar_ptr->current_gpu_brightness_level = brightness;
        scrollbar_ptr->force_gpu_brightness_update  = false;
    }

    scrollbar_ptr->pGLDisable(GL_DEPTH_TEST);
    scrollbar_ptr->pGLEnable(GL_BLEND);
    scrollbar_ptr->pGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    {
        scrollbar_ptr->pGLUseProgram(ogl_program_get_id(scrollbar_ptr->program_slider) );

        /* Draw the slider area */
        scrollbar_ptr->pGLProgramUniform2fv(program_id, scrollbar_ptr->program_slider_border_width_uniform_location, sizeof(scrollbar_ptr->slider_border_width) / sizeof(float) / 2, scrollbar_ptr->slider_border_width);
        scrollbar_ptr->pGLProgramUniform1i (program_id, scrollbar_ptr->program_slider_is_handle_uniform_location,    0);
        scrollbar_ptr->pGLProgramUniform4fv(program_id, scrollbar_ptr->program_slider_x1y1x2y2_uniform_location,     sizeof(scrollbar_ptr->slider_x1y1x2y2)     / sizeof(float) / 4, scrollbar_ptr->slider_x1y1x2y2);

        scrollbar_ptr->pGLDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        /* Draw the handle */
        scrollbar_ptr->pGLProgramUniform2fv(program_id, scrollbar_ptr->program_slider_border_width_uniform_location, sizeof(scrollbar_ptr->slider_handle_border_width) / sizeof(float) / 2, scrollbar_ptr->slider_handle_border_width);
        scrollbar_ptr->pGLProgramUniform1i (program_id, scrollbar_ptr->program_slider_is_handle_uniform_location,    1);
        scrollbar_ptr->pGLProgramUniform4fv(program_id, scrollbar_ptr->program_slider_x1y1x2y2_uniform_location,     sizeof(scrollbar_ptr->slider_handle_x1y1x2y2) / sizeof(float) / 4, scrollbar_ptr->slider_handle_x1y1x2y2);

        scrollbar_ptr->pGLDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    scrollbar_ptr->pGLDisable(GL_BLEND);
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_hover(void* internal_instance, const float* xy_screen_norm)
{
}

/** TODO */
PUBLIC void* ogl_ui_scrollbar_init(__in           __notnull   ogl_ui                         instance,
                                   __in           __notnull   ogl_text                       text_renderer,
                                   __in           __notnull   system_hashed_ansi_string      name,
                                   __in           __notnull   system_variant                 min_value,
                                   __in           __notnull   system_variant                 max_value,
                                   __in_ecount(2) __notnull   const float*                   x1y1,
                                   __in_ecount(2) __notnull   const float*                   x2y2,
                                   __in           __notnull   PFNOGLUIGETCURRENTVALUEPROCPTR pfn_get_current_value_ptr,
                                   __in           __maybenull void*                          get_current_value_ptr_user_arg,
                                   __in           __notnull   PFNOGLUISETCURRENTVALUEPROCPTR pfn_set_current_value_ptr,
                                   __in           __maybenull void*                          set_current_value_ptr_user_arg)
{
    _ogl_ui_scrollbar* new_scrollbar = new (std::nothrow) _ogl_ui_scrollbar;

    ASSERT_ALWAYS_SYNC(new_scrollbar != NULL, "Out of memory");
    if (new_scrollbar != NULL)
    {
        /* Initialize fields */
        memset(new_scrollbar, 0, sizeof(_ogl_ui_scrollbar) );

        new_scrollbar->gpu_slider_handle_position = 0;
        new_scrollbar->slider_x1y1x2y2[0] =     x1y1[0];
        new_scrollbar->slider_x1y1x2y2[1] = 1 - x2y2[1];
        new_scrollbar->slider_x1y1x2y2[2] =     x2y2[0];
        new_scrollbar->slider_x1y1x2y2[3] = 1 - x1y1[1];

        new_scrollbar->current_gpu_brightness_level   = NONFOCUSED_BRIGHTNESS;
        new_scrollbar->context                        = ogl_ui_get_context(instance);
        new_scrollbar->pfn_get_current_value_ptr      = pfn_get_current_value_ptr;
        new_scrollbar->get_current_value_ptr_user_arg = get_current_value_ptr_user_arg;
        new_scrollbar->pfn_set_current_value_ptr      = pfn_set_current_value_ptr;
        new_scrollbar->set_current_value_ptr_user_arg = set_current_value_ptr_user_arg;

        new_scrollbar->text_renderer                  = text_renderer;
        new_scrollbar->text_index                     = ogl_text_add_string(text_renderer);

        new_scrollbar->max_value_variant              = system_variant_create(system_variant_get_type(max_value) );
        new_scrollbar->min_value_variant              = system_variant_create(system_variant_get_type(min_value) );
        new_scrollbar->temp_variant                   = system_variant_create(system_variant_get_type(max_value) );

        system_variant_set(new_scrollbar->max_value_variant, max_value, false);
        system_variant_set(new_scrollbar->min_value_variant, min_value, false);

        ogl_context_retain(new_scrollbar->context);

        /* Cache GL func pointers */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(new_scrollbar->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_scrollbar->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_scrollbar->pGLBlendFunc         = entry_points->pGLBlendFunc;
            new_scrollbar->pGLDisable           = entry_points->pGLDisable;
            new_scrollbar->pGLDrawArrays        = entry_points->pGLDrawArrays;
            new_scrollbar->pGLEnable            = entry_points->pGLEnable;
            new_scrollbar->pGLProgramUniform1f  = entry_points->pGLProgramUniform1f;
            new_scrollbar->pGLProgramUniform1i  = entry_points->pGLProgramUniform1i;
            new_scrollbar->pGLProgramUniform2fv = entry_points->pGLProgramUniform2fv;
            new_scrollbar->pGLProgramUniform4fv = entry_points->pGLProgramUniform4fv;
            new_scrollbar->pGLUseProgram        = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_scrollbar->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_scrollbar->pGLBlendFunc         = entry_points->pGLBlendFunc;
            new_scrollbar->pGLDisable           = entry_points->pGLDisable;
            new_scrollbar->pGLDrawArrays        = entry_points->pGLDrawArrays;
            new_scrollbar->pGLEnable            = entry_points->pGLEnable;
            new_scrollbar->pGLProgramUniform1f  = entry_points->pGLProgramUniform1f;
            new_scrollbar->pGLProgramUniform1i  = entry_points->pGLProgramUniform1i;
            new_scrollbar->pGLProgramUniform2fv = entry_points->pGLProgramUniform2fv;
            new_scrollbar->pGLProgramUniform4fv = entry_points->pGLProgramUniform4fv;
            new_scrollbar->pGLUseProgram        = entry_points->pGLUseProgram;
        }

        /* Configure the text to be shown on the button */
        ogl_text_set(new_scrollbar->text_renderer, new_scrollbar->text_index, system_hashed_ansi_string_get_buffer(name) );

        int           text_height   = 0;
        int           text_xy[2]    = {0};
        int           text_width    = 0;
        system_window window        = NULL;
        int           window_height = 0;
        int           window_width  = 0;

        ogl_text_get_text_string_property(new_scrollbar->text_renderer,
                                          OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                          new_scrollbar->text_index,
                                         &text_height);
        ogl_text_get_text_string_property(new_scrollbar->text_renderer,
                                          OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                          new_scrollbar->text_index,
                                         &text_width);

        ogl_context_get_property(new_scrollbar->context,
                                 OGL_CONTEXT_PROPERTY_WINDOW,
                                &window);


        system_window_get_dimensions(window,
                                    &window_width,
                                    &window_height);

        text_xy[0]                         = (int) ((x1y1[0] + (x2y2[0] - x1y1[0] - float(text_width)  / window_width)  * 0.5f) * (float) window_width);
        text_xy[1]                         = (int) ((x2y2[1] - (float(text_height) / window_height) ) * (float) window_height);
        new_scrollbar->slider_x1y1x2y2[3] -= float(text_height + TEXT_SCROLLBAR_SEPARATION_PX) / window_height;

        ASSERT_DEBUG_SYNC(new_scrollbar->slider_x1y1x2y2[1] < new_scrollbar->slider_x1y1x2y2[3], "Scrollbar has illegal height");

        ogl_text_set_text_string_property(new_scrollbar->text_renderer,
                                          new_scrollbar->text_index,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          _ui_scrollbar_text_color);
        ogl_text_set_text_string_property(new_scrollbar->text_renderer,
                                          new_scrollbar->text_index,
                                          OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                          text_xy);

        /* Calculate slider sizes */
        new_scrollbar->slider_border_width       [0] = 1.0f / (float)((new_scrollbar->slider_x1y1x2y2[2] - new_scrollbar->slider_x1y1x2y2[0]) * window_width);
        new_scrollbar->slider_border_width       [1] = 1.0f / (float)((new_scrollbar->slider_x1y1x2y2[3] - new_scrollbar->slider_x1y1x2y2[1]) * window_height);
        new_scrollbar->slider_handle_border_width[0] = 1.0f / (float)(SLIDER_SIZE_PX);
        new_scrollbar->slider_handle_border_width[1] = 1.0f / (float)(SLIDER_SIZE_PX);

        /* Calculate slider handle size */
        new_scrollbar->slider_handle_size[0] = float(SLIDER_SIZE_PX) / float(window_width);
        new_scrollbar->slider_handle_size[1] = float(SLIDER_SIZE_PX) / float(window_height);

        /* Update slider handle position */
        _ogl_ui_scrollbar_update_slider_handle_position(new_scrollbar);

        /* Retrieve the rendering program */
        new_scrollbar->program_slider = ogl_ui_get_registered_program(instance, _ui_scrollbar_slider_program_name);

        if (new_scrollbar->program_slider == NULL)
        {
            _ogl_ui_scrollbar_init_program(instance, new_scrollbar);

            ASSERT_DEBUG_SYNC(new_scrollbar->program_slider != NULL, "Could not initialize scrollbar slider UI program");
        } /* if (new_scrollbar->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(new_scrollbar->context,
                                                         _ogl_ui_scrollbar_init_renderer_callback,
                                                         new_scrollbar);
    } /* if (new_button != NULL) */

    return (void*) new_scrollbar;
}

/** TODO */
PUBLIC bool ogl_ui_scrollbar_is_over(void* internal_instance, const float* xy)
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
PUBLIC void ogl_ui_scrollbar_on_lbm_down(void* internal_instance, const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;
    float              inversed_y    = 1.0f - xy[1];

    if (xy[0]      >= scrollbar_ptr->slider_handle_x1y1x2y2[0] && xy[0]      <= scrollbar_ptr->slider_handle_x1y1x2y2[2] &&
        inversed_y >= scrollbar_ptr->slider_handle_x1y1x2y2[1] && inversed_y <= scrollbar_ptr->slider_handle_x1y1x2y2[3])
    {
        scrollbar_ptr->is_lbm_on                   = true;
        scrollbar_ptr->force_gpu_brightness_update = true;
    }
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_lbm_up(void* internal_instance, const float* xy)
{
    _ogl_ui_scrollbar* scrollbar_ptr = (_ogl_ui_scrollbar*) internal_instance;

    scrollbar_ptr->is_lbm_on                   = false;
    scrollbar_ptr->force_gpu_brightness_update = true;
}

/** TODO */
PUBLIC void ogl_ui_scrollbar_on_mouse_move(void* internal_instance, const float* xy)
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

        system_variant_get_float(scrollbar_ptr->max_value_variant, &max_value);
        system_variant_get_float(scrollbar_ptr->min_value_variant, &min_value);

        new_value = min_value + (x_clicked - scrollbar_ptr->slider_x1y1x2y2[0]) / (scrollbar_ptr->slider_x1y1x2y2[2] - scrollbar_ptr->slider_x1y1x2y2[0]) * (max_value - min_value);

        system_variant_set_float                (scrollbar_ptr->temp_variant,                   new_value);
        scrollbar_ptr->pfn_set_current_value_ptr(scrollbar_ptr->set_current_value_ptr_user_arg, scrollbar_ptr->temp_variant);

        /* Update UI */
        _ogl_ui_scrollbar_update_slider_handle_position(scrollbar_ptr);
    } /* if (scrollbar_ptr->is_lbm_on) */
}
