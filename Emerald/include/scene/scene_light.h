/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SCENE_LIGHT_H
#define SCENE_LIGHT_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_light, scene_light)


typedef enum scene_light_property
{
    SCENE_LIGHT_PROPERTY_COLOR,                 /* Settable,     curve_container[3]         */
    SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,       /* Settable,     curve_container            */
    SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION,  /* Settable,     curve_container            */
    SCENE_LIGHT_PROPERTY_DIRECTION,             /* Settable,     float[3]. Set in run-time  */
    SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION,    /* Settable,     curve_container            */
    SCENE_LIGHT_PROPERTY_NAME,                  /* Not settable, system_hashed_ansi_string  */
    SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION, /* Settable,     curve_container            */
    SCENE_LIGHT_PROPERTY_TYPE,                  /* Not settable, scene_light_type           */
    SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,       /* Settable,     bool                       */

    /* NOTE: This property is set during run-time by ogl_scene_renderer and is NOT
     *       serialized. It acts merely as a communication mean between the scene
     *       graph traversation layer and actual renderer.
     *
     * This property is only accepted for point & spot lights. Any attempt to access it
     * for a directional light will throw assertion failure. Getters will return
     * undefined content, if executed for incorrect light types.
     */
    SCENE_LIGHT_PROPERTY_POSITION,              /* Settable,     float[3]. Set in run-time */

    /* Always last */
    SCENE_LIGHT_PROPERTY_COUNT
};

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_ambient(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_directional(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_point(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_light_get_property(__in  __notnull scene_light,
                                                 __in            scene_light_property,
                                                 __out __notnull void*);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_light scene_light_load(__in     __notnull system_file_serializer serializer,
                                    __in_opt           scene                  owner_scene);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 **/
PUBLIC bool scene_light_save(__in     __notnull system_file_serializer serializer,
                             __in     __notnull const scene_light      light,
                             __in_opt           scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_light_set_property(__in __notnull scene_light,
                                                 __in           scene_light_property,
                                                 __in __notnull const void*);

#endif /* SCENE_CAMERA_H */
