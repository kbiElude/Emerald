/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "object_manager/object_manager_item.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/* Private definitions */
typedef struct
{
    system_hashed_ansi_string  name;
    system_hashed_ansi_string  origin_file;
    int                        origin_file_line;
    void*                      ptr;
    object_manager_object_type type;

} _object_manager_item;

/* Private variables */

/* Please see header for specification */
PUBLIC object_manager_item object_manager_item_create(system_hashed_ansi_string  name,
                                                      system_hashed_ansi_string  origin_file,
                                                      int                        origin_line,
                                                      object_manager_object_type type,
                                                      void*                      ptr)
{
    _object_manager_item* new_item_ptr = new (std::nothrow) _object_manager_item;

    ASSERT_DEBUG_SYNC(new_item_ptr != nullptr,
                      "Out of memory while allocating object manager item descriptor.");

    if (new_item_ptr != nullptr)
    {
        new_item_ptr->name             = name;
        new_item_ptr->origin_file      = origin_file;
        new_item_ptr->origin_file_line = origin_line;
        new_item_ptr->ptr              = ptr;
        new_item_ptr->type             = type;
    }

    return reinterpret_cast<object_manager_item>(new_item_ptr);
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string object_manager_item_get_name(object_manager_item item)
{
    return reinterpret_cast<_object_manager_item*>(item)->name;
}

/* Please see header for specification */
PUBLIC void object_manager_item_get_origin_details(object_manager_item        item,
                                                   system_hashed_ansi_string* out_file_ptr,
                                                   int*                       out_file_line_ptr)
{
    *out_file_ptr      = reinterpret_cast<_object_manager_item*>(item)->origin_file;
    *out_file_line_ptr = reinterpret_cast<_object_manager_item*>(item)->origin_file_line;
}

/* Please see header for specification */
PUBLIC void* object_manager_item_get_raw_pointer(object_manager_item item)
{
    return reinterpret_cast<_object_manager_item*>(item)->ptr;
}

/* Please see header for specification */
PUBLIC object_manager_object_type object_manager_item_get_type(object_manager_item item)
{
    return reinterpret_cast<_object_manager_item*>(item)->type;
}

/* Please see header for specification */
PUBLIC void object_manager_item_release(object_manager_item item)
{
    delete reinterpret_cast<_object_manager_item*>(item);
}
