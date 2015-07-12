/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_H

#include "collada/collada_types.h"
#include "tinyxml2.h"


typedef enum
{
    COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_DATA_HANDLE, /* not settable, void* */
    COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_SID,         /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_GRAPH_NODE_ITEM_PROPERTY_TYPE         /* not settable, _collada_data_node_item_type */
} collada_data_scene_graph_node_item_property;

typedef enum
{
    COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_ID,           /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_N_NODE_ITEMS, /* not settable, uint32_t */
    COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_NAME,         /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_SCENE_GRAPH_NODE_PROPERTY_TYPE          /* not settable, _collada_data_node_type */
} collada_data_scene_graph_node_property;


/** TODO */
PUBLIC void collada_data_scene_graph_node_add_node_item(collada_data_scene_graph_node      node,
                                                        collada_data_scene_graph_node_item node_item);

/** TODO */
PUBLIC collada_data_scene_graph_node collada_data_scene_graph_node_create(void* parent);

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_item_create_node(tinyxml2::XMLElement*         element_ptr,
                                                                                         collada_data_scene_graph_node parent_node,
                                                                                         collada_data                  collada_data);

/** TODO */
PUBLIC collada_data_scene_graph_node_item collada_data_scene_graph_node_item_create(_collada_data_node_item_type type,
                                                                                    void*                        data,
                                                                                    system_hashed_ansi_string    sid);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_get_node_item(collada_data_scene_graph_node       node,
                                                                    unsigned int                        n_node_item,
                                                                    collada_data_scene_graph_node_item* out_node_item);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_item_get_property(collada_data_scene_graph_node_item          node_item,
                                                                        collada_data_scene_graph_node_item_property property,
                                                                        void*                                       out_result);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_get_property(collada_data_scene_graph_node          node,
                                                                   collada_data_scene_graph_node_property property,
                                                                   void*                                  out_result);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_H */
