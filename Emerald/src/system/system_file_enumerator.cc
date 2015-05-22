/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_unpacker.h"
#include "system/system_resizable_vector.h"
#include "commdlg.h"

typedef struct
{
    /* This structure could include more properties like file size in the future */

    system_hashed_ansi_string file_name;

} _system_file_enumerator_entry;

typedef struct _system_file_enumerator
{
    system_resizable_vector   entries;
    system_hashed_ansi_string file_pattern;

    explicit _system_file_enumerator(system_hashed_ansi_string in_file_pattern)
    {
        entries      = system_resizable_vector_create(4 /* capacity */);
        file_pattern = in_file_pattern;
    }

    ~_system_file_enumerator()
    {
        if (entries != NULL)
        {
            _system_file_enumerator_entry* entry_ptr = NULL;

            while (system_resizable_vector_pop(entries,
                                              &entry_ptr) )
            {
                delete entry_ptr;

                entry_ptr = NULL;
            }
        }
    }
} _system_file_enumerator;


/** TODO */
PRIVATE bool _system_file_enumerator_fill(__in __notnull _system_file_enumerator* enumerator_ptr)
{
    HANDLE file_finder = INVALID_HANDLE_VALUE;
    bool   result      = true;

    /* Release any data that may be in the enumartor */
    {
        _system_file_enumerator_entry* entry_ptr   = NULL;

        while (system_resizable_vector_pop(enumerator_ptr->entries,
                                          &entry_ptr) )
        {
            delete entry_ptr;

            entry_ptr = NULL;
        }
    }

    /* Initialize API file finder */
    WIN32_FIND_DATA file_finder_data = {0};

    file_finder = ::FindFirstFileA(system_hashed_ansi_string_get_buffer(enumerator_ptr->file_pattern),
                                  &file_finder_data);

    if (file_finder == INVALID_HANDLE_VALUE)
    {
        result = false;

        goto end;
    }

    /* Iterate over all available files */
    do
    {
        if (file_finder_data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE)
        {
            /* Spawn the entry descriptor */
            _system_file_enumerator_entry* entry_ptr = new (std::nothrow) _system_file_enumerator_entry;

            ASSERT_ALWAYS_SYNC(entry_ptr != NULL,
                               "Out of memory");

            if (entry_ptr == NULL)
            {
                result = false;

                goto end;
            }

            entry_ptr->file_name = system_hashed_ansi_string_create(file_finder_data.cFileName);

            /* Stash it */
            system_resizable_vector_push(enumerator_ptr->entries,
                                         entry_ptr);
        } /* if (file_finder_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) */
    } while (::FindNextFileA(file_finder,
                            &file_finder_data) != 0);

    result = true;
end:
    if (file_finder != INVALID_HANDLE_VALUE)
    {
        ::FindClose(file_finder);

        file_finder = NULL;
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API system_hashed_ansi_string system_file_enumerator_choose_file_via_ui(__in                                   system_file_enumerator_file_operation operation,
                                                                                       __in                                   unsigned int                          n_filters,
                                                                                       __in_ecount(n_filters) __notnull const system_hashed_ansi_string*            filter_descriptions,
                                                                                       __in_ecount(n_filters) __notnull const system_hashed_ansi_string*            filter_extensions,
                                                                                       __in                   __notnull       system_hashed_ansi_string             dialog_title)
{
    char                      buffer[MAX_PATH];
    OPENFILENAME              config;
    system_hashed_ansi_string result = NULL;

    memset(buffer,
           0,
           sizeof(buffer) );

    /* Construct filter string: Count number of characters we will need for the lpstrFilter string */
    char*        filter_adapted_raw_ptr = NULL;
    unsigned int n_filter_characters    = 1; /* final terminator at the end */

    for (unsigned int n_filter = 0;
                      n_filter < n_filters;
                    ++n_filter)
    {
        n_filter_characters += system_hashed_ansi_string_get_length(filter_descriptions[n_filter]) + 1; /* terminator */
        n_filter_characters += system_hashed_ansi_string_get_length(filter_extensions  [n_filter]) + 1; /* terminator */
    } /* for (all filters) */

    filter_adapted_raw_ptr = new (std::nothrow) char[n_filter_characters];

    ASSERT_DEBUG_SYNC(filter_adapted_raw_ptr != NULL,
                      "Out of memory");

    if (filter_adapted_raw_ptr != NULL)
    {
        /* Construct filter string */
        char* traveller_ptr = filter_adapted_raw_ptr;

        memset(filter_adapted_raw_ptr,
               0,
               n_filter_characters);

        for (unsigned int n_filter = 0;
                          n_filter < n_filters;
                        ++n_filter)
        {
            const unsigned int filter_description_length = system_hashed_ansi_string_get_length(filter_descriptions[n_filter]);
            const unsigned int filter_extension_length   = system_hashed_ansi_string_get_length(filter_extensions  [n_filter]);

            memcpy(traveller_ptr,
                   system_hashed_ansi_string_get_buffer(filter_descriptions[n_filter]),
                   filter_description_length);

              traveller_ptr += filter_description_length;
            ++traveller_ptr; /* terminator */

            memcpy(traveller_ptr,
                   system_hashed_ansi_string_get_buffer(filter_extensions[n_filter]),
                   filter_extension_length);

              traveller_ptr += filter_extension_length;
            ++traveller_ptr; /* terminator */
        } /* for (all filters) */

        /* Set up the OPENFILE descriptor */
        memset(&config,
               0,
               sizeof(config) );

        config.Flags           = ((operation == SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD) ? OFN_FILEMUSTEXIST : 0) | OFN_PATHMUSTEXIST;
        config.lStructSize     = sizeof(config);
        config.lpstrFile       = buffer;
        config.lpstrFilter     = filter_adapted_raw_ptr;
        config.lpstrInitialDir = ".";
        config.lpstrTitle      = system_hashed_ansi_string_get_buffer(dialog_title);
        config.nMaxFile        = MAX_PATH;

        /* Show the dialog */
        BOOL execution_result = FALSE;

        if (operation == SYSTEM_FILE_ENUMERATOR_FILE_OPERATION_LOAD)
        {
            execution_result = ::GetOpenFileName(&config);
        }
        else
        {
            execution_result = ::GetSaveFileName(&config);
        }

        delete [] filter_adapted_raw_ptr;
        filter_adapted_raw_ptr = NULL;

        if (execution_result == FALSE)
        {
            goto end;
        }

        /* WinAPI does not seem to append the selected extension. If that's the case, make sure to append it
         * when creating the result value */
        const char* selected_filter_extension_raw_ptr = NULL;

        ASSERT_DEBUG_SYNC((config.nFilterIndex - 1) < n_filters,
                          "Invalid filter index reported by WinAPI");

        selected_filter_extension_raw_ptr = system_hashed_ansi_string_get_buffer(filter_extensions[config.nFilterIndex - 1]);

        if (strstr(buffer,
                   selected_filter_extension_raw_ptr + 1 /* wildcard */) == NULL)
        {
            ASSERT_DEBUG_SYNC(selected_filter_extension_raw_ptr[0] == '*',
                              "Invalid first character in the filter extension");

            const char* file_name_parts[] =
            {
                buffer,
                selected_filter_extension_raw_ptr + 1, /* skip the wildcard character */
            };
            const unsigned int n_file_name_parts = sizeof(file_name_parts) / sizeof(file_name_parts[0]);

            result = system_hashed_ansi_string_create_by_merging_strings(n_file_name_parts,
                                                                         file_name_parts);
        }
        else
        {
            result = system_hashed_ansi_string_create(buffer);
        }
    }

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API system_file_enumerator system_file_enumerator_create(__in __notnull system_hashed_ansi_string file_pattern)
{
    system_file_enumerator result = NULL;

    /* Spawn the descriptor */
    _system_file_enumerator* file_enumerator_ptr = new (std::nothrow) _system_file_enumerator(file_pattern);

    ASSERT_ALWAYS_SYNC(file_enumerator_ptr != NULL,
                       "Out of memory");

    if (file_enumerator_ptr == NULL)
    {
        goto end;
    }

    /* Fill the descriptor with entries */
    if (!_system_file_enumerator_fill(file_enumerator_ptr) )
    {
        system_file_enumerator_release( (system_file_enumerator) file_enumerator_ptr);

        goto end;
    }

    /* Done */
    result = (system_file_enumerator) file_enumerator_ptr;
end:
    /* All done */
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_enumerator_get_file_property(__in  __notnull system_file_enumerator               enumerator,
                                                                 __in            uint32_t                             n_file,
                                                                 __in            system_file_enumerator_file_property property,
                                                                 __out __notnull void*                                out_result)
{
    _system_file_enumerator*       enumerator_ptr = (_system_file_enumerator*) enumerator;
    _system_file_enumerator_entry* entry_ptr      = NULL;

    if (system_resizable_vector_get_element_at(enumerator_ptr->entries,
                                               n_file,
                                              &entry_ptr) )
    {
        switch (property)
        {
            case SYSTEM_FILE_ENUMERATOR_FILE_PROPERTY_FILE_NAME:
            {
                *(system_hashed_ansi_string*) out_result = entry_ptr->file_name;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized system_file_enumerator_file_property value");
            }
        } /* switch (property) */
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find descriptor for file entry at index [%d]",
                          n_file);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_enumerator_get_property(__in  __notnull system_file_enumerator          enumerator,
                                                            __in            system_file_enumerator_property property,
                                                            __out __notnull void*                           out_result)
{
    _system_file_enumerator* enumerator_ptr = (_system_file_enumerator*) enumerator;

    switch (property)
    {
        case SYSTEM_FILE_ENUMERATOR_PROPERTY_N_FILES:
        {
            *(uint32_t*) out_result = system_resizable_vector_get_amount_of_elements(enumerator_ptr->entries);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_file_enumerator_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC EMERALD_API bool system_file_enumerator_is_file_present(__in __notnull system_hashed_ansi_string file_name)
{
    HANDLE file_handle = ::CreateFileA(system_hashed_ansi_string_get_buffer(file_name),
                                       GENERIC_READ,
                                       FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
                                       NULL, /* no security attributes */
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL); /* no template file */
    bool   result      = false;

    if (file_handle != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(file_handle);

        result = true;
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool system_file_enumerator_is_file_present_in_system_file_unpacker(__in __notnull system_file_unpacker      file_unpacker,
                                                                                       __in __notnull system_hashed_ansi_string file_name,
                                                                                       __in           bool                      use_exact_match,
                                                                                       __out_opt      unsigned int*             out_file_index)
{
    unsigned int n_packed_files = 0;
    bool         result         = false;

    system_file_unpacker_get_property(file_unpacker,
                                      SYSTEM_FILE_UNPACKER_PROPERTY_N_OF_EMBEDDED_FILES,
                                     &n_packed_files);

    for (unsigned int n_packed_file = 0;
                      n_packed_file < n_packed_files;
                    ++n_packed_file)
    {
        system_hashed_ansi_string current_file_name = NULL;

        system_file_unpacker_get_file_property(file_unpacker,
                                               n_packed_file,
                                               SYSTEM_FILE_UNPACKER_FILE_PROPERTY_NAME,
                                              &current_file_name);

        if ( use_exact_match && system_hashed_ansi_string_is_equal_to_hash_string(current_file_name,
                                                                                  file_name)        ||
            !use_exact_match && system_hashed_ansi_string_contains               (current_file_name,
                                                                                  file_name) )
        {
            /* OK, located the file! */
            if (out_file_index != NULL)
            {
                *out_file_index = n_packed_file;
            }

            result = true;
            break;
        }
    } /* for (all packed files) */

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_enumerator_release(__in __notnull system_file_enumerator enumerator)
{
    if (enumerator != NULL)
    {
        _system_file_enumerator* enumerator_ptr = (_system_file_enumerator*) enumerator;

        delete enumerator_ptr;
        enumerator_ptr = NULL;
    }
}