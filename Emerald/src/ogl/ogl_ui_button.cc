/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_button.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(450) )

const float _ui_button_text_color[] = {1, 1, 1, 1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_button_program_name = system_hashed_ansi_string_create("UI Button");

/** Internal types */
typedef struct
{
    float               x1y1x2y2[4];

    void*               fire_proc_user_arg;
    PFNOGLUIFIREPROCPTR pfn_fire_proc_ptr;

    float                current_gpu_brightness_level;
    bool                 force_gpu_brightness_update;
    bool                 is_hovering;
    bool                 is_lbm_on;
    bool                 should_update_border_width;
    float                start_hovering_brightness;
    system_timeline_time start_hovering_time;

    ogl_context context;
    ogl_program program;
    GLint       program_border_width_uniform_location;
    GLint       program_brightness_uniform_location;
    GLint       program_stop_data_uniform_location;
    GLint       program_x1y1x2y2_uniform_location;

    GLint    text_index;
    ogl_text text_renderer;

    /* Cached func ptrs */
    PFNGLDRAWARRAYSPROC        pGLDrawArrays;
    PFNGLPROGRAMUNIFORM1FPROC  pGLProgramUniform1f;
    PFNGLPROGRAMUNIFORM2FVPROC pGLProgramUniform2fv;
    PFNGLPROGRAMUNIFORM4FVPROC pGLProgramUniform4fv;
    PFNGLUSEPROGRAMPROC        pGLUseProgram;
} _ogl_ui_button;

/** Internal variables */
static const char* ui_button_fragment_shader_body = "#version 330\n"
                                                    "\n"
                                                    "in  vec2 uv;\n"
                                                    "out vec3 result;\n"
                                                    /* stop, rgb */
                                                    "uniform float brightness;\n"
                                                    "uniform vec2  border_width;\n"
                                                    "uniform vec4  stop_data[4];\n"
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
PRIVATE void _ogl_ui_button_init_program(__in __notnull ogl_ui          ui,
                                         __in __notnull _ogl_ui_button* button_ptr)
{
    /* Create all objects */
    ogl_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = ogl_shader_create (context,
                                                     SHADER_TYPE_FRAGMENT,
                                                     system_hashed_ansi_string_create("UI button fragment shader") );
    ogl_shader  vertex_shader   = ogl_shader_create (context,
                                                     SHADER_TYPE_VERTEX,
                                                     system_hashed_ansi_string_create("UI button vertex shader") );

    button_ptr->program = ogl_program_create(context,
                                             system_hashed_ansi_string_create("UI button program") );

    /* Set up shaders */
    ogl_shader_set_body(fragment_shader,
                        system_hashed_ansi_string_create(ui_button_fragment_shader_body) );
    ogl_shader_set_body(vertex_shader,
                        system_hashed_ansi_string_create(ui_general_vertex_shader_body) );

    ogl_shader_compile(fragment_shader);
    ogl_shader_compile(vertex_shader);

    /* Set up program object */
    ogl_program_attach_shader(button_ptr->program,
                              fragment_shader);
    ogl_program_attach_shader(button_ptr->program,
                              vertex_shader);

    ogl_program_link(button_ptr->program);

    /* Register the prgoram with UI so following button instances will reuse the program */
    ogl_ui_register_program(ui,
                            ui_button_program_name,
                            button_ptr->program);

    /* Release shaders we will no longer need */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** TODO */
PRIVATE void _ogl_ui_button_init_renderer_callback(ogl_context context, void* button)
{
    _ogl_ui_button* button_ptr  = (_ogl_ui_button*) button;
    const GLuint    program_id  = ogl_program_get_id(button_ptr->program);
    const GLfloat   stop_data[] = {0,     174.0f / 255.0f * 0.5f, 188.0f / 255.0f * 0.5f, 191.0f / 255.0f * 0.5f,
                                   0.5f,  110.0f / 255.0f * 0.5f, 119.0f / 255.0f * 0.5f, 116.0f / 255.0f * 0.5f,
                                   0.51f, 10.0f  / 255.0f * 0.5f, 14.0f  / 255.0f * 0.5f, 10.0f  / 255.0f * 0.5f,
                                   1.0f,  10.0f  / 255.0f * 0.5f, 8.0f   / 255.0f * 0.5f, 9.0f   / 255.0f * 0.5f};
    system_window   window      = NULL;

    ogl_context_get_property  (button_ptr->context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);

    /* Retrieve uniform locations */
    const ogl_program_uniform_descriptor* border_width_uniform = NULL;
    const ogl_program_uniform_descriptor* brightness_uniform   = NULL;
    const ogl_program_uniform_descriptor* stop_data_uniform    = NULL;
    const ogl_program_uniform_descriptor* x1y1x2y2_uniform     = NULL;

    ogl_program_get_uniform_by_name(button_ptr->program,
                                    system_hashed_ansi_string_create("border_width"),
                                   &border_width_uniform);
    ogl_program_get_uniform_by_name(button_ptr->program,
                                    system_hashed_ansi_string_create("brightness"),
                                   &brightness_uniform);
    ogl_program_get_uniform_by_name(button_ptr->program,
                                    system_hashed_ansi_string_create("stop_data[0]"),
                                   &stop_data_uniform);
    ogl_program_get_uniform_by_name(button_ptr->program,
                                    system_hashed_ansi_string_create("x1y1x2y2"),
                                   &x1y1x2y2_uniform);

    button_ptr->program_border_width_uniform_location = border_width_uniform->location;
    button_ptr->program_brightness_uniform_location   = brightness_uniform->location;
    button_ptr->program_stop_data_uniform_location    = stop_data_uniform->location;
    button_ptr->program_x1y1x2y2_uniform_location     = x1y1x2y2_uniform->location;

    /* Set them up */
    button_ptr->pGLProgramUniform4fv(program_id,
                                     stop_data_uniform->location,
                                     sizeof(stop_data) / sizeof(float) / 4,
                                     stop_data);

    button_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;
}

/** TODO */
PRIVATE void _ogl_ui_button_update_text_location(__in __notnull _ogl_ui_button* button_ptr)
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

    ogl_text_get_text_string_property(button_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                      button_ptr->text_index,
                                     &text_height);
    ogl_text_get_text_string_property(button_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                      button_ptr->text_index,
                                     &text_width);

    ogl_context_get_property  (button_ptr->context,
                               OGL_CONTEXT_PROPERTY_WINDOW,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    text_xy[0] = (int) ((x1y1[0] + (x2y2[0] - x1y1[0] - float(text_width)  / window_size[0]) * 0.5f) * (float) window_size[0]);
    text_xy[1] = (int) ((x2y2[1] - (x2y2[1] - x1y1[1] - float(text_height) / window_size[1]) * 0.5f) * (float) window_size[1]);

    ogl_text_set_text_string_property(button_ptr->text_renderer,
                                      button_ptr->text_index,
                                      OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                      text_xy);
}


/** Please see header for specification */
PUBLIC void ogl_ui_button_deinit(void* internal_instance)
{
    _ogl_ui_button* ui_button_ptr = (_ogl_ui_button*) internal_instance;

    ogl_context_release(ui_button_ptr->context);
    ogl_program_release(ui_button_ptr->program);
    ogl_text_set       (ui_button_ptr->text_renderer,
                        ui_button_ptr->text_index,
                        "");

    delete internal_instance;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_button_draw(void* internal_instance)
{
    _ogl_ui_button*      button_ptr  = (_ogl_ui_button*) internal_instance;
    system_timeline_time time_now    = system_time_now();

    /* Update brightness if necessary */
    float brightness = button_ptr->current_gpu_brightness_level;

    if (button_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_timeline_time transition_start = button_ptr->start_hovering_time;
        system_timeline_time transition_end   = button_ptr->start_hovering_time + NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

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
        system_timeline_time transition_start = button_ptr->start_hovering_time;
        system_timeline_time transition_end   = button_ptr->start_hovering_time +
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
    GLuint program_id = ogl_program_get_id(button_ptr->program);

    if (button_ptr->current_gpu_brightness_level != brightness ||
        button_ptr->force_gpu_brightness_update)
    {
        button_ptr->current_gpu_brightness_level = brightness;
        button_ptr->force_gpu_brightness_update  = false;
    }

    button_ptr->pGLProgramUniform1f (program_id,
                                     button_ptr->program_brightness_uniform_location,
                                     brightness * (button_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1) );
    button_ptr->pGLProgramUniform4fv(program_id,
                                     button_ptr->program_x1y1x2y2_uniform_location,
                                     sizeof(button_ptr->x1y1x2y2) / sizeof(float) / 4,
                                     button_ptr->x1y1x2y2);

    if (button_ptr->should_update_border_width)
    {
        float         border_width[2] = {0};
        system_window window          = NULL;
        int           window_size[2];

        ogl_context_get_property    (button_ptr->context,
                                     OGL_CONTEXT_PROPERTY_WINDOW,
                                    &window);
        system_window_get_property  (window,
                                     SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                     window_size);

        border_width[0] = 1.0f / (float)((button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0]) * (window_size[0]) );
        border_width[1] = 1.0f / (float)((button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1]) * (window_size[1]));

        button_ptr->pGLProgramUniform2fv(program_id,
                                         button_ptr->program_border_width_uniform_location,
                                         1,
                                         border_width);

        button_ptr->should_update_border_width = false;
    }

    /* Draw */
    button_ptr->pGLUseProgram(ogl_program_get_id(button_ptr->program) );
    button_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                              0,
                              4);
}

/** Please see header for specification */
PUBLIC void ogl_ui_button_get_property(__in  __notnull const void* button,
                                       __in  __notnull int         property_value,
                                       __out __notnull void*       out_result)
{
    const _ogl_ui_button* button_ptr = (const _ogl_ui_button*) button;

    switch (property_value)
    {
        case OGL_UI_BUTTON_PROPERTY_HEIGHT_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1];

            break;
        }

        case OGL_UI_BUTTON_PROPERTY_WIDTH_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_ui_button property");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_button_init(__in           __notnull   ogl_ui                    instance,
                                __in           __notnull   ogl_text                  text_renderer,
                                __in           __notnull   system_hashed_ansi_string name,
                                __in_ecount(2) __notnull   const float*              x1y1,
                                __in_ecount(2) __notnull   const float*              x2y2,
                                __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_proc_ptr,
                                __in           __maybenull void*                     fire_proc_user_arg)
{
    _ogl_ui_button* new_button = new (std::nothrow) _ogl_ui_button;

    ASSERT_ALWAYS_SYNC(new_button != NULL,
                       "Out of memory");

    if (new_button != NULL)
    {
        /* Initialize fields */
        memset(new_button,
               0,
               sizeof(_ogl_ui_button) );

        new_button->should_update_border_width = true;
        new_button->x1y1x2y2[0]                =     x1y1[0];
        new_button->x1y1x2y2[1]                = 1 - x2y2[1];
        new_button->x1y1x2y2[2]                =     x2y2[0];
        new_button->x1y1x2y2[3]                = 1 - x1y1[1];

        new_button->context            = ogl_ui_get_context(instance);
        new_button->fire_proc_user_arg = fire_proc_user_arg;
        new_button->pfn_fire_proc_ptr  = pfn_fire_proc_ptr;
        new_button->text_renderer      = text_renderer;
        new_button->text_index         = ogl_text_add_string(text_renderer);

        ogl_context_retain(new_button->context);

        /* Cache GL func pointers */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(new_button->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_button->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_button->pGLDrawArrays        = entry_points->pGLDrawArrays;
            new_button->pGLProgramUniform1f  = entry_points->pGLProgramUniform1f;
            new_button->pGLProgramUniform2fv = entry_points->pGLProgramUniform2fv;
            new_button->pGLProgramUniform4fv = entry_points->pGLProgramUniform4fv;
            new_button->pGLUseProgram        = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_button->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_button->pGLDrawArrays        = entry_points->pGLDrawArrays;
            new_button->pGLProgramUniform1f  = entry_points->pGLProgramUniform1f;
            new_button->pGLProgramUniform2fv = entry_points->pGLProgramUniform2fv;
            new_button->pGLProgramUniform4fv = entry_points->pGLProgramUniform4fv;
            new_button->pGLUseProgram        = entry_points->pGLUseProgram;
        }

        /* Configure the text to be shown on the button */
        ogl_text_set(new_button->text_renderer,
                     new_button->text_index,
                     system_hashed_ansi_string_get_buffer(name) );

        _ogl_ui_button_update_text_location(new_button);

        ogl_text_set_text_string_property(new_button->text_renderer,
                                          new_button->text_index,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          _ui_button_text_color);

        /* Retrieve the rendering program */
        new_button->program = ogl_ui_get_registered_program(instance,
                                                            ui_button_program_name);

        if (new_button->program == NULL)
        {
            _ogl_ui_button_init_program(instance,
                                        new_button);

            ASSERT_DEBUG_SYNC(new_button->program != NULL,
                              "Could not initialize button UI program");
        } /* if (new_button->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(new_button->context,
                                                         _ogl_ui_button_init_renderer_callback,
                                                         new_button);
    } /* if (new_button != NULL) */

    return (void*) new_button;
}

/** Please see header for specification */
PUBLIC bool ogl_ui_button_is_over(void* internal_instance, const float* xy)
{
    _ogl_ui_button* button_ptr = (_ogl_ui_button*) internal_instance;
    float           inversed_y = 1.0f - xy[1];

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
PUBLIC void ogl_ui_button_on_lbm_down(void* internal_instance, const float* xy)
{
    _ogl_ui_button* button_ptr = (_ogl_ui_button*) internal_instance;
    float           inversed_y = 1.0f - xy[1];

    if (xy[0]      >= button_ptr->x1y1x2y2[0] && xy[0]      <= button_ptr->x1y1x2y2[2] &&
        inversed_y >= button_ptr->x1y1x2y2[1] && inversed_y <= button_ptr->x1y1x2y2[3])
    {
        button_ptr->is_lbm_on                   = true;
        button_ptr->force_gpu_brightness_update = true;
    }
}

/** Please see header for specification */
PUBLIC void ogl_ui_button_on_lbm_up(void* internal_instance, const float* xy)
{
    _ogl_ui_button* button_ptr = (_ogl_ui_button*) internal_instance;

    button_ptr->is_lbm_on                   = false;
    button_ptr->force_gpu_brightness_update = true;

    if (ogl_ui_button_is_over(internal_instance, xy) &&
        button_ptr->pfn_fire_proc_ptr != NULL)
    {
        system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                         (PFNSYSTEMTHREADPOOLCALLBACKPROC) button_ptr->pfn_fire_proc_ptr,
                                                                                                         button_ptr->fire_proc_user_arg);

        system_thread_pool_submit_single_task(task);
    }
}

/** Please see header for specification */
PUBLIC void ogl_ui_button_set_property(__in  __notnull void*       button,
                                       __in  __notnull int         property_value,
                                       __out __notnull const void* data)
{
    _ogl_ui_button* button_ptr = (_ogl_ui_button*) button;

    switch (property_value)
    {
        case OGL_UI_BUTTON_PROPERTY_X1Y1:
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

            _ogl_ui_button_update_text_location(button_ptr);

            break;
        }

        case OGL_UI_BUTTON_PROPERTY_X1Y1X2Y2:
        {
            memcpy(button_ptr->x1y1x2y2,
                   data,
                   sizeof(float) * 4);

            button_ptr->x1y1x2y2[1]                = 1.0f - button_ptr->x1y1x2y2[1];
            button_ptr->x1y1x2y2[3]                = 1.0f - button_ptr->x1y1x2y2[3];
            button_ptr->should_update_border_width = true;

            _ogl_ui_button_update_text_location(button_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_ui_button property");
        }
    } /* switch (property_value) */
}