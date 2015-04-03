/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "test_list_bidirectional.h"
#include "shared.h"
#include "system/system_log.h"
#include "system/system_list_bidirectional.h"
#include "gtest/gtest.h"

#define N_OPERATIONS (10000)

typedef enum
{
    RANDOM_OPERATION_TYPE_APPEND,
    RANDOM_OPERATION_TYPE_CLEAR,
    RANDOM_OPERATION_TYPE_DELETE,
    RANDOM_OPERATION_TYPE_PUSH_AT_END,
    RANDOM_OPERATION_TYPE_PUSH_AT_FRONT,
    RANDOM_OPERATION_TYPE_REMOVE_ITEM,

    RANDOM_OPERATION_TYPE_COUNT
} random_operation_type;


bool append_item(std::vector<int>&         ref,
                 system_list_bidirectional list)
{
    unsigned int                   new_element          = rand();
    unsigned int                   new_element_location = (ref.size() > 0) ? rand() % ref.size() : 0;
    system_list_bidirectional_item prepending_item      = NULL;
    bool                           result               = true;

    LOG_INFO("Adding a new element [%d] at location [%d]",
             new_element, new_element_location);

    if (ref.size() == 0)
    {
        result = false;

        goto end;
    }

    system_list_bidirectional_get_item_at(list,
                                          new_element_location,
                                          &prepending_item);

    ref.insert(ref.begin() + (new_element_location + 1),
               new_element);

    ASSERT_ALWAYS_SYNC(prepending_item != NULL,
                       "Prepending item is NULL");

    system_list_bidirectional_append(list,
                                     prepending_item,
                                     (void*) new_element);

end:
    return result;
}

bool clear(std::vector<int>&         ref,
           system_list_bidirectional list)
{
    ref.clear                      ();
    system_list_bidirectional_clear(list);

    return true;
}

bool delete_item(std::vector<int>&         ref,
                 system_list_bidirectional list)
{
    if (ref.size() > 0)
    {
        unsigned int                   element_location = rand() % ref.size();
        system_list_bidirectional_item item_to_remove   = NULL;

        LOG_INFO("Deleting an item at [%d]",
                 element_location);

        system_list_bidirectional_get_item_at(list,
                                              element_location,
                                             &item_to_remove);

        ASSERT_ALWAYS_SYNC(item_to_remove != NULL,
                           "Item to remove is NULL");

        ref.erase                            (ref.begin() + element_location);
        system_list_bidirectional_remove_item(list,
                                              item_to_remove);

        return true;
    }
    else
    {
        return false;
    }
}

bool push_item_at_end(std::vector<int>&         ref,
                      system_list_bidirectional list)
{
    unsigned int new_element = rand();

    LOG_INFO("Pushing a new item [%d] at the end",
             new_element);

    ref.push_back                        (new_element);
    system_list_bidirectional_push_at_end(list,
                                          (void*) new_element);

    return true;
}

bool push_item_at_front(std::vector<int>&         ref,
                        system_list_bidirectional list)
{
    unsigned int new_element = rand();

    LOG_INFO("Pushing a new item [%d] at the front",
             new_element);

    ref.insert                             (ref.begin(),
                                            new_element);
    system_list_bidirectional_push_at_front(list,
                                            (void*) new_element);

    return true;
}

bool remove_item(std::vector<int>&         ref,
                 system_list_bidirectional list)
{
    if (ref.size() > 0)
    {
        LOG_INFO("Removing an item");

        unsigned int                   element_location              = rand() % ref.size();
        system_list_bidirectional_item list_item_at_element_location = NULL;

        system_list_bidirectional_get_item_at(list,
                                              element_location,
                                             &list_item_at_element_location);

        ASSERT_ALWAYS_SYNC(list_item_at_element_location != NULL,
                           "List returned NULL for an existing element location");

        ref.erase                            (ref.begin() + element_location);
        system_list_bidirectional_remove_item(list,
                                              list_item_at_element_location);

        return true;
    }
    else
    {
        return false;
    }
}

