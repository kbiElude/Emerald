/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_TRANSFORMATION_H
#define COLLADA_DATA_TRANSFORMATION_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

typedef enum
{
    COLLADA_DATA_TRANSFORMATION_PROPERTY_DATA_HANDLE,
    COLLADA_DATA_TRANSFORMATION_PROPERTY_SID,
    COLLADA_DATA_TRANSFORMATION_PROPERTY_TYPE,
} collada_data_transformation_property;

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_lookat(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(9) __notnull const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_matrix(__in            __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(16) __notnull const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_rotate(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                             __in_ecount(4) __notnull const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_scale(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                            __in_ecount(3) __notnull const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_skew(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                           __in_ecount(7) __notnull const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_translate(__in           __notnull tinyxml2::XMLElement* element_ptr,
                                                                                __in_ecount(3) __notnull const float*          data);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_matrix_properties(__in                 __notnull collada_data_transformation transformation,
                                                                          __out_ecount_opt(16)           collada_value*              out_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_property(__in  __notnull collada_data_transformation          transformation,
                                                                 __in  __notnull collada_data_transformation_property property,
                                                                 __out __notnull void*                                out_result);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_rotate_properties(__in            __notnull collada_data_transformation transformation,
                                                                          __out_ecount(3) __notnull collada_value*              axis_vector,
                                                                          __out           __notnull collada_value*              angle);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_scale_properties(__in            __notnull collada_data_transformation transformation,
                                                                         __out_ecount(3) __notnull collada_value*              scale_vector);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_translate_properties(__in            __notnull collada_data_transformation transformation,
                                                                             __out_ecount(3) __notnull collada_value*              translation_vector);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_type(__in __notnull collada_data_transformation,
                                                             __out_opt      _collada_data_transformation_type* out_type);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_release(__in __notnull __post_invalid collada_data_transformation);

#endif /* COLLADA_DATA_TRANSFORMATION_H */
