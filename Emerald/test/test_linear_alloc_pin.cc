/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "test_linear_alloc_pin.h"
#include "system/system_linear_alloc_pin.h"
#include "gtest/gtest.h"


TEST(LinearAllocPinTest, EntryAlignmentTest)
{
    size_t allocation_size = 3;
    size_t n_entries       = 5;
    size_t n_pins          = 1;
    void*  entries[5]      = {0};

    system_linear_alloc_pin_handle handle = system_linear_alloc_pin_create(allocation_size, n_entries, n_pins);
    {
        for (size_t n_entry = 0; n_entry < 5; ++n_entry)
        {
            entries[n_entry] = system_linear_alloc_pin_get_from_pool(handle);
        }

        for (size_t n_entry = 0; n_entry < 4; ++n_entry)
        {
            ASSERT_TRUE( ((char*)entries[n_entry] + sizeof(void*)) == entries[n_entry+1]);
        }
    }
    system_linear_alloc_pin_release(handle);
}


TEST(LinearAllocPinTest, BlobReallocationTest)
{
    size_t allocation_size = 3;
    size_t n_entries       = 5;
    size_t n_pins          = 1;
    void*  entries[10]     = {0};

    system_linear_alloc_pin_handle handle = system_linear_alloc_pin_create(allocation_size, n_entries, n_pins);
    {
        for (size_t n_entry = 0; n_entry < 10; ++n_entry)
        {
            entries[n_entry] = system_linear_alloc_pin_get_from_pool(handle);
        }

        /* Verify the fifth allocation comes from a different blob */
        ASSERT_EQ(entries[4], (char*) entries[0] + sizeof(void*) * 4);
        ASSERT_NE(entries[5], (char*) entries[0] + sizeof(void*) * 5);

        /* Make sure reported pointers are not equal! */
        for (size_t n_base_ptr = 0; n_base_ptr < 10; ++n_base_ptr)
        {
            for (size_t n_base2_ptr = n_base_ptr + 1; n_base2_ptr < 10; ++n_base2_ptr)
            {
                ASSERT_NE(entries[n_base_ptr], entries[n_base2_ptr]);
            }
        }
    }
    system_linear_alloc_pin_release(handle);
}

TEST(LinearAllocPinTest, Pins)
{
    size_t allocation_size     = 3;
    size_t n_entries           = 10;
    size_t n_pins              = 1;
    void*  entries[10]         = {0};
    void*  afterpin_entries[9] = {0};

    system_linear_alloc_pin_handle handle = system_linear_alloc_pin_create(allocation_size, n_entries, n_pins);
    {
        for (size_t n_pass = 1; n_pass < 10; ++n_pass)
        {
            system_linear_alloc_pin_mark(handle);

            for (size_t n_entry = 0; n_entry < 10; ++n_entry)
            {
                if (n_entry == n_pass)
                {
                    system_linear_alloc_pin_mark(handle);
                }

                entries[n_entry] = system_linear_alloc_pin_get_from_pool(handle);
            }

            system_linear_alloc_pin_unmark(handle);

            ASSERT_EQ(system_linear_alloc_pin_get_from_pool(handle), entries[n_pass]);

            system_linear_alloc_pin_unmark(handle);
        }
    }
    system_linear_alloc_pin_release(handle);

    //
    handle = system_linear_alloc_pin_create(allocation_size, n_entries / 2, n_pins);
    {
        for (size_t n_pass = 1; n_pass < 10; ++n_pass)
        {
            system_linear_alloc_pin_mark(handle);

            for (size_t n_entry = 0; n_entry < 10; ++n_entry)
            {
                if (n_entry == n_pass)
                {
                    system_linear_alloc_pin_mark(handle);
                }

                entries[n_entry] = system_linear_alloc_pin_get_from_pool(handle);
            }

            system_linear_alloc_pin_unmark(handle);

            ASSERT_EQ(system_linear_alloc_pin_get_from_pool(handle), entries[n_pass]);

            system_linear_alloc_pin_unmark(handle);
        }
    }
    system_linear_alloc_pin_release(handle);
}
