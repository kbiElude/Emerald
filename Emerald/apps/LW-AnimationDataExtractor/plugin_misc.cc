#define _MSWIN

#include "shared.h"
#include "plugin.h"
#include "plugin_misc.h"
#include "mesh/mesh.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
void FillMiscellaneousData(scene in_scene)
{
    float duration = (float) (interface_info_ptr->previewEnd - interface_info_ptr->previewStart) /
                     (float) (scene_info_ptr->framesPerSecond);
    float fps      = (float)  scene_info_ptr->framesPerSecond;

    scene_set_property(in_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &duration);
    scene_set_property(in_scene,
                       SCENE_PROPERTY_FPS,
                      &fps);

    /* Set "safe" near/far span for the scene.
     *
     * To do this the correct way, we'd need to do these calculations per frame
     * for all visible objects. This will be implemented at some point in the future.
     * For now, this will have to suffice :)
     */
          unsigned int n_cameras = 0;
    const float        zfar      = 10000.0f;
    const float        znear     = 0.01f;

    scene_get_property(in_scene,
                       SCENE_PROPERTY_N_CAMERAS,
                      &n_cameras);

    for (unsigned int n_camera = 0;
                      n_camera < n_cameras;
                    ++n_camera)
    {
        scene_camera current_scene_camera = NULL;

        current_scene_camera = scene_get_camera_by_index(in_scene,
                                                         n_camera);

        ASSERT_DEBUG_SYNC(current_scene_camera != NULL,
                          "Could not extract scene camera");
        if (current_scene_camera == NULL)
        {
            continue;
        }

        scene_camera_set_property(current_scene_camera,
                                  SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,
                                 &znear);
        scene_camera_set_property(current_scene_camera,
                                  SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,
                                 &zfar);
    } /* for (all cameras) */
}


