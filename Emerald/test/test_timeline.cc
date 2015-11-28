/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_time.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "demo/demo_app.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "demo/demo_window.h"
#include "system/system_time.h"


TEST(TimelineTest, FunctionalTest)
{
    system_time               duration          = 0;
    demo_timeline_segment_id  segment_a_id      = -1;
    demo_timeline_segment_id  segment_b_id      = -1;
    demo_timeline_segment_id  segment_c_id      = -1;
    const system_time         time_1s           = system_time_get_time_for_s(1);
    const system_time         time_4s           = system_time_get_time_for_s(4);
    const system_time         time_7s           = system_time_get_time_for_s(7);
    const system_time         time_10s          = system_time_get_time_for_s(10);
    const system_time         time_14s          = system_time_get_time_for_s(14);
    demo_timeline             timeline          = NULL;
    demo_window               window            = NULL;
    uint32_t                  window_target_fps = ~0;
    system_hashed_ansi_string window_name       = system_hashed_ansi_string_create("Test window");

    window = demo_app_create_window(window_name,
                                    RAL_BACKEND_TYPE_GL);

    ASSERT_NE(window,
              (demo_window) NULL);

    demo_window_set_property(window,
                             DEMO_WINDOW_PROPERTY_TARGET_FRAME_RATE,
                            &window_target_fps);

    ASSERT_TRUE(demo_window_show(window) );

    demo_window_get_property(window,
                             DEMO_WINDOW_PROPERTY_TIMELINE,
                            &timeline);

    /* What's the default duration? */
    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                0);

    /* Add a postprocessing segment from 0s to 10s */
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Segment A"),
                                                         0, /* start_time */
                                                         time_10s,
                                                        &segment_a_id,
                                                         NULL) ); /* out_opt_video_segment_ptr */

    /* Make sure the duration is reported correctly at this point */
    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_10s);

    /* Try to add a postprocessing segment from 1s to 4s. This should fail, and the duration should be
     * unaffected. */
    ASSERT_FALSE(demo_timeline_add_postprocessing_segment(timeline,
                                                          system_hashed_ansi_string_create("Segment B"),
                                                          time_1s,
                                                          time_4s,
                                                         &segment_b_id,
                                                          NULL) ); /* out_opt_video_segment_ptr */

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_10s);

    /* What if we move the segment A 4 seconds ahead.. */
    ASSERT_TRUE(demo_timeline_move_segment(timeline,
                                           DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                           segment_a_id,
                                           time_4s) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* ..and retry? */
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Segment B"),
                                                         time_1s,
                                                         time_4s,
                                                        &segment_b_id,
                                                         NULL) ); /* out_opt_video_segment_ptr */

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
                                              DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                              segment_b_id,
                                              time_7s) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Now resize it so that its duration is only 1s. */
    ASSERT_TRUE(demo_timeline_resize_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
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
                                              DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                              0xFFFFFFFF) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Delete segment B */
    ASSERT_TRUE(demo_timeline_delete_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                             segment_b_id) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Finally, create a segment C fitting the gap at the beginning */
    ASSERT_TRUE(demo_timeline_add_postprocessing_segment(timeline,
                                                         system_hashed_ansi_string_create("Segment C"),
                                                         0,
                                                         time_4s,
                                                        &segment_c_id,
                                                         NULL) ); /* out_opt_video_segment_ptr */

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_14s);

    /* Delete segment A and check if the reported timeline duration is still valid */
    ASSERT_TRUE(demo_timeline_delete_segment(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                             segment_a_id) );

    ASSERT_TRUE(demo_timeline_get_property(timeline,
                                           DEMO_TIMELINE_PROPERTY_DURATION,
                                          &duration) );
    ASSERT_EQ  (duration,
                time_4s);

    /* Destroy the window */
    ASSERT_TRUE(demo_app_destroy_window(window_name) );
}

