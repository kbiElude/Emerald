/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_ui.h"
#include "ogl/ogl_ui_button.h"
#include "ogl/ogl_ui_shared.h"
#include "raGL/raGL_buffer.h"
#include "ral/ral_context.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (1.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(450) )

#define UB_DATAFS_BP_INDEX (0)
#define UB_DATAVS_BP_INDEX (1)


const float _ui_button_text_color[] = {1, 1, 1, 1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_button_program_name = system_hashed_ansi_string_create("UI Button");

/** Internal types */
typedef struct
{
    float               x1y1x2y2[4];

    void*               fire_proc_user_arg;
    PFNOGLUIFIREPROCPTR pfn_fire_proc_ptr;

    float       current_gpu_brightness_level;
    bool        force_gpu_brightness_update;
    bool        is_hovering;
    bool        is_lbm_on;
    bool        should_update_border_width;
    float       start_hovering_brightness;
    system_time start_hovering_time;
    bool        visible;

    ral_context    context;
    ogl_program    program;
    GLint          program_border_width_ub_offset;
    GLint          program_brightness_ub_offset;
    GLint          program_stop_data_ub_offset;
    ogl_program_ub program_ub_fs;
    ral_buffer     program_ub_fs_bo;
    GLuint         program_ub_fs_bo_size;
    ogl_program_ub program_ub_vs;
    ral_buffer     program_ub_vs_bo;
    GLuint         program_ub_vs_bo_size;
    GLint          program_x1y1x2y2_ub_offset;

    GLint    text_index;
    ogl_text text_renderer;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ogl_ui_button;

/** Internal variables */
static const char* ui_button_fragment_shader_body = "#version 430 core\n"
                                                    "\n"
                                                    "in  vec2 uv;\n"
                                                    "out vec3 result;\n"
                                                    /* stop, rgb */
                                                    "uniform dataFS\n"
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
PRIVATE void _ogl_ui_button_init_program(ogl_ui          ui,
                                         _ogl_ui_button* button_ptr)
{
    /* Create all objects */
    ral_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = NULL;
    ogl_shader  vertex_shader   = NULL;

    fragment_shader = ogl_shader_create (context,
                                         RAL_SHADER_TYPE_FRAGMENT,
                                         system_hashed_ansi_string_create("UI button fragment shader") );
    vertex_shader   = ogl_shader_create (context,
                                         RAL_SHADER_TYPE_VERTEX,
                                         system_hashed_ansi_string_create("UI button vertex shader") );

    button_ptr->program = ogl_program_create(context,
                                             system_hashed_ansi_string_create("UI button program"),
                                             OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

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

    ral_context_get_property  (button_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);

    /* Retrieve uniform UB offsets */
    const ogl_program_variable* border_width_uniform = NULL;
    const ogl_program_variable* brightness_uniform   = NULL;
    const ogl_program_variable* stop_data_uniform    = NULL;
    const ogl_program_variable* x1y1x2y2_uniform     = NULL;

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

    button_ptr->program_border_width_ub_offset = border_width_uniform->block_offset;
    button_ptr->program_brightness_ub_offset   = brightness_uniform->block_offset;
    button_ptr->program_stop_data_ub_offset    = stop_data_uniform->block_offset;
    button_ptr->program_x1y1x2y2_ub_offset     = x1y1x2y2_uniform->block_offset;

    /* Retrieve uniform block data */
    unsigned int ub_fs_index = -1;
    unsigned int ub_vs_index = -1;

    button_ptr->program_ub_fs = NULL;
    button_ptr->program_ub_vs = NULL;

    ogl_program_get_uniform_block_by_name(button_ptr->program,
                                          system_hashed_ansi_string_create("dataFS"),
                                         &button_ptr->program_ub_fs);
    ogl_program_get_uniform_block_by_name(button_ptr->program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &button_ptr->program_ub_vs);

    ogl_program_ub_get_property(button_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &button_ptr->program_ub_fs_bo_size);
    ogl_program_ub_get_property(button_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &button_ptr->program_ub_vs_bo_size);

    ogl_program_ub_get_property(button_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &button_ptr->program_ub_fs_bo);
    ogl_program_ub_get_property(button_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &button_ptr->program_ub_vs_bo);

    ogl_program_ub_get_property(button_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_INDEX,
                               &ub_fs_index);
    ogl_program_ub_get_property(button_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_INDEX,
                               &ub_vs_index);

    button_ptr->pGLUniformBlockBinding(ogl_program_get_id(button_ptr->program),
                                       ub_fs_index,         /* uniformBlockIndex */
                                       UB_DATAFS_BP_INDEX); /* uniformBlockBinding */
    button_ptr->pGLUniformBlockBinding(ogl_program_get_id(button_ptr->program),
                                       ub_vs_index,         /* uniformBlockIndex */
                                       UB_DATAVS_BP_INDEX); /* uniformBlockBinding */

    /* Set them up */
    ogl_program_ub_set_arrayed_uniform_value(button_ptr->program_ub_fs,
                                             stop_data_uniform->block_offset,
                                             stop_data,
                                             0, /* src_data_flags */
                                             sizeof(stop_data),
                                             0, /* dst_array_start_index */
                                             sizeof(stop_data) / sizeof(float) / 4);

    button_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;
}

/** TODO */
PRIVATE void _ogl_ui_button_update_text_location(_ogl_ui_button* button_ptr)
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

    ral_context_get_property  (button_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
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

    ral_context_release(ui_button_ptr->context);
    ogl_program_release(ui_button_ptr->program);
    ogl_text_set       (ui_button_ptr->text_renderer,
                        ui_button_ptr->text_index,
                        "");

    delete ui_button_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_button_draw(void* internal_instance)
{
    _ogl_ui_button* button_ptr  = (_ogl_ui_button*) internal_instance;
    system_time     time_now    = system_time_now();

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
    const float new_brightness = brightness * (button_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1);

    GLuint program_id = ogl_program_get_id(button_ptr->program);

    if (button_ptr->current_gpu_brightness_level != brightness ||
        button_ptr->force_gpu_brightness_update)
    {
        button_ptr->current_gpu_brightness_level = brightness;
        button_ptr->force_gpu_brightness_update  = false;
    }

    ogl_program_ub_set_nonarrayed_uniform_value(button_ptr->program_ub_fs,
                                                button_ptr->program_brightness_ub_offset,
                                               &new_brightness,
                                                0, /* src_data_flags */
                                                sizeof(new_brightness) );
    ogl_program_ub_set_nonarrayed_uniform_value(button_ptr->program_ub_vs,
                                                button_ptr->program_x1y1x2y2_ub_offset,
                                                button_ptr->x1y1x2y2,
                                                0, /* src_data_flags */
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

        ogl_program_ub_set_nonarrayed_uniform_value(button_ptr->program_ub_fs,
                                                    button_ptr->program_border_width_ub_offset,
                                                    border_width,
                                                    0, /* src_data_flags */
                                                    sizeof(border_width) );

        button_ptr->should_update_border_width = false;
    }

    /* Draw */
    GLuint      program_ub_fs_bo_id           = 0;
    raGL_buffer program_ub_fs_bo_raGL         = NULL;
    uint32_t    program_ub_fs_bo_start_offset = -1;
    GLuint      program_ub_vs_bo_id           = 0;
    raGL_buffer program_ub_vs_bo_raGL         = NULL;
    uint32_t    program_ub_vs_bo_start_offset = -1;

    program_ub_fs_bo_raGL = ral_context_get_buffer_gl(button_ptr->context,
                                                      button_ptr->program_ub_fs_bo);
    program_ub_vs_bo_raGL = ral_context_get_buffer_gl(button_ptr->context,
                                                      button_ptr->program_ub_vs_bo);

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
                                   UB_DATAFS_BP_INDEX,
                                   program_ub_fs_bo_id,
                                   program_ub_fs_bo_start_offset,
                                   button_ptr->program_ub_fs_bo_size);
    button_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                   UB_DATAVS_BP_INDEX,
                                   program_ub_vs_bo_id,
                                   program_ub_vs_bo_start_offset,
                                   button_ptr->program_ub_vs_bo_size);

    ogl_program_ub_sync(button_ptr->program_ub_fs);
    ogl_program_ub_sync(button_ptr->program_ub_vs);

    button_ptr->pGLUseProgram(ogl_program_get_id(button_ptr->program) );
    button_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                              0,
                              4);
}

/** Please see header for specification */
PUBLIC void ogl_ui_button_get_property(const void*              button,
                                       _ogl_ui_control_property property,
                                       void*                    out_result)
{
    const _ogl_ui_button* button_ptr = (const _ogl_ui_button*) button;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_BUTTON_HEIGHT_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[3] - button_ptr->x1y1x2y2[1];

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_BUTTON_WIDTH_SS:
        {
            *(float*) out_result = button_ptr->x1y1x2y2[2] - button_ptr->x1y1x2y2[0];

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = button_ptr->visible;

            break;
        }


        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property property");
        }
    } /* switch (property_value) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_button_init(ogl_ui                    instance,
                                ogl_text                  text_renderer,
                                system_hashed_ansi_string name,
                                const float*              x1y1,
                                const float*              x2y2,
                                PFNOGLUIFIREPROCPTR       pfn_fire_proc_ptr,
                                void*                     fire_proc_user_arg)
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
        new_button->visible            = true;

        ral_context_retain(new_button->context);

        /* Cache GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(new_button->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_button->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_button->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_button->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_button->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_button->pGLUseProgram          = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized backend type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_button->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_button->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_button->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_button->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_button->pGLUseProgram          = entry_points->pGLUseProgram;
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
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_button->context),
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
        system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                   (PFNSYSTEMTHREADPOOLCALLBACKPROC) button_ptr->pfn_fire_proc_ptr,
                                                                                   button_ptr->fire_proc_user_arg);

        system_thread_pool_submit_single_task(task);
    }
}

/** Please see header for specification */
PUBLIC void ogl_ui_button_set_property(void*                    button,
                                       _ogl_ui_control_property property,
                                       const void*              data)
{
    _ogl_ui_button* button_ptr = (_ogl_ui_button*) button;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1:
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

        case OGL_UI_CONTROL_PROPERTY_BUTTON_X1Y1X2Y2:
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
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}