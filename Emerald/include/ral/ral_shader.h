#ifndef RAL_SHADER_H
#define RAL_SHADER_H

#include "ral/ral_types.h"

typedef enum
{
    /* Notification fired whenever the shader body is updated.
     *
     * arg: ral_shader instance.
     */
    RAL_SHADER_CALLBACK_ID_GLSL_BODY_UPDATED,

    /* Always last */
    RAL_SHADER_CALLBACK_ID_COUNT,
};

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_SHADER_PROPERTY_CALLBACK_MANAGER,

    /* not settable; system_hashed_ansi_string.
     *
     * Only valid if RAL_SHADER_PROPERTY_SOURCE property value is RAL_SHADER_SOURCE_GLSL
     */
    RAL_SHADER_PROPERTY_GLSL_BODY,

    /* not settable; system_hashed_ansi_string */
    RAL_SHADER_PROPERTY_NAME,

    /* settable; ral_shader_source
     *
     * Tells what representation the shader uses
     */
    RAL_SHADER_PROPERTY_SOURCE,

    /* not settable; ral_shader_type.
     *
     * Tells what pipeline stage the shader is designed for.
     */
    RAL_SHADER_PROPERTY_TYPE
} ral_shader_property;


/** TODO
 *
 *  NOTE: This function should only be used by ral_context. Use ral_context_create_shaders() to create
 *        a shader instance instead.
 **/
PUBLIC ral_shader ral_shader_create(const ral_shader_create_info* shader_create_info_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_shader_get_property(ral_shader          shader,
                                                ral_shader_property property,
                                                void*               out_result_ptr);

/** TODO */
PUBLIC void ral_shader_release(ral_shader& shader);

/** TODO */
PUBLIC EMERALD_API void ral_shader_set_property(ral_shader          shader,
                                                ral_shader_property property,
                                                const void*         data);

#endif /* RAL_SHADER_H */