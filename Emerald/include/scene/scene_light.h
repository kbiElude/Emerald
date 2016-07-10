/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef SCENE_LIGHT_H
#define SCENE_LIGHT_H

#include "scene/scene_types.h"


REFCOUNT_INSERT_DECLARATIONS(scene_light,
                             scene_light)


typedef enum
{
    /* arg: source scene_light instance */
    SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_ALGORITHM_CHANGED,

    /* arg: source scene_light instance */
    SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_BIAS_CHANGED,

    /* arg: source scene_light instance */
    SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_FILTERING_CHANGED,

    /* arg: source scene_light instance */
    SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_POINTLIGHT_ALGORITHM_CHANGED,

    /* Always last */
    SCENE_LIGHT_CALLBACK_ID_LAST = SCENE_LIGHT_CALLBACK_ID_SHADOW_MAP_POINTLIGHT_ALGORITHM_CHANGED
} scene_light_callback;

typedef enum
{
                                            /* GENERAL PROPERTIES: */

    /* Not settable, system_callback_manager */
    SCENE_LIGHT_PROPERTY_CALLBACK_MANAGER,

    /* Settable, curve_container[3] */
    SCENE_LIGHT_PROPERTY_COLOR,

    /* Settable, curve_container */
    SCENE_LIGHT_PROPERTY_COLOR_INTENSITY,

    /* Settable, curve_container (radians).
     *
     * (spot lights only)
     */
    SCENE_LIGHT_PROPERTY_CONE_ANGLE_HALF,

    /* Settable, float[3].
     *
     * Set in run-time by Emerald, deduced from the transformation matrix of the node,
     * to which the light is bound.
     */
    SCENE_LIGHT_PROPERTY_DIRECTION,

    /* Settable, curve_container (radians).
     *
     * (spot lights only)
     */
    SCENE_LIGHT_PROPERTY_EDGE_ANGLE,

    /* Settable, scene_graph_node.
     *
     * Set in run-time.
     */
    SCENE_LIGHT_PROPERTY_GRAPH_OWNER_NODE,

    /* Not settable, system_hashed_ansi_string */
    SCENE_LIGHT_PROPERTY_NAME,

    /* Settable, curve_container.
     *
     * (point lights only)
     */
    SCENE_LIGHT_PROPERTY_RANGE,

    /* Not settable, scene_light_type */
    SCENE_LIGHT_PROPERTY_TYPE,


                                                /* SHADOW MAPPING: PROPERTIES: */

    /* Settable, float[3]. Set in run-time.
     *
     * NOTE: This property is set during run-time by scene_renderer and is NOT
     *       serialized. It acts merely as a communication mean between the scene
     *       graph traversation layer and actual renderer.
     *
     * This property is only accepted for point & spot lights. Any attempt to access it
     * for a directional light will throw assertion failure. Getters will return
     * undefined content, if executed for incorrect light types.
     */
    SCENE_LIGHT_PROPERTY_POSITION,

    /* Settable, scene_light_shadow_map_bias. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_BIAS,

    /* Settable, scene_light_shadow_map_algorithm.
     *
     * Configures SM generation process. By definition, it affects the uber shader
     * implementation and shadow map texture internalformat.
     *
     * This is the property you want to change, if you want to enable more complex algorithms
     * like Variance Shadow Mapping.
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_ALGORITHM,

    /* Settable, bool */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_CULL_FRONT_FACES,

    /* Settable, scene_light_shadow_map_filtering. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_FILTERING,

    /* Settable, ral_format.
     *
     * Stores internalformat of draw buffers used during SM generation pass.
     * Such texture is currently only generated for VSM SM algorithm.
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_FORMAT_COLOR,

    /* Settable, ral_format.
     *
     * Stores internalformat of the depth texture used during SM generation pass.
     */
     SCENE_LIGHT_PROPERTY_SHADOW_MAP_FORMAT_DEPTH,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_PROJECTION,

    /* Settable, uint[2]. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_SIZE,

    /* Settable, ral_texture.
     *
     * Set in run-time. Note that SM textures are taken from the texture pool
     * and are returned as soon as the SM-enabled rendering process finishes.
     *
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_COLOR_RAL,
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_TEXTURE_DEPTH_RAL,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VIEW,

    /* Not settable, system_matrix4x4.
     *
     * Update by doing system_matrix4x4_*() calls on the exposed matrix. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VP,

    /* Settable, float.
      *
      * Adjusts the number of times SM color texture is going to be blurred.
      *
      * Note: if frac(n_times) != 0, the SM color texture will be a lerp between floor(n_times)
      *       and ceil(n_times). This means two separate blur passes will be executed which will
      *       affect the performance.
      */
     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_PASSES,

    /* Settable, unsigned int.
     *
     * Adjusts the number of taps used for blurring.
     *
     */
     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_N_TAPS,

    /* Settable, postprocessing_blur_gaussian_resolution.
     *
     * Adjusts the resolution, at which the SM color texture is going to be blurred.
     */
     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_BLUR_RESOLUTION,

    /* Settable, float.
     *
     * This parameter adjusts the cut-off range for Variance Shadow Mapping.
     * Helps fight the light bleeding artifact at the cost of dimming the penumbras.
     *
     * Should be tweaked per-scene.
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_CUTOFF,

    /* Settable, float.
     *
     * This parameter defines the maximum allowed variance value that should be
     * stored in the color shadow map. This setting should be used to reduce the
     * light bleeding effect.
     */
     SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_MAX_VARIANCE,

    /* Settable, float.
     *
     * This parameter defines the minimum allowed variance value that should be
     * stored in the color shadow map. In conjuction with the color shadow map
     * internalformat, this setting can be used to reduce precision requirements
     * at the cost of the SM quality.
     *
     */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_VSM_MIN_VARIANCE,

    /* Settable, bool */
    SCENE_LIGHT_PROPERTY_USES_SHADOW_MAP,


                                        /* POINT LIGHT SHADOW MAPPING: PROPERTIES: */

    /* Settable, scene_light_shadow_map_pointlight_algorithm. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_ALGORITHM,

    /* Settable, float. Set in run-time by scene_renderer_sm during
     * the shadow map baking phase (for dual paraboloid shadow mapping only).
     * Info necessary to 
     * . */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_FAR_PLANE,

    /* Settable, float.
     *
     * Tweakable near-plane distance for point light shadow mapping. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_POINTLIGHT_NEAR_PLANE,


                                        /* SPOT LIGHT SHADOW MAPPING: PROPERTIES: */

    /* Settable, float.
     *
     * Tweakable near-plane distance for spot light shadow mapping. */
    SCENE_LIGHT_PROPERTY_SHADOW_MAP_SPOTLIGHT_NEAR_PLANE,


                                         /* SHADING: GENERAL PROPERTIES: */

    /* Settable, curve_container.
     *
     * NOTE: Only used if SCENE_LIGHT_PROPERTY_FALLOFF is set at SCENE_LIGHT_FALLOFF_CUSTOM, in which case
     *       attenuation is called as in:
     *
     *       final_att = 1 / (constant_att + linear_att * dist + quadratic_att * dist^2).
     **/
    SCENE_LIGHT_PROPERTY_CONSTANT_ATTENUATION,
    SCENE_LIGHT_PROPERTY_LINEAR_ATTENUATION,
    SCENE_LIGHT_PROPERTY_QUADRATIC_ATTENUATION,

    /* Settable, scene_light_falloff */
    SCENE_LIGHT_PROPERTY_FALLOFF,


    /* Always last */
    SCENE_LIGHT_PROPERTY_COUNT
} scene_light_property;

typedef enum
{
    /* Depth texture-based plain shadow mapping */
    SCENE_LIGHT_SHADOW_MAP_ALGORITHM_PLAIN,

    /* Variance shadow maps */
    SCENE_LIGHT_SHADOW_MAP_ALGORITHM_VSM,

    SCENE_LIGHT_SHADOW_MAP_ALGORITHM_UNKNOWN
} scene_light_shadow_map_algorithm;

typedef enum
{
    /* 6-pass cube-map-based omnidirectional SM implementation */
    SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_CUBICAL,

    /* 2-pass 2D texture-based omnidirectional SM implementation.
     *
     * This version DOES NOT use geometry shader + layered rendering approach.
     */
     SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_DUAL_PARABOLOID,

     SCENE_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM_UNKNOWN
} scene_light_shadow_map_pointlight_algorithm;

typedef enum
{
    /* Single splat should be enough for everybody! */
    SCENE_LIGHT_SHADOW_MAP_FILTERING_PLAIN,
    /* Well, maybe not? */
    SCENE_LIGHT_SHADOW_MAP_FILTERING_PCF,

    SCENE_LIGHT_SHADOW_MAP_FILTERING_UNKNOWN
} scene_light_shadow_map_filtering;


/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_ambient(system_hashed_ansi_string name,
                                                          system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_directional(system_hashed_ansi_string name,
                                                              system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_point(system_hashed_ansi_string name,
                                                        system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API scene_light scene_light_create_spot(system_hashed_ansi_string name,
                                                       system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API void scene_light_get_property(scene_light          light,
                                                 scene_light_property property,
                                                 void*                out_result_ptr);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_falloff_has(scene_light_falloff falloff);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_algorithm_has(scene_light_shadow_map_algorithm algorithm);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_bias_has(scene_light_shadow_map_bias bias);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_filtering_has(scene_light_shadow_map_filtering filtering);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_shadow_map_pointlight_algorithm_has(scene_light_shadow_map_pointlight_algorithm algorithm);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string scene_light_get_scene_light_type_has(scene_light_type light_type);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_light scene_light_load(system_file_serializer    serializer,
                                    scene                     owner_scene,
                                    system_hashed_ansi_string object_manager_path);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 **/
PUBLIC bool scene_light_save(system_file_serializer serializer,
                             const scene_light      light,
                             scene                  owner_scene);

/** TODO */
PUBLIC EMERALD_API void scene_light_set_property(scene_light          light,
                                                 scene_light_property property,
                                                 const void*          data);

#endif /* SCENE_CAMERA_H */
