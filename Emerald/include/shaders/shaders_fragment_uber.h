/**
 *
 * Emerald (kbi/elude @2014)
 * 
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_UBER_H
#define SHADERS_FRAGMENT_UBER_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_uber,
                             shaders_fragment_uber)

typedef enum
{
    UBER_INPUT_ATTRIBUTE_NORMAL,
    UBER_INPUT_ATTRIBUTE_TEXCOORD,

    UBER_INPUT_ATTRIBUTE_UNKNOWN
} shaders_fragment_uber_input_attribute_type;

typedef enum
{
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_NONE,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_AMBIENT, /* just a static vec3 added to the result, really :) */
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_DIRECTIONAL,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_LAMBERT_POINT,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH3,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PROJECTION_SH4,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_DIRECTIONAL,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_POINT,
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_PHONG_SPOT,

    /* Always last */
    SHADERS_FRAGMENT_UBER_LIGHT_TYPE_COUNT
} shaders_fragment_uber_light_type;

typedef enum
{
    /* Default value: SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE */
    SHADERS_FRAGMENT_UBER_PROPERTY_AMBIENT_DATA_SOURCE,

    /* Default value: SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE */
    SHADERS_FRAGMENT_UBER_PROPERTY_DIFFUSE_DATA_SOURCE,

    /* Default value: SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE */
    SHADERS_FRAGMENT_UBER_PROPERTY_LUMINOSITY_DATA_SOURCE,

    /* Default value: SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE */
    SHADERS_FRAGMENT_UBER_PROPERTY_SHININESS_DATA_SOURCE,

    /* Default value: SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE */
    SHADERS_FRAGMENT_UBER_PROPERTY_SPECULAR_DATA_SOURCE,

    /* Always last */
    SHADERS_FRAGMENT_UBER_PROPERTY_COUNT
} shaders_fragment_uber_property;

typedef enum
{
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_NONE,
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_FLOAT,
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_CURVE_CONTAINER_VEC3,
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_FLOAT,
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_TEXTURE2D,
    SHADERS_FRAGMENT_UBER_PROPERTY_VALUE_VEC4,
} shaders_fragment_uber_property_value;

typedef enum
{
    SHADERS_FRAGMENT_UBER_ITEM_INPUT_ATTRIBUTE,
    SHADERS_FRAGMENT_UBER_ITEM_LIGHT,
    SHADERS_FRAGMENT_UBER_ITEM_NONE,

    /* Always last */
    SHADERS_FRAGMENT_UBER_ITEM_UNKNOWN
} shaders_fragment_uber_item_type;


typedef unsigned int shaders_fragment_uber_item_id;

/** TODO */
typedef enum
{
    /* uses _shaders_fragment_uber_new_fragment_input_callback structure for data */
    SHADERS_FRAGMENT_UBER_PARENT_CALLBACK_NEW_FRAGMENT_INPUT,

    /* .. */
} _shaders_fragment_uber_parent_callback_type;

/** TODO */
typedef struct _shaders_fragment_uber_new_fragment_input_callback
{
    system_hashed_ansi_string fs_attribute_name;
    _shader_variable_type     fs_attribute_type;
    system_hashed_ansi_string vs_attribute_name;

    _shaders_fragment_uber_new_fragment_input_callback()
    {
        fs_attribute_name = NULL;
        fs_attribute_type = TYPE_UNKNOWN;
        vs_attribute_name = NULL;
    }

} _shaders_fragment_uber_new_fragment_input_callback;

/** Call-back used by shaders_fragment_uber_add_light() to inform the parent object (eg. ogl_uber)
 *  about a certain event. Check documentation for the first argument for more details. */
typedef void (*PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC)(_shaders_fragment_uber_parent_callback_type type,
                                                         void*                                       data,
                                                         void*                                       user_arg);

/** TODO */
PUBLIC EMERALD_API shaders_fragment_uber_item_id shaders_fragment_uber_add_input_attribute_contribution(shaders_fragment_uber                      uber,
                                                                                                        shaders_fragment_uber_input_attribute_type attribute_type,
                                                                                                        PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC   pCallbackProc,
                                                                                                        void*                                      user_arg);

/** TODO.
 *
 *  @param diffuse_property_values Stores n_diffuse_properties instances of <shaders_fragment_uber_diffuse_property,
 *                                 shaders_fragment_uber_diffuse_property_value> pairs.
 **/
PUBLIC EMERALD_API shaders_fragment_uber_item_id shaders_fragment_uber_add_light(shaders_fragment_uber                    uber,
                                                                                 shaders_fragment_uber_light_type         light_type,
                                                                                 scene_light                              light_instance,
                                                                                 bool                                     is_shadow_caster,
                                                                                 unsigned int                             n_light_properties,
                                                                                 void*                                    light_property_values,
                                                                                 PFNSHADERSFRAGMENTUBERPARENTCALLBACKPROC pCallbackProc,
                                                                                 void*                                    user_arg);

/** Creates a shaders_fragment_uber object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    TODO
 * 
 *  @return shaders_fragment_static instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_uber shaders_fragment_uber_create(ral_context                context,
                                                                      system_hashed_ansi_string  name);

/** TODO */
PUBLIC EMERALD_API bool shaders_fragment_uber_get_item_type(shaders_fragment_uber            uber,
                                                            shaders_fragment_uber_item_id    item_id,
                                                            shaders_fragment_uber_item_type* out_item_type);

/** TODO */
PUBLIC EMERALD_API bool shaders_fragment_uber_get_light_item_properties(const shaders_fragment_uber       uber,
                                                                        shaders_fragment_uber_item_id     item_id,
                                                                        shaders_fragment_uber_light_type* out_light_type);

/** TODO */
PUBLIC EMERALD_API uint32_t shaders_fragment_uber_get_n_items(shaders_fragment_uber uber);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param uber Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_uber_get_shader(shaders_fragment_uber uber);

/** TODO */
PUBLIC EMERALD_API bool shaders_fragment_uber_is_dirty(shaders_fragment_uber uber);

/** TODO */
PUBLIC EMERALD_API void shaders_fragment_uber_recompile(shaders_fragment_uber uber);

#endif /* SHADERS_FRAGMENT_UBER_H */