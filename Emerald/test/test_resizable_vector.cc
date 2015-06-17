/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "test_resizable_vector.h"
#include "gtest/gtest.h"
#include "shared.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

#define N_OPERATIONS (100)

typedef enum
{
    RANDOM_OPERATION_TYPE_ADD,
    RANDOM_OPERATION_TYPE_DELETE,
    RANDOM_OPERATION_TYPE_PUSH,
    RANDOM_OPERATION_TYPE_POP,

    RANDOM_OPERATION_TYPE_COUNT
} random_operation_type;

bool add_new_item(std::vector<int>&       ref,
                  system_resizable_vector vec)
{
    unsigned int new_element          = rand();
    unsigned int new_element_location = (ref.size() > 0) ? rand() % ref.size() : 0;

    LOG_INFO("Adding a new element [%d] at location [%d]",
             new_element, new_element_location);

    ref.insert                               (ref.begin() + new_element_location,
                                              new_element);
    system_resizable_vector_insert_element_at(vec, new_element_location,
                                              (void*) (intptr_t) new_element);

    return true;
}

bool delete_item(std::vector<int>&       ref,
                 system_resizable_vector vec)
{
    if (ref.size() > 0)
    {
        unsigned int element_location = rand() % ref.size();

        LOG_INFO("Deleting an item at [%d]",
                 element_location);

        ref.erase                                (ref.begin() + element_location);
        system_resizable_vector_delete_element_at(vec,
                                                  element_location);

        return true;
    }
    else
    {
        return false;
    }
}

bool push_item(std::vector<int>&       ref,
               system_resizable_vector vec)
{
    unsigned int new_element = rand();

    LOG_INFO("Pushing a new item [%d]",
             new_element);

    ref.push_back               (new_element);
    system_resizable_vector_push(vec,
                                 (void*) (intptr_t) new_element);

    return true;
}

bool pop_item(std::vector<int>&       ref,
              system_resizable_vector vec)
{
    if (ref.size() > 0)
    {
        LOG_INFO("Popping an item");

        int ref_value = ref.back();
        int vec_value = 0;

        system_resizable_vector_pop(vec,
                                   &vec_value);

        ref.pop_back();

        if (ref_value != vec_value)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Sanity check failed!");
        }

        return true;
    }
    else
    {
        return false;
    }
}

TEST(ResizableVectorTest, RandomOperations)
{
    system_resizable_vector vec = system_resizable_vector_create(4 /* capacity */);
    std::vector<int>        ref;

    srand( (unsigned int) time(0) );

    for (unsigned int n_operation = 0;
                      n_operation < N_OPERATIONS;
                    ++n_operation)
    {
        bool can_exit_loop = false;

        while (!can_exit_loop)
        {
            random_operation_type op        = (random_operation_type) (rand() % RANDOM_OPERATION_TYPE_COUNT);
            bool                  op_result = false;

            switch(op)
            {
                case RANDOM_OPERATION_TYPE_ADD:     op_result = add_new_item(ref, vec); break;
                case RANDOM_OPERATION_TYPE_DELETE:  op_result = delete_item (ref, vec); break;
                case RANDOM_OPERATION_TYPE_PUSH:    op_result = push_item   (ref, vec); break;
                case RANDOM_OPERATION_TYPE_POP:     op_result = pop_item    (ref, vec); break;
            }

            if (op_result)
            {
                break;
            }
        }

        /* Make sure reference vector's contents matches resizable vector's */
        uint32_t n_vec_items = 0;

        system_resizable_vector_get_property(vec,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_vec_items);

        ASSERT_TRUE(n_vec_items == ref.size() );

        LOG_INFO("Contents at iteration [%d]:",
                 n_operation);

        for (size_t n = 0;
                    n < ref.size();
                  ++n)
        {
            LOG_INFO("[%d]. %d",
                     (int) n,
                     (int) ref[n]);

            int  vec_value = 0;
            bool vec_get_result = system_resizable_vector_get_element_at(vec,
                                                                         n,
                                                                        &vec_value);

            ASSERT_TRUE(vec_get_result);
            ASSERT_EQ  (vec_value,
                        ref[n]);
        }
    }

    system_resizable_vector_release(vec);
}

