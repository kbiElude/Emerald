/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_VALUE_H
#define COLLADA_VALUE_H

#include "collada/collada_types.h"

typedef enum collada_value_property
{
    COLLADA_VALUE_PROPERTY_COLLADA_DATA_ANIMATION,
    COLLADA_VALUE_PROPERTY_FLOAT_VALUE,
    COLLADA_VALUE_PROPERTY_TYPE,
};

typedef enum collada_value_type
{
    COLLADA_VALUE_TYPE_COLLADA_DATA_ANIMATION,
    COLLADA_VALUE_TYPE_FLOAT,

    COLLADA_VALUE_TYPE_UNKNOWN
};

/** TODO */
PUBLIC collada_value collada_value_create(__in           collada_value_type type,
                                          __in __notnull const void*        data_ptr);

/** TODO */
PUBLIC void collada_value_get_property(__in  __notnull const collada_value    value,
                                       __in            collada_value_property property,
                                       __out __notnull void*                  out_result);

/** TODO */
PUBLIC void collada_value_release(__in __notnull collada_value);

/** TODO */
PUBLIC void collada_value_set(__in __notnull collada_value      value,
                              __in           collada_value_type type,
                              __in __notnull const void*        data_ptr);

#endif /* COLLADA_VALUE_H */
