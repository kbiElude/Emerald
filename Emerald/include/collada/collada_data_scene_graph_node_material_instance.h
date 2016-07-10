/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_H
#define COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_scene_graph_node_material_instance_property
{
    COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_SYMBOL_NAME,

    /* Always last */
    COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_PROPERTY_COUNT
};

/** TODO */
PUBLIC void collada_data_scene_graph_node_material_instance_add_material_binding(collada_data_scene_graph_node_material_instance instance,
                                                                                 collada_data_geometry_material_binding          binding);

/** TODO */
PUBLIC collada_data_scene_graph_node_material_instance collada_data_scene_graph_node_material_instance_create(tinyxml2::XMLElement* element_ptr,
                                                                                                              system_hash64map      materials_by_id_map);

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_material_binding(collada_data_scene_graph_node_material_instance instance,
                                                                                             unsigned int                                    n_binding,
                                                                                             collada_data_geometry_material_binding*         out_material_binding_ptr);

/* TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_properties(collada_data_scene_graph_node_material_instance instance,
                                                                                       collada_data_material*                          out_material_ptr,
                                                                                       system_hashed_ansi_string*                      out_symbol_name_ptr,
                                                                                       unsigned int*                                   out_n_bindings_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_property(collada_data_scene_graph_node_material_instance          instance,
                                                                                     collada_data_scene_graph_node_material_instance_property property,
                                                                                     void*                                                    out_result_ptr);

/** TODO */
PUBLIC void collada_data_scene_graph_node_material_instance_release(collada_data_scene_graph_node_material_instance instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_H */
