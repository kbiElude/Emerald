/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SOURCE_H
#define COLLADA_DATA_SOURCE_H

#include "collada/collada_types.h"
#include "collada/collada_data_float_array.h"
#include "tinyxml2.h"

enum collada_data_source_property
{
    COLLADA_DATA_SOURCE_PROPERTY_ID,

    /* Always last */
    COLLADA_DATA_SOURCE_PROPERTY_COUNT
};

/** TODO */
PUBLIC collada_data_source collada_data_source_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                      __in __notnull collada_data              data,
                                                      __in __notnull system_hashed_ansi_string parent_geometry_name);

/** TODO */
PUBLIC void collada_data_source_get_source_float_data(__in      __notnull collada_data_source       source,
                                                      __out_opt           collada_data_float_array* out_float_array_ptr);

/** TODO */
PUBLIC void collada_data_source_get_source_name_data(__in      __notnull collada_data_source      source,
                                                     __out_opt           collada_data_name_array* out_name_array_ptr);

/** TODO */
PUBLIC void collada_data_source_get_property(__in  __notnull collada_data_source          source,
                                             __in            collada_data_source_property property,
                                             __out __notnull void*                        result_ptr);

/** TODO */
PUBLIC void collada_data_source_release(__in __notnull __post_invalid collada_data_source source);

#endif /* COLLADA_DATA_SOURCE_H */
