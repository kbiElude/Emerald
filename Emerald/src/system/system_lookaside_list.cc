/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_lookaside_list.h"
#include "system/system_resizable_vector.h"

/** Internal system looakside list descriptor */
struct _system_lookaside_list_descriptor
{
    system_critical_section cs;
    system_resizable_vector container;

    size_t n_entries_in_alloc;
    size_t entry_size;
};

/** Initializes a system look-aside list descriptor.
 *
 *  @param descriptor         Descriptor. CANNOT be null.
 *  @param n_entries_in_alloc Amount of items to allocate whenever look-aside list runs out of items.
 *  @param entry_size         Size of a signle item.
 **/
PRIVATE void _init_system_lookaside_list_descriptor(__in _system_lookaside_list_descriptor* descriptor, __in size_t n_entries_in_alloc, __in size_t entry_size)
{
    descriptor->n_entries_in_alloc = n_entries_in_alloc;
    descriptor->entry_size         = entry_size;
    descriptor->cs                 = system_critical_section_create();
    descriptor->container          = system_resizable_vector_create(n_entries_in_alloc, entry_size);
}

/** Deinitializes a system look-aside list descriptor. DO NOT attempt to use the descriptor after calling
 *  this function.
 *
 *  @param descriptor Descriptor to deinitialize.
 */
PRIVATE void _deinit_system_lookaside_list_descriptor(__in _system_lookaside_list_descriptor* descriptor)
{
    system_critical_section_release(descriptor->cs);
    system_resizable_vector_release(descriptor->container);

    delete descriptor;
}

/** Allocates a set of new items for the look-aside list.
 *
 *  @param descriptor Descriptor to use.
 **/
PRIVATE void _allocate_items(__in _system_lookaside_list_descriptor* descriptor)
{
    LOG_INFO("Allocating items for look-aside list.");
    {
        system_critical_section_enter(descriptor->cs);
        {
            for (size_t n_item = 0; n_item < descriptor->n_entries_in_alloc; ++n_item)
            {
                void* new_item = new char[descriptor->entry_size];

                system_resizable_vector_push(descriptor->container, &new_item);
            }
        }
        system_critical_section_leave(descriptor->cs);
    }
    LOG_INFO("Allocated items for look-aside list.");
}

/** Please see header for specification */
PUBLIC EMERALD_API system_lookaside_list system_lookaside_list_create(__in size_t entry_size, __in size_t entries_to_alloc_when_needed)
{
    _system_lookaside_list_descriptor* new_descriptor = new _system_lookaside_list_descriptor();

    _init_system_lookaside_list_descriptor(new_descriptor, entries_to_alloc_when_needed, entry_size);

    return (system_lookaside_list) new_descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_lookaside_list_release(__in __deallocate(mem) system_lookaside_list lookaside_list)
{
    _deinit_system_lookaside_list_descriptor( (_system_lookaside_list_descriptor*) lookaside_list);
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t system_lookaside_list_get_amount_of_available_items(__in system_lookaside_list lookaside_list)
{
    _system_lookaside_list_descriptor* descriptor = (_system_lookaside_list_descriptor*) lookaside_list;
    uint32_t                           result     = 0;

    system_critical_section_enter(descriptor->cs);
    {
        result = system_resizable_vector_get_amount_of_elements(descriptor->container);
    }
    system_critical_section_leave(descriptor->cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_lookaside_list_item system_lookaside_list_get_item(__in system_lookaside_list lookaside_list)
{
    _system_lookaside_list_descriptor* descriptor = (_system_lookaside_list_descriptor*) lookaside_list;
    system_lookaside_list_item         result     = NULL;

    system_critical_section_enter(descriptor->cs);
    {
        if (system_resizable_vector_get_amount_of_elements(descriptor->container) == 0)
        {
            _allocate_items(descriptor);
        }

        bool result_get = system_resizable_vector_pop(descriptor->container, &result);
        ASSERT_DEBUG_SYNC(result_get, "Could not retrieve look-aside list descriptor.");
    }
    system_critical_section_leave(descriptor->cs);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_lookaside_list_release_item(__in system_lookaside_list lookaside_list, __in __deallocate(mem) system_lookaside_list_item item)
{
    _system_lookaside_list_descriptor* descriptor = (_system_lookaside_list_descriptor*) lookaside_list;

    system_critical_section_enter(descriptor->cs);
    {
        system_resizable_vector_push(descriptor->container, &item);
    }
    system_critical_section_leave(descriptor->cs);
}

