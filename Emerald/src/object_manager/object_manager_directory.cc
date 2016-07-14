/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "object_manager/object_manager_directory.h"
#include "object_manager/object_manager_item.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"

/** Internal type definition */
typedef struct
{

    system_critical_section   cs;
    system_hash64map          directories; /* maps hashes to object_manager_directory pointers */
    system_hash64map          items;       /* maps hashes to object_manager_item pointers */
    system_hashed_ansi_string name;

} _object_manager_directory;

/** TODO */
bool _verbose_status = false;


/* Please see header for specification */
PUBLIC object_manager_directory object_manager_directory_create(system_hashed_ansi_string name)
{
    _object_manager_directory* new_instance = new (std::nothrow) _object_manager_directory;

    ASSERT_DEBUG_SYNC(new_instance != nullptr,
                      "Out of memory while instantiating _object_manager_directory");

    if (new_instance == nullptr)
    {
        goto end;
    }

    /* Fill the fields */
    new_instance->cs          = system_critical_section_create();
    new_instance->directories = system_hash64map_create( sizeof(object_manager_directory) );
    new_instance->items       = system_hash64map_create( sizeof(void*) );
    new_instance->name        = name;

end: 
    return (object_manager_directory) new_instance;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_create_directory_structure(object_manager_directory  directory,
                                                                system_hashed_ansi_string path)
{
    _object_manager_directory* directory_ptr  = reinterpret_cast<_object_manager_directory*>(directory);
    const char*                path_ptr       = system_hashed_ansi_string_get_buffer(path);
    object_manager_directory   curr_directory = directory;
    const char*                curr_path_ptr  = path_ptr;
    const char*                prev_path_ptr  = path_ptr;
    bool                       result         = true;

    system_critical_section_enter(directory_ptr->cs);
    {
        while ( (curr_path_ptr = strstr(path_ptr, "\\")) != nullptr)
        {
            size_t directory_name_length = curr_path_ptr - prev_path_ptr;
            char*  directory_name        = new (std::nothrow) char[directory_name_length + 1];

            ASSERT_DEBUG_SYNC(directory_name != nullptr,
                              "Could not allocate space for directory name");

            if (directory_name != nullptr)
            {
                memcpy(directory_name,
                       prev_path_ptr,
                       directory_name_length);

                directory_name[directory_name_length] = 0;

                /* Try to find the sub-directory in currently processed directory */
                system_hashed_ansi_string subdirectory_name_has = system_hashed_ansi_string_create          (directory_name);
                object_manager_directory  subdirectory          = object_manager_directory_find_subdirectory(curr_directory,
                                                                                                             subdirectory_name_has);

                if (subdirectory == nullptr)
                {
                    subdirectory = object_manager_directory_create(subdirectory_name_has);

                    ASSERT_DEBUG_SYNC(subdirectory != nullptr,
                                      "Could not create directory [%s]",
                                      directory_name);

                    if (subdirectory != nullptr)
                    {
                        if (!object_manager_directory_insert_subdirectory(curr_directory,
                                                                          subdirectory) )
                        {
                            LOG_ERROR        ("Could not insert sub-directory [%s]",
                                              directory_name);
                            ASSERT_DEBUG_SYNC(false,
                                              "");

                            result = false;
                            goto end;
                        }
                    }
                }

                curr_directory = subdirectory;
                path_ptr       = curr_path_ptr + 1;
                prev_path_ptr  = path_ptr;

                delete [] directory_name;
                directory_name = nullptr;
            }
        }    
    }
end:
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_delete_directory(object_manager_directory  directory,
                                                      system_hashed_ansi_string directory_name)
{
    bool                       result        = false;
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);

    system_critical_section_enter(directory_ptr->cs);
    {
        system_hash64 directory_name_hash = system_hashed_ansi_string_get_hash(directory_name);

        result = system_hash64map_remove(directory_ptr->directories,
                                         directory_name_hash);
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_delete_item(object_manager_directory  directory,
                                                 system_hashed_ansi_string item_name)
{
    bool                       result        = false;
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);

    system_critical_section_enter(directory_ptr->cs);
    {
        object_manager_item item           = nullptr;
        system_hash64       item_name_hash = system_hashed_ansi_string_get_hash(item_name);

        result = system_hash64map_get(directory_ptr->items,
                                      item_name_hash,
                                     &item);

        ASSERT_DEBUG_SYNC(result,
                          "Could not retrieve item [%s] for deletion!",
                          system_hashed_ansi_string_get_buffer(item_name) );

        if (result)
        {
            object_manager_item_release(item);

            result = system_hash64map_remove(directory_ptr->items,
                                             item_name_hash);
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_extract_item_name_and_path(system_hashed_ansi_string  registration_path,
                                                                system_hashed_ansi_string* out_item_path,
                                                                system_hashed_ansi_string* out_item_name)
{
    const char* path = system_hashed_ansi_string_get_buffer(registration_path);

    /* Make sure path starts with \ */
    if (path    == nullptr ||
        path[0] != '\\')
    {
        LOG_ERROR("Cannot retrieve directory object for path that is either nullptr or does not start with /");

        return false;
    }

    /* Move through the directory structure */
    const char*               cur_traveller_ptr = path;
    const char*               prv_traveller_ptr = cur_traveller_ptr + 1;
    system_hashed_ansi_string result            = system_hashed_ansi_string_get_default_empty_string();

    while (true)
    {
        /* Look for separator */
        prv_traveller_ptr = cur_traveller_ptr;
        cur_traveller_ptr = strstr(cur_traveller_ptr, "\\");

        if (cur_traveller_ptr == nullptr)
        {
            *out_item_name = system_hashed_ansi_string_create(prv_traveller_ptr);

            /* Create path storage */
            size_t path_length = prv_traveller_ptr - path - 1;
            char*  only_path   = new (std::nothrow) char[path_length + 1];

            ASSERT_DEBUG_SYNC(only_path != nullptr,
                              "Could not allocate memory for path storage (%d bytes)",
                              path_length + 1);

            if (only_path != nullptr)
            {
                memcpy(only_path,
                       path + 1,
                       path_length);

                only_path[path_length] = 0;

                *out_item_path = system_hashed_ansi_string_create(only_path);

                delete [] only_path;
                only_path = nullptr;
            }

            break;
        }

        /* Move one character ahead so that we don't get stuck at '\' in next iteration */
        cur_traveller_ptr++;
    }

    return true;
}

/* Please see header for specification */
PUBLIC object_manager_item object_manager_directory_find_item(object_manager_directory  directory,
                                                              system_hashed_ansi_string item_name)
{
    object_manager_item        result         = nullptr;
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);

    system_critical_section_enter(directory_ptr->cs);
    {
        system_hash64 item_name_hash = system_hashed_ansi_string_get_hash(item_name);

        system_hash64map_get(directory_ptr->items,
                             item_name_hash,
                            &result);
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC object_manager_item object_manager_directory_find_item_recursive(object_manager_directory  directory,
                                                                        system_hashed_ansi_string item_path)
{
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);
    object_manager_item        result        = nullptr;

    system_critical_section_enter(directory_ptr->cs);
    {
        bool                      boolean_result      = false;
        system_hashed_ansi_string extracted_item_name = nullptr;
        system_hashed_ansi_string extracted_item_path = nullptr;

        boolean_result = object_manager_directory_extract_item_name_and_path(item_path,
                                                                            &extracted_item_path,
                                                                            &extracted_item_name);

        ASSERT_DEBUG_SYNC(boolean_result,
                          "Input path [%s] appears to be invalid",
                          system_hashed_ansi_string_get_buffer(item_path) );

        if (boolean_result)
        {
            object_manager_directory item_root_directory = object_manager_directory_find_subdirectory_recursive(directory,
                                                                                                                extracted_item_path);

            ASSERT_DEBUG_SYNC(item_root_directory != nullptr,
                              "Could not find directory [%s]",
                              system_hashed_ansi_string_get_buffer(extracted_item_path) );

            if (item_root_directory != nullptr)
            {
                result = object_manager_directory_find_item(item_root_directory,
                                                            extracted_item_name);

                ASSERT_DEBUG_SYNC(result != nullptr,
                                  "Unable to find item [%s] at [%s]",
                                  system_hashed_ansi_string_get_buffer(extracted_item_name),
                                  system_hashed_ansi_string_get_buffer(extracted_item_path) );
            }
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC object_manager_directory object_manager_directory_find_subdirectory(object_manager_directory  start_directory,
                                                                           system_hashed_ansi_string subdirectory_name)
{
    object_manager_directory   result                = nullptr;
    _object_manager_directory* start_directory_ptr   = reinterpret_cast<_object_manager_directory*>(start_directory);

    system_critical_section_enter(start_directory_ptr->cs);
    {
        system_hash64 subdirectory_name_hash = system_hashed_ansi_string_get_hash(subdirectory_name);

        system_hash64map_get(start_directory_ptr->directories,
                             subdirectory_name_hash,
                            &result);
    }
    system_critical_section_leave(start_directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC object_manager_directory object_manager_directory_find_subdirectory_recursive(object_manager_directory  start_directory,
                                                                                     system_hashed_ansi_string path)
{
    object_manager_directory   result              = start_directory;
    _object_manager_directory* start_directory_ptr = reinterpret_cast<_object_manager_directory*>(start_directory);
    _object_manager_directory* curr_directory_ptr  = start_directory_ptr;
    const char*                path_ptr            = system_hashed_ansi_string_get_buffer(path);

    while (true)
    {
        /* Try to find next '\' sign */
        const char* new_path_ptr = strstr(path_ptr,
                                          "\\");

        if (new_path_ptr == nullptr)
        {
            /* No subdirectory - the rest of the path is item's name */
            //result = object_manager_directory_find_subdirectory(result, system_hashed_ansi_string_create(path_ptr) );

            break;
        }
        else
        {
            /* Subdirectories follow - extract directory name */
            size_t directory_name_length = new_path_ptr - path_ptr;
            char*  directory_name        = new (std::nothrow) char[directory_name_length + 1];
            bool   should_break          = false;

            if (directory_name != nullptr)
            {
                memcpy(directory_name,
                       path_ptr,
                       directory_name_length);

                directory_name[directory_name_length] = 0;

                /* Find the subdirectory */
                result = object_manager_directory_find_subdirectory(result,
                                                                    system_hashed_ansi_string_create(directory_name) );

                if (result != nullptr)
                {
                    path_ptr = new_path_ptr + 1;
                }
                else
                {
                    LOG_ERROR        ("Could not enter subdirectory [%s]",
                                      directory_name);
                    ASSERT_DEBUG_SYNC(false,
                                      "");

                    result       = nullptr;
                    should_break = true;
                }

                delete [] directory_name;
                directory_name = nullptr;
            }
            else
            {
                LOG_ERROR        ("Could not allocate %u bytes while traversing directory path.",
                                  (unsigned int) (directory_name_length + 1) );
                ASSERT_DEBUG_SYNC(false,
                                  "");

                result       = nullptr;
                should_break = true;
            }

            if (should_break)
            {
                break;
            }
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC uint32_t object_manager_directory_get_amount_of_children_for_directory(object_manager_directory directory)
{
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);
    uint32_t                   result        = 0;

    system_critical_section_enter(directory_ptr->cs);
    {
        size_t n_directories = 0;
        size_t n_items       = 0;

        system_hash64map_get_property(directory_ptr->directories,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_directories);
        system_hash64map_get_property(directory_ptr->items,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_items);

        if (n_directories != 0 ||
            n_items       != 0)
        {
            LOG_INFO("Directory [%s] has %u directories and %u items:",
                     system_hashed_ansi_string_get_buffer(directory_ptr->name),
                     (unsigned int) n_directories,
                     (unsigned int) n_items);

            for (unsigned int n_item = 0;
                              n_item < n_items;
                            ++n_item)
            {
                object_manager_item current_item = nullptr;

                if (system_hash64map_get_element_at(directory_ptr->items,
                                                    n_item,
                                                   &current_item,
                                                    nullptr) ) /* pOutHash */
                {
                    system_hashed_ansi_string current_item_name            = object_manager_item_get_name       (current_item);
                    void*                     current_item_object          = object_manager_item_get_raw_pointer(current_item);
                    system_hashed_ansi_string current_item_origin_filename = nullptr;
                    int                       current_item_origin_line     = -1;

                    object_manager_item_get_origin_details(current_item,
                                                          &current_item_origin_filename,
                                                          &current_item_origin_line);

                    LOG_INFO("Item [%d] (%p): [%s] created at [%s]:[%d]",
                              n_item,
                              current_item_object,
                              system_hashed_ansi_string_get_buffer(current_item_name),
                              system_hashed_ansi_string_get_buffer(current_item_origin_filename),
                              current_item_origin_line);
                }
            }
        }

        result = n_directories + n_items;

        if (n_directories > 0 &&
            _verbose_status)
        {
            object_manager_directory  first_directory      = nullptr;
            system_hashed_ansi_string first_directory_name = nullptr;
            system_hash64             first_directory_hash = 0;

            system_hash64map_get_element_at(directory_ptr->directories,
                                            0,
                                           &first_directory,
                                           &first_directory_hash);

            first_directory_name = object_manager_directory_get_name(first_directory);

            LOG_INFO("1st directory: [%s]",
                     system_hashed_ansi_string_get_buffer(first_directory_name) );
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC uint32_t object_manager_directory_get_amount_of_subdirectories_for_directory(object_manager_directory directory)
{
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);
    uint32_t                   result        = 0;

    system_critical_section_enter(directory_ptr->cs);
    {
        system_hash64map_get_property(directory_ptr->directories,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &result);
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC system_hashed_ansi_string object_manager_directory_get_name(object_manager_directory directory)
{
    return reinterpret_cast<_object_manager_directory*>(directory)->name;
}

/* Please see header for specification */
PUBLIC object_manager_directory object_manager_directory_get_subdirectory_at(object_manager_directory directory,
                                                                             uint32_t                 index)
{
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);
    object_manager_directory   result        = nullptr;
    system_hash64              result_hash   = 0;

    system_critical_section_enter(directory_ptr->cs);
    {
        /* This call could fail */
        system_hash64map_get_element_at(directory_ptr->directories,
                                        index,
                                       &result,
                                       &result_hash);
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_get_subitem_at(object_manager_directory directory,
                                                    uint32_t                 index,
                                                    object_manager_item*     out_item)
{
    _object_manager_directory* directory_ptr   = reinterpret_cast<_object_manager_directory*>(directory);
    system_hash64              item_hash       = 0;
    bool                       result          = false;

    system_critical_section_enter(directory_ptr->cs);
    {
        /* This call could fail */
        result = system_hash64map_get_element_at(directory_ptr->items,
                                                 index,
                                                 out_item,
                                                &item_hash);
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;

}

/* Please see header for specification */
PUBLIC  bool object_manager_directory_insert_item(object_manager_directory directory,
                                                  object_manager_item      item)
{
    _object_manager_directory* directory_ptr  = reinterpret_cast<_object_manager_directory*>(directory);
    system_hashed_ansi_string  item_name      = object_manager_item_get_name      (item);
    system_hash64              item_name_hash = system_hashed_ansi_string_get_hash(item_name);
    bool                       result         = false;

    system_critical_section_enter(directory_ptr->cs);
    {
        if (!system_hash64map_contains(directory_ptr->items,
                                       item_name_hash))
        {
            if (!system_hash64map_insert(directory_ptr->items,
                                         item_name_hash,
                                         item,
                                         nullptr,
                                         nullptr) )
            {
                LOG_ERROR("Could not insert item [%s] into [%s]",
                          system_hashed_ansi_string_get_buffer(item_name),
                          system_hashed_ansi_string_get_buffer(directory_ptr->name) );
            }
            else
            {
                result = true;
            }
        }
        else
        {
            LOG_ERROR("Directory [%s] already contains item [%s]",
                      system_hashed_ansi_string_get_buffer(directory_ptr->name),
                      system_hashed_ansi_string_get_buffer(item_name) );

            /* ASSERT_DEBUG_SYNC(false, ""); */
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC bool object_manager_directory_insert_subdirectory(object_manager_directory directory,
                                                         object_manager_directory subdirectory)
{
    _object_manager_directory* directory_ptr          = reinterpret_cast<_object_manager_directory*>(directory);
    bool                       result                 = false;
    _object_manager_directory* subdirectory_ptr       = reinterpret_cast<_object_manager_directory*>(subdirectory);
    system_hash64              subdirectory_name_hash = system_hashed_ansi_string_get_hash(subdirectory_ptr->name);

    system_critical_section_enter(directory_ptr->cs);
    {
        if (!system_hash64map_contains(directory_ptr->directories,
                                       subdirectory_name_hash))
        {
            if (!system_hash64map_insert(directory_ptr->directories,
                                         subdirectory_name_hash,
                                         subdirectory,
                                         nullptr,
                                         nullptr) )
            {
                LOG_ERROR("Could not insert directory [%s] into [%s]",
                          system_hashed_ansi_string_get_buffer(subdirectory_ptr->name),
                          system_hashed_ansi_string_get_buffer(directory_ptr->name) );
            }
            else
            {
                result = true;
            }
        }
        else
        {
            LOG_ERROR("Directory [%s] already contains subdirectory [%s]",
                      system_hashed_ansi_string_get_buffer(directory_ptr->name),
                      system_hashed_ansi_string_get_buffer(subdirectory_ptr->name) );
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    return result;
}

/* Please see header for specification */
PUBLIC void object_manager_directory_release(object_manager_directory directory)
{
    _object_manager_directory* directory_ptr = reinterpret_cast<_object_manager_directory*>(directory);

    system_critical_section_enter(directory_ptr->cs);
    {
        /* Release all subdirectory objects */
        size_t n_directories = 0;

        system_hash64map_get_property(directory_ptr->directories,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_directories);

        for (size_t n_directory = 0;
                    n_directory < n_directories;
                  ++n_directory)
        {
            object_manager_directory subdirectory           = nullptr;
            system_hash64            subdirectory_name_hash = 0;

            if (system_hash64map_get_element_at(directory_ptr->directories,
                                                n_directory,
                                               &subdirectory,
                                               &subdirectory_name_hash) )
            {
                object_manager_directory_release(subdirectory);
            }
            else
            {
                LOG_ERROR("Could not retrieve %uth sub-directory",
                          (unsigned int) n_directory);
            }
        }
    }
    system_critical_section_leave(directory_ptr->cs);

    /* Finally release the container */
    system_hash64map_release       (directory_ptr->directories);
    system_critical_section_release(directory_ptr->cs);

    delete directory_ptr;
}

/** Please see header for specification  */
PUBLIC void object_manager_directory_set_verbose_mode(bool verbose)
{
    _verbose_status = verbose;
}