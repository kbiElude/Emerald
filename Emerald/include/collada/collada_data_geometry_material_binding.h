/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H
#define COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

/** TODO */
PUBLIC collada_data_geometry_material_binding collada_data_geometry_material_binding_create(system_hashed_ansi_string input_semantic_name,
                                                                                            unsigned int              input_set,
                                                                                            system_hashed_ansi_string semantic_name);

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_geometry_material_binding_get_properties(collada_data_geometry_material_binding binding,
                                                                              system_hashed_ansi_string*             out_input_semantic_name_ptr,
                                                                              unsigned int*                          out_input_set_ptr,
                                                                              system_hashed_ansi_string*             out_semantic_name_ptr);

/** TODO */
PUBLIC void collada_data_geometry_material_binding_release(collada_data_geometry_material_binding binding);

#endif /* COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H */
