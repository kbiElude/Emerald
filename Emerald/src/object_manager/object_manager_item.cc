/**
 *
 * Emerald (kbi/elude @2012)
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
PUBLIC object_manager_item object_manager_item_create(__in __notnull system_hashed_ansi_string name, __in __notnull system_hashed_ansi_string origin_file, __in int origin_line, __in object_manager_object_type type, __in __notnull void* ptr)
{
    _object_manager_item* new_item = new (std::nothrow) _object_manager_item;

    ASSERT_DEBUG_SYNC(new_item != NULL, "Out of memory while allocating object manager item descriptor.");
    if (new_item != NULL)
    {
        new_item->name             = name;
        new_item->origin_file      = origin_file;
        new_item->origin_file_line = origin_line;
        new_item->ptr              = ptr;
        new_item->type             = type;
    }

    return (object_manager_item) new_item;
}

/* Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string object_manager_item_get_name(__in __notnull object_manager_item item)
{
    return ((_object_manager_item*) item)->name;
}

/* Please see header for specification */
PUBLIC EMERALD_API void object_manager_item_get_origin_details(__in __notnull object_manager_item item, __out __notnull system_hashed_ansi_string* out_file, __out __notnull int* out_file_line)
{
    *out_file      = ((_object_manager_item*) item)->origin_file;
    *out_file_line = ((_object_manager_item*) item)->origin_file_line;
}

/* Please see header for specification */
PUBLIC EMERALD_API void* object_manager_item_get_raw_pointer(__in __notnull object_manager_item item)
{
    return ((_object_manager_item*) item)->ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API object_manager_object_type object_manager_item_get_type(__in __notnull object_manager_item item)
{
    return ((_object_manager_item*) item)->type;
}

/* Please see header for specification */
PUBLIC void object_manager_item_release(__in __notnull __post_invalid object_manager_item item)
{
    delete (_object_manager_item*) item;
}
