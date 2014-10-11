/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_CAMERA_H
#define COLLADA_DATA_CAMERA_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

typedef enum
{
    COLLADA_DATA_CAMERA_PROPERTY_AR,
    COLLADA_DATA_CAMERA_PROPERTY_ID,
    COLLADA_DATA_CAMERA_PROPERTY_NAME,
    COLLADA_DATA_CAMERA_PROPERTY_XFOV,
    COLLADA_DATA_CAMERA_PROPERTY_YFOV,
    COLLADA_DATA_CAMERA_PROPERTY_ZFAR,
    COLLADA_DATA_CAMERA_PROPERTY_ZNEAR
} collada_data_camera_property;

/** TODO */
PUBLIC collada_data_camera collada_data_camera_create(__in __notnull tinyxml2::XMLElement* current_camera_element_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_camera_get_property(__in      __notnull collada_data_camera          camera,
                                                         __in                collada_data_camera_property property,
                                                         __out_opt           void*                        out_result);

/** TODO */
PUBLIC void collada_data_camera_release(__in __post_invalid collada_data_camera camera);

#endif /* COLLADA_DATA_CAMERA_H */
