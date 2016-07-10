/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "collada/collada_data.h"
#include "collada/collada_data_geometry_mesh.h"
#include "collada/collada_data_polylist.h"
#include "collada/collada_data_source.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_hash64.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <mesh> node */
typedef struct _collada_data_geometry_mesh
{
    collada_data_geometry parent_geometry;

    /* Polygon lists */
    system_resizable_vector polylists;

    /* Sources contain raw FP data of a specific category (eg. vertex positions, normals, etc.) */
    system_resizable_vector sources;
    system_hash64map        sources_map_by_id;

    explicit _collada_data_geometry_mesh(collada_data_geometry parent_geometry);

    ~_collada_data_geometry_mesh();
} _collada_data_geometry_mesh;


/** TODO */
_collada_data_geometry_mesh::_collada_data_geometry_mesh(collada_data_geometry in_parent_geometry)
{
    parent_geometry   = in_parent_geometry;
    polylists         = system_resizable_vector_create(4 /* capacity */);
    sources           = system_resizable_vector_create(4 /* capacity */);
    sources_map_by_id = system_hash64map_create       (sizeof(struct _collada_data_geometry_mesh_source*) );
}

/** TODO */
_collada_data_geometry_mesh::~_collada_data_geometry_mesh()
{
    if (polylists != nullptr)
    {
        collada_data_polylist polylist = nullptr;

        while (system_resizable_vector_pop(polylists,
                                          &polylist) )
        {
            collada_data_polylist_release(polylist);

            polylist = nullptr;
        }

        system_resizable_vector_release(polylists);
        polylists = nullptr;
    }

    if (sources != nullptr)
    {
        unsigned int n_sources = 0;

        system_resizable_vector_get_property(sources,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_sources);

        while (n_sources > 0)
        {
            collada_data_source source = nullptr;

            if (system_resizable_vector_pop(sources,
                                           &source) )
            {
                /* Release the item */
                collada_data_source_release(source);

                source = nullptr;
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not retrieve COLLADA geometry mesh source descriptor");
            }
        }

        system_resizable_vector_release(sources);
        sources = nullptr;
    }

    if (sources_map_by_id != nullptr)
    {
        system_hash64map_release(sources_map_by_id);

        sources_map_by_id = nullptr;
    }
}

