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

REFCOUNT_INSERT_DECLARATIONS(ogl_uber, ogl_uber)


typedef enum
{
    OGL_UBER_GENERAL_PROPERTY_CAMERA_LOCATION,     /* vec3                      */
    OGL_UBER_GENERAL_PROPERTY_FAR_NEAR_PLANE_DIFF, /* float                     */
    OGL_UBER_GENERAL_PROPERTY_FLIP_Z,              /* float                     */
    OGL_UBER_GENERAL_PROPERTY_NAME,                /* system_hashed_ansi_string */
    OGL_UBER_GENERAL_PROPERTY_NEAR_PLANE,          /* float                     */
    OGL_UBER_GENERAL_PROPERTY_N_ITEMS,
    OGL_UBER_GENERAL_PROPERTY_VP,

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
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_AMBIENT_COLOR,      /* settable, float[3].      used by ambient light */

    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_ATTENUATIONS,      /* settable, float[3].             used by point lights               */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_CONE_ANGLE,        /* settable, float, radians.       used by spot lights                */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIFFUSE,           /* settable, float[3].             used by directional & point lights */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_DIRECTION,         /* settable, float[3].             used by directional lights         */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_EDGE_ANGLE,        /* settable, float, radians.       used by spot lights.               */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_FAR_NEAR_DIFF,     /* settable, float,                used by point lights (>= 1 algo)   */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_LOCATION,          /* settable, float[3].             used by point lights               */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_NEAR_PLANE,        /* settable, float,                used by point lights (>= 1 algo)   */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_PROJECTION_MATRIX, /* settable, float[16], row-major. used by point lights (>= 1 algo)   */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_RANGE,             /* settable, float.                used by point & spot lights        */
    OGL_UBER_ITEM_PROPERTY_FRAGMENT_LIGHT_VIEW_MATRIX,       /* settable, float[16], row-major. used by point lights (>= 1 algo)   */

    OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_DEPTH_VP, /* settable, float[16], row-major */
    OGL_UBER_ITEM_PROPERTY_VERTEX_LIGHT_SH_DATA,  /* settable, _ogl_uber_light_sh_data */

    OGL_UBER_ITEM_PROPERTY_LIGHT_FALLOFF,        /* not settable, scene_light_falloff */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP,
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_BIAS,                 /* not settable, scene_light_shadow_map_bias */
    OGL_UBER_ITEM_PROPERTY_LIGHT_SHADOW_MAP_POINTLIGHT_ALGORITHM, /* not settable, scene_light_shadow_map_pointlight_algorithm */
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
                                            __in __notnull system_hashed_ansi_string  name,
                                            __in           shaders_fragment_uber_type fs_type,
                                            __in           shaders_vertex_uber_type   vs_type);

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
