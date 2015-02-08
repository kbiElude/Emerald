/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef SCENE_TYPES_H
#define SCENE_TYPES_H

#include "system/system_types.h"


DECLARE_HANDLE(scene);
DECLARE_HANDLE(scene_camera);
DECLARE_HANDLE(scene_collada_loader);
DECLARE_HANDLE(scene_curve);
DECLARE_HANDLE(scene_graph);
DECLARE_HANDLE(scene_graph_node);
DECLARE_HANDLE(scene_light);
DECLARE_HANDLE(scene_material);
DECLARE_HANDLE(scene_mesh);
DECLARE_HANDLE(scene_texture);

typedef uint32_t scene_curve_id;

typedef enum
{
    SCENE_CAMERA_TYPE_ORTHOGONAL,
    SCENE_CAMERA_TYPE_PERSPECTIVE,

    SCENE_CAMERA_TYPE_UNDEFINED
} _scene_camera_type;

/* Defines falloff behavior for point and spot lights. Ignored for
 * other light types.
 */
typedef enum
{
    /* Falloff = 1 */
    SCENE_LIGHT_FALLOFF_OFF,

    /* Falloff = custom: 1 / (constant_att + linear_att * dist + quadratic_att * dist^2). */
    SCENE_LIGHT_FALLOFF_CUSTOM,

    /* Falloff = clamp(1 - (distance from the light) / (distance at which the intensity of the light falls to 0), 0, 1) */
    SCENE_LIGHT_FALLOFF_LINEAR,

    /* Falloff = (distance at which the light intensity equals the user's intensity setting) / (distance from the light) */
    SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE,

    /* Falloff = squared( (distance at which the light intensity equals the user's intensity setting) / (distance from the light) ) */
    SCENE_LIGHT_FALLOFF_INVERSED_DISTANCE_SQUARE,

    SCENE_LIGHT_FALLOFF_UNKNOWN
} scene_light_falloff;

typedef enum
{
    /* No bias at all */
    SCENE_LIGHT_SHADOW_MAP_BIAS_NONE,
    /* Uses a constant bias value */
    SCENE_LIGHT_SHADOW_MAP_BIAS_CONSTANT,
    /* Uses a bias value that adapts on a per-sample basis (acos(tan(dot(normal, light))) */
    SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE,
    /* Uses a bias value that adapts on a per-sample basis (approximation of ADAPTIVE) */
    SCENE_LIGHT_SHADOW_MAP_BIAS_ADAPTIVE_FAST,

    SCENE_LIGHT_SHADOW_MAP_BIAS_UNKNOWN
} scene_light_shadow_map_bias;

typedef enum
{
    SCENE_LIGHT_TYPE_AMBIENT,
    SCENE_LIGHT_TYPE_DIRECTIONAL,
    SCENE_LIGHT_TYPE_POINT,
    SCENE_LIGHT_TYPE_SPOT,

    SCENE_LIGHT_TYPE_UNKNOWN
} scene_light_type;

#endif /* SCENE_TYPES_H */
