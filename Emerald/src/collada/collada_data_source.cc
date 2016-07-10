/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data_float_array.h"
#include "collada/collada_data_name_array.h"
#include "collada/collada_data_source.h"
#include "system/system_assertions.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <source> node */
typedef struct _collada_data_source
{
    /* So, we're doing a gross over-simplification here. <source> can define one
     * of the following data array types:
     *
     * <IDREF_array>
     * <Name_array>
     * <bool_array>
     * <float_array>
     * <int_array>
     *
     * but we're assuming either floats or names will only be used.
     * Implement support for the remaining types when necessary.
     **/
    collada_data_float_array  data_float_array;
    collada_data_name_array   data_name_array;

    system_hashed_ansi_string id;

     _collada_data_source();
    ~_collada_data_source();
} _collada_data_source;


/** TODO */
_collada_data_source::_collada_data_source()
{
    data_float_array = nullptr;
    data_name_array  = nullptr;
    id               = system_hashed_ansi_string_get_default_empty_string();
}


/** TODO */
_collada_data_source::~_collada_data_source()
{
    if (data_float_array != nullptr)
    {
        collada_data_float_array_release(data_float_array);

        data_float_array = nullptr;
    }

    if (data_name_array != nullptr)
    {
        collada_data_name_array_release(data_name_array);

        data_name_array = nullptr;
    }
}


/** TODO */
PUBLIC collada_data_source collada_data_source_create(tinyxml2::XMLElement*     element_ptr,
                                                      collada_data              data,
                                                      system_hashed_ansi_string parent_geometry_name)
{
    _collada_data_source* result_source_ptr = new (std::nothrow) _collada_data_source;

    ASSERT_ALWAYS_SYNC(result_source_ptr != nullptr,
                       "Out of memory");

    if (result_source_ptr != nullptr)
    {
        const char* source_element_id = element_ptr->Attribute("id");

        if (source_element_id == nullptr)
        {
            source_element_id = "[unknown]";
        }

        /* Is it a float array? */
        unsigned int          array_count             = 0;
        tinyxml2::XMLElement* float_array_element_ptr = nullptr;
        tinyxml2::XMLElement* name_array_element_ptr  = nullptr;

        float_array_element_ptr = element_ptr->FirstChildElement("float_array");
        name_array_element_ptr  = element_ptr->FirstChildElement("Name_array");

        if (float_array_element_ptr != nullptr)
        {
            array_count = float_array_element_ptr->IntAttribute("count");

            ASSERT_DEBUG_SYNC(array_count != 0,
                              "count attribute of <float_array> is 0");
        }
        else
        if (name_array_element_ptr != nullptr)
        {
            /* Is it a name array? */
            array_count = name_array_element_ptr->IntAttribute("count");

            ASSERT_DEBUG_SYNC(array_count != 0,
                              "count attribute of <name_array> is 0");
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "<source> node uses an unsupported array type.");
        }

        /* Determine amount of components used by float array. It is a gross over-simplification
         * to assume specific component order, but. */
        unsigned int          accessor_count               = 0;
        unsigned int          accessor_n_components        = 0;
        unsigned int          accessor_stride              = 0;
        tinyxml2::XMLElement* technique_common_element_ptr = element_ptr->FirstChildElement("technique_common");

        if (technique_common_element_ptr == nullptr)
        {
            LOG_FATAL("Source [%s] is not defined by a technique_common which is not supported",
                      source_element_id);

            ASSERT_DEBUG_SYNC(false,
                              "technique_common sub-node missing");

            goto end;
        }
        else
        {
            /* There should be a 'accessor' sub-node here. */
            tinyxml2::XMLElement* accessor_element_ptr = technique_common_element_ptr->FirstChildElement("accessor");

            if (accessor_element_ptr == nullptr)
            {
                LOG_FATAL("Accessor sub-node not defined for a <source>/<technique_common>/<accessor> path which is invalid.");

                ASSERT_DEBUG_SYNC(false,
                                  "<accessor> sub-node missing");

                goto end;
            }

            accessor_count  = accessor_element_ptr->IntAttribute("count");
            accessor_stride = accessor_element_ptr->IntAttribute("stride");

            /* Sanity checks to guard us against use cases that we currently do not support. */
            ASSERT_DEBUG_SYNC(accessor_count != 0,
                              "count attribute not defined for a <accessor> node");
            ASSERT_DEBUG_SYNC(accessor_stride != 0,
                              "stride attribute not defined for a <accessor> node");

            ASSERT_DEBUG_SYNC((array_count % accessor_stride) == 0,
                              "Unsupported stride configuration");
            ASSERT_DEBUG_SYNC((accessor_stride * accessor_count) == array_count,
                              "Invalid count attribute value for a <float_array> node");

            /* Good to assume stride = n_components */
            accessor_n_components = accessor_stride;
        }

        /* Parse the array and fill the descriptor */
        if (float_array_element_ptr != nullptr)
        {
            result_source_ptr->data_float_array = collada_data_float_array_create(float_array_element_ptr,
                                                                                  accessor_n_components,
                                                                                  accessor_stride,
                                                                                  data,
                                                                                  parent_geometry_name);
        }
        else
        if (name_array_element_ptr != nullptr)
        {
            result_source_ptr->data_name_array = collada_data_name_array_create(name_array_element_ptr);
        }

        /* Store the name */
        result_source_ptr->id = system_hashed_ansi_string_create(source_element_id);

        /* Done */
    }

end:
    return (collada_data_source) result_source_ptr;
}

/** Please see header for spec */
PUBLIC void collada_data_source_get_property(collada_data_source          source,
                                             collada_data_source_property property,
                                             void*                        result_ptr)
{
    _collada_data_source* source_ptr = reinterpret_cast<_collada_data_source*>(source);

    switch (property)
    {
        case COLLADA_DATA_SOURCE_PROPERTY_ID:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(result_ptr) = source_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA source property");
        }
    }
}

/* Please see header for specification */
PUBLIC void collada_data_source_get_source_float_data(collada_data_source       source,
                                                      collada_data_float_array* out_float_array_ptr)
{
    _collada_data_source* source_ptr = reinterpret_cast<_collada_data_source*>(source);

    if (out_float_array_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(source_ptr->data_float_array != nullptr,
                          "Requested float array is nullptr");

        *out_float_array_ptr = source_ptr->data_float_array;
    }
}

/* Please see header for spec */
PUBLIC void collada_data_source_get_source_name_data(collada_data_source      source,
                                                     collada_data_name_array* out_name_array_ptr)
{
    _collada_data_source* source_ptr = reinterpret_cast<_collada_data_source*>(source);

    if (out_name_array_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(source_ptr->data_name_array != nullptr,
                          "Requested name array is nullptr");

        *out_name_array_ptr = source_ptr->data_name_array;
    }
}

/** Please see header for a spec */
PUBLIC void collada_data_source_release(collada_data_source source)
{
    _collada_data_source* source_ptr = reinterpret_cast<_collada_data_source*>(source);

    delete source_ptr;
    source_ptr = nullptr;
}
