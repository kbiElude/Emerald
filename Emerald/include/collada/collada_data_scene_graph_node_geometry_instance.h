/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H

#include "collada/collada_types.h"
#include "collada/collada_data_geometry.h"
#include "collada/collada_data_scene_graph_node.h"
#include "tinyxml2.h"

typedef enum collada_data_scene_graph_node_geometry_instance_property
{
    COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_GEOMETRY, /* not settable, collada_data_scene_graph_node_geometry_instance */
    COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_PROPERTY_NAME      /* not settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC collada_data_scene_graph_node_geometry_instance collada_data_scene_graph_node_geometry_instance_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                                                                              __in           system_hash64map          geometry_by_id_map,
                                                                                                              __in __notnull system_hashed_ansi_string name,
                                                                                                              __in __notnull system_hash64map          materials_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_geometry_instance_get_property(__in  __notnull collada_data_scene_graph_node_geometry_instance          geometry_instance,
                                                                                     __in            collada_data_scene_graph_node_geometry_instance_property property,
                                                                                     __out __notnull void*                                                    out_result);


/** TODO */
PUBLIC void collada_data_scene_graph_node_geometry_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_geometry_instance geometry_instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_GEOMETRY_INSTANCE_H */
