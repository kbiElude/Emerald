/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef MESH_MATERIAL_H
#define MESH_MATERIAL_H

#include "curve/curve_types.h"
#include "mesh/mesh_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"
#include "scene_renderer/scene_renderer_types.h"

REFCOUNT_INSERT_DECLARATIONS(mesh_material,
                             mesh_material)

typedef enum
{
    /* material property change resulted in a scene_renderer_uber update.
     *
     * callback_proc_data: source mesh_material instance
     */
    MESH_MATERIAL_CALLBACK_ID_UBER_UPDATED,

    /* vertex smoothing angle changed;
     *
     * callback_proc_data: source mesh_material instance
     */
    MESH_MATERIAL_CALLBACK_ID_VSA_CHANGED,

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
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL,
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD,

    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN
} mesh_material_input_fragment_attribute;

typedef enum
{
    /* not settable, system_callback_manager */
    MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,

    /* not settable, ral_context
     *
     * Tells what RAL context was used to instantiate the instance.
     **/
    MESH_MATERIAL_PROPERTY_CONTEXT,

    /* not settable, system_hashed_ansi_string */
    MESH_MATERIAL_PROPERTY_NAME,

    /* settable, mesh_material_shading */
    MESH_MATERIAL_PROPERTY_SHADING,

    /* not settable, ral_program */
    MESH_MATERIAL_PROPERTY_SOURCE_RAL_PROGRAM,

    /* not settable, scene_material */
    MESH_MATERIAL_PROPERTY_SOURCE_SCENE_MATERIAL,

    /* not settable, mesh_material_type */
    MESH_MATERIAL_PROPERTY_TYPE,

    /* settable, system_hashed_ansi_string */
    MESH_MATERIAL_PROPERTY_UV_MAP_NAME,

    /* settable, float.
     *
     * Negative value indicates vertex smoothing is disabled.
     */
    MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,

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
    MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_LINEAR,
    MESH_MATERIAL_TEXTURE_FILTERING_LINEAR_NEAREST,
    MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_LINEAR,
    MESH_MATERIAL_TEXTURE_FILTERING_NEAREST_NEAREST,

    /* Always last */
    MESH_MATERIAL_TEXTURE_FILTERING_UNKNOWN
} mesh_material_texture_filtering;

/* Tells how the mesh_material instance was initialized. */
typedef enum
{
    /* Created with:
     *
     * mesh_material_create()
     * mesh_material_create_from_scene_material()
     */
    MESH_MATERIAL_TYPE_GENERAL,

    /* Created with mesh_material_create_from_shader_bodies() */
    MESH_MATERIAL_TYPE_PROGRAM,

    MESH_MATERIAL_TYPE_UNDEFINED
} mesh_material_type;

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create(system_hashed_ansi_string name,
                                                      ral_context               context,
                                                      system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_copy(system_hashed_ansi_string name,
                                                           mesh_material             src_material);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_scene_material(scene_material src_material,
                                                                          ral_context    context);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_from_shader_bodies(system_hashed_ansi_string name,
                                                                         ral_context               context,
                                                                         system_hashed_ansi_string object_manager_path,
                                                                         system_hashed_ansi_string fs_body,
                                                                         system_hashed_ansi_string gs_body,
                                                                         system_hashed_ansi_string tc_body,
                                                                         system_hashed_ansi_string te_body,
                                                                         system_hashed_ansi_string vs_body);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_property_attachment_has(mesh_material_property_attachment attachment);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_has(mesh_material_shading shading);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_shading_property_has(mesh_material_shading_property property);

/** TODO
 *
 *  Internal usage only.
 */
PUBLIC system_hashed_ansi_string mesh_material_get_mesh_material_type_has(mesh_material_type type);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed the returned ogl_uber does not need
 *  to consider lighting.
 */
PUBLIC EMERALD_API scene_renderer_uber mesh_material_get_uber(mesh_material material,
                                                              scene         scene,
                                                              bool          use_shadow_maps);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_property(const mesh_material    material,
                                                   mesh_material_property property,
                                                   void*                  out_result_ptr);


/** TODO */
PUBLIC EMERALD_API mesh_material_property_attachment mesh_material_get_shading_property_attachment_type(mesh_material                  material,
                                                                                                        mesh_material_shading_property property);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_float(mesh_material                  material,
                                                                                       mesh_material_shading_property property,
                                                                                       system_time                    time,
                                                                                       float*                         out_float_value_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_curve_container_vec3(mesh_material                  material,
                                                                                      mesh_material_shading_property property,
                                                                                      system_time                    time,
                                                                                      float*                         out_vec3_value_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_float(mesh_material                  material,
                                                                       mesh_material_shading_property property,
                                                                       float*                         out_float_value_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_input_fragment_attribute(mesh_material                           material,
                                                                                          mesh_material_shading_property          property,
                                                                                          mesh_material_input_fragment_attribute* out_attribute_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_texture_view(mesh_material                   material,
                                                                              mesh_material_shading_property  property,
                                                                              ral_texture_view*               out_texture_view_ptr,
                                                                              ral_sampler*                    out_sampler_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_shading_property_value_vec4(mesh_material                  material,
                                                                      mesh_material_shading_property property,
                                                                      float*                         out_vec4_data_ptr);

/** TODO */
PUBLIC bool mesh_material_is_a_match_to_mesh_material(mesh_material material_a,
                                                      mesh_material material_b);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_property(mesh_material          material,
                                                   mesh_material_property property,
                                                   const void*            data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_float(mesh_material                  material,
                                                                                    mesh_material_shading_property property,
                                                                                    curve_container                data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_curve_container_vec3(mesh_material                  material,
                                                                                   mesh_material_shading_property property,
                                                                                   curve_container*               data);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_float(mesh_material                  material,
                                                                    mesh_material_shading_property property,
                                                                    float                          value);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_input_fragment_attribute(mesh_material                          material,
                                                                                       mesh_material_shading_property         property,
                                                                                       mesh_material_input_fragment_attribute attribute);

/** TODO. */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_texture_view(mesh_material                   material,
                                                                           mesh_material_shading_property  property,
                                                                           ral_texture_view                texture_view,
                                                                           mesh_material_texture_filtering mag_filter,
                                                                           mesh_material_texture_filtering min_filter);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_shading_property_to_vec4(mesh_material                  material,
                                                                   mesh_material_shading_property property,
                                                                   const float*                   data);

#endif /* MESH_MATERIAL_H */
