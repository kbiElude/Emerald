/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef MESH_MATERIAL_H
#define MESH_MATERIAL_H

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
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_NORMAL,
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_AMBIENT,
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_DIFFUSE,
    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_TEXCOORD_SPECULAR,

    MESH_MATERIAL_INPUT_FRAGMENT_ATTRIBUTE_UNKNOWN
} mesh_material_input_fragment_attribute;

typedef enum
{
    MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,       /* not settable, system_callback_manager         */
    MESH_MATERIAL_PROPERTY_NAME,                   /* not settable, system_hashed_ansi_string       */
    MESH_MATERIAL_PROPERTY_SHADING,                /* settable,     mesh_material_shading           */
    MESH_MATERIAL_PROPERTY_UV_MAP_NAME,            /* settable,     system_hashed_ansi_string       */
    MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE, /* settable,     float. Negative value indicates */
                                                   /*               vertex smoothing is disabled.   */

} mesh_material_property;

typedef enum
{
    MESH_MATERIAL_PROPERTY_ATTACHMENT_NONE,
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

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create(__in __notnull system_hashed_ansi_string name,
                                                      __in __notnull ogl_context               context);

/** TODO */
PUBLIC EMERALD_API mesh_material mesh_material_create_copy(__in __notnull system_hashed_ansi_string name,
                                                           __in __notnull mesh_material             src_material);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string mesh_material_get_name(__in __notnull mesh_material material);

/** TODO.
 *
 *  Input scene can be NULL, in which case it is assumed the returned ogl_uber does not need
 *  to consider lighting.
 */
PUBLIC EMERALD_API ogl_uber mesh_material_get_ogl_uber(__in     __notnull mesh_material material,
                                                       __in_opt           scene         scene);

/** TODO */
PUBLIC EMERALD_API void mesh_material_get_property(__in  __notnull mesh_material          material,
                                                   __in            mesh_material_property property,
                                                   __out __notnull void*                  out_result);

/** TODO */
PUBLIC EMERALD_API mesh_material_property_attachment mesh_material_get_shading_property_attachment_type(__in __notnull mesh_material                  material,
                                                                                                        __in           mesh_material_shading_property property);

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
PUBLIC mesh_material mesh_material_load(__in __notnull system_file_serializer serializer,
                                        __in __notnull ogl_context            context,
                                        __in __notnull system_hash64map       texture_id_to_ogl_texture_map);

/** TODO */
PUBLIC bool mesh_material_save(__in __notnull system_file_serializer serializer,
                               __in __notnull mesh_material          material);

/** TODO */
PUBLIC EMERALD_API void mesh_material_set_property(__in __notnull mesh_material          material,
                                                   __in           mesh_material_property property,
                                                   __in __notnull const void*            data);

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
