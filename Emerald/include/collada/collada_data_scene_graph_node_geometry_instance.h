/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H

#include "collada/collada_types.h"
#include "collada/collada_data_geometry.h"
#include "collada/collada_data_scene_graph_node.h"
#include "tinyxml2.h"

typedef enum
{
    /* not settable, collada_data_scene_graph_node_geometry_instance */
    COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_GEOMETRY,

    /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_NAME
} collada_data_scene_graph_node_geometry_instance_property;

/** TODO */
PUBLIC collada_data_scene_graph_node_geometry_instance collada_data_scene_graph_node_geometry_instance_create(tinyxml2::XMLElement*     element_ptr,
                                                                                                              system_hash64map          geometry_by_id_map,
                                                                                                              system_hashed_ansi_string name,
                                                                                                              system_hash64map          materials_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_geometry_instance_get_property(collada_data_scene_graph_node_geometry_instance          geometry_instance,
                                                                                     collada_data_scene_graph_node_geometry_instance_property property,
                                                                                     void*                                                    out_result_ptr);


/** TODO */
PUBLIC void collada_data_scene_graph_node_geometry_instance_release(collada_data_scene_graph_node_geometry_instance geometry_instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H */
