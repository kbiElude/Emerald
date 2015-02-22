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
                                            /* GENERAL PROPERTIES: */

    /* Settable, curve_container[3] */
    SCENE_LIGHT_PROPERTY_COLOR,

    /* Settable, curve_container */
    SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,

    /* Settable, curve_container (radians).
     *
     * (spot lights only)
     */
    SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF,

    /* Settable, float[3].
     *
     * Set in run-time
     */
    SCENE_LIGHT_PROPERTY_DIRECTION,

    /* Settable, curve_container (radians).
     *
     * (spot lights only)
     */
    SCENE_LIGHT_PROPERTY_EDGE_ANGLE,

    /* Settable, scene_graph_node.
     *
     * Set in run-time.
     */
    SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE,

    /* Not settable, system_hashed_ansi_string */
    SCENE_LIGHT_PROPERTY_NAME,

    /* Settable, curve_container.
     *
     * (point lights only)
     */
    SCENE_LIGHT_PROPERTY_RANGE,

    /* Not settable, scene_light_type */
    SCENE_LIGHT_PROPERTY_TYPE,


                                                /* SHADOW MAPPING: PROPERTIES: */

    /* Settable, float[3]. Set in run-time.
     *
     * NOTE: This property is set during run-time by ogl_scene_renderer and is NOT
     *       serialized. It acts merely as a communication mean between the scene
     *       graph traversation layer and actual renderer.
     *
     * This property is only accepted for point & spot lights. Any attempt to access it
     * for a directional light will throw assertion failure. Getters will return
     * undefined content, if executed for incorrect light types.
     */
    SCENE_LIGHT_PROPERTY_POSITION,

    /* Settable, scene_light_shadow_map_bias. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,

    /* Settable, bool */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES,

    /* Settable, scene_light_shadow_map_filtering. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING,

    /* Settable, ogl_texture_internalformat */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_INTERNALFORMAT,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_PROJECTION,

    /* Settable, uint[2]. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,

    /* Settable, ogl_texture.
     *
     * Set in run-time. Note that SM textures are taken from the texture pool
     * and are returned as soon as the SM-enabled rendering process finishes.
     *
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,

    /* Settable, bool */
    SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,


                                        /* POINT LIGHT SHADOW MAPPING: PROPERTIES: */

    /* Settable, scene_light_shadow_map_pointlight_algorithm. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,

    /* Settable, float. Set in run-time by ogl_shadow_mapping during
     * the shadow map baking phase (for dual paraboloid shadow mapping only).
     * Info necessary to 
     * . */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,

    /* Settable, float.
     *
     * Tweakable near-plane distance for point light shadow mapping. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,


                                        /* SPOT LIGHT SHADOW MAPPING: PROPERTIES: */

    /* Settable, float.
     *
     * Tweakable near-plane distance for spot light shadow mapping. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_SPOTLIGHT_NEAR_PLANE,


                                         /* SHADING: GENERAL PROPERTIES: */

    /* Settable, curve_container.
     *
     * NOTE: Only used if SCENE_LIGHT_PROPERTY_FALLOFF is set at SCENE_LIGHT_FALLOFF_CUSTOM, in which case
     *       attenuation is called as in:
     *
     *       final_att = 1 / (constant_att + linear_att * dist + quadratic_att * dist^2).
     **/
    SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION,
    SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION,
    SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION,

    /* Settable, scene_light_falloff */
    SCENE_LIGHT_PROPERTY_FALLOFF,


    /* Always last */
    SCENE_LIGHT_PROPERTY_COUNT
};

typedef enum
{
    /* 6-pass cube-map-based omnidirectional SM implementation */
    SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL,

    /* 2-pass 2D texture-based omnidirectional SM implementation.
     *
     * This version DOES NOT use geometry shader + layered rendering approach.
     */
     SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID,

     SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN
} scene_light_shadow_map_pointlight_algorithm;

typedef enum
{
    /* Single splat should be enough for everybody! */
    SCENE_LIGHT_SHADOW_MAP_FILTERING_PLAIN,
    /* Well, maybe not? */
    SCENE_LIGHT_SHADOW_MAP_FILTERING_PCF,

    SCENE_LIGHT_SHADOW_MAP_FILTERING_UNKNOWN
} scene_light_shadow_map_filtering;

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_ambient(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_directional(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_point(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_spot(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void scene_light_get_property(__in  __notnull scene_light,
                                                 __in            scene_light_property,
                                                 __out __notnull void*);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_falloff_has(__in scene_light_falloff falloff);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_bias_has(__in scene_light_shadow_map_bias bias);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_filtering_has(__in scene_light_shadow_map_filtering filtering);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_pointlight_algorithm_has(__in scene_light_shadow_map_pointlight_algorithm algorithm);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_type_has(__in scene_light_type light_type);

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
