/**
 *
 * Emerald (kbi/elude @2014)
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
PUBLIC void collada_data_scene_graph_node_material_instance_add_material_binding(__in __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                 __in           collada_data_geometry_material_binding          binding);

/** TODO */
PUBLIC collada_data_scene_graph_node_material_instance collada_data_scene_graph_node_material_instance_create(__in __notnull tinyxml2::XMLElement* element_ptr,
                                                                                                              __in __notnull system_hash64map      materials_by_id_map);

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_material_binding(__in  __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                             __in            unsigned int                                    n_binding,
                                                                                             __out __notnull collada_data_geometry_material_binding*         out_material_binding);

/* TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_properties(__in      __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                       __out_opt           collada_data_material*                          out_material,
                                                                                       __out_opt           system_hashed_ansi_string*                      out_symbol_name,
                                                                                       __out_opt           unsigned int*                                   out_n_bindings);

/** TODO */
PUBLIC EMERALD_API void collada_data_scene_graph_node_material_instance_get_property(__in      __notnull collada_data_scene_graph_node_material_instance          instance,
                                                                                     __in                collada_data_scene_graph_node_material_instance_property property,
                                                                                     __out_opt           void*                                                    out_result_ptr);

/** TODO */
PUBLIC void collada_data_scene_graph_node_material_instance_release(__in __notnull __post_invalid collada_data_scene_graph_node_material_instance instance);

#endif /* COLLADA_DATA_SCENE_GRAPH_NODE_MATERIAL_INSTANCE_H */
