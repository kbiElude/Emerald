/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_geometry_material_binding.h"
#include "collada/collada_data_scene_graph_node_geometry_instance.h"
#include "collada/collada_data_scene_graph_node_material_instance.h"
#include "collada/collada_data_scene_graph_node.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_hash64.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <instance_geometry> node */
typedef struct _collada_data_scene_graph_node_geometry_instance
{
    collada_data_geometry     geometry;
    system_hashed_ansi_string name;

    _collada_data_scene_graph_node_geometry_instance();
} _collada_data_scene_graph_node_geometry_instance;


/** TODO */
_collada_data_scene_graph_node_geometry_instance::_collada_data_scene_graph_node_geometry_instance()
{
    geometry = NULL;
    name     = system_hashed_ansi_string_get_default_empty_string();
}

/** Please see header for specification */
PUBLIC collada_data_scene_graph_node_geometry_instance collada_data_scene_graph_node_geometry_instance_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                                                                              __in           system_hash64map          geometry_by_id_map,
                                                                                                              __in __notnull system_hashed_ansi_string name,
                                                                                                              __in __notnull system_hash64map          materials_by_id_map)
{
    /* Allocate space for the descriptor */
    tinyxml2::XMLElement*                             bind_material_element_ptr = NULL;
    collada_data_geometry                             geometry                  = NULL;
    const char*                                       geometry_name             = NULL;
    system_hash64                                     geometry_name_hash;
    _collada_data_scene_graph_node_geometry_instance* new_geometry_instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_geometry_instance;

    ASSERT_DEBUG_SYNC(new_geometry_instance_ptr != NULL,
                      "Out of memory");

    if (new_geometry_instance_ptr == NULL)
    {
        goto end;
    }

    /* Locate the geometry the instance is referring to */
    geometry_name = element_ptr->Attribute("url");

    ASSERT_DEBUG_SYNC(geometry_name != NULL,
                      "url attribute missing");

    if (geometry_name == NULL)
    {
        goto end;
    }

    ASSERT_DEBUG_SYNC(geometry_name[0] == '#',
                      "Invalid url attribute");

    geometry_name++;

    geometry_name_hash = system_hash64_calculate(geometry_name,
                                                 strlen(geometry_name) );

    ASSERT_DEBUG_SYNC(system_hash64map_contains(geometry_by_id_map,
                                                geometry_name_hash),
                      "Geometry that is being referred to was not found");

    system_hash64map_get(geometry_by_id_map,
                         geometry_name_hash,
                        &geometry);

    ASSERT_DEBUG_SYNC(geometry != NULL,
                      "NULL pointer returned for a geometry URL");

    /* Iterate over material instances, if any */
    bind_material_element_ptr = element_ptr->FirstChildElement("bind_material");

    if (bind_material_element_ptr != NULL)
    {
        /* We only support common technique */
        tinyxml2::XMLElement* technique_common_element_ptr = bind_material_element_ptr->FirstChildElement("technique_common");

        ASSERT_DEBUG_SYNC(technique_common_element_ptr != NULL,
                          "Common technique not defined for a <bind_material> none");

        if (technique_common_element_ptr != NULL)
        {
            /* Iterate over defined instances */
            tinyxml2::XMLElement* instance_material_element_ptr = technique_common_element_ptr->FirstChildElement("instance_material");

            while (instance_material_element_ptr != NULL)
            {
                /* Parse the tree */
                collada_data_scene_graph_node_material_instance instance = collada_data_scene_graph_node_material_instance_create(instance_material_element_ptr,
                                                                                                                                  materials_by_id_map);

                /* If there are any bindings defined, iterate through them and add to the instance */
                tinyxml2::XMLElement* bind_vertex_input_element_ptr = instance_material_element_ptr->FirstChildElement("bind_vertex_input");

                while (bind_vertex_input_element_ptr != NULL)
                {
                    /* Read the attributes */
                    const char*  input_semantic_attribute = bind_vertex_input_element_ptr->Attribute   ("input_semantic");
                    unsigned int input_set_attribute      = bind_vertex_input_element_ptr->IntAttribute("input_set");
                    const char*  semantic_attribute       = bind_vertex_input_element_ptr->Attribute   ("semantic");

                    ASSERT_DEBUG_SYNC(input_semantic_attribute != NULL,
                                      "Required <input_semantic> attribute is missing for <bind_vertex_input> node");
                    ASSERT_DEBUG_SYNC(semantic_attribute       != NULL,
                                      "Required <semantic> attribute is missing for <bind_vertex_input> node");

                    /* Spawn a new descriptor */
                    collada_data_geometry_material_binding binding = collada_data_geometry_material_binding_create(system_hashed_ansi_string_create(input_semantic_attribute),
                                                                                                                   input_set_attribute,
                                                                                                                   system_hashed_ansi_string_create(semantic_attribute) );

                    /* Store it */
                    collada_data_scene_graph_node_material_instance_add_material_binding(instance, binding);

                    /* Move to next sibling item */
                    bind_vertex_input_element_ptr = bind_vertex_input_element_ptr->NextSiblingElement("bind_vertex_input");
                }

                /* Add the instance to the vector */
                collada_data_geometry_add_material_instance(geometry,
                                                            instance);

                /* Move to next sibling */
                instance_material_element_ptr = instance_material_element_ptr->NextSiblingElement("instance_material");
            } /* while (instance_material_element_ptr != NULL) */
        }
    } /* if (bind_material_element_ptr != NULL) */

    /* Init the descriptor */
    new_geometry_instance_ptr->geometry = geometry;
    new_geometry_instance_ptr->name     = name;

end:
    return (collada_data_scene_graph_node_geometry_instance) new_geometry_instance_ptr;
}

/** TODO */
PUBLIC void collada_data_scene_graph_node_geometry_instance_get_properties(__in      __notnull collada_data_scene_graph_node_geometry_instance geometry_instance,
                                                                           __out_opt           collada_data_geometry*                          out_geometry,
                                                                           __out_opt           system_hashed_ansi_string*                      out_instance_name)
{
    _collada_data_scene_graph_node_geometry_instance* instance_ptr = (_collada_data_scene_graph_node_geometry_instance*) geometry_instance;

    if (out_geometry != NULL)
    {
        *out_geometry = (collada_data_geometry) instance_ptr->geometry;
    }

    if (out_instance_name != NULL)
    {
        *out_instance_name = instance_ptr->name;
    }
}

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_geometry_instance_get_property(__in  __notnull collada_data_scene_graph_node_geometry_instance          geometry_instance,
                                                                                     __in            collada_data_scene_graph_node_geometry_instance_property property,
                                                                                     __out __notnull void*                                                    out_result)
{
    _collada_data_scene_graph_node_geometry_instance* instance_ptr = (_collada_data_scene_graph_node_geometry_instance*) geometry_instance;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_GEOMETRY:
        {
            *(collada_data_geometry*) out_result = instance_ptr->geometry;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = instance_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_scene_graph_node_geometry_instance_property value");
        }
    }
}

/** TODO */
PUBLIC void collada_data_scene_graph_node_geometry_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_geometry_instance geometry_instance)
{
    delete (_collada_data_scene_graph_node_geometry_instance*) geometry_instance;

    geometry_instance = NULL;
}
