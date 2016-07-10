/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_H

#include "collada/collada_types.h"
#include "collada/collada_data_camera.h"
#include "tinyxml2.h"

typedef enum
{
    /* not settable, collada_data_scene_graph_node_camera_instance */
    COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_CAMERA,

    /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_PROPERTY_NAME
} collada_data_scene_graph_node_camera_instance_property;

/** TODO */
PUBLIC collada_data_scene_graph_node_camera_instance collada_data_scene_graph_node_camera_instance_create(tinyxml2::XMLElement*     element_ptr,
                                                                                                          system_hash64map          cameras_by_id_map,
                                                                                                          system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_camera_instance_get_property(collada_data_scene_graph_node_camera_instance          camera_instance,
                                                                                   collada_data_scene_graph_node_camera_instance_property property,
                                                                                   void*                                                  out_result_ptr);

/** TODO */
PUBLIC void collada_data_scene_graph_node_camera_instance_release(collada_data_scene_graph_node_camera_instance camera_instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_CAMERA_INSTANCE_H */
