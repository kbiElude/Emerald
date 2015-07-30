/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_time.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "demo/demo_timeline.h"
#include "ogl/ogl_rendering_handler.h"
#include "system/system_pixel_format.h"
#include "system/system_window.h"

TEST(TimelineTest, FunctionalTest)
{
    ogl_context           context           = NULL;
    system_time           duration          = 0;
    ogl_rendering_handler rendering_handler = NULL;
    uint32_t              segment_a_id      = -1;
    uint32_t              segment_b_id      = -1;
    uint32_t              segment_c_id      = -1;
    const system_time     time_1s           = system_time_get_time_for_s(1);
    const system_time     time_4s           = system_time_get_time_for_s(4);
    const system_time     time_7s           = system_time_get_time_for_s(7);
    const system_time     time_10s          = system_time_get_time_for_s(10);
    const system_time     time_14s          = system_time_get_time_for_s(14);
    demo_timeline         timeline          = NULL;
    const int             xywh[]            = {0, 0, 1, 1};
    system_pixel_format   window_pf         = system_pixel_format_create         (8,  /* color_buffer_red_bits   */
                                                                                  8,  /* color_buffer_green_bits */
                                                                                  8,  /* color_buffer_blue_bits  */
                                                                                  0,  /* color_buffer_alpha_bits */
                                                                                  16, /* depth_buffer_bits       */
                                                                                  1,  /* n_samples               */
                                                                                  0); /* stencil_buffer_bits     */
    system_window         window_handle     = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                                                  xywh,
                                                                                  system_hashed_ansi_string_create("Test window"),
                                                                                  false,
                                                                                  false, /* vsync_enabled */
                                                                                  true,  /* visible */
                                                                                  window_pf);

    rendering_handler = ogl_rendering_handler_create_with_max_performance_policy(system_hashed_ansi_string_create("rendering handler"),
                                                                                 NULL,
                                                                                 0);

    system_window_set_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &rendering_handler);

    /* Create the timeline instance */
    system_window_get_property(window_handle,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &context);

    timeline = demo_timeline_create(system_hashed_ansi_string_create("Test timeline"),
                                    context);

    /* What's the default duration? */
    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                0);

    /* Add a video segment from 0s to 10s */
    ASSERT_TRUE(demo_timeline_add_video_segment(timeline,
                                                system_hashed_ansi_string_create("Segment A"),
                                                0, /* start_time */
                                                time_10s,
                                               &segment_a_id,
                                                NULL) ); /* out_stage_id_ptr */

    /* Make sure the duration is reported correctly at this point */
    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_10s);

    /* Try to add a video segment from 1s to 4s. This should fail, and the duration should be
     * unaffected. */
    ASSERT_FALSE(demo_timeline_add_video_segment(timeline,
                                                 system_hashed_ansi_string_create("Segment B"),
                                                 time_1s,
                                                 time_4s,
                                                &segment_b_id,
                                                 NULL) ); /* out_stage_id_ptr */

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_10s);

    /* What if we move the segment A 4 seconds ahead.. */
    ASSERT_TRUE(demo_timeline_move_segment(timeline,
                                           DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                           segment_a_id,
                                           time_4s) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* ..and retry? */
    ASSERT_TRUE(demo_timeline_add_video_segment(timeline,
                                                system_hashed_ansi_string_create("Segment B"),
                                                time_1s,
                                                time_4s,
                                               &segment_b_id,
                                                NULL) ); /* out_stage_id_ptr */

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* We now have:
     * 01234567890123
     * _BBBAAAAAAAAAA
     *
     * Try to resize B so that it lasts for 7s. Should definitely fail!
     */
    ASSERT_FALSE(demo_timeline_resize_segment(timeline,
                                              DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                              segment_b_id,
                                              time_7s) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Now resize it so that its duration is only 1s. */
    ASSERT_TRUE(demo_timeline_resize_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                             segment_b_id,
                                             time_1s) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Situation at this point is as below:
     *
     * 01234567890123
     * _B__AAAAAAAAAA
     *
     * Try to delete a non-existing segment.
     */
    ASSERT_FALSE(demo_timeline_delete_segment(timeline,
                                              DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                              0xFFFFFFFF) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Delete segment B */
    ASSERT_TRUE(demo_timeline_delete_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                             segment_b_id) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Finally, create a segment C fitting the gap at the beginning */
    ASSERT_TRUE(demo_timeline_add_video_segment(timeline,
                                                system_hashed_ansi_string_create("Segment C"),
                                                0,
                                                time_4s,
                                               &segment_c_id,
                                                NULL) ); /* opt_out_stage_id_ptr */

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Delete segment A and check if the reported timeline duration is still valid */
    ASSERT_TRUE(demo_timeline_delete_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                             segment_a_id) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_4s);

    /* Destroy the window */
    ASSERT_TRUE(system_window_close(window_handle) );
}

