/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_INPUT_H
#define COLLADA_DATA_INPUT_H

#include "collada_data_geometry.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

/** TODO */
PUBLIC void collada_data_input_add_input_set(collada_data_input     input,
                                             unsigned int           n_set_id,
                                             collada_data_input_set input_set);

/** TODO */
PUBLIC collada_data_input collada_data_input_create(_collada_data_input_type type);

/** TODO */
PUBLIC collada_data_input_set collada_data_input_set_create(int                 offset,
                                                            collada_data_source mesh_source);

/** TODO */
PUBLIC void collada_data_input_get_properties(collada_data_input        input,
                                              unsigned int              n_set,
                                              unsigned int*             out_n_sets,
                                              int*                      out_offset,
                                              collada_data_source*      out_source,
                                              _collada_data_input_type* out_type);

/** TODO */
PUBLIC _collada_data_input_type collada_data_input_convert_from_string(system_hashed_ansi_string input_type_string);

#endif /* COLLADA_DATA_GEOMETRY_H */
