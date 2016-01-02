/**
 *
 * Emerald (kbi/elude @2014-2015)
 */
#ifndef SCENE_CAMERA_H
#define SCENE_CAMERA_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_camera,
                             scene_camera)

typedef enum
{
    /* "show frustum" setting changed; callback_proc_data: source scene_camera instance */
    SCENE_CAMERA_CALLBACK_ID_SHOW_FRUSTUM_CHANGED,

    /* Always last */
    SCENE_CAMERA_CALLBACK_ID_COUNT
} scene_camera_callback_id;

enum scene_camera_property
{
    /* settable, float.
     *
     * NOTE: If 0.0 is returned for this property, it is caller's responsibility to calculate the AR
     *       using current viewport's size.
     */
    SCENE_CAMERA_PROPERTY_ASPECT_RATIO,

    /* not settable, system_callback_manager */
    SCENE_CAMERA_PROPERTY_CALLBACK_MANAGER,

    /* settable, curve_container */
    SCENE_CAMERA_PROPERTY_F_STOP,

    /* settable, float */
    SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE,

    /* settable, curve_container */
    SCENE_CAMERA_PROPERTY_FOCAL_DISTANCE,

    /* not settable, float[3]. Model space coordinates.*/
    SCENE_CAMERA_PROPERTY_FRUSTUM_CENTROID,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_LEFT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_BOTTOM_RIGHT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_LEFT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_FAR_TOP_RIGHT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_LEFT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_BOTTOM_RIGHT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_LEFT,

    /* not settable, float[3]. Model space coordinates */
    SCENE_CAMERA_PROPERTY_FRUSTUM_NEAR_TOP_RIGHT,

    /* not settable, system_hashed_ansi_string */
    SCENE_CAMERA_PROPERTY_NAME,

    /* settable, float */
    SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE,

    /* settable, scene_graph_node */
    SCENE_CAMERA_PROPERTY_OWNER_GRAPH_NODE,

    /* settable, bool; NOT serialized. */
    SCENE_CAMERA_PROPERTY_SHOW_FRUSTUM,

    /* settable, _scene_camera_type */
    SCENE_CAMERA_PROPERTY_TYPE,

    /* settable, bool.
     *
     * Causes getters for:
     *
     * - SCENE_CAMERA_PROPERTY_FAR_PLANE_DISTANCE
     * - SCENE_CAMERA_PROPERTY_NEAR_PLANE_DISTANCE
     *
     * to calculate the requested values using
     * F-Stop, Focal Distance and Zoom Factor curves.
     */
    SCENE_CAMERA_PROPERTY_USE_CAMERA_PHYSICAL_PROPERTIES,

    /* settable, bool */
    SCENE_CAMERA_PROPERTY_USE_CUSTOM_VERTICAL_FOV,

    /* settable, curve_container;
     *
     * Getter will throw an assertion failure if use_custom_vertical_fov
     * is false.
     **/
    SCENE_CAMERA_PROPERTY_VERTICAL_FOV_CUSTOM,

    /* not settable, float.
     *
     * Getter will throw an assertion failure if use_custom_vertical_fov
     * is true.
     */
    SCENE_CAMERA_PROPERTY_VERTICAL_FOV_FROM_ZOOM_FACTOR,

    /* settable, uint32_t[2]
     *
     * Viewport size information is used to calculate the returned aspect
     * ratio, if camera's internal AR is set to 0.0.
     */
    SCENE_CAMERA_PROPERTY_VIEWPORT,

    /* not settable, curve_container */
    SCENE_CAMERA_PROPERTY_ZOOM_FACTOR
};

/** TODO */
PUBLIC EMERALD_API scene_camera scene_camera_create(system_hashed_ansi_string name,
                                                    system_hashed_ansi_string object_manager_path);

/** TODO.
 *
 *  NOTE: Some properties may need a recalc, which is why the function takes @param time.
 **/
PUBLIC EMERALD_API void scene_camera_get_property(scene_camera          camera,
                                                  scene_camera_property property,
                                                  system_time           time,
                                                  void*                 out_result);

/** TODO */
PUBLIC scene_camera scene_camera_load(system_file_serializer    serializer,
                                      scene                     owner_scene,
                                      system_hashed_ansi_string object_manager_path);

/** TODO
 *
 *  NOTE: This function is for internal use only.
 **/
PUBLIC bool scene_camera_save(system_file_serializer serializer,
                              const scene_camera     camera,
                              scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_camera_set_property(scene_camera          camera,
                                                  scene_camera_property property,
                                                  const void*           data);

#endif /* SCENE_CAMERA_H */
