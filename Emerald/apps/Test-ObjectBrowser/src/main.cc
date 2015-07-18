/**
 *
 * Object browser test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve/curve_container.h"
#include "curve_editor/curve_editor_general.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

ogl_context  _context             = NULL;
system_event _window_closed_event = system_event_create(true); /* manual_reset */

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        void*       renderer)
{
    const  ogl_context_gl_entrypoints* entry_points = NULL;
    static int                         counter      = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if ( (++counter) % 256 == 0)
    {
        counter = 0;
    }

    entry_points->pGLClearColor(float(counter) / 255.0f,
                                float(counter) / 255.0f,
                                float(counter) / 255.0f,
                                1.0f);
    entry_points->pGLClear     (GL_COLOR_BUFFER_BIT);
}

/** Event callback handlers */
bool _rendering_key_callback_handler(system_window  window,
                                     unsigned short character,
                                     void*)
{
    system_event_set(_window_closed_event);

    return true;
}

bool _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);

    return true;
}

bool _rendering_rbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    curve_editor_show(_context);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    int window_size    [2] = {640, 480};
    int window_x1y1x2y2[4] = {0};

    /* Add some test curves */
    curve_container curve_float  = curve_container_create(system_hashed_ansi_string_create("Test1\\zxcv 2d float"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_FLOAT);
    curve_container curve_float3 = curve_container_create(system_hashed_ansi_string_create("Test1\\Another test 2d float"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_FLOAT);
    curve_container curve_float4 = curve_container_create(system_hashed_ansi_string_create("Test1\\aoe 2d float"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_FLOAT);
    curve_container curve_float5 = curve_container_create(system_hashed_ansi_string_create("Test1\\2d float"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_FLOAT);
    curve_container curve_float2 = curve_container_create(system_hashed_ansi_string_create("Test2\\3d float"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_FLOAT);
    curve_container curve_int    = curve_container_create(system_hashed_ansi_string_create("1d int"),
                                                          NULL, /* object_manager_path */
                                                          SYSTEM_VARIANT_INTEGER);

    /* Carry on */
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               16, /* depth_buffer_bits       */
                                                               1,  /* n_samples               */
                                                               0); /* stencil_buffer_bits     */

    system_window_get_centered_window_position_for_primary_monitor(window_size,
                                                                   window_x1y1x2y2);

    system_window         window                   = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_GL,
                                                                                                  window_x1y1x2y2,
                                                                                                  system_hashed_ansi_string_create("Test window"),
                                                                                                  false,
                                                                                                  false, /* vsync_enabled */
                                                                                                  true,  /* visible */
                                                                                                  window_pf);
    ogl_rendering_handler window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                                                  10,
                                                                                                  _rendering_handler,
                                                                                                  NULL);

    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_CHAR,
                                        (void*) _rendering_key_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        (void*) _rendering_lbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);

    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    curve_editor_hide();

    system_window_close (window);
    system_event_release(_window_closed_event);

    curve_container_release(curve_float);
    curve_container_release(curve_float2);
    curve_container_release(curve_float3);
    curve_container_release(curve_float4);
    curve_container_release(curve_float5);
    curve_container_release(curve_int);

    return 0;
}