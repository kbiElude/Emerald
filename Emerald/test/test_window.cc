/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_window.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "demo/demo_types.h"
#include "demo/nodes/nodes_postprocessing_video_segment.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

static void _clear_color_buffer_with_color(ogl_context context,
                                           uint32_t    frame_index,
                                           system_time unused,
                                           const int*  rendering_area_px_topdown,
                                           void*       clear_color_arg)
{
    const float*                clear_color = (const float*) clear_color_arg;
    ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    entrypoints->pGLClearColor(clear_color[0],
                               clear_color[1],
                               clear_color[2],
                               clear_color[3]);

    entrypoints->pGLScissor(rendering_area_px_topdown[0],
                            rendering_area_px_topdown[1],
                            rendering_area_px_topdown[2] - rendering_area_px_topdown[0],
                            rendering_area_px_topdown[3] - rendering_area_px_topdown[1]);
    entrypoints->pGLEnable (GL_SCISSOR_TEST);
    {
        entrypoints->pGLClear(GL_COLOR_BUFFER_BIT);
    }
    entrypoints->pGLDisable(GL_SCISSOR_TEST);
}

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
                                      const int*  rendering_area_px_topdown,
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
    HWND window_sys_handle = 0;

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

TEST(WindowTest, TimelineTest_ShouldRenderFourDifferentlyColoredScreensWithDifferentARs)
{
    /* Create the window */
    ogl_context                   context                   = NULL;
    ral_texture_format            context_texture_format    = RAL_TEXTURE_FORMAT_UNKNOWN;
    const float                   frame_1_ar                = 1.0f;
    const float                   frame_1_color[]           = {0.0f, 1.0f, 0.0f, 1.0f};
    system_time                   frame_1_end_time          =  0;
    demo_timeline_segment         frame_1_ps                = NULL;
    demo_timeline_segment_id      frame_1_ps_id             = -1;
    uint32_t                      frame_1_pipeline_stage_id = -1;
    demo_timeline_segment         frame_1_vs                = NULL;
    demo_timeline_segment_id      frame_1_vs_id             = -1;
    system_time                   frame_1_start_time        =  0;
    const float                   frame_2_ar                = 1.5f;
    const float                   frame_2_color[]           = {0.0f, 1.0f, 1.0f, 1.0f};
    system_time                   frame_2_end_time          =  0;
    uint32_t                      frame_2_pipeline_stage_id = -1;
    demo_timeline_segment         frame_2_ps                = NULL;
    demo_timeline_segment_id      frame_2_ps_id             = -1;
    demo_timeline_segment         frame_2_vs                = NULL;
    demo_timeline_segment_id      frame_2_vs_id             = -1;
    demo_timeline_segment_node_id frame_2_vs_node_id        = -1;
    system_time                   frame_2_start_time        =  0;
    const float                   frame_3_ar                =  1.75f;
    const float                   frame_3_color[]           = {0.0f, 0.0f, 1.0f, 1.0f};
    system_time                   frame_3_end_time          =  0;
    uint32_t                      frame_3_pipeline_stage_id = -1;
    demo_timeline_segment         frame_3_ps                = NULL;
    demo_timeline_segment_id      frame_3_ps_id             = -1;
    demo_timeline_segment         frame_3_vs                = NULL;
    demo_timeline_segment_id      frame_3_vs_id             = -1;
    demo_timeline_segment_node_id frame_3_vs_node_id        = -1;
    system_time                   frame_3_start_time        =  0;
    bool                          result                    =  false;
    demo_timeline                 timeline                  =  NULL;
    ogl_pipeline                  timeline_pipeline         =  NULL;
    const int                     xywh[]                    = {0, 0, 320, 240};
    system_pixel_format           window_pf                 = NULL;
    system_window                 window_handle             = NULL;

    demo_timeline_segment_node_id frame_1_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_1_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_1_vs_output_node_id        = -1;
    demo_timeline_segment_node_id frame_1_vs_pass_renderer_node_id = -1;
    demo_timeline_segment_node_id frame_2_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_2_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_2_vs_output_node_id        = -1;
    demo_timeline_segment_node_id frame_2_vs_pass_renderer_node_id = -1;
    demo_timeline_segment_node_id frame_3_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_3_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_3_vs_output_node_id        = -1;
    demo_timeline_segment_node_id frame_3_vs_pass_renderer_node_id = -1;

    window_pf     = system_pixel_format_create         (8,  /* color_buffer_red_bits   */
                                                        8,  /* color_buffer_green_bits */
                                                        8,  /* color_buffer_blue_bits  */
                                                        0,  /* color_buffer_alpha_bits */
                                                        0,  /* depth_buffer_bits       */
                                                        1,  /* n_samples               */
                                                        0); /* stencil_buffer_bits     */
    window_handle = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                        xywh,
                                                        system_hashed_ansi_string_create("Test window"),
                                                        false,
                                                        true,  /* vsync_enabled */
                                                        true,  /* visible */
                                                        window_pf);

    /* Create a rendering handler */
    ogl_rendering_handler rendering_handler = ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string_create("rendering handler"),
                                                                                                       _on_render_frame_callback,
                                                                                                       0);
    ASSERT_TRUE(rendering_handler != NULL);

    /* Bind the handler to the window */
    system_window_set_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    /* Create a timeline instance. Define three video segments, each clearing the color buffer with a different color. */
    system_window_get_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &context);

    timeline = demo_timeline_create(system_hashed_ansi_string_create("Test timeline"),
                                    context,
                                    rendering_handler,
                                    window_handle);

    ASSERT_NE(timeline,
              (demo_timeline) NULL);

    ogl_context_get_property  (context,
                               OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                              &context_texture_format);
    demo_timeline_get_property(timeline,
                               DEMO_TIMELINE_PROPERTY_RENDERING_PIPELINE,
                              &timeline_pipeline);

    ASSERT_NE(timeline_pipeline,
              (ogl_pipeline) NULL);

    frame_1_start_time = system_time_get_time_for_msec(0);
    frame_1_end_time   = system_time_get_time_for_msec(1000);
    frame_2_start_time = frame_1_end_time;
    frame_2_end_time   = frame_2_start_time + frame_1_end_time;
    frame_3_start_time = frame_2_end_time;
    frame_3_end_time   = frame_3_start_time + frame_1_end_time;

    const ogl_pipeline_stage_step_declaration frame_1_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Color 1 pipeline stage step"),
                                             _clear_color_buffer_with_color,
                                             (void*) frame_1_color
        }
    };
    const ogl_pipeline_stage_step_declaration frame_2_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Color 2 pipeline stage step"),
                                             _clear_color_buffer_with_color,
                                             (void*) frame_2_color
        }
    };
    const ogl_pipeline_stage_step_declaration frame_3_segment_passes[] =
    {
        {
            system_hashed_ansi_string_create("Color 3 pipeline stage step"),
                                             _clear_color_buffer_with_color,
                                             (void*) frame_3_color
        }
    };

    /* Create postprocessing & video segments for "frame" time regions */
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Color 1 postprocessing segment"),
                                                         frame_1_start_time,
                                                         frame_1_end_time,
                                                         &frame_1_ps_id,
                                                         &frame_1_ps) );
    ASSERT_TRUE(demo_timeline_add_video_segment         (timeline,
                                                         system_hashed_ansi_string_create("Color 1 video segment"),
                                                         frame_1_start_time,
                                                         frame_1_end_time,
                                                         frame_1_ar,
                                                         context_texture_format,
                                                        &frame_1_vs_id,
                                                        &frame_1_vs) );

    ASSERT_TRUE(demo_timeline_add_video_segment         (timeline,
                                                         system_hashed_ansi_string_create("Color 2 video segment"),
                                                         frame_2_start_time,
                                                         frame_2_end_time,
                                                         frame_2_ar,
                                                         context_texture_format,
                                                        &frame_2_vs_id,
                                                        &frame_2_vs) );
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Color 2 postprocessing segment"),
                                                         frame_2_start_time,
                                                         frame_2_end_time,
                                                         &frame_2_ps_id,
                                                         &frame_2_ps) );

    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Color 3 postprocessing segment"),
                                                         frame_3_start_time,
                                                         frame_3_end_time,
                                                         &frame_3_ps_id,
                                                         &frame_3_ps) );
    ASSERT_TRUE(demo_timeline_add_video_segment         (timeline,
                                                         system_hashed_ansi_string_create("Color 3 video segment"),
                                                         frame_3_start_time,
                                                         frame_3_end_time,
                                                         frame_3_ar,
                                                         context_texture_format,
                                                        &frame_3_vs_id,
                                                        &frame_3_vs) );

    /* Each video segment we created must have been assigned one video segment node in each postprocessing segment.
     * Retrieve video segment node handles for postprocessing segment from the same time region. We'll need to
     * connect these nodes to output nodes in the next step . */
    ASSERT_TRUE(demo_timeline_segment_get_video_segment_node_id_for_ps_and_vs_segments(frame_1_ps,
                                                                                       frame_1_vs,
                                                                                      &frame_1_ps_vs_node_id) );
    ASSERT_TRUE(demo_timeline_segment_get_video_segment_node_id_for_ps_and_vs_segments(frame_2_ps,
                                                                                       frame_2_vs,
                                                                                      &frame_2_ps_vs_node_id) );
    ASSERT_TRUE(demo_timeline_segment_get_video_segment_node_id_for_ps_and_vs_segments(frame_3_ps,
                                                                                       frame_3_vs,
                                                                                      &frame_3_ps_vs_node_id) );

    /* Do the same for output nodes */
    demo_timeline_segment_get_property(frame_1_ps,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_1_ps_output_node_id);
    demo_timeline_segment_get_property(frame_1_vs,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_1_vs_output_node_id);

    demo_timeline_segment_get_property(frame_2_ps,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_2_ps_output_node_id);
    demo_timeline_segment_get_property(frame_2_vs,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_2_vs_output_node_id);

    demo_timeline_segment_get_property(frame_3_ps,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_3_ps_output_node_id);
    demo_timeline_segment_get_property(frame_3_vs,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_3_vs_output_node_id);

    /* Create pass renderer nodes in video segments */
    ASSERT_TRUE(demo_timeline_segment_add_node(frame_1_vs,
                                               DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,
                                               system_hashed_ansi_string_create("Pass renderer"),
                                               &frame_1_vs_pass_renderer_node_id,
                                               NULL) ); /* out_opt_node_ptr */

    ASSERT_TRUE(demo_timeline_segment_add_node(frame_2_vs,
                                               DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,
                                               system_hashed_ansi_string_create("Pass renderer"),
                                               &frame_2_vs_pass_renderer_node_id,
                                               NULL) ); /* out_opt_node_ptr */

    ASSERT_TRUE(demo_timeline_segment_add_node(frame_3_vs,
                                               DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,
                                               system_hashed_ansi_string_create("Pass renderer"),
                                               &frame_3_vs_pass_renderer_node_id,
                                               NULL) ); /* out_opt_node_ptr */

    /* Connect the nodes */
    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_1_vs,
                                                    frame_1_vs_pass_renderer_node_id,
                                                    0, /* output_id */
                                                    frame_1_vs_output_node_id,
                                                    0 /* input_id */) );
    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_2_vs,
                                                    frame_2_vs_pass_renderer_node_id,
                                                    0, /* output_id */
                                                    frame_2_vs_output_node_id,
                                                    0 /* input_id */) );
    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_3_vs,
                                                    frame_3_vs_pass_renderer_node_id,
                                                    0, /* output_id */
                                                    frame_3_vs_output_node_id,
                                                    0 /* input_id */) );

    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_1_ps,
                                                    frame_1_ps_vs_node_id,
                                                    0, /* output_id */
                                                    frame_1_ps_output_node_id,
                                                    0 /* input_id */) );
    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_2_ps,
                                                    frame_2_ps_vs_node_id,
                                                    0, /* output_id */
                                                    frame_2_ps_output_node_id,
                                                    0 /* input_id */) );
    ASSERT_TRUE(demo_timeline_segment_connect_nodes(frame_3_ps,
                                                    frame_3_ps_vs_node_id,
                                                    0, /* output_id */
                                                    frame_3_ps_output_node_id,
                                                    0 /* input_id */) );

    /* Create three pipeline stages. Each stage will be assigned a different rendering handler. */
    frame_1_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(timeline_pipeline,
                                                                  1, /* n_passes */
                                                                  frame_1_segment_passes);
    frame_2_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(timeline_pipeline,
                                                                  1, /* n_passes */
                                                                  frame_2_segment_passes);
    frame_3_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(timeline_pipeline,
                                                                  1, /* n_passes */
                                                                  frame_3_segment_passes);

    demo_timeline_segment_set_node_property(frame_1_vs,
                                            frame_1_vs_pass_renderer_node_id,
                                            NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID,
                                           &frame_1_pipeline_stage_id);
    demo_timeline_segment_set_node_property(frame_2_vs,
                                            frame_2_vs_pass_renderer_node_id,
                                            NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID,
                                           &frame_2_pipeline_stage_id);
    demo_timeline_segment_set_node_property(frame_3_vs,
                                            frame_3_vs_pass_renderer_node_id,
                                            NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID,
                                           &frame_3_pipeline_stage_id);

    /* Assign the timeline to the window. Note that this setter automatically takes ownership of the object,
     * so we need not release it at the end of the test */
    ogl_rendering_handler_set_property(rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_TIMELINE,
                                      &timeline);

    /* Let's render a couple of frames. */
    ASSERT_TRUE(ogl_rendering_handler_play(rendering_handler,
                                           0) );

    #ifdef _WIN32
    {
        ::Sleep(4000);
    }
    #else
    {
        sleep(4);
    }
    #endif

    /* Stop the playback */
    ASSERT_TRUE(ogl_rendering_handler_stop(rendering_handler) );

    /* Destroy the window */
    ASSERT_TRUE(system_window_close(window_handle) );

}
