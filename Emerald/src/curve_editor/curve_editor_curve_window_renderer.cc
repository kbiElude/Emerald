/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_segment.h"
#include "curve_editor/curve_editor_types.h"
#include "curve_editor/curve_editor_curve_window.h"
#include "curve_editor/curve_editor_curve_window_renderer.h"
#include "curve_editor/curve_editor_program_curvebackground.h"
#include "curve_editor/curve_editor_program_lerp.h"
#include "curve_editor/curve_editor_program_quadselector.h"
#include "curve_editor/curve_editor_program_static.h"
#include "curve_editor/curve_editor_program_tcb.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pixel_format_descriptor.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_text.h"
#include "ogl/ogl_types.h"
#include "shaders/shaders_vertex_combinedmvp_simplified_twopoint.h"
#include "shaders/shaders_vertex_fullscreen.h"
#include "system/system_assertions.h"
#include "system/system_context_menu.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resources.h"
#include "system/system_variant.h"
#include "system/system_window.h"


/* Lerp from x e < A,B> to <C,D>
 *
 * A => C  // C = aA+b 
 * B => D  // D = aB+b 
 *
 *  b = C-aA
 *  
 *  D = aB+b = aB+C-aA = a(B-A) + C
 *  D - C = a(B-A)
 *  a     = (D - C) / (B - A)
 *
 *  y = ax+b = ax + C - aA = a(x-A) + C = (D-C)/(B-A)(x-A) + C
 *
 *  y = (D-C)/(B-A)(x-A) + C
 */ 
#define LERP_FROM_A_B_TO_C_D(x, a, b, c, d) \
    ((float(d) - float(c)) / (float(b) - float(a) ) * (float(x) - float(a)) + float(c))

#define CLICK_THRESHOLD            (2    )
#define COLOR_AXES                 {1.0f, 0.0f, 0.0f, 0.5f}
#define HEIGHT_SEPARATORS_PX       ( 32  )
#define LENGTH_SEPARATORS_PX       ( 16  )
#define MAX_SCREEN_HEIGHT_DEFAULT  ( 16.0f)
#define MAX_SCREEN_WIDTH_DEFAULT   ( 32.0f)
#define MIN_SCREEN_HEIGHT          ( 0.1f)
#define MIN_SCREEN_WIDTH           ( 0.1f)
#define NODE_SIZE_PX               ( 12 )
#define N_TEXT_STRINGS_ON_START    ( 16 )
#define SEGMENT_RESIZE_MARGIN      ( 8  )
#define START_X1                   (-1.0f)
#define START_X_WIDTH              ( 2.0f)
#define START_Y1                   (-2.0f)
#define START_Y_HEIGHT             ( 4.0f) 
#define TEXT_SEPARATOR_DISTANCE_PX ( 3   )
#define WIDTH_AXES                 ( 2.0f)
#define WIDTH_SEPARATORS           ( 1.0f)
#define WIDTH_SEPARATOR_PX         ( 64  )

/* Private definition */
typedef enum
{
    STATUS_NO_REUPLOAD_NEEDED,
    STATUS_PARTIAL_REUPLOAD_NEEDED,
    STATUS_FULL_REUPLOAD_NEEDED
} _data_status;

typedef struct
{
    uint32_t             partial_reupload_start_index;
    uint32_t             partial_reupload_end_index;
    _data_status         status;
    system_timeline_time tcb_ubo_modification_time;
    GLuint               tcb_ubo_id;

} _tcb_segment_rendering;

typedef struct
{

    ogl_context               context;
    curve_container           curve;
    system_hashed_ansi_string name;
    curve_editor_curve_window owner;
    ogl_rendering_handler     rendering_handler;
    HWND                      view_window_handle;
    system_window             window;

    GLuint         static_color_program_a_ub_offset;
    GLuint         static_color_program_b_ub_offset;
    GLuint         static_color_program_color_ub_offset;
    GLuint         static_color_program_mvp_ub_offset;
    ogl_program_ub static_color_program_ub_fs;
    GLuint         static_color_program_ub_fs_bo_id;
    GLuint         static_color_program_ub_fs_bo_size;
    GLuint         static_color_program_ub_fs_bo_start_offset;
    GLuint         static_color_program_ub_fs_bp;
    ogl_program_ub static_color_program_ub_vs;
    GLuint         static_color_program_ub_vs_bo_id;
    GLuint         static_color_program_ub_vs_bo_size;
    GLuint         static_color_program_ub_vs_bo_start_offset;
    GLuint         static_color_program_ub_vs_bp;

    float                     max_screen_height;
    float                     max_screen_width;
    int                       mouse_x; /* Used when drawing the surface */
    int                       mouse_y; /* Used when drawing the surface */
    float                     mouse_y_value;

    system_variant            float_variant;
    uint32_t                  n_text_strings;
    ogl_text                  text_renderer;
    system_variant            text_variant;

    float                     x1;       /* X at left view border */
    float                     x_width;  /* x1+x_width = X at right view border */
    float                     y1;       /* Y at bottom view border */
    float                     y_height; /* y1+y_height = Y at top view border */

    bool                  is_left_button_down;
    bool                  nodemove_mode_active;
    curve_segment_node_id nodemove_node_id;
    curve_segment_id      nodemove_segment_id;
    int                   quadselector_mouse_x;
    bool                  quadselector_mode_active;
    int                   quadselector_start_mouse_x;
    RECT                  quadselector_window_rect;
    LONG                  segmentmove_click_x;
    bool                  segmentmove_mode_active;
    curve_segment_id      segmentmove_segment_id;
    system_timeline_time  segmentmove_start_time;
    system_timeline_time  segmentmove_end_time;
    LONG                  segmentresize_click_x;
    system_timeline_time  segmentresize_end_time;
    bool                  segmentresize_left_border_resize; /* if false, resize from right border */
    bool                  segmentresize_mode_active;
    curve_segment_id      segmentresize_segment_id;
    system_timeline_time  segmentresize_start_time;

    curve_segment_id      hovered_curve_segment_id; /* only configured by on right click handler */
    curve_segment_node_id selected_node_id;
    curve_segment_id      selected_segment_id;

    system_hash64map      tcb_segments;

    LONG                  left_button_click_location[2];
    float                 left_button_click_x1y1    [2];

} _curve_editor_curve_window_renderer;

typedef struct
{
    ogl_program                 bg_program;
    ogl_shader                  bg_fragment_shader;
    shaders_vertex_fullscreen   bg_vertex_shader;

    ogl_shader                                     static_color_fragment_shader;
    ogl_program                                    static_color_program;
    shaders_vertex_combinedmvp_simplified_twopoint static_color_vertex_shader;

    curve_editor_program_curvebackground curvebackground_program;
    curve_editor_program_lerp            lerp_curve_program;
    curve_editor_program_quadselector    quadselector_program;
    curve_editor_program_static          static_curve_program;
    curve_editor_program_tcb             tcb_curve_program;

    unsigned long long          ref_counter;
} _curve_window_renderer_globals;

/* Private variables */
_curve_window_renderer_globals* _globals                = NULL;
bool                            _is_globals_initialized = false;

system_hashed_ansi_string _bg_fragment_shader_body = system_hashed_ansi_string_create("#version 430 core\n"
                                                                                      "\n"
                                                                                      "in  vec2 uv;\n"
                                                                                      "out vec4 color;\n"
                                                                                      "\n"
                                                                                      "void main()\n"
                                                                                      "{\n"
                                                                                      "   color = vec4(vec3(uv.y * 0.25), 1);\n"
                                                                                      "}\n"
                                                                                     );
system_hashed_ansi_string _static_color_fragment_shader_body = system_hashed_ansi_string_create("#version 430 core\n"
                                                                                                "\n"
                                                                                                "uniform dataFS\n"
                                                                                                "{\n"
                                                                                                "    vec4 in_color;\n"
                                                                                                "};\n"
                                                                                                "\n"
                                                                                                "out vec4 color;\n"
                                                                                                "\n"
                                                                                                "void main()\n"
                                                                                                "{\n"
                                                                                                "   color = in_color;\n"
                                                                                                "}\n"
                                                                                                );

/* Forward declarations */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve                (ogl_context,
                                                                            curve_editor_curve_window_renderer,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            int*);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_background     (ogl_context,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            float,
                                                                            float,
                                                                            const float*,
                                                                            const char*,
                                                                            int*);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_px        (ogl_context,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_segment   (ogl_context,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            curve_container,
                                                                            curve_segment_id,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            int*);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_ss        (ogl_context,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_node_ss        (_curve_editor_curve_window_renderer*,
                                                                            ogl_context,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            bool,
                                                                            curve_segment_id,
                                                                            curve_segment_node_id);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_px      (ogl_context,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            float,
                                                                            float,
                                                                            float);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_segment (ogl_context,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            curve_container,
                                                                            curve_segment_id,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            int*);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_ss      (ogl_context,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            float,
                                                                            float,
                                                                            float);
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_tcb_segment    (ogl_context,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            curve_container,
                                                                            curve_segment_id,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            float,
                                                                            int*);
PRIVATE void _curve_editor_curve_window_renderer_init_globals              (ogl_context,
                                                                            void* not_used);
PRIVATE void _curve_editor_curve_window_renderer_init_tcb_segment_rendering(ogl_context,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*,
                                                                            curve_container,
                                                                            curve_segment_id);
PRIVATE void _curve_editor_curve_window_renderer_rendering_callback_handler(ogl_context,
                                                                            uint32_t /* n_frame */,
                                                                            system_timeline_time /* frame time */,
                                                                            void*);
PRIVATE bool _curve_editor_curve_window_renderer_on_left_button_down       (system_window,
                                                                            LONG,
                                                                            LONG,
                                                                            system_window_vk_status,
                                                                            void*);
PRIVATE bool _curve_editor_curve_window_renderer_on_left_button_up         (system_window,
                                                                            LONG,
                                                                            LONG,
                                                                            system_window_vk_status,
                                                                            void*);
PRIVATE bool _curve_editor_curve_window_renderer_on_right_button_up        (system_window,
                                                                            LONG,
                                                                            LONG,
                                                                            system_window_vk_status,
                                                                            void*);
PRIVATE bool _curve_editor_curve_window_renderer_on_mouse_move             (system_window,
                                                                            LONG,
                                                                            LONG,
                                                                            system_window_vk_status,
                                                                            void*);
PRIVATE bool _curve_editor_curve_window_renderer_on_mouse_wheel            (system_window,
                                                                            unsigned short,
                                                                            unsigned short,
                                                                            short,
                                                                            system_window_vk_status,
                                                                            void*);
PRIVATE void _curve_editor_curve_window_renderer_update_tcb_ubo            (curve_segment,
                                                                            GLuint,
                                                                            bool,
                                                                            uint32_t,
                                                                            uint32_t /* n */,
                                                                            _curve_editor_curve_window_renderer*,
                                                                            const ogl_context_gl_entrypoints*);

/** TODO */
PRIVATE void _curve_editor_curve_window_remove_node_handler(void* arg)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) arg;

    /* Delete the node */
    curve_container_delete_node(renderer_ptr->curve,
                                renderer_ptr->selected_segment_id,
                                renderer_ptr->selected_node_id);

    /* Update node storage at next redraw */
    _tcb_segment_rendering* tcb_segment_rendering = NULL;

    if (system_hash64map_get(renderer_ptr->tcb_segments,
                             renderer_ptr->selected_segment_id,
                            &tcb_segment_rendering) )
    {
        tcb_segment_rendering->status = STATUS_FULL_REUPLOAD_NEEDED;
    }

    /* Issue a redraw */
    ogl_rendering_handler_play(renderer_ptr->rendering_handler, 0);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_remove_segment_handler_renderer_callback(ogl_context context,
                                                                                 void*       arg)
{
    const ogl_context_gl_entrypoints*    entry_points          = NULL;
    _curve_editor_curve_window_renderer* renderer_ptr          = (_curve_editor_curve_window_renderer*) arg;
    _tcb_segment_rendering*              tcb_segment_rendering = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* This might be a TCB segment. If so, handle the case */
    if (system_hash64map_get(renderer_ptr->tcb_segments,
                             renderer_ptr->hovered_curve_segment_id,
                            &tcb_segment_rendering) )
    {
        /* Release renderer-side object */
        entry_points->pGLDeleteBuffers(1,
                                      &tcb_segment_rendering->tcb_ubo_id);

        ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                          "Could not release TCB UBO");

        /* Free the instance */
        delete tcb_segment_rendering;

        /* Remove it from the map */
        system_hash64map_remove(renderer_ptr->tcb_segments,
                                renderer_ptr->selected_segment_id);
    }
}

/** TODO */
PRIVATE void _curve_editor_curve_window_remove_segment_handler(void* arg)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) arg;

    /* Delete the node */
    curve_container_delete_segment(renderer_ptr->curve,
                                   renderer_ptr->hovered_curve_segment_id);

    /* Renderer-side segment descriptor contains a buffer object we need to release. Request a renderer thread call-back
     * to handle the cleanup.
     */
    ogl_rendering_handler_request_callback_from_context_thread(renderer_ptr->rendering_handler,
                                                               _curve_editor_curve_window_remove_segment_handler_renderer_callback,
                                                               renderer_ptr);

    /* Issue a redraw */
    ogl_rendering_handler_play(renderer_ptr->rendering_handler, 0);
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_deinit(_curve_editor_curve_window_renderer* descriptor)
{
    bool result = false;

    if (descriptor->text_renderer != NULL)
    {
        ogl_text_release(descriptor->text_renderer);
    }

    if (descriptor->window != NULL)
    {
        system_window_close(descriptor->window);

        descriptor->window = NULL;
    }

    if (descriptor->rendering_handler != NULL)
    {
        /* Already released */
        descriptor->rendering_handler = NULL;
    }

    if (descriptor->float_variant != NULL)
    {
        system_variant_release(descriptor->float_variant);

        descriptor->float_variant = NULL;
    }
    if (descriptor->text_variant != NULL)
    {
        system_variant_release(descriptor->text_variant);

        descriptor->text_variant = NULL;
    }

    if (descriptor->tcb_segments != NULL)
    {
        system_hash64map_release(descriptor->tcb_segments);

        descriptor->tcb_segments = NULL;
    }

    /* Hide the window we use for retrieving dimensions */
    ::ShowWindow(descriptor->view_window_handle, SW_SHOW);

    result = true;
    return result;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_deinit_globals()
{
    if (::InterlockedDecrement(&_globals->ref_counter) == 0)
    {
        if (_globals->bg_program != NULL)
        {
            ogl_program_release(_globals->bg_program);
        }

        if (_globals->bg_fragment_shader != NULL)
        {
            ogl_shader_release(_globals->bg_fragment_shader);
        }

        if (_globals->bg_vertex_shader != NULL)
        {
            shaders_vertex_fullscreen_release(_globals->bg_vertex_shader);
        }

        if (_globals->static_color_program != NULL)
        {
            ogl_program_release(_globals->static_color_program);
        }

        if (_globals->static_color_fragment_shader != NULL)
        {
            ogl_shader_release(_globals->static_color_fragment_shader);
        }

        if (_globals->static_color_vertex_shader != NULL)
        {
            shaders_vertex_combinedmvp_simplified_twopoint_release(_globals->static_color_vertex_shader);
        }

        curve_editor_program_curvebackground_release(_globals->curvebackground_program);
        curve_editor_program_quadselector_release   (_globals->quadselector_program);
        curve_editor_program_static_release         (_globals->static_curve_program);
        curve_editor_program_lerp_release           (_globals->lerp_curve_program);
        curve_editor_program_tcb_release            (_globals->tcb_curve_program);

        delete _globals;
        _globals = NULL;
    }
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve(ogl_context                        context,
                                                            curve_editor_curve_window_renderer descriptor,
                                                            const ogl_context_gl_entrypoints*  entry_points,
                                                            int*                               n_strings_used_ptr)
{
    /* The approach implemented below is rather simple but there was no reason to put too much effort in optimising the implementation.
     * Redraws happen only when needed, curve won't update without explicit user interaction, and we can afford being a little bit
     * sluggish when it comes to updating the output.
     */
    _curve_editor_curve_window_renderer* descriptor_ptr = (_curve_editor_curve_window_renderer*) descriptor;
    float                                current_x      = descriptor_ptr->x1;
    float                                max_x          = descriptor_ptr->x1 + descriptor_ptr->x_width;
    system_timeline_time                 max_time       = system_time_get_timeline_time_for_msec(uint32_t(max_x * 1000.0f) );

    system_window_handle window_handle;
    RECT                 window_rect;

    system_window_get_property(descriptor_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    float mouse_x = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_x,
                                         0,
                                         (window_rect.right  - window_rect.left),
                                         descriptor_ptr->x1,
                                         (descriptor_ptr->x1 + descriptor_ptr->x_width) );
    float mouse_y = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_y,
                                         (window_rect.bottom - window_rect.top),
                                         0,
                                         descriptor_ptr->y1,
                                         (descriptor_ptr->y1 + descriptor_ptr->y_height));

    /* Don't draw curve for x < 0 */
    if (current_x < 0)
    {
        current_x = 0;
    }

    if (!descriptor_ptr->nodemove_mode_active)
    {
        descriptor_ptr->nodemove_node_id    = -1;
        descriptor_ptr->nodemove_segment_id = 0;
    }

    if (max_x >= 0)
    {
        float            next_x;
        curve_segment_id prev_segment_id = -1;

        while (current_x < max_x)
        {
            system_timeline_time x_time     = system_time_get_timeline_time_for_msec(uint32_t(current_x * 1000.0f) );
            curve_segment_id     segment_id = -1;

            /* Retrieve distance to the segment following the one already drawn */
            if (!curve_container_get_segment_id_relative_to_time(true,
                                                                 false,
                                                                 descriptor_ptr->curve,
                                                                 x_time,
                                                                 prev_segment_id,
                                                                &segment_id) )
            {
                /* No segments left! */
                next_x = max_x;
            }

            /* If the distance is larger than 0, we need to draw a static curve preview for the default value */
            system_timeline_time next_segment_start_time;
            uint32_t             next_segment_start_msec = 0;
            system_timeline_time next_segment_end_time;
            uint32_t             next_segment_end_msec = 0;
            curve_segment_type   next_segment_type;

            bool has_retrieved_segment_data = true;

            has_retrieved_segment_data &= curve_container_get_segment_property(descriptor_ptr->curve,
                                                                               segment_id,
                                                                               CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                                              &next_segment_start_time);
            has_retrieved_segment_data &= curve_container_get_segment_property(descriptor_ptr->curve,
                                                                               segment_id,
                                                                               CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                                              &next_segment_end_time);
            has_retrieved_segment_data &= curve_container_get_segment_property(descriptor_ptr->curve,
                                                                               segment_id,
                                                                               CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE,
                                                                              &next_segment_type);

            if (has_retrieved_segment_data)
            {
                if (x_time < next_segment_start_time)
                {
                    /* We need to get the static value preview drawn! */
                    float curve_value          = 0;
                    float next_segment_start_x = 0;

                    system_time_get_msec_for_timeline_time(next_segment_start_time,
                                                          &next_segment_start_msec);

                    next_segment_start_x = float(next_segment_start_msec) / 1000.0f;
                    curve_value          = curve_container_get_default_value(descriptor_ptr->curve,
                                                                             true,
                                                                             descriptor_ptr->float_variant);

                    system_variant_get_float(descriptor_ptr->float_variant,
                                            &curve_value);

                    _curve_editor_curve_window_renderer_draw_curve_static_px(context,
                                                                             entry_points,
                                                                             descriptor_ptr,
                                                                             curve_value,
                                                                             current_x,
                                                                             next_segment_start_x);

                    /* Before we start drawing the segment.. */
                    x_time    = next_segment_start_time;
                    current_x = next_segment_start_x;
                }

                if (x_time <= max_time)
                {
                    /* Draw the segment. */
                    system_time_get_msec_for_timeline_time(next_segment_end_time,
                                                          &next_segment_end_msec);

                    /* Retrieve x for start & end times of the segment */
                    uint32_t segment_start_msec;
                    float    segment_start_x;
                    uint32_t segment_end_msec;
                    float    segment_end_x;

                    system_time_get_msec_for_timeline_time(next_segment_start_time,
                                                          &segment_start_msec);
                    system_time_get_msec_for_timeline_time(next_segment_end_time,
                                                          &segment_end_msec);

                    segment_start_x = float(segment_start_msec) / 1000.0f;
                    segment_end_x   = float(segment_end_msec)   / 1000.0f;

                    float segment_start_x_px     = LERP_FROM_A_B_TO_C_D(segment_start_x,
                                                                        descriptor_ptr->x1,
                                                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                                                       -1,
                                                                        1);
                    float segment_end_x_px       = LERP_FROM_A_B_TO_C_D(segment_end_x,
                                                                        descriptor_ptr->x1,
                                                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                                                       -1,
                                                                        1);
                    float x_time_px              = LERP_FROM_A_B_TO_C_D(mouse_x,
                                                                        descriptor_ptr->x1,
                                                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                                                       -1,
                                                                        1);
                    float y_time_px              = LERP_FROM_A_B_TO_C_D(mouse_y,
                                                                        descriptor_ptr->y1,
                                                                        (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                                                       -1,
                                                                        1);

                    switch (next_segment_type)
                    {
                        case CURVE_SEGMENT_STATIC:
                        {
                            _curve_editor_curve_window_renderer_draw_curve_static_segment(context,
                                                                                          descriptor_ptr,
                                                                                          entry_points,
                                                                                          descriptor_ptr->curve,
                                                                                          segment_id,
                                                                                          segment_start_x_px,
                                                                                          segment_end_x_px,
                                                                                          x_time_px,
                                                                                          y_time_px,
                                                                                          n_strings_used_ptr);

                            break;
                        }

                        case CURVE_SEGMENT_LERP:
                        {
                            _curve_editor_curve_window_renderer_draw_curve_lerp_segment(context,
                                                                                        descriptor_ptr,
                                                                                        entry_points,
                                                                                        descriptor_ptr->curve,
                                                                                        segment_id,
                                                                                        segment_start_x_px,
                                                                                        segment_end_x_px,
                                                                                        x_time_px,
                                                                                        y_time_px,
                                                                                        n_strings_used_ptr);

                            break;
                        }

                        case CURVE_SEGMENT_TCB:
                        {
                            _curve_editor_curve_window_renderer_draw_curve_tcb_segment(context,
                                                                                       descriptor_ptr,
                                                                                       entry_points,
                                                                                       descriptor_ptr->curve,
                                                                                       segment_id,
                                                                                       segment_start_x_px,
                                                                                       segment_end_x_px,
                                                                                       x_time_px,
                                                                                       y_time_px,
                                                                                       n_strings_used_ptr);

                            break;
                        }

                        default:
                        {
                            LOG_ERROR("Unrecognized curve segment type!");
                        }
                    }

                    /* Update current X */
                    current_x = float(next_segment_end_msec) / 1000.0f;
                }
            }
            else
            {
                /* No more segments. Draw static value preview till the end of available space. */
                float  curve_value  = 0;

                curve_value = curve_container_get_default_value(descriptor_ptr->curve,
                                                                true,
                                                                descriptor_ptr->float_variant);
                system_variant_get_float                       (descriptor_ptr->float_variant,
                                                               &curve_value);

                _curve_editor_curve_window_renderer_draw_curve_static_px(context,
                                                                         entry_points,
                                                                         descriptor_ptr,
                                                                         curve_value,
                                                                         current_x,
                                                                         max_x);

                /* Update current X so that we break out of the loop */
                current_x = max_x;
            }

            /* .. */
            if (segment_id != -1)
            {
                prev_segment_id = segment_id;
            }
        }
    }
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_background(ogl_context                          context,
                                                                       _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                       const ogl_context_gl_entrypoints*    entry_points,
                                                                       float                                start_ss,
                                                                       float                                end_ss,
                                                                       const float*                         color,
                                                                       const char*                          segment_type_name,
                                                                       int*                                 n_text_strings_used_ptr)
{
    static const float white_color[4]  = {1, 1, 1, 1};
    float              positions  [20] = {start_ss, -1, 0, 1,  end_ss, -1, 0, 1,  end_ss, 1, 0, 1,  start_ss, 1, 0, 1,  start_ss, -1, 0, 1};

    entry_points->pGLLineWidth(2);
    entry_points->pGLBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    entry_points->pGLEnable(GL_BLEND);
    {
        curve_editor_program_curvebackground_set_property(_globals->curvebackground_program,
                                                          CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_COLORS_DATA,
                                                          color);
        curve_editor_program_curvebackground_set_property(_globals->curvebackground_program,
                                                          CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_POSITIONS_DATA,
                                                          positions);
        curve_editor_program_curvebackground_use         (context,
                                                          _globals->curvebackground_program);

        entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0,
                                    4);

        /* Border */
        positions[0 ] = start_ss; positions[1 ] = -1;
        positions[4 ] = end_ss;   positions[5 ] = -1;
        positions[8 ] = end_ss;   positions[9 ] =  1;
        positions[12] = start_ss; positions[13] =  1;

        curve_editor_program_curvebackground_set_property(_globals->curvebackground_program,
                                                          CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_COLORS_DATA,
                                                          white_color);
        curve_editor_program_curvebackground_set_property(_globals->curvebackground_program,
                                                          CURVE_EDITOR_PROGRAM_CURVEBACKGROUND_PROPERTY_POSITIONS_DATA,
                                                          positions);
        curve_editor_program_curvebackground_use         (context,
                                                          _globals->curvebackground_program);

        entry_points->pGLDrawArrays(GL_LINE_STRIP,
                                    0,
                                    5);
    }
    entry_points->pGLDisable(GL_BLEND);
    entry_points->pGLLineWidth(1);

    /* Draw the segment type info */
    int                  segment_start_x_ss;
    int                  text_position[2];
    system_window_handle window_handle;
    RECT                 window_rect;

    system_window_get_property(descriptor_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    segment_start_x_ss = (int) LERP_FROM_A_B_TO_C_D(start_ss, -1, 1, 0, window_rect.right - window_rect.left);
    text_position[0]   = segment_start_x_ss;
    text_position[1]   = 0;

    if (*n_text_strings_used_ptr < descriptor_ptr->n_text_strings)
    {
        ogl_text_set                     (descriptor_ptr->text_renderer,
                                          *n_text_strings_used_ptr,
                                           segment_type_name);
        ogl_text_set_text_string_property(descriptor_ptr->text_renderer,
                                         *n_text_strings_used_ptr,
                                          OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                          text_position);
    }

    (*n_text_strings_used_ptr)++;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_px(ogl_context                          context,
                                                                    const ogl_context_gl_entrypoints*    entry_points,
                                                                    _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                    float                                start_curve_value,
                                                                    float                                end_curve_value,
                                                                    float                                start_x,
                                                                    float                                end_x)
{
    /* Draw static curve preview */
    float start_pos[4] = {0, 0, 0, 1};
    float end_pos  [4] = {1, 0, 0, 1};

    start_pos[0] = LERP_FROM_A_B_TO_C_D(start_x,
                                        descriptor_ptr->x1,
                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                       -1,
                                        1);
    start_pos[1] = LERP_FROM_A_B_TO_C_D(start_curve_value,
                                        descriptor_ptr->y1,
                                        (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                       -1,
                                        1);
    end_pos  [0] = LERP_FROM_A_B_TO_C_D(end_x,
                                        descriptor_ptr->x1,
                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                       -1,
                                        1);
    end_pos  [1] = LERP_FROM_A_B_TO_C_D(end_curve_value,
                                        descriptor_ptr->y1,
                                        (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                       -1,
                                        1);

    curve_editor_program_lerp_set_property(_globals->lerp_curve_program,
                                           CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS1,
                                           start_pos);
    curve_editor_program_lerp_set_property(_globals->lerp_curve_program,
                                           CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS2,
                                           end_pos);
    curve_editor_program_lerp_use         (context,
                                           _globals->lerp_curve_program);

    entry_points->pGLDrawArrays       (GL_LINES,
                                       0,
                                       2);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_ss(ogl_context                          context,
                                                                    const ogl_context_gl_entrypoints*    entry_points,
                                                                    _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                    float                                start_curve_value,
                                                                    float                                end_curve_value,
                                                                    float                                start_x,
                                                                    float                                end_x)
{
    /* Draw static curve preview */
    float  start_pos[4] = {0, 0, 0, 1};
    float  end_pos  [4] = {1, 0, 0, 1};

    start_pos[0] = start_x;
    start_pos[1] = start_curve_value;
    end_pos  [0] = end_x;
    end_pos  [1] = end_curve_value;

    curve_editor_program_lerp_set_property(_globals->lerp_curve_program,
                                           CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS1,
                                           start_pos);
    curve_editor_program_lerp_set_property(_globals->lerp_curve_program,
                                           CURVE_EDITOR_PROGRAM_LERP_PROPERTY_POS2,
                                           end_pos);
    curve_editor_program_lerp_use         (context,
                                           _globals->lerp_curve_program);

    entry_points->pGLDrawArrays(GL_LINES,
                                0,
                                2);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_lerp_segment(ogl_context                          context,
                                                                         _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                         const ogl_context_gl_entrypoints*    entry_points,
                                                                         curve_container                      curve,
                                                                         curve_segment_id                     segment_id,
                                                                         float                                segment_start_ss,
                                                                         float                                segment_end_ss,
                                                                         float                                cursor_x_ss,
                                                                         float                                cursor_y_ss,
                                                                         int*                                 n_text_strings_used_ptr)
{
    float bg_color [4] = {0.5, 0.25, 0.75, 0.25};

    /* Draw the curve */
    system_timeline_time end_node_time;
    float                end_node_value;
    float                end_node_value_ss;
    system_timeline_time start_node_time;
    float                start_node_value;
    float                start_node_value_ss;

    curve_container_get_general_node_data(descriptor_ptr->curve,
                                          segment_id,
                                          0,
                                         &start_node_time,
                                          descriptor_ptr->float_variant);
    system_variant_get_float             (descriptor_ptr->float_variant,
                                         &start_node_value);
    curve_container_get_general_node_data(descriptor_ptr->curve,
                                          segment_id,
                                          1,
                                         &end_node_time,
                                          descriptor_ptr->float_variant);
    system_variant_get_float             (descriptor_ptr->float_variant,
                                         &end_node_value);

    start_node_value_ss = LERP_FROM_A_B_TO_C_D(start_node_value,
                                               descriptor_ptr->y1,
                                               (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                              -1,
                                               1);
    end_node_value_ss   = LERP_FROM_A_B_TO_C_D(end_node_value,
                                               descriptor_ptr->y1,
                                               (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                              -1,
                                               1);

    _curve_editor_curve_window_renderer_draw_curve_lerp_ss(context,
                                                           entry_points,
                                                           descriptor_ptr,
                                                           start_node_value_ss,
                                                           end_node_value_ss,
                                                           segment_start_ss,
                                                           segment_end_ss);

    /* Draw the background */
    if (cursor_x_ss >= segment_start_ss &&
        cursor_x_ss <= segment_end_ss)
    {
        /* Highlight */
        bg_color[3] = 0.45f;
    }

    if (descriptor_ptr->segmentmove_mode_active   && descriptor_ptr->segmentmove_segment_id   == segment_id ||
        descriptor_ptr->segmentresize_mode_active && descriptor_ptr->segmentresize_segment_id == segment_id)
    {
        bg_color[3] = 0.45f * 1.25f;
    }

    _curve_editor_curve_window_renderer_draw_curve_background(context,
                                                              descriptor_ptr,
                                                              entry_points,
                                                              segment_start_ss,
                                                              segment_end_ss,
                                                              bg_color,
                                                              "Linear",
                                                              n_text_strings_used_ptr);

    /* Draw both nodes */
    HWND window_handle;
    RECT window_rect;

    system_window_get_property(descriptor_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    float node_width_half_ss  = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX, 0, (window_rect.right  - window_rect.left), 0, 2) / 2.0f;
    float node_height_half_ss = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX, 0, (window_rect.bottom - window_rect.top),  0, 2) / 2.0f;
    float start_node_x_start  = segment_start_ss    - node_width_half_ss;
    float start_node_x_end    = segment_start_ss    + node_width_half_ss;
    float start_node_y_start  = start_node_value_ss - node_height_half_ss;
    float start_node_y_end    = start_node_value_ss + node_height_half_ss;
    float end_node_x_start    = segment_end_ss      - node_width_half_ss;
    float end_node_x_end      = segment_end_ss      + node_width_half_ss;
    float end_node_y_start    = end_node_value_ss   - node_height_half_ss;
    float end_node_y_end      = end_node_value_ss   + node_height_half_ss;
    bool  is_start_node_lit   = (cursor_x_ss >= start_node_x_start && cursor_x_ss <= start_node_x_end && cursor_y_ss >= start_node_y_start && cursor_y_ss <= start_node_y_end);
    bool  is_end_node_lit     = (cursor_x_ss >= end_node_x_start   && cursor_x_ss <= end_node_x_end   && cursor_y_ss >= end_node_y_start   && cursor_y_ss <= end_node_y_end);

    if (is_start_node_lit)
    {
        descriptor_ptr->nodemove_node_id    = 0;
        descriptor_ptr->nodemove_segment_id = segment_id;
    }
    else
    if (is_end_node_lit)
    {
        descriptor_ptr->nodemove_node_id    = 1;
        descriptor_ptr->nodemove_segment_id = segment_id;
    }

    /* This could be done so much better.. */
    _curve_editor_curve_window_renderer_draw_curve_node_ss(descriptor_ptr,
                                                           context,
                                                           entry_points,
                                                           start_node_x_start,
                                                           start_node_x_end,
                                                           start_node_y_start,
                                                           start_node_y_end,
                                                           is_start_node_lit,
                                                           segment_id,
                                                           0);

    _curve_editor_curve_window_renderer_draw_curve_node_ss(descriptor_ptr,
                                                           context,
                                                           entry_points,
                                                           end_node_x_start,
                                                           end_node_x_end,
                                                           end_node_y_start,
                                                           end_node_y_end,
                                                           is_end_node_lit,
                                                           segment_id,
                                                           1);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_node_ss(_curve_editor_curve_window_renderer* descriptor_ptr,
                                                                    ogl_context                          context,
                                                                    const ogl_context_gl_entrypoints*    entry_points,
                                                                    float                                node_x_start,
                                                                    float                                node_x_end,
                                                                    float                                node_y_start,
                                                                    float                                node_y_end,
                                                                    bool                                 is_node_lit,
                                                                    curve_segment_id                     segment_id,
                                                                    curve_segment_node_id                node_id)
{
    float positions[16] = {node_x_start, node_y_start, 0, 1,
                           node_x_end,   node_y_start, 0, 1,
                           node_x_end,   node_y_end,   0, 1,
                           node_x_start, node_y_end,   0, 1};

    float alpha = 0.25f;

    if (is_node_lit)
    {
        alpha = 0.5f;
    }

    if (descriptor_ptr->selected_node_id == node_id && descriptor_ptr->selected_segment_id == segment_id)
    {
        alpha *= 2.5f;
    }

    entry_points->pGLBlendFunc(GL_SRC_ALPHA,
                               GL_ONE_MINUS_SRC_ALPHA);
    entry_points->pGLEnable   (GL_BLEND);

    curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                   CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_ALPHA_DATA,
                                                  &alpha);
    curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                   CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_POSITIONS_DATA,
                                                   positions);
    curve_editor_program_quadselector_use         (context,
                                                   _globals->quadselector_program);

    entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                0,
                                4);
    entry_points->pGLDisable   (GL_BLEND);

}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_px(ogl_context                          context,
                                                                      const ogl_context_gl_entrypoints*    entry_points,
                                                                      _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                      float                                curve_value,
                                                                      float                                start_x,
                                                                      float                                end_x)
{
    /* Draw static curve preview */
    float  start_pos[4] = {0, 0, 0, 1};
    float  end_pos  [4] = {1, 0, 0, 1};

    start_pos[0] = LERP_FROM_A_B_TO_C_D(start_x,
                                        descriptor_ptr->x1,
                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                       -1,
                                        1);
    start_pos[1] = LERP_FROM_A_B_TO_C_D(curve_value,
                                        descriptor_ptr->y1,
                                        (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                       -1,
                                        1);
    end_pos  [0] = LERP_FROM_A_B_TO_C_D(end_x,
                                        descriptor_ptr->x1,
                                        (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                       -1,
                                        1);
    end_pos  [1] = start_pos[1];

    curve_editor_program_static_set_property(_globals->static_curve_program,
                                             CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS1_DATA,
                                             start_pos);
    curve_editor_program_static_set_property(_globals->static_curve_program,
                                             CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS2_DATA,
                                             end_pos);
    curve_editor_program_static_use         (context,
                                             _globals->static_curve_program);

    entry_points->pGLDrawArrays(GL_LINES,
                                0,
                                2);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_ss(ogl_context                          context,
                                                                      const ogl_context_gl_entrypoints*    entry_points,
                                                                      _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                      float                                curve_value,
                                                                      float                                start_x,
                                                                      float                                end_x)
{
    /* Draw static curve preview */
    float  start_pos[4] = {0, 0, 0, 1};
    float  end_pos  [4] = {1, 0, 0, 1};

    start_pos[0] = start_x;
    start_pos[1] = curve_value;
    end_pos  [0] = end_x;
    end_pos  [1] = start_pos[1];

    curve_editor_program_static_set_property(_globals->static_curve_program,
                                             CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS1_DATA,
                                             start_pos);
    curve_editor_program_static_set_property(_globals->static_curve_program,
                                             CURVE_EDITOR_PROGRAM_STATIC_PROPERTY_POS2_DATA,
                                             end_pos);
    curve_editor_program_static_use         (context,
                                             _globals->static_curve_program);

    entry_points->pGLDrawArrays(GL_LINES,
                                0,
                                2);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_static_segment(ogl_context                          context,
                                                                           _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                           const ogl_context_gl_entrypoints*    entry_points,
                                                                           curve_container                      curve,
                                                                           curve_segment_id                     segment_id,
                                                                           float                                segment_start_ss,
                                                                           float                                segment_end_ss,
                                                                           float                                cursor_x_ss,
                                                                           float                                cursor_y_ss,
                                                                           int*                                 n_text_strings_used_ptr)
{
    float bg_color [4] = {0.25f, 0.75f, 0.5f, 0.25f};

    /* Draw the curve */
    system_timeline_time node_time;
    float                node_value;
    float                node_value_ss;

    curve_container_get_general_node_data(descriptor_ptr->curve,
                                          segment_id,
                                          0,
                                         &node_time,
                                         descriptor_ptr->float_variant);
    system_variant_get_float             (descriptor_ptr->float_variant,
                                         &node_value);

    node_value_ss = LERP_FROM_A_B_TO_C_D(node_value,
                                         descriptor_ptr->y1,
                                         (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                        -1,
                                         1);

    _curve_editor_curve_window_renderer_draw_curve_static_ss(context,
                                                             entry_points,
                                                             descriptor_ptr,
                                                             node_value_ss,
                                                             segment_start_ss,
                                                             segment_end_ss);

    /* Draw the background */
    if (cursor_x_ss >= segment_start_ss &&
        cursor_x_ss <= segment_end_ss)
    {
        /* Highlight */
        bg_color[3] = 0.45f;
    }

    if (descriptor_ptr->segmentmove_mode_active   && descriptor_ptr->segmentmove_segment_id   == segment_id ||
        descriptor_ptr->segmentresize_mode_active && descriptor_ptr->segmentresize_segment_id == segment_id)
    {
        bg_color[3] = 0.45f * 1.25f;
    }

    _curve_editor_curve_window_renderer_draw_curve_background(context,
                                                              descriptor_ptr,
                                                              entry_points,
                                                              segment_start_ss,
                                                              segment_end_ss,
                                                              bg_color,
                                                              "Static",
                                                              n_text_strings_used_ptr);

    /* Draw the node - always in the center of the segment */
    HWND window_handle;
    RECT window_rect;

    system_window_get_property(descriptor_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    float node_width_half_ss  = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX,
                                                     0,
                                                     (window_rect.right  - window_rect.left),
                                                     0,
                                                     2) / 2.0f;
    float node_height_half_ss = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX,
                                                     0,
                                                     (window_rect.bottom - window_rect.top),
                                                     0,
                                                     2) / 2.0f;
    float node_x_start        = segment_start_ss + (segment_end_ss - segment_start_ss) / 2.0f - node_width_half_ss;
    float node_x_end          = segment_start_ss + (segment_end_ss - segment_start_ss) / 2.0f + node_width_half_ss;
    float node_y_start        = node_value_ss - node_height_half_ss;
    float node_y_end          = node_value_ss + node_height_half_ss;
    bool  is_node_lit         = (cursor_x_ss >= node_x_start && cursor_x_ss <= node_x_end && cursor_y_ss >= node_y_start && cursor_y_ss <= node_y_end);

    if (is_node_lit)
    {
        descriptor_ptr->nodemove_node_id    = 0;
        descriptor_ptr->nodemove_segment_id = segment_id;
    }

    _curve_editor_curve_window_renderer_draw_curve_node_ss(descriptor_ptr,
                                                           context,
                                                           entry_points,
                                                           node_x_start,
                                                           node_x_end,
                                                           node_y_start,
                                                           node_y_end,
                                                           is_node_lit,
                                                           segment_id,
                                                           0);
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_draw_curve_tcb_segment(ogl_context                          context,
                                                                        _curve_editor_curve_window_renderer* descriptor_ptr,
                                                                        const ogl_context_gl_entrypoints*    entry_points,
                                                                        curve_container                      curve,
                                                                        curve_segment_id                     segment_id,
                                                                        float                                segment_start_ss,
                                                                        float                                segment_end_ss,
                                                                        float                                cursor_x_ss,
                                                                        float                                cursor_y_ss,
                                                                        int*                                 n_strings_used_ptr)
{
    float bg_color [4] = {0.75, 0.25, 0.5, 0.25};

    if (cursor_x_ss >= segment_start_ss && cursor_x_ss <= segment_end_ss)
    {
        /* Highlight */
        bg_color[3] = 0.45f;
    }

    _curve_editor_curve_window_renderer_draw_curve_background(context,
                                                              descriptor_ptr,
                                                              entry_points,
                                                              segment_start_ss,
                                                              segment_end_ss,
                                                              bg_color,
                                                              "TCB",
                                                              n_strings_used_ptr);

    /* Retrieve window properties */
    HWND window_handle;
    RECT window_rect;

    system_window_get_property(descriptor_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    /* Before we move to drawing curve preview, make sure necessary data is already stored in VRAM */
    if (!system_hash64map_contains(descriptor_ptr->tcb_segments,
                                   segment_id) )
    {
        _curve_editor_curve_window_renderer_init_tcb_segment_rendering(context,
                                                                       descriptor_ptr,
                                                                       entry_points,
                                                                       curve,
                                                                       segment_id);
    }

    /* Draw curve preview */
    float                   delta_time      = (descriptor_ptr->x_width) / (window_rect.right - window_rect.left) * 0.5f;
    float                   delta_x         = 1.0f                      / (window_rect.right - window_rect.left);
    float                   val_range[2]    = {descriptor_ptr->y1, descriptor_ptr->y1 + descriptor_ptr->y_height};
    curve_segment           segment         = curve_container_get_segment(descriptor_ptr->curve,
                                                                          segment_id);
    uint32_t                n_nodes         = 0;
    _tcb_segment_rendering* tcb_segment_ptr = NULL;

    curve_segment_get_amount_of_nodes(segment,
                                     &n_nodes);
    system_hash64map_get             (descriptor_ptr->tcb_segments,
                                      segment_id,
                                     &tcb_segment_ptr);

    curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                          CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_TIME_DATA,
                                         &delta_time);
    curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                          CURVE_EDITOR_PROGRAM_TCB_PROPERTY_DELTA_X_DATA,
                                         &delta_x);
    curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                          CURVE_EDITOR_PROGRAM_TCB_PROPERTY_VAL_RANGE_DATA,
                                          val_range);
    curve_editor_program_tcb_use         (context,
                                          _globals->tcb_curve_program);

    entry_points->pGLBindBufferBase   (GL_UNIFORM_BUFFER,
                                       0,
                                       tcb_segment_ptr->tcb_ubo_id);

    /* If node data might have changed since last UBO reupload, update the status */
    if (tcb_segment_ptr->tcb_ubo_modification_time < curve_segment_get_modification_time(segment) )
    {
        system_timeline_time time_now = system_time_now();

        tcb_segment_ptr->status                    = STATUS_FULL_REUPLOAD_NEEDED;
        tcb_segment_ptr->tcb_ubo_modification_time = time_now;
    }

    /* If a reupload is needed, do it now */
    switch(tcb_segment_ptr->status)
    {
        case STATUS_PARTIAL_REUPLOAD_NEEDED:
        {
            _curve_editor_curve_window_renderer_update_tcb_ubo(segment,
                                                               tcb_segment_ptr->tcb_ubo_id,
                                                               true,
                                                               tcb_segment_ptr->partial_reupload_start_index,
                                                               tcb_segment_ptr->partial_reupload_end_index - tcb_segment_ptr->partial_reupload_start_index,
                                                               descriptor_ptr,
                                                               entry_points);

            tcb_segment_ptr->status                    = STATUS_NO_REUPLOAD_NEEDED;
            tcb_segment_ptr->tcb_ubo_modification_time = system_time_now();

            break;
        }

        case STATUS_FULL_REUPLOAD_NEEDED:
        {
            _curve_editor_curve_window_renderer_update_tcb_ubo(segment,
                                                               tcb_segment_ptr->tcb_ubo_id,
                                                               false,
                                                               0,
                                                               n_nodes,
                                                               descriptor_ptr,
                                                               entry_points);

            tcb_segment_ptr->tcb_ubo_modification_time = system_time_now();
            tcb_segment_ptr->status                    = STATUS_NO_REUPLOAD_NEEDED;

            break;
        }
    }

    /* Draw a line strip, separate for each node-node region */
    int                     node_indexes[4]  = {-1, -1, -1, -1};
    curve_segment_node_id   node_a_id        = -1;
    system_timeline_time    node_a_time      = 0;
    uint32_t                node_a_time_msec = 0;
    curve_segment_node_id   node_b_id        = -1;
    system_timeline_time    node_b_time      = 0;
    uint32_t                node_b_time_msec = 0;
    curve_segment_node_id   node_c_id        = -1;
    system_timeline_time    node_c_time      = 0;
    uint32_t                node_c_time_msec = 0;
    curve_segment_node_id   node_d_id        = -1;
    system_timeline_time    node_d_time      = 0;
    uint32_t                node_d_time_msec = 0;
    system_variant_type     segment_data_type;

    curve_segment_get_node_in_order(segment,
                                    0,
                                   &node_b_id);
    curve_segment_get_node_in_order(segment,
                                    1,
                                   &node_c_id);

    if (n_nodes > 2)
    {
        curve_segment_get_node_in_order(segment,
                                        2,
                                       &node_d_id);
    }

    curve_segment_get_node(segment,
                           node_b_id,
                          &node_b_time,
                           NULL);
    curve_segment_get_node(segment,
                           node_c_id,
                          &node_c_time,
                           NULL);

    if (n_nodes > 2)
    {
        curve_segment_get_node(segment,
                               node_d_id,
                              &node_d_time,
                              NULL);
    }

    node_indexes[1] = node_b_id;
    node_indexes[2] = node_c_id;
    node_indexes[3] = node_d_id;

    system_time_get_msec_for_timeline_time(node_b_time,
                                          &node_b_time_msec);
    system_time_get_msec_for_timeline_time(node_c_time,
                                          &node_c_time_msec);
    system_time_get_msec_for_timeline_time(node_d_time,
                                          &node_d_time_msec);

    ASSERT_DEBUG_SYNC(node_b_time < node_c_time &&
                      node_c_time < node_d_time,
                      "Nodes are not sorted in order!");

    curve_segment_get_node_value_variant_type(segment,
                                             &segment_data_type);

    const bool new_should_round_data_value = segment_data_type == SYSTEM_VARIANT_INTEGER;

    curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                          CURVE_EDITOR_PROGRAM_TCB_PROPERTY_SHOULD_ROUND_DATA,
                                         &new_should_round_data_value);
    curve_editor_program_tcb_use         (context,
                                          _globals->tcb_curve_program);

    for (uint32_t n_node = 0;
                  n_node < n_nodes - 1;
                ++n_node)
    {
        float node_b_ss =       LERP_FROM_A_B_TO_C_D(float(node_b_time_msec) / 1000.0f,
                                                     descriptor_ptr->x1,
                                                     (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                                    -1,
                                                     1);
        int   width_px  = (int) LERP_FROM_A_B_TO_C_D(float(node_c_time_msec - node_b_time_msec) / 1000.0f,
                                                     0,
                                                     descriptor_ptr->x_width,
                                                     0,
                                                     (window_rect.right - window_rect.left) ) * 2;

        const float new_start_time_value = float(node_b_time_msec) / 1000.0f;

        curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                              CURVE_EDITOR_PROGRAM_TCB_PROPERTY_NODE_INDEXES_DATA,
                                              node_indexes);
        curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                              CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_TIME_DATA,
                                             &new_start_time_value);
        curve_editor_program_tcb_set_property(_globals->tcb_curve_program,
                                              CURVE_EDITOR_PROGRAM_TCB_PROPERTY_START_X_DATA,
                                             &node_b_ss);

        ASSERT_DEBUG_SYNC(width_px >= 0,
                          "Invalid width (in pixels): %d",
                          width_px);

        if (width_px > 0)
        {
            curve_editor_program_tcb_use(context,
                                         _globals->tcb_curve_program);

            entry_points->pGLDrawArrays(GL_LINE_STRIP,
                                        0,
                                        width_px);
        }

        /* Update node indexes */
        node_indexes[0] = node_indexes[1];
        node_a_time     = node_b_time;
        node_indexes[1] = node_indexes[2];
        node_b_time     = node_c_time;
        node_indexes[2] = node_indexes[3];
        node_c_time     = node_d_time;

        if ((n_node + 3) < n_nodes)
        {
            node_d_id = -1;

            curve_segment_get_node_in_order(segment,
                                            n_node + 3,
                                           &node_d_id);
            curve_segment_get_node         (segment,
                                            node_d_id,
                                           &node_d_time,
                                            NULL);

            node_indexes[3] = node_d_id;
        }
        else
        {
            node_indexes[3] = -1;
            node_d_time     = 0;
        }

        system_time_get_msec_for_timeline_time(node_a_time,
                                              &node_a_time_msec);
        system_time_get_msec_for_timeline_time(node_b_time,
                                              &node_b_time_msec);

        if (node_indexes[2] != -1)
        {
            system_time_get_msec_for_timeline_time(node_c_time,
                                                  &node_c_time_msec);
            
            ASSERT_DEBUG_SYNC(node_b_time < node_c_time,
                          "Nodes are not sorted in order!");
        }

        if (node_indexes[3] != -1)
        {
            system_time_get_msec_for_timeline_time(node_d_time,
                                                  &node_d_time_msec);

            ASSERT_DEBUG_SYNC(node_c_time < node_d_time,
                              "Nodes are not sorted in order!");
        }

        ASSERT_DEBUG_SYNC(node_a_time < node_b_time,
                          "Nodes are not sorted in order!");
    }

    /* Draw nodes */
    float node_width_half_ss  = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX,
                                                     0,
                                                     (window_rect.right  - window_rect.left),
                                                     0,
                                                     2) / 2.0f;
    float node_height_half_ss = LERP_FROM_A_B_TO_C_D(NODE_SIZE_PX,
                                                     0,
                                                     (window_rect.bottom - window_rect.top),
                                                     0,
                                                     2) / 2.0f;

    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        curve_segment_node_id node_id;
        system_timeline_time  node_time;

        if (curve_segment_get_node_in_order(segment,
                                            n_node,
                                           &node_id)                                  &&
            curve_segment_get_node         (segment,
                                            node_id,
                                           &node_time,
                                            descriptor_ptr->float_variant) )
        {
            uint32_t node_time_msec;
            float    node_value;

            system_time_get_msec_for_timeline_time(node_time,
                                                  &node_time_msec);
            system_variant_get_float              (descriptor_ptr->float_variant,
                                                  &node_value);

            float node_x_ss    = LERP_FROM_A_B_TO_C_D(node_time_msec,
                                                      descriptor_ptr->x1 * 1000.0f,
                                                      (descriptor_ptr->x1 + descriptor_ptr->x_width) * 1000.0f,
                                                     -1,
                                                      1);
            float node_y_ss    = LERP_FROM_A_B_TO_C_D(node_value,
                                                      descriptor_ptr->y1,
                                                      (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                                     -1,
                                                      1);
            float node_x_start = node_x_ss - node_width_half_ss;
            float node_x_end   = node_x_ss + node_width_half_ss;
            float node_y_start = node_y_ss - node_height_half_ss;
            float node_y_end   = node_y_ss + node_height_half_ss;
            bool  is_node_lit  = (cursor_x_ss >= node_x_start &&
                                  cursor_x_ss <= node_x_end   &&
                                  cursor_y_ss >= node_y_start &&
                                  cursor_y_ss <= node_y_end);

            if (is_node_lit)
            {
                descriptor_ptr->nodemove_node_id    = node_id;
                descriptor_ptr->nodemove_segment_id = segment_id;
            }

            /* This could be done so much better.. */
            _curve_editor_curve_window_renderer_draw_curve_node_ss(descriptor_ptr,
                                                                   context,
                                                                   entry_points,
                                                                   node_x_start,
                                                                   node_x_end,
                                                                   node_y_start,
                                                                   node_y_end,
                                                                   is_node_lit,
                                                                   segment_id,
                                                                   node_id);

        } /* if (curve_segment_get_node_in_order(segment, n_node, &node_id) ) */
    }
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_init(_curve_editor_curve_window_renderer* descriptor)
{
    bool result = false;

    /* We need to create a new context as rendering handler needs to be bound on a 1:1 basis. All contexts share namespaces
     * so we're on the safe side.
     **/
    const char* string_table[] =
    {
        "Curve window renderer for ",
        system_hashed_ansi_string_get_buffer(descriptor->name)
    };

    system_hashed_ansi_string full_name = system_hashed_ansi_string_create_by_merging_strings(2,
                                                                                              string_table);

    descriptor->window = system_window_create_by_replacing_window(full_name,
                                                                  OGL_CONTEXT_TYPE_GL,
                                                                  0,
                                                                  true,
                                                                  descriptor->view_window_handle,
                                                                  false);

    /* Register for call-backs we need to handle scrolling requests */
    system_window_add_callback_func(descriptor->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                    _curve_editor_curve_window_renderer_on_left_button_down,
                                    descriptor);
    system_window_add_callback_func(descriptor->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_UP,
                                    _curve_editor_curve_window_renderer_on_left_button_up,
                                    descriptor);
    system_window_add_callback_func(descriptor->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                    _curve_editor_curve_window_renderer_on_mouse_move,
                                    descriptor);
    system_window_add_callback_func(descriptor->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_WHEEL,
                                    _curve_editor_curve_window_renderer_on_mouse_wheel,
                                    descriptor);
    system_window_add_callback_func(descriptor->window,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_UP,
                                    _curve_editor_curve_window_renderer_on_right_button_up,
                                    descriptor);

    /* Create the rendering handler now. */
    descriptor->rendering_handler = ogl_rendering_handler_create_with_render_per_request_policy(system_hashed_ansi_string_create_by_merging_two_strings("Curve window renderer for ",
                                                                                                                                                        system_hashed_ansi_string_get_buffer(full_name) ),
                                                                                                _curve_editor_curve_window_renderer_rendering_callback_handler,
                                                                                                descriptor);

    /* Bind the rendering handler to our window */
    result = system_window_set_rendering_handler(descriptor->window,
                                                 descriptor->rendering_handler);

    ASSERT_DEBUG_SYNC(result,
                      "Could not bind rendering handler to curve window renderer window.");

    /* Hide the window we use for retrieving dimensions */
    RECT window_rect = {0};

    ::GetClientRect(descriptor->view_window_handle,
                   &window_rect);
    ::ShowWindow   (descriptor->view_window_handle,
                    SW_HIDE);

    /* Create text renderer */
    float text_color[] = {1, 1, 1, 1};
    float text_scale   = 0.4f;

    system_window_get_property(descriptor->window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &descriptor->context);

    string_table[0] = "Curve window renderer text renderer for ";

    descriptor->text_renderer = ogl_text_create(system_hashed_ansi_string_create_by_merging_strings(2,
                                                                                                    string_table),
                                                descriptor->context,
                                                system_resources_get_meiryo_font_table(),
                                                window_rect.right  - window_rect.left,
                                                window_rect.bottom - window_rect.top);

    ogl_text_set_text_string_property(descriptor->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_COLOR,
                                      text_color);
    ogl_text_set_text_string_property(descriptor->text_renderer,
                                      TEXT_STRING_ID_DEFAULT,
                                      OGL_TEXT_STRING_PROPERTY_SCALE,
                                      &text_scale);

    for (uint32_t n = 0;
                  n < N_TEXT_STRINGS_ON_START;
                ++n)
    {
        ogl_text_add_string(descriptor->text_renderer);
    } /* for (uint32_t n = 0; n < N_TEXT_STRINGS_ON_START; ++n) */

    descriptor->n_text_strings = N_TEXT_STRINGS_ON_START;

    /* Initialize editor globals */
    if (_globals == NULL)
    {
        ogl_rendering_handler_request_callback_from_context_thread(descriptor->rendering_handler,
                                                                   _curve_editor_curve_window_renderer_init_globals,
                                                                   NULL);
    }

    /* Render one frame */
    ogl_rendering_handler_play(descriptor->rendering_handler,
                               0);

    /* That's it */
    result = true;
    return result;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_init_descriptor(     _curve_editor_curve_window_renderer* descriptor,
                                                                      HWND                                 view_window_handle,
                                                                      system_hashed_ansi_string            name,
                                                                 __in curve_container                      curve,
                                                                 __in curve_editor_curve_window            owner)
{
    descriptor->context                                    = NULL;
    descriptor->curve                                      = curve;
    descriptor->float_variant                              = system_variant_create(SYSTEM_VARIANT_FLOAT);
    descriptor->hovered_curve_segment_id                   = -1;
    descriptor->is_left_button_down                        = false;
    descriptor->max_screen_height                          = MAX_SCREEN_HEIGHT_DEFAULT;
    descriptor->max_screen_width                           = MAX_SCREEN_WIDTH_DEFAULT;
    descriptor->mouse_x                                    = 0;
    descriptor->mouse_y                                    = 0;
    descriptor->name                                       = name;
    descriptor->nodemove_mode_active                       = false;
    descriptor->nodemove_node_id                           = -1;
    descriptor->nodemove_segment_id                        = 0;
    descriptor->n_text_strings                             = 0;
    descriptor->owner                                      = owner;
    descriptor->quadselector_mode_active                   = false;
    descriptor->segmentmove_mode_active                    = false;
    descriptor->segmentresize_mode_active                  = false;
    descriptor->selected_node_id                           = -1;
    descriptor->selected_segment_id                        = -1;
    descriptor->static_color_program_a_ub_offset           = -1;
    descriptor->static_color_program_b_ub_offset           = -1;
    descriptor->static_color_program_color_ub_offset       = -1;
    descriptor->static_color_program_mvp_ub_offset         = -1;
    descriptor->static_color_program_ub_fs                 = NULL;
    descriptor->static_color_program_ub_fs_bo_id           = 0;
    descriptor->static_color_program_ub_fs_bo_size         = 0;
    descriptor->static_color_program_ub_fs_bo_start_offset = 0;
    descriptor->static_color_program_ub_vs                 = NULL;
    descriptor->static_color_program_ub_vs_bo_id           = 0;
    descriptor->static_color_program_ub_vs_bo_size         = 0;
    descriptor->static_color_program_ub_vs_bo_start_offset = 0;
    descriptor->tcb_segments                               = system_hash64map_create(sizeof(void*) );
    descriptor->text_renderer                              = NULL;
    descriptor->text_variant                               = system_variant_create(SYSTEM_VARIANT_ANSI_STRING);
    descriptor->view_window_handle                         = view_window_handle;
    descriptor->window                                     = NULL;
    descriptor->x1                                         = START_X1;
    descriptor->x_width                                    = START_X_WIDTH;
    descriptor->y1                                         = START_Y1;
    descriptor->y_height                                   = START_Y_HEIGHT;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_init_globals(ogl_context context,
                                                              void*       not_used)
{
    ASSERT_DEBUG_SYNC(_globals == NULL,
                      "Globals not null - reinitialization to take place!");
    ASSERT_DEBUG_SYNC(context  != NULL,
                      "Input GL context is null.");

    _globals = new (std::nothrow) _curve_window_renderer_globals;

    ASSERT_DEBUG_SYNC(_globals != NULL,
                      "Out of memory while allocating globals for curve window renderer.");

    if (_globals != NULL)
    {
        system_timeline_time start_time = system_time_now();

        /* Create background program */
        _globals->bg_program = ogl_program_create(context,
                                                  system_hashed_ansi_string_create("Background program") );

        ASSERT_DEBUG_SYNC(_globals->bg_program != NULL,
                          "Could not create background program for curve window renderer.");

        _globals->bg_fragment_shader = ogl_shader_create               (context,
                                                                        SHADER_TYPE_FRAGMENT,
                                                                        system_hashed_ansi_string_create("Curve window renderer: background fragment shader") );
        _globals->bg_vertex_shader   = shaders_vertex_fullscreen_create(context,
                                                                        true,
                                                                        system_hashed_ansi_string_create("Curve window renderer: background vertex shader") );

        ASSERT_DEBUG_SYNC(_globals->bg_fragment_shader != NULL,
                          "Could not create background fragment shader for curve window renderer.");
        ASSERT_DEBUG_SYNC(_globals->bg_vertex_shader   != NULL,
                          "Could not create background vertex shader for curve window renderer.");

        /* Configure background fragment shader. */
        bool result = ogl_shader_set_body(_globals->bg_fragment_shader,
                                          _bg_fragment_shader_body);

        ASSERT_DEBUG_SYNC(result,
                          "Could not set background fragment shader body for curve window renderer.");

        /* Configure background program */
        result = ogl_program_attach_shader(_globals->bg_program,
                                           _globals->bg_fragment_shader);
        ASSERT_DEBUG_SYNC(result,
                          "Could not attach background fragment shader to background program of curve window renderer.");

        result = ogl_program_attach_shader(_globals->bg_program,
                                           shaders_vertex_fullscreen_get_shader(_globals->bg_vertex_shader) );

        ASSERT_DEBUG_SYNC(result,
                          "Could not attach background vertex shader to background program of curve window renderer.");

        result = ogl_program_link(_globals->bg_program);
        ASSERT_DEBUG_SYNC(result,
                          "Could not link background program for curve window renderer.");

        /* Create static color program and corresponding shaders */
        _globals->static_color_program = ogl_program_create(context,
                                                            system_hashed_ansi_string_create("Static color program"),
                                                            OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_PER_CONTEXT);

        ASSERT_DEBUG_SYNC(_globals->static_color_program != NULL,
                          "Could not craete static color program for curve window renderer.");

        _globals->static_color_fragment_shader = ogl_shader_create                                    (context,
                                                                                                       SHADER_TYPE_FRAGMENT,
                                                                                                       system_hashed_ansi_string_create("Curve window renderer: static color fragment shader") );
        _globals->static_color_vertex_shader   = shaders_vertex_combinedmvp_simplified_twopoint_create(context,
                                                                                                       system_hashed_ansi_string_create("Curve window renderer: static color vertex shader") );

        ASSERT_DEBUG_SYNC(_globals->static_color_fragment_shader != NULL,
                          "Could not create static color fragment shader for curve window renderer.");
        ASSERT_DEBUG_SYNC(_globals->static_color_vertex_shader   != NULL,
                          "Could not create static color vertex shader for curve window renderer.");

        /* Configure shaders */
        result = ogl_shader_set_body(_globals->static_color_fragment_shader,
                                     _static_color_fragment_shader_body);

        ASSERT_DEBUG_SYNC(result,
                          "Could not set static color fragment shader body for curve window renderer.");

        /* Configure static color program */
        result = ogl_program_attach_shader(_globals->static_color_program,
                                           _globals->static_color_fragment_shader);

        ASSERT_DEBUG_SYNC(result,
                          "Could not attach static color fragment shader to static color program of curve window renderer.");

        result = ogl_program_attach_shader(_globals->static_color_program,
                                           shaders_vertex_combinedmvp_simplified_twopoint_get_shader(_globals->static_color_vertex_shader) );

        ASSERT_DEBUG_SYNC(result,
                          "Could not attach static color vertex shader to static color program of curve window renderer.");

        result = ogl_program_link(_globals->static_color_program);

        ASSERT_DEBUG_SYNC(result,
                          "Could not link static color program for curve window renderer.");

        /* Create curve background program */
        _globals->curvebackground_program = curve_editor_program_curvebackground_create(context,
                                                                                        system_hashed_ansi_string_create("Global Curve Background") );

        ASSERT_DEBUG_SYNC(_globals->curvebackground_program != NULL,
                          "Could not create curve background program.");

        /* Create curve static program */
        _globals->static_curve_program = curve_editor_program_static_create(context,
                                                                            system_hashed_ansi_string_create("Global Curve Static") );

        ASSERT_DEBUG_SYNC(_globals->static_curve_program != NULL,
                          "Could not create static curve program.");

        /* Create curve lerp program */
        _globals->lerp_curve_program = curve_editor_program_lerp_create(context,
                                                                        system_hashed_ansi_string_create("Global Curve LERP") );

        ASSERT_DEBUG_SYNC(_globals->lerp_curve_program != NULL,
                          "Could not create lerp curve program.");

        /* Create curve tcb program */
        _globals->tcb_curve_program = curve_editor_program_tcb_create(context,
                                                                      system_hashed_ansi_string_create("Global Curve TCB") );

        ASSERT_DEBUG_SYNC(_globals->tcb_curve_program != NULL,
                          "Could not create tcb curve program.");

        /* Create quad selector program */
        _globals->quadselector_program = curve_editor_program_quadselector_create(context,
                                                                                  system_hashed_ansi_string_create("Global Curve QuadSelector") );

        ASSERT_DEBUG_SYNC(_globals->quadselector_program != NULL,
                          "Could not create quad selector program");

        /* Reset reference counter */
        _globals->ref_counter = 1;

        system_timeline_time end_time            = system_time_now();
        uint32_t             execution_time_msec = 0;

        system_time_get_msec_for_timeline_time(end_time - start_time,
                                              &execution_time_msec);

        LOG_INFO("Time to initialize curve window renderer: %d ms",
                 execution_time_msec);

        /* Mark initialization as done */
        _is_globals_initialized = true;
    }
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_init_tcb_segment_rendering(ogl_context                          context,
                                                                            _curve_editor_curve_window_renderer* renderer_ptr,
                                                                            const ogl_context_gl_entrypoints*    entry_points,
                                                                            curve_container                      curve,
                                                                            curve_segment_id                     segment_id)
{
    _tcb_segment_rendering* new_descriptor = new (std::nothrow) _tcb_segment_rendering;

    ASSERT_DEBUG_SYNC(new_descriptor != NULL,
                      "Could not allocate space for _tcb_segment_rendering");

    if (new_descriptor != NULL)
    {
        entry_points->pGLGenBuffers(1,
                                   &new_descriptor->tcb_ubo_id);

        /* Update contents of the TCB UBO for all nodes */
        uint32_t      n_nodes = 0;
        curve_segment segment = curve_container_get_segment(curve,
                                                            segment_id);

        if (curve_segment_get_amount_of_nodes(segment,
                                             &n_nodes) )
        {
            _curve_editor_curve_window_renderer_update_tcb_ubo(segment,
                                                               new_descriptor->tcb_ubo_id,
                                                               false,
                                                               0,
                                                               n_nodes,
                                                               renderer_ptr,
                                                               entry_points);

            new_descriptor->status = STATUS_NO_REUPLOAD_NEEDED;
        }
        else
        {
            LOG_ERROR("Could not retrieve amount of nodes! TCB segment is marked as requiring full data reupload");

            new_descriptor->status = STATUS_FULL_REUPLOAD_NEEDED;
        }

        new_descriptor->tcb_ubo_modification_time = system_time_now();
    }

    /* Insert the descriptor */
    bool result = system_hash64map_insert(renderer_ptr->tcb_segments,
                                          segment_id,
                                          new_descriptor,
                                          NULL,
                                          NULL);

    ASSERT_ALWAYS_SYNC(result, "Could not insert TCB segment descriptor");
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_on_left_button_down(system_window           window,
                                                                     LONG                    x,
                                                                     LONG                    y,
                                                                     system_window_vk_status status,
                                                                     void*                   renderer)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;
    HWND                                 window_handle;

    system_window_get_property(renderer_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &renderer_ptr->quadselector_window_rect);

    renderer_ptr->is_left_button_down           = true;
    renderer_ptr->left_button_click_location[0] = x;
    renderer_ptr->left_button_click_location[1] = y;
    renderer_ptr->left_button_click_x1y1    [0] = renderer_ptr->x1;
    renderer_ptr->left_button_click_x1y1    [1] = renderer_ptr->y1;

    if (status & SYSTEM_WINDOW_VK_STATUS_CONTROL_PRESSED)
    {
        /* Make sure does not try to create a segment for x < 0 */
        LONG zero_in_px = (LONG) LERP_FROM_A_B_TO_C_D(0,
                                                      renderer_ptr->x1,
                                                      (renderer_ptr->x1 + renderer_ptr->x_width),
                                                      renderer_ptr->quadselector_window_rect.left,
                                                      renderer_ptr->quadselector_window_rect.right);

        if (x < zero_in_px)
        {
            x = zero_in_px;
        }

        renderer_ptr->quadselector_start_mouse_x = x;
        renderer_ptr->quadselector_mode_active   = true;
    }
    else
    {
        renderer_ptr->quadselector_mode_active = false;

        /* Update node selection */
        renderer_ptr->selected_node_id    = renderer_ptr->nodemove_node_id;
        renderer_ptr->selected_segment_id = renderer_ptr->nodemove_segment_id;

        /* If node can be moved, let's take care of it */
        if (renderer_ptr->nodemove_segment_id != 0)
        {
            renderer_ptr->nodemove_mode_active = true;
        }
        else
        /* If segment resizing is possible, let's get down to it */
        if (renderer_ptr->segmentresize_segment_id != 0)
        {
            renderer_ptr->segmentresize_mode_active = true;
        }
        else
        {
            /* See if the user is hovering over a segment. If he/she is, we should enable 'move segment' mode */
            curve_segment_id     segment_id;
            float                mouse_x      = LERP_FROM_A_B_TO_C_D                  (x,
                                                                                       renderer_ptr->quadselector_window_rect.left,
                                                                                       renderer_ptr->quadselector_window_rect.right,
                                                                                       renderer_ptr->x1,
                                                                                       (renderer_ptr->x1 + renderer_ptr->x_width) );
            system_timeline_time mouse_x_time = system_time_get_timeline_time_for_msec( uint32_t(mouse_x * 1000.0f) );

            if (curve_container_get_segment_id_relative_to_time(false,
                                                                false,
                                                                renderer_ptr->curve,
                                                                mouse_x_time,
                                                                -1,
                                                               &segment_id) )
            {
                bool                 has_retrieved_segment_data = true;
                system_timeline_time segment_start_time;
                system_timeline_time segment_end_time;
                curve_segment_type   segment_type;

                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                                                  &segment_start_time);
                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                                                  &segment_end_time);
                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE,
                                                                                  &segment_type);

                if (has_retrieved_segment_data)
                {
                    renderer_ptr->segmentmove_click_x     = x;
                    renderer_ptr->segmentmove_mode_active = true;
                    renderer_ptr->segmentmove_segment_id  = segment_id;
                    renderer_ptr->segmentmove_start_time  = segment_start_time;
                    renderer_ptr->segmentmove_end_time    = segment_end_time;
                }
            }
        }
    }

    /* Redraw just in case */
    ogl_rendering_handler_play(renderer_ptr->rendering_handler,
                               0);

    /* Enable capturing */
    ::SetCapture(window_handle);

    return true;
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_on_left_button_up(system_window           window_handle,
                                                                   LONG                    x,
                                                                   LONG                    y,
                                                                   system_window_vk_status vk_status,
                                                                   void*                   renderer)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;

    if (renderer_ptr->quadselector_mode_active)
    {
        /* Prepare menu callback argument */
        _curve_window_renderer_segment_creation_callback_argument arg;

        float x1 = LERP_FROM_A_B_TO_C_D(renderer_ptr->quadselector_start_mouse_x,
                                        renderer_ptr->quadselector_window_rect.left,
                                        renderer_ptr->quadselector_window_rect.right,
                                        renderer_ptr->x1,
                                        (renderer_ptr->x1 + renderer_ptr->x_width) );
        float x2 = LERP_FROM_A_B_TO_C_D(renderer_ptr->quadselector_mouse_x,
                                        renderer_ptr->quadselector_window_rect.left,
                                        renderer_ptr->quadselector_window_rect.right,
                                        renderer_ptr->x1,
                                        (renderer_ptr->x1 + renderer_ptr->x_width) );
        float x1x2[2];

        if (x1 < 0)
        {
            x1 = 0;
        }

        if (x2 < 0)
        {
            x2 = 0;
        }

        if (x1 < x2)
        {
            x1x2[0] = x1;
            x1x2[1] = x2;
        }
        else
        {
            x1x2[0] = x2;
            x1x2[1] = x1;
        }

        arg.curve           = renderer_ptr->curve;
        arg.renderer        = (curve_editor_curve_window_renderer) renderer_ptr;
        arg.start_time      = system_time_get_timeline_time_for_msec(uint32_t(x1x2[0] * 1000.0f) );
        arg.end_time        = system_time_get_timeline_time_for_msec(uint32_t(x1x2[1] * 1000.0f) );

        /* Got to show the context menu now */
        system_context_menu  context_menu         = system_context_menu_create(system_hashed_ansi_string_create("Segment type chooser") );
        system_window_handle system_window_handle = NULL;

        system_window_get_property(window_handle,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &system_window_handle);

        system_context_menu_append_item(context_menu,
                                        system_hashed_ansi_string_create("Static segment"),
                                        curve_editor_curve_window_create_static_segment_handler,
                                        &arg,
                                        false, /* not checked */
                                        true);/* enabled */
        system_context_menu_append_item(context_menu,
                                        system_hashed_ansi_string_create("LERP segment"),
                                        curve_editor_curve_window_create_lerp_segment_handler,
                                        &arg,
                                        false, /* not checked */
                                        true);/* enabled */
        system_context_menu_append_item(context_menu,
                                        system_hashed_ansi_string_create("TCB segment"),
                                        curve_editor_curve_window_create_tcb_segment_handler,
                                        &arg,
                                        false, /* not checked */
                                        true);/* enabled */

        system_context_menu_show   (context_menu,
                                    system_window_handle,
                                    x,
                                    y);
        system_context_menu_release(context_menu);
    }
    else
    if ( !renderer_ptr->nodemove_mode_active                                                                                                              &&
        (!renderer_ptr->segmentmove_mode_active || renderer_ptr->segmentmove_mode_active && abs(renderer_ptr->segmentmove_click_x - x) < CLICK_THRESHOLD) &&
         !renderer_ptr->segmentresize_mode_active)
    {
        /* If user has clicked within a TCB segment, we need to add a new node */
        HWND window;
        RECT window_rect;

        system_window_get_property(renderer_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &window);
        ::GetWindowRect           (window,
                                  &window_rect);

        curve_segment_id     clicked_segment_id = -1;
        float                clicked_time_msec  = LERP_FROM_A_B_TO_C_D                  (x,
                                                                                         window_rect.left,
                                                                                         window_rect.right,
                                                                                         renderer_ptr->x1,
                                                                                         (renderer_ptr->x1 + renderer_ptr->x_width) ) * 1000.0f;
        system_timeline_time clicked_time       = system_time_get_timeline_time_for_msec(uint32_t(clicked_time_msec) );

        if (curve_container_get_segment_id_relative_to_time(false,
                                                            false,
                                                            renderer_ptr->curve,
                                                            clicked_time,
                                                            -1,
                                                           &clicked_segment_id) )
        {
            curve_segment clicked_segment = curve_container_get_segment(renderer_ptr->curve,
                                                                        clicked_segment_id);

            if (clicked_segment != NULL)
            {
                renderer_ptr->selected_segment_id = clicked_segment_id;
                renderer_ptr->selected_node_id    = -1;

                if (vk_status & SYSTEM_WINDOW_VK_STATUS_SHIFT_PRESSED)
                {
                    /* Only TCB segments allow new nodes */
                    curve_segment_type clicked_segment_type;

                    if (curve_segment_get_type(clicked_segment, &clicked_segment_type) &&
                        clicked_segment_type == CURVE_SEGMENT_TCB)
                    {
                        /* Excellent. Means we should create a new node at the pointed place */
                        float                 clicked_value = LERP_FROM_A_B_TO_C_D(y,
                                                                                   window_rect.bottom,
                                                                                   window_rect.top,
                                                                                   renderer_ptr->y1,
                                                                                   (renderer_ptr->y1 + renderer_ptr->y_height) );
                        curve_segment_node_id new_node_id   = 0;

                        system_variant_set_float(renderer_ptr->float_variant,
                                                 clicked_value);
                        curve_segment_add_node  (clicked_segment,
                                                 clicked_time,
                                                 renderer_ptr->float_variant,
                                                &new_node_id);

                        /* We need to reupload node data for TCB segment at this point! */
                        _tcb_segment_rendering* clicked_segment_renderer_data = NULL;

                        if (system_hash64map_get(renderer_ptr->tcb_segments,
                                                 clicked_segment_id,
                                                &clicked_segment_renderer_data) &&
                            clicked_segment_renderer_data != NULL)
                        {
                            clicked_segment_renderer_data->status = STATUS_FULL_REUPLOAD_NEEDED;
                        }
                    }
                }
            }
        }
    }

    renderer_ptr->is_left_button_down       = false;
    renderer_ptr->nodemove_mode_active      = false;
    renderer_ptr->quadselector_mode_active  = false;
    renderer_ptr->segmentmove_mode_active   = false;
    renderer_ptr->segmentresize_mode_active = false;

    if (renderer_ptr->selected_segment_id != 0 &&
        renderer_ptr->nodemove_node_id    != -1)
    {
        curve_editor_select_node(renderer_ptr->owner,
                                 renderer_ptr->selected_segment_id,
                                 renderer_ptr->nodemove_node_id);
    }
    else
    {
        curve_editor_select_node(renderer_ptr->owner,
                                 -1,
                                 -1);
    }

    ::ReleaseCapture();

    return true;
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_on_right_button_up(system_window           window_handle,
                                                                    LONG                    x,
                                                                    LONG                    y,
                                                                    system_window_vk_status,
                                                                    void*                   renderer)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;

    /* Check which segment the user is currently hovering over */
    HWND window_system_handle;
    RECT window_rect;

    system_window_get_property(renderer_ptr->window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_system_handle);
    ::GetWindowRect           (window_system_handle,
                              &window_rect);

    float                hover_time_s             = LERP_FROM_A_B_TO_C_D                  ((x - window_rect.left),
                                                                                           0,
                                                                                           (window_rect.right - window_rect.left),
                                                                                           renderer_ptr->x1,
                                                                                           (renderer_ptr->x1 + renderer_ptr->x_width));
    system_timeline_time hover_time               = system_time_get_timeline_time_for_msec(uint32_t(hover_time_s * 1000.0f) );

    curve_container_get_segment_id_relative_to_time(false,
                                                    false,
                                                    renderer_ptr->curve,
                                                    hover_time,
                                                    -1,
                                                   &renderer_ptr->hovered_curve_segment_id);

    /* If user has selected a node and pressed the right button, we need to let him remove the node */
    if (renderer_ptr->selected_segment_id      != -1 &&
        renderer_ptr->selected_node_id         != -1 ||
        renderer_ptr->hovered_curve_segment_id != -1)
    {
        system_context_menu  context_menu         = system_context_menu_create(system_hashed_ansi_string_create("Node options chooser") );
        system_window_handle system_window_handle = NULL;

        system_window_get_property(window_handle,
                                   SYSTEM_WINDOW_PROPERTY_HANDLE,
                                  &system_window_handle);

        if (renderer_ptr->selected_node_id != -1)
        {
            system_context_menu_append_item(context_menu,
                                            system_hashed_ansi_string_create("Remove node"),
                                            _curve_editor_curve_window_remove_node_handler,
                                            renderer_ptr,
                                            false, /* not checked */
                                            true);/* enabled */
        }

        if (renderer_ptr->hovered_curve_segment_id != -1)
        {
            system_context_menu_append_item(context_menu,
                                            system_hashed_ansi_string_create("Remove segment"),
                                            _curve_editor_curve_window_remove_segment_handler,
                                            renderer_ptr,
                                            false /* not checked */,
                                            true);
        }

        system_context_menu_show   (context_menu,
                                    system_window_handle,
                                    x,
                                    y);
        system_context_menu_release(context_menu);
    }

    return true;
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_on_mouse_move(system_window           window,
                                                               LONG                    x,
                                                               LONG                    y,
                                                               system_window_vk_status,
                                                               void*                   renderer)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;

    if ( renderer_ptr->is_left_button_down      &&
        !renderer_ptr->nodemove_mode_active     &&
        !renderer_ptr->quadselector_mode_active &&
        !renderer_ptr->segmentmove_mode_active  &&
        !renderer_ptr->segmentresize_mode_active)
    {
        int size_pixels[2] = {0};

        system_window_get_property(renderer_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   size_pixels);

        /* Scrolling .. */
        LONG  dx_pixels =  LONG(renderer_ptr->left_button_click_location[0]) - x;
        LONG  dy_pixels =  LONG(renderer_ptr->left_button_click_location[1]) - y;
        float dx_units  =  LERP_FROM_A_B_TO_C_D(dx_pixels,
                                                0,
                                                size_pixels[0],
                                                0,
                                                renderer_ptr->x_width);
        float dy_units  = -LERP_FROM_A_B_TO_C_D(dy_pixels,
                                                0,
                                                size_pixels[1],
                                                0,
                                                renderer_ptr->y_height);

        renderer_ptr->x1 = renderer_ptr->left_button_click_x1y1[0] + dx_units;
        renderer_ptr->y1 = renderer_ptr->left_button_click_x1y1[1] + dy_units;
    }

    /* As the value pointed by cursor most likely needs to change, do a redraw */
    HWND window_handle;
    RECT window_rect;

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_handle);
    ::GetWindowRect           (window_handle,
                              &window_rect);

    renderer_ptr->mouse_x = x - window_rect.left;
    renderer_ptr->mouse_y = y - window_rect.top;

    if (renderer_ptr->quadselector_mode_active)
    {
        /* Make sure does not try to create a segment for x < 0 */
        LONG zero_in_px = (LONG) LERP_FROM_A_B_TO_C_D(0,
                                                      renderer_ptr->x1,
                                                      (renderer_ptr->x1 + renderer_ptr->x_width),
                                                      window_rect.left,
                                                      window_rect.right);

        if (x < zero_in_px)
        {
            x = zero_in_px;
        }

        renderer_ptr->quadselector_mouse_x = x;
    }
    else
    /* If 'segment move' mode is enabled, move the segment according to the cursor */
    if (renderer_ptr->segmentmove_mode_active)
    {
        LONG                 x_delta                = x - renderer_ptr->segmentmove_click_x;
        float                x_delta_ss             = LERP_FROM_A_B_TO_C_D                  (x_delta,
                                                                                             0,
                                                                                             (window_rect.right - window_rect.left),
                                                                                             0,
                                                                                             renderer_ptr->x_width);
        system_timeline_time x_delta_time           = system_time_get_timeline_time_for_msec(uint32_t((x_delta_ss) * 1000.0f) );
        system_timeline_time new_segment_start_time;
        system_timeline_time new_segment_end_time;

        if (x_delta_ss >= 0)
        {
            new_segment_start_time = renderer_ptr->segmentmove_start_time + x_delta_time;
            new_segment_end_time   = renderer_ptr->segmentmove_end_time   + x_delta_time;

            if (new_segment_start_time < 0)
            {
                new_segment_start_time = 0;
            }
        }
        else
        {
            if (x_delta_time > renderer_ptr->segmentmove_start_time)
            {
                new_segment_start_time = 0;
            }
            else
            {
                new_segment_start_time = renderer_ptr->segmentmove_start_time - x_delta_time;

                if (new_segment_start_time < 0)
                {
                    new_segment_start_time = 0;
                }
            }

            new_segment_end_time = new_segment_start_time + (renderer_ptr->segmentmove_end_time - renderer_ptr->segmentmove_start_time);
        }

        if (new_segment_end_time < new_segment_start_time)
        {
            new_segment_end_time = new_segment_start_time;
        }

        curve_container_set_segment_times(renderer_ptr->curve,
                                          renderer_ptr->segmentmove_segment_id,
                                          new_segment_start_time,
                                          new_segment_end_time);

        /* If TCB segment was moved, we need node data reuploaded. Yes, this could have been more effective ;) */
        curve_segment      segment = curve_container_get_segment(renderer_ptr->curve,
                                                                 renderer_ptr->segmentmove_segment_id);
        curve_segment_type segment_type;

        if (curve_segment_get_type(segment, &segment_type) && 
            segment_type == CURVE_SEGMENT_TCB)
        {
           _tcb_segment_rendering* renderer_segment = NULL;

            if (system_hash64map_get(renderer_ptr->tcb_segments,
                                     renderer_ptr->segmentmove_segment_id,
                                    &renderer_segment) )
            {
                renderer_segment->status = STATUS_FULL_REUPLOAD_NEEDED;
            }
        }

        system_window_set_cursor(renderer_ptr->window, SYSTEM_WINDOW_MOUSE_CURSOR_MOVE);
    }
    else
    if (renderer_ptr->segmentresize_mode_active)
    {
        LONG                 x_delta                = x - renderer_ptr->segmentresize_click_x;
        float                x_delta_ss             = LERP_FROM_A_B_TO_C_D                  (x_delta,
                                                                                             0,
                                                                                             (window_rect.right - window_rect.left),
                                                                                             0,
                                                                                             renderer_ptr->x_width);
        system_timeline_time x_delta_time           = system_time_get_timeline_time_for_msec(uint32_t((x_delta_ss) * 1000.0f) );
        system_timeline_time new_segment_start_time = 0;
        system_timeline_time new_segment_end_time   = 0;

        if (x_delta_ss >= 0)
        {
            if (renderer_ptr->segmentresize_left_border_resize)
            {
                new_segment_start_time = renderer_ptr->segmentresize_start_time + x_delta_time;
                new_segment_end_time   = renderer_ptr->segmentresize_end_time;

                if (new_segment_start_time < 0)
                {
                    new_segment_start_time = 0;
                }
            }
            else
            {
                new_segment_start_time = renderer_ptr->segmentresize_start_time;
                new_segment_end_time   = renderer_ptr->segmentresize_end_time + x_delta_time;
            }
        }
        else
        {
            if (renderer_ptr->segmentresize_left_border_resize)
            {
                new_segment_start_time = renderer_ptr->segmentresize_start_time - x_delta_time;
                new_segment_end_time   = renderer_ptr->segmentresize_end_time;

                if (new_segment_start_time < 0)
                {
                    new_segment_start_time = 0;
                }
            }
            else
            {
                new_segment_start_time = renderer_ptr->segmentresize_start_time;
                new_segment_end_time   = renderer_ptr->segmentresize_end_time - x_delta_time;
            }
        }

        curve_container_set_segment_times(renderer_ptr->curve,
                                          renderer_ptr->segmentresize_segment_id,
                                          new_segment_start_time,
                                          new_segment_end_time);

        /* If TCB segment was resized, we need first node's data reuploaded. */
        curve_segment      segment = curve_container_get_segment(renderer_ptr->curve,
                                                                 renderer_ptr->segmentresize_segment_id);
        curve_segment_type segment_type;

        if (curve_segment_get_type(segment,
                                  &segment_type) &&
            segment_type == CURVE_SEGMENT_TCB)
        {
           _tcb_segment_rendering* renderer_segment = NULL;

            if (system_hash64map_get(renderer_ptr->tcb_segments,
                                     renderer_ptr->segmentresize_segment_id,
                                    &renderer_segment) )
            {
                renderer_segment->status = STATUS_FULL_REUPLOAD_NEEDED;
            }
        }
    }
    else
    if (renderer_ptr->nodemove_mode_active)
    {
        curve_segment        segment = curve_container_get_segment(renderer_ptr->curve,
                                                                   renderer_ptr->nodemove_segment_id);
        curve_segment_type   segment_type;
        float                x_ss    = LERP_FROM_A_B_TO_C_D                  (x,
                                                                              window_rect.left,
                                                                              window_rect.right,
                                                                              renderer_ptr->x1,
                                                                              (renderer_ptr->x1 + renderer_ptr->x_width) );
        system_timeline_time x_time  = system_time_get_timeline_time_for_msec(uint32_t((x_ss >= 0 ? x_ss : 0) * 1000.0f) );
        float                y_ss    = LERP_FROM_A_B_TO_C_D                  (y,
                                                                              window_rect.bottom,
                                                                              window_rect.top,
                                                                              renderer_ptr->y1,
                                                                              (renderer_ptr->y1 + renderer_ptr->y_height));

        system_variant_set_float_forced(renderer_ptr->float_variant,
                                        y_ss);
        curve_container_modify_node    (renderer_ptr->curve,
                                        renderer_ptr->nodemove_segment_id,
                                        renderer_ptr->nodemove_node_id,
                                        x_time,
                                        renderer_ptr->float_variant);

        /* To keep things extra-simple, we assume the worst case where full reupload is needed */
        if (curve_segment_get_type(segment, &segment_type) &&
            segment_type == CURVE_SEGMENT_TCB)
        {
            _tcb_segment_rendering* renderer_segment = NULL;

            if (system_hash64map_get(renderer_ptr->tcb_segments,
                                     renderer_ptr->nodemove_segment_id,
                                    &renderer_segment) )
            {
                renderer_segment->status = STATUS_FULL_REUPLOAD_NEEDED;
            }
        } /* if (curve_segment_get_type(segment, &segment_type) && segment_type == CURVE_SEGMENT_TCB) */
    }
    else
    {
        /* If no modes are active, make sure to update cursor depending on where it currently is placed */
        uint32_t n_segments         = 0;
        bool     can_resize_segment = false;

        curve_container_get_property(renderer_ptr->curve,
                                     CURVE_CONTAINER_PROPERTY_N_SEGMENTS,
                                    &n_segments);

        for (uint32_t n_segment = 0;
                      n_segment < n_segments;
                    ++n_segment)
        {
            curve_segment_id segment_id;

            if (curve_container_get_segment_id_for_nth_segment(renderer_ptr->curve,
                                                               n_segment,
                                                              &segment_id) )
            {
                bool                 has_retrieved_segment_data = true;
                system_timeline_time segment_start_time;
                uint32_t             segment_start_time_msec;
                system_timeline_time segment_end_time;
                uint32_t             segment_end_time_msec;
                curve_segment_type   segment_type;

                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                                                                  &segment_start_time);
                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                                                                  &segment_end_time);
                has_retrieved_segment_data &= curve_container_get_segment_property(renderer_ptr->curve,
                                                                                   segment_id,
                                                                                   CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE,
                                                                                  &segment_type);

                if (has_retrieved_segment_data)
                {
                    system_time_get_msec_for_timeline_time(segment_start_time, &segment_start_time_msec);
                    system_time_get_msec_for_timeline_time(segment_end_time,   &segment_end_time_msec);

                    LONG segment_start_x  = (LONG) LERP_FROM_A_B_TO_C_D(float(segment_start_time_msec) / 1000.0f,
                                                                        renderer_ptr->x1,
                                                                        (renderer_ptr->x1 + renderer_ptr->x_width),
                                                                        window_rect.left,
                                                                        window_rect.right);
                    LONG segment_end_x    = (LONG) LERP_FROM_A_B_TO_C_D(float(segment_end_time_msec) / 1000.0f,
                                                                        renderer_ptr->x1,
                                                                        (renderer_ptr->x1 + renderer_ptr->x_width),
                                                                        window_rect.left,
                                                                        window_rect.right);

                    bool can_left_resize  = (x >= (segment_start_x - SEGMENT_RESIZE_MARGIN / 2) &&
                                             x <= (segment_start_x + SEGMENT_RESIZE_MARGIN / 2));
                    bool can_right_resize = (x >= (segment_end_x   - SEGMENT_RESIZE_MARGIN / 2) &&
                                             x <= (segment_end_x   + SEGMENT_RESIZE_MARGIN / 2));

                    if (can_left_resize || can_right_resize)
                    {
                        can_resize_segment                             = true;
                        renderer_ptr->segmentresize_click_x            = x;
                        renderer_ptr->segmentresize_segment_id         = segment_id;
                        renderer_ptr->segmentresize_left_border_resize = can_left_resize;
                        renderer_ptr->segmentresize_start_time         = segment_start_time;
                        renderer_ptr->segmentresize_end_time           = segment_end_time;

                        break;
                    }
                }
            }
        }

        /* Get value at X pointed by the cursor */
        float                x_ss    = LERP_FROM_A_B_TO_C_D                  (x,
                                                                              window_rect.left,
                                                                              window_rect.right,
                                                                              renderer_ptr->x1,
                                                                              (renderer_ptr->x1 + renderer_ptr->x_width) );
        system_timeline_time x_time  = system_time_get_timeline_time_for_msec(uint32_t(x_ss * 1000.0f) );

        if (x_time < 0)
        {
            x_time = 0;
        }

        curve_container_get_value(renderer_ptr->curve,
                                  x_time,
                                  true,
                                  renderer_ptr->float_variant);
        system_variant_get_float (renderer_ptr->float_variant,
                                 &renderer_ptr->mouse_y_value);

        if (renderer_ptr->nodemove_segment_id != 0)
        {
            /* Set the movement arrow */
            system_window_set_cursor(renderer_ptr->window,
                                     SYSTEM_WINDOW_MOUSE_CURSOR_MOVE);
        }
        else
        if (can_resize_segment)
        {
            /* Set the resize arrow */
            system_window_set_cursor(renderer_ptr->window,
                                     SYSTEM_WINDOW_MOUSE_CURSOR_HORIZONTAL_RESIZE);
        }
        else
        {
            /* Set the default cursor arrow */
            system_window_set_cursor(renderer_ptr->window,
                                     SYSTEM_WINDOW_MOUSE_CURSOR_ARROW);

            renderer_ptr->segmentresize_segment_id = 0;
        }
    }

    /* Make sure to highlight any segment we happen to be hovering over */
    ogl_rendering_handler_play(renderer_ptr->rendering_handler, 0);

    return true;
}

/** TODO */
PRIVATE bool _curve_editor_curve_window_renderer_on_mouse_wheel(system_window           window,
                                                                unsigned short          x,
                                                                unsigned short          y,
                                                                short                   delta,
                                                                system_window_vk_status key_status,
                                                                void*                   renderer)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;
    int                                  n            = delta / WHEEL_DELTA;

    if (key_status & SYSTEM_WINDOW_VK_STATUS_CONTROL_PRESSED)
    {
        if (n > 0)
        {
            float new_height = renderer_ptr->y_height * 0.5f;
            float new_y1     = renderer_ptr->y1 + renderer_ptr->y_height * 0.25f;

            if (new_height >= MIN_SCREEN_HEIGHT)
            {
                renderer_ptr->y1       = new_y1;
                renderer_ptr->y_height = new_height;
            }
        }
        else
        if (n < 0)
        {
            float new_height = renderer_ptr->y_height / 0.5f;
            float new_y1     = renderer_ptr->y1 - renderer_ptr->y_height * 0.5f;

            if (new_height <= renderer_ptr->max_screen_height)
            {
                renderer_ptr->y1       = new_y1;
                renderer_ptr->y_height = new_height;
            }
        }
    }
    else
    {
        if (n > 0)
        {
            float new_width = renderer_ptr->x_width * 0.5f;
            float new_x1    = renderer_ptr->x1 + renderer_ptr->x_width * 0.25f;

            if (new_width >= MIN_SCREEN_WIDTH)
            {
                renderer_ptr->x1      = new_x1;
                renderer_ptr->x_width = new_width;
            }
        }
        else
        if (n < 0)
        {
            float new_width = renderer_ptr->x_width / 0.5f;
            float new_x1    = renderer_ptr->x1 - renderer_ptr->x_width * 0.5f;

            if (new_width <= renderer_ptr->max_screen_width)
            {
                renderer_ptr->x1      = new_x1;
                renderer_ptr->x_width = new_width;
            }
        }
    }

    ogl_rendering_handler_play(renderer_ptr->rendering_handler, 0);

    return true;
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_rendering_callback_handler(ogl_context          context,
                                                                            uint32_t,
                                                                            system_timeline_time,
                                                                            void*                descriptor)
{
    _curve_editor_curve_window_renderer* descriptor_ptr      = (_curve_editor_curve_window_renderer*) descriptor;
    const ogl_context_gl_entrypoints*    entry_points        = NULL;
    int                                  n_text_strings_used = 0;
    int                                  window_size[2]      = {0};

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    /* If this is the first draw call ever, initialize uniform & uniform block data */
    if (descriptor_ptr->static_color_program_ub_fs == NULL)
    {
        /* Retrieve properties of all the uniforms & uniform blocks */
        ogl_program_get_uniform_block_by_name(_globals->static_color_program,
                                              system_hashed_ansi_string_create("dataFS"),
                                             &descriptor_ptr->static_color_program_ub_fs);
        ogl_program_get_uniform_block_by_name(_globals->static_color_program,
                                              system_hashed_ansi_string_create("dataVS"),
                                             &descriptor_ptr->static_color_program_ub_vs);

        ASSERT_DEBUG_SYNC(descriptor_ptr->static_color_program_ub_fs != NULL &&
                          descriptor_ptr->static_color_program_ub_vs != NULL,
                          "One of the required uniform blocks is considered inactive.");

        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &descriptor_ptr->static_color_program_ub_fs_bo_size);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                   &descriptor_ptr->static_color_program_ub_fs_bo_id);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                   &descriptor_ptr->static_color_program_ub_fs_bo_start_offset);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_fs,
                                    OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                                   &descriptor_ptr->static_color_program_ub_fs_bp);

        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                                   &descriptor_ptr->static_color_program_ub_vs_bo_size);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BO_ID,
                                   &descriptor_ptr->static_color_program_ub_vs_bo_id);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                                   &descriptor_ptr->static_color_program_ub_vs_bo_start_offset);
        ogl_program_ub_get_property(descriptor_ptr->static_color_program_ub_vs,
                                    OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                                   &descriptor_ptr->static_color_program_ub_vs_bp);

        /* Retrieve static color program's attribute & uniform locations */
        const ogl_program_variable* in_color_uniform_descriptor = NULL;
        const ogl_program_variable* a_uniform_descriptor        = NULL;
        const ogl_program_variable* b_uniform_descriptor        = NULL;
        const ogl_program_variable* mvp_uniform_descriptor      = NULL;

        ogl_program_get_uniform_by_name(_globals->static_color_program,
                                        system_hashed_ansi_string_create("in_color"),
                                       &in_color_uniform_descriptor);
        ogl_program_get_uniform_by_name(_globals->static_color_program,
                                        system_hashed_ansi_string_create("a"),
                                       &a_uniform_descriptor);
        ogl_program_get_uniform_by_name(_globals->static_color_program,
                                        system_hashed_ansi_string_create("b"),
                                       &b_uniform_descriptor);
        ogl_program_get_uniform_by_name(_globals->static_color_program,
                                        system_hashed_ansi_string_create("mvp"),
                                       &mvp_uniform_descriptor);

        descriptor_ptr->static_color_program_color_ub_offset = in_color_uniform_descriptor->block_offset;
        descriptor_ptr->static_color_program_a_ub_offset     = a_uniform_descriptor->block_offset;
        descriptor_ptr->static_color_program_b_ub_offset     = b_uniform_descriptor->block_offset;
        descriptor_ptr->static_color_program_mvp_ub_offset   = mvp_uniform_descriptor->block_offset;
    }

    /* If globals are not initialized DO NOT continue */
    if (!_is_globals_initialized)
    {
        return;
    }

    /* Enable line smoothing */
    GLuint vao_id = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_VAO_NO_VAAS,
                            &vao_id);

    entry_points->pGLEnable         (GL_LINE_SMOOTH);
    entry_points->pGLBindVertexArray(vao_id);
    {
        /* Retrieve window dimensions */
        system_window_get_property(descriptor_ptr->window,
                                   SYSTEM_WINDOW_PROPERTY_DIMENSIONS,
                                   window_size);

        /* Draw background */
        entry_points->pGLUseProgram(ogl_program_get_id(_globals->bg_program) );
        entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                    0,
                                    4);

        /* Set up uniform buffers */
        entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         descriptor_ptr->static_color_program_ub_fs_bp,
                                         descriptor_ptr->static_color_program_ub_fs_bo_id,
                                         descriptor_ptr->static_color_program_ub_fs_bo_start_offset,
                                         descriptor_ptr->static_color_program_ub_fs_bo_size);
        entry_points->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                         descriptor_ptr->static_color_program_ub_vs_bp,
                                         descriptor_ptr->static_color_program_ub_vs_bo_id,
                                         descriptor_ptr->static_color_program_ub_vs_bo_start_offset,
                                         descriptor_ptr->static_color_program_ub_vs_bo_size);

        /* Draw axes */
        float axis_x    = LERP_FROM_A_B_TO_C_D(0,
                                               descriptor_ptr->x1,
                                               (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                              -1.0f,
                                               1.0f);
        float axis_x_px = LERP_FROM_A_B_TO_C_D(axis_x,
                                              -1,
                                               1,
                                               0,
                                               window_size[0]);
        float axis_y    = LERP_FROM_A_B_TO_C_D(0,
                                               descriptor_ptr->y1,
                                               (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                              -1.0f,
                                               1.0f);
        float axis_y_px = LERP_FROM_A_B_TO_C_D(axis_y,
                                              -1,
                                               1,
                                               0,
                                               window_size[1]);

        const float horizontal_a[4]     = {-1,       axis_y, 0, 1};
        const float horizontal_b[4]     = { 1,       axis_y, 0, 1};
        const float vertical_a  [4]     = { axis_x, -1,      0, 1};
        const float vertical_b  [4]     = { axis_x,  1,      0, 1};
        const float identity_matrix[16] = {1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
        const float color[4]            = COLOR_AXES;

        entry_points->pGLBlendFunc(GL_SRC_ALPHA,
                                   GL_DST_ALPHA);
        entry_points->pGLEnable   (GL_BLEND);
        {
            GLuint program_id = ogl_program_get_id(_globals->static_color_program);

            entry_points->pGLLineWidth (WIDTH_AXES);
            entry_points->pGLUseProgram(program_id);

            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_a_ub_offset,
                                                        horizontal_a,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);
            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_b_ub_offset,
                                                        horizontal_b,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);
            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_mvp_ub_offset,
                                                        identity_matrix,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 16);
            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_fs,
                                                        descriptor_ptr->static_color_program_color_ub_offset,
                                                        color,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            ogl_program_ub_sync(descriptor_ptr->static_color_program_ub_fs);
            ogl_program_ub_sync(descriptor_ptr->static_color_program_ub_vs);

            entry_points->pGLDrawArrays(GL_LINES,
                                        0,
                                        2);

            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_a_ub_offset,
                                                        vertical_a,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);
            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_b_ub_offset,
                                                        vertical_b,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            ogl_program_ub_sync(descriptor_ptr->static_color_program_ub_vs);

            entry_points->pGLDrawArrays(GL_LINES,
                                        0,
                                        2);
        }
        entry_points->pGLDisable(GL_BLEND);

        /* Draw separators. The way we handle them is that we start from 0 and render a separator every __predefined__ amount of pixels, 
         * no matter what current horizontal scale is.
         */
        float horizontal_separator_width = 0;

        /* Calculate separator width */
        horizontal_separator_width = LERP_FROM_A_B_TO_C_D(WIDTH_SEPARATOR_PX,
                                                          0,
                                                          window_size[0],
                                                          0,
                                                          descriptor_ptr->x_width);

        /* Draw horizontal separators */
        int   origin_x_px           = (int) LERP_FROM_A_B_TO_C_D(0,
                                                                 descriptor_ptr->x1,
                                                                 (descriptor_ptr->x1 + descriptor_ptr->x_width),
                                                                 0,
                                                                 window_size[0]);
        int   origin_y_px           = (int) LERP_FROM_A_B_TO_C_D(0,
                                                                 descriptor_ptr->y1,
                                                                 (descriptor_ptr->y1 + descriptor_ptr->y_height),
                                                                 0,
                                                                 window_size[1]);

        int   n_separators_per_view = int(float(window_size[0]) / float(WIDTH_SEPARATOR_PX) );
        int   separator_y_px_1      = (int) axis_y_px - int(LENGTH_SEPARATORS_PX >> 1);
        int   separator_y_px_2      = (int) axis_y_px + int(LENGTH_SEPARATORS_PX >> 1);
        int   separator_x_px        = n_separators_per_view * WIDTH_SEPARATOR_PX + origin_x_px % WIDTH_SEPARATOR_PX;

        float separator_y1_ss       = LERP_FROM_A_B_TO_C_D(separator_y_px_1,
                                                           0,
                                                           window_size[1],
                                                          -1,
                                                          1);
        float separator_y2_ss       = LERP_FROM_A_B_TO_C_D(separator_y_px_2,
                                                           0,
                                                           window_size[1],
                                                          -1,
                                                          1);

        entry_points->pGLLineWidth(WIDTH_SEPARATORS);

        for (int n_separator = 0;
                 n_separator <= n_separators_per_view;
               ++n_separator)
        {
            if (separator_x_px >= 0 && separator_x_px <= window_size[0])
            {
                const GLuint program_id = ogl_program_get_id(_globals->static_color_program);

                entry_points->pGLUseProgram(program_id);

                float separator_x_ss  = LERP_FROM_A_B_TO_C_D(separator_x_px,
                                                             0,
                                                             window_size[0] - WIDTH_SEPARATORS,
                                                            -1,
                                                             1);
                float a[4]            = {separator_x_ss, separator_y1_ss, 0, 1};
                float b[4]            = {separator_x_ss, separator_y2_ss, 0, 1};

                ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                            descriptor_ptr->static_color_program_a_ub_offset,
                                                            a,
                                                            0, /* src_data_flags */
                                                            sizeof(float) * 4);
                ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                            descriptor_ptr->static_color_program_b_ub_offset,
                                                            b,
                                                            0, /* src_data_flags */
                                                            sizeof(float) * 4);

                ogl_program_ub_sync(descriptor_ptr->static_color_program_ub_vs);

                entry_points->pGLDrawArrays(GL_LINES,
                                            0,
                                            2);

                /* Update text to be drawn */
                char  buffer[32];
                float value = LERP_FROM_A_B_TO_C_D(separator_x_px,
                                                   0,
                                                   window_size[0],
                                                   descriptor_ptr->x1,
                                                   (descriptor_ptr->x1 + descriptor_ptr->x_width) );

                sprintf_s(buffer,
                          32,
                          "%8.2f",
                          value);

                ogl_text_set(descriptor_ptr->text_renderer,
                             n_text_strings_used,
                             buffer);

                /* Draw corresponding value */
                int text_height     = 0;
                int text_width      = 0;

                ogl_text_get_text_string_property(descriptor_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                  n_text_strings_used,
                                                 &text_height);
                ogl_text_get_text_string_property(descriptor_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                  n_text_strings_used,
                                                 &text_width);

                int text_x          = separator_x_px - text_width / 2;
                int text_position[] = {text_x,
                                       window_size[1] - separator_y_px_1 + (LENGTH_SEPARATORS_PX >> 1) + TEXT_SEPARATOR_DISTANCE_PX};

                if (n_text_strings_used < descriptor_ptr->n_text_strings)
                {
                    ogl_text_set_text_string_property(descriptor_ptr->text_renderer,
                                                      n_text_strings_used,
                                                      OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                                      text_position);
                }

                n_text_strings_used++;
            }

            separator_x_px -= WIDTH_SEPARATOR_PX;
        }

        /* And for the vertical ones */
        n_separators_per_view = int(float(window_size[1]) / float (WIDTH_SEPARATOR_PX) );
        
        int   bottom_separator_index = int(window_size[1] / HEIGHT_SEPARATORS_PX);
        int   separator_x_px_1       = (int) axis_x_px - int(LENGTH_SEPARATORS_PX >> 1);
        int   separator_x_px_2       = (int) axis_x_px + int(LENGTH_SEPARATORS_PX >> 1);
        float separator_x1_ss        = LERP_FROM_A_B_TO_C_D(separator_x_px_1,
                                                            0,
                                                            window_size[0],
                                                           -1,
                                                            1);
        float separator_x2_ss        = LERP_FROM_A_B_TO_C_D(separator_x_px_2,
                                                            0,
                                                            window_size[0],
                                                           -1,
                                                            1);

        int bottom_y_px = bottom_separator_index * HEIGHT_SEPARATORS_PX + origin_y_px % HEIGHT_SEPARATORS_PX;

        while (bottom_y_px > -HEIGHT_SEPARATORS_PX)
        {
            const GLuint program_id = ogl_program_get_id(_globals->static_color_program);

            entry_points->pGLUseProgram(program_id);

            float bottom_y_ss  = LERP_FROM_A_B_TO_C_D(bottom_y_px,
                                                      0,
                                                      window_size[1] - WIDTH_SEPARATORS,
                                                     -1,
                                                      1);
            float a[4]         = {separator_x1_ss, bottom_y_ss, 0, 1};
            float b[4]         = {separator_x2_ss, bottom_y_ss, 0, 1};

            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_a_ub_offset,
                                                        a,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);
            ogl_program_ub_set_nonarrayed_uniform_value(descriptor_ptr->static_color_program_ub_vs,
                                                        descriptor_ptr->static_color_program_b_ub_offset,
                                                        b,
                                                        0, /* src_data_flags */
                                                        sizeof(float) * 4);

            ogl_program_ub_sync(descriptor_ptr->static_color_program_ub_vs);

            entry_points->pGLDrawArrays(GL_LINES,
                                        0,
                                        2);

            if (n_text_strings_used < descriptor_ptr->n_text_strings)
            {
                /* Update text to be drawn */
                char  buffer[32];
                float value = LERP_FROM_A_B_TO_C_D(bottom_y_px + HEIGHT_SEPARATORS_PX,
                                                   0,
                                                   window_size[1],
                                                   descriptor_ptr->y1,
                                                   (descriptor_ptr->y1 + descriptor_ptr->y_height) );

                sprintf_s(buffer,
                          32,
                          "%8.2f",
                          value);

                ogl_text_set(descriptor_ptr->text_renderer,
                             n_text_strings_used,
                             buffer);

                /* Draw corresponding value */
                int text_height     = 0;
                int text_width      = 0;

                ogl_text_get_text_string_property(descriptor_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_HEIGHT_PX,
                                                  n_text_strings_used,
                                                 &text_height);
                ogl_text_get_text_string_property(descriptor_ptr->text_renderer,
                                                  OGL_TEXT_STRING_PROPERTY_TEXT_WIDTH_PX,
                                                  n_text_strings_used,
                                                 &text_width);

                int text_x          = separator_x_px_2 - TEXT_SEPARATOR_DISTANCE_PX;
                int text_y          = window_size[1] - bottom_y_px + text_height / 4;
                int text_position[] = {text_x, text_y};

                ogl_text_set_text_string_property(descriptor_ptr->text_renderer,
                                                  n_text_strings_used,
                                                  OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                                  text_position);
            }

            n_text_strings_used++;
            bottom_y_px -= HEIGHT_SEPARATORS_PX;
        }

        /* Draw cursor position value */
        char                      buffer[128];
        int                       text_position[2] = {0};
        float                     x_value          = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_x,
                                                                          0,
                                                                          window_size[0],
                                                                          descriptor_ptr->x1,
                                                                          (descriptor_ptr->x1 + descriptor_ptr->x_width) );
        float                     y_value          = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_y,
                                                                          window_size[1],
                                                                          0,
                                                                          descriptor_ptr->y1,
                                                                          (descriptor_ptr->y1 + descriptor_ptr->y_height));
        system_hashed_ansi_string curve_value;

        if (x_value < 0)
        {
            x_value = 0;
        }

        curve_container_get_value     (descriptor_ptr->curve,
                                       system_time_get_timeline_time_for_msec(uint32_t(x_value * 1000) ),
                                       true,
                                       descriptor_ptr->text_variant);
        system_variant_get_ansi_string(descriptor_ptr->text_variant,
                                       true,
                                      &curve_value);

        sprintf_s(buffer,
                  128,
                  "(%8.4f, %8.4f): %s",
                  x_value,
                  y_value,
                  system_hashed_ansi_string_get_buffer(curve_value) );

        if (n_text_strings_used < descriptor_ptr->n_text_strings)
        {
            ogl_text_set(descriptor_ptr->text_renderer,
                         n_text_strings_used,
                         buffer);

            ogl_text_set_text_string_property(descriptor_ptr->text_renderer,
                                              n_text_strings_used,
                                              OGL_TEXT_STRING_PROPERTY_POSITION_PX,
                                              text_position);
        }

        n_text_strings_used++;

        /* Draw curve */
        _curve_editor_curve_window_renderer_draw_curve(context,
                                                       (curve_editor_curve_window_renderer) descriptor,
                                                       entry_points,
                                                      &n_text_strings_used);

        /* Draw all texts */
        ogl_text_draw(descriptor_ptr->context,
                      descriptor_ptr->text_renderer);

        /* Not all strings might have landed because of insufficient space. If that was the case, allocate missing space
         * so that next frame will draw correctly */
        if (n_text_strings_used > descriptor_ptr->n_text_strings)
        {
            uint32_t n_missing_strings = n_text_strings_used - descriptor_ptr->n_text_strings;

            for (uint32_t n = 0;
                          n < n_missing_strings;
                        ++n)
            {
                ogl_text_add_string(descriptor_ptr->text_renderer);
            } /* for (uint32_t n = 0; n < n_missing_strings; ++n) */

            LOG_FATAL("Added [%d] text strings to text renderer instance",
                      n_missing_strings);

            descriptor_ptr->n_text_strings = n_text_strings_used;
        }

        /* If quad selector mode is active, draw the selector */
        if (descriptor_ptr->quadselector_mode_active)
        {
            /*                      top-left     top-right     bottom-right bottom-left */
            float positions [16] = {0, -1, 0, 1, 0, -1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1};
            float click_x        = LERP_FROM_A_B_TO_C_D(descriptor_ptr->quadselector_start_mouse_x,
                                                        descriptor_ptr->quadselector_window_rect.left,
                                                        descriptor_ptr->quadselector_window_rect.right,
                                                       -1,
                                                        1);
            float current_x      = LERP_FROM_A_B_TO_C_D(descriptor_ptr->quadselector_mouse_x,
                                                        descriptor_ptr->quadselector_window_rect.left,
                                                        descriptor_ptr->quadselector_window_rect.right,
                                                       -1,
                                                        1);
            float x1             = 0;
            float x2             = 0;

            if (click_x < current_x)
            {
                x1 = click_x;
                x2 = current_x;
            }
            else
            {
                x1 = current_x;
                x2 = click_x;
            }

            positions[0]  = x1;
            positions[4]  = x2;
            positions[8]  = x2;
            positions[12] = x1;

            entry_points->pGLBlendFunc(GL_SRC_ALPHA,
                                       GL_ONE_MINUS_SRC_ALPHA);
            entry_points->pGLEnable   (GL_BLEND);
            {
                const float new_alpha = 0.25f;

                curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                               CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_ALPHA_DATA,
                                                              &new_alpha);
                curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                               CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_POSITIONS_DATA,
                                                               positions);
                curve_editor_program_quadselector_use         (context,
                                                               _globals->quadselector_program);

                entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                            0,
                                            4);
            }
            entry_points->pGLDisable(GL_BLEND);
        }
        else
        {
            /* Not in quad selector mode - draw small quad at place where the software-calculated curve value is at */
            #define VALUE_PREVIEW_WIDTH_HALF  (8)
            #define VALUE_PREVIEW_HEIGHT_HALF (8)

            /*                      top-left     top-right     bottom-right bottom-left */
            float positions [16] = {0, -1, 0, 1, 0, -1, 0, 1,  0, 1, 0, 1,  0, 1, 0, 1};
            float mouse_x        = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_x,
                                                        0,
                                                        window_size[0],
                                                       -1,
                                                       1);
            float mouse_y        = LERP_FROM_A_B_TO_C_D(descriptor_ptr->mouse_y_value,
                                                        descriptor_ptr->y1,
                                                        descriptor_ptr->y1 + descriptor_ptr->y_height,
                                                       -1,
                                                        1);
            float x1             = mouse_x - float(VALUE_PREVIEW_WIDTH_HALF)  / window_size[0];
            float x2             = mouse_x + float(VALUE_PREVIEW_WIDTH_HALF)  / window_size[0];
            float y1             = mouse_y - float(VALUE_PREVIEW_HEIGHT_HALF) / window_size[1];
            float y2             = mouse_y + float(VALUE_PREVIEW_HEIGHT_HALF) / window_size[1];

            positions[0]  = x1;
            positions[1]  = y2;
            positions[4]  = x2;
            positions[5]  = y2;
            positions[8]  = x2;
            positions[9]  = y1;
            positions[12] = x1;
            positions[13]  = y1;

            const float new_alpha = 0.75f;

            curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                           CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_ALPHA_DATA,
                                                          &new_alpha);
            curve_editor_program_quadselector_set_property(_globals->quadselector_program,
                                                           CURVE_EDITOR_PROGRAM_QUADSELECTOR_PROPERTY_POSITIONS_DATA,
                                                           positions);
            curve_editor_program_quadselector_use         (context,
                                                           _globals->quadselector_program);

            entry_points->pGLDrawArrays(GL_TRIANGLE_FAN,
                                        0,
                                        4);
        }
    }
    entry_points->pGLBindVertexArray(0);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                      "");
}

