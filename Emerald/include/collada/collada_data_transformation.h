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
PUBLIC collada_data_transformation collada_data_transformation_create_lookat(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_matrix(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_rotate(tinyxml2::XMLElement* element_ptr,
                                                                             const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_scale(tinyxml2::XMLElement* element_ptr,
                                                                            const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_skew(tinyxml2::XMLElement* element_ptr,
                                                                           const float*          data);

/** TODO */
PUBLIC collada_data_transformation collada_data_transformation_create_translate(tinyxml2::XMLElement* element_ptr,
                                                                                const float*          data);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_matrix_properties(collada_data_transformation transformation,
                                                                          collada_value*              out_data);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_property(collada_data_transformation          transformation,
                                                                 collada_data_transformation_property property,
                                                                 void*                                out_result);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_rotate_properties(collada_data_transformation transformation,
                                                                          collada_value*              axis_vector,
                                                                          collada_value*              angle);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_scale_properties(collada_data_transformation transformation,
                                                                         collada_value*              scale_vector);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_translate_properties(collada_data_transformation transformation,
                                                                             collada_value*              translation_vector);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_get_type(collada_data_transformation,
                                                             _collada_data_transformation_type* out_type);

/** TODO */
PUBLIC EMERALD_API void collada_data_transformation_release(collada_data_transformation);

#endif /* COLLADA_DATA_TRANSFORMATION_H */
