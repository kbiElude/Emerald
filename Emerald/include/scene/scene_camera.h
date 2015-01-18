/**
 *
 * Emerald (kbi/elude @2014)
 *
 * TODO: Floats are temporary. IN the longer run, we'll want these replaced with curves.
 */
#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_camera, scene_camera)

enum scene_camera_property
{
    SCENE_CAMERA_PROPERTY_ASPECT_RATIO,                   /*     settable, float */
    SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,             /*     settable, float */
    SCENE_CAMERA_PROPERTY_FAR_PLANE_HEIGHT,               /* not settable, float */
    SCENE_CAMERA_PROPERTY_FAR_PLANE_WIDTH,                /* not settable, float */
    SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,                 /*     settable, curve_container */
    SCENE_CAMERA_PROPERTY_F_STOP,                         /*     settable, curve_container */
    SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID,               /* not settable, float[3].
                                                           *
                                                           * Throws an assertion failure if use_camera_physical_props is on */
    SCENE_CAMERA_PROPERTY_NAME,                           /* not settable, system_hashed_ansi_string */
    SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,            /*     settable, float */
    SCENE_CAMERA_PROPERTY_NEAR_PLANE_HEIGHT,              /* not settable, float */
    SCENE_CAMERA_PROPERTY_NEAR_PLANE_WIDTH,               /* not settable, float */
    SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,               /*     settable, scene_graph_node */
    SCENE_CAMERA_PROPERTY_TYPE,                           /*     settable, _scene_camera_type */
    SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES, /*     settable, bool. Causes getters for:
                                                           *
                                                           * - SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE
                                                           * - SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE
                                                           *
                                                           * to calculate the requested values using
                                                           * F-Stop, Focal Distance and Zoom Factor curves.
                                                           */
    SCENE_CAMERA_PROPERTY_USE_CUSTOM_VERTICAL_FOV,        /*     settable, bool */
    SCENE_CAMERA_PROPERTY_VERTICAL_FOV_CUSTOM,            /*     settable, curve_container;
                                                           *
                                                           * Getter will throw an assertion failure if use_custom_vertical_fov
                                                           * is false.
                                                           **/
    SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,  /* not settable, float.
                                                           *
                                                           * Getter will throw an assertion failure if use_custom_vertical_fov
                                                           * is true.
                                                           */
    SCENE_CAMERA_PROPERTY_ZOOM_FACTOR                     /* not settable, curve_container */
};

/** TODO */
PUBLIC EMERALD_API scene_camera scene_camera_create(__in __notnull system_hashed_ansi_string name);

/** TODO.
 *
 *  NOTE: Some properties may need a recalc, which is why the function takes @param time.
 **/
PUBLIC EMERALD_API void scene_camera_get_property(__in  __notnull scene_camera          camera,
                                                  __in            scene_camera_property property,
                                                  __in            system_timeline_time  time,
                                                  __out __notnull void*                 out_result);

/** TODO */
PUBLIC scene_camera scene_camera_load(__in     __notnull system_file_serializer serializer,
                                      __in_opt           scene                  owner_scene);

/** TODO
 *
 *  NOTE: This function is for internal use only.
 **/
PUBLIC bool scene_camera_save(__in __notnull system_file_serializer serializer,
                              __in __notnull const scene_camera     camera,
                              __in __notnull scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_camera_set_property(__in __notnull scene_camera          camera,
                                                  __in           scene_camera_property property,
                                                  __in __notnull const void*           data);

#endif /* SCENE_CAMERA_H */
