/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_CAMERAS_H
#define PLUGIN_CAMERAS_H

#include "scene/scene_types.h"

typedef enum
{
    CAMERA_PROPERTY_OBJECT_ID,           /* void*              */
    CAMERA_PROPERTY_PARENT_OBJECT_ID,    /* void*              */
    CAMERA_PROPERTY_ROTATION_HPB_CURVES, /* curve_container[3] */
    CAMERA_PROPERTY_TRANSLATION_CURVES,  /* curve_container[3] */
} CameraProperty;

/** TODO */
PUBLIC void DeinitCameraData();

/** TODO */
PUBLIC void FillSceneWithCameraData(__in __notnull scene            in_scene,
                                    __in __notnull system_hash64map curve_id_to_curve_container_map);

/** TODO */
PUBLIC void GetCameraPropertyValue(__in  __notnull scene_camera   camera,
                                   __in            CameraProperty property,
                                   __out __notnull void*          out_result);

/** TODO */
PUBLIC void InitCameraData();

#endif /* PLUGIN_CAMERAS_H */