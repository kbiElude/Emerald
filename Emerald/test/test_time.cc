/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_time.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "system/system_time.h"


TEST(TimeTest, Conversions)
{
    uint32_t hz_per_sec = system_time_get_hz_per_sec();

    ASSERT_EQ(system_time_get_timeline_time_for_s(60),
              60 * hz_per_sec);
    ASSERT_EQ(system_time_get_timeline_time_for_ms(2, 10),
              (2 * 60 + 10 ) * hz_per_sec);
    ASSERT_EQ(system_time_get_timeline_time_for_hms(1, 2, 3),
              (3600 + 2 * 60 + 3) * hz_per_sec);

    uint32_t result_32_1;
    uint8_t  result_8_1;
    uint8_t  result_8_2;

    system_time_get_s_for_timeline_time(75 * hz_per_sec,
                                       &result_32_1);
    ASSERT_EQ                          (result_32_1,
                                        75);

    system_time_get_ms_for_timeline_time(150 * hz_per_sec,
                                        &result_32_1,
                                        &result_8_1);
    ASSERT_EQ                          (result_32_1,
                                        2);
    ASSERT_EQ                          (result_8_1,
                                        30);

    system_time_get_hms_for_timeline_time(3676 * hz_per_sec,
                                         &result_32_1,
                                         &result_8_1,
                                         &result_8_2);
    ASSERT_EQ                            (result_32_1,
                                          1);
    ASSERT_EQ                            (result_8_1,
                                          1);
    ASSERT_EQ                            (result_8_2,
                                          16);
}