TEST(ListBidirectional, RandomOperations)
{
    system_list_bidirectional list = system_list_bidirectional_create();
    std::vector<int>          ref;

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
                case RANDOM_OPERATION_TYPE_APPEND:        op_result = append_item       (ref, list); break;
                case RANDOM_OPERATION_TYPE_CLEAR:         op_result = clear             (ref, list); break;
                case RANDOM_OPERATION_TYPE_DELETE:        op_result = delete_item       (ref, list); break;
                case RANDOM_OPERATION_TYPE_PUSH_AT_END:   op_result = push_item_at_end  (ref, list); break;
                case RANDOM_OPERATION_TYPE_PUSH_AT_FRONT: op_result = push_item_at_front(ref, list); break;
                case RANDOM_OPERATION_TYPE_REMOVE_ITEM:   op_result = remove_item       (ref, list); break;
            }

            if (op_result)
            {
                break;
            }
        }

        /* Verify list's head & tail */
        ASSERT_TRUE(system_list_bidirectional_get_number_of_elements(list) == ref.size() );

        system_list_bidirectional_item list_head      = system_list_bidirectional_get_head_item(list);
        void*                          list_head_data = NULL;
        system_list_bidirectional_item list_tail      = system_list_bidirectional_get_tail_item(list);
        void*                          list_tail_data = NULL;

        if (ref.size() == 0)
        {
            ASSERT_TRUE(list_head == NULL);
            ASSERT_TRUE(list_tail == NULL);
        }

        if (list_head != NULL)
        {
            system_list_bidirectional_get_item_data(list_head,
                                                   &list_head_data);

            ASSERT_EQ(ref[0],
                      (int) list_head_data);
        }
        else
        {
            ASSERT_EQ(ref.size(),
                      0);
        }

        if (list_tail != NULL)
        {
            system_list_bidirectional_get_item_data(list_tail,
                                                   &list_tail_data);

            ASSERT_EQ(ref[ref.size() - 1],
                      (int) list_tail_data);
        }
        else
        {
            ASSERT_EQ(ref.size(),
                      0);
        }

        /* Verify that correct items are reported, when moving from the first to the last item */
        system_list_bidirectional_item current_item       = list_head;
        void*                          current_item_data  = NULL;
        unsigned int                   current_item_index = 0;

        while (current_item != NULL)
        {
            void* current_item_data = NULL;

            system_list_bidirectional_get_item_data(current_item,
                                                   &current_item_data);

            ASSERT_EQ( (int) current_item_data,
                       ref[current_item_index]);

            /* Move ahead */
            current_item_index++;

            current_item = system_list_bidirectional_get_next_item(current_item);
        } /* while (current_item != NULL) */

        /* ..and vice versa */
        current_item       = list_tail;
        current_item_index = system_list_bidirectional_get_number_of_elements(list) - 1;

        while (current_item != NULL)
        {
            void* current_item_data = NULL;

            system_list_bidirectional_get_item_data(current_item,
                                                   &current_item_data);

            ASSERT_EQ( (int) current_item_data,
                       ref[current_item_index]);

            /* Move ahead */
            current_item_index--;

            current_item = system_list_bidirectional_get_previous_item(current_item);
        } /* while (current_item != NULL) */

        /* Make sure reference vector's contents matches resizable vector's */
        LOG_INFO("Contents at iteration [%d]:",
                 n_operation);

        for (size_t n = 0;
                    n < ref.size();
                  ++n)
        {
            LOG_INFO("[%d]. %d",
                     n,
                     ref[n]);

            system_list_bidirectional_item list_item       = NULL;
            void*                          list_item_data  = NULL;
            bool                           list_get_result = system_list_bidirectional_get_item_at(list,
                                                                                                   n,
                                                                                                  &list_item);

            system_list_bidirectional_get_item_data(list_item,
                                                   &list_item_data);

            ASSERT_TRUE(list_get_result);
            ASSERT_EQ  (ref[n],
                        (int) list_item_data);
        }
    }

    system_list_bidirectional_release(list);
}

