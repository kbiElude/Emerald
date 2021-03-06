/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_linear_alloc_pin.h"
#include "system/system_list_bidirectional.h"


typedef struct _system_list_bidirectional_item
{
    void*                            data;
    _system_list_bidirectional_item* next_ptr;
    _system_list_bidirectional_item* prev_ptr;

    explicit _system_list_bidirectional_item(void* in_data)
    {
        data     = in_data;
        next_ptr = NULL;
        prev_ptr = NULL;
    }
} _system_list_bidirectional_item;

typedef struct _system_list_bidirectional
{
    _system_list_bidirectional_item* head_ptr;
    system_linear_alloc_pin          item_allocator;
    unsigned int                     n_items;
    _system_list_bidirectional_item* last_ptr;

    _system_list_bidirectional()
    {
        head_ptr       = NULL;
        item_allocator = system_linear_alloc_pin_create(sizeof(_system_list_bidirectional_item),
                                                        16, /* n_entries_to_prealloc */
                                                        1); /* n_pins_to_prealloc */
        n_items        = 0;
        last_ptr       = NULL;
    }

    ~_system_list_bidirectional()
    {
        if (item_allocator != NULL)
        {
            system_linear_alloc_pin_release(item_allocator);

            item_allocator = NULL;
        }
    }
} _system_list_bdirectional;


/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_append(system_list_bidirectional      list,
                                                         system_list_bidirectional_item item,
                                                         void*                          new_item_data)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(list != NULL,
                      "List is NULL");
    ASSERT_DEBUG_SYNC(item != NULL,
                      "Item is NULL");

    /* Go for it.. */
    _system_list_bidirectional_item* item_ptr     = (_system_list_bidirectional_item*) item;
    _system_list_bidirectional*      list_ptr     = (_system_list_bidirectional*)      list;
    _system_list_bidirectional_item* new_item_ptr = (_system_list_bidirectional_item*) system_linear_alloc_pin_get_from_pool(list_ptr->item_allocator);

    new_item_ptr->data     = new_item_data;
    new_item_ptr->next_ptr = NULL;
    new_item_ptr->prev_ptr = NULL;

    if (item_ptr->next_ptr == NULL)
    {
        /* No item following the provided item */
        item_ptr->next_ptr     = new_item_ptr;
        new_item_ptr->prev_ptr = item_ptr;
    }
    else
    {
        /* Insert the new item between the submitted item and the one that used to follow it */
        new_item_ptr->next_ptr       = item_ptr->next_ptr;
        new_item_ptr->prev_ptr       = item_ptr;
        item_ptr->next_ptr->prev_ptr = new_item_ptr;
        item_ptr->next_ptr           = new_item_ptr;
    }

    if (new_item_ptr->next_ptr == NULL)
    {
        /* This is the last item on the list */
        list_ptr->last_ptr = new_item_ptr;
    }

    list_ptr->n_items++;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_clear(system_list_bidirectional list)
{
    _system_list_bidirectional* list_ptr = (_system_list_bidirectional*) list;

    list_ptr->head_ptr = NULL;
    list_ptr->last_ptr = NULL;
    list_ptr->n_items  = 0;

    system_linear_alloc_pin_return_all(list_ptr->item_allocator);
}

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional system_list_bidirectional_create()
{
    _system_list_bidirectional* new_list_ptr = new (std::nothrow) _system_list_bidirectional;

    ASSERT_DEBUG_SYNC(new_list_ptr != NULL,
                      "Out of memory");

    return (system_list_bidirectional) new_list_ptr;
}

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_head_item(system_list_bidirectional list)
{
    return (system_list_bidirectional_item) ( (_system_list_bidirectional*) list)->head_ptr;
}

/** TODO */
PUBLIC EMERALD_API bool system_list_bidirectional_get_item_at(system_list_bidirectional       list,
                                                              unsigned int                    index,
                                                              system_list_bidirectional_item* out_item)
{
    _system_list_bidirectional*      list_ptr       = (_system_list_bidirectional*) list;
    _system_list_bidirectional_item* item_ptr       = list_ptr->head_ptr;
    unsigned int                     n_current_item = 0;
    bool                             result         = false;

    while (item_ptr       != NULL &&
           n_current_item != index)
    {
        item_ptr = item_ptr->next_ptr;

        ++n_current_item;
    } /* while (item_ptr != NULL && n_current_item != index) */

    if (item_ptr != NULL && n_current_item == index)
    {
        *out_item = (system_list_bidirectional_item) item_ptr;
        result    = true;
    }

    return result;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_get_item_data(system_list_bidirectional_item list_item,
                                                                void**                         out_result)
{
    _system_list_bidirectional_item* list_item_ptr = (_system_list_bidirectional_item*) list_item;

    *out_result = list_item_ptr->data;
}

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_next_item(system_list_bidirectional_item list_item)
{
    return (system_list_bidirectional_item) ( (_system_list_bidirectional_item*) list_item)->next_ptr;
}

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_previous_item(system_list_bidirectional_item list_item)
{
    return (system_list_bidirectional_item) ( (_system_list_bidirectional_item*) list_item)->prev_ptr;
}

/** TODO */
PUBLIC EMERALD_API unsigned int system_list_bidirectional_get_number_of_elements(system_list_bidirectional list)
{
    return ( (_system_list_bidirectional*) list)->n_items;
}

/** TODO */
PUBLIC EMERALD_API system_list_bidirectional_item system_list_bidirectional_get_tail_item(system_list_bidirectional list)
{
    return (system_list_bidirectional_item) ( (_system_list_bidirectional*) list)->last_ptr;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_end(system_list_bidirectional list,
                                                              void*                      new_item_data)
{
    _system_list_bidirectional*      list_ptr     = (_system_list_bidirectional*)      list;
    _system_list_bidirectional_item* new_item_ptr = (_system_list_bidirectional_item*) system_linear_alloc_pin_get_from_pool(list_ptr->item_allocator);

    new_item_ptr->data     = new_item_data;
    new_item_ptr->next_ptr = NULL;
    new_item_ptr->prev_ptr = NULL;

    if (list_ptr->last_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(list_ptr->head_ptr == NULL,
                          "No last item enlisted even though the list holds at least 1 element");

        list_ptr->head_ptr = new_item_ptr;
        list_ptr->last_ptr = new_item_ptr;
    }
    else
    {
        ASSERT_DEBUG_SYNC(list_ptr->head_ptr != NULL,
                          "No head item for a non-empty list");
        ASSERT_DEBUG_SYNC(list_ptr->last_ptr->next_ptr == NULL,
                          "Last list item is not the last one?!");

        list_ptr->last_ptr->next_ptr = new_item_ptr;
        new_item_ptr->prev_ptr       = list_ptr->last_ptr;

        list_ptr->last_ptr = new_item_ptr;
    }

    list_ptr->n_items++;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_push_at_front(system_list_bidirectional list,
                                                                void*                      new_item_data)
{
    _system_list_bidirectional*      list_ptr     = (_system_list_bidirectional*)      list;
    _system_list_bidirectional_item* new_item_ptr = (_system_list_bidirectional_item*) system_linear_alloc_pin_get_from_pool(list_ptr->item_allocator);

    new_item_ptr->data     = new_item_data;
    new_item_ptr->next_ptr = list_ptr->head_ptr;
    new_item_ptr->prev_ptr = NULL;

    if (list_ptr->head_ptr != NULL)
    {
        list_ptr->head_ptr->prev_ptr = new_item_ptr;
    }

    list_ptr->head_ptr = new_item_ptr;

    if (list_ptr->last_ptr == NULL)
    {
        list_ptr->last_ptr = new_item_ptr;
    }

    list_ptr->n_items++;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_release(system_list_bidirectional list)
{
    delete (_system_list_bidirectional*) list;
}

/** TODO */
PUBLIC EMERALD_API void system_list_bidirectional_remove_item(system_list_bidirectional      list,
                                                              system_list_bidirectional_item list_item)
{
    _system_list_bidirectional_item* item_ptr = (_system_list_bidirectional_item*) list_item;
    _system_list_bidirectional*      list_ptr = (_system_list_bidirectional*)      list;

    if (item_ptr->prev_ptr != NULL && item_ptr->next_ptr != NULL)
    {
        /* Removal of an in-between item */
        ASSERT_DEBUG_SYNC(item_ptr->prev_ptr->next_ptr == (_system_list_bidirectional_item*) list_item,
                          "Sanity check failed.");
        ASSERT_DEBUG_SYNC(item_ptr->next_ptr->prev_ptr == (_system_list_bidirectional_item*) list_item,
                          "Sanity check failed.");

        item_ptr->prev_ptr->next_ptr = item_ptr->next_ptr;
        item_ptr->next_ptr->prev_ptr = item_ptr->prev_ptr;
    }
    else
    if (item_ptr->prev_ptr != NULL && item_ptr->next_ptr == NULL)
    {
        /* Removal of the tail item */
        ASSERT_DEBUG_SYNC(item_ptr->prev_ptr->next_ptr == item_ptr,
                          "Sanity check failed.");
        ASSERT_DEBUG_SYNC(list_ptr->last_ptr           == item_ptr,
                          "Sanity check failed.");

        item_ptr->prev_ptr->next_ptr = NULL;
        list_ptr->last_ptr           = item_ptr->prev_ptr;

        if (list_ptr->head_ptr == item_ptr)
        {
            list_ptr->head_ptr = NULL;
        }
    }
    else
    if (item_ptr->prev_ptr == NULL && item_ptr->next_ptr != NULL)
    {
        /* Removal of the head item */
        ASSERT_DEBUG_SYNC(item_ptr->next_ptr->prev_ptr == item_ptr,
                          "Sanity check failed.");
        ASSERT_DEBUG_SYNC(list_ptr->head_ptr           == item_ptr,
                          "Sanity check failed.");

        item_ptr->next_ptr->prev_ptr = NULL;
        list_ptr->head_ptr           = item_ptr->next_ptr;

        if (list_ptr->last_ptr == item_ptr)
        {
            list_ptr->last_ptr = NULL;
        }
    }
    else
    if (list_ptr->n_items == 1)
    {
        list_ptr->head_ptr = NULL;
        list_ptr->last_ptr = NULL;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "An abandonded item was detected in the list.");
    }

    list_ptr->n_items--;
}
