/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef SCENE_RENDERER_SM_H
#define SCENE_RENDERER_SM_H

#include "glsl/glsl_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"
#include "shaders/shaders_fragment_uber.h"

typedef enum
{
    /* Fragment shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
    SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_FS,

    /* Vertex shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
    SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_VS,

    /* Fragment shader to be used for generating color shadow map data for
     * Variance Shadow Mapping.
     */
    SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_FS,

    /* Vertex shader to be used for generating color shadow map data for
     * Variance Shadow Mapping.
     */
     SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_VS,

    /* Fragment shader to be used for generating color shadow map data for
     * Variance Shadow Mapping (DPSM-specific) */
    SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_FS,

    /* Vertex shader to be used for generating color shadow map data for
     * Variance Shadow Mapping (DPSM-specific) */
    SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_AND_SQUARED_DEPTH_CLIP_DUAL_PARABOLOID_VS,

    /* Fragment shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
     SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_FS,

    /* Vertex shader to be used for generating depth shadow map data for
     * Plain Shadow Mapping.
     */
     SCENE_RENDERER_SM_SPECIAL_MATERIAL_BODY_TYPE_DEPTH_CLIP_DUAL_PARABOLOID_VS

} scene_renderer_sm_special_material_body_type;

typedef enum
{
    /* not settable, unsigned int
     *
     * Can be called for NULL scene_renderer_sm instances
     **/
    SCENE_RENDERER_SM_PROPERTY_N_MAX_BLUR_TAPS,

    /* not settable, unsigned int
     *
     * Can be called for NULL scene_renderer_sm instances
     **/
    SCENE_RENDERER_SM_PROPERTY_N_MIN_BLUR_TAPS,
} scene_renderer_sm_property;

typedef enum
{
    /* directional / spot lights */
    SCENE_RENDERER_SM_TARGET_FACE_2D,

    /* DPSM point lights */
    SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_REAR,
    SCENE_RENDERER_SM_TARGET_FACE_2D_PARABOLOID_FRONT,

    /* Cubical SM point lights
     *
     * CM target face order must not be changed */
    SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_X,
    SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_X,
    SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Y,
    SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Y,
    SCENE_RENDERER_SM_TARGET_FACE_POSITIVE_Z,
    SCENE_RENDERER_SM_TARGET_FACE_NEGATIVE_Z,

    SCENE_RENDERER_SM_TARGET_FACE_UNKNOWN
} scene_renderer_sm_target_face;

/** TODO */
PUBLIC void scene_renderer_sm_adjust_fragment_uber_code(glsl_shader_constructor                  shader_constructor_fs,
                                                        uint32_t                                 n_light,
                                                        scene_light                              light_instance,
                                                        glsl_shader_constructor_uniform_block_id ub_fs,
                                                        system_hashed_ansi_string                light_world_pos_var_name,
                                                        system_hashed_ansi_string                light_vector_norm_var_name,
                                                        system_hashed_ansi_string                light_vector_non_norm_var_name,
                                                        system_hashed_ansi_string*               out_visibility_var_name);

/** TODO */
PUBLIC void scene_renderer_sm_adjust_vertex_uber_code(glsl_shader_constructor                  shader_constructor_vs,
                                                      uint32_t                                 n_light,
                                                      shaders_fragment_uber_light_type         light_type,
                                                      glsl_shader_constructor_uniform_block_id ub_vs,
                                                      system_hashed_ansi_string                world_vertex_vec4_variable_name);

/** TODO.
 *
 *  NOTE: MUST be called from within an active GL context.
 */
PUBLIC scene_renderer_sm scene_renderer_sm_create(ral_context context);

/** TODO */
PUBLIC void scene_renderer_sm_get_matrices_for_light(scene_renderer_sm             shadow_mapping,
                                                     scene_light                   light,
                                                     scene_renderer_sm_target_face light_target_face,
                                                     scene_camera                  current_camera,
                                                     system_time                   time,
                                                     const float*                  aabb_min_world,
                                                     const float*                  aabb_max_world,
                                                     system_matrix4x4*             out_view_matrix,
                                                     system_matrix4x4*             out_projection_matrix);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_sm_get_property(const scene_renderer_sm    shadow_mapping,
                                                       scene_renderer_sm_property property,
                                                       void*                      out_result_ptr);

/** TODO */
PUBLIC bool scene_renderer_sm_get_sampler_for_light(const scene_renderer_sm shadow_mapping,
                                                    const scene_light       light,
                                                    bool                    need_color_tv_sampler,
                                                    ral_sampler*            out_sampler_ptr);

/** TODO.
 *
 *  NOTE: Internal usage only.
 **/
PUBLIC system_hashed_ansi_string scene_renderer_sm_get_special_material_shader_body(scene_renderer_sm_special_material_body_type body_type);

/** TODO */
PUBLIC void scene_renderer_sm_process_mesh_for_shadow_map_rendering(scene_mesh scene_mesh_instance,
                                                                    void*      renderer);

/** TODO. **/
PUBLIC void scene_renderer_sm_release(scene_renderer_sm handler);

/** TODO
 *
 *  NOTE: Internal use only. This function should only be used by scene_renderer and scene_renderer_sm.
 **/
PUBLIC ral_present_task scene_renderer_sm_render_shadow_map_meshes(scene_renderer_sm                shadow_mapping,
                                                                   scene_renderer                   renderer,
                                                                   scene                            scene,
                                                                   system_time                      frame_time,
                                                                   const ral_gfx_state_create_info* ref_gfx_state_create_info_ptr);

/** TODO.
 *
 *  The result present task exposes one or more texture views, each holding a result shadow map.
 *  Exact number of outputs and texture view properties depend on:
 *
 *  1) Number of shadow-casting lights.
 *  2) Type of shadow mapping algorithm, selected for each light.
 *
 */
PUBLIC ral_present_task scene_renderer_sm_render_shadow_maps(scene_renderer_sm shadow_mapping,
                                                             scene_renderer    renderer,
                                                             scene             current_scene,
                                                             scene_camera      target_camera,
                                                             system_time       frame_time);

#endif /* SCENE_RENDERER_SM_H */