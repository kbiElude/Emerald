/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_LIGHT_H
#define COLLADA_DATA_LIGHT_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_light_property
{
    COLLADA_DATA_LIGHT_PROPERTY_COLOR,                 /* not settable, float[3] */
    COLLADA_DATA_LIGHT_PROPERTY_CONSTANT_ATTENUATION,  /* not settable, float */
    COLLADA_DATA_LIGHT_PROPERTY_FALLOFF_ANGLE,         /* not settable, float, degrees */
    COLLADA_DATA_LIGHT_PROPERTY_FALLOFF_EXPONENT,      /* not settable, float */
    COLLADA_DATA_LIGHT_PROPERTY_ID,                    /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_LIGHT_PROPERTY_NAME,                  /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_LIGHT_PROPERTY_LINEAR_ATTENUATION,    /* not settable, float */
    COLLADA_DATA_LIGHT_PROPERTY_QUADRATIC_ATTENUATION, /* not settable, float */
    COLLADA_DATA_LIGHT_PROPERTY_TYPE,                  /* not settable, collada_data_light_type */
    COLLADA_DATA_LIGHT_PROPERTY_ZFAR,                  /* not settable, float */

    /* Always last */
    COLLADA_DATA_LIGHT_PROPERTY_COUNT
};

enum collada_data_light_type
{
    /* An ambient light source radiates light from all directions at once. The intensity of an ambient light
     * source is not attenuated.
     */
    COLLADA_DATA_LIGHT_TYPE_AMBIENT,

    /* A directional light source radiates light in one direction from a known direction in space that is
     * infinitely far away. The intensity of a directional light source is not attenuated.
     */
    COLLADA_DATA_LIGHT_TYPE_DIRECTIONAL,

    /* A point light source radiates light in all directions from a known location in space. The intensity of a
     * point light source is attenuated as the distance to the light source increases.     */
    COLLADA_DATA_LIGHT_TYPE_POINT,

    /* A spot light source radiates light in one direction from a known location in space. The light radiates
     * from the spot light source in a cone shape. The intensity of the light is attenuated as the radiation
     * angle increases away from the direction of the light source. The intensity of a spot light source is
     * also attenuated as the distance to the light source increases.     */
    COLLADA_DATA_LIGHT_TYPE_SPOT,

    /* Always last */
    COLLADA_DATA_LIGHT_TYPE_UNKNOWN
};

/** TODO */
PUBLIC collada_data_light collada_data_light_create(__in __notnull tinyxml2::XMLElement* current_light_element_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_light_get_property(__in  __notnull const collada_data_light          light,
                                                        __in                  collada_data_light_property property,
                                                        __out __notnull void*                             out_data_ptr);

/** TODO */
PUBLIC void collada_data_light_release(__in __notnull __post_invalid collada_data_light light);

#endif /* COLLADA_DATA_LIGHT_H */
