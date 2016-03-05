/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef SCENE_RENDERER_UBER_H
#define SCENE_RENDERER_UBER_H

#include "mesh/mesh_types.h"
#include "ogl/ogl_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_renderer_uber,
                             scene_renderer_uber)

typedef enum
{
    /* vec3 */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,

    /* float */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF,

    /* float */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_FLIP_Z,

    /* system_hashed_ansi_string */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_NAME,

    /* float */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_NEAR_PLANE,

    SCENE_RENDERER_UBER_GENERAL_PROPERTY_N_ITEMS,
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_VP,

    /* settable, float.
     *
     * This property is global, as opposed to _min_variance. The reason is that
     * max variance is used during SM generation pass. The pass uses an
     * scene_renderer_uber instance created from an ral_program delivered by
     * scene_renderer_materials (SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED).
     */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_VSM_MAX_VARIANCE,

    /* Always last */
    SCENE_RENDERER_UBER_GENERAL_PROPERTY_UNKNOWN
} scene_renderer_uber_general_property;

typedef enum
{
    SCENE_RENDERER_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE,
    SCENE_RENDERER_UBER_ITEM_LIGHT,

    /* Always last */
    SCENE_RENDERER_UBER_ITEM_UNKNOWN
} scene_renderer_uber_item_type;

typedef enum
{
    SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL,
    SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD,

    /* Always last */
    SCENE_RENDERER_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN
} scene_renderer_uber_input_fragment_attribute;

typedef enum
{
    /* NOT gettable, settable, float[3]. Used by ambient light */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR,


    /* NOT gettable, settable, float[3]. Used by point lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,

    /* NOT gettable, settable, float, radians. Used by spot lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE,

    /* NOT gettable, settable, float[3]. Used by directional & point lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE,

    /* NOT gettable, settable, float[3]. Used by directional lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,

    /* NOT gettable, settable, float, radians. Used by spot lights. */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE,

    /* NOT gettable, settable, float, Used by point lights (>= 1 algo) */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF,

    /* NOT gettable, settable, float[3]. Used by point lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,

    /* NOT gettable, settable, float. Used by point lights (>= 1 algo) */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE,

    /* NOT gettable, settable, float[16], row-major. Used by point lights (>= 1 algo) */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX,

    /* NOT gettable, settable, float. Used by point & spot lights */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE,

    /* NOT gettable, settable, float[16], row-major. Used by point lights (>= 1 algo) */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,


    /* NOT gettable, settable, float[16], row-major */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP,

    /* NOT gettable, settable, _ogl_uber_light_sh_data */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA,


    /* not settable, scene_light_falloff */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_FALLOFF,

    /* not settable, scene_light_shadow_map_algorithm */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM,

    /* settable, ral_texture */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_COLOR,

    /* settable, ral_texture */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_RAL_DEPTH,

    /* not settable, scene_light_shadow_map_bias */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS,

    /* not settable, scene_light_shadow_map_pointlight_algorithm */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM,

    /* NOT gettable, settable, float */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF,

    /* NOT gettable, settable, float.
     *
     * This property is per-light, as opposed to _max_variance. The reason is that
     * min variance can theoretically be different for each light.
     **/
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE,

    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP,
    SCENE_RENDERER_UBER_ITEM_PROPERTY_LIGHT_TYPE,
    SCENE_RENDERER_UBER_ITEM_PROPERTY_TYPE,


    /* Always last */
    SCENE_RENDERER_UBER_ITEM_PROPERTY_UNKNOWN
} scene_renderer_uber_item_property;

typedef struct
{
    GLuint bo_id;
    GLuint bo_offset;
} scene_renderer_uber_light_sh_data;


typedef unsigned int scene_renderer_uber_item_id;

/** TODO */
PUBLIC EMERALD_API scene_renderer_uber_item_id scene_renderer_uber_add_input_fragment_attribute_item(scene_renderer_uber                          uber,
                                                                                                     scene_renderer_uber_input_fragment_attribute input_attribute);

/** TODO */
PUBLIC EMERALD_API scene_renderer_uber_item_id scene_renderer_uber_add_light_item(scene_renderer_uber              uber,
                                                                                  scene_light                      light_instance,
                                                                                  shaders_fragment_uber_light_type light_type,
                                                                                  bool                             is_shadow_caster,
                                                                                  unsigned int                     n_light_properties,
                                                                                  void*                            light_property_values);

/** TODO */
PUBLIC EMERALD_API scene_renderer_uber scene_renderer_uber_create(ral_context                context,
                                                                  system_hashed_ansi_string  name);

/** TODO */
PUBLIC EMERALD_API scene_renderer_uber scene_renderer_uber_create_from_ral_program(ral_context               context,
                                                                                   system_hashed_ansi_string name,
                                                                                   ral_program               program);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_uber_get_shader_general_property(const scene_renderer_uber            uber,
                                                                        scene_renderer_uber_general_property property,
                                                                        void*                                out_result);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_uber_get_shader_item_property(const scene_renderer_uber         uber,
                                                                     scene_renderer_uber_item_id       item_id,
                                                                     scene_renderer_uber_item_property property,
                                                                     void*                             result);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_uber_link(scene_renderer_uber uber);

/** TODO */
PUBLIC void scene_renderer_uber_render_mesh(mesh                mesh_gpu,
                                            system_matrix4x4    model,
                                            system_matrix4x4    normal_matrix,
                                            scene_renderer_uber uber,
                                            mesh_material       material,
                                            system_time         time);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void scene_renderer_uber_rendering_start(scene_renderer_uber);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_uber_set_shader_general_property(scene_renderer_uber                  uber,
                                                                        scene_renderer_uber_general_property property,
                                                                        const void*                          data);

/** TODO */
PUBLIC EMERALD_API void scene_renderer_uber_set_shader_item_property(scene_renderer_uber               uber,
                                                                     unsigned int                      item_index,
                                                                     scene_renderer_uber_item_property property,
                                                                     const void*                       data);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void scene_renderer_uber_rendering_stop(scene_renderer_uber uber);

#endif /* SCENE_RENDERER_UBER_H */
