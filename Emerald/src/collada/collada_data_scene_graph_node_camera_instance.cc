/**
 *
 * Emerald (kbi/elude @2014-2016)
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
    camera = nullptr;
    name   = system_hashed_ansi_string_get_default_empty_string();
}

/** TODO */
PUBLIC collada_data_scene_graph_node_camera_instance collada_data_scene_graph_node_camera_instance_create(tinyxml2::XMLElement*         element_ptr,
                                                                                                          system_hash64map              cameras_by_id_map,
                                                                                                          system_hashed_ansi_string name)
{
    const char*                                     camera_name             = nullptr;
    system_hash64                                   camera_name_hash        = 0;
    collada_data_camera                             camera                  = nullptr;
    _collada_data_scene_graph_node_camera_instance* new_camera_instance_ptr = nullptr;

    /* Allocate space for the descriptor */
    new_camera_instance_ptr = new (std::nothrow) _collada_data_scene_graph_node_camera_instance;

    ASSERT_DEBUG_SYNC(new_camera_instance_ptr != nullptr,
                      "Out of memory");

    if (new_camera_instance_ptr == nullptr)
    {
        goto end;
    }

    /* Locate the geometry the instance is referring to */
    camera_name = element_ptr->Attribute("url");

    ASSERT_DEBUG_SYNC(camera_name != nullptr,
                      "url attribute missing");

    if (camera_name == nullptr)
    {
        goto end;
    }

    ASSERT_DEBUG_SYNC(camera_name[0] == '#',
                      "Invalid url attribute");

    camera_name++;

    camera_name_hash = system_hash64_calculate(camera_name,
                                               strlen(camera_name) );

    ASSERT_DEBUG_SYNC(system_hash64map_contains(cameras_by_id_map,
                                                camera_name_hash),
                      "Camera that is being referred to was not found");

    system_hash64map_get(cameras_by_id_map,
                         camera_name_hash,
                        &camera);

    ASSERT_DEBUG_SYNC(camera != nullptr,
                      "nullptr pointer returned for a camera URL");

    /* Init the descriptor */
    new_camera_instance_ptr->camera = camera;
    new_camera_instance_ptr->name   = name;

end:
    return (collada_data_scene_graph_node_camera_instance) new_camera_instance_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_camera_instance_get_property(collada_data_scene_graph_node_camera_instance          camera_instance,
                                                                                   collada_data_scene_graph_node_camera_instance_property property,
                                                                                   void*                                                  out_result_ptr)
{
    _collada_data_scene_graph_node_camera_instance* instance_ptr = reinterpret_cast<_collada_data_scene_graph_node_camera_instance*>(camera_instance);

    switch (property)
    {
        case COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_CAMERA:
        {
            *reinterpret_cast<collada_data_camera*>(out_result_ptr) = instance_ptr->camera;

            break;
        }

        case COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = instance_ptr->name;

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
PUBLIC void collada_data_scene_graph_node_camera_instance_release(collada_data_scene_graph_node_camera_instance camera_instance)
{
    delete reinterpret_cast<_collada_data_scene_graph_node_camera_instance*>(camera_instance);

    camera_instance = nullptr;
}