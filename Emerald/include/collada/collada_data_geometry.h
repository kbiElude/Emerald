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
PUBLIC void collada_data_geometry_add_material_instance(collada_data_geometry                           geometry,
                                                        collada_data_scene_graph_node_material_instance material_instance);

/** TODO */
PUBLIC collada_data_geometry collada_data_geometry_create_async(tinyxml2::XMLElement*   xml_element_ptr,
                                                                collada_data            collada_data,
                                                                system_event            geometry_processed_event,
                                                                volatile unsigned int*  n_geometry_elements_processed_ptr,
                                                                const    unsigned int*  n_geometry_elements_ptr,
                                                                system_resizable_vector result_geometries,
                                                                system_hash64map        result_geometries_by_id_map);

/** TODO */
PUBLIC void collada_data_geometry_get_property(const collada_data_geometry          geometry,
                                                     collada_data_geometry_property property,
                                                     void*                          out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_geometry_get_properties(collada_data_geometry       geometry,
                                                             system_hashed_ansi_string*  out_name,
                                                             collada_data_geometry_mesh* out_geometry_mesh,
                                                             unsigned int*               out_n_material_instances);

/** TODO */
PUBLIC EMERALD_API bool collada_data_geometry_get_material_binding_index_by_input_semantic_name(collada_data_scene_graph_node_material_instance instance,
                                                                                                unsigned int                                    input_set,
                                                                                                system_hashed_ansi_string                       input_semantic_name,
                                                                                                unsigned int*                                   out_binding_index_ptr);

/** TODO */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance(collada_data_geometry geometry,
                                                                                                               unsigned int          n_material_instance);

/** TODO */
PUBLIC EMERALD_API collada_data_scene_graph_node_material_instance collada_data_geometry_get_material_instance_by_symbol_name(collada_data_geometry     geometry,
                                                                                                                              system_hashed_ansi_string symbol_name);

/** TODO */
PUBLIC void collada_data_geometry_release(collada_data_geometry geometry);

/** TODO */
PUBLIC void collada_data_geometry_set_property(collada_data_geometry          geometry,
                                               collada_data_geometry_property property,
                                               void*                          data);

#endif /* COLLADA_DATA_GEOMETRY_H */
