/**
 *
 * Emerald (kbi/elude @2014)
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
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_DATA,           /* not settable, unsigned int* */
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MAXIMUM_VALUE,  /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_INDEX_MINIMUM_VALUE,  /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_MATERIAL_SYMBOL_NAME, /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_POLYLIST_PROPERTY_N_INDICES,            /* not settable, unsigned int */
    COLLADA_DATA_POLYLIST_PROPERTY_N_INPUTS,             /* not settable, unsigned int */
};

/** TODO */
PUBLIC collada_data_polylist collada_data_polylist_create(__in __notnull tinyxml2::XMLElement*      polylist_element_ptr,
                                                          __in __notnull collada_data_geometry_mesh geometry_mesh,
                                                          __in __notnull collada_data               data);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_input(__in      __notnull collada_data_polylist    polylist,
                                                        __in                _collada_data_input_type type,
                                                        __out_opt           collada_data_input*      out_input);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_input_types(__in      __notnull collada_data_polylist     polylist,
                                                              __out_opt           unsigned int*             out_n_input_types,
                                                              __out               _collada_data_input_type* out_input_types);

/** TODO */
PUBLIC EMERALD_API void collada_data_polylist_get_property(__in  __notnull collada_data_polylist          polylist,
                                                           __in            collada_data_polylist_property property,
                                                           __out           void*                          out_result);

/** TODO */
PUBLIC void collada_data_polylist_release(__in __notnull __post_invalid collada_data_polylist polylist);

#endif /* COLLADA_DATA_GEOMETRY_H */
