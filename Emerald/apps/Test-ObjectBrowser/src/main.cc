/**
 *
 * Object browser test app (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include <stdlib.h>
#include "curve/curve_container.h"
#include "curve_editor/curve_editor_general.h"
#include "demo/demo_app.h"
#include "demo/demo_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

ral_context  _context             = NULL;
system_event _window_closed_event = system_event_create(true); /* manual_reset */

/** Rendering handler */
void _rendering_handler(ogl_context context,
                        uint32_t    n_frames_rendered,
                        system_time frame_time,
                        const int*  rendering_area_px_topdown,
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

PRIVATE void _window_closed_callback_handler(system_window window,
                                             void*         unused)
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
    int window_size[2] = {640, 480};

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
    PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_callback_proc  = _rendering_handler;
    ogl_rendering_handler                   rendering_handler  = NULL;
    demo_window                             window             = NULL;
    demo_window_create_info                 window_create_info;
    const system_hashed_ansi_string         window_name        = system_hashed_ansi_string_create("Object browser test app");

    window_create_info.resolution[0] = window_size[0];
    window_create_info.resolution[1] = window_size[1];

    window = demo_app_create_window(window_name,
                                    window_create_info,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &_context);
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &rendering_handler);

    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_callback_proc);

    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_CHAR,
                                  (void*) _rendering_key_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_LEFT_BUTTON_DOWN,
                                  (void*) _rendering_lbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                  (void*) _rendering_rbm_callback_handler,
                                  NULL);
    demo_window_add_callback_func(window,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                  SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                  (void*) _window_closed_callback_handler,
                                  NULL);

    demo_window_start_rendering(window,
                                0 /* rendering_start_time */);

    system_event_wait_single(_window_closed_event);

    /* Clean up */
    curve_editor_hide();

    demo_app_destroy_window(window_name);
    system_event_release   (_window_closed_event);

    curve_container_release(curve_float);
    curve_container_release(curve_float2);
    curve_container_release(curve_float3);
    curve_container_release(curve_float4);
    curve_container_release(curve_float5);
    curve_container_release(curve_int);

    return 0;
}