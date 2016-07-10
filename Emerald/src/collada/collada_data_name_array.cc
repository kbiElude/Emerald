/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_name_array.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in a <Name_array> */
typedef struct _collada_data_name_array
{
    system_resizable_vector strings;

     _collada_data_name_array();
    ~_collada_data_name_array();
} _collada_data_name_array;


/** TODO */
_collada_data_name_array::_collada_data_name_array()
{
    strings = system_resizable_vector_create(4 /* capacity */);
}

/* TODO */
_collada_data_name_array::~_collada_data_name_array()
{
    if (strings != nullptr)
    {
        system_resizable_vector_release(strings);

        strings = nullptr;
    }
}


/** TODO */
PUBLIC collada_data_name_array collada_data_name_array_create(tinyxml2::XMLElement* name_array_element_ptr)
{
    _collada_data_name_array* result_ptr = new (std::nothrow) _collada_data_name_array;

    ASSERT_ALWAYS_SYNC(result_ptr != nullptr,
                       "Out of memory");

    if (result_ptr != nullptr)
    {
        unsigned int count = name_array_element_ptr->UnsignedAttribute("count");
        const char*  data  = name_array_element_ptr->GetText          ();
        const char*  id    = name_array_element_ptr->Attribute        ("id");

        /* Sanity checks */
        if (id == nullptr)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Name array ID is undefined");

            goto end;
        }

        if (data == nullptr)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Null value reported for name_array value");

            goto end;
        }

        if (count == 0)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Zero count encountered in a name_array - this is invalid");

            goto end;
        }

        /* Extract the values */
        unsigned int n_value       = 0;
        const char*  traveller_ptr = data;

        while (n_value != count)
        {
            const char* traveller_space_ptr = strchr(traveller_ptr, ' ');

            ASSERT_DEBUG_SYNC(traveller_space_ptr != nullptr,
                              "Could not find a space character");

            /* Spawn the entry */
            system_hashed_ansi_string new_entry = system_hashed_ansi_string_create_substring(traveller_ptr,
                                                                                             0,
                                                                                             traveller_space_ptr - traveller_ptr);

            system_resizable_vector_push(result_ptr->strings,
                                         new_entry);

            /* Move on */
            traveller_ptr = traveller_space_ptr + 1;
            n_value      ++;
        }
    }

end:
    return (collada_data_name_array) result_ptr;
}

/** Please see header for spec */
PUBLIC void collada_data_name_array_get_property(collada_data_name_array          array,
                                                 collada_data_name_array_property property,
                                                 void*                            out_result_ptr)
{
    _collada_data_name_array* array_ptr = reinterpret_cast<_collada_data_name_array*>(array);

    switch (property)
    {
        case COLLADA_DATA_NAME_ARRAY_PROPERTY_N_VALUES:
        {
            unsigned int n_strings = 0;

            system_resizable_vector_get_property(array_ptr->strings,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_strings);

            *reinterpret_cast<uint32_t*>(out_result_ptr) = n_strings;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_name_array_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC system_hashed_ansi_string collada_data_name_array_get_value_at_index(collada_data_name_array array,
                                                                            uint32_t                index)
{
    _collada_data_name_array* array_ptr = (_collada_data_name_array*) array;
    system_hashed_ansi_string result    = nullptr;

    system_resizable_vector_get_element_at(array_ptr->strings,
                                           index,
                                          &result);

    return result;
}

/** Please see header for spec */
PUBLIC void collada_data_name_array_release(collada_data_name_array array)
{
    delete reinterpret_cast<_collada_data_name_array*>(array);

    array = nullptr;
}