/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_GEOMETRY_H
#define COLLADA_DATA_GEOMETRY_H

#include "collada/collada_types.h"
#include "collada/collada_data_material.h"
#include "mesh/mesh_types.h"
#include "tinyxml2.h"

/** Forward declarations */
enum collada_data_geometry_property
{
    COLLADA_DATA_GEOMETRY_PROPERTY_EMERALD_MESH,
    COLLADA_DATA_GEOMETRY_PROPERTY_ID,
    COLLADA_DATA_GEOMETRY_PROPERTY_MESH,
    COLLADA_DATA_GEOMETRY_PROPERTY_NAME,

    /* Always last */
    COLLADA_DATA_GEOMETRY_PROPERTY_COUNT
};


/** TODO */
PUBLIC void collada_data_geometry_add_material_instance(__in __notnull collada_data_geometry                           geometry,
                                                        __in __notnull collada_data_scene_graph_node_material_instance material_instance);

/** TODO */
PUBLIC collada_data_geometry collada_data_geometry_create_async(__in    __notnull tinyxml2::XMLElement*   xml_element_ptr,
                                                                __in    __notnull collada_data            collada_data,
                                                                __in    __notnull system_event            geometry_processed_event,
                                                                __inout __notnull volatile unsigned int*  n_geometry_elements_processed_ptr,
                                                                __inout __notnull const    unsigned int*  n_geometry_elements_ptr,
                                                                __in    __notnull system_resizable_vector result_geometries,
                                                                __in    __notnull system_hash64map        result_geometries_by_id_map);

/** TODO */
PUBLIC void collada_data_geometry_get_property(__in __notnull const collada_data_geometry          geometry,
                                               __in                 collada_data_geometry_property property,
                                               __out                void*                          out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_geometry_get_properties(__in  __notnull collada_data_geometry       geometry,
                                                             __out_opt       system_hashed_ansi_string*  out_name,
                                                             __out_opt       collada_data_geometry_mesh* out_geometry_mesh,
                                                             __out_opt       unsigned int*               out_n_material_instances);

/** TODO */
PUBLIC EMERALD_API bool collada_data_geometry_get_material_binding_index_by_input_semantic_name(__in  __notnull collada_data_scene_graph_node_material_instance instance,
                                                                                                __in            unsigned int                                    input_set,
                                                                                                __in  __notnull system_hashed_ansi_string                       input_semantic_name,
                                                                                                __out __notnull unsigned int*                                   out_binding_index_ptr);

/** TODO */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance(__in __notnull collada_data_geometry geometry,
                                                                                                               __in           unsigned int          n_material_instance);

/** TODO */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance_by_symbol_name(__in __notnull collada_data_geometry     geometry,
                                                                                                                              __in __notnull system_hashed_ansi_string symbol_name);

/** TODO */
PUBLIC void collada_data_geometry_release(__in __notnull __post_invalid collada_data_geometry geometry);

/** TODO */
PUBLIC void collada_data_geometry_set_property(__in __notnull collada_data_geometry          geometry,
                                               __in           collada_data_geometry_property property,
                                               __in __notnull void*                          data);

#endif /* COLLADA_DATA_GEOMETRY_H */
