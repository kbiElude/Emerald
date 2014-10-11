/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H
#define COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

/** TODO */
PUBLIC collada_data_geometry_material_binding collada_data_geometry_material_binding_create(__in __notnull system_hashed_ansi_string input_semantic_name,
                                                                                            __in           unsigned int              input_set,
                                                                                            __in __notnull system_hashed_ansi_string semantic_name);

/* Please see header for specification */
PUBLIC EMERALD_API void collada_data_geometry_material_binding_get_properties(__in __notnull collada_data_geometry_material_binding binding,
                                                                              __out_opt      system_hashed_ansi_string*             out_input_semantic_name,
                                                                              __out_opt      unsigned int*                          out_input_set,
                                                                              __out_opt      system_hashed_ansi_string*             out_semantic_name);

/** TODO */
PUBLIC void collada_data_geometry_material_binding_release(__in __notnull __post_invalid collada_data_geometry_material_binding binding);

#endif /* COLLADA_DATA_GEOMETRY_MATERIAL_BINDING_H */
