#ifndef RAL_UTILS_H
#define RAL_UTILS_H

#include "ral/ral_types.h"


typedef enum
{
    /* bool.
     *
     * true if the texture format is compressed; false otherwise */
    RAL_TEXTURE_FORMAT_PROPERTY_IS_COMPRESSED,

    /* uint32_t.
     *
     * Number of components that a given texture format provides data for.
     */
    RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS
} ral_texture_format_property;

/** Provides information about specified RAL texture format.
 *
 *  @param texture_format RAL texture format to return info for.
 *  @param property       Property to use for the query.
 *  @param out_result_ptr Deref will be set to the requested value if the function returns true.
 *                        Not touched otherwise.
 *
 *  @return true if the query was successful, false otherwise.
 **/
PUBLIC EMERALD_API bool ral_utils_get_texture_format_property(ral_texture_format          texture_format,
                                                              ral_texture_format_property property,
                                                              void*                       out_result_ptr);

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