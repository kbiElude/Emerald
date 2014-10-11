/**
 *
 * Emerald (kbi/elude @2014)
 *
 * TODO: Floats are temporary. IN the longer run, we'll want these replaced with curves.
 */
#ifndef SCENE_LIGHT_H
#define SCENE_LIGHT_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_light, scene_light)


typedef enum scene_light_property
{
    SCENE_LIGHT_PROPERTY_DIFFUSE,   /* Settable,     float[3] */
    SCENE_LIGHT_PROPERTY_DIRECTION, /* Settable,     float[3] */
    SCENE_LIGHT_PROPERTY_TYPE,      /* Not settable, scene_light_type */

    /* Always last */
    SCENE_LIGHT_PROPERTY_COUNT
};

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_directional(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_light_get_property(__in  __notnull scene_light,
                                                 __in            scene_light_property,
                                                 __out __notnull void*);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_light scene_light_load(__in __notnull system_file_serializer serializer);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 **/
PUBLIC bool scene_light_save(__in __notnull system_file_serializer serializer,
                             __in __notnull const scene_light      light);

/** TODO */
PUBLIC EMERALD_API void scene_light_set_property(__in __notnull scene_light,
                                                 __in           scene_light_property,
                                                 __in __notnull const void*);

#endif /* SCENE_CAMERA_H */
