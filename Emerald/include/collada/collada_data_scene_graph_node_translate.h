/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_TRANSLATE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_TRANSLATE_H

#include "collada/collada_types.h"
#include "collada/collada_data_scene_graph_node.h"
#include "tinyxml2.h"

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_translate_create(tinyxml2::XMLElement* element_ptr,
                                                                                         const float*          overriding_vec3 = NULL,
                                                                                         float*                result_vec3     = NULL);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_TRANSLATE_H */
