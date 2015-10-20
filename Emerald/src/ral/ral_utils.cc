/**
 *
 * Emerald (kbi/elude @ 2015)
 *
 * Rendering Abstraction Layer utility functions
 */
#include "shared.h"
#include "ral/ral_utils.h"

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_is_texture_type_cubemap(ral_texture_type texture_type)
{
    return (texture_type == RAL_TEXTURE_TYPE_CUBE_MAP        ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_is_texture_type_layered(ral_texture_type texture_type)
{
    return (texture_type == RAL_TEXTURE_TYPE_1D_ARRAY             ||
            texture_type == RAL_TEXTURE_TYPE_2D_ARRAY             ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY       ||
            texture_type == RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_is_texture_type_mipmappable(ral_texture_type texture_type)
{
    return (texture_type == RAL_TEXTURE_TYPE_1D             ||
            texture_type == RAL_TEXTURE_TYPE_1D_ARRAY       ||
            texture_type == RAL_TEXTURE_TYPE_2D             ||
            texture_type == RAL_TEXTURE_TYPE_2D_ARRAY       ||
            texture_type == RAL_TEXTURE_TYPE_3D             ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP       ||
            texture_type == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_is_texture_type_multisample(ral_texture_type texture_type)
{
    return (texture_type == RAL_TEXTURE_TYPE_MULTISAMPLE_2D       ||
            texture_type == RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY);
}