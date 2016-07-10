/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_geometry_mesh.h"
#include "collada/collada_data_input.h"
#include "collada/collada_data_polylist.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <polylist> node */
typedef struct _collada_data_polylist
{
    collada_data_input        inputs[COLLADA_DATA_INPUT_TYPE_COUNT];
    system_hashed_ansi_string material_name; /* this is an abstract name that an actual material instance needs to be bound by <bind_material> ! */
    unsigned int              n_inputs;
    unsigned int              n_vertices;

    unsigned int              polygon_indices_max;
    unsigned int              polygon_indices_min;
    unsigned int*             polygon_indices;
    unsigned int*             vertex_counts;

    /* Constructor */
     _collada_data_polylist();
    ~_collada_data_polylist();
} _collada_data_polylist;


/** TODO */
_collada_data_polylist::_collada_data_polylist()
{
    memset(inputs,
           0,
           sizeof(inputs) );

    material_name       = nullptr;
    n_inputs            = 0;
    n_vertices          = 0;
    polygon_indices_max = -1;
    polygon_indices_min = -1;
    polygon_indices     = nullptr;
    vertex_counts       = nullptr;
}

/** TODO */
_collada_data_polylist::~_collada_data_polylist()
{
    for (int n_input = 0;
             n_input < COLLADA_DATA_INPUT_TYPE_COUNT;
           ++n_input)
    {
        if (inputs[n_input] != nullptr)
        {
            delete inputs[n_input];

            inputs[n_input] = nullptr;
        }
    }

    if (polygon_indices != nullptr)
    {
        delete [] polygon_indices;

        polygon_indices = nullptr;
    }

    if (vertex_counts != nullptr)
    {
        delete [] vertex_counts;

        vertex_counts = nullptr;
    }
}


/** TODO */
PRIVATE system_hashed_ansi_string _collada_data_polylist_get_cached_blob_file_name(system_hashed_ansi_string geometry_name,
                                                                                   unsigned int              count,
                                                                                   system_hashed_ansi_string material)
{
    char temp_file_name[1024];

    snprintf(temp_file_name,
             sizeof(temp_file_name),
             "collada_polylist_%s_%u_%s",
             system_hashed_ansi_string_get_buffer(geometry_name),
             count,
             system_hashed_ansi_string_get_buffer(material) );

    return system_hashed_ansi_string_create(temp_file_name);
}

