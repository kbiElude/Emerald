/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_camera.h"
#include "collada/collada_data_scene_graph_node.h"
#include "collada/collada_data_scene_graph_node_light_instance.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes a single camera instance as described by <instance_light> */
typedef struct _collada_data_scene_graph_node_light_instance
{
    collada_data_light        light;
    system_hashed_ansi_string name;

    _collada_data_scene_graph_node_light_instance();
} _collada_data_scene_graph_node_light_instance;


/** TODO */
_collada_data_scene_graph_node_light_instance::_collada_data_scene_graph_node_light_instance()
{
    light = NULL;
    name  = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
PUBLIC collada_data_scene_graph_node_light_instance collada_data_scene_graph_node_light_instance_create(__in __notnull tinyxml2::XMLElement*         element_ptr,
                                                                                                        __in __notnull system_hash64map              lights_by_id_map,
                                                                                                        __in __notnull system_hashed_ansi_string name)
{
    const char*        light_name      = element_ptr->Attribute("url");
    system_hash64      light_name_hash = 0;
    collada_data_light light           = NULL;

    /* Allocate space for the descriptor */
    _collada_data_scene_graph_node_light_instance* new_light_instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_light_instance;

    ASSERT_DEBUG_SYNC(new_light_instance_ptr != NULL,
                      "Out of memory");

    if (new_light_instance_ptr == NULL)
    {
        goto end;
    }

    /* Locate the light the instance is referring to */
    light_name = element_ptr->Attribute("url");

    ASSERT_DEBUG_SYNC(light_name != NULL,
                      "url attribute missing");

    if (light_name == NULL)
    {
        goto end;
    }

    ASSERT_DEBUG_SYNC(light_name[0] == '#',
                      "Invalid url attribute");

    light_name++;

    light_name_hash = system_hash64_calculate(light_name,
                                              strlen(light_name) );

    ASSERT_DEBUG_SYNC(system_hash64map_contains(lights_by_id_map,
                                                light_name_hash),
                      "Light that is being referred to was not found");

    system_hash64map_get(lights_by_id_map,
                         light_name_hash,
                        &light);

    ASSERT_DEBUG_SYNC(light != NULL,
                      "NULL pointer returned for a light URL");

    /* Init the descriptor */
    new_light_instance_ptr->light = light;
    new_light_instance_ptr->name  = name;

end:
    return (collada_data_scene_graph_node_light_instance) new_light_instance_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_light_instance_get_property(__in  __notnull collada_data_scene_graph_node_light_instance          light_instance,
                                                                                  __in            collada_data_scene_graph_node_light_instance_property property,
                                                                                  __out __notnull void*                                                 out_result)
{
    _collada_data_scene_graph_node_light_instance* instance_ptr = (_collada_data_scene_graph_node_light_instance*) light_instance;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_LIGHT:
        {
            *(collada_data_light*) out_result = instance_ptr->light;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = instance_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_scene_graph_node_light_instance_property value");
        }
    }
}

/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_light_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_light_instance light_instance)
{
    delete (_collada_data_scene_graph_node_light_instance*) light_instance;

    light_instance = NULL;
}