/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_NAME_ARRAY_H
#define COLLADA_DATA_NAME_ARRAY_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_name_array_property
{
    COLLADA_DATA_NAME_ARRAY_PROPERTY_N_VALUES, /* not settable, uint32_t */
};

/** TODO. **/
PUBLIC collada_data_name_array collada_data_name_array_create(tinyxml2::XMLElement* name_array_element_ptr);

/** TODO */
PUBLIC void collada_data_name_array_get_property(collada_data_name_array          array,
                                                 collada_data_name_array_property property,
                                                 void*                            out_result);

/** TODO */
PUBLIC system_hashed_ansi_string collada_data_name_array_get_value_at_index(collada_data_name_array array,
                                                                            uint32_t                index);

/** TODO */
PUBLIC void collada_data_name_array_release(collada_data_name_array);

#endif /* COLLADA_DATA_NAME_ARRAY_H */