/** TODO */
PRIVATE void _curve_editor_curve_window_renderer_update_tcb_ubo(      curve_segment                        segment,
                                                                      GLuint                               ubo_id,
                                                                      bool                                 is_ubo_initialized,
                                                                      uint32_t                             start_index,
                                                                      uint32_t                             n,
                                                                      _curve_editor_curve_window_renderer* renderer_ptr,
                                                                const ogl_context_gl_entrypoints*          entry_points)
{
    float* storage = new (std::nothrow) float[8 * n];

    ASSERT_ALWAYS_SYNC(storage != NULL,
                       "Could not allocate memory for TCB UBO update operation.");
    if (storage != NULL)
    {
        /* Fill RAM representation */
        system_variant variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

        for (uint32_t index = start_index;
                      index < start_index + n;
                      index++)
        {
            float                 node_bias;
            float                 node_continuity;
            curve_segment_node_id node_id = -1;
            float                 node_tension;
            system_timeline_time  node_time;
            uint32_t              node_time_msec;
            float                 node_value;

            curve_segment_get_node_by_index(segment,
                                            index,
                                           &node_id);
            curve_segment_get_node         (segment,
                                            node_id,
                                           &node_time,
                                            variant);
            system_variant_get_float       (variant,
                                           &node_value);

            curve_segment_get_node_property(segment,
                                            node_id,
                                            CURVE_SEGMENT_NODE_PROPERTY_BIAS,
                                            variant);
            system_variant_get_float       (variant,
                                           &node_bias);

            curve_segment_get_node_property(segment,
                                            node_id,
                                            CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY,
                                            variant);
            system_variant_get_float       (variant,
                                           &node_continuity);

            curve_segment_get_node_property(segment,
                                            node_id,
                                            CURVE_SEGMENT_NODE_PROPERTY_TENSION,
                                            variant);
            system_variant_get_float       (variant,
                                           &node_tension);

            system_time_get_msec_for_timeline_time(node_time,
                                                  &node_time_msec);

            storage[8*(index - start_index)    ] = node_bias;
            storage[8*(index - start_index) + 1] = node_continuity;
            storage[8*(index - start_index) + 2] = node_tension;
            storage[8*(index - start_index) + 3] = float(node_time_msec) / 1000.0f;
            storage[8*(index - start_index) + 4] = node_value;

            /* The remaining part goes for padding */
        }

        system_variant_release(variant);

        /* Blit to VRAM */
        if (!is_ubo_initialized)
        {
            ASSERT_DEBUG_SYNC(start_index == 0,
                              "Start index != 0 when reinitializing UBO contents.");

            entry_points->pGLBindBuffer(GL_ARRAY_BUFFER,
                                        ubo_id);
            entry_points->pGLBufferData(GL_ARRAY_BUFFER,
                                        8 * n * sizeof(float),
                                        storage,
                                        GL_DYNAMIC_DRAW);
        }
        else
        {
            entry_points->pGLBindBuffer   (GL_ARRAY_BUFFER,
                                           ubo_id);
            entry_points->pGLBufferSubData(GL_ARRAY_BUFFER,
                                           8 * start_index * sizeof(float),
                                           8 * n * sizeof(float), storage);
        }

        ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                          "Error when updating TCB UBO");

        /* Free storage */
        delete [] storage;
        storage = NULL;
    }
}


/* Please see header for specification */
PUBLIC curve_editor_curve_window_renderer curve_editor_curve_window_renderer_create(__in __notnull system_hashed_ansi_string name,
                                                                                    __in __notnull HWND                      view_window_handle,
                                                                                    __in __notnull ogl_context               context,
                                                                                    __in           curve_container           curve,
                                                                                    __in __notnull curve_editor_curve_window owner)
{
    /* Initialize globals if necessary.
     *
     * Note: Multi-threading not considered in the implementation for sake of simplicity */
    if (_globals != NULL)
    {
        ::InterlockedIncrement(&_globals->ref_counter);
    }

    /* Carry on */
    _curve_editor_curve_window_renderer* result = new (std::nothrow) _curve_editor_curve_window_renderer;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memory while allocating renderer instance.");

    if (result != NULL)
    {
        _curve_editor_curve_window_renderer_init_descriptor(result,
                                                            view_window_handle,
                                                            name,
                                                            curve,
                                                            owner);

        if (!_curve_editor_curve_window_renderer_init(result) )
        {
            ASSERT_DEBUG_SYNC(false, "Could not initialize curve window renderer.");

            delete result;
            result = NULL;
        }
    }

    return (curve_editor_curve_window_renderer) result;
}

/* Please see header for specification */
PUBLIC system_window curve_editor_curve_window_renderer_get_window(__in __notnull curve_editor_curve_window_renderer descriptor)
{
    _curve_editor_curve_window_renderer* descriptor_ptr = (_curve_editor_curve_window_renderer*) descriptor;

    return descriptor_ptr->window;
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_renderer_redraw(__in __notnull curve_editor_curve_window_renderer renderer)
{
    ogl_rendering_handler_play( ((_curve_editor_curve_window_renderer*)renderer)->rendering_handler, 0);
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_renderer_release(__in __post_invalid curve_editor_curve_window_renderer descriptor)
{
    /* Deinitialize globals if necessary. */
    if (_globals != NULL)
    {
        _curve_editor_curve_window_renderer_deinit_globals();
    }

    _curve_editor_curve_window_renderer* descriptor_ptr = (_curve_editor_curve_window_renderer*) descriptor;

    _curve_editor_curve_window_renderer_deinit(descriptor_ptr);
    delete descriptor_ptr;
}

void _curve_editor_curve_window_renderer_on_resize(ogl_context context,
                                                   void*       arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    int*                              x1y1x2y2     = (int*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLViewport(0,
                              0,
                              x1y1x2y2[2] - x1y1x2y2[0],
                              x1y1x2y2[3] - x1y1x2y2[1]);
}

/* Please see header for specification */
PUBLIC void curve_editor_curve_window_renderer_resize(__in __notnull   curve_editor_curve_window_renderer renderer,
                                                      __in __ecount(4) int*                               x1y1x2y2)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;
    uint32_t                             new_height   = x1y1x2y2[3] - x1y1x2y2[1];
    uint32_t                             new_width    = x1y1x2y2[2] - x1y1x2y2[0];

    system_window_set_size        (renderer_ptr->window,
                                   new_width,
                                   new_height);
    ogl_text_set_screen_properties(renderer_ptr->text_renderer,
                                   new_width,
                                   new_height);

    /* After resizing the view, we need to repaint the view. First, update viewport */
    ogl_rendering_handler_request_callback_from_context_thread(renderer_ptr->rendering_handler,
                                                               _curve_editor_curve_window_renderer_on_resize,
                                                               x1y1x2y2);
    ogl_rendering_handler_play                                (renderer_ptr->rendering_handler,
                                                               0);
}

/* Please see header for spec */
PUBLIC void curve_editor_curve_window_renderer_set_property(__in __notnull curve_editor_curve_window_renderer          renderer,
                                                            __in           curve_editor_curve_window_renderer_property property,
                                                            __in __notnull void*                                       data)
{
    _curve_editor_curve_window_renderer* renderer_ptr = (_curve_editor_curve_window_renderer*) renderer;

    switch (property)
    {
        case CURVE_EDITOR_CURVE_WINDOW_RENDERER_PROPERTY_MAX_VISIBLE_TIMELINE_HEIGHT:
        {
            renderer_ptr->max_screen_height = *(float*) data;

            break;
        }

        case CURVE_EDITOR_CURVE_WINDOW_RENDERER_PROPERTY_MAX_VISIBLE_TIMELINE_WIDTH:
        {
            renderer_ptr->max_screen_width = *(float*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized curve_editor_curve_window_renderer property");
        }
    } /* switch (property) */
}
