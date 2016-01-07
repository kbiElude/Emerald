/**
 *
 * ES 3.1 application template
 *
 */
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"

ogl_context   _context             = NULL;
demo_window   _window              = NULL;
system_event  _window_closed_event = system_event_create(true); /* manual_reset */


/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
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

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
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
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ogl_rendering_handler                   rendering_handler  = NULL;
    demo_window                             window             = NULL;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("ES context test app");
    const uint32_t                          window_size[2]     = {320, 240};
    int                                     window_x1y1x2y2[4] = {0};

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_ES,
                                    false /* use_timeline */);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_size);

    demo_window_show(window);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);


    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(_window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);

    /* Carry on */
    demo_window_start_rendering(_window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    return 0;
}