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
#include "shaders/shaders_fragment_uber.h"

DECLARE_HANDLE(ogl_shadow_mapping);


typedef enum
{
    /* Fragment shader to be used for generating color shadow map data for
     * Variance Shadow Mapping.
     */
    OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_FS,

    /* Vertex shader to be used for generating color shadow map data for
     * Variance Shadow Mapping.
     */
     OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_VS,

    /* Fragment shader to be used for generating color shadow map data for
     * Variance Shadow Mapping (DPSM-specific) */
    OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_FS,

    /* Vertex shader to be used for generating color shadow map data for
     * Variance Shadow Mapping (DPSM-specific) */
    OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_VS,

    /* Fragment shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
     OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_FS,

    /* Vertex shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
     OGL_SHADOW_MAPPING_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_VS

} ogl_shadow_mapping_special_material_body_type;

typedef enum
{
    /* not settable, unsigned int */
    OGL_SHADOW_MAPPING_PROPERTY_N_MAX_BLUR_TAPS,

    /* not settable, unsigned int */
    OGL_SHADOW_MAPPING_PROPERTY_N_MIN_BLUR_TAPS,
} ogl_shadow_mapping_property;

typedef enum
{
    /* directional / spot lights */
    OGL_SHADOW_MAPPING_TARGET_FACE_2D,

    /* DPSM point lights */
    OGL_SHADOW_MAPPING_TARGET_FACE_2D_PARABOLOID_REAR,
    OGL_SHADOW_MAPPING_TARGET_FACE_2D_PARABOLOID_FRONT,

    /* Cubical SM point lights
     *
     * CM target face order must not be changed */
    OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_X,
    OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_X,
    OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Y,
    OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Y,
    OGL_SHADOW_MAPPING_TARGET_FACE_POSITIVE_Z,
    OGL_SHADOW_MAPPING_TARGET_FACE_NEGATIVE_Z,

    OGL_SHADOW_MAPPING_TARGET_FACE_UNKNOWN
} ogl_shadow_mapping_target_face;

/** TODO */
PUBLIC void ogl_shadow_mapping_adjust_fragment_uber_code(__in  __notnull ogl_shader_constructor     shader_constructor_fs,
                                                         __in            uint32_t                   n_light,
                                                         __in __notnull  scene_light                light_instance,
                                                         __in            _uniform_block_id          ub_fs,
                                                         __in            system_hashed_ansi_string  light_world_pos_var_name,
                                                         __in            system_hashed_ansi_string  light_vector_norm_var_name,
                                                         __in            system_hashed_ansi_string  light_vector_non_norm_var_name,
                                                         __out __notnull system_hashed_ansi_string* out_visibility_var_name);

/** TODO */
PUBLIC void ogl_shadow_mapping_adjust_vertex_uber_code(__in __notnull ogl_shader_constructor           shader_constructor_vs,
                                                       __in           uint32_t                         n_light,
                                                       __in           shaders_fragment_uber_light_type light_type,
                                                       __in           _uniform_block_id                ub_vs,
                                                       __in __notnull system_hashed_ansi_string        world_vertex_vec4_variable_name);

/** TODO.
 *
 *  NOTE: MUST be called from within an active GL context.
 */
PUBLIC RENDERING_CONTEXT_CALL ogl_shadow_mapping ogl_shadow_mapping_create(__in __notnull ogl_context context);

/** TODO */
PUBLIC void ogl_shadow_mapping_get_matrices_for_light(__in            __notnull ogl_shadow_mapping             shadow_mapping,
                                                      __in            __notnull scene_light                    light,
                                                      __in                      ogl_shadow_mapping_target_face light_target_face,
                                                      __in_ecount(3)  __notnull scene_camera                   current_camera,
                                                      __in                      system_timeline_time           time,
                                                      __in_ecount(3)  __notnull const float*                   aabb_min_world,
                                                      __in_ecount(3)  __notnull const float*                   aabb_max_world,
                                                      __out           __notnull system_matrix4x4*              out_view_matrix,
                                                      __out           __notnull system_matrix4x4*              out_projection_matrix);

/** TODO */
PUBLIC EMERALD_API void ogl_shadow_mapping_get_property(__in  __notnull ogl_shadow_mapping          shadow_mapping,
                                                        __in            ogl_shadow_mapping_property property,
                                                        __out __notnull void*                       out_result);

/** TODO.
 *
 *  NOTE: Internal usage only.
 **/
PUBLIC system_hashed_ansi_string ogl_shadow_mapping_get_special_material_shader_body(__in ogl_shadow_mapping_special_material_body_type body_type);

/** TODO */
PUBLIC void ogl_shadow_mapping_process_mesh_for_shadow_map_rendering(     __notnull scene_mesh scene_mesh_instance,
                                                                     __in __notnull void*      renderer);

/** TODO. **/
PUBLIC void ogl_shadow_mapping_release(__in __notnull ogl_shadow_mapping handler);

/** TODO */
PUBLIC void ogl_shadow_mapping_render_shadow_map_meshes(__in __notnull ogl_shadow_mapping   shadow_mapping,
                                                        __in __notnull ogl_scene_renderer   renderer,
                                                        __in __notnull scene                scene,
                                                        __in           system_timeline_time frame_time);

/** TODO */
PUBLIC void ogl_shadow_mapping_render_shadow_maps(__in __notnull ogl_shadow_mapping   shadow_mapping,
                                                  __in __notnull ogl_scene_renderer   renderer,
                                                  __in __notnull scene                current_scene,
                                                  __in __notnull scene_camera         target_camera,
                                                  __in           system_timeline_time frame_time);

/** TODO.
 *
 *  NOTE: This function changes the draw framebuffer binding!
 *
 **/
PUBLIC RENDERING_CONTEXT_CALL void ogl_shadow_mapping_toggle(__in __notnull ogl_shadow_mapping             handler,
                                                             __in __notnull scene_light                    light,
                                                             __in           bool                           should_enable,
                                                             __in           ogl_shadow_mapping_target_face target_face = OGL_SHADOW_MAPPING_TARGET_FACE_2D);

#endif /* OGL_SHADOW_MAPPING_H */