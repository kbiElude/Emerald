/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_FLYBY_H
#define OGL_FLYBY_H

#include "ogl/ogl_types.h"


typedef enum
{
    OGL_FLYBY_PROPERTY_CAMERA_LOCATION, /* settable, float[3]         */
    OGL_FLYBY_PROPERTY_IS_ACTIVE,       /* settable, bool             */
    OGL_FLYBY_PROPERTY_MOVEMENT_DELTA,  /* settable, float            */
    OGL_FLYBY_PROPERTY_PITCH,           /* settable, float            */
    OGL_FLYBY_PROPERTY_ROTATION_DELTA,  /* settable, float            */
    OGL_FLYBY_PROPERTY_YAW,             /* settable, float            */

    /* not settable.
     *
     * This property returns a fake scene_camera instance which wraps
     * the owning ogl_flyby. The returned instance is not a part of
     * any scene instance.
     */
    OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA,

    /* settable, float */
    OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_FAR,
    OGL_FLYBY_PROPERTY_FAKE_SCENE_CAMERA_Z_NEAR,

    /* not settable.
     *
     * Will set @param out_result to the contents of the view matrix
     * assigned to the flyby at the time of the call.
     */
    OGL_FLYBY_PROPERTY_VIEW_MATRIX,
} ogl_flyby_property;

REFCOUNT_INSERT_DECLARATIONS(ogl_flyby,
                             ogl_flyby);


/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC ogl_flyby ogl_flyby_create(ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_deactivate(ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_get_property(ogl_flyby          flyby,
                                               ogl_flyby_property property,
                                               void*              out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_lock(ogl_flyby flyby);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_property(ogl_flyby          flyby,
                                               ogl_flyby_property property,
                                               const void*        data);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_unlock(ogl_flyby flyby);

/** TODO. Must be called once per frame after flyby has been activated */
PUBLIC EMERALD_API void ogl_flyby_update(ogl_flyby flyby);

#endif /* OGL_FLYBY_H */
