/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_camera.h"
#include "collada/collada_data_scene_graph_node.h"
#include "collada/collada_data_scene_graph_node_camera_instance.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes a single camera instance as described by <instance_camera> */
typedef struct _collada_data_scene_graph_node_camera_instance
{
    collada_data_camera       camera;
    system_hashed_ansi_string name;

    _collada_data_scene_graph_node_camera_instance();
} _collada_data_scene_graph_node_camera_instance;


/** TODO */
_collada_data_scene_graph_node_camera_instance::_collada_data_scene_graph_node_camera_instance()
{
    camera = NULL;
    name   = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
PUBLIC collada_data_scene_graph_node_camera_instance collada_data_scene_graph_node_camera_instance_create(__in __notnull tinyxml2::XMLElement*         element_ptr,
                                                                                                          __in __notnull system_hash64map              cameras_by_id_map,
                                                                                                          __in __notnull system_hashed_ansi_string name)
{
    /* Allocate space for the descriptor */
    _collada_data_scene_graph_node_camera_instance* new_camera_instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_camera_instance;

    ASSERT_DEBUG_SYNC(new_camera_instance_ptr != NULL, "Out of memory");
    if (new_camera_instance_ptr == NULL)
    {
        goto end;
    }

    /* Locate the geometry the instance is referring to */
    const char*         camera_name      = element_ptr->Attribute("url");
    system_hash64       camera_name_hash;
    collada_data_camera camera           = NULL;

    ASSERT_DEBUG_SYNC(camera_name != NULL, "url attribute missing");
    if (camera_name == NULL)
    {
        goto end;
    }

    ASSERT_DEBUG_SYNC(camera_name[0] == '#', "Invalid url attribute");
    camera_name++;

    camera_name_hash = system_hash64_calculate(camera_name, strlen(camera_name) );
    ASSERT_DEBUG_SYNC(system_hash64map_contains(cameras_by_id_map, camera_name_hash),
                      "Camera that is being referred to was not found");

    system_hash64map_get(cameras_by_id_map, camera_name_hash, &camera);
    ASSERT_DEBUG_SYNC(camera != NULL, "NULL pointer returned for a camera URL");

    /* Init the descriptor */
    new_camera_instance_ptr->camera = camera;
    new_camera_instance_ptr->name   = name;

end:
    return (collada_data_scene_graph_node_camera_instance) new_camera_instance_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_camera_instance_get_property(__in  __notnull collada_data_scene_graph_node_camera_instance          camera_instance,
                                                                                   __in            collada_data_scene_graph_node_camera_instance_property property,
                                                                                   __out __notnull void*                                                  out_result)
{
    _collada_data_scene_graph_node_camera_instance* instance_ptr = (_collada_data_scene_graph_node_camera_instance*) camera_instance;

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_CAMERA:
        {
            *(collada_data_camera*) out_result = instance_ptr->camera;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = instance_ptr->name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_scene_graph_node_camera_instance_property value");
        }
    }
}


/** Please see header for spec */
PUBLIC void collada_data_scene_graph_node_camera_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_camera_instance camera_instance)
{
    delete (_collada_data_scene_graph_node_camera_instance*) camera_instance;

    camera_instance = NULL;
}