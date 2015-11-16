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
        bool     is_compressed;
        uint32_t n_components;
    } format_data[] =
    {
        /* RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_UNORM */
        {true, 1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_UNORM */
        {true, 1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_UNORM */
        {true, 2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_UNORM */
        {true, 2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT */
        {true, 3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT */
        {true, 3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_ETC2_UNORM */
        {true, 3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RGBA_BPTC_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_SNORM */
        {true, 1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_SNORM */
        {true, 1},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_SNORM */
        {true, 2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_SNORM */
        {true, 2},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM */
        {true, 3},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM */
        {true, 4},

        /* RAL_TEXTURE_FORMAT_DEPTH16_SNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_DEPTH24_SNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_DEPTH32_FLOAT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_DEPTH32_SNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_DEPTH24_STENCIL8 */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_DEPTH32F_STENCIL8 */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_R11FG11FB10F */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_R16_FLOAT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R16_SINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R16_SNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R16_UINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R16_UNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R3G3B2_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_R32_FLOAT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R32_SINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R32_UINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R8_SINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R8_SNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R8_UINT */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_R8_UNORM */
        {false, 1},

        /* RAL_TEXTURE_FORMAT_RG16_FLOAT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG16_SINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG16_SNORM */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG16_UINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG16_UNORM */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG32_FLOAT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG32_SINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG32_UINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG8_UNORM */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG8_SINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG8_SNORM */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RG8_UINT */
        {false, 2},

        /* RAL_TEXTURE_FORMAT_RGB10_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB10A2_UINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGB10A2_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGB12_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB16_FLOAT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB16_SINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB16_SNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB16_UINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB16_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB32_FLOAT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB32_SINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB32_UINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB4_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB5_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB5A1_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGB8_SINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB8_SNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB8_UINT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB8_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGB9E5_FLOAT */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_RGBA12_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA16_FLOAT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA16_SINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA16_SNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA16_UINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA16_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA2_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA32_FLOAT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA32_SINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA32_UINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA4_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA8_UNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA8_SINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA8_SNORM */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_RGBA8_UINT */
        {false, 4},

        /* RAL_TEXTURE_FORMAT_SRGB8_UNORM */
        {false, 3},

        /* RAL_TEXTURE_FORMAT_SRGBA8_UNORM */
        {false, 4},
    };

    ASSERT_DEBUG_SYNC(sizeof(format_data) / sizeof(format_data[0]) == RAL_TEXTURE_FORMAT_COUNT,
                      "Predefined data out of sync!");

    switch (property)
    {
        case RAL_TEXTURE_FORMAT_PROPERTY_IS_COMPRESSED:
        {
            *(bool*) out_result_ptr = format_data[texture_format].is_compressed;

            break;
        }

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