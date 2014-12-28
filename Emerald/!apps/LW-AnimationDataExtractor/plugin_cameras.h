/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_CAMERAS_H
#define PLUGIN_CAMERAS_H

#include "scene/scene_types.h"
#include "system/system_types.h"

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
PUBLIC void GetCameraPropertyValue(__in  __notnull scene_camera   camera,
                                   __in            CameraProperty property,
                                   __out __notnull void*          out_result);

/** TODO */
PUBLIC void InitCameraData();

/** TODO */
PUBLIC system_event StartCameraDataExtraction(__in __notnull scene in_scene);

#endif /* PLUGIN_CAMERAS_H */