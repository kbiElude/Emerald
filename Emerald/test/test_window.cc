/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "test_window.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_window.h"
#include "gtest/gtest.h"

static void _mouse_move_entrypoint(system_window window, unsigned short x, unsigned short y, system_window_vk_status new_status)
{
    /* Empty on purpose */
}

TEST(WindowTest, CreationTest)
{
    /* Create the window */
    const int     xywh[]            = {0, 0, 320, 240};
    system_window window_handle     = system_window_create_not_fullscreen(xywh, system_hashed_ansi_string_create("test name"), false, 0, true);
    HWND          window_sys_handle = 0;

    ASSERT_TRUE(window_handle != NULL);

    ASSERT_TRUE(system_window_get_handle(window_handle, &window_sys_handle) );
    ASSERT_EQ  (::IsWindow(window_sys_handle),          TRUE);

    /* Set a callback func */
    system_window_set_callback_func(window_handle, SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE, _mouse_move_entrypoint);

    /* Destroy the window */
    ASSERT_TRUE(system_window_close(window_handle) );
    ASSERT_EQ  (::IsWindow(window_sys_handle), FALSE);
}

/* rendering handler tests */
 unsigned int global_n_frames_rendered = 0;

static void _on_render_frame_callback(ogl_context context, uint32_t n_frames_rendered, system_timeline_time frame_time, void*)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS,
                            &entry_points);

    entry_points->pGLClearColor(0.75f, 0, 0, 1.0f);
    entry_points->pGLClear(GL_COLOR_BUFFER_BIT);

    global_n_frames_rendered = n_frames_rendered;
}

TEST(WindowTest, RenderingHandlerTest)
{
    /* Create the window */
    const int     xywh[]            = {0, 0, 320, 240};
    system_window window_handle     = system_window_create_not_fullscreen(xywh, system_hashed_ansi_string_create("test name"), false, 0, true);
    HWND          window_sys_handle = 0;

    ASSERT_TRUE(window_handle != NULL);

    ASSERT_TRUE(system_window_get_handle(window_handle, &window_sys_handle) );
    ASSERT_EQ  (::IsWindow(window_sys_handle),          TRUE);

    /* Create a rendering handler */
    ogl_rendering_handler rendering_handler = ogl_rendering_handler_create_with_max_performance_policy(_on_render_frame_callback, 0);
    ASSERT_TRUE(rendering_handler != NULL);

    /* Bind the handler to the window */
    ASSERT_TRUE(system_window_set_rendering_handler(window_handle, rendering_handler) );

    /* Let's render a couple of frames. */
    ASSERT_TRUE(ogl_rendering_handler_play(rendering_handler, 0) );

    ::Sleep(1000);
    ASSERT_NE(global_n_frames_rendered, 0);

    /* Stop the playback and see if more frames are added along the way.*/
    ASSERT_TRUE(ogl_rendering_handler_stop(rendering_handler) );

    global_n_frames_rendered = 0;

    ::Sleep(1000);
    ASSERT_EQ(global_n_frames_rendered, 0);

    /* Start the playback and let's destroy the window in a blunt and naughty way to see if it is handled correctly */
    ogl_rendering_handler_play(rendering_handler, 0);

    /* Destroy the window in a blunt way */
    ASSERT_TRUE(system_window_close(window_handle) );
    ASSERT_EQ  (::IsWindow(window_sys_handle), FALSE);
}
