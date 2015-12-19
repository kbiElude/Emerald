/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_window.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "demo/demo_types.h"
#include "demo/demo_window.h"
#include "demo/nodes/nodes_postprocessing_video_segment.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "raGL/raGL_framebuffer.h"
#include "ral/ral_context.h"
#include "system/system_hashed_ansi_string.h"

static const float frame_1_color[]    = {0.0f, 1.0f, 0.0f, 1.0f};
static system_time frame_1_end_time   =  0;
static system_time frame_1_start_time =  0;
static const float frame_2_color[]    = {0.0f, 1.0f, 1.0f, 1.0f};
static system_time frame_2_end_time   =  0;
static system_time frame_2_start_time =  0;
static const float frame_3_color[]    = {0.0f, 0.0f, 1.0f, 1.0f};
static system_time frame_3_end_time   =  0;
static system_time frame_3_start_time =  0;


static void _clear_color_buffer_with_color(ral_context context,
                                           uint32_t    frame_index,
                                           system_time frame_time,
                                           const int*  rendering_area_px_topdown,
                                           void*       clear_color_arg)
{
    const float*                clear_color = (const float*) clear_color_arg;
    ogl_context                 context_gl  = ral_context_get_gl_context(context);
    ogl_context_gl_entrypoints* entrypoints = NULL;

    ogl_context_get_property(context_gl,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);

    /* This function should only be called with valid clear color requests */
    if (frame_time >= frame_1_start_time &&
        frame_time <  frame_1_end_time)
    {
        ASSERT_TRUE(memcmp(clear_color,
                           frame_1_color,
                           sizeof(frame_1_color) ) == 0);
    }
    else
    if (frame_time >= frame_2_start_time &&
        frame_time <  frame_2_end_time)
    {
        ASSERT_TRUE(memcmp(clear_color,
                           frame_2_color,
                           sizeof(frame_2_color) ) == 0);
    }
    else
    if (frame_time >= frame_3_start_time &&
        frame_time <  frame_3_end_time)
    {
        ASSERT_TRUE(memcmp(clear_color,
                           frame_3_color,
                           sizeof(frame_3_color) ) == 0);
    }

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

TEST(WindowTest, CreationTest)
{
    demo_window                     window               = NULL;
    const system_hashed_ansi_string window_name          = system_hashed_ansi_string_create("Test window");
    const uint32_t                  window_resolution[2] = { 320, 240 };

    /* Create the window */
    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_resolution);

    demo_window_show       (window);
    demo_app_destroy_window(window_name);
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

TEST(WindowTest, RenderingHandlerTest)
{
    /* Create the window */
    static const PFNOGLRENDERINGHANDLERRENDERINGCALLBACK pfn_rendering_callback_func_ptr = _on_render_frame_callback;
    demo_window                                          window                          = NULL;
    const system_hashed_ansi_string                      window_name                     = system_hashed_ansi_string_create("Test window");
    ogl_rendering_handler                                window_rendering_handler        = NULL;
    const uint32_t                                       window_target_fps               = 0xFFFFFFFF;
    const uint32_t                                       window_resolution[]             = { 320, 240 };

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL,
                                    false /* use_timeline */);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,
                            &window_target_fps);
    demo_window_show(window);

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_HANDLER,
                            &window_rendering_handler);

    ogl_rendering_handler_set_property(window_rendering_handler,
                                       OGL_RENDERING_HANDLER_PROPERTY_RENDERING_CALLBACK,
                                      &pfn_rendering_callback_func_ptr);

    /* Let's render a couple of frames. */
    demo_window_start_rendering(window,
                                0); /* rendering_start_time */

#ifdef _WIN32
    ::Sleep(1000);
#else
    sleep(1);
#endif

    ASSERT_NE(global_n_frames_rendered,
              0);

    /* Stop the playback and see if more frames are added along the way.*/
    ASSERT_TRUE(demo_window_stop_rendering(window) );

    global_n_frames_rendered = 0;

#ifdef _WIN32
    ::Sleep(1000);
#else
    sleep(1);
#endif

    ASSERT_EQ(global_n_frames_rendered,
              0);

    /* Start the playback and let's destroy the window in a blunt and naughty way to see if it is handled correctly */
    demo_window_start_rendering(window,
                                0); /* rendering_start_time */

    ASSERT_TRUE(demo_app_destroy_window    (window_name) );
    ASSERT_EQ  (demo_app_get_window_by_name(window_name),
                (demo_window) NULL);
}

TEST(WindowTest, TimelineTest_ShouldRenderFourDifferentlyColoredScreensWithDifferentARs)
{
    /* Create the window */
    ral_texture_format              context_texture_format    = RAL_TEXTURE_FORMAT_UNKNOWN;
    const float                     frame_1_ar                = 1.0f;
    demo_timeline_segment           frame_1_ps                = NULL;
    demo_timeline_segment_id        frame_1_ps_id             = -1;
    uint32_t                        frame_1_pipeline_stage_id = -1;
    demo_timeline_segment           frame_1_vs                = NULL;
    demo_timeline_segment_id        frame_1_vs_id             = -1;
    const float                     frame_2_ar                = 1.5f;
    uint32_t                        frame_2_pipeline_stage_id = -1;
    demo_timeline_segment           frame_2_ps                = NULL;
    demo_timeline_segment_id        frame_2_ps_id             = -1;
    demo_timeline_segment           frame_2_vs                = NULL;
    demo_timeline_segment_id        frame_2_vs_id             = -1;
    demo_timeline_segment_node_id   frame_2_vs_node_id        = -1;
    const float                     frame_3_ar                =  1.75f;
    uint32_t                        frame_3_pipeline_stage_id = -1;
    demo_timeline_segment           frame_3_ps                = NULL;
    demo_timeline_segment_id        frame_3_ps_id             = -1;
    demo_timeline_segment           frame_3_vs                = NULL;
    demo_timeline_segment_id        frame_3_vs_id             = -1;
    demo_timeline_segment_node_id   frame_3_vs_node_id        = -1;
    bool                            result                    =  false;
    demo_window                     window                    =  NULL;
    ral_context                     window_context            =  NULL;
    const system_hashed_ansi_string window_name               = system_hashed_ansi_string_create("Test window");
    const uint32_t                  window_resolution[]       = { 320, 240 };
    const uint32_t                  window_target_fps         = ~0;
    demo_timeline                   window_timeline           =  NULL;
    ogl_pipeline                    window_timeline_pipeline  =  NULL;

    demo_timeline_segment_node_id frame_1_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_1_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_1_vs_pass_renderer_node_id = -1;
    demo_timeline_segment_node_id frame_2_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_2_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_2_vs_pass_renderer_node_id = -1;
    demo_timeline_segment_node_id frame_3_ps_output_node_id        = -1;
    demo_timeline_segment_node_id frame_3_ps_vs_node_id            = -1;
    demo_timeline_segment_node_id frame_3_vs_pass_renderer_node_id = -1;

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_RESOLUTION,
                             window_resolution);
    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,
                            &window_target_fps);

    ASSERT_TRUE(demo_window_show(window) );

    /* Retrieve the timeline instance. Define three video segments, each clearing the color buffer with a different color. */
    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_RENDERING_CONTEXT,
                            &window_context);
    demo_window_get_property(window,
                            DEMO_WINDOW_PROPERTY_TIMELINE,
                            &window_timeline);

    ASSERT_NE(window_timeline,
              (demo_timeline) NULL);

    ral_context_get_property  (window_context,
                               RAL_CONTEXT_PROPERTY_SYSTEM_FRAMEBUFFER_COLOR_ATTACHMENT_TEXTURE_FORMAT,
                              &context_texture_format);
    demo_timeline_get_property(window_timeline,
                               DEMO_TIMELINE_PROPERTY_RENDERING_PIPELINE,
                              &window_timeline_pipeline);

    ASSERT_NE(window_timeline_pipeline,
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
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(window_timeline,
                                                         system_hashed_ansi_string_create("Color 1 postprocessing segment"),
                                                         frame_1_start_time,
                                                         frame_1_end_time,
                                                         &frame_1_ps_id,
                                                         &frame_1_ps) );
    ASSERT_TRUE(demo_timeline_add_video_segment         (window_timeline,
                                                         system_hashed_ansi_string_create("Color 1 video segment"),
                                                         frame_1_start_time,
                                                         frame_1_end_time,
                                                         frame_1_ar,
                                                         context_texture_format,
                                                        &frame_1_vs_id,
                                                        &frame_1_vs) );

    ASSERT_TRUE(demo_timeline_add_video_segment         (window_timeline,
                                                         system_hashed_ansi_string_create("Color 2 video segment"),
                                                         frame_2_start_time,
                                                         frame_2_end_time,
                                                         frame_2_ar,
                                                         context_texture_format,
                                                        &frame_2_vs_id,
                                                        &frame_2_vs) );
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(window_timeline,
                                                         system_hashed_ansi_string_create("Color 2 postprocessing segment"),
                                                         frame_2_start_time,
                                                         frame_2_end_time,
                                                         &frame_2_ps_id,
                                                         &frame_2_ps) );

    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(window_timeline,
                                                         system_hashed_ansi_string_create("Color 3 postprocessing segment"),
                                                         frame_3_start_time,
                                                         frame_3_end_time,
                                                         &frame_3_ps_id,
                                                         &frame_3_ps) );
    ASSERT_TRUE(demo_timeline_add_video_segment         (window_timeline,
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
    demo_timeline_segment_get_property(frame_2_ps,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_2_ps_output_node_id);
    demo_timeline_segment_get_property(frame_3_ps,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,
                                      &frame_3_ps_output_node_id);

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

    demo_timeline_segment_expose_node_io(frame_1_vs,
                                         false, /* is_input_io */
                                         frame_1_vs_pass_renderer_node_id,
                                         0,      /* node_io_id                */
                                         true,   /* should_expose             */
                                         false); /* should_expose_as_vs_input */
    demo_timeline_segment_expose_node_io(frame_2_vs,
                                         false, /* is_input_io */
                                         frame_2_vs_pass_renderer_node_id,
                                         0,      /* node_io_id                */
                                         true,   /* should_expose             */
                                         false); /* should_expose_as_vs_input */
    demo_timeline_segment_expose_node_io(frame_3_vs,
                                         false, /* is_input_io */
                                         frame_3_vs_pass_renderer_node_id,
                                         0,      /* node_io_id                */
                                         true,   /* should_expose             */
                                         false); /* should_expose_as_vs_input */

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
    frame_1_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(window_timeline_pipeline,
                                                                  1, /* n_passes */
                                                                  frame_1_segment_passes);
    frame_2_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(window_timeline_pipeline,
                                                                  1, /* n_passes */
                                                                  frame_2_segment_passes);
    frame_3_pipeline_stage_id = ogl_pipeline_add_stage_with_steps(window_timeline_pipeline,
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

    /* Let's render a couple of frames. */
    ASSERT_TRUE(demo_window_start_rendering(window,
                                            0) ); /* rendering_start_time */

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
    ASSERT_TRUE(demo_window_stop_rendering(window) );

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );

}
