/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_RENDERER_TYPES_H
#define SCENE_RENDERER_TYPES_H

#include "system/system_types.h"


DECLARE_HANDLE(scene_renderer);
DECLARE_HANDLE(scene_renderer_bbox_preview);
DECLARE_HANDLE(scene_renderer_frustum_preview);
DECLARE_HANDLE(scene_renderer_lights_preview);
DECLARE_HANDLE(scene_renderer_normals_preview);
DECLARE_HANDLE(scene_renderer_uber);

typedef enum
{
    /* Passes all objects that are located in front of the current camera.
     *
     * behavior_data parameter: [0]..[2]: world-space location of the camera
     *                          [3]..[5]: normalized view direction vector.
     *
     * This behavior is required for DPSM shadow map generation passes, where
     * there is no projection matrix available (the projection is done directly
     * in VS)
     */
    SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_PASS_OBJECTS_IN_FRONT_OF_CAMERA,

    /* Extracts clipping planes from current projection matrix and checks
     * if the pointed mesh intersects or is fully embedded within the frustum.
     *
     * behavior_data parameter: ignored
     *
     * Recommended culling behavior, as of now.
     */
    SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_USE_CAMERA_CLIPPING_PLANES,

    SCENE_RENDERER_FRUSTUM_CULLING_BEHAVIOR_UNKNOWN
} scene_renderer_frustum_culling_behavior;

typedef enum
{
    HELPER_VISUALIZATION_NONE           = 0,
    HELPER_VISUALIZATION_BOUNDING_BOXES = 1 << 0,
    HELPER_VISUALIZATION_FRUSTUMS       = 1 << 3,
    HELPER_VISUALIZATION_NORMALS        = 1 << 1, /* Only supported for regualr meshes */
    HELPER_VISUALIZATION_LIGHTS         = 1 << 2,
} scene_renderer_helper_visualization;

typedef enum
{
    /* All meshes will be forward-rendered */
    RENDER_MODE_FORWARD_WITH_DEPTH_PREPASS,
    RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,

    /* TODO: RENDER_MODE_DEFERRED */

    /* TODO: RENDER_MODE_INFERRED? */

    /* (debug) All meshes will be shaded with their normal data */
    RENDER_MODE_NORMALS_ONLY,
    /* (debug) All meshes will be shaded with their texcoord data */
    RENDER_MODE_TEXCOORDS_ONLY,

    /* Crucial parts of the rendering process are handed over to
     * ogl_shadow_mapping. */
    RENDER_MODE_SHADOW_MAP,

} scene_renderer_render_mode;

#endif /* SCENE_RENDERER_TYPES_H */