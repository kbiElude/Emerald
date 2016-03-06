/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
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
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_window.h"
#include "ui/ui.h"
#include "ui/ui_dropdown.h"
#include "ui/ui_shared.h"
#include "varia/varia_text_renderer.h"

#define BUTTON_WIDTH_PX                       (14)
#define CLICK_BRIGHTNESS_MODIFIER             (1.5f)
#define FOCUSED_BRIGHTNESS                    (2.5f)
#define FOCUSED_TO_NONFOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(200) )
#define LABEL_X_SEPARATOR_PX                  (4)
#define MAX_N_ENTRIES_VISIBLE                 (5)
#define NONFOCUSED_BRIGHTNESS                 (1.0f)
#define NONFOCUSED_TO_FOCUSED_TRANSITION_TIME (system_time_get_time_for_msec(250) )
#define SLIDER_Y_SEPARATOR_PX                 (4)
#define SLIDER_WIDTH_PX                       (7)

#define UB_FSDATA_BP (0)
#define UB_VSDATA_BP (1)

const float _ui_dropdown_label_text_color[] = {1,     1,     1,     0.5f};
const float _ui_dropdown_slider_color[]     = {0.25f, 0.25f, 0.25f, 0.5f};
const float _ui_dropdown_text_color[]       = {1,     1,     1,     1.0f};

/** Internal definitions */
static system_hashed_ansi_string ui_dropdown_bg_program_name        = system_hashed_ansi_string_create("UI Dropdown BG");
static system_hashed_ansi_string ui_dropdown_label_bg_program_name  = system_hashed_ansi_string_create("UI Dropdown Label BG");
static system_hashed_ansi_string ui_dropdown_program_name           = system_hashed_ansi_string_create("UI Dropdown");
static system_hashed_ansi_string ui_dropdown_separator_program_name = system_hashed_ansi_string_create("UI Dropdown Separator");
static system_hashed_ansi_string ui_dropdown_slider_program_name    = system_hashed_ansi_string_create("UI Dropdown Slider");

typedef struct _ui_dropdown_fire_event
{
    void*            event_user_arg;
    void*            fire_proc_user_arg;
    PFNUIFIREPROCPTR pfn_fire_proc_ptr;

    _ui_dropdown_fire_event()
    {
        event_user_arg     = NULL;
        fire_proc_user_arg = NULL;
        pfn_fire_proc_ptr  = NULL;
    }
} _ui_dropdown_fire_event;

/** Internal types */
typedef struct _ui_dropdown_entry
{
    int                                base_x;
    int                                base_y;
    system_hashed_ansi_string          name;
    varia_text_renderer_text_string_id string_id;
    int                                text_y;
    void*                              user_arg;

    _ui_dropdown_entry()
    {
        base_x    = 0;
        base_y    = 0;
        name      = NULL;
        string_id = -1;
        text_y    = 0;
        user_arg  = NULL;
    }
} _ui_dropdown_entry;

typedef struct
{
    float            accumulated_wheel_delta;

    float            button_x1y1x2y2  [4];
    float            drop_x1y2x2y1    [4];
    float            label_x1y1       [2];
    float            label_bg_x1y1x2y2[4];
    float            hover_ss[2];
    float            separator_delta_y;

    float            slider_delta_y;
    float            slider_delta_y_base;
    float            slider_height;
    float            slider_lbm_start_y;
    float            slider_separator_y;
    float            slider_x1x2[2];
    float            x1y1x2y2[4];

    void*            fire_proc_user_arg;
    PFNUIFIREPROCPTR pfn_fire_proc_ptr;

    float       current_gpu_brightness_level;
    bool        force_gpu_brightness_update;
    bool        is_button_lbm;
    bool        is_droparea_lbm;
    bool        is_droparea_visible;
    bool        is_hovering;
    bool        is_lbm_on;
    bool        is_slider_lbm;
    uint32_t    n_selected_entry;
    float       start_hovering_brightness;
    system_time start_hovering_time;

    ral_context               context;
    ral_program               program;
    GLint                     program_border_width_ub_offset;
    GLint                     program_button_start_u_ub_offset;
    GLint                     program_brightness_ub_offset;
    GLint                     program_stop_data_ub_offset;
    GLint                     program_x1y1x2y2_ub_offset;
    ral_program_block_buffer  program_ub_fs;
    GLuint                    program_ub_fs_bo_size;
    ral_program_block_buffer  program_ub_vs;
    GLuint                    program_ub_vs_bo_size;

    ral_program               program_bg;
    GLint                     program_bg_border_width_ub_offset;
    GLint                     program_bg_highlighted_v1v2_ub_offset;
    GLint                     program_bg_selected_v1v2_ub_offset;
    GLint                     program_bg_x1y1x2y2_ub_offset;
    ral_program_block_buffer  program_bg_ub_fs;
    GLuint                    program_bg_ub_fs_bo_size;
    ral_program_block_buffer  program_bg_ub_vs;
    GLuint                    program_bg_ub_vs_bo_size;

    ral_program               program_label_bg;
    GLuint                    program_label_bg_x1y1x2y2_ub_offset;
    ral_program_block_buffer  program_label_bg_ub_vs;
    GLuint                    program_label_bg_ub_vs_bo_size;

    ral_program               program_separator;
    ral_program_block_buffer  program_separator_ub_vs;
    GLuint                    program_separator_ub_vs_bo_size;
    GLint                     program_separator_x1_x2_y_ub_offset;

    ral_program               program_slider;
    GLint                     program_slider_color_ub_offset;
    GLint                     program_slider_x1y1x2y2_ub_offset;
    ral_program_block_buffer  program_slider_ub_fs;
    GLuint                    program_slider_ub_fs_bo_size;
    ral_program_block_buffer  program_slider_ub_vs;
    GLuint                    program_slider_ub_vs_bo_size;

    varia_text_renderer_text_string_id current_entry_string_id;
    system_resizable_vector            entries;
    system_hashed_ansi_string          label_text;
    varia_text_renderer_text_string_id label_string_id;
    varia_text_renderer                text_renderer;
    ui                                 ui_instance; /* NOT reference-counted */

    ui_control owner_control; /* this object, as visible for applications */

    bool visible;

    /* Cached func ptrs */
    PFNGLBINDBUFFERRANGEPROC     pGLBindBufferRange;
    PFNGLDISABLEPROC             pGLDisable;
    PFNGLDRAWARRAYSPROC          pGLDrawArrays;
    PFNGLENABLEPROC              pGLEnable;
    PFNGLUNIFORMBLOCKBINDINGPROC pGLUniformBlockBinding;
    PFNGLUSEPROGRAMPROC          pGLUseProgram;
} _ui_dropdown;

/** Internal variables */
static const char* ui_dropdown_bg_fragment_shader_body = "#version 430 core\n"
                                                         "\n"
                                                         "in  vec2 uv;\n"
                                                         "out vec3 result;\n"
                                                         "\n"
                                                         "uniform dataFS\n"
                                                         "{\n"
                                                         "    vec2 border_width;\n"
                                                         "    vec2 button_start_uv;\n"
                                                         "    vec2 highlighted_v1v2;\n"
                                                         "    vec2 selected_v1v2;\n"
                                                         "};\n"
                                                         "\n"
                                                         "void main()\n"
                                                         "{\n"
                                                         "    result = vec3(mix(vec3(0.12), vec3(0.22), uv.yyy) );\n"
                                                         /* Scroll area? */
                                                         "    if (uv.x > button_start_uv.x)\n"
                                                         "    {\n"
                                                         "        result *= 0.8;\n"
                                                         "    }\n"
                                                         "    if (uv.x <= button_start_uv.x)\n"
                                                         "    {\n"
                                                         /* Highlighted entry? */
                                                         "        if (uv.y >= highlighted_v1v2.x &&\n"
                                                         "            uv.y <= highlighted_v1v2.y)\n"
                                                         "        {\n"
                                                         "            result *= mix(vec3(0.09, 0.64, 0.8125) * vec3(1.25),\n"
                                                         "                          vec3(0.09, 0.64, 0.8125) * vec3(2.125),\n"
                                                         "                          vec3((uv.y - highlighted_v1v2.x) / (highlighted_v1v2.y - highlighted_v1v2.x)) );\n"
                                                         "        }\n"
                                                         "        else\n"
                                                         /* Selected entry? */
                                                         "        if (uv.y >= selected_v1v2.x   &&\n"
                                                         "            uv.y <= selected_v1v2.y)\n"
                                                         "        {\n"
                                                         "            result *= mix(vec3(0.09, 0.64, 0.8125) * vec3(2.5),\n"
                                                         "                          vec3(0.09, 0.64, 0.8125) * vec3(4.25),\n"
                                                         "                          vec3((uv.y - selected_v1v2.x) / (selected_v1v2.y - selected_v1v2.x)) );\n"
                                                         "        }\n"
                                                         "    }\n"
                                                         /* Border? */
                                                         "    if (uv.x < border_width[0]         ||\n"
                                                         "        uv.x > (1.0 - border_width[0]) ||\n"
                                                         "        uv.y < border_width[1]         ||\n"
                                                         "        uv.y > (1.0 - border_width[1]) )\n"
                                                         "    {\n"
                                                         "        result = vec3(0.42f, 0.41f, 0.41f);\n"
                                                         "    }\n"
                                                         "}\n";

static const char* ui_dropdown_fragment_shader_body    = "#version 430 core\n"
                                                         "\n"
                                                         "in  vec2 uv;\n"
                                                         "out vec3 result;\n"
                                                         "\n"
                                                         "uniform dataFS\n"
                                                         "{\n"
                                                         "    float brightness;\n"
                                                         "    vec2  border_width;\n"
                                                         "    float button_start_u;\n"
                                                         "    vec4  stop_data[4];\n"
                                                         "};\n"
                                                         "\n"
                                                         "void main()\n"
                                                         "{\n"
                                                         "    if (uv.y >= stop_data[0].x && uv.y <= stop_data[1].x || uv.x >= 0.85)\n"
                                                         "        result = mix(stop_data[0].yzw, stop_data[1].yzw, (uv.y - stop_data[0].x) / (stop_data[1].x - stop_data[0].x));\n"
                                                         "    else\n"
                                                         "    if (uv.y >= stop_data[1].x && uv.y <= stop_data[2].x)\n"
                                                         "        result = mix(stop_data[1].yzw, stop_data[2].yzw, (uv.y - stop_data[1].x) / (stop_data[2].x - stop_data[1].x));\n"
                                                         "    else\n"
                                                         "        result = mix(stop_data[2].yzw, stop_data[3].yzw, (uv.y - stop_data[2].x) / (stop_data[3].x - stop_data[2].x));\n"
                                                         "\n" 
                                                         "    if (uv.x >= button_start_u)\n"
                                                         "        result *= brightness + smoothstep(0.0, 1.0, (uv.x - button_start_u) * 5.0);\n"
                                                         /* Border? */
                                                         "    if (uv.x < border_width[0]         ||\n"
                                                         "        uv.x > (1.0 - border_width[0]) ||\n"
                                                         "        uv.y < border_width[1]         ||\n"
                                                         "        uv.y > (1.0 - border_width[1]) )\n"
                                                         "    {\n"
                                                         "        result = vec3(0.42f, 0.41f, 0.41f);\n"
                                                         "    }\n"
                                                         "}\n";

static const char* ui_dropdown_label_bg_fragment_shader_body  = "#version 430 core\n"
                                                                "\n"
                                                                "out vec4 result;\n"
                                                                "\n"
                                                                "void main()\n"
                                                                "{\n"
                                                                "    result = vec4(0.1f, 0.1f, 0.2f, 0.8f);\n"
                                                                "}\n";

static const char* ui_dropdown_separator_fragment_shader_body = "#version 430 core\n"
                                                                "\n"
                                                                "out vec3 result;\n"
                                                                "\n"
                                                                "void main()\n"
                                                                "{\n"
                                                                "    result = vec3(0.42f, 0.41f, 0.41f);\n"
                                                                "}";
static const char* ui_dropdown_separator_vertex_shader_body   = "#version 430 core\n"
                                                                "\n"
                                                                "uniform dataVS\n"
                                                                "{\n"
                                                                "    vec3 x1_x2_y;\n"
                                                                "};\n"
                                                                "\n"
                                                                "void main()\n"
                                                                "{\n"
                                                                "    int   end = gl_VertexID % 2;\n"
                                                                "    float x;\n"
                                                                "\n"
                                                                "    if (end == 0) x = x1_x2_y.x;\n"
                                                                "    else          x = x1_x2_y.y;\n"
                                                                "\n"
                                                                "    gl_Position = vec4(x, x1_x2_y.z, 0.0, 1.0);\n"
                                                                "}";

static const char* ui_dropdown_slider_fragment_shader_body    = "#version 430 core\n"
                                                                "\n"
                                                                "in  vec2 uv;\n"
                                                                "out vec4 result;\n"
                                                                "\n"
                                                                "uniform dataFS\n"
                                                                "{\n"
                                                                "    uniform vec4 color;\n"
                                                                "};\n"
                                                                "\n"
                                                                "void main()\n"
                                                                "{\n"
                                                                "    vec2 uv_ss = uv * vec2(2.0) - vec2(1.0);\n"
                                                                "\n"
                                                                "    result = color * vec4(1.0 - length(pow(abs(uv_ss), vec2(4.0))));\n"
                                                                "}\n";

/** TODO */
volatile void _ui_dropdown_fire_callback(system_thread_pool_callback_argument arg)
{
    _ui_dropdown_fire_event* event_ptr = (_ui_dropdown_fire_event*) arg;

    ASSERT_DEBUG_SYNC(event_ptr != NULL,
                      "Event descriptor is NULL");

    if (event_ptr != NULL)
    {
        event_ptr->pfn_fire_proc_ptr(event_ptr->fire_proc_user_arg,
                                     event_ptr->event_user_arg);

        /* Good to release the event descriptor at this point */
        delete event_ptr;
    } /* if (event_ptr != NULL) */
}

/** TODO */
PRIVATE void _ui_dropdown_get_highlighted_v1v2(_ui_dropdown* dropdown_ptr,
                                               bool          offset_by_slider_dy,
                                               float*        out_highlighted_v1v2)
{
    unsigned int n_entries = 0;
    float        slider_height_ss;

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_entries);

    if (dropdown_ptr->slider_height < 1.0f)
    {
        slider_height_ss = (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * (1.0f - dropdown_ptr->slider_height);
    }
    else
    {
        slider_height_ss = 1.0f;
    }

    const float slider_dy = -(dropdown_ptr->slider_delta_y + dropdown_ptr->slider_delta_y_base) / slider_height_ss;
    float       v         =  1.0f - (dropdown_ptr->hover_ss[1] - dropdown_ptr->drop_x1y2x2y1[3]) /
                                    (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]);

    float entries_skipped_ss  = slider_dy * float(n_entries - MAX_N_ENTRIES_VISIBLE - 1);
    int   n_entry_highlighted = 1 + int(entries_skipped_ss + v * float(MAX_N_ENTRIES_VISIBLE));

    /* Store the result */
    out_highlighted_v1v2[0] = 1.0f - float(n_entry_highlighted) / float(MAX_N_ENTRIES_VISIBLE);
    out_highlighted_v1v2[1] = out_highlighted_v1v2[0] + 1.0f / float(MAX_N_ENTRIES_VISIBLE);

    if (!offset_by_slider_dy)
    {
        out_highlighted_v1v2[0] += entries_skipped_ss / float(MAX_N_ENTRIES_VISIBLE);
        out_highlighted_v1v2[1] += entries_skipped_ss / float(MAX_N_ENTRIES_VISIBLE);
    }
}

/** TODO */
PRIVATE void _ui_dropdown_get_selected_v1v2(_ui_dropdown* dropdown_ptr,
                                            float*        out_selected_v1v2)
{
    unsigned int n_entries = 0;

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_entries);

    const float        slider_height_ss   =  (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * (1.0f - dropdown_ptr->slider_height);
    const float        slider_dy          = -(dropdown_ptr->slider_delta_y + dropdown_ptr->slider_delta_y_base) / slider_height_ss;
    float              entries_skipped_ss = slider_dy * float(n_entries - MAX_N_ENTRIES_VISIBLE - 1);

    /* Store the result */
    out_selected_v1v2[0] = 1.0f - float(dropdown_ptr->n_selected_entry + 1) / float(MAX_N_ENTRIES_VISIBLE);
    out_selected_v1v2[1] = out_selected_v1v2[0] + 1.0f / float(MAX_N_ENTRIES_VISIBLE);

    out_selected_v1v2[0] += entries_skipped_ss / float(MAX_N_ENTRIES_VISIBLE);
    out_selected_v1v2[1] += entries_skipped_ss / float(MAX_N_ENTRIES_VISIBLE);
}

/** TODO */
PRIVATE void _ui_dropdown_get_slider_x1y1x2y2(_ui_dropdown* dropdown_ptr,
                                              float*        out_result)
{
    out_result[0] = dropdown_ptr->slider_x1x2[0];
    out_result[1] = dropdown_ptr->drop_x1y2x2y1[1]   -
                    dropdown_ptr->slider_separator_y +
                    dropdown_ptr->slider_delta_y     +
                    dropdown_ptr->slider_delta_y_base;
    out_result[2] = dropdown_ptr->slider_x1x2[1];
    out_result[3] = dropdown_ptr->drop_x1y2x2y1[3]       +
                    dropdown_ptr->slider_separator_y     +
                    (dropdown_ptr->drop_x1y2x2y1[1]      -
                     dropdown_ptr->drop_x1y2x2y1[3])     *
                    (1.0f - dropdown_ptr->slider_height) +
                    dropdown_ptr->slider_delta_y         +
                    dropdown_ptr->slider_delta_y_base;
}

/** TODO */
PRIVATE void _ui_dropdown_init_program(ui            ui_instance,
                                       _ui_dropdown* dropdown_ptr)
{
    /* Create all objects */
    ral_context context      = ui_get_context(ui_instance);
    ral_shader  fs_bg        = NULL;
    ral_shader  fs_label_bg  = NULL;
    ral_shader  fs_separator = NULL;
    ral_shader  fs_slider    = NULL;
    ral_shader  fs           = NULL;
    ral_shader  vs           = NULL;
    ral_shader  vs_separator = NULL;

    ral_program_create_info program_bg_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI dropdown program (bg)")
    };

    ral_program_create_info program_label_bg_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI dropdown program (label bg)")
    };

    ral_program_create_info program_separator_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI dropdown program (separator)")
    };

    ral_program_create_info program_slider_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI dropdown program (slider)")
    };

    ral_program_create_info program_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT | RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX,
        system_hashed_ansi_string_create("UI dropdown program")
    };


    ral_shader_create_info fs_bg_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown fragment shader (bg)"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    ral_shader_create_info fs_label_bg_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown fragment shader (label bg)"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    ral_shader_create_info fs_separator_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown framgent shader (separator)"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    ral_shader_create_info fs_slider_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown framgent shader (slider)"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    ral_shader_create_info fs_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown fragment shader"),
        RAL_SHADER_TYPE_FRAGMENT
    };
    ral_shader_create_info vs_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown vertex shader"),
        RAL_SHADER_TYPE_VERTEX
    };
    ral_shader_create_info vs_separator_create_info =
    {
        system_hashed_ansi_string_create("UI dropdown vertex shader (separator)"),
        RAL_SHADER_TYPE_VERTEX
    };


    ral_program_create_info program_create_info_items[] =
    {
        program_bg_create_info,
        program_label_bg_create_info,
        program_separator_create_info,
        program_slider_create_info,
        program_create_info
    };
    ral_shader_create_info shader_create_info_items[] =
    {
        fs_bg_create_info,
        fs_label_bg_create_info,
        fs_separator_create_info,
        fs_slider_create_info,
        fs_create_info,
        vs_create_info,
        vs_separator_create_info
    };

    const uint32_t n_program_create_info_items = sizeof(program_create_info_items) / sizeof(program_create_info_items[0]);
    const uint32_t n_shader_create_info_items  = sizeof(shader_create_info_items)  / sizeof(shader_create_info_items[0]);

    ral_program result_programs[n_program_create_info_items];
    ral_shader  result_shaders [n_shader_create_info_items];

    if (!ral_context_create_programs(context,
                                     n_program_create_info_items,
                                     program_create_info_items,
                                     result_programs) )
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

    dropdown_ptr->program_bg        = result_programs[0];
    dropdown_ptr->program_label_bg  = result_programs[1];
    dropdown_ptr->program_separator = result_programs[2];
    dropdown_ptr->program_slider    = result_programs[3];
    dropdown_ptr->program           = result_programs[4];

    fs_bg        = result_shaders[0];
    fs_label_bg  = result_shaders[1];
    fs_separator = result_shaders[2];
    fs_slider    = result_shaders[3];
    fs           = result_shaders[4];
    vs           = result_shaders[5];
    vs_separator = result_shaders[6];

    /* Set up shaders */
    const system_hashed_ansi_string fs_body           = system_hashed_ansi_string_create(ui_dropdown_fragment_shader_body);
    const system_hashed_ansi_string fs_bg_body        = system_hashed_ansi_string_create(ui_dropdown_bg_fragment_shader_body);
    const system_hashed_ansi_string fs_label_bg_body  = system_hashed_ansi_string_create(ui_dropdown_label_bg_fragment_shader_body);
    const system_hashed_ansi_string fs_separator_body = system_hashed_ansi_string_create(ui_dropdown_separator_fragment_shader_body);
    const system_hashed_ansi_string fs_slider_body    = system_hashed_ansi_string_create(ui_dropdown_slider_fragment_shader_body);
    const system_hashed_ansi_string vs_body           = system_hashed_ansi_string_create(ui_general_vertex_shader_body);
    const system_hashed_ansi_string vs_separator_body = system_hashed_ansi_string_create(ui_dropdown_separator_vertex_shader_body);

    ral_shader_set_property(fs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_body);
    ral_shader_set_property(fs_bg,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_bg_body);
    ral_shader_set_property(fs_label_bg,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_label_bg_body);
    ral_shader_set_property(fs_separator,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_separator_body);
    ral_shader_set_property(fs_slider,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &fs_slider_body);
    ral_shader_set_property(vs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_body);
    ral_shader_set_property(vs_separator,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &vs_separator_body);

    /* Set up program objects */
    if (!ral_program_attach_shader(dropdown_ptr->program,
                                   fs) ||
        !ral_program_attach_shader(dropdown_ptr->program,
                                   vs) ||

        !ral_program_attach_shader(dropdown_ptr->program_bg,
                                   fs_bg) ||
        !ral_program_attach_shader(dropdown_ptr->program_bg,
                                   vs) ||

        !ral_program_attach_shader(dropdown_ptr->program_label_bg,
                                   fs_label_bg) ||
        !ral_program_attach_shader(dropdown_ptr->program_label_bg,
                                   vs) ||

        !ral_program_attach_shader(dropdown_ptr->program_separator,
                                   fs_separator) ||
        !ral_program_attach_shader(dropdown_ptr->program_separator,
                                   vs_separator) ||

        !ral_program_attach_shader(dropdown_ptr->program_slider,
                                   fs_slider) ||
        !ral_program_attach_shader(dropdown_ptr->program_slider,
                                   vs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program configuration failed.");
    }


    /* Register the programs with UI so following button instances will reuse the program */
    ui_register_program(ui_instance,
                        ui_dropdown_program_name,
                        dropdown_ptr->program);
    ui_register_program(ui_instance,
                        ui_dropdown_bg_program_name,
                        dropdown_ptr->program_bg);
    ui_register_program(ui_instance,
                        ui_dropdown_label_bg_program_name,
                        dropdown_ptr->program_label_bg);
    ui_register_program(ui_instance,
                        ui_dropdown_separator_program_name,
                        dropdown_ptr->program_separator);
    ui_register_program(ui_instance,
                        ui_dropdown_slider_program_name,
                        dropdown_ptr->program_slider);

    /* Release shaders we will no longer need */
    const ral_shader shaders_to_release[] =
    {
        fs,
        fs_bg,
        fs_label_bg,
        fs_separator,
        fs_slider,
        vs,
        vs_separator
    };
    const uint32_t n_shaders_to_release = sizeof(shaders_to_release) / sizeof(shaders_to_release[0]);

    ral_context_delete_objects(context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               n_shaders_to_release,
                               (const void**) shaders_to_release);
}

/** TODO */
PRIVATE void _ui_dropdown_init_renderer_callback(ogl_context context,
                                                 void*       dropdown)
{
    float                border_width_bg[2]        = {0};
    float                border_width[2]           = {0};
    _ui_dropdown*        dropdown_ptr              = (_ui_dropdown*) dropdown;
    raGL_program         program_bg_raGL           = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                dropdown_ptr->program_bg);
    GLuint               program_bg_raGL_id        = 0;
    raGL_program         program_raGL              = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                dropdown_ptr->program);
    GLuint               program_raGL_id           = 0;
    raGL_program         program_label_bg_raGL     = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                dropdown_ptr->program_label_bg);
    GLuint               program_label_bg_raGL_id  = 0;
    raGL_program         program_separator_raGL    = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                dropdown_ptr->program_separator);
    GLuint               program_separator_raGL_id = 0;
    raGL_program         program_slider_raGL       = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                dropdown_ptr->program_slider);
    GLuint               program_slider_raGL_id    = 0;
    static const GLfloat stop_data[]               = {0,     174.0f / 255.0f * 0.35f, 188.0f / 255.0f * 0.35f, 191.0f / 255.0f * 0.35f,
                                                      0.5f,  110.0f / 255.0f * 0.35f, 119.0f / 255.0f * 0.35f, 116.0f / 255.0f * 0.35f,
                                                      0.51f, 10.0f  / 255.0f * 0.35f, 14.0f  / 255.0f * 0.35f, 10.0f  / 255.0f * 0.35f,
                                                      1.0f,  10.0f  / 255.0f * 0.35f, 8.0f   / 255.0f * 0.35f, 9.0f   / 255.0f * 0.35f};
    system_window        window                    = NULL;
    int                  window_size[2]            = {0};

    raGL_program_get_property(program_bg_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_bg_raGL_id);
    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);
    raGL_program_get_property(program_label_bg_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_label_bg_raGL_id);
    raGL_program_get_property(program_separator_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_separator_raGL_id);
    raGL_program_get_property(program_slider_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_slider_raGL_id);

    ral_context_get_property  (dropdown_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    border_width_bg[0] =  1.0f / (float)((dropdown_ptr->drop_x1y2x2y1[2] - dropdown_ptr->drop_x1y2x2y1[0]) * window_size[0]);
    border_width_bg[1] = -1.0f / (float)((dropdown_ptr->drop_x1y2x2y1[3] - dropdown_ptr->drop_x1y2x2y1[1]) * window_size[1]);
    border_width   [0] =  1.0f / (float)((dropdown_ptr->x1y1x2y2     [2] - dropdown_ptr->x1y1x2y2     [0]) * window_size[0]);
    border_width   [1] =  1.0f / (float)((dropdown_ptr->x1y1x2y2     [3] - dropdown_ptr->x1y1x2y2     [1]) * window_size[1]);

    /* Retrieve UBOs */
    ral_buffer program_bg_ub_fs_bo_ral        = NULL;
    ral_buffer program_bg_ub_vs_bo_ral        = NULL;
    ral_buffer program_label_bg_ub_vs_bo_ral  = NULL;
    ral_buffer program_separator_ub_vs_bo_ral = NULL;
    ral_buffer program_slider_ub_fs_bo_ral    = NULL;
    ral_buffer program_slider_ub_vs_bo_ral    = NULL;
    ral_buffer program_ub_fs_bo_ral           = NULL;
    ral_buffer program_ub_vs_bo_ral           = NULL;

    dropdown_ptr->program_ub_fs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                  dropdown_ptr->program,
                                                                  system_hashed_ansi_string_create("dataFS") );
    dropdown_ptr->program_ub_vs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                  dropdown_ptr->program,
                                                                  system_hashed_ansi_string_create("dataVS") );

    dropdown_ptr->program_bg_ub_fs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                     dropdown_ptr->program_bg,
                                                                     system_hashed_ansi_string_create("dataFS") );
    dropdown_ptr->program_bg_ub_vs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                     dropdown_ptr->program_bg,
                                                                     system_hashed_ansi_string_create("dataVS") );

    dropdown_ptr->program_label_bg_ub_vs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                           dropdown_ptr->program_label_bg,
                                                                           system_hashed_ansi_string_create("dataVS") );

    dropdown_ptr->program_separator_ub_vs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                            dropdown_ptr->program_separator,
                                                                            system_hashed_ansi_string_create("dataVS") );

    dropdown_ptr->program_slider_ub_fs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                         dropdown_ptr->program_slider,
                                                                         system_hashed_ansi_string_create("dataFS") );
    dropdown_ptr->program_slider_ub_vs = ral_program_block_buffer_create(dropdown_ptr->context,
                                                                         dropdown_ptr->program_slider,
                                                                         system_hashed_ansi_string_create("dataVS") );

    ral_program_block_buffer_get_property(dropdown_ptr->program_bg_ub_fs,        RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_bg_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_bg_ub_vs,        RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_bg_ub_vs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_label_bg_ub_vs,  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_label_bg_ub_vs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_separator_ub_vs, RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_separator_ub_vs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_slider_ub_fs,    RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_slider_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_slider_ub_vs,    RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_slider_ub_vs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_ub_fs,           RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_ub_vs,           RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL, &program_ub_vs_bo_ral);

    ral_buffer_get_property(program_ub_fs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_ub_fs_bo_size);
    ral_buffer_get_property(program_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_ub_vs_bo_size);

    ral_buffer_get_property(program_bg_ub_fs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_bg_ub_fs_bo_size);
    ral_buffer_get_property(program_bg_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_bg_ub_vs_bo_size);

    ral_buffer_get_property(program_label_bg_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_label_bg_ub_vs_bo_size);

    ral_buffer_get_property(program_separator_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_separator_ub_vs_bo_size);

    ral_buffer_get_property(program_slider_ub_fs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_slider_ub_fs_bo_size);
    ral_buffer_get_property(program_slider_ub_vs_bo_ral,
                            RAL_BUFFER_PROPERTY_SIZE,
                           &dropdown_ptr->program_slider_ub_vs_bo_size);

    /* Set up uniform block->buffer binding points mappings.
     *
     * This actually needs not be repeated more than once, but the cost is negligible.
     */
    const ral_program programs[] =
    {
        dropdown_ptr->program,
        dropdown_ptr->program_bg,
        dropdown_ptr->program_label_bg,
        dropdown_ptr->program_separator,
        dropdown_ptr->program_slider
    };
    const unsigned int n_programs = sizeof(programs) / sizeof(programs[0]);

    for (unsigned int n_program = 0;
                      n_program < n_programs;
                    ++n_program)
    {
        const ral_program     current_program         = programs[n_program];
        const raGL_program    current_program_raGL    = ral_context_get_program_gl(dropdown_ptr->context,
                                                                                   current_program);
        uint32_t              current_program_raGL_id = 0;
        static const uint32_t fs_ub_bp                = UB_FSDATA_BP;
        static const uint32_t vs_ub_bp                = UB_VSDATA_BP;

        if (ral_program_is_block_defined(current_program,
                                         system_hashed_ansi_string_create("dataFS") ))
        {
            raGL_program_set_block_property_by_name(current_program_raGL,
                                                    system_hashed_ansi_string_create("dataFS"),
                                                    RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                                   &fs_ub_bp);
        }

        if (ral_program_is_block_defined(current_program,
                                         system_hashed_ansi_string_create("dataVS") ))
        {
            raGL_program_set_block_property_by_name(current_program_raGL,
                                                    system_hashed_ansi_string_create("dataVS"),
                                                    RAGL_PROGRAM_BLOCK_PROPERTY_INDEXED_BP,
                                                   &vs_ub_bp);
        }
    } /* for (all programs) */

    /* Retrieve uniform locations */
    const ral_program_variable* border_width_uniform_ral_ptr              = NULL;
    const ral_program_variable* border_width_bg_uniform_ral_ptr           = NULL;
    const ral_program_variable* brightness_uniform_ral_ptr                = NULL;
    const ral_program_variable* button_start_u_uniform_ral_ptr            = NULL;
    const ral_program_variable* button_start_uv_uniform_ral_ptr           = NULL;
    const ral_program_variable* color_uniform_ral_ptr                     = NULL;
    const ral_program_variable* highlighted_v1v2_uniform_ral_ptr          = NULL;
    const ral_program_variable* selected_v1v2_uniform_ral_ptr             = NULL;
    const ral_program_variable* stop_data_uniform_ral_ptr                 = NULL;
    const ral_program_variable* x1_x2_y_uniform_ral_ptr                   = NULL;
    const ral_program_variable* x1y1x2y2_program_bg_uniform_ral_ptr       = NULL;
    const ral_program_variable* x1y1x2y2_program_label_bg_uniform_ral_ptr = NULL;
    const ral_program_variable* x1y1x2y2_program_slider_uniform_ral_ptr   = NULL;
    const ral_program_variable* x1y1x2y2_program_uniform_ral_ptr          = NULL;

    ral_program_get_block_variable_by_name(dropdown_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("button_start_u"),
                                          &button_start_u_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("brightness"),
                                          &brightness_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("stop_data[0]"),
                                          &stop_data_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_program_uniform_ral_ptr);

    ral_program_get_block_variable_by_name(dropdown_ptr->program_bg,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("border_width"),
                                          &border_width_bg_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program_bg,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("button_start_uv"),
                                          &button_start_uv_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program_bg,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("highlighted_v1v2"),
                                          &highlighted_v1v2_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program_bg,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("selected_v1v2"),
                                          &selected_v1v2_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program_bg,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_program_bg_uniform_ral_ptr);

    ral_program_get_block_variable_by_name(dropdown_ptr->program_label_bg,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_program_label_bg_uniform_ral_ptr);

    ral_program_get_block_variable_by_name(dropdown_ptr->program_separator,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1_x2_y"),
                                          &x1_x2_y_uniform_ral_ptr);

    ral_program_get_block_variable_by_name(dropdown_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataFS"),
                                           system_hashed_ansi_string_create("color"),
                                          &color_uniform_ral_ptr);
    ral_program_get_block_variable_by_name(dropdown_ptr->program_slider,
                                           system_hashed_ansi_string_create("dataVS"),
                                           system_hashed_ansi_string_create("x1y1x2y2"),
                                          &x1y1x2y2_program_slider_uniform_ral_ptr);

    dropdown_ptr->program_bg_border_width_ub_offset     = border_width_bg_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_bg_highlighted_v1v2_ub_offset = highlighted_v1v2_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_bg_selected_v1v2_ub_offset    = selected_v1v2_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_bg_x1y1x2y2_ub_offset         = x1y1x2y2_program_bg_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_border_width_ub_offset        = border_width_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_brightness_ub_offset          = brightness_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_button_start_u_ub_offset      = button_start_u_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_label_bg_x1y1x2y2_ub_offset   = x1y1x2y2_program_label_bg_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_separator_x1_x2_y_ub_offset   = x1_x2_y_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_slider_color_ub_offset        = color_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_slider_x1y1x2y2_ub_offset     = x1y1x2y2_program_slider_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_stop_data_ub_offset           = stop_data_uniform_ral_ptr->block_offset;
    dropdown_ptr->program_x1y1x2y2_ub_offset            = x1y1x2y2_program_uniform_ral_ptr->block_offset;

    /* Set them up */
    const float button_start_uv[] =
    {
        1.0f - BUTTON_WIDTH_PX * border_width[0],
        1.0f - BUTTON_WIDTH_PX * border_width[1]
    };

    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_ub_fs,
                                                           button_start_u_uniform_ral_ptr->block_offset,
                                                          &button_start_uv[0],
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_bg_ub_fs,
                                                           button_start_uv_uniform_ral_ptr->block_offset,
                                                           button_start_uv,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_ub_fs,
                                                           border_width_uniform_ral_ptr->block_offset,
                                                           border_width,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_bg_ub_fs,
                                                           border_width_bg_uniform_ral_ptr->block_offset,
                                                           border_width_bg,
                                                           sizeof(float) * 2);

    ral_program_block_buffer_set_arrayed_variable_value(dropdown_ptr->program_ub_fs,
                                                        stop_data_uniform_ral_ptr->block_offset,
                                                        stop_data,
                                                        sizeof(float) * 4 /* vec4 */ * 4 /* array items */,
                                                        0,                /* dst_array_start_index */
                                                        4);               /* dst_array_item_count */

    dropdown_ptr->current_gpu_brightness_level = NONFOCUSED_BRIGHTNESS;

}

/* TODO */
PRIVATE void _ui_dropdown_update_entry_positions(_ui_dropdown* dropdown_ptr)
{
    system_window context_window = NULL;
    uint32_t      n_entries      = 0;
    int           window_size[2] = {0};

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_entries);

    ral_context_get_property  (dropdown_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &context_window);
    system_window_get_property(context_window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    for (uint32_t n_entry = 0;
                  n_entry < n_entries - 1; /* exclude bar string */
                ++n_entry)
    {
        _ui_dropdown_entry* entry_ptr = NULL;

        if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                   n_entry,
                                                  &entry_ptr) )
        {
            const float slider_height_ss = (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3] + float(SLIDER_Y_SEPARATOR_PX) / window_size[1]);

            int new_xy[] =
            {
                entry_ptr->base_x,
                entry_ptr->base_y + int((dropdown_ptr->slider_delta_y + dropdown_ptr->slider_delta_y_base) / slider_height_ss * (n_entries * dropdown_ptr->separator_delta_y * window_size[1]))
            };

            entry_ptr->text_y = new_xy[1];

            varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                         entry_ptr->string_id,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                         new_xy);
        }
    } /* for (all entries) */
}

/** TODO */
PRIVATE void _ui_dropdown_update_entry_strings(_ui_dropdown* dropdown_ptr,
                                               bool          only_update_selected_entry)
{
    unsigned int n_strings = 0;

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_strings);

    system_window      window         = NULL;
    int                window_size[2] = {0};
    const float        x1y1[2]        =
    {
               dropdown_ptr->x1y1x2y2[0],
        1.0f - dropdown_ptr->x1y1x2y2[3]
    };
    const float x2y2[2] =
    {
               dropdown_ptr->x1y1x2y2[2],
        1.0f - dropdown_ptr->x1y1x2y2[1]
    };

    ral_context_get_property  (dropdown_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    const GLint scissor_box[] = {(GLint) ( dropdown_ptr->drop_x1y2x2y1[0]  * window_size[0]),
                                 (GLint) ((dropdown_ptr->drop_x1y2x2y1[3]) * window_size[1]),
                                 (GLint) ((dropdown_ptr->drop_x1y2x2y1[2] - dropdown_ptr->drop_x1y2x2y1[0]) * window_size[0]),
                                 (GLint) ((dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * window_size[1])};

    for (uint32_t n_entry = 0;
                  n_entry < n_strings;
                ++n_entry)
    {
        _ui_dropdown_entry* entry_ptr = NULL;

        if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                   n_entry,
                                                  &entry_ptr) )
        {
            if (n_entry == (n_strings - 1))
            {
                 dropdown_ptr->current_entry_string_id = entry_ptr->string_id;

                 /* Also update the string that should be shown on the bar */
                 _ui_dropdown_entry* selected_entry_ptr = NULL;

                 if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                            dropdown_ptr->n_selected_entry,
                                                           &selected_entry_ptr) )
                 {
                     ASSERT_DEBUG_SYNC(entry_ptr != selected_entry_ptr,
                                       "Bar string was selected?!");

                     entry_ptr->name = selected_entry_ptr->name;
                 }
            } /* if (n_entry == (n_strings - 1)) */

            if ( only_update_selected_entry && n_entry == (n_strings - 1) ||
                !only_update_selected_entry)
            {
                varia_text_renderer_set(dropdown_ptr->text_renderer,
                                        entry_ptr->string_id,
                                        system_hashed_ansi_string_get_buffer(entry_ptr->name) );

                /* Position the string */
                float         delta_y       = dropdown_ptr->separator_delta_y * float(n_entry + 1);
                int           text_height   = 0;
                int           text_xy[2]    = {0};
                int           text_width    = 0;

                varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                             entry_ptr->string_id,
                                                            &text_height);
                varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                             entry_ptr->string_id,
                                                            &text_width);

                if (n_entry == (n_strings - 1) )
                {
                    delta_y = 0;
                }

                text_xy[0] = (int) ((x1y1[0] + (x2y2[0] - x1y1[0] - float(BUTTON_WIDTH_PX + text_width) / window_size[0])  * 0.5f) * (float) window_size[0]);
                text_xy[1] = (int) ((x2y2[1] - (x2y2[1] - x1y1[1] - float(text_height) / window_size[1]) * 0.5f + delta_y)         * (float) window_size[1]);

                entry_ptr->base_x = text_xy[0];
                entry_ptr->base_y = text_xy[1];

                varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                             entry_ptr->string_id,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                             _ui_dropdown_text_color);
                varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                             entry_ptr->string_id,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                             text_xy);

                if (n_entry != (n_strings - 1) )
                {
                    varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                                 entry_ptr->string_id,
                                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_SCISSOR_BOX,
                                                                 scissor_box);
                }
            } /* if (string should be updated) */
        } /* if (string descriptor was retrieved) */
    } /* for (all text entries) */
}

/** TODO */
PRIVATE void _ui_dropdown_update_entry_visibility(_ui_dropdown* dropdown_ptr)
{
    unsigned int n_entries = 0;

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_entries);

    varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                 dropdown_ptr->current_entry_string_id,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                &dropdown_ptr->visible);

    if (dropdown_ptr->label_string_id != -1)
    {
        varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                     dropdown_ptr->label_string_id,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                    &dropdown_ptr->visible);
    }

    for (unsigned int n_entry = 0;
                      n_entry < n_entries;
                    ++n_entry)
    {
        _ui_dropdown_entry* entry_ptr = NULL;

        if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                   n_entry,
                                                  &entry_ptr) )
        {
            if (n_entry < (n_entries - 1) )
            {
                bool visibility = dropdown_ptr->is_droparea_visible &&
                                  dropdown_ptr->visible;

                varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                             entry_ptr->string_id,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                            &visibility);
            }
            else
            {
                /* Bar string */
                varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                             entry_ptr->string_id,
                                                             VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                            &dropdown_ptr->visible);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve text entry descriptor at index [%d]",
                              n_entry);
        }
    } /* for (all entries) */
}

PRIVATE void _ui_dropdown_update_position(_ui_dropdown* dropdown_ptr,
                                          const float*  x1y1)
{
    system_window window         = NULL;
    int           window_size[2] = {0};

    ral_context_get_property  (dropdown_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    /* Retrieve maximum width & height needed to render the text strings.
     * We will use this data to compute x2y2 */
    unsigned int n_strings       = 0;
    unsigned int text_max_height = 0;
    unsigned int text_max_width  = 0;

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_strings);
    n_strings--;

    for (unsigned int n_entry = 0;
                      n_entry < n_strings;
                    ++n_entry)
    {
        _ui_dropdown_entry* entry_ptr = NULL;

        if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                   n_entry,
                                                  &entry_ptr) )
        {
            const varia_text_renderer_text_string_id& string_id   = entry_ptr->string_id;
            unsigned int                              text_height = 0;
            unsigned int                              text_width  = 0;

            varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                         string_id,
                                                        &text_height);
            varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                         VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                         string_id,
                                                        &text_width);

            if (text_height > text_max_height)
            {
                text_max_height = text_height;
            }

            if (text_width > text_max_width)
            {
                text_max_width = text_width;
            }
        } /* if (entry descriptor was retrieved successfully) */
    } /* for (all text strings) */

    const float x2y2[2] =
    {
        x1y1[0] + float(text_max_width + BUTTON_WIDTH_PX) / window_size[0],
        x1y1[1] + float(text_max_height)                  / window_size[1]
    };

    /* Calculate various vectors needed for rendering */
    dropdown_ptr->separator_delta_y   = fabs(x2y2[1] - x1y1[1]);
    dropdown_ptr->slider_delta_y      = 0;
    dropdown_ptr->slider_delta_y_base = 0;
    dropdown_ptr->slider_height       = float(MAX_N_ENTRIES_VISIBLE) / float(n_strings);
    dropdown_ptr->slider_separator_y  = float(SLIDER_Y_SEPARATOR_PX) / window_size[1];
    dropdown_ptr->x1y1x2y2[0]         =     x1y1[0];
    dropdown_ptr->x1y1x2y2[1]         = 1 - x2y2[1];
    dropdown_ptr->x1y1x2y2[2]         =     x2y2[0];
    dropdown_ptr->x1y1x2y2[3]         = 1 - x1y1[1];
    dropdown_ptr->button_x1y1x2y2[0]  = dropdown_ptr->x1y1x2y2[2] - float(BUTTON_WIDTH_PX) / window_size[0];
    dropdown_ptr->button_x1y1x2y2[1]  = dropdown_ptr->x1y1x2y2[1];
    dropdown_ptr->button_x1y1x2y2[2]  = dropdown_ptr->x1y1x2y2[2];
    dropdown_ptr->button_x1y1x2y2[3]  = dropdown_ptr->x1y1x2y2[3];
    dropdown_ptr->drop_x1y2x2y1[0]    = dropdown_ptr->x1y1x2y2[0];
    dropdown_ptr->drop_x1y2x2y1[1]    = dropdown_ptr->x1y1x2y2[1] + 1.0f / window_size[1];
    dropdown_ptr->drop_x1y2x2y1[2]    = dropdown_ptr->x1y1x2y2[2];
    dropdown_ptr->drop_x1y2x2y1[3]    = dropdown_ptr->drop_x1y2x2y1[1] - float((MAX_N_ENTRIES_VISIBLE) * dropdown_ptr->separator_delta_y);

    if (dropdown_ptr->slider_height > 1.0f)
    {
        dropdown_ptr->slider_height = 1.0f;
    }

    /* Update slider position */
    const float droparea_button_start_u = dropdown_ptr->drop_x1y2x2y1[2] - float(BUTTON_WIDTH_PX) / float(window_size[0]);

    dropdown_ptr->slider_x1x2[0] = droparea_button_start_u      + (BUTTON_WIDTH_PX - float(SLIDER_WIDTH_PX)) / float(window_size[0]) * 0.5f;
    dropdown_ptr->slider_x1x2[1] = dropdown_ptr->slider_x1x2[0] + float(SLIDER_WIDTH_PX)                     / float(window_size[0]);

    /* We can now position the text strings */
    _ui_dropdown_update_entry_strings   (dropdown_ptr, false);
    _ui_dropdown_update_entry_positions (dropdown_ptr);
    _ui_dropdown_update_entry_visibility(dropdown_ptr);

    /* Set up the label */
    unsigned int text_height = 0;
    unsigned int text_width  = 0;
             int text_xy[2]  = {0};
       const int x1y1_ss[2]  =
    {
        int(x1y1[0] * float(window_size[0]) ),
        int(x1y1[1] * float(window_size[1]))
    };
       const int y2_ss = int(x2y2[1] * float(window_size[1]));

    if (dropdown_ptr->label_string_id == -1)
    {
        dropdown_ptr->label_string_id = varia_text_renderer_add_string(dropdown_ptr->text_renderer);
    }

    varia_text_renderer_set(dropdown_ptr->text_renderer,
                            dropdown_ptr->label_string_id,
                            system_hashed_ansi_string_get_buffer(dropdown_ptr->label_text) );

    varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                 dropdown_ptr->label_string_id,
                                                &text_height);
    varia_text_renderer_get_text_string_property(dropdown_ptr->text_renderer,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                 dropdown_ptr->label_string_id,
                                                &text_width);
    varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                 dropdown_ptr->label_string_id,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_COLOR,
                                                 _ui_dropdown_label_text_color);

    text_xy[0] = x1y1_ss[0] - text_width - LABEL_X_SEPARATOR_PX;
    text_xy[1] = x1y1_ss[1] + text_height + ((y2_ss - x1y1_ss[1]) - int(text_height)) / 2;

    varia_text_renderer_set_text_string_property(dropdown_ptr->text_renderer,
                                                 dropdown_ptr->label_string_id,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_POSITION_PX,
                                                 text_xy);

    dropdown_ptr->label_x1y1[0] = float(text_xy[0]) / float(window_size[0]);
    dropdown_ptr->label_x1y1[1] = 1.0f - float(text_xy[1]) / float(window_size[1]);

    /* Set up coordinates of the dimmed background underneath the label */
    text_xy[0] -= LABEL_X_SEPARATOR_PX * 2;

    dropdown_ptr->label_bg_x1y1x2y2[0] =        float(text_xy[0])               / float(window_size[0]);
    dropdown_ptr->label_bg_x1y1x2y2[1] = 1.0f - float(y2_ss) / float(window_size[1]);
    dropdown_ptr->label_bg_x1y1x2y2[2] = x1y1[0];
    dropdown_ptr->label_bg_x1y1x2y2[3] = 1.0f - x1y1[1];
}

/** Please see header for specification */
PUBLIC void ui_dropdown_deinit(void* internal_instance)
{
    _ui_dropdown*     ui_dropdown_ptr  = (_ui_dropdown*) internal_instance;
    const static bool visibility_false = false;

    ral_program       programs_to_release[] =
    {
        ui_dropdown_ptr->program,
        ui_dropdown_ptr->program_bg,
        ui_dropdown_ptr->program_label_bg,
        ui_dropdown_ptr->program_separator,
        ui_dropdown_ptr->program_slider
    };
    const uint32_t n_programs_to_release = sizeof(programs_to_release) / sizeof(programs_to_release[0]);

    ral_context_delete_objects(ui_dropdown_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                               n_programs_to_release,
                               (const void**) programs_to_release);

    /* Release all entry instances */
    _ui_dropdown_entry* entry_ptr = NULL;

    varia_text_renderer_set_text_string_property(ui_dropdown_ptr->text_renderer,
                                                 ui_dropdown_ptr->current_entry_string_id,
                                                 VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                &visibility_false);

    while (system_resizable_vector_pop(ui_dropdown_ptr->entries, &entry_ptr) )
    {
        varia_text_renderer_set_text_string_property(ui_dropdown_ptr->text_renderer,
                                                     entry_ptr->string_id,
                                                     VARIA_TEXT_RENDERER_TEXT_STRING_PROPERTY_VISIBILITY,
                                                    &visibility_false);

        /* Go ahead with the release */
        delete entry_ptr;

        entry_ptr = NULL;
    }
    system_resizable_vector_release(ui_dropdown_ptr->entries);
    ui_dropdown_ptr->entries = NULL;

    ral_program_block_buffer block_buffers[] =
    {
        ui_dropdown_ptr->program_ub_fs,
        ui_dropdown_ptr->program_ub_vs,
        ui_dropdown_ptr->program_bg_ub_fs,
        ui_dropdown_ptr->program_bg_ub_vs,
        ui_dropdown_ptr->program_label_bg_ub_vs,
        ui_dropdown_ptr->program_separator_ub_vs,
        ui_dropdown_ptr->program_slider_ub_fs,
        ui_dropdown_ptr->program_slider_ub_vs,
    };
    const uint32_t n_block_buffers = sizeof(block_buffers) / sizeof(block_buffers[0]);

    for (uint32_t n_block_buffer = 0;
                  n_block_buffer < n_block_buffers;
                ++n_block_buffer)
    {
        if (block_buffers[n_block_buffer] != NULL)
        {
            ral_program_block_buffer_release(block_buffers[n_block_buffer]);
        }
    }

    delete ui_dropdown_ptr;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void ui_dropdown_draw(void* internal_instance)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;
    system_time   time_now     = system_time_now();
    system_window window       = NULL;
    int           window_size[2];

    ral_context_get_property  (dropdown_ptr->context,
                               RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                              &window);
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                               window_size);

    if (!dropdown_ptr->visible)
    {
        return;
    }

    /* Prepare for rendeirng */
    dropdown_ptr->pGLDisable(GL_DEPTH_TEST);

    /* Update brightness if necessary */
    float brightness = dropdown_ptr->current_gpu_brightness_level;

    if (dropdown_ptr->is_hovering)
    {
        /* Are we transiting? */
        system_time transition_start = dropdown_ptr->start_hovering_time;
        system_time transition_end   = dropdown_ptr->start_hovering_time + NONFOCUSED_TO_FOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) / float(NONFOCUSED_TO_FOCUSED_TRANSITION_TIME);

            brightness = dropdown_ptr->start_hovering_brightness + dt * (FOCUSED_BRIGHTNESS - NONFOCUSED_BRIGHTNESS);

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
        system_time transition_start = dropdown_ptr->start_hovering_time;
        system_time transition_end   = dropdown_ptr->start_hovering_time + FOCUSED_TO_NONFOCUSED_TRANSITION_TIME;

        if (time_now >= transition_start &&
            time_now <= transition_end)
        {
            float dt = float(time_now - transition_start) / float(FOCUSED_TO_NONFOCUSED_TRANSITION_TIME);

            brightness = dropdown_ptr->start_hovering_brightness + dt * (NONFOCUSED_BRIGHTNESS - FOCUSED_BRIGHTNESS);

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
    float              highlighted_v1v2[2];
    float              selected_v1v2   [2];
    const raGL_program program_bg_raGL           = ral_context_get_program_gl(dropdown_ptr->context,
                                                                              dropdown_ptr->program_bg);
    GLuint             program_bg_raGL_id        = 0;
    const raGL_program program_raGL              = ral_context_get_program_gl(dropdown_ptr->context,
                                                                              dropdown_ptr->program);
    GLuint             program_raGL_id           = 0;
    const raGL_program program_label_bg_raGL     = ral_context_get_program_gl(dropdown_ptr->context,
                                                                              dropdown_ptr->program_label_bg);
    GLuint             program_label_bg_raGL_id  = 0;
    const raGL_program program_separator_raGL    = ral_context_get_program_gl(dropdown_ptr->context,
                                                                              dropdown_ptr->program_separator);
    GLuint             program_separator_raGL_id = 0;
    const raGL_program program_slider_raGL       = ral_context_get_program_gl(dropdown_ptr->context,
                                                                              dropdown_ptr->program_slider);
    GLuint             program_slider_raGL_id    = 0;
    uint32_t           n_entries                 = 0;

    raGL_program_get_property(program_bg_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_bg_raGL_id);
    raGL_program_get_property(program_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_raGL_id);
    raGL_program_get_property(program_label_bg_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_label_bg_raGL_id);
    raGL_program_get_property(program_separator_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_separator_raGL_id);
    raGL_program_get_property(program_slider_raGL,
                              RAGL_PROGRAM_PROPERTY_ID,
                             &program_slider_raGL_id);

    system_resizable_vector_get_property(dropdown_ptr->entries,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_entries);

    _ui_dropdown_get_highlighted_v1v2(dropdown_ptr, false, highlighted_v1v2);
    _ui_dropdown_get_selected_v1v2   (dropdown_ptr,        selected_v1v2);

    if (fabs(highlighted_v1v2[0]  - selected_v1v2[0]) <  1e-5f                             && /* exclude the area outside the drop area, */
        fabs(highlighted_v1v2[1]  - selected_v1v2[1]) <  1e-5f                             ||
        dropdown_ptr->hover_ss[0]                     <= dropdown_ptr->drop_x1y2x2y1[0]    || /* exclude the slider area */
        dropdown_ptr->hover_ss[0]                     >= dropdown_ptr->button_x1y1x2y2[0])
    {
        if ( dropdown_ptr->is_lbm_on && !dropdown_ptr->is_droparea_lbm ||
            !dropdown_ptr->is_lbm_on)
        {
            highlighted_v1v2[0] = 0.0f;
            highlighted_v1v2[1] = 0.0f;
        }
    }

    if (dropdown_ptr->current_gpu_brightness_level != brightness ||
        dropdown_ptr->force_gpu_brightness_update)
    {
        dropdown_ptr->current_gpu_brightness_level = brightness;
        dropdown_ptr->force_gpu_brightness_update  = false;
    }

    const float new_brightness_uniform_value = brightness * ((dropdown_ptr->is_lbm_on && dropdown_ptr->is_button_lbm) ? CLICK_BRIGHTNESS_MODIFIER : 1);

    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_ub_fs,
                                                           dropdown_ptr->program_brightness_ub_offset,
                                                          &new_brightness_uniform_value,
                                                           sizeof(float) );
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_ub_vs,
                                                           dropdown_ptr->program_x1y1x2y2_ub_offset,
                                                           dropdown_ptr->x1y1x2y2,
                                                           sizeof(float) * 4);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_bg_ub_fs,
                                                           dropdown_ptr->program_bg_highlighted_v1v2_ub_offset,
                                                           highlighted_v1v2,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_bg_ub_fs,
                                                           dropdown_ptr->program_bg_selected_v1v2_ub_offset,
                                                           selected_v1v2,
                                                           sizeof(float) * 2);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_bg_ub_vs,
                                                           dropdown_ptr->program_bg_x1y1x2y2_ub_offset,
                                                           dropdown_ptr->drop_x1y2x2y1,
                                                           sizeof(float) * 4);
    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_label_bg_ub_vs,
                                                           dropdown_ptr->program_label_bg_x1y1x2y2_ub_offset,
                                                           dropdown_ptr->label_bg_x1y1x2y2,
                                                           sizeof(float) * 4);

    /* Draw */
    if (dropdown_ptr->is_droparea_visible)
    {
        GLuint      program_bg_ub_fs_bo_id                  = 0;
        raGL_buffer program_bg_ub_fs_bo_raGL                = NULL;
        ral_buffer  program_bg_ub_fs_bo_ral                 = NULL;
        uint32_t    program_bg_ub_fs_bo_start_offset        = -1;
        GLuint      program_bg_ub_vs_bo_id                  = 0;
        raGL_buffer program_bg_ub_vs_bo_raGL                = NULL;
        ral_buffer  program_bg_ub_vs_bo_ral                 = NULL;
        uint32_t    program_bg_ub_vs_bo_start_offset        = -1;
        GLuint      program_separator_ub_vs_bo_id           = 0;
        raGL_buffer program_separator_ub_vs_bo_raGL         = NULL;
        ral_buffer  program_separator_ub_vs_bo_ral          = NULL;
        uint32_t    program_separator_ub_vs_bo_start_offset = -1;

        ral_program_block_buffer_get_property(dropdown_ptr->program_bg_ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_bg_ub_fs_bo_ral);
        ral_program_block_buffer_get_property(dropdown_ptr->program_bg_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_bg_ub_vs_bo_ral);
        ral_program_block_buffer_get_property(dropdown_ptr->program_separator_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_separator_ub_vs_bo_ral);

        program_bg_ub_fs_bo_raGL        = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                                    program_bg_ub_fs_bo_ral);
        program_bg_ub_vs_bo_raGL        = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                                    program_bg_ub_vs_bo_ral);
        program_separator_ub_vs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                                    program_separator_ub_vs_bo_ral);

        raGL_buffer_get_property(program_bg_ub_fs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &program_bg_ub_fs_bo_id);
        raGL_buffer_get_property(program_bg_ub_fs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &program_bg_ub_fs_bo_start_offset);
        raGL_buffer_get_property(program_bg_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &program_bg_ub_vs_bo_id);
        raGL_buffer_get_property(program_bg_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &program_bg_ub_vs_bo_start_offset);

        raGL_buffer_get_property(program_separator_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &program_separator_ub_vs_bo_id);
        raGL_buffer_get_property(program_separator_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &program_separator_ub_vs_bo_start_offset);

        /* Background first */
        dropdown_ptr->pGLUseProgram     (program_bg_raGL_id);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_FSDATA_BP,
                                         program_bg_ub_fs_bo_id,
                                         program_bg_ub_fs_bo_start_offset,
                                         dropdown_ptr->program_bg_ub_fs_bo_size);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_VSDATA_BP,
                                         program_bg_ub_vs_bo_id,
                                         program_bg_ub_vs_bo_start_offset,
                                         dropdown_ptr->program_bg_ub_vs_bo_size);

        ral_program_block_buffer_sync(dropdown_ptr->program_bg_ub_fs);
        ral_program_block_buffer_sync(dropdown_ptr->program_bg_ub_vs);

        dropdown_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0, /* first */
                                    4);/* count */

        /* Follow with separators */
        dropdown_ptr->pGLUseProgram     (program_separator_raGL_id);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_VSDATA_BP,
                                         program_separator_ub_vs_bo_id,
                                         program_separator_ub_vs_bo_start_offset,
                                         dropdown_ptr->program_separator_ub_vs_bo_size);

        unsigned int n_separators     = 0;
        const float  slider_height_ss = (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * (1.0f - dropdown_ptr->slider_height);

        system_resizable_vector_get_property(dropdown_ptr->entries,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_separators);

        for (unsigned int n_entry = 0;
                          n_entry < n_entries;
                        ++n_entry)
        {
            _ui_dropdown_entry* entry_ptr = NULL;

            if (system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                       n_entry,
                                                      &entry_ptr) )
            {
                float x1_x2_y[] =
                {
                    -1.0f + 2.0f * dropdown_ptr->drop_x1y2x2y1[0],
                    -1.0f + 2.0f * dropdown_ptr->button_x1y1x2y2[0],
                    (1.0f - entry_ptr->text_y / float(window_size[1]))
                };

                if (x1_x2_y[2] >= dropdown_ptr->drop_x1y2x2y1[3] &&
                    x1_x2_y[2] <= dropdown_ptr->drop_x1y2x2y1[1])
                {
                    x1_x2_y[2] = -1.0f + 2.0f * x1_x2_y[2];

                    ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_separator_ub_vs,
                                                                           dropdown_ptr->program_separator_x1_x2_y_ub_offset,
                                                                           x1_x2_y,
                                                                           sizeof(float) * 3);

                    /* TODO: Improve! */
                    ral_program_block_buffer_sync(dropdown_ptr->program_separator_ub_vs);

                    dropdown_ptr->pGLDrawArrays(GL_LINES,
                                                0, /* first */
                                                2);
                }
            }
        }

        /* Draw the slider */
        GLuint      program_slider_ub_fs_bo_id           = 0;
        raGL_buffer program_slider_ub_fs_bo_raGL         = NULL;
        ral_buffer  program_slider_ub_fs_bo_ral          = NULL;
        uint32_t    program_slider_ub_fs_bo_start_offset = -1;
        GLuint      program_slider_ub_vs_bo_id           = 0;
        raGL_buffer program_slider_ub_vs_bo_raGL         = NULL;
        ral_buffer  program_slider_ub_vs_bo_ral          = NULL;
        uint32_t    program_slider_ub_vs_bo_start_offset = -1;
        float       slider_x1y1x2y2[4];

        ral_program_block_buffer_get_property(dropdown_ptr->program_slider_ub_fs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_slider_ub_fs_bo_ral);
        ral_program_block_buffer_get_property(dropdown_ptr->program_slider_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_slider_ub_vs_bo_ral);

        program_slider_ub_fs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                                 program_slider_ub_fs_bo_ral);
        program_slider_ub_vs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
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

        _ui_dropdown_get_slider_x1y1x2y2(dropdown_ptr,
                                         slider_x1y1x2y2);

        bool        is_cursor_over_slider = (dropdown_ptr->hover_ss[0] >= slider_x1y1x2y2[0] && dropdown_ptr->hover_ss[0] <= slider_x1y1x2y2[2] &&
                                             dropdown_ptr->hover_ss[1] >= slider_x1y1x2y2[3] && dropdown_ptr->hover_ss[1] <= slider_x1y1x2y2[1] ||
                                             dropdown_ptr->is_slider_lbm);
        const float slider_color[4] =
        {
            is_cursor_over_slider ? 1.0f : 0.5f,
            is_cursor_over_slider ? 1.0f : 0.5f,
            is_cursor_over_slider ? 1.0f : 0.5f,
            1.0f
        };

        dropdown_ptr->pGLUseProgram     (program_slider_raGL_id);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_FSDATA_BP,
                                         program_slider_ub_fs_bo_id,
                                         program_slider_ub_fs_bo_start_offset,
                                         dropdown_ptr->program_slider_ub_fs_bo_size);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_VSDATA_BP,
                                         program_slider_ub_vs_bo_id,
                                         program_slider_ub_vs_bo_start_offset,
                                         dropdown_ptr->program_slider_ub_vs_bo_size);

        ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_slider_ub_fs,
                                                               dropdown_ptr->program_slider_color_ub_offset,
                                                               slider_color,
                                                               sizeof(float) * 4);
        ral_program_block_buffer_set_nonarrayed_variable_value(dropdown_ptr->program_slider_ub_vs,
                                                               dropdown_ptr->program_slider_x1y1x2y2_ub_offset,
                                                               slider_x1y1x2y2,
                                                               sizeof(float) * 4);

        ral_program_block_buffer_sync(dropdown_ptr->program_slider_ub_fs);
        ral_program_block_buffer_sync(dropdown_ptr->program_slider_ub_vs);

        dropdown_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0,  /* first */
                                    4); /* count */
    }

    /* Draw the bar */
    GLuint      program_ub_fs_bo_id           = 0;
    raGL_buffer program_ub_fs_bo_raGL         = NULL;
    ral_buffer  program_ub_fs_bo_ral          = NULL;
    uint32_t    program_ub_fs_bo_start_offset = -1;
    GLuint      program_ub_vs_bo_id           = 0;
    raGL_buffer program_ub_vs_bo_raGL         = NULL;
    ral_buffer  program_ub_vs_bo_ral          = NULL;
    uint32_t    program_ub_vs_bo_start_offset = -1;

    ral_program_block_buffer_get_property(dropdown_ptr->program_ub_fs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_fs_bo_ral);
    ral_program_block_buffer_get_property(dropdown_ptr->program_ub_vs,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &program_ub_vs_bo_ral);

    program_ub_fs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                      program_ub_fs_bo_ral);
    program_ub_vs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
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

    dropdown_ptr->pGLUseProgram     (program_raGL_id);
    dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     UB_FSDATA_BP,
                                     program_ub_fs_bo_id,
                                     program_ub_fs_bo_start_offset,
                                     dropdown_ptr->program_ub_fs_bo_size);
    dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                     UB_VSDATA_BP,
                                     program_ub_vs_bo_id,
                                     program_ub_vs_bo_start_offset,
                                     dropdown_ptr->program_ub_vs_bo_size);

    ral_program_block_buffer_sync(dropdown_ptr->program_ub_fs);
    ral_program_block_buffer_sync(dropdown_ptr->program_ub_vs);

    dropdown_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                0,  /* first */
                                4); /* count */

    /* Draw the label background */
    dropdown_ptr->pGLEnable(GL_BLEND);
    {
        GLuint      program_label_bg_ub_vs_bo_id                = 0;
        raGL_buffer program_label_bg_ub_vs_bo_raGL              = NULL;
        uint32_t    program_label_bg_ub_vs_bo_raGL_start_offset = -1;
        ral_buffer  program_label_bg_ub_vs_bo_ral               = NULL;
        uint32_t    program_label_bg_ub_vs_bo_ral_start_offset  = -1;

        ral_program_block_buffer_get_property(dropdown_ptr->program_label_bg_ub_vs,
                                              RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                             &program_label_bg_ub_vs_bo_ral);

        program_label_bg_ub_vs_bo_raGL = ral_context_get_buffer_gl(dropdown_ptr->context,
                                                                   program_label_bg_ub_vs_bo_ral);

        raGL_buffer_get_property(program_label_bg_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &program_label_bg_ub_vs_bo_id);
        raGL_buffer_get_property(program_label_bg_ub_vs_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &program_label_bg_ub_vs_bo_raGL_start_offset);
        ral_buffer_get_property (program_label_bg_ub_vs_bo_ral,
                                 RAL_BUFFER_PROPERTY_START_OFFSET,
                                &program_label_bg_ub_vs_bo_ral_start_offset);

        dropdown_ptr->pGLUseProgram     (program_label_bg_raGL_id);
        dropdown_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         UB_VSDATA_BP,
                                         program_label_bg_ub_vs_bo_id,
                                         program_label_bg_ub_vs_bo_raGL_start_offset + program_label_bg_ub_vs_bo_ral_start_offset,
                                         dropdown_ptr->program_label_bg_ub_vs_bo_size);

        ral_program_block_buffer_sync(dropdown_ptr->program_label_bg_ub_vs);

        dropdown_ptr->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0,  /* first */
                                    4); /* count */
    }
    dropdown_ptr->pGLDisable(GL_BLEND);
}

/** Please see header for specification */
PUBLIC void ui_dropdown_get_property(const void*         dropdown,
                                     ui_control_property property,
                                     void*               out_result)
{
    const _ui_dropdown* dropdown_ptr = (const _ui_dropdown*) dropdown;

    switch (property)
    {
        case UI_CONTROL_PROPERTY_DROPDOWN_IS_DROPAREA_VISIBLE:
        {
            *(bool*) out_result = dropdown_ptr->is_droparea_visible;

            break;
        }

        case UI_CONTROL_PROPERTY_DROPDOWN_LABEL_BG_X1Y1X2Y2:
        {
            memcpy(out_result,
                   dropdown_ptr->label_bg_x1y1x2y2,
                   sizeof(dropdown_ptr->label_bg_x1y1x2y2) );

            break;
        }

        case UI_CONTROL_PROPERTY_DROPDOWN_LABEL_X1Y1:
        {
            memcpy(out_result,
                   dropdown_ptr->label_x1y1,
                   sizeof(dropdown_ptr->label_x1y1) );

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED:
        {
            if (dropdown_ptr->visible)
            {
                if (!dropdown_ptr->is_droparea_visible)
                {
                    *(float*) out_result = dropdown_ptr->x1y1x2y2[3] - dropdown_ptr->x1y1x2y2[1];
                }
                else
                {
                    *(float*) out_result = dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3] +
                                           dropdown_ptr->x1y1x2y2     [3] - dropdown_ptr->x1y1x2y2     [1];
                }
            } /* if (dropdown_ptr->visible) */
            else
            {
                *(float*) out_result = 0.0f;
            }

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_WIDTH_NORMALIZED:
        {
            *(float*) out_result = dropdown_ptr->x1y1x2y2[2] - dropdown_ptr->label_bg_x1y1x2y2[0];

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            *(bool*) out_result = dropdown_ptr->visible;

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        {
            float* result = (float*) out_result;

            result[0] = dropdown_ptr->label_bg_x1y1x2y2[0];
            result[1] = dropdown_ptr->label_bg_x1y1x2y2[1];

            break;
        }

        case UI_CONTROL_PROPERTY_DROPDOWN_X1Y1X2Y2:
        {
            /* NOTE: Update UI_CONTROL_PROPERTY_GENERAL_HEIGHT_NORMALIZED handler if this changes */
            float* result = (float*) out_result;

            if (!dropdown_ptr->is_droparea_visible)
            {
                result[0] = dropdown_ptr->x1y1x2y2[0];
                result[1] = dropdown_ptr->x1y1x2y2[1];
                result[2] = dropdown_ptr->x1y1x2y2[2];
                result[3] = dropdown_ptr->x1y1x2y2[3];
            }
            else
            {
                result[0] = dropdown_ptr->drop_x1y2x2y1[0];
                result[1] = dropdown_ptr->drop_x1y2x2y1[3];
                result[2] = dropdown_ptr->x1y1x2y2[2];
                result[3] = dropdown_ptr->x1y1x2y2[1];
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property property requested");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC void* ui_dropdown_init(ui                         instance,
                              varia_text_renderer        text_renderer,
                              system_hashed_ansi_string  label_text,
                              uint32_t                   n_strings,
                              system_hashed_ansi_string* strings,
                              void**                     user_args,
                              uint32_t                   n_selected_entry,
                              system_hashed_ansi_string  name,
                              const float*               x1y1,
                              PFNUIFIREPROCPTR           pfn_fire_proc_ptr,
                              void*                      fire_proc_user_arg,
                              ui_control                 owner_control)
{
    _ui_dropdown* new_dropdown_ptr = new (std::nothrow) _ui_dropdown;

    ASSERT_ALWAYS_SYNC(new_dropdown_ptr != NULL,
                       "Out of memory");

    if (new_dropdown_ptr != NULL)
    {
        /* Initialize fields */
        memset(new_dropdown_ptr,
               0,
               sizeof(_ui_dropdown) );

        new_dropdown_ptr->context             = ui_get_context                (instance);
        new_dropdown_ptr->entries             = system_resizable_vector_create(4 /* capacity */);
        new_dropdown_ptr->fire_proc_user_arg  = fire_proc_user_arg;
        new_dropdown_ptr->is_button_lbm       = false;
        new_dropdown_ptr->is_droparea_lbm     = false;
        new_dropdown_ptr->is_lbm_on           = false;
        new_dropdown_ptr->is_droparea_visible = false;
        new_dropdown_ptr->is_slider_lbm       = false;
        new_dropdown_ptr->label_string_id     = -1;
        new_dropdown_ptr->label_text          = label_text;
        new_dropdown_ptr->n_selected_entry    = n_selected_entry;
        new_dropdown_ptr->owner_control       = owner_control;
        new_dropdown_ptr->pfn_fire_proc_ptr   = pfn_fire_proc_ptr;
        new_dropdown_ptr->text_renderer       = text_renderer;
        new_dropdown_ptr->ui_instance         = instance;
        new_dropdown_ptr->visible             = true;

        /* Cache GL func pointers */
        ral_backend_type backend_type = RAL_BACKEND_TYPE_UNKNOWN;

        ral_context_get_property(new_dropdown_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND_TYPE,
                                &backend_type);

        if (backend_type == RAL_BACKEND_TYPE_ES)
        {
            const ogl_context_es_entrypoints* entry_points_ptr = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_dropdown_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                    &entry_points_ptr);

            new_dropdown_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_dropdown_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_dropdown_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_dropdown_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_dropdown_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_dropdown_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }
        else
        {
            ASSERT_DEBUG_SYNC(backend_type == RAL_BACKEND_TYPE_GL,
                              "Unrecognized context type");

            const ogl_context_gl_entrypoints* entry_points_ptr = NULL;

            ogl_context_get_property(ral_context_get_gl_context(new_dropdown_ptr->context),
                                     OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                    &entry_points_ptr);

            new_dropdown_ptr->pGLBindBufferRange     = entry_points_ptr->pGLBindBufferRange;
            new_dropdown_ptr->pGLDisable             = entry_points_ptr->pGLDisable;
            new_dropdown_ptr->pGLDrawArrays          = entry_points_ptr->pGLDrawArrays;
            new_dropdown_ptr->pGLEnable              = entry_points_ptr->pGLEnable;
            new_dropdown_ptr->pGLUniformBlockBinding = entry_points_ptr->pGLUniformBlockBinding;
            new_dropdown_ptr->pGLUseProgram          = entry_points_ptr->pGLUseProgram;
        }

        /* Initialize entries */
        system_window window         = NULL;
        int           window_size[2] = {0};

        ral_context_get_property  (new_dropdown_ptr->context,
                                   RAL_CONTEXT_PROPERTY_WINDOW_SYSTEM,
                                  &window);
        system_window_get_property(window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        for (uint32_t n_entry = 0;
                      n_entry < n_strings + 1 /* currently selected entry */;
                    ++n_entry)
        {
            _ui_dropdown_entry* entry_ptr = NULL;

            entry_ptr = new (std::nothrow) _ui_dropdown_entry;
            ASSERT_DEBUG_SYNC(entry_ptr != NULL, "Out of memory");

            /* Fill the descriptor */
            entry_ptr->string_id = varia_text_renderer_add_string(text_renderer);

            if (n_entry != n_strings)
            {
                entry_ptr->name = strings[n_entry];

                if (user_args != NULL)
                {
                    entry_ptr->user_arg = user_args[n_entry];
                }
                else
                {
                    entry_ptr->user_arg = NULL;
                }
            }
            else
            {
                entry_ptr->name     = strings[new_dropdown_ptr->n_selected_entry];
                entry_ptr->user_arg = NULL;
            }

            system_resizable_vector_push(new_dropdown_ptr->entries,
                                         entry_ptr);
        } /* for (all text entries) */

        /* Create text string representations.
         *
         * NOTE: This call will also attempt to set scissor box for the text strings.
         *       Unfortunately, the required vector values have not been calculated yet,
         *       so we will need to redo this call after these become available.
         */
        _ui_dropdown_update_entry_strings(new_dropdown_ptr,
                                          false);

        /* Update sub-control sizes */
        _ui_dropdown_update_position(new_dropdown_ptr,
                                     x1y1);

        /* Retrieve the rendering program */
        new_dropdown_ptr->program           = ui_get_registered_program(instance,
                                                                        ui_dropdown_program_name);
        new_dropdown_ptr->program_bg        = ui_get_registered_program(instance,
                                                                        ui_dropdown_bg_program_name);
        new_dropdown_ptr->program_label_bg  = ui_get_registered_program(instance,
                                                                        ui_dropdown_label_bg_program_name);
        new_dropdown_ptr->program_separator = ui_get_registered_program(instance,
                                                                        ui_dropdown_separator_program_name);
        new_dropdown_ptr->program_slider    = ui_get_registered_program(instance,
                                                                        ui_dropdown_slider_program_name);

        if (new_dropdown_ptr->program           == NULL ||
            new_dropdown_ptr->program_bg        == NULL ||
            new_dropdown_ptr->program_label_bg  == NULL ||
            new_dropdown_ptr->program_separator == NULL ||
            new_dropdown_ptr->program_slider    == NULL)
        {
            _ui_dropdown_init_program(instance,
                                      new_dropdown_ptr);

            ASSERT_DEBUG_SYNC(new_dropdown_ptr->program != NULL,
                              "Could not initialize dropdown UI programs");
        } /* if (new_button->program == NULL) */

        /* Set up predefined values */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_dropdown_ptr->context),
                                                         _ui_dropdown_init_renderer_callback,
                                                         new_dropdown_ptr);
    } /* if (new_dropdown != NULL) */

    return (void*) new_dropdown_ptr;
}

/** Please see header for specification */
PUBLIC bool ui_dropdown_is_over(void*        internal_instance,
                                const float* xy)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;
    float         inversed_y   = 1.0f - xy[1];

    if (!dropdown_ptr->visible)
    {
        return false;
    }

    /* Store the hover UV location. Position it relative to the drop area, so that
     * we can pass the pair during the draw call.*/
    if (!dropdown_ptr->is_lbm_on)
    {
        dropdown_ptr->hover_ss[0] = xy[0];
        dropdown_ptr->hover_ss[1] = inversed_y;
    }

    if (xy[0]      >= dropdown_ptr->x1y1x2y2[0]        && xy[0]      <= dropdown_ptr->x1y1x2y2[2]        && /* for the bar, consider whole button width, */
        inversed_y >= dropdown_ptr->x1y1x2y2[1]        && inversed_y <= dropdown_ptr->button_x1y1x2y2[3] || /* but for the drop area, exclude the slider space */
        xy[0]      >= dropdown_ptr->x1y1x2y2[0]        && xy[0]      <= dropdown_ptr->button_x1y1x2y2[0] &&
        inversed_y >= dropdown_ptr->button_x1y1x2y2[3] && inversed_y <= dropdown_ptr->x1y1x2y2[3])
    {
        if (!dropdown_ptr->is_hovering)
        {
            dropdown_ptr->start_hovering_brightness = dropdown_ptr->current_gpu_brightness_level;
            dropdown_ptr->start_hovering_time       = system_time_now();
            dropdown_ptr->is_hovering               = true;
        } /* if (!button_ptr->is_hovering) */

        return true;
    }
    else
    {
        if (dropdown_ptr->is_hovering)
        {
            dropdown_ptr->is_hovering               = false;
            dropdown_ptr->start_hovering_brightness = dropdown_ptr->current_gpu_brightness_level;
            dropdown_ptr->start_hovering_time       = system_time_now();
        }

        /* Slider? */
        if (xy[0] >= dropdown_ptr->slider_x1x2[0] && xy[0] <= dropdown_ptr->slider_x1x2[1])
        {
            float slider_x1y1x2y2[4];

            _ui_dropdown_get_slider_x1y1x2y2(dropdown_ptr,
                                             slider_x1y1x2y2);

            if (inversed_y >= slider_x1y1x2y2[3] && inversed_y <= slider_x1y1x2y2[1])
            {
                return true;
            }
        }
        else
        /* Drop area? */
        if (xy[0]      >= dropdown_ptr->drop_x1y2x2y1[0] && xy[0]      <= dropdown_ptr->drop_x1y2x2y1[2] &&
            inversed_y >= dropdown_ptr->drop_x1y2x2y1[3] && inversed_y <= dropdown_ptr->drop_x1y2x2y1[1] &&
            dropdown_ptr->is_droparea_visible)
        {
            return true;
        }
    }

    return false;
}

/** Please see header for specification */
PUBLIC void ui_dropdown_on_lbm_down(void*        internal_instance,
                                    const float* xy)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;
    float         inversed_y = 1.0f - xy[1];

    if (!dropdown_ptr->visible)
    {
        return;
    }

    if (xy[0]      >= dropdown_ptr->button_x1y1x2y2[0] && xy[0]      <= dropdown_ptr->button_x1y1x2y2[2] &&
        inversed_y >= dropdown_ptr->button_x1y1x2y2[1] && inversed_y <= dropdown_ptr->button_x1y1x2y2[3] &&
        !dropdown_ptr->is_lbm_on)
    {
        dropdown_ptr->is_button_lbm               = true;
        dropdown_ptr->is_lbm_on                   = true;
        dropdown_ptr->force_gpu_brightness_update = true;
    }
    else
    if (xy[0] >= dropdown_ptr->slider_x1x2[0] && xy[0] <= dropdown_ptr->slider_x1x2[1])
    {
        float slider_x1y1x2y2[4];

        _ui_dropdown_get_slider_x1y1x2y2(dropdown_ptr,
                                         slider_x1y1x2y2);

        if (inversed_y >= slider_x1y1x2y2[3] && inversed_y <= slider_x1y1x2y2[1])
        {
            dropdown_ptr->is_lbm_on          = true;
            dropdown_ptr->is_slider_lbm      = true;
            dropdown_ptr->slider_delta_y     = 0.0f;
            dropdown_ptr->slider_lbm_start_y = xy[1];
        }
    }
    else
    if (xy[0]      >= dropdown_ptr->drop_x1y2x2y1[0] && xy[0]      <= dropdown_ptr->drop_x1y2x2y1[2] &&
        inversed_y >= dropdown_ptr->drop_x1y2x2y1[3] && inversed_y <= dropdown_ptr->drop_x1y2x2y1[1])
    {
        dropdown_ptr->is_droparea_lbm = true;
    }
}

/** Please see header for specification */
PUBLIC void ui_dropdown_on_lbm_up(void*        internal_instance,
                                  const float* xy)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;
    float         inversed_y = 1.0f - xy[1];

    if (!dropdown_ptr->visible)
    {
        return;
    }

    dropdown_ptr->is_lbm_on                   = false;
    dropdown_ptr->force_gpu_brightness_update = true;

    if (dropdown_ptr->is_slider_lbm)
    {
        dropdown_ptr->slider_delta_y_base += dropdown_ptr->slider_delta_y;
        dropdown_ptr->slider_delta_y       = 0.0f;

        _ui_dropdown_update_entry_positions (dropdown_ptr);
        _ui_dropdown_update_entry_visibility(dropdown_ptr);
    }
    else
    if (xy[0]      >= dropdown_ptr->button_x1y1x2y2[0] && xy[0]      <= dropdown_ptr->button_x1y1x2y2[2] &&
        inversed_y >= dropdown_ptr->button_x1y1x2y2[1] && inversed_y <= dropdown_ptr->button_x1y1x2y2[3] &&
        dropdown_ptr->is_button_lbm)
    {
        dropdown_ptr->is_droparea_visible = !dropdown_ptr->is_droparea_visible;

        /* Update visibility of the entries */
        _ui_dropdown_update_entry_visibility(dropdown_ptr);

        /* Call back any subscribers */
        ui_receive_control_callback(dropdown_ptr->ui_instance,
                                    (ui_control) dropdown_ptr,
                                    UI_DROPDOWN_CALLBACK_ID_DROPAREA_TOGGLE,
                                    dropdown_ptr);
    }
    else
    {
        /* If the click is within drop area, the user has requested to change the current selection */
        if (xy[0]      >= dropdown_ptr->drop_x1y2x2y1[0] && xy[0]      <= dropdown_ptr->drop_x1y2x2y1[2] &&
            inversed_y >= dropdown_ptr->drop_x1y2x2y1[3] && inversed_y <= dropdown_ptr->drop_x1y2x2y1[1] &&
            dropdown_ptr->is_droparea_lbm)
        {
            unsigned int n_entries        = 0;
            unsigned int n_selected_entry = 0;
            float        highlighted_v1v2[2];

            system_resizable_vector_get_property(dropdown_ptr->entries,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_entries);

            _ui_dropdown_get_highlighted_v1v2(dropdown_ptr,
                                              true,
                                              highlighted_v1v2);

            /* NOTE: This value starts from 1, as 0 corresponds to the header bar string */
            n_selected_entry = (unsigned int)((1.0f - highlighted_v1v2[0]) * MAX_N_ENTRIES_VISIBLE + 0.5f);

            if (n_selected_entry <= n_entries)
            {
                dropdown_ptr->n_selected_entry = (n_selected_entry - 1);

                /* Fire the associated callback, if assigned */
                _ui_dropdown_entry* entry_ptr = NULL;

                system_resizable_vector_get_element_at(dropdown_ptr->entries,
                                                       dropdown_ptr->n_selected_entry,
                                                      &entry_ptr);

                if (entry_ptr                       != NULL &&
                    dropdown_ptr->pfn_fire_proc_ptr != NULL)
                {
                    /* Spawn the callback descriptor. It will be released in the thread pool call-back
                     * handler after the event is taken care of by the dropdown fire event handler.
                     */
                    _ui_dropdown_fire_event* event_ptr = new (std::nothrow) _ui_dropdown_fire_event;

                    ASSERT_ALWAYS_SYNC(event_ptr != NULL, "Out of memory");
                    if (event_ptr != NULL)
                    {
                        event_ptr->event_user_arg     = entry_ptr->user_arg;
                        event_ptr->fire_proc_user_arg = dropdown_ptr->fire_proc_user_arg;
                        event_ptr->pfn_fire_proc_ptr  = dropdown_ptr->pfn_fire_proc_ptr;

                        system_thread_pool_task task = system_thread_pool_create_task_handler_only(THREAD_POOL_TASK_PRIORITY_NORMAL,
                                                                                                   _ui_dropdown_fire_callback,
                                                                                                   event_ptr);

                        system_thread_pool_submit_single_task(task);
                    }
                }

                /* Finally, update the bar string */
                _ui_dropdown_update_entry_strings(dropdown_ptr,
                                                  true); /* only update selected entry */
            }
        }
    }

    dropdown_ptr->is_button_lbm   = false;
    dropdown_ptr->is_droparea_lbm = false;
    dropdown_ptr->is_slider_lbm   = false;
}

/** Please see header for specification */
PUBLIC void ui_dropdown_on_mouse_move(void*        internal_instance,
                                      const float* xy)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;
    float         inversed_y   = 1.0f - xy[1];

    if (!dropdown_ptr->visible)
    {
        return;
    }

    if (dropdown_ptr->is_slider_lbm)
    {
        const float slider_height_ss = (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * (1.0f - dropdown_ptr->slider_height);

        dropdown_ptr->slider_delta_y = dropdown_ptr->slider_lbm_start_y - xy[1];

        if (dropdown_ptr->slider_delta_y > -dropdown_ptr->slider_delta_y_base)
        {
            dropdown_ptr->slider_delta_y = -dropdown_ptr->slider_delta_y_base;
        }
        else
        if (dropdown_ptr->slider_delta_y < -slider_height_ss - dropdown_ptr->slider_delta_y_base)
        {
            dropdown_ptr->slider_delta_y = -slider_height_ss - dropdown_ptr->slider_delta_y_base;
        }

        /* Update entry positions */
        _ui_dropdown_update_entry_positions(dropdown_ptr);
    } /* if (dropdown_ptr->is_slider_lbm) */
}

/** Please see header for specification */
PUBLIC void ui_dropdown_on_mouse_wheel(void* internal_instance,
                                       float wheel_delta)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) internal_instance;

    if (!dropdown_ptr->visible)
    {
        return;
    }

    if (!dropdown_ptr->is_lbm_on &&
        dropdown_ptr->is_droparea_visible)
    {
        /* Move the slider accordingly */
        dropdown_ptr->slider_delta_y_base += wheel_delta * 0.25f * dropdown_ptr->separator_delta_y;

        /* Clamp it */
        const float slider_height_ss = (dropdown_ptr->drop_x1y2x2y1[1] - dropdown_ptr->drop_x1y2x2y1[3]) * (1.0f - dropdown_ptr->slider_height);

        if (dropdown_ptr->slider_delta_y_base > 0.0f)
        {
            dropdown_ptr->slider_delta_y_base = 0.0f;
        }

        if (dropdown_ptr->slider_delta_y_base <= -slider_height_ss)
        {
            dropdown_ptr->slider_delta_y_base = -slider_height_ss;
        }

        _ui_dropdown_update_entry_positions(dropdown_ptr);
    } /* if (!dropdown_ptr->is_lbm_on) */
}

/* Please see header for spec */
PUBLIC void ui_dropdown_set_property(void*               dropdown,
                                     ui_control_property property,
                                     const void*         data)
{
    _ui_dropdown* dropdown_ptr = (_ui_dropdown*) dropdown;

    switch (property)
    {
        case UI_CONTROL_PROPERTY_DROPDOWN_VISIBLE:
        case UI_CONTROL_PROPERTY_GENERAL_VISIBLE:
        {
            dropdown_ptr->visible = *(bool*) data;

            _ui_dropdown_update_entry_visibility(dropdown_ptr);

            /* If there's anyone waiting on this event, let them know */
            ui_receive_control_callback(dropdown_ptr->ui_instance,
                                        (ui_control) dropdown_ptr,
                                        UI_DROPDOWN_CALLBACK_ID_VISIBILITY_TOGGLE,
                                        dropdown_ptr);

            break;
        }

        case UI_CONTROL_PROPERTY_GENERAL_X1Y1:
        case UI_CONTROL_PROPERTY_DROPDOWN_X1Y1:
        {
            _ui_dropdown_update_position(dropdown_ptr,
                                         (const float*) data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ui_control_property value");
        }
    } /* switch (property_value) */
}
