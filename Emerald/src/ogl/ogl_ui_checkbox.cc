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
#include "ogl/ogl_ui_checkbox.h"
#include "ogl/ogl_ui_shared.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"

#define BORDER_WIDTH_PX                       (1)
#define CHECK_BRIGHTNESS_MODIFIER             (3.0f) /* multiplied */
#define CLICK_BRIGHTNESS_MODIFIER             (1.5f) /* multiplied */
#define CHECKBOX_WIDTH_PX                     (20) /* also modify the vertex shader if you change this field */
#define FOCUSED_BRIGHTNESS                    (0.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(200) )
#define NONFOCUSED_BRIGHTNESS                 (0.1f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_timeline_time_for_msec(450) )
#define TEXT_DELTA_PX                         (4)

#define UB_FSDATA_BP_INDEX (0)
#define UB_VSDATA_BP_INDEX (1)

const float _ui_checkbox_text_color[] = {1, 1, 1, 1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_checkbox_program_name = system_hashed_ansi_string_create("UI Checkbox");

/** Internal types */
typedef struct
{
    float               base_x1y1[2]; /* bottom-left position */
    float               x1y1x2y2[4];

    void*               fire_proc_user_arg;
    PFNOGLUIFIREPROCPTR pfn_fire_proc_ptr;

    float                current_gpu_brightness_level;
    bool                 force_gpu_brightness_update;
    bool                 is_hovering;
    bool                 is_lbm_on;
    float                start_hovering_brightness;
    system_timeline_time start_hovering_time;
    bool                 status;
    bool                 visible;

    ogl_context             context;
    ogl_program             program;
    GLint                   program_border_width_ub_offset;
    GLint                   program_brightness_ub_offset;
    GLint                   program_text_brightness_ub_offset;
    ogl_program_ub          program_ub_fs;
    GLuint                  program_ub_fs_bo_id;
    GLuint                  program_ub_fs_bo_size;
    GLuint                  program_ub_fs_bo_start_offset;
    ogl_program_ub          program_ub_vs;
    GLuint                  program_ub_vs_bo_id;
    GLuint                  program_ub_vs_bo_size;
    GLuint                  program_ub_vs_bo_start_offset;
    GLint                   program_x1y1x2y2_ub_offset;

    GLint    text_index;
    ogl_text text_renderer;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ogl_ui_checkbox;

/** Internal variables */
static const char* ui_checkbox_fragment_shader_body = "#version 430 core\n"
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

/** TODO */
PRIVATE void _ogl_ui_checkbox_init_program(__in __notnull ogl_ui            ui,
                                           __in __notnull _ogl_ui_checkbox* checkbox_ptr)
{
    /* Create all objects */
    ogl_context context         = ogl_ui_get_context(ui);
    ogl_shader  fragment_shader = ogl_shader_create (context,
                                                     SHADER_TYPE_FRAGMENT,
                                                     system_hashed_ansi_string_create("UI checkbox fragment shader") );
    ogl_shader  vertex_shader   = ogl_shader_create (context,
                                                     SHADER_TYPE_VERTEX,
                                                     system_hashed_ansi_string_create("UI checkbox vertex shader") );

    checkbox_ptr->program = ogl_program_create(context,
                                               system_hashed_ansi_string_create("UI checkbox program"),
                                               OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    /* Set up shaders */
    ogl_shader_set_body(fragment_shader,
                        system_hashed_ansi_string_create(ui_checkbox_fragment_shader_body) );
    ogl_shader_set_body(vertex_shader,
                        system_hashed_ansi_string_create(ui_general_vertex_shader_body) );

    ogl_shader_compile(fragment_shader);
    ogl_shader_compile(vertex_shader);

    /* Set up program object */
    ogl_program_attach_shader(checkbox_ptr->program,
                              fragment_shader);
    ogl_program_attach_shader(checkbox_ptr->program,
                              vertex_shader);

    ogl_program_link(checkbox_ptr->program);

    /* Set up uniform block bindings */
    ogl_program_ub ub_fs       = NULL;
    unsigned int   ub_fs_index = -1;
    ogl_program_ub ub_vs       = NULL;
    unsigned int   ub_vs_index = -1;

    ogl_program_get_uniform_block_by_name(checkbox_ptr->program,
                                          system_hashed_ansi_string_create("dataFS"),
                                         &ub_fs);
    ogl_program_get_uniform_block_by_name(checkbox_ptr->program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &ub_vs);

    ogl_program_ub_get_property(ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_INDEX,
                               &ub_fs_index);
    ogl_program_ub_get_property(ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_INDEX,
                               &ub_vs_index);

    checkbox_ptr->pGLUniformBlockBinding(ogl_program_get_id(checkbox_ptr->program),
                                         ub_fs_index,         /* uniformBlockIndex */
                                         UB_FSDATA_BP_INDEX); /* uniformBlockBinding */
    checkbox_ptr->pGLUniformBlockBinding(ogl_program_get_id(checkbox_ptr->program),
                                         ub_vs_index,         /* uniformBlockIndex */
                                         UB_VSDATA_BP_INDEX); /* uniformBlockBinding */

    /* Register the prgoram with UI so following button instances will reuse the program */
    ogl_ui_register_program(ui,
                            ui_checkbox_program_name,
                            checkbox_ptr->program);

    /* Release shaders we will no longer need */
    ogl_shader_release(fragment_shader);
    ogl_shader_release(vertex_shader);
}

/** TODO */
PRIVATE void _ogl_ui_checkbox_init_renderer_callback(ogl_context context,
                                                     void*       checkbox)
{
    float                             border_width[2] = {0};
    _ogl_ui_checkbox*                 checkbox_ptr    = (_ogl_ui_checkbox*) checkbox;
    const GLuint                      program_id      = ogl_program_get_id(checkbox_ptr->program);
    system_window                     window          = NULL;
    int                               window_size[2]  = {0};

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    border_width[0] = 1.0f / (float)((checkbox_ptr->x1y1x2y2[2] - checkbox_ptr->x1y1x2y2[0]) * window_size[0]);
    border_width[1] = 1.0f / (float)((checkbox_ptr->x1y1x2y2[3] - checkbox_ptr->x1y1x2y2[1]) * window_size[1]);

    /* Retrieve uniform locations */
    const ogl_program_variable* border_width_uniform    = NULL;
    const ogl_program_variable* brightness_uniform      = NULL;
    const ogl_program_variable* text_brightness_uniform = NULL;
    const ogl_program_variable* x1y1x2y2_uniform        = NULL;

    ogl_program_get_uniform_by_name(checkbox_ptr->program,
                                    system_hashed_ansi_string_create("border_width"),
                                   &border_width_uniform);
    ogl_program_get_uniform_by_name(checkbox_ptr->program,
                                    system_hashed_ansi_string_create("brightness"),
                                   &brightness_uniform);
    ogl_program_get_uniform_by_name(checkbox_ptr->program,
                                    system_hashed_ansi_string_create("text_brightness"),
                                   &text_brightness_uniform);
    ogl_program_get_uniform_by_name(checkbox_ptr->program,
                                    system_hashed_ansi_string_create("x1y1x2y2"),
                                   &x1y1x2y2_uniform);

    checkbox_ptr->program_border_width_ub_offset    = border_width_uniform->block_offset;
    checkbox_ptr->program_brightness_ub_offset      = brightness_uniform->block_offset;
    checkbox_ptr->program_text_brightness_ub_offset = text_brightness_uniform->block_offset;
    checkbox_ptr->program_x1y1x2y2_ub_offset        = x1y1x2y2_uniform->block_offset;

    /* Set up uniform blocks */
    checkbox_ptr->program_ub_fs = NULL;
    checkbox_ptr->program_ub_vs = NULL;

    ogl_program_get_uniform_block_by_name(checkbox_ptr->program,
                                          system_hashed_ansi_string_create("dataFS"),
                                         &checkbox_ptr->program_ub_fs);
    ogl_program_get_uniform_block_by_name(checkbox_ptr->program,
                                          system_hashed_ansi_string_create("dataVS"),
                                         &checkbox_ptr->program_ub_vs);

    ASSERT_DEBUG_SYNC(checkbox_ptr->program_ub_fs != NULL,
                      "dataFS uniform block descriptor is NULL");
    ASSERT_DEBUG_SYNC(checkbox_ptr->program_ub_vs != NULL,
                      "dataVS uniform block descriptor is NULL");

    ogl_program_ub_get_property(checkbox_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &checkbox_ptr->program_ub_fs_bo_size);
    ogl_program_ub_get_property(checkbox_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &checkbox_ptr->program_ub_vs_bo_size);

    ogl_program_ub_get_property(checkbox_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &checkbox_ptr->program_ub_fs_bo_id);
    ogl_program_ub_get_property(checkbox_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &checkbox_ptr->program_ub_vs_bo_id);

    ogl_program_ub_get_property(checkbox_ptr->program_ub_fs,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &checkbox_ptr->program_ub_fs_bo_start_offset);
    ogl_program_ub_get_property(checkbox_ptr->program_ub_vs,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &checkbox_ptr->program_ub_vs_bo_start_offset);

    /* Set them up */
    const float default_brightness = NONFOCUSED_BRIGHTNESS;

    ogl_program_ub_set_nonarrayed_uniform_value(checkbox_ptr->program_ub_fs,
                                                border_width_uniform->block_offset,
                                               &border_width,
                                                0, /* src_data_flags */
                                                sizeof(float) * 2);
    ogl_program_ub_set_nonarrayed_uniform_value(checkbox_ptr->program_ub_fs,
                                                text_brightness_uniform->block_offset,
                                               &default_brightness,
                                                0, /* src_data_flags */
                                                sizeof(float) );

    checkbox_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;
}

/** TODO */
PRIVATE void _ogl_ui_checkbox_update_text_location(__in __notnull _ogl_ui_checkbox* checkbox_ptr)
{
    int           text_height    = 0;
    int           text_xy[2]     = {0};
    int           text_width     = 0;
    system_window window         = NULL;
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

    ogl_text_get_text_string_property(checkbox_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                      checkbox_ptr->text_index,
                                     &text_height);
    ogl_text_get_text_string_property(checkbox_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                      checkbox_ptr->text_index,
                                     &text_width);

    ogl_context_get_property(checkbox_ptr->context,
                             OGL_CONTEXT_PROPERTY_WINDOW,
                            &window);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    text_xy[0] = (int) ((x1y1[0] + (x2y2[0] + float(CHECKBOX_WIDTH_PX) / window_size[0] - x1y1[0] - float(text_width)  / window_size[0]) * 0.5f) * (float) window_size[0]);
    text_xy[1] = (int) ((x2y2[1] - (x2y2[1] +                                           - x1y1[1] - float(text_height) / window_size[1]) * 0.5f) * (float) window_size[1]);

    ogl_text_set_text_string_property(checkbox_ptr->text_renderer,
                                      checkbox_ptr->text_index,
                                      OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                      text_xy);
}

/** TODO */
PRIVATE void _ogl_ui_checkbox_update_x1y1x2y2(__in __notnull _ogl_ui_checkbox* checkbox_ptr)
{
    int           text_height    = 0;
    int           text_width     = 0;
    system_window window;
    int           window_size[2] = {0};

    ogl_text_get_text_string_property(checkbox_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                      checkbox_ptr->text_index,
                                     &text_height);
    ogl_text_get_text_string_property(checkbox_ptr->text_renderer,
                                      OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                      checkbox_ptr->text_index,
                                     &text_width);

    ogl_context_get_property   (checkbox_ptr->context,
                                OGL_CONTEXT_PROPERTY_WINDOW,
                               &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    checkbox_ptr->x1y1x2y2[0] = checkbox_ptr->base_x1y1[0];
    checkbox_ptr->x1y1x2y2[1] = checkbox_ptr->base_x1y1[1];
    checkbox_ptr->x1y1x2y2[2] = checkbox_ptr->base_x1y1[0] + float(text_width  + 2 * BORDER_WIDTH_PX + CHECKBOX_WIDTH_PX + TEXT_DELTA_PX) / float(window_size[0]);
    checkbox_ptr->x1y1x2y2[3] = checkbox_ptr->base_x1y1[1] + float(max(text_height, CHECKBOX_WIDTH_PX)                                    / float(window_size[1]) );
}

/** Please see header for specification */
PUBLIC void ogl_ui_checkbox_deinit(void* internal_instance)
{
    _ogl_ui_checkbox* ui_checkbox_ptr = (_ogl_ui_checkbox*) internal_instance;

    ogl_context_release(ui_checkbox_ptr->context);
    ogl_program_release(ui_checkbox_ptr->program);
    ogl_text_set       (ui_checkbox_ptr->text_renderer,
                        ui_checkbox_ptr->text_index,
                        "");

    delete internal_instance;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ogl_ui_checkbox_draw(void* internal_instance)
{
    _ogl_ui_checkbox*    checkbox_ptr = (_ogl_ui_checkbox*) internal_instance;
    system_timeline_time time_now     = system_time_now();

    /* Update brightness if necessary */
    float brightness = checkbox_ptr->current_gpu_brightness_level;

    if (checkbox_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_timeline_time transition_start = checkbox_ptr->start_hovering_time;
        system_timeline_time transition_end   = checkbox_ptr->start_hovering_time +
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
        system_timeline_time transition_start = checkbox_ptr->start_hovering_time;
        system_timeline_time transition_end   = checkbox_ptr->start_hovering_time +
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
            } /* if (brightness < 0) */
        }
        else
        {
            /* Past the transition time, make sure brightness is valid */
            brightness = NONFOCUSED_BRIGHTNESS;
        }
    }

    /* Update uniforms */
    const float new_brightness = brightness * (checkbox_ptr->is_lbm_on ? CLICK_BRIGHTNESS_MODIFIER : 1) * (checkbox_ptr->status ? CHECK_BRIGHTNESS_MODIFIER : 1);
    GLuint      program_id     = ogl_program_get_id(checkbox_ptr->program);

    if (checkbox_ptr->current_gpu_brightness_level != brightness ||
        checkbox_ptr->force_gpu_brightness_update)
    {
        checkbox_ptr->current_gpu_brightness_level = brightness;
        checkbox_ptr->force_gpu_brightness_update  = false;
    }

    ogl_program_ub_set_nonarrayed_uniform_value(checkbox_ptr->program_ub_fs,
                                                checkbox_ptr->program_brightness_ub_offset,
                                               &new_brightness,
                                                0, /* src_data_flags */
                                                sizeof(float) );
    ogl_program_ub_set_nonarrayed_uniform_value(checkbox_ptr->program_ub_vs,
                                                checkbox_ptr->program_x1y1x2y2_ub_offset,
                                                checkbox_ptr->x1y1x2y2,
                                                0, /* src_data_flags */
                                                sizeof(float) * 4);

    ogl_program_ub_sync(checkbox_ptr->program_ub_fs);
    ogl_program_ub_sync(checkbox_ptr->program_ub_vs);

    /* Draw */
    checkbox_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     UB_FSDATA_BP_INDEX,
                                     checkbox_ptr->program_ub_fs_bo_id,
                                     checkbox_ptr->program_ub_fs_bo_start_offset,
                                     checkbox_ptr->program_ub_fs_bo_size);
    checkbox_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     UB_VSDATA_BP_INDEX,
                                     checkbox_ptr->program_ub_vs_bo_id,
                                     checkbox_ptr->program_ub_vs_bo_start_offset,
                                     checkbox_ptr->program_ub_vs_bo_size);

    checkbox_ptr->pGLUseProgram(ogl_program_get_id(checkbox_ptr->program) );
    checkbox_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                0,
                                4);
}

