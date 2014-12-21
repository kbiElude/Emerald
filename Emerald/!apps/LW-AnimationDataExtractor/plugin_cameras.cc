#define _MSWIN

#include "shared.h"

#include "plugin.h"
#include "plugin_cameras.h"
#include "plugin_common.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"

/** TODO */
void FillSceneWithCameraData(__in __notnull scene            in_scene,
                             __in __notnull system_hash64map envelope_id_to_curve_container_map)
{
    /* Extract available cameras */
    LWItemID camera_item_id = item_info_ptr->first(LWI_CAMERA,
                                                   LWITEM_NULL);

    while (camera_item_id != LWITEM_NULL)
    {
        /* Extract camera name */
        const char*               camera_name     = item_info_ptr->name(camera_item_id);
        system_hashed_ansi_string camera_name_has = system_hashed_ansi_string_create(camera_name);

        /* Instantiate a new camera */
        scene_camera new_camera = scene_camera_create(camera_name_has);

        ASSERT_ALWAYS_SYNC(new_camera != NULL,
                           "scene_camera_create() failed.");

        /* Retrieve position curves */
        curve_container new_camera_position_x = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_X,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_position_y = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_Y,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_position_z = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_TRANSLATION_Z,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);

        /* Retrieve rotation curves */
        curve_container new_camera_rotation_b = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_B,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_rotation_h = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_H,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);
        curve_container new_camera_rotation_p = GetCurveContainerForProperty(camera_name_has,
                                                                             ITEM_PROPERTY_ROTATION_P,
                                                                             camera_item_id,
                                                                             envelope_id_to_curve_container_map);

        /* Retrieve F-Stop data */
        curve_container new_camera_fstop = GetCurveContainerForProperty(camera_name_has,
                                                                        ITEM_PROPERTY_F_STOP,
                                                                        camera_item_id,
                                                                        envelope_id_to_curve_container_map);

        /* Retrieve Focal Distance data */
        curve_container new_camera_focal_distance = GetCurveContainerForProperty(camera_name_has,
                                                                                 ITEM_PROPERTY_FOCAL_DISTANCE,
                                                                                 camera_item_id,
                                                                                 envelope_id_to_curve_container_map);

        /* Configure the camera using the properties we've retrieved */
        curve_container new_camera_positions[] =
        {
            new_camera_position_x,
            new_camera_position_y,
            new_camera_position_z,
        };
        curve_container new_camera_rotations[] =
        {
            new_camera_rotation_h,
            new_camera_rotation_p,
            new_camera_rotation_b
        };

        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_POSITION,
                                  new_camera_positions);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_ROTATION,
                                  new_camera_rotations);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,
                                 &new_camera_focal_distance);
        scene_camera_set_property(new_camera,
                                  SCENE_CAMERA_PROPERTY_F_STOP,
                                 &new_camera_fstop);

        /* Add the camera */
        if (!scene_add_camera(in_scene,
                              new_camera) )
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not add new camera [%s]",
                               camera_name);
        }

        /* Move on */
        camera_item_id = item_info_ptr->next(camera_item_id);
    }
}


