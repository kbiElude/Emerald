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
    OGL_FLYBY_PROPERTY_CAMERA_LOCATION, /*     settable, float[3]         */
    OGL_FLYBY_PROPERTY_IS_ACTIVE,       /* not settable, bool             */
    OGL_FLYBY_PROPERTY_MOVEMENT_DELTA,  /*     settable, float            */
    OGL_FLYBY_PROPERTY_PITCH,           /*     settable, float            */
    OGL_FLYBY_PROPERTY_ROTATION_DELTA,  /*     settable, float            */
    OGL_FLYBY_PROPERTY_YAW,             /*     settable, float            */

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

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_activate(__in           __notnull ogl_context context,
                                           __in_ecount(3) __notnull const float*      camera_xyz);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_deactivate(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_get_property(__in  __notnull ogl_context        context,
                                               __in            ogl_flyby_property property,
                                               __out __notnull void*              out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_lock();

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_property(__in __notnull ogl_context        context,
                                               __in           ogl_flyby_property property,
                                               __in __notnull const void*        data);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_unlock();

/** TODO. Must be called once per frame after flyby has been activated */
PUBLIC EMERALD_API void ogl_flyby_update(__in __notnull ogl_context context);

#endif /* OGL_FLYBY_H */
