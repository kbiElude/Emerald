/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

typedef enum collada_data_scene_graph_node_light_instance_property
{
    COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_LIGHT, /* not settable, collada_data_scene_graph_node_light_instance */
    COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_PROPERTY_NAME   /* not settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC collada_data_scene_graph_node_light_instance collada_data_scene_graph_node_light_instance_create(__in __notnull tinyxml2::XMLElement*           element_ptr,
                                                                                                        __in __notnull system_hash64map                lights_by_id_map,
                                                                                                        __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_light_instance_get_property(__in  __notnull collada_data_scene_graph_node_light_instance          light_instance,
                                                                                  __in            collada_data_scene_graph_node_light_instance_property property,
                                                                                  __out __notnull void*                                                 out_result);

/** TODO */
PUBLIC void collada_data_scene_graph_node_light_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_light_instance light_instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_LIGHT_INSTANCE_H */