/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_POLYLIST_H
#define COLLADA_DATA_POLYLIST_H

#include "collada_data_geometry.h"
#include "collada_data_input.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_polylist_property
{
    /* not settable, unsigned int* */
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_DATA,

    /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MAXIMUM_VALUE,

    /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MINIMUM_VALUE,

    /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME,

    /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_N_INDICES,

    /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_N_INPUTS,
};

/** TODO */
PUBLIC collada_data_polylist collada_data_polylist_create(tinyxml2::XMLElement*      polylist_element_ptr,
                                                          collada_data_geometry_mesh geometry_mesh,
                                                          collada_data               data);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_input(collada_data_polylist    polylist,
                                                        _collada_data_input_type type,
                                                        collada_data_input*      out_input_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_input_types(collada_data_polylist     polylist,
                                                              unsigned int*             out_n_input_types_ptr,
                                                              _collada_data_input_type* out_input_types_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_property(collada_data_polylist          polylist,
                                                           collada_data_polylist_property property,
                                                           void*                          out_result_ptr);

/** TODO */
PUBLIC void collada_data_polylist_release(collada_data_polylist polylist);

#endif /* COLLADA_DATA_GEOMETRY_H */