/** Please see header for specification */
PUBLIC void ogl_ui_checkbox_get_property(__in  __notnull const void*              checkbox,
                                         __in            _ogl_ui_control_property property,
                                         __out __notnull void*                    out_result)
{
    const _ogl_ui_checkbox* checkbox_ptr = (const _ogl_ui_checkbox*) checkbox;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_CHECKBOX_CHECK_STATUS:
        {
            *(bool*) out_result = checkbox_ptr->status;

            break;
        }

        case OGL_UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = checkbox_ptr->visible;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property property");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC void* ogl_ui_checkbox_init(__in           __notnull   ogl_ui                    instance,
                                  __in           __notnull   ogl_text                  text_renderer,
                                  __in           __notnull   system_hashed_ansi_string name,
                                  __in_ecount(2) __notnull   const float*              x1y1,
                                  __in           __notnull   PFNOGLUIFIREPROCPTR       pfn_fire_proc_ptr,
                                  __in           __maybenull void*                     fire_proc_user_arg,
                                  __in                       bool                      default_status)
{
    _ogl_ui_checkbox* new_checkbox = new (std::nothrow) _ogl_ui_checkbox;

    ASSERT_ALWAYS_SYNC(new_checkbox != NULL,
                       "Out of memory");

    if (new_checkbox != NULL)
    {
        /* Initialize fields */
        memset(new_checkbox,
               0,
               sizeof(_ogl_ui_checkbox) );

        new_checkbox->base_x1y1[0] = x1y1[0];
        new_checkbox->base_x1y1[1] = x1y1[1];

        new_checkbox->context            = ogl_ui_get_context(instance);
        new_checkbox->fire_proc_user_arg = fire_proc_user_arg;
        new_checkbox->pfn_fire_proc_ptr  = pfn_fire_proc_ptr;
        new_checkbox->status             = default_status;
        new_checkbox->text_renderer      = text_renderer;
        new_checkbox->text_index         = ogl_text_add_string(text_renderer);
        new_checkbox->visible            = true;

        ogl_context_retain(new_checkbox->context);

        /* Cache GL func pointers */
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(new_checkbox->context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        if (context_type == OGL_CONTEXT_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_checkbox->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points);

            new_checkbox->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_checkbox->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_checkbox->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_checkbox->pGLUseProgram          = entry_points->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points = NULL;

            ogl_context_get_property(new_checkbox->context,
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points);

            new_checkbox->pGLBindBufferRange     = entry_points->pGLBindBufferRange;
            new_checkbox->pGLDrawArrays          = entry_points->pGLDrawArrays;
            new_checkbox->pGLUniformBlockBinding = entry_points->pGLUniformBlockBinding;
            new_checkbox->pGLUseProgram          = entry_points->pGLUseProgram;
        }

        /* Configure the text to be shown on the button */
        ogl_text_set(new_checkbox->text_renderer,
                     new_checkbox->text_index,
                     system_hashed_ansi_string_get_buffer(name) );

        _ogl_ui_checkbox_update_x1y1x2y2     (new_checkbox);
        _ogl_ui_checkbox_update_text_location(new_checkbox);

        ogl_text_set_text_string_property(new_checkbox->text_renderer,
                                          new_checkbox->text_index,
                                          OGL_TEXT_STRING_PROPERTY_COLOR,
                                          _ui_checkbox_text_color);

        /* Retrieve the rendering program */
        new_checkbox->program = ogl_ui_get_registered_program(instance,
                                                              ui_checkbox_program_name);

        if (new_checkbox->program == NULL)
        {
            _ogl_ui_checkbox_init_program(instance,
                                          new_checkbox);

            ASSERT_DEBUG_SYNC(new_checkbox->program != NULL,
                              "Could not initialize checkbox UI program");
        } /* if (new_button->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(new_checkbox->context,
                                                         _ogl_ui_checkbox_init_renderer_callback,
                                                         new_checkbox);
    } /* if (new_checkbox != NULL) */

    return (void*) new_checkbox;
}

/** Please see header for specification */
PUBLIC bool ogl_ui_checkbox_is_over(void*        internal_instance,
                                    const float* xy)
{
    _ogl_ui_checkbox* checkbox_ptr = (_ogl_ui_checkbox*) internal_instance;
    float             inversed_y   = 1.0f - xy[1];

    if (xy[0]      >= checkbox_ptr->x1y1x2y2[0] && xy[0]      <= checkbox_ptr->x1y1x2y2[2] &&
        inversed_y >= checkbox_ptr->x1y1x2y2[1] && inversed_y <= checkbox_ptr->x1y1x2y2[3])
    {
        if (!checkbox_ptr->is_hovering)
        {
            checkbox_ptr->start_hovering_brightness = checkbox_ptr->current_gpu_brightness_level;
            checkbox_ptr->start_hovering_time       = system_time_now();
            checkbox_ptr->is_hovering               = true;
        } /* if (!checkbox_ptr->is_hovering) */

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
PUBLIC void ogl_ui_checkbox_on_lbm_down(void*        internal_instance,
                                        const float* xy)
{
    _ogl_ui_checkbox* checkbox_ptr = (_ogl_ui_checkbox*) internal_instance;
    float             inversed_y   = 1.0f - xy[1];

    if (xy[0]      >= checkbox_ptr->x1y1x2y2[0] && xy[0]      <= checkbox_ptr->x1y1x2y2[2] &&
        inversed_y >= checkbox_ptr->x1y1x2y2[1] && inversed_y <= checkbox_ptr->x1y1x2y2[3])
    {
        checkbox_ptr->is_lbm_on                   = true;
        checkbox_ptr->force_gpu_brightness_update = true;
    }
}

/** Please see header for specification */
PUBLIC void ogl_ui_checkbox_on_lbm_up(void*        internal_instance,
                                      const float* xy)
{
    _ogl_ui_checkbox* checkbox_ptr = (_ogl_ui_checkbox*) internal_instance;

    checkbox_ptr->is_lbm_on = false;

    if (ogl_ui_checkbox_is_over(internal_instance,
                                xy) )
    {
        if (checkbox_ptr->pfn_fire_proc_ptr != NULL)
        {
            system_thread_pool_task_descriptor task = system_thread_pool_create_task_descriptor_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                             (PFNSYSTEMTHREADPOOLCALLBACKPROC) checkbox_ptr->pfn_fire_proc_ptr,
                                                                                                             checkbox_ptr->fire_proc_user_arg);

            system_thread_pool_submit_single_task(task);
        }

        checkbox_ptr->force_gpu_brightness_update = true;
        checkbox_ptr->status                      = !checkbox_ptr->status;
    }
}

/* Please see header for spec */
PUBLIC void ogl_ui_checkbox_set_property(__in __notnull void*                    checkbox,
                                         __in __notnull _ogl_ui_control_property property,
                                         __in __notnull const void*              data)
{
    _ogl_ui_checkbox* checkbox_ptr = (_ogl_ui_checkbox*) checkbox;

    switch (property)
    {
        case OGL_UI_CONTROL_PROPERTY_CHECKBOX_X1Y1:
        {
            __analysis_assume(sizeof(data) == sizeof(float) * 2);

            memcpy(checkbox_ptr->base_x1y1,
                   data,
                   sizeof(float) * 2);

            checkbox_ptr->base_x1y1[1] = 1.0f - checkbox_ptr->base_x1y1[1];

            _ogl_ui_checkbox_update_x1y1x2y2     (checkbox_ptr);
            _ogl_ui_checkbox_update_text_location(checkbox_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_ui_control_property value");
        }
    } /* switch (property_value) */
}
