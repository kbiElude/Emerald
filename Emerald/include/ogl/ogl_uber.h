/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_UBER_H
#define OGL_UBER_H

#include "mesh/mesh_types.h"
#include "ogl/ogl_types.h"
#include "scene/scene_types.h"
#include "shaders/shaders_fragment_uber.h"
#include "shaders/shaders_vertex_uber.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_uber,
                             ogl_uber)

typedef enum
{
    /* vec3 */
    OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,

    /* float */
    OGL_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF,

    /* float */
    OGL_UBER_GENERAL_PROPERTY_FLIP_Z,

    /* system_hashed_ansi_string */
    OGL_UBER_GENERAL_PROPERTY_NAME,

    /* float */
    OGL_UBER_GENERAL_PROPERTY_NEAR_PLANE,

    OGL_UBER_GENERAL_PROPERTY_N_ITEMS,
    OGL_UBER_GENERAL_PROPERTY_VP,

    /* settable, float.
     *
     * This property is global, as opposed to _min_variance. The reason is that
     * max variance is used during SM generation pass. The pass uses an
     * ogl_uber instance created from an ogl_program delivered by ogl_materials
     * (SPECIAL_MATERIAL_DEPTH_CLIP_AND_DEPTH_CLIP_SQUARED).
     */
    OGL_UBER_GENERAL_PROPERTY_VSM_MAX_VARIANCE,

    /* Always last */
    OGL_UBER_GENERAL_PROPERTY_UNKNOWN
} _ogl_uber_general_property;

typedef enum
{
    OGL_UBER_ITEM_INPUT_FRAGMENT_ATTRIBUTE,
    OGL_UBER_ITEM_LIGHT,

    /* Always last */
    OGL_UBER_ITEM_UNKNOWN
} _ogl_uber_item_type;

typedef enum
{
    OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_NORMAL,
    OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD,

    /* Always last */
    OGL_UBER_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN
} _ogl_uber_input_fragment_attribute;

typedef enum
{
    /* NOT gettable, settable, float[3]. Used by ambient light */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR,


    /* NOT gettable, settable, float[3]. Used by point lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,

    /* NOT gettable, settable, float, radians. Used by spot lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE,

    /* NOT gettable, settable, float[3]. Used by directional & point lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE,

    /* NOT gettable, settable, float[3]. Used by directional lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,

    /* NOT gettable, settable, float, radians. Used by spot lights. */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE,

    /* NOT gettable, settable, float, Used by point lights (>= 1 algo) */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF,

    /* NOT gettable, settable, float[3]. Used by point lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,

    /* NOT gettable, settable, float. Used by point lights (>= 1 algo) */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE,

    /* NOT gettable, settable, float[16], row-major. Used by point lights (>= 1 algo) */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX,

    /* NOT gettable, settable, float. Used by point & spot lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE,

    /* NOT gettable, settable, float[16], row-major. Used by point lights (>= 1 algo) */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,


    /* NOT gettable, settable, float[16], row-major */
    OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP,

    /* NOT gettable, settable, _ogl_uber_light_sh_data */
    OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA,


    /* not settable, scene_light_falloff */
    OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF,

    /* not settable, scene_light_shadow_map_algorithm */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_ALGORITHM,

    /* settable, ogl_texture */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_COLOR,

    /* settable, ogl_texture */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_TEXTURE_DEPTH,

    /* not settable, scene_light_shadow_map_bias */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS,

    /* not settable, scene_light_shadow_map_pointlight_algorithm */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM,

    /* NOT gettable, settable, float */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_CUTOFF,

    /* NOT gettable, settable, float.
     *
     * This property is per-light, as opposed to _max_variance. The reason is that
     * min variance can theoretically be different for each light.
     **/
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_VSM_MIN_VARIANCE,

    OGL_UBER_ITEM_PROPERTY_LIGHT_USES_SHADOW_MAP,
    OGL_UBER_ITEM_PROPERTY_LIGHT_TYPE,
    OGL_UBER_ITEM_PROPERTY_TYPE,


    /* Always last */
    OGL_UBER_ITEM_PROPERTY_UNKNOWN
} _ogl_uber_item_property;

typedef struct
{
    GLuint bo_id;
    GLuint bo_offset;
} _ogl_uber_light_sh_data;


typedef unsigned int ogl_uber_item_id;

/** TODO */
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_input_fragment_attribute_item(__in __notnull ogl_uber                           uber,
                                                                               __in __notnull _ogl_uber_input_fragment_attribute input_attribute);

/** TODO */
PUBLIC EMERALD_API ogl_uber_item_id ogl_uber_add_light_item(__in __notnull                        ogl_uber                         uber,
                                                            __in_opt                              scene_light                      light_instance,
                                                            __in                                  shaders_fragment_uber_light_type light_type,
                                                            __in                                  bool                             is_shadow_caster,
                                                            __in __notnull                        unsigned int                     n_light_properties,
                                                            __in_ecount_opt(n_light_properties*2) void*                            light_property_values);

/** TODO */
PUBLIC EMERALD_API ogl_uber ogl_uber_create(__in __notnull ogl_context                context,
                                            __in __notnull system_hashed_ansi_string  name);

/** TODO */
PUBLIC EMERALD_API ogl_uber ogl_uber_create_from_ogl_program(__in __notnull ogl_context               context,
                                                             __in __notnull system_hashed_ansi_string name,
                                                             __in __notnull ogl_program               program);

/** TODO */
PUBLIC EMERALD_API void ogl_uber_get_shader_general_property(__in  __notnull const ogl_uber             uber,
                                                             __in            _ogl_uber_general_property property,
                                                             __out __notnull void*                      out_result);

/** TODO */
PUBLIC EMERALD_API void ogl_uber_get_shader_item_property(__in __notnull const ogl_uber          uber,
                                                          __in           ogl_uber_item_id        item_id,
                                                          __in           _ogl_uber_item_property property,
                                                          __out          void*                   result);

/** TODO */
PUBLIC EMERALD_API void ogl_uber_link(__in __notnull ogl_uber uber);

/** TODO */
PUBLIC void ogl_uber_rendering_render_mesh(__in     __notnull mesh                 mesh_gpu,
                                           __in     __notnull system_matrix4x4     model,
                                           __in     __notnull system_matrix4x4     normal_matrix,
                                           __in     __notnull ogl_uber             uber,
                                           __in_opt           mesh_material        material,
                                           __in               system_timeline_time time);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_start(__in __notnull ogl_uber);

/** TODO */
PUBLIC EMERALD_API void ogl_uber_set_shader_general_property(__in __notnull ogl_uber                   uber,
                                                             __in           _ogl_uber_general_property property,
                                                             __in           const void*                data);

/** TODO */
PUBLIC EMERALD_API void ogl_uber_set_shader_item_property(__in __notnull ogl_uber                uber,
                                                          __in           unsigned int            item_index,
                                                          __in           _ogl_uber_item_property property,
                                                          __in           const void*             data);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void ogl_uber_rendering_stop(__in __notnull ogl_uber);

#endif /* OGL_UBER_H */
