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
PUBLIC void collada_data_input_add_input_set(__in __notnull collada_data_input     input,
                                             __in           unsigned int           n_set_id,
                                             __in __notnull collada_data_input_set input_set);

/** TODO */
PUBLIC collada_data_input collada_data_input_create(__in _collada_data_input_type type);

/** TODO */
PUBLIC collada_data_input_set collada_data_input_set_create(__in           int                 offset,
                                                            __in __notnull collada_data_source mesh_source);

/** TODO */
PUBLIC void collada_data_input_get_properties(__in      __notnull collada_data_input        input,
                                              __in                unsigned int              n_set,
                                              __out_opt           unsigned int*             out_n_sets,
                                              __out_opt           int*                      out_offset,
                                              __out_opt           collada_data_source*      out_source,
                                              __out_opt           _collada_data_input_type* out_type);

/** TODO */
PUBLIC _collada_data_input_type collada_data_input_convert_from_string(__in __notnull system_hashed_ansi_string input_type_string);

#endif /* COLLADA_DATA_GEOMETRY_H */