/** TODO */
PUBLIC collada_data_geometry_mesh collada_data_geometry_mesh_create(tinyxml2::XMLElement* mesh_element_ptr,
                                                                    collada_data_geometry parent_geometry,
                                                                    const collada_data    collada_data)
{
    _collada_data_geometry_mesh* result_mesh_ptr      = new (std::nothrow) _collada_data_geometry_mesh(parent_geometry);
    system_hashed_ansi_string    parent_geometry_id   = nullptr;
    system_hashed_ansi_string    parent_geometry_name = nullptr;

    collada_data_geometry_get_property(parent_geometry,
                                       COLLADA_DATA_GEOMETRY_PROPERTY_ID,
                                       &parent_geometry_id);
    collada_data_geometry_get_property(parent_geometry,
                                       COLLADA_DATA_GEOMETRY_PROPERTY_NAME,
                                       &parent_geometry_name);

    ASSERT_ALWAYS_SYNC(result_mesh_ptr != nullptr,
                       "Out of memory");

    if (result_mesh_ptr != nullptr)
    {
        /* 1) Retrieve data of all defined sources */
        tinyxml2::XMLElement* current_source_element_ptr = mesh_element_ptr->FirstChildElement("source");

        while (current_source_element_ptr != nullptr)
        {
            collada_data_source new_source = collada_data_source_create(current_source_element_ptr,
                                                                        collada_data,
                                                                        parent_geometry_id);

            ASSERT_DEBUG_SYNC(new_source != nullptr,
                              "Could not create COLLADA geometry mesh source descriptor");

            if (new_source != nullptr)
            {
                /* Store the source descriptor instance */
                system_resizable_vector_push(result_mesh_ptr->sources,
                                             new_source);

                /* If the name is defined, store 1:1 relation in the name->source descriptor instance map */
                system_hashed_ansi_string new_source_name = nullptr;

                collada_data_source_get_property(new_source,
                                                 COLLADA_DATA_SOURCE_PROPERTY_ID,
                                                 &new_source_name);

                if (new_source_name != system_hashed_ansi_string_get_default_empty_string() )
                {
                    system_hash64map_insert(result_mesh_ptr->sources_map_by_id,
                                            system_hashed_ansi_string_get_hash(new_source_name),
                                            new_source,
                                            nullptr,  /* no release call-back needed */
                                            nullptr); /* no release call-back needed */
                }
            }

            /* Move on */
            current_source_element_ptr = current_source_element_ptr->NextSiblingElement("source");
        }

        /* 2) Check the <vertices> node, as it will provide 1:1 mapping to actual vertex data source */
        tinyxml2::XMLElement* vertices_element_ptr = mesh_element_ptr->FirstChildElement("vertices");

        if (vertices_element_ptr == nullptr)
        {
            LOG_FATAL        ("<mesh> node lacks <vertices> child which is invalid");
            ASSERT_DEBUG_SYNC(false, "No <vertices> child found");
        }
        else
        {
            /* Look for an <input> element using POSITION semantic. Its source attribute will
             * be pointing at actual vertex data source we should be using. */
            tinyxml2::XMLElement* current_position_element_ptr  = vertices_element_ptr->FirstChildElement("input");
            bool                  has_found_position_definition = false;
            const char*           vertices_id                   = vertices_element_ptr->Attribute("id");

            if (vertices_id == nullptr)
            {
                LOG_FATAL        ("<vertices> node does not define id attribute which is invalid");
                ASSERT_DEBUG_SYNC(false, "Cannot form vertex data");
            }

            while (current_position_element_ptr != nullptr)
            {
                /* Does this element use a POSITION semantic? */
                if (current_position_element_ptr->Attribute("semantic", "POSITION") )
                {
                    /* Yes it does! */
                    has_found_position_definition = true;

                    /* Retrieve vertex data source name */
                    const char* vertex_source_name = current_position_element_ptr->Attribute("source");

                    if (vertex_source_name == nullptr)
                    {
                        LOG_FATAL("<input> node does not define a source attribute");

                        ASSERT_DEBUG_SYNC(false,
                                          "Cannot form vertex data");
                    }
                    else
                    {
                        /* Simplify things and assume the vertex data source ID anymore. */
                        ASSERT_DEBUG_SYNC(vertex_source_name[0] == '#', "Reference should be prefixed with #");

                        vertex_source_name++; /* Skip # character */

                        /* Before we continue, find the new source descriptor */
                        collada_data_source mapped_source = nullptr;

                        if (!system_hash64map_get(result_mesh_ptr->sources_map_by_id,
                                                  system_hash64_calculate(vertex_source_name, strlen(vertex_source_name) ),
                                                  &mapped_source) )
                        {
                            LOG_FATAL("Could not locate descriptor for mesh source [%s]",
                                      vertex_source_name);

                            ASSERT_DEBUG_SYNC(false,
                                              "Cannot form vertex data");
                        }
                        else
                        {
                            /* Add a new mapping from <vertices id> to the descriptor we've just found */
                            system_hash64 entry_hash = system_hash64_calculate(vertices_id, strlen(vertices_id) );

                            ASSERT_DEBUG_SYNC(!system_hash64map_contains(result_mesh_ptr->sources_map_by_id,
                                                                         entry_hash),
                                              "Item already added to a hash-map");

                            system_hash64map_insert(result_mesh_ptr->sources_map_by_id,
                                                    entry_hash,
                                                    mapped_source,
                                                    nullptr,  /* no removal call-back needed */
                                                    nullptr); /* no removal call-back needed */
                        }
                    }

                    break;
                }

                /* Couldn't find the semantic we've been looking for.. keep searching */
                current_position_element_ptr = current_position_element_ptr->NextSiblingElement("input");
            }

            if (!has_found_position_definition)
            {
                LOG_FATAL("Could not find <input> with POSITION semantic");

                ASSERT_DEBUG_SYNC(false,
                                  "Cannot form vertex data");
            }
        }

        /* 3) Read index data for all supported inputs.
         *
         * NOTE: Meshes can be defined with <lines>, <linestrips>, <polygons>,
         *       <polylist>, <triangles>, <trifans> and <tristrips>. We only
         *       support <polylist> currently. Make sure to log fatals if we
         *       encounter any of the unsupported nodes at this point */
        if (mesh_element_ptr->FirstChildElement("lines") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of lines which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        if (mesh_element_ptr->FirstChildElement("linestrips") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of line strips which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        if (mesh_element_ptr->FirstChildElement("polygons") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of polygons which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        if (mesh_element_ptr->FirstChildElement("triangles") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of triangles which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        if (mesh_element_ptr->FirstChildElement("trifans") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of triangle fans which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        if (mesh_element_ptr->FirstChildElement("tristrips") != nullptr)
        {
            LOG_FATAL("Geometry [%s] defines a mesh consisting of triangle strips which are not supported",
                      system_hashed_ansi_string_get_buffer(parent_geometry_name) );
        }

        /* Finally, polylists. */
        tinyxml2::XMLElement* current_polylist_element_ptr = mesh_element_ptr->FirstChildElement("polylist");
        unsigned int          n_current_polylist           = 0;

        while (current_polylist_element_ptr != nullptr)
        {
            /* Initialize polylist descriptor */
            collada_data_polylist result_polylist = collada_data_polylist_create(current_polylist_element_ptr,
                                                                                 (collada_data_geometry_mesh) result_mesh_ptr,
                                                                                 collada_data);

            ASSERT_DEBUG_SYNC(result_polylist != nullptr,
                              "Could not create polylist descriptor");

            if (result_polylist != nullptr)
            {
                /* Store the polygon list */
                system_resizable_vector_push(result_mesh_ptr->polylists,
                                             result_polylist);
            }

            /* Move on.. */
            n_current_polylist++;

            current_polylist_element_ptr = current_polylist_element_ptr->NextSiblingElement("polylist");
        }
    }

    return (collada_data_geometry_mesh) result_mesh_ptr;
}

/* Please see header for spec */
PUBLIC EMERALD_API void collada_data_geometry_mesh_get_polylist(collada_data_geometry_mesh mesh,
                                                                unsigned int               n_polylist,
                                                                collada_data_polylist*     out_polylist)
{
    _collada_data_geometry_mesh* mesh_ptr = reinterpret_cast<_collada_data_geometry_mesh*>(mesh);

    system_resizable_vector_get_element_at(mesh_ptr->polylists,
                                           n_polylist,
                                           out_polylist);
}

/* Please see header for spec */
PUBLIC void collada_data_geometry_mesh_get_property(collada_data_geometry_mesh          mesh,
                                                    collada_data_geometry_mesh_property property,
                                                    void*                               out_result_ptr)
{
    _collada_data_geometry_mesh* mesh_ptr = reinterpret_cast<_collada_data_geometry_mesh*>(mesh);

    switch (property)
    {
        case COLLADA_DATA_GEOMETRY_MESH_PROPERTY_N_POLYLISTS:
        {
            system_resizable_vector_get_property(mesh_ptr->polylists,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case COLLADA_DATA_GEOMETRY_MESH_PROPERTY_PARENT_GEOMETRY:
        {
            *reinterpret_cast<collada_data_geometry*>(out_result_ptr) = mesh_ptr->parent_geometry;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA data geometry mesh property");
        }
    }
}

/* Please see header for spec */
PUBLIC collada_data_source collada_data_geometry_mesh_get_source_by_id(collada_data_geometry_mesh mesh,
                                                                       system_hashed_ansi_string  source_id)
{
    _collada_data_geometry_mesh* mesh_ptr = reinterpret_cast<_collada_data_geometry_mesh*>(mesh);
    collada_data_source          result   = nullptr;

    system_hash64map_get(mesh_ptr->sources_map_by_id,
                         system_hashed_ansi_string_get_hash(source_id),
                         &result);

    return result;
}

/* Please see header for spec */
PUBLIC void collada_data_geometry_mesh_release(collada_data_geometry_mesh mesh)
{
    delete reinterpret_cast<_collada_data_geometry_mesh*>(mesh);

    mesh = nullptr;
}