/** TODO */
PUBLIC collada_data_polylist collada_data_polylist_create(tinyxml2::XMLElement*      polylist_element_ptr,
                                                          collada_data_geometry_mesh geometry_mesh,
                                                          collada_data               data)
{
    system_hashed_ansi_string blob_file_name            = nullptr;
    tinyxml2::XMLElement*     current_input_element_ptr = nullptr;
    bool                      has_loaded_cached_blob    = false;
    unsigned int*             index_data                = nullptr;
    const char*               material_name             = nullptr;
    system_hashed_ansi_string material_name_has         = nullptr;
    unsigned int              n_inputs                  = 0;
    unsigned int              polylist_count            = 0;
    _collada_data_polylist*   result_polylist_ptr       = new (std::nothrow) _collada_data_polylist;
    bool                      should_cache_blobs        = false;
    unsigned int              total_vcount              = 0;
    unsigned int*             vcount_data               = nullptr;

    ASSERT_ALWAYS_SYNC(result_polylist_ptr != nullptr,
                       "Out of memory");

    if (result_polylist_ptr == nullptr)
    {
        goto end;
    }

    /* Step 1) Iterate over all inputs defined and process the nodes */
    current_input_element_ptr = polylist_element_ptr->FirstChildElement("input");
    material_name             = polylist_element_ptr->Attribute        ("material");
    polylist_count            = polylist_element_ptr->IntAttribute     ("count");

    while (current_input_element_ptr != nullptr)
    {
        _collada_data_input_type input_type;
        collada_data_input_set   new_input_set;
        unsigned int             offset        = current_input_element_ptr->IntAttribute("offset");
        const char*              semantic      = current_input_element_ptr->Attribute   ("semantic");
        unsigned int             set           = current_input_element_ptr->IntAttribute("set");
        collada_data_source      source        = nullptr;
        const char*              source_name   = current_input_element_ptr->Attribute   ("source");

        /* Sanity checks */
        if (semantic == nullptr)
        {
            LOG_FATAL("Semantic not defined for <input> sub-node of <polylist>");

            ASSERT_DEBUG_SYNC(false,
                              "Will skip an input");

            goto next_input;
        }

        if (source_name == nullptr)
        {
            LOG_FATAL("Source not defined for <input> sub-node of <polylist>");

            ASSERT_DEBUG_SYNC(false,
                              "Will skip an input");

            goto next_input;
        }
        else
        {
            ASSERT_DEBUG_SYNC(source_name[0] == '#',
                              "Source name is not a reference");

            source_name++;
        }

        /* Convert a semantic to internal equivalent */
        input_type = collada_data_input_convert_from_string(system_hashed_ansi_string_create(semantic) );

        if (input_type == COLLADA_DATA_INPUT_TYPE_UNDEFINED)
        {
            LOG_FATAL("Unrecognized semantic used for a polylist input");

            ASSERT_DEBUG_SYNC(false,
                              "Will skip an input");

            goto next_input;
        }

        /* Try to locate the source the input is referring to */
        source = collada_data_geometry_mesh_get_source_by_id(geometry_mesh,
                                                             system_hashed_ansi_string_create(source_name) );

        if (source == nullptr)
        {
            LOG_FATAL("Unrecognized source used in a polylist input");

            ASSERT_DEBUG_SYNC(false,
                              "Will skip an input");

            goto next_input;
        }

        /* We've got everything we need */
        ASSERT_DEBUG_SYNC(input_type <= COLLADA_DATA_INPUT_TYPE_UNDEFINED,
                          "Invalid input type");

        if (result_polylist_ptr->inputs[input_type] == nullptr)
        {
            collada_data_input new_input = collada_data_input_create(input_type);

            ASSERT_DEBUG_SYNC(new_input != nullptr,
                              "Out of memory");

            if (new_input == nullptr)
            {
                goto next_input;
            }

            result_polylist_ptr->inputs[input_type] = new_input;
        }

        /* Form the set descriptor and stash it into the hash map */
        new_input_set = collada_data_input_set_create(offset,
                                                      source);

        ASSERT_DEBUG_SYNC(new_input_set != nullptr,
                          "Out of memory");

        if (new_input_set == nullptr)
        {
            goto next_input;
        }

        collada_data_input_add_input_set(result_polylist_ptr->inputs[input_type],
                                         set,
                                         new_input_set);

next_input:
        /* Move on */
        current_input_element_ptr = current_input_element_ptr->NextSiblingElement("input");
        n_inputs                 ++;
    }

    /* Step 1.5) The following two steps take an awful amount of time to execute. Hence, let's see
    *            if a binary representation has already been generated by previous instances. If so,
    *            it will take a blink of an eye to load the binary data instead of parsing the text
    *            format.
    *            On another hand, COLLADA format is considered an intermediate format, so we want to
    *            make sure actual demos do not ever generate any binary representations. Hence, the
    *            mode needs to be manually toggled.
    */
    collada_data_get_property(data,
                              COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
                             &should_cache_blobs);

    if (should_cache_blobs)
    {
        collada_data_geometry     geometry_mesh_parent = nullptr;
        system_hashed_ansi_string geometry_name        = nullptr;

        collada_data_geometry_mesh_get_property(geometry_mesh,
                                                COLLADA_DATA_GEOMETRY_MESH_PROPERTY_PARENT_GEOMETRY,
                                                &geometry_mesh_parent);

        collada_data_geometry_get_property(geometry_mesh_parent,
                                           COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                           &geometry_name);

        blob_file_name = _collada_data_polylist_get_cached_blob_file_name(geometry_name,
                                                                          polylist_count,
                                                                          system_hashed_ansi_string_create(material_name) );

        /* Is the blob present already?
         *
         * NOTE: Reading must not be done asynchronously, since geometry data is already parsed
         *       in multiple threads, and all available threads may be used at the same time,
         *       leading the async read to effectively lock up.
         */
        system_file_serializer blob_serializer = system_file_serializer_create_for_reading(blob_file_name,
                                                                                           false); /* async_read */
        const void*            blob_data       = nullptr;

        system_file_serializer_get_property(blob_serializer,
                                            SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                           &blob_data);

        if (blob_data != nullptr)
        {
            system_file_serializer_read(blob_serializer,
                                        sizeof(unsigned int),
                                       &total_vcount);

            const unsigned int n_indices = total_vcount * n_inputs;

            vcount_data = new (std::nothrow) unsigned int [polylist_count];
            index_data  = new (std::nothrow) unsigned int [n_indices];

            ASSERT_ALWAYS_SYNC(vcount_data != nullptr &&
                               index_data  != nullptr,
                               "Out of memory");

            if (vcount_data != nullptr &&
                index_data  != nullptr)
            {
                system_file_serializer_read(blob_serializer,
                                            sizeof(unsigned int) * polylist_count,
                                            vcount_data);
                system_file_serializer_read(blob_serializer,
                                            sizeof(unsigned int) * n_indices,
                                            index_data);
                system_file_serializer_read(blob_serializer,
                                            sizeof(unsigned int),
                                           &result_polylist_ptr->polygon_indices_min);
                system_file_serializer_read(blob_serializer,
                                            sizeof(unsigned int),
                                           &result_polylist_ptr->polygon_indices_max);

                /* All done */
                has_loaded_cached_blob = true;
            }
            else
            {
                if (vcount_data != nullptr)
                {
                    delete [] vcount_data;

                    vcount_data = nullptr;
                }

                if (index_data != nullptr)
                {
                    delete [] index_data;

                    index_data = nullptr;
                }
            }

            LOG_INFO("Loaded cached blob for [%s]",
                     system_hashed_ansi_string_get_buffer(blob_file_name) );
        }
        else
        {
            LOG_INFO("Could not load cached blob for [%s]",
                     system_hashed_ansi_string_get_buffer(blob_file_name) );
        }

        system_file_serializer_release(blob_serializer);
    }

    if (!has_loaded_cached_blob)
    {
        /* Step 2) Retrieve vcount data */
        tinyxml2::XMLElement* vcount_element_ptr = polylist_element_ptr->FirstChildElement("vcount");

        ASSERT_DEBUG_SYNC(vcount_element_ptr != nullptr,
                          "<vcount> node was not found");

        if (vcount_element_ptr != nullptr)
        {
            vcount_data = new (std::nothrow) unsigned int[polylist_count];

            ASSERT_DEBUG_SYNC(vcount_data != nullptr,
                              "Out of memory");

            if (vcount_data != nullptr)
            {
                const char*  traveller_ptr = vcount_element_ptr->GetText();
                unsigned int n_counts_read = 0;

                while (n_counts_read != polylist_count)
                {
                    sscanf(traveller_ptr,
                           "%u",
                           vcount_data + n_counts_read);

                    if (result_polylist_ptr->polygon_indices_max == 0xFFFFFFFF                  ||
                        result_polylist_ptr->polygon_indices_max <  vcount_data[n_counts_read])
                    {
                        result_polylist_ptr->polygon_indices_max = vcount_data[n_counts_read];
                    }

                    if (result_polylist_ptr->polygon_indices_min == 0xFFFFFFFF                   ||
                        result_polylist_ptr->polygon_indices_min >  vcount_data[n_counts_read])
                    {
                        result_polylist_ptr->polygon_indices_min = vcount_data[n_counts_read];
                    }

                    traveller_ptr = strchr(traveller_ptr, ' ') + 1;

                    /* NOTE: We only support vcounts of 3 - triangles. */
                    ASSERT_DEBUG_SYNC(vcount_data[n_counts_read] == 3,
                                      "Unsupported vcount entry found!");

                    /* Move on */
                    total_vcount += vcount_data[n_counts_read];
                    n_counts_read++;
                }
            }
        }

        /* Step 3) Retrieve polygon construction data */
        tinyxml2::XMLElement* index_element_ptr = polylist_element_ptr->FirstChildElement("p");

        ASSERT_DEBUG_SYNC(index_element_ptr != nullptr,
                          "<p> node was not found");

        if (index_element_ptr != nullptr)
        {
            index_data = new (std::nothrow) unsigned int[total_vcount * n_inputs];

            ASSERT_DEBUG_SYNC(index_data != nullptr,
                              "Out of memory");

            if (index_data != nullptr)
            {
                const char*  traveller_ptr  = index_element_ptr->GetText();
                unsigned int n_indices_read = 0;

                while (n_indices_read != total_vcount * n_inputs)
                {
                    sscanf(traveller_ptr,
                           "%u",
                           index_data + n_indices_read);

                    traveller_ptr = strchr(traveller_ptr, ' ') + 1;

                    /* Move on */
                    n_indices_read++;
                }
            }
        }

        /* Step 3.5) If the 'cache blobs' mode is on, store the data we've read */
        if (should_cache_blobs)
        {
            system_file_serializer blob_serializer = system_file_serializer_create_for_writing(blob_file_name);
            const unsigned int     n_indices       = total_vcount * n_inputs;

            system_file_serializer_write(blob_serializer,
                                         sizeof(unsigned int),
                                         &total_vcount);
            system_file_serializer_write(blob_serializer,
                                         sizeof(unsigned int) * polylist_count,
                                         vcount_data);
            system_file_serializer_write(blob_serializer,
                                         sizeof(unsigned int) * n_indices,
                                         index_data);
            system_file_serializer_write(blob_serializer,
                                         sizeof(unsigned int),
                                         &result_polylist_ptr->polygon_indices_min);
            system_file_serializer_write(blob_serializer,
                                         sizeof(unsigned int),
                                         &result_polylist_ptr->polygon_indices_max);

            system_file_serializer_release(blob_serializer);

            LOG_INFO("Stored a cached polylist blob for [%s]",
                     system_hashed_ansi_string_get_buffer(blob_file_name) );
        }
    }

    /* Step 4) Store the results */
    material_name_has = (material_name != nullptr) ? system_hashed_ansi_string_create                  (material_name)
                                                   : system_hashed_ansi_string_get_default_empty_string();

    result_polylist_ptr->material_name   = material_name_has;
    result_polylist_ptr->n_inputs        = n_inputs;
    result_polylist_ptr->n_vertices      = total_vcount;
    result_polylist_ptr->polygon_indices = index_data;
    result_polylist_ptr->vertex_counts   = vcount_data;

end:
    return (collada_data_polylist) result_polylist_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_polylist_get_input(collada_data_polylist    polylist,
                                                        _collada_data_input_type type,
                                                        collada_data_input*      out_input_ptr)
{
    _collada_data_polylist* polylist_ptr = reinterpret_cast<_collada_data_polylist*>(polylist);

    if (out_input_ptr != nullptr)
    {
        *out_input_ptr = reinterpret_cast<collada_data_input>(polylist_ptr->inputs[type]);
    }
}

/* Please see header for properties */
PUBLIC EMERALD_API void collada_data_polylist_get_input_types(collada_data_polylist     polylist,
                                                              unsigned int*             out_n_input_types_ptr,
                                                              _collada_data_input_type* out_input_types_ptr)
{
    unsigned int            input_type_counter   = 0;
    unsigned int            result_n_input_types = 0;
    _collada_data_polylist* polylist_ptr         = reinterpret_cast<_collada_data_polylist*>(polylist);

    for (unsigned int n_input_type = 0;
                      n_input_type < COLLADA_DATA_INPUT_TYPE_COUNT;
                    ++n_input_type)
    {
        if (polylist_ptr->inputs[n_input_type] != nullptr)
        {
            result_n_input_types++;

            if (out_input_types_ptr != nullptr)
            {
                out_input_types_ptr[input_type_counter++] = static_cast<_collada_data_input_type>(n_input_type);
            }
        }
    }

    if (out_n_input_types_ptr != nullptr)
    {
        *out_n_input_types_ptr = result_n_input_types;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_polylist_get_property(const collada_data_polylist    polylist,
                                                           collada_data_polylist_property property,
                                                           void*                          out_result_ptr)
{
    const _collada_data_polylist* polylist_ptr = reinterpret_cast<const _collada_data_polylist*>(polylist);

    switch (property)
    {
        case COLLADA_DATA_POLYLIST_PROPERTY_INDEX_DATA:
        {
            *reinterpret_cast<unsigned int**>(out_result_ptr) = polylist_ptr->polygon_indices;

            break;
        }

        case COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MAXIMUM_VALUE:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = polylist_ptr->polygon_indices_max;

            break;
        }

        case COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MINIMUM_VALUE:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = polylist_ptr->polygon_indices_min;

            break;
        }

        case COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = polylist_ptr->material_name;

            break;
        }

        case COLLADA_DATA_POLYLIST_PROPERTY_N_INDICES:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = polylist_ptr->n_inputs * polylist_ptr->n_vertices;

            break;
        }

        case COLLADA_DATA_POLYLIST_PROPERTY_N_INPUTS:
        {
            *reinterpret_cast<unsigned int*>(out_result_ptr) = polylist_ptr->n_inputs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA data polylist property");
        }
    }
}

/* Please see header for spec */
PUBLIC void collada_data_polylist_release(collada_data_polylist polylist)
{
    delete reinterpret_cast<_collada_data_polylist*>(polylist);

    polylist = nullptr;
}