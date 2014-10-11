/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_FLOAT_ARRAY_H
#define COLLADA_DATA_FLOAT_ARRAY_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_float_array_property
{
    COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_COMPONENTS, /* not settable, uint32_t     */
    COLLADA_DATA_FLOAT_ARRAY_PROPERTY_N_VALUES,     /* not settable, uint32_t     */
    COLLADA_DATA_FLOAT_ARRAY_PROPERTY_DATA          /* not settable, const float* */
};

/** TODO.
 *
 *  NOTE: The reason this function takes parent geometry name is that Lightwave COLLADA exporter
 *        uses the same ids for float_array of differing contents..
 **/
PUBLIC collada_data_float_array collada_data_float_array_create(__in __notnull tinyxml2::XMLElement*     float_array_element_ptr,
                                                                __in           unsigned int              n_components,
                                                                __in           unsigned int              stride,
                                                                __in __notnull collada_data              data,
                                                                __in __notnull system_hashed_ansi_string parent_name);

/** TODO */
PUBLIC void collada_data_float_array_get_property(__in  __notnull collada_data_float_array          array,
                                                  __in            collada_data_float_array_property property,
                                                  __out __notnull void*                             out_result);

/** TODO */
PUBLIC void collada_data_float_array_release(__in __notnull __post_invalid collada_data_float_array);

#endif /* COLLADA_DATA_GEOMETRY_H */
