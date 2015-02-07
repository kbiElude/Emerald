/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef OGL_SHADOW_MAPPING_H
#define OGL_SHADOW_MAPPING_H

#include "ogl/ogl_types.h"
#include "ogl/ogl_shader_constructor.h"
#include "scene/scene_types.h"

DECLARE_HANDLE(ogl_shadow_mapping);


/** TODO */
PUBLIC void ogl_shadow_mapping_adjust_fragment_uber_code(__in  __notnull ogl_shader_constructor      shader_constructor_fs,
                                                         __in            uint32_t                    n_light,
                                                         __in            scene_light_shadow_map_bias sm_bias,
                                                         __in            _uniform_block_id           ub_fs,
                                                         __out __notnull system_hashed_ansi_string*  out_visibility_var_name);

/** TODO */
PUBLIC void ogl_shadow_mapping_adjust_vertex_uber_code(__in __notnull ogl_shader_constructor    shader_constructor_vs,
                                                       __in           uint32_t                  n_light,
                                                       __in           _uniform_block_id         ub_vs,
                                                       __in __notnull system_hashed_ansi_string world_vertex_vec4_variable_name);

/** TODO.
 *
 *  NOTE: MUST be called from within an active GL context.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_shadow_mapping_get_matrices_for_directional_light(__in            __notnull scene_light          light,
                                                                  __in_ecount(3)  __notnull scene_camera         current_camera,
                                                                  __in                      system_timeline_time time,
                                                                  __in_ecount(3)  __notnull const float*         aabb_min_world,
                                                                  __in_ecount(3)  __notnull const float*         aabb_max_world,
                                                                  __out           __notnull system_matrix4x4*    out_view_matrix,
                                                                  __out           __notnull system_matrix4x4*    out_projection_matrix);

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