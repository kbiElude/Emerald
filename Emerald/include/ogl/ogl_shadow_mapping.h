/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef OGL_SHADOW_MAPPING_H
#define OGL_SHADOW_MAPPING_H

#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_shadow_mapping);


/** TODO.
 *
 *  NOTE: MUST be called from within an active GL context.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_shadow_mapping_get_matrices_for_directional_light(__in            __notnull scene_light          light,
                                                                  __in            __notnull scene_camera         sm_user_camera,
                                                                  __in                      system_timeline_time frame_time,
                                                                  __out           __notnull system_matrix4x4*    out_view_matrix,
                                                                  __out           __notnull system_matrix4x4*    out_projection_matrix,
                                                                  __out_ecount(3) __notnull float*               out_camera_position);

/** TODO. **/
PUBLIC void ogl_shadow_mapping_release(__in __notnull ogl_shadow_mapping handler);

/** TODO.
 *
 *  NOTE: This function changes the draw framebuffer binding!
 *
 **/
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping handler,
                                                             __in __notnull scene_light        light,
                                                             __in           bool               should_enable);

#endif /* OGL_SHADOW_MAPPING_H */