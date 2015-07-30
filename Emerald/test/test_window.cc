/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_window.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

static void _mouse_move_entrypoint(system_window           window,
                                   unsigned short          x,
                                   unsigned short          y,
                                   system_window_vk_status new_status)
{
    /* Empty on purpose */
}

TEST(WindowTest, CreationTest)
{
    /* Create the window */
    const int           xywh[]        = {0, 0, 320, 240};
    system_pixel_format window_pf     = system_pixel_format_create         (8,  /* color_buffer_red_bits   */
                                                                            8,  /* color_buffer_green_bits */
                                                                            8,  /* color_buffer_blue_bits  */
                                                                            0,  /* color_buffer_alpha_bits */
                                                                            0,  /* depth_buffer_bits       */
                                                                            1,  /* n_samples               */
                                                                            0); /* stencil_buffer_bits     */
    system_window       window_handle = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                                            xywh,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false,
                                                                            true,  /* vsync_enabled */
                                                                            true,  /* visible */
                                                                            window_pf);

#ifdef _WIN32
    HWND          window_sys_handle = 0;

    ASSERT_TRUE(window_handle != NULL);

    system_window_get_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_sys_handle);

    ASSERT_EQ(::IsWindow(window_sys_handle),
                         TRUE);
#endif

    /* Set a callback func */
    system_window_add_callback_func(window_handle,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                    SYSTEM_WINDOW_CALLBACK_FUNC_MOUSE_MOVE,
                                    (void*) _mouse_move_entrypoint,
                                    NULL); /* callback_func_user_arg */

    /* Destroy the window */
    ASSERT_TRUE(system_window_close(window_handle) );

#ifdef _WIN32
    ASSERT_EQ  (::IsWindow          (window_sys_handle),
                FALSE);
#endif
}

/* rendering handler tests */
 unsigned int global_n_frames_rendered = 0;

static void _on_render_frame_callback(ogl_context context,
                                      uint32_t    n_frames_rendered,
                                      system_time frame_time,
                                      void*)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    entry_points->pGLClearColor(0.75f, /* red */
                                1.0f,  /* green */
                                0,     /* blue */
                                1.0f); /* alpha */
    entry_points->pGLClear     (GL_COLOR_BUFFER_BIT);

    global_n_frames_rendered = n_frames_rendered;
}

TEST(WindowTest, MSAAEnumerationTest)
{
    unsigned int*       msaa_samples_ptr = NULL;
    unsigned int        n_msaa_samples   = 0;
    system_pixel_format window_pf        = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                                                      8,  /* color_buffer_green_bits */
                                                                      8,  /* color_buffer_blue_bits  */
                                                                      0,  /* color_buffer_alpha_bits */
                                                                      16,  /* depth_buffer_bits       */
                                                                      16, /* n_samples               */
                                                                      0); /* stencil_buffer_bits     */

    ogl_context_enumerate_msaa_samples(window_pf,
                                      &n_msaa_samples,
                                      &msaa_samples_ptr);

    system_pixel_format_release(window_pf);

    delete[] msaa_samples_ptr;
    msaa_samples_ptr = NULL;
}

TEST(WindowTest, RenderingHandlerTest)
{
    /* Create the window */
    const int           xywh[]        = {0, 0, 320, 240};
    system_pixel_format window_pf     = system_pixel_format_create         (8,  /* color_buffer_red_bits   */
                                                                            8,  /* color_buffer_green_bits */
                                                                            8,  /* color_buffer_blue_bits  */
                                                                            0,  /* color_buffer_alpha_bits */
                                                                            16, /* depth_buffer_bits       */
                                                                            1,  /* n_samples               */
                                                                            0); /* stencil_buffer_bits     */
    system_window       window_handle = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                                            xywh,
                                                                            system_hashed_ansi_string_create("Test window"),
                                                                            false,
                                                                            true,  /* vsync_enabled */
                                                                            true,  /* visible */
                                                                            window_pf);
#ifdef _WIN32
    HWND          window_sys_handle = 0;

    ASSERT_TRUE(window_handle != NULL);

    system_window_get_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_HANDLE,
                              &window_sys_handle);

    ASSERT_EQ(::IsWindow(window_sys_handle),
              TRUE);
#endif

    /* Create a rendering handler */
    ogl_rendering_handler rendering_handler = ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string_create("rendering handler"),
                                                                                                       _on_render_frame_callback,
                                                                                                       0);
    ASSERT_TRUE(rendering_handler != NULL);

    /* Bind the handler to the window */
    system_window_set_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    /* Let's render a couple of frames. */
    ASSERT_TRUE(ogl_rendering_handler_play(rendering_handler,
                                           0) );

#ifdef _WIN32
    ::Sleep(1000);
#else
    sleep(1);
#endif

    ASSERT_NE(global_n_frames_rendered,
              0);

    /* Stop the playback and see if more frames are added along the way.*/
    ASSERT_TRUE(ogl_rendering_handler_stop(rendering_handler) );

    global_n_frames_rendered = 0;

#ifdef _WIN32
    ::Sleep(1000);
#else
    sleep(1);
#endif

    ASSERT_EQ(global_n_frames_rendered,
              0);

    /* Start the playback and let's destroy the window in a blunt and naughty way to see if it is handled correctly */
    ogl_rendering_handler_play(rendering_handler,
                               0);

    /* Destroy the window in a blunt way */
    ASSERT_TRUE(system_window_close(window_handle) );

#ifdef _WIN32
    ASSERT_EQ  (::IsWindow         (window_sys_handle),
                FALSE);
#endif
}
