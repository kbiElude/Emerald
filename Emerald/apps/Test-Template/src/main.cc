/**
 *
 * Pipeline test app (kbi/elude @2012)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_window.h"

ogl_context   _context             = NULL;
system_window _window              = NULL;
system_event  _window_closed_event = system_event_create(true, false);


/** Rendering handler */
void _rendering_handler(ogl_context context, uint32_t n_frames_rendered, system_timeline_time frame_time, void* renderer)
{
    const ogl_context_gl_entrypoints* entry_points = ogl_context_retrieve_entrypoints(context);
}

void _rendering_lbm_callback_handler(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status, void*)
{
    system_event_set(_window_closed_event);
}

/** Entry point */
int WINAPI WinMain(HINSTANCE instance_handle, HINSTANCE, LPTSTR, int)
{
    bool                  context_result           = false;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_size    [2]       = {640, 480};
    int                   window_x1y1x2y2[4]       = {0};

    /* Carry on */
    system_window_get_centered_window_position_for_primary_monitor(window_size, window_x1y1x2y2);

    _window                  = system_window_create_not_fullscreen         (window_x1y1x2y2, system_hashed_ansi_string_create("Test window"), false, 0, false, false, true);
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"), 30, _rendering_handler, NULL);
    context_result           = system_window_get_context(_window, &_context);

    ASSERT_DEBUG_SYNC(context_result, "Could not retrieve OGL context");

    system_window_set_rendering_handler(_window, window_rendering_handler);
    system_window_add_callback_func    (_window, SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,  _rendering_lbm_callback_handler, NULL);

    /* Carry on */
    ogl_rendering_handler_play(window_rendering_handler, 0);

    system_event_wait_single_infinite(_window_closed_event);

    /* Clean up */
    ogl_rendering_handler_stop(window_rendering_handler);

    system_window_close (_window);
    system_event_release(_window_closed_event);

    return 0;
}