/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "system/system_assertions.h"
#include "system/system_critical_section.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_hashed_ansi_string.h"

/* Internal type definitions */
typedef struct 
{
    char*         contents;
    system_hash64 hash;
    size_t        length;
} _system_hashed_ansi_string_descriptor;

/* Internal variables */
system_hash64map          _dictionary    = NULL;
system_critical_section   _dictionary_cs = system_critical_section_create();
system_hashed_ansi_string _empty_string  = NULL;

/* Internal functions */
/** TODO */
PRIVATE void _init_system_hashed_ansi_string_descriptor(__in __notnull const char* text, __out _system_hashed_ansi_string_descriptor* descriptor)
{
    size_t length = strlen(text);

    descriptor->contents         = new char[length + 1];
    descriptor->contents[length] = 0;

    memcpy(descriptor->contents, text, length);
    
    descriptor->length = length;
    descriptor->hash   = system_hash64_calculate(text, (uint32_t) length);
}

/** TODO */
PRIVATE VOID _deinit_system_hashed_ansi_string_descriptor(__in __notnull _system_hashed_ansi_string_descriptor* descriptor)
{
    delete [] descriptor->contents;
    delete descriptor;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hashed_ansi_string_contains(__in __notnull system_hashed_ansi_string has_1, __in __notnull system_hashed_ansi_string has_2)
{
    _system_hashed_ansi_string_descriptor* descriptor_1 = (_system_hashed_ansi_string_descriptor*) has_1;
    _system_hashed_ansi_string_descriptor* descriptor_2 = (_system_hashed_ansi_string_descriptor*) has_2;

    if (strstr(descriptor_1->contents, descriptor_2->contents) == 0)
    {
        return false;
    }
    else
    {
        return true;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_hashed_ansi_string_create(__in __notnull const char* raw)
{
    system_hashed_ansi_string result = NULL;

    system_critical_section_enter(_dictionary_cs);
    {
        if (_dictionary == NULL)
        {
            system_hashed_ansi_string_init();
        };

        if (raw == NULL)
        {
            result = NULL;

            goto end;
        }

        if (_dictionary != NULL)
        {
            /* Check if the entry has not already been created. */
            void*                     found_entry = 0;
            system_hash64             hash        = system_hash64_calculate(raw, (uint32_t) strlen(raw) );

            if (system_hash64map_get(_dictionary, hash, &found_entry) )
            {
                result = (system_hashed_ansi_string) found_entry;
            }
            else
            {
                _system_hashed_ansi_string_descriptor* new_descriptor = new _system_hashed_ansi_string_descriptor;

                _init_system_hashed_ansi_string_descriptor(raw, new_descriptor);
                system_hash64map_insert                   (_dictionary, hash, new_descriptor, 0, 0);

                result = (system_hashed_ansi_string) new_descriptor;
            }
        }
    }
end:
    system_critical_section_leave(_dictionary_cs);

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_hashed_ansi_string_create_by_merging_strings(uint32_t n_strings, __in __notnull const char** strings)
{
    if (_dictionary == NULL)
    {
        system_hashed_ansi_string_init();
    };

    ASSERT_DEBUG_SYNC(_dictionary != NULL, "Dictionary not initialized.");

    /* Sum length of all strings*/
    size_t total_length = 0;

    for (uint32_t n_string = 0; n_string < n_strings; ++n_string)
    {
        total_length += strlen(strings[n_string]);
    }

    /* Allocate storage space */
    system_hashed_ansi_string result     = NULL;
    char*                     new_string = new (std::nothrow) char[total_length+1];

    ASSERT_DEBUG_SYNC(new_string != NULL, "Out of memory while allocating temporary space for merged stngs.");
    if (new_string != NULL)
    {
        char* traveller_ptr = new_string;

        for (uint32_t n_string = 0; n_string < n_strings; ++n_string)
        {
            size_t string_length = strlen(strings[n_string]);

            memcpy(traveller_ptr, strings[n_string], string_length);
            traveller_ptr += string_length;
        }

        new_string[total_length] = 0;

        /* Construct the hashed ansi string, now that the string is ready */
        result = system_hashed_ansi_string_create(new_string);

        /* Free the storage */
        delete [] new_string;
        new_string = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_hashed_ansi_string_create_by_merging_two_strings(__in __notnull const char* src1, __in __notnull const char* src2)
{
    if (_dictionary == NULL)
    {
        system_hashed_ansi_string_init();
    };

    ASSERT_DEBUG_SYNC(_dictionary != NULL, "Dictionary not initialized.");

    /* Go for it */
    size_t                    src1_length = strlen(src1);
    size_t                    src2_length = strlen(src2);
    char*                     new_src     = new (std::nothrow) char[src1_length + src2_length + 1];
    system_hashed_ansi_string result      = NULL;

    ASSERT_DEBUG_SYNC(new_src != NULL, "Out of memory while allocating temporary space for merged string.");
    if (new_src != NULL)
    {
        memcpy(new_src,               src1, src1_length);
        memcpy(new_src + src1_length, src2, src2_length);

        new_src[src1_length + src2_length] = 0;
        result                             = system_hashed_ansi_string_create(new_src);

        delete [] new_src;
        new_src = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_hashed_ansi_string_create_substring(__in __notnull const char* string,
                                                                                        __in           uint32_t    start_offset,
                                                                                        __in           uint32_t    length)
{
    uint32_t                  original_length = strlen(string);
    char*                     buffer          = (char*) malloc(length + 1);
    system_hashed_ansi_string result          = NULL;

    if (buffer != NULL)
    {
        memcpy(buffer, string + start_offset, length);

        buffer[length] = 0;
        result         = system_hashed_ansi_string_create(buffer);

        free(buffer);
    } /* if (buffer != NULL) */

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string system_hashed_ansi_string_get_default_empty_string()
{
    ASSERT_DEBUG_SYNC(_empty_string != NULL, "Empty string is null");

    return _empty_string;
}

/** Please see header for specification */
PUBLIC EMERALD_API uint32_t system_hashed_ansi_string_get_length(__in __notnull system_hashed_ansi_string string)
{
    return (uint32_t) ((_system_hashed_ansi_string_descriptor*) string)->length;
}

/** Please see header for specification */
PUBLIC EMERALD_API const char* system_hashed_ansi_string_get_buffer(__in __notnull system_hashed_ansi_string string)
{
    return ((_system_hashed_ansi_string_descriptor*) string)->contents;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hashed_ansi_string_is_equal_to_raw_string(__in __notnull system_hashed_ansi_string string, __in __notnull const char* raw_text)
{
    _system_hashed_ansi_string_descriptor* descriptor = (_system_hashed_ansi_string_descriptor*) string;

    if (descriptor->hash != system_hash64_calculate(raw_text, (uint32_t) strlen(raw_text) ) )
    {
        return false;
    }
    else
    {
        return (strcmp(descriptor->contents, raw_text) == 0);
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hash64 system_hashed_ansi_string_get_hash(__in __notnull system_hashed_ansi_string string)
{
    return ((_system_hashed_ansi_string_descriptor*) string)->hash;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_hashed_ansi_string_is_equal_to_hash_string(__in __notnull system_hashed_ansi_string has_1, __in __notnull system_hashed_ansi_string has_2)
{
    _system_hashed_ansi_string_descriptor* has_1_descriptor = (_system_hashed_ansi_string_descriptor*) has_1;
    _system_hashed_ansi_string_descriptor* has_2_descriptor = (_system_hashed_ansi_string_descriptor*) has_2;
    bool                                   result           = false;

    if (has_1_descriptor == has_2_descriptor)
    {
        result = true;
    }
    else
    if (has_1_descriptor->hash != has_2_descriptor->hash)
    {
        result = false;
    }
    else
    if (has_1_descriptor->length != has_2_descriptor->length)
    {
        result = false;
    }
    else
    {
        result = (strcmp(has_1_descriptor->contents, has_2_descriptor->contents) == 0);
    }

    return result;
}

/** Please see header for specification */
PUBLIC void system_hashed_ansi_string_init()
{
    _dictionary   = system_hash64map_create         (sizeof(_system_hashed_ansi_string_descriptor*),
                                                     true); /* should_be_thread_safe */
    _empty_string = system_hashed_ansi_string_create("");
}

/** Please see header for specification */
PUBLIC void system_hashed_ansi_string_deinit()
{
    ASSERT_DEBUG_SYNC(_dictionary   != NULL, "Dictionary not initialized");
    ASSERT_DEBUG_SYNC(_empty_string != NULL, "Empty string not initialized.");

    if (_dictionary != NULL)
    {
        system_hash64map_release(_dictionary);
        _dictionary = NULL;
    }

    if (_dictionary_cs != NULL)
    {
        system_critical_section_release(_dictionary_cs);
    }

    /* TODO: Leaking dictionary contents here. Oh well */
}
