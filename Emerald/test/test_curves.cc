/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_curves.h"
#include "shared.h"
#include "curve/curve_container.h"
#include "gtest/gtest.h"
#include "system/system_time.h"
#include "system/system_variant.h"

TEST(CurvesTest, DefaultValue)
{
    float           result_float               = 0.0f;
    system_variant  result_variant             = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_container test_curve                 = curve_container_create(system_hashed_ansi_string_create("test curve"),
                                                                        NULL, /* object_manager_path */
                                                                        SYSTEM_VARIANT_FLOAT);
    system_variant  test_default_value_variant = system_variant_create (SYSTEM_VARIANT_FLOAT);

    system_variant_set_float(test_default_value_variant,
                             666.0f);

    ASSERT_TRUE(curve_container_set_default_value(test_curve,
                                                  test_default_value_variant) );
    ASSERT_TRUE(curve_container_get_value        (test_curve,
                                                  0,              /* time */
                                                  false,          /* should_force */
                                                  result_variant) );

    system_variant_get_float(result_variant,
                            &result_float);

    ASSERT_EQ(result_float,
              666.0f);

    /* Clean up */
    system_variant_release (test_default_value_variant);
    system_variant_release (result_variant);
    curve_container_release(test_curve);

}

TEST(CurvesTest, StaticSegment)
{
    float            result_float                 = 0.0f;
    system_variant   result_variant               = system_variant_create (SYSTEM_VARIANT_FLOAT);
    system_variant   static_segment_value_variant = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_container  test_curve                   = curve_container_create(system_hashed_ansi_string_create("test curve"),
                                                                           NULL, /* object_manager_path */
                                                                           SYSTEM_VARIANT_FLOAT);
    system_variant   test_default_value_variant   = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_segment_id static_segment_id            = 0;

    system_variant_set_float(test_default_value_variant,
                             666.0f);
    system_variant_set_float(static_segment_value_variant,
                             222.0f);

    ASSERT_TRUE(curve_container_set_default_value       (test_curve,
                                                         test_default_value_variant) );
    ASSERT_TRUE(curve_container_add_static_value_segment(test_curve,
                                                         0, /* time */
                                                         system_time_get_timeline_time_for_s(2),
                                                         static_segment_value_variant,
                                                        &static_segment_id) );

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          0,                 /* time */
                                          false,             /* should_force */
                                          result_variant) );

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             222.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(1),
                                          false, /* should_force */
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             222.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(2),
                                          false, /* should_force */
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             222.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(3),
                                          false, /* should_force */
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             666.0f);

    /* Clean up */
    system_variant_release (test_default_value_variant);
    system_variant_release (static_segment_value_variant);
    system_variant_release (result_variant);
    curve_container_release(test_curve);
}

TEST(CurvesTest, LerpSegment)
{
    float            result_float                    = 0.0f;
    system_variant   result_variant                  = system_variant_create (SYSTEM_VARIANT_FLOAT);
    system_variant   static_lerp_start_value_variant = system_variant_create (SYSTEM_VARIANT_FLOAT);
    system_variant   static_lerp_end_value_variant   = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_container  test_curve                      = curve_container_create(system_hashed_ansi_string_create("test curve"),
                                                                              NULL, /* object_manager_path */
                                                                              SYSTEM_VARIANT_FLOAT);
    system_variant   test_default_value_variant      = system_variant_create (SYSTEM_VARIANT_FLOAT);
    curve_segment_id static_segment_id               = 0;

    system_variant_set_float(test_default_value_variant,
                             666.0f);
    system_variant_set_float(static_lerp_start_value_variant,
                             222.0f);
    system_variant_set_float(static_lerp_end_value_variant,
                             444.0f);

    ASSERT_TRUE(curve_container_set_default_value(test_curve,
                                                  test_default_value_variant) );
    ASSERT_TRUE(curve_container_add_lerp_segment (test_curve,
                                                  0, /* start_time */
                                                  system_time_get_timeline_time_for_s(2),
                                                  static_lerp_start_value_variant,
                                                  static_lerp_end_value_variant,
                                                 &static_segment_id) );

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          0,     /* time */
                                          false, /* should_force */
                                          result_variant) );

    system_variant_get_float(result_variant,
                            &result_float);

    ASSERT_EQ(result_float,
              222.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(1),
                                          false, /* should_force */
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_TRUE             (result_float > 300.0f &&
                             result_float < 350.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(2),
                                          false, /* should_force */
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             444.0f);

    ASSERT_TRUE(curve_container_get_value(test_curve,
                                          system_time_get_timeline_time_for_s(3),
                                          false,
                                          result_variant));

    system_variant_get_float(result_variant,
                            &result_float);
    ASSERT_EQ               (result_float,
                             666.0f);

    /* Clean up */
    system_variant_release (test_default_value_variant);
    system_variant_release (static_lerp_start_value_variant);
    system_variant_release (static_lerp_end_value_variant);
    system_variant_release (result_variant);
    curve_container_release(test_curve);
}