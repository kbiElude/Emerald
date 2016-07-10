/**
 *
 * Emerald (kbi/elude @2014-2016)
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
    light = nullptr;
    name  = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
PUBLIC collada_data_scene_graph_node_light_instance collada_data_scene_graph_node_light_instance_create(tinyxml2::XMLElement*         element_ptr,
                                                                                                        system_hash64map              lights_by_id_map,
                                                                                                        system_hashed_ansi_string name)
{
    const char*        light_name      = element_ptr->Attribute("url");
    system_hash64      light_name_hash = 0;
    collada_data_light light           = nullptr;

    /* Allocate space for the descriptor */
    _collada_data_scene_graph_node_light_instance* new_light_instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_light_instance;

    ASSERT_DEBUG_SYNC(new_light_instance_ptr != nullptr,
                      "Out of memory");

    if (new_light_instance_ptr == nullptr)
    {
        goto end;
    }

    /* Locate the light the instance is referring to */
    light_name = element_ptr->Attribute("url");

    ASSERT_DEBUG_SYNC(light_name != nullptr,
                      "url attribute missing");

    if (light_name == nullptr)
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

    ASSERT_DEBUG_SYNC(light != nullptr,
                      "nullptr pointer returned for a light URL");

    /* Init the descriptor */
    new_light_instance_ptr->light = light;
    new_light_instance_ptr->name  = name;

end:
    return reinterpret_cast<collada_data_scene_graph_node_light_instance>(new_light_instance_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_light_instance_get_property(collada_data_scene_graph_node_light_instance          light_instance,
                                                                                  collada_data_scene_graph_node_light_instance_property property,
                                                                                  void*                                                 out_result_ptr)
{
    _collada_data_scene_graph_node_light_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_light_instance*>(light_instance);

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_LIGHT:
        {
            *reinterpret_cast<collada_data_light*>(out_result_ptr) = instance_ptr->light;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = instance_ptr->name;

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
PUBLIC void collada_data_scene_graph_node_light_instance_release(collada_data_scene_graph_node_light_instance light_instance)
{
    delete reinterpret_cast<_collada_data_scene_graph_node_light_instance*>(light_instance);

    light_instance = nullptr;
}