/**
 *
 * Emerald (kbi/elude @ 2015)
 *
 * Rendering Abstraction Layer utility functions
 */
#include "shared.h"
#include "ral/ral_utils.h"

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_get_texture_format_property(ral_texture_format          texture_format,
                                                              ral_texture_format_property property,
                                                              void*                       out_result_ptr)
{
    bool result = true;

    static const struct _format_data
    {
        uint32_t n_components;
    } format_data[] =
    {
        /* RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_UNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_UNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_UNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_UNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT */
        {3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT */
        {3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_ETC2_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGBA_BPTC_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_SNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_SNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_DEPTH16_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_DEPTH24_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_DEPTH32_FLOAT */
        {1},

        /* RAL_TEXTURE_FORMAT_DEPTH32_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_DEPTH24_STENCIL8 */
        {2},

        /* RAL_TEXTURE_FORMAT_DEPTH32F_STENCIL8 */
        {2},

        /* RAL_TEXTURE_FORMAT_R11FG11FB10F */
        {3},

        /* RAL_TEXTURE_FORMAT_R16_FLOAT */
        {1},

        /* RAL_TEXTURE_FORMAT_R16_SINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R16_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_R16_UINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R16_UNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_R3G3B2_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_R32_FLOAT */
        {1},

        /* RAL_TEXTURE_FORMAT_R32_SINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R32_UINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R8_SINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R8_SNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_R8_UINT */
        {1},

        /* RAL_TEXTURE_FORMAT_R8_UNORM */
        {1},

        /* RAL_TEXTURE_FORMAT_RG16_FLOAT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG16_SINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG16_SNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_RG16_UINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG16_UNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_RG32_FLOAT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG32_SINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG32_UINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG8_UNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_RG8_SINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RG8_SNORM */
        {2},

        /* RAL_TEXTURE_FORMAT_RG8_UINT */
        {2},

        /* RAL_TEXTURE_FORMAT_RGB10_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB10A2_UINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGB10A2_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGB12_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB16_FLOAT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB16_SINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB16_SNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB16_UINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB16_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB32_FLOAT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB32_SINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB32_UINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB4_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB5_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB5A1_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGB8_SINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB8_SNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB8_UINT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB8_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_RGB9E5_FLOAT */
        {3},

        /* RAL_TEXTURE_FORMAT_RGBA12_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA16_FLOAT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA16_SINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA16_SNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA16_UINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA16_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA2_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA32_FLOAT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA32_SINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA32_UINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA4_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA8_UNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA8_SINT */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA8_SNORM */
        {4},

        /* RAL_TEXTURE_FORMAT_RGBA8_UINT */
        {4},

        /* RAL_TEXTURE_FORMAT_SRGB8_UNORM */
        {3},

        /* RAL_TEXTURE_FORMAT_SRGBA8_UNORM */
        {4},
    };

    ASSERT_DEBUG_SYNC(sizeof(format_data) / sizeof(format_data[0]) == RAL_TEXTURE_FORMAT_COUNT,
                      "Predefined data out of sync!");

    switch (property)
    {
        case RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS:
        {
            *(uint32_t*) out_result_ptr = format_data[texture_format].n_components;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_format_property value");

            result = false;
        }
    } /* switch (property) */

    return result;
}

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