/**
 *
 * ES 3.1 application template
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

ogl_context   _context             = NULL;
system_window _window              = NULL;
system_event  _window_closed_event = system_event_create(true); /* manual_reset */


/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        void*       unused)
{
    const ogl_context_es_entrypoints* entry_points = NULL;

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                            &entry_points);

    entry_points->pGLClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    entry_points->pGLClear     (GL_COLOR_BUFFER_BIT);
}

void _rendering_lbm_callback_handler(system_window           window,
                                     unsigned short          x,
                                     unsigned short          y,
                                     system_window_vk_status new_status,
                                     void*)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

/** Entry point */
#ifdef _WIN32
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
#else
int main()
#endif
{
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_size    [2]       = {640, 480};
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_pixel_format window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                               8,  /* color_buffer_green_bits */
                                                               8,  /* color_buffer_blue_bits  */
                                                               0,  /* color_buffer_alpha_bits */
                                                               8,  /* depth_buffer_bits       */
                                                               1); /* n_samples               */

    system_window_get_centered_window_position_for_primary_monitor(window_size,
                                                                   window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (OGL_CONTEXT_TYPE_ES,
                                                                            window_x1y1x2y2,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false,
                                                                            false, /* vsync_enabled */
                                                                            true,  /* visible */
                                                                            window_pf);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,                 /* desired_fps */
                                                                            _rendering_handler,
                                                                            NULL);              /* user_arg */

    system_window_get_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(_window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                        (void*) _rendering_lbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (_window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    system_window_close (_window);
    system_event_release(_window_closed_event);

    return 0;
}