/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_float_array.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_text.h"

/** Describes data stored in a <float_array> */
typedef struct _collada_data_float_array
{
    unsigned int count;
    float*       data;
    bool         has_data_been_rearranged_for_collada_up_axis;
    unsigned int n_components;
    unsigned int stride;

     _collada_data_float_array();
    ~_collada_data_float_array();
} _collada_data_float_array;


/** TODO */
_collada_data_float_array::_collada_data_float_array()
{
    count        = 0;
    data         = NULL;
    n_components = 0;
    stride       = 0;

    has_data_been_rearranged_for_collada_up_axis = false;
}

/* TODO */
_collada_data_float_array::~_collada_data_float_array()
{
    if (data != NULL)
    {
        delete [] data;

        data = NULL;
    }
}


/** TODO */
PRIVATE system_hashed_ansi_string _collada_data_float_array_get_cached_blob_file_name(system_hashed_ansi_string float_array_id,
                                                                                      unsigned int              count,
                                                                                      system_hashed_ansi_string parent_source_name)
{
    char temp_file_name[1024];

    snprintf(temp_file_name,
             sizeof(temp_file_name),
             "collada_float_array_%s_%s_%u",
             system_hashed_ansi_string_get_buffer(parent_source_name),
             system_hashed_ansi_string_get_buffer(float_array_id),
             count);

    return system_hashed_ansi_string_create(temp_file_name);
}

/** TODO */
PUBLIC collada_data_float_array collada_data_float_array_create(tinyxml2::XMLElement*     float_array_element_ptr,
                                                                unsigned int              n_components,
                                                                unsigned int              stride,
                                                                collada_data              in_collada_data,
                                                                system_hashed_ansi_string parent_name)
{
    _collada_data_float_array* result_ptr = new (std::nothrow) _collada_data_float_array;

    ASSERT_ALWAYS_SYNC(result_ptr != NULL,
                       "Out of memory");

    if (result_ptr != NULL)
    {
        unsigned int count = float_array_element_ptr->UnsignedAttribute("count");
        const char*  data  = float_array_element_ptr->GetText          ();
        const char*  name  = float_array_element_ptr->Attribute        ("id");

        /* Sanity checks */
        if (name == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Float array ID is undefined - this is valid but unsupported ATM");

            goto end;
        }

        if (data == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Null value reported for float_array value");

            goto end;
        }

        if (count == 0)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Zero count encountered in a float_array - this is invalid");

            goto end;
        }

        /* Start forming the result descriptor */
        result_ptr->count        = count;
        result_ptr->data         = new float[count];
        result_ptr->n_components = n_components;
        result_ptr->stride       = stride;

        /* If we're in 'cache blobs' mode, check if a blob is already available. If so, we'll use
         * it, instead of parsing the string in the COLLADA file, which can take a lot of time for
         * large scenes.
         */
        system_hashed_ansi_string blob_file_name         = NULL;
        bool                      has_loaded_cached_blob = false;
        bool                      should_cache_blobs     = false;

        collada_data_get_property(in_collada_data,
                                  COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
                                 &should_cache_blobs);

        if (should_cache_blobs)
        {
            collada_data_geometry     geometry_mesh_parent = NULL;
            system_hashed_ansi_string geometry_name        = NULL;

            blob_file_name = _collada_data_float_array_get_cached_blob_file_name(system_hashed_ansi_string_create(name),
                                                                                 count,
                                                                                 parent_name
                                                                                );

            /* Is the blob present already?
             *
             * NOTE: Reading must not be done asynchronously, since geometry data is already parsed
             *       in multiple threads, and all available threads may be used at the same time,
             *       leading the async read to effectively lock up.
             **/
            system_file_serializer blob_serializer = system_file_serializer_create_for_reading(blob_file_name,
                                                                                               false); /* async_read */
            const void*            blob_data       = NULL;

            system_file_serializer_get_property(blob_serializer,
                                                SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                               &blob_data);

            if (blob_data != NULL)
            {
                system_file_serializer_read(blob_serializer,
                                            sizeof(float) * count,
                                            result_ptr->data);

                LOG_INFO("Loaded cached blob for [%s]",
                         system_hashed_ansi_string_get_buffer(blob_file_name) );

                has_loaded_cached_blob = true;
            } /* if (blob_data != NULL) */
            else
            {
                LOG_INFO("Could not load cached blob for [%s]",
                         system_hashed_ansi_string_get_buffer(blob_file_name) );
            }

            system_file_serializer_release(blob_serializer);
        }

        if (!has_loaded_cached_blob)
        {
            /* Extract the values */
            unsigned int n_value       = 0;
            const char*  traveller_ptr = data;

            while (n_value != count)
            {
                /* Read the value */
                system_text_get_float_from_text(traveller_ptr,
                                                result_ptr->data + n_value);

                /* Move to next value */
                traveller_ptr = strchr(traveller_ptr, ' ') + 1;

                n_value++;
            }

            if (should_cache_blobs)
            {
                /* If the blob mode is on but the file was not available, create one now */
                system_file_serializer blob_serializer = system_file_serializer_create_for_writing(blob_file_name);

                system_file_serializer_write  (blob_serializer,
                                               sizeof(float) * count,
                                               result_ptr->data);
                system_file_serializer_release(blob_serializer);

                LOG_INFO("Stored a cached float array blob for [%s]",
                         system_hashed_ansi_string_get_buffer(blob_file_name) );
            }
        } /* if (!has_loaded_cached_blob) */
    }

end:
    return (collada_data_float_array) result_ptr;
}

/** Please see header for spec */
PUBLIC void collada_data_float_array_get_property(collada_data_float_array          array,
                                                  collada_data_float_array_property property,
                                                  void*                             out_result)
{
    _collada_data_float_array* array_ptr = (_collada_data_float_array*) array;

    switch (property)
    {
        case COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_COMPONENTS:
        {
            *(uint32_t*) out_result = array_ptr->n_components;

            break;
        }

        case COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES:
        {
            *(uint32_t*) out_result = array_ptr->count;

            break;
        }

        case COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA:
        {
            *(const float**) out_result = array_ptr->data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_float_array_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void collada_data_float_array_release(collada_data_float_array array)
{
    delete (_collada_data_float_array*) array;

    array = NULL;
}