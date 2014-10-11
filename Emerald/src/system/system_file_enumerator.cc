/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "system/system_file_enumerator.h"
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
        entries      = system_resizable_vector_create(4, /* capacity */
                                                      sizeof(_system_file_enumerator_entry*) );
        file_pattern = in_file_pattern;
    }

    ~_system_file_enumerator()
    {
        if (entries != NULL)
        {
            _system_file_enumerator_entry* entry_ptr = NULL;

            while (system_resizable_vector_pop(entries, &entry_ptr) )
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

        while (system_resizable_vector_pop(enumerator_ptr->entries, &entry_ptr) )
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
            system_resizable_vector_push(enumerator_ptr->entries, entry_ptr);
        } /* if (file_finder_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) */
    } while (::FindNextFileA(file_finder, &file_finder_data) != 0);

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
PUBLIC EMERALD_API system_hashed_ansi_string system_file_enumerator_choose_file_via_ui(__in __notnull system_hashed_ansi_string filter,
                                                                                       __in __notnull system_hashed_ansi_string filter_name,
                                                                                       __in __notnull system_hashed_ansi_string dialog_title)
{
    char                      buffer[MAX_PATH];
    OPENFILENAME              config;
    const char*               filter_name_raw_ptr    = system_hashed_ansi_string_get_buffer(filter_name);
    unsigned int              filter_name_length     = strlen(filter_name_raw_ptr);
    const char*               filter_raw_ptr         = system_hashed_ansi_string_get_buffer(filter);
    unsigned int              filter_raw_length      = strlen(filter_raw_ptr);
    unsigned int              filter_adapted_length  = filter_name_length + 1 + filter_raw_length + 2;
    char*                     filter_adapted_raw_ptr = new char[filter_adapted_length];
    system_hashed_ansi_string result                 = NULL;

    memset(buffer, 0, sizeof(buffer) );

    ASSERT_ALWAYS_SYNC(filter_adapted_raw_ptr != NULL, "Out of memory");
    if (filter_adapted_raw_ptr != NULL)
    {
        memset(filter_adapted_raw_ptr,                         0,                   filter_adapted_length);
        memcpy(filter_adapted_raw_ptr,                         filter_name_raw_ptr, filter_name_length);
        memcpy(filter_adapted_raw_ptr + filter_name_length + 1,filter_raw_ptr,      filter_raw_length);

        memset(&config, 0, sizeof(config) );

        config.Flags           = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        config.lStructSize     = sizeof(config);
        config.lpstrFile       = buffer;
        config.lpstrFilter     = filter_adapted_raw_ptr;
        config.lpstrInitialDir = ".";
        config.lpstrTitle      = system_hashed_ansi_string_get_buffer(dialog_title);
        config.nMaxFile        = MAX_PATH;

        /* Show the dialog */
        BOOL execution_result = ::GetOpenFileName(&config);

        delete [] filter_adapted_raw_ptr;
        filter_adapted_raw_ptr = NULL;

        if (execution_result == FALSE)
        {
            goto end;
        }

        result = system_hashed_ansi_string_create(buffer);
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
PUBLIC EMERALD_API void system_file_enumerator_release(__in __notnull system_file_enumerator enumerator)
{
    if (enumerator != NULL)
    {
        _system_file_enumerator* enumerator_ptr = (_system_file_enumerator*) enumerator;

        delete enumerator_ptr;
        enumerator_ptr = NULL;
    }
}