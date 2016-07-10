/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_VALUE_H
#define COLLADA_VALUE_H

#include "collada/collada_types.h"

typedef enum
{
    COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION,
    COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
    COLLADA_VALUE_PROPERTY_TYPE,
} collada_value_property;

typedef enum
{
    COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
    COLLADA_VALUE_TYPE_FLOAT,

    COLLADA_VALUE_TYPE_UNKNOWN
} collada_value_type;

/** TODO */
PUBLIC collada_value collada_value_create(collada_value_type type,
                                          const void*        data_ptr);

/** TODO */
PUBLIC void collada_value_get_property(const collada_value    value,
                                       collada_value_property property,
                                       void*                  out_result_ptr);

/** TODO */
PUBLIC void collada_value_release(collada_value value);

/** TODO */
PUBLIC void collada_value_set(collada_value      value,
                              collada_value_type type,
                              const void*        data_ptr);

#endif /* COLLADA_VALUE_H */
