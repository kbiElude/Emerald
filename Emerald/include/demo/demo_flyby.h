/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef DEMO_FLYBY_H
#define DEMO_FLYBY_H

#include "demo/demo_types.h"


typedef enum
{
    /* settable, float[3] */
    DEMO_FLYBY_PROPERTY_CAMERA_LOCATION,

    /* settable, bool */
    DEMO_FLYBY_PROPERTY_IS_ACTIVE,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_MOVEMENT_DELTA,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_PITCH,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_ROTATION_DELTA,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_YAW,

    /* not settable.
     *
     * This property returns a fake scene_camera instance which wraps
     * the owning demo_flyby. The returned instance is not a part of
     * any scene instance.
     */
    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_FAR,

    /* settable, float */
    DEMO_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_NEAR,

    /* not settable.
     *
     * Will set @param out_result to the contents of the view matrix
     * assigned to the flyby at the time of the call. Therefore the
     * pointer passed as an argument to the demo_flyby_get_property()
     * must NOT be NULL.
     */
    DEMO_FLYBY_PROPERTY_VIEW_MATRIX,
} demo_flyby_property;

REFCOUNT_INSERT_DECLARATIONS(demo_flyby,
                             demo_flyby);


/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC demo_flyby demo_flyby_create(ral_context context);

/** TODO */
PUBLIC EMERALD_API void demo_flyby_deactivate(ral_context context);

/** TODO */
PUBLIC EMERALD_API void demo_flyby_get_property(demo_flyby          flyby,
                                                demo_flyby_property property,
                                                void*              out_result);

/** TODO */
PUBLIC EMERALD_API void demo_flyby_lock(demo_flyby flyby);

/** TODO */
PUBLIC EMERALD_API void demo_flyby_set_property(demo_flyby          flyby,
                                                demo_flyby_property property,
                                                const void*        data);

/** TODO */
PUBLIC EMERALD_API void demo_flyby_unlock(demo_flyby flyby);

/** TODO. Must be called once per frame after flyby has been activated */
PUBLIC EMERALD_API void demo_flyby_update(demo_flyby flyby);

#endif /* DEMO_FLYBY_H */
