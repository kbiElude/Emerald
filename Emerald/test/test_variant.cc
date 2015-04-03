/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_variant.h"
#include "shared.h"
#include "system/system_variant.h"
#include "gtest/gtest.h"


TEST(VariantTest, PoolManagement)
{
    system_variant variant_1 = system_variant_create(SYSTEM_VARIANT_INTEGER);
    system_variant variant_2 = system_variant_create(SYSTEM_VARIANT_BOOL);
    system_variant variant_3 = system_variant_create(SYSTEM_VARIANT_FLOAT);

    ASSERT_NE(variant_1,
              variant_2);
    ASSERT_NE(variant_2,
              variant_3);
    ASSERT_NE(variant_1,
              (void*)NULL);
    ASSERT_NE(variant_2,
              (void*)NULL);
    ASSERT_NE(variant_3,
              (void*)NULL);

    system_variant_release(variant_3);
    system_variant_release(variant_2);

    system_variant new_variant = system_variant_create(SYSTEM_VARIANT_FLOAT);

    ASSERT_EQ(new_variant,
              variant_2);

    system_variant new_variant_2 = system_variant_create(SYSTEM_VARIANT_INTEGER);

    ASSERT_EQ(new_variant_2,
              variant_3);

    system_variant new_variant_3 = system_variant_create(SYSTEM_VARIANT_BOOL);

    ASSERT_NE(new_variant_3,
              (void*)variant_1);
    ASSERT_NE(new_variant_3,
              (void*)variant_2);
    ASSERT_NE(new_variant_3,
              (void*)variant_3);
    ASSERT_NE(new_variant_3,
              (void*)new_variant);
    ASSERT_NE(new_variant_3,
              (void*)new_variant_2);
}

