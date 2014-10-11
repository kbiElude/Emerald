/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_ROT_H
#define SH_ROT_H

#include "shared.h"
#include "sh/sh_config.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(sh_rot, sh_rot)


/** TODO */
PUBLIC EMERALD_API sh_rot sh_rot_create(__in __notnull ogl_context,
                                        __in           uint32_t /* n bands */,
                                        __in __notnull system_hashed_ansi_string);

#ifdef INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION
    /** TODO */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_krivanek_y(__in __notnull sh_rot,
                                                                            __in            float  angle_radians,
                                                                            __in __notnull  GLuint sh_data_tbo,
                                                                            __in __notnull  GLuint out_rotated_sh_data_bo_id);
#endif /* INCLUDE_KRIVANEK_FAST_SH_Y_ROTATION */

/** TODO. Supports up to 3 bands! */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_xyz(__in __notnull sh_rot,
                                                                 __in            float       x_angle_radians,
                                                                 __in            float       y_angle_radians,
                                                                 __in            float       z_angle_radians,
                                                                 __in __notnull  ogl_texture sh_data_tbo,
                                                                 __in __notnull  GLuint      out_rotated_sh_data_bo_id,
                                                                 __in            GLuint      out_rotated_sh_data_bo_offset,
                                                                 __in            GLuint      out_rotated_sh_data_bo_size);

/** TODO */
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_rot_rotate_z(__in __notnull sh_rot,
                                                              __in            float       angle_radians,
                                                              __in __notnull  ogl_texture sh_data_tbo,
                                                              __in __notnull  GLuint      out_rotated_sh_data_bo_id);

#endif /* SH_ROT_H */