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

typedef enum
{
    SCENE_LIGHT_TYPE_AMBIENT,
    SCENE_LIGHT_TYPE_DIRECTIONAL,
    SCENE_LIGHT_TYPE_POINT,

    SCENE_LIGHT_TYPE_UNKNOWN
} scene_light_type;

#endif /* SCENE_TYPES_H */
