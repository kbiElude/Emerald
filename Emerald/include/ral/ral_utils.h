#ifndef RAL_UTILS_H
#define RAL_UTILS_H

#include "ral/ral_types.h"


typedef enum
{
    /* ral_format_type */
    RAL_FORMAT_PROPERTY_FORMAT_TYPE,

    /* bool.
     *
     * true if the format can be used to describe color data; false otherwise */
    RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,

    /* bool.
     *
     * true if the format can be used to describe depth data; false otherwise */
    RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,

    /* bool.
     *
     * true if the format can be used to describe stencil data; false otherwise */
    RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,

    /* system_hashed_ansi_string
     *
     * Null if no image layout qualifier exists for the specified format.
     */
    RAL_FORMAT_PROPERTY_IMAGE_LAYOUT_QUALIFIER,

    /* bool.
     *
     * true if the format is compressed; false otherwise */
    RAL_FORMAT_PROPERTY_IS_COMPRESSED,

    /* uint32_t.
     *
     * Number of components that a given format provides data for.
     */
    RAL_FORMAT_PROPERTY_N_COMPONENTS,

    /* const char*
     *
     * format name as a null-terminated string
     */
    RAL_FORMAT_PROPERTY_NAME,
} ral_format_property;

typedef enum
{
    /* system_hashed_ansi_string.
     *
     * GLSL image layout qualifier, corresponding to the texture type and data type,
     * as indicated by the property's name.
     */
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_IMAGE_LAYOUT_QUALIFIER,
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_SINT_IMAGE_LAYOUT_QUALIFIER,
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_UINT_IMAGE_LAYOUT_QUALIFIER,

    /* system_hashed_ansi_string.
     *
     * GLSL sampler type, corresponding to the texture type and data type,
     * as indicated by the property's name.
     */
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_SAMPLER_TYPE,
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_SINT_SAMPLER_TYPE,
    RAL_TEXTURE_TYPE_PROPERTY_GLSL_UINT_SAMPLER_TYPE,

    /* uint32_t.
     *
     * Number of dimensions used by the texture type. */
    RAL_TEXTURE_TYPE_PROPERTY_N_DIMENSIONS

} ral_texture_type_property;

/** TODO */
PUBLIC system_hashed_ansi_string ral_utils_get_ral_context_object_type_has(ral_context_object_type object_type);

/** Provides information about specified RAL format.
 *
 *  @param texture_format RAL format to return info for.
 *  @param property       Property to use for the query.
 *  @param out_result_ptr Deref will be set to the requested value if the function returns true.
 *                        Not touched otherwise.
 *
 *  @return true if the query was successful, false otherwise.
 **/
PUBLIC EMERALD_API bool ral_utils_get_format_property(ral_format          format,
                                                      ral_format_property property,
                                                      void*               out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_utils_get_ral_program_variable_type_class_property(ral_program_variable_type_class          variable_type_class,
                                                                               ral_program_variable_type_class_property property,
                                                                               void**                                   out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_utils_get_ral_program_variable_type_property(ral_program_variable_type          variable_type,
                                                                         ral_program_variable_type_property property,
                                                                         void**                             out_result_ptr);

/** Provides information about specified RAL texture type.
 *
 *  @param texture_type   RAL texture type to return info for.
 *  @param property       Property to use for the query.
 *  @param out_result_ptr Deref will be set to the requetsed value if the function returns true.
 *                        Not touched otherwise. Must not be NULL.
 *
 *  @return true if the query was successful, false otherwise.
 */
PUBLIC EMERALD_API bool ral_utils_get_texture_type_property(ral_texture_type          texture_type,
                                                            ral_texture_type_property property,
                                                            void*                     out_result_ptr);

/** Tells whether the specified RAL texture type holds cube-map data.
 *
 *  @param texture_type RAL texture type to use for the uqery.
 *
 *  @return true if the specified texture type is a cube-map or a cube-map array;
 *          false otherwise.
 **/
PUBLIC EMERALD_API bool ral_utils_is_texture_type_cubemap(ral_texture_type texture_type);

/** Tells whether the specified RAL texture type is layered or not.
 *
 *  @param texture_type RAL texture type to use for the query.
 *
 *  @return true if the specified texture type is layered; false otherwise.
 *          Cube-map and 3D texture types are NOT considered layered by this
 *          function.
 **/
PUBLIC EMERALD_API bool ral_utils_is_texture_type_layered(ral_texture_type texture_type);

/** Tells whether the specified RAL texture type can hold mipmaps.
 *
 *  @param texture_type RAL texture type to use for the query.
 *
 *  @return true if the specified texture type is mipmappable, false otherwise.
 **/
PUBLIC EMERALD_API bool ral_utils_is_texture_type_mipmappable(ral_texture_type texture_type);

/** Tells whether the specified RAL texture type is multisample or not.
 *
 *  @param texture_type RAL texture type to use for the query.
 *
 *  @return true if the specified texture type is multisample, false otherwise.
 **/
PUBLIC EMERALD_API bool ral_utils_is_texture_type_multisample(ral_texture_type texture_type);

#endif /* RAL_UTILS_H */