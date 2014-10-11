/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OGL_FLYBY_H
#define OGL_FLYBY_H

#include "ogl/ogl_types.h"

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_activate(__in           __notnull ogl_context context,
                                           __in_ecount(3) __notnull const float*      camera_xyz);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_deactivate(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API const float* ogl_flyby_get_camera_location(__in __notnull ogl_context);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_get_up_forward_right_vectors(__in            __notnull ogl_context context,
                                                               __out_ecount(3) __notnull float*      up,
                                                               __out_ecount(3) __notnull float*      forward,
                                                               __out_ecount(3) __notnull float*      right);
/** TODO */
PUBLIC EMERALD_API bool ogl_flyby_get_view_matrix(__in __notnull ogl_context      context,
                                                  __in __notnull system_matrix4x4 result);

/** TODO */
PUBLIC EMERALD_API bool ogl_flyby_is_active(__in __notnull ogl_context context);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_lock();

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_pitch_yaw(__in __notnull ogl_context context, __in float pitch, __in float yaw);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_position(__in __notnull ogl_context context, __in float x, __in float y, __in float z);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_movement_delta(__in __notnull ogl_context context, __in float);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_set_rotation_delta(__in __notnull ogl_context context, __in float);

/** TODO */
PUBLIC EMERALD_API void ogl_flyby_unlock();

/** TODO. Must be called once per frame after flyby has been activated */
PUBLIC EMERALD_API void ogl_flyby_update(__in __notnull ogl_context context);

#endif /* OGL_FLYBY_H */
