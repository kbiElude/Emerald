/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef MESH_MATERIAL_H
#define MESH_MATERIAL_H

#include "curve/curve_types.h"
#include "mesh/mesh_types.h"
#include "ogl/ogl_types.h"
#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(mesh_material, mesh_material)

typedef enum
{
    MESH_MATERIAL_CALLBACK_ID_VSA_CHANGED, /* vertex smoothing angle changed; callback_proc_data: source mesh_material instance */

    /* Always last */
    MESH_MATERIAL_CALLBACK_ID_COUNT
} mesh_material_callback_id;


typedef enum
{
    MESH_MATERIAL_DATA_VECTOR_LIGHT_VECTOR,
    MESH_MATERIAL_DATA_VECTOR_VIEW_VECTOR,

    MESH_MATERIAL_DATA_VECTOR_UNKNOWN
} mesh_material_data_vector;

typedef enum
{
    /* The default FS behavior is unaffected. */
    MESH_MATERIAL_FS_BEHAVIOR_DEFAULT,

    /* Discards samples, whose clip_depth < 0. */
     MESH_MATERIAL_FS_BEHAVIOR_DUAL_PARABOLOID_SM

} mesh_material_fs_behavior;

typedef enum
{
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL,
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD,

    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN
} mesh_material_input_fragment_attribute;

typedef enum
{
    /* not settable, system_callback_manager */
    MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,

    /* settable, mesh_material_fs_behavior.
     *
     * Tells which fragment shader implementation should be used.
     * Unless for specialized behavior, you'll be OK to go with
     * the default setting.
     */
    MESH_MATERIAL_PROPERTY_FS_BEHAVIOR,

    /* not settable, system_hashed_ansi_string */
    MESH_MATERIAL_PROPERTY_NAME,

    /* settable, mesh_material_shading */
    MESH_MATERIAL_PROPERTY_SHADING,

    /* not settable, scene_material */
    MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL,

    /* settable, system_hashed_ansi_string */
    MESH_MATERIAL_PROPERTY_UV_MAP_NAME,

    /* settable, float.
     *
     * Negative value indicates vertex smoothing is disabled.
     */
    MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,

    /* settable, mesh_material_vs_behavior.
     *
     * Tells which vertex shader implementation should be used.
     * Unless for specialized behavior, you'll be OK to go with
     * the default setting.
     */
    MESH_MATERIAL_PROPERTY_VS_BEHAVIOR,

} mesh_material_property;

typedef enum
{
    MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_FLOAT,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_CURVE_CONTAINER_VEC3,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_FLOAT,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_INPUT_FRAGMENT_ATTRIBUTE,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_TEXTURE,
    MESH_MATERIAL_PROPERTY_ATTACHMENT_VEC4,

    /* Always last */
    MESH_MATERIAL_PROPERTY_ATTACHMENT_UNKNOWN
} mesh_material_property_attachment;

typedef enum
{
    MESH_MATERIAL_SHADING_LAMBERT,
    MESH_MATERIAL_SHADING_PHONG,

    MESH_MATERIAL_SHADING_NONE, /* used for dummy material, which is in turn used for shadow map rendering */
    MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE,

    /* Always last */
    MESH_MATERIAL_SHADING_UNKNOWN
} mesh_material_shading;

typedef enum
{
    MESH_MATERIAL_SHADING_PROPERTY_FIRST,

    /* only used if MESH_MATERIAL_SHADING_LAMBERT or MESH_MATERIAL_SHADING_PHONG shading is used.
     * Supports texture & vec4 attachments.
     */
    MESH_MATERIAL_SHADING_PROPERTY_AMBIENT = MESH_MATERIAL_SHADING_PROPERTY_FIRST,
    MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,

    /* Currently only supports float attachments */
    MESH_MATERIAL_SHADING_PROPERTY_LUMINOSITY,
    MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
    MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,

    /* only used if MESH_MATERIAL_SHADING_INPUT_FRAGMENT_ATTRIBUTE shading is used.
     * Only supports "input fragment attribute" attachments
     */
    MESH_MATERIAL_SHADING_PROPERTY_INPUT_ATTRIBUTE,

    /* Always last */
    MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN,
    MESH_MATERIAL_SHADING_PROPERTY_COUNT = MESH_MATERIAL_SHADING_PROPERTY_UNKNOWN
} mesh_material_shading_property;

typedef enum
{
    MESH_MATERIAL_TEXTURE_FILTERING_LINEAR,
    MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR,
    MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST,
    MESH_MATERIAL_TEXTURE_FILTERING_NEAREST,
    MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR,
    MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST,

    /* Always last */
    MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN
} mesh_material_texture_filtering;

typedef enum
{
    /* Outputs all data necessary to carry out shading in world space.
     * Sets gl_Position to clip-space position of the input vertex.
     */
    MESH_MATERIAL_VS_BEHAVIOR_DEFAULT,

    /* Sets gl_Position to clip-space position of the input vertex,
     * adjusted for the dual paraboloid shadow mapping purposes.
     * Output clip_depth to FS, so that fragments outside the paraboloid
     * can be discarded.
     */
     MESH_MATERIAL_VS_BEHAVIOR_DUAL_PARABOLOID_SM

} mesh_material_vs_behavior;

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create(__in __notnull system_hashed_ansi_string name,
                                                      __in __notnull ogl_context               context,
                                                      __in_opt       system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_copy(__in __notnull system_hashed_ansi_string name,
                                                           __in __notnull mesh_material             src_material);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_scene_material(__in __notnull scene_material src_material,
                                                                          __in_opt       ogl_context    context);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_fs_behavior_has(__in mesh_material_fs_behavior fs_behavior);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_property_attachment_has(__in mesh_material_property_attachment attachment);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_has(__in mesh_material_shading shading);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_property_has(__in mesh_material_shading_property property);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_vs_behavior_has(__in mesh_material_vs_behavior vs_behavior);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed the returned ogl_uber does not need
 *  to consider lighting.
 */
PUBLIC EMERALD_API ogl_uber mesh_material_get_ogl_uber(__in     __notnull mesh_material material,
                                                       __in_opt           scene         scene,
                                                       __in               bool          use_shadow_maps);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_property(__in  __notnull mesh_material          material,
                                                   __in            mesh_material_property property,
                                                   __out __notnull void*                  out_result);

/** TODO */
PUBLIC EMERALD_API mesh_material_property_attachment mesh_material_get_shading_property_attachment_type(__in __notnull mesh_material                  material,
                                                                                                        __in           mesh_material_shading_property property);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_float(__in      __notnull mesh_material                  material,
                                                                                       __in                mesh_material_shading_property property,
                                                                                       __in                system_timeline_time           time,
                                                                                       __out_opt           float*                         out_float_value);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_vec3(__in      __notnull mesh_material                  material,
                                                                                      __in                mesh_material_shading_property property,
                                                                                      __in                system_timeline_time           time,
                                                                                      __out_ecount_opt(3) float*                         out_vec3_value);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_float(__in      __notnull mesh_material                  material,
                                                                       __in                mesh_material_shading_property property,
                                                                       __out_opt           float*                         out_float_value);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_input_fragment_attribute(__in __notnull mesh_material                           material,
                                                                                          __in           mesh_material_shading_property          property,
                                                                                          __out_opt      mesh_material_input_fragment_attribute* out_attribute);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_texture(__in      __notnull mesh_material                    material,
                                                                         __in                mesh_material_shading_property   property,
                                                                         __out_opt           ogl_texture*                     out_texture,
                                                                         __out_opt           unsigned int*                    out_mipmap_level,
                                                                         __out_opt           ogl_sampler*                     out_sampler);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_vec4(__in           __notnull mesh_material                  material,
                                                                      __in                     mesh_material_shading_property property,
                                                                      __out_ecount(4)          float*                         out_vec4_data);

/** TODO */
PUBLIC bool mesh_material_is_a_match_to_mesh_material(__in __notnull mesh_material material_a,
                                                      __in __notnull mesh_material material_b);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_property(__in __notnull mesh_material          material,
                                                   __in           mesh_material_property property,
                                                   __in __notnull const void*            data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_float(__in __notnull mesh_material                  material,
                                                                                    __in           mesh_material_shading_property property,
                                                                                    __in           curve_container                data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_vec3(__in           __notnull mesh_material                  material,
                                                                                   __in                     mesh_material_shading_property property,
                                                                                   __in_ecount(3)           curve_container*               data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(__in __notnull mesh_material                  material,
                                                                    __in           mesh_material_shading_property property,
                                                                    __in           float                          value);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_input_fragment_attribute(__in __notnull mesh_material                          material,
                                                                                       __in           mesh_material_shading_property         property,
                                                                                       __in           mesh_material_input_fragment_attribute attribute);

/** TODO.
 *
 *  (ogl_texture here is not perfect since it's a GL asset, not a gfx_image, but oh well)
 */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_texture(__in __notnull mesh_material                   material,
                                                                      __in           mesh_material_shading_property  property,
                                                                      __in           ogl_texture                     texture,
                                                                      __in           unsigned int                    mipmap_level,
                                                                      __in           mesh_material_texture_filtering mag_filter,
                                                                      __in           mesh_material_texture_filtering min_filter);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_vec4(__in __notnull mesh_material                  material,
                                                                   __in           mesh_material_shading_property property,
                                                                   __in_ecount(4) const float*                   data);

#endif /* MESH_MATERIAL_H */
