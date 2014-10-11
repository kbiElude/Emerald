/**
 *
 * Emerald (kbi/elude @2014)
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
    data_float_array = NULL;
    data_name_array  = NULL;
    id               = system_hashed_ansi_string_get_default_empty_string();
}


/** TODO */
_collada_data_source::~_collada_data_source()
{
    if (data_float_array != NULL)
    {
        collada_data_float_array_release(data_float_array);

        data_float_array = NULL;
    }

    if (data_name_array != NULL)
    {
        collada_data_name_array_release(data_name_array);

        data_name_array = NULL;
    }
}


/** TODO */
PUBLIC collada_data_source collada_data_source_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                      __in __notnull collada_data              data,
                                                      __in __notnull system_hashed_ansi_string parent_geometry_name)
{
    _collada_data_source* result_source_ptr = new (std::nothrow) _collada_data_source;

    ASSERT_ALWAYS_SYNC(result_source_ptr != NULL, "Out of memory");
    if (result_source_ptr != NULL)
    {
        const char* source_element_id = element_ptr->Attribute("id");

        if (source_element_id == NULL)
        {
            source_element_id = "[unknown]";
        }

        /* Is it a float array? */
        unsigned int          array_count             = 0;
        tinyxml2::XMLElement* float_array_element_ptr = NULL;
        tinyxml2::XMLElement* name_array_element_ptr  = NULL;

        float_array_element_ptr = element_ptr->FirstChildElement("float_array");
        name_array_element_ptr  = element_ptr->FirstChildElement("Name_array");

        if (float_array_element_ptr != NULL)
        {
            array_count = float_array_element_ptr->IntAttribute("count");

            ASSERT_DEBUG_SYNC(array_count != 0,
                              "count attribute of <float_array> is 0");
        }
        else
        if (name_array_element_ptr != NULL)
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

        if (technique_common_element_ptr == NULL)
        {
            LOG_FATAL        ("Source [%s] is not defined by a technique_common which is not supported", source_element_id);
            ASSERT_DEBUG_SYNC(false, "technique_common sub-node missing");

            goto end;
        }
        else
        {
            /* There should be a 'accessor' sub-node here. */
            tinyxml2::XMLElement* accessor_element_ptr = technique_common_element_ptr->FirstChildElement("accessor");

            if (accessor_element_ptr == NULL)
            {
                LOG_FATAL        ("Accessor sub-node not defined for a <source>/<technique_common>/<accessor> path which is invalid.");
                ASSERT_DEBUG_SYNC(false, "<accessor> sub-node missing");

                goto end;
            }

            accessor_count  = accessor_element_ptr->IntAttribute("count");
            accessor_stride = accessor_element_ptr->IntAttribute("stride");

            /* Sanity checks to guard us against use cases that we currently do not support. */
            ASSERT_DEBUG_SYNC(accessor_count  != 0, "count attribute not defined for a <accessor> node");
            ASSERT_DEBUG_SYNC(accessor_stride != 0, "stride attribute not defined for a <accessor> node");

            ASSERT_DEBUG_SYNC((array_count     % accessor_stride) == 0,           "Unsupported stride configuration");
            ASSERT_DEBUG_SYNC((accessor_stride * accessor_count)  == array_count, "Invalid count attribute value for a <float_array> node");

            /* Good to assume stride = n_components */
            accessor_n_components = accessor_stride;
        }

        /* Parse the array and fill the descriptor */
        if (float_array_element_ptr != NULL)
        {
            result_source_ptr->data_float_array = collada_data_float_array_create(float_array_element_ptr,
                                                                                  accessor_n_components,
                                                                                  accessor_stride,
                                                                                  data,
                                                                                  parent_geometry_name);
        }
        else
        if (name_array_element_ptr != NULL)
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
PUBLIC void collada_data_source_get_property(__in  __notnull collada_data_source          source,
                                             __in            collada_data_source_property property,
                                             __out __notnull void*                        result_ptr)
{
    _collada_data_source* source_ptr = (_collada_data_source*) source;

    switch (property)
    {
        case COLLADA_DATA_SOURCE_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) result_ptr) = source_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA source property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC void collada_data_source_get_source_float_data(__in      __notnull collada_data_source       source,
                                                      __out_opt           collada_data_float_array* out_float_array_ptr)
{
    _collada_data_source* source_ptr = (_collada_data_source*) source;

    if (out_float_array_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(source_ptr->data_float_array != NULL,
                          "Requested float array is NULL");

        *out_float_array_ptr = source_ptr->data_float_array;
    }
}

/* Please see header for spec */
PUBLIC void collada_data_source_get_source_name_data(__in      __notnull collada_data_source      source,
                                                     __out_opt           collada_data_name_array* out_name_array_ptr)
{
    _collada_data_source* source_ptr = (_collada_data_source*) source;

    if (out_name_array_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(source_ptr->data_name_array != NULL,
                          "Requested name array is NULL");

        *out_name_array_ptr = source_ptr->data_name_array;
    }
}

/** Please see header for a spec */
PUBLIC void collada_data_source_release(__in __notnull __post_invalid collada_data_source source)
{
    _collada_data_source* source_ptr = (_collada_data_source*) source;

    delete source_ptr;
    source_ptr = NULL;
}
