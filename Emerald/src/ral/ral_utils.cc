/**
 *
 * Emerald (kbi/elude @ 2015 - 2016)
 *
 * Rendering Abstraction Layer utility functions
 */
#include "shared.h"
#include "ral/ral_utils.h"

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_get_format_property(ral_format          format,
                                                      ral_format_property property,
                                                      void*               out_result_ptr)
{
    bool result = true;

    static const struct _format_data
    {
        ral_format_type format_type;
        bool            has_color_data;
        bool            has_depth_data;
        bool            has_stencil_data;
        bool            is_compressed;
        uint32_t        n_components;
        const char*     name;
    } format_data[] =
    {
        /* format_type                  | has_color | has_depth | has_stencil | is_compressed | n_components | name */
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           1,             "RAL_FORMAT_COMPRESSED_R11_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           1,             "RAL_FORMAT_COMPRESSED_RED_RGTC1_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           2,             "RAL_FORMAT_COMPRESSED_RG11_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           2,             "RAL_FORMAT_COMPRESSED_RG_RGTC2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        true,           3,             "RAL_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT"},
        {  RAL_FORMAT_TYPE_UFLOAT, true,       false,      false,        true,           3,             "RAL_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           3,             "RAL_FORMAT_COMPRESSED_RGB8_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_RGBA_BPTC_UNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        true,           1,             "RAL_FORMAT_COMPRESSED_R11_EAC_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        true,           1,             "RAL_FORMAT_COMPRESSED_RED_RGTC1_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        true,           2,             "RAL_FORMAT_COMPRESSED_RG11_EAC_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        true,           2,             "RAL_FORMAT_COMPRESSED_RG_RGTC2_SNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           3,             "RAL_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        true,           4,             "RAL_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        false,          1,             "RAL_FORMAT_DEPTH16_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        false,          1,             "RAL_FORMAT_DEPTH24_SNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, false,      true,       false,        false,          1,             "RAL_FORMAT_DEPTH32_FLOAT"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        false,          1,             "RAL_FORMAT_DEPTH32_SNORM"},
        {  RAL_FORMAT_TYPE_DS,     false,      true,       true,         false,          2,             "RAL_FORMAT_DEPTH24_STENCIL8"},
        {  RAL_FORMAT_TYPE_DS,     false,      true,       true,         false,          2,             "RAL_FORMAT_DEPTH32F_STENCIL8"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          3,             "RAL_FORMAT_R11FG11FB10F"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          1,             "RAL_FORMAT_R16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          1,             "RAL_FORMAT_R16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          1,             "RAL_FORMAT_R16_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_R3G3B2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          1,             "RAL_FORMAT_R32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R32_UINT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          1,             "RAL_FORMAT_R8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          1,             "RAL_FORMAT_R8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          1,             "RAL_FORMAT_R8_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          2,             "RAL_FORMAT_RG16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          2,             "RAL_FORMAT_RG16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          2,             "RAL_FORMAT_RG16_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          2,             "RAL_FORMAT_RG32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          2,             "RAL_FORMAT_RG8_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          2,             "RAL_FORMAT_RG8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          2,             "RAL_FORMAT_RG8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB10_UNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGB10A2_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGB10A2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB12_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          3,             "RAL_FORMAT_RGB16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB16_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          3,             "RAL_FORMAT_RGB32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB4_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB5_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGB5A1_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          3,             "RAL_FORMAT_RGB8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_RGB8_UNORM" },
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          3,             "RAL_FORMAT_RGB9E5_FLOAT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA12_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA16_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA4_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA8_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        false,          4,             "RAL_FORMAT_RGBA8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          3,             "RAL_FORMAT_SRGB8_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        false,          4,             "RAL_FORMAT_SRGBA8_UNORM"},
    };

    static_assert(sizeof(format_data) / sizeof(format_data[0]) == RAL_FORMAT_COUNT,
                  "Predefined data out of sync!");

    switch (property)
    {
        case RAL_FORMAT_PROPERTY_FORMAT_TYPE:
        {
            *(ral_format_type*) out_result_ptr = format_data[format].format_type;

            break;
        }

        case RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS:
        {
            *(bool*) out_result_ptr = format_data[format].has_depth_data;

            break;
        }

        case RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS:
        {
            *(bool*) out_result_ptr = format_data[format].has_stencil_data;

            break;
        }

        case RAL_FORMAT_PROPERTY_IS_COMPRESSED:
        {
            *(bool*) out_result_ptr = format_data[format].is_compressed;

            break;
        }

        case RAL_FORMAT_PROPERTY_N_COMPONENTS:
        {
            *(uint32_t*) out_result_ptr = format_data[format].n_components;

            break;
        }

        case RAL_FORMAT_PROPERTY_NAME:
        {
            *(const char**) out_result_ptr = format_data[format].name;

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
PUBLIC EMERALD_API void ral_utils_get_ral_program_variable_type_property(ral_program_variable_type          variable_type,
                                                                         ral_program_variable_type_property property,
                                                                         void**                             out_result_ptr)
{
    static const struct
    {
        const char*                     name;
        ral_program_variable_type_class type_class;
    } results[] =
    {
        /* name                                                               | type_class */
        {"RAL_PROGRAM_VARIABLE_TYPE_BOOL",                                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {"RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC2",                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC3",                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC4",                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT",                                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D_ARRAY",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_ARRAY",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE",                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE_ARRAY",                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_RECT",                             RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_3D",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_BUFFER",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE_MAP_ARRAY",                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT",                                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D_ARRAY",                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_ARRAY",                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE",                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE_ARRAY",            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_RECT",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_3D",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_BUFFER",                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE_MAP_ARRAY",                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D_ARRAY",                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_ARRAY",                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE",                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY",          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_RECT",                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_3D",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_BUFFER",                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_CUBE",                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_VEC2",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_VEC3",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_INT_VEC4",                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY",                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY_SHADOW",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_SHADOW",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY",                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY_SHADOW",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE",                    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY",              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT",                           RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT_SHADOW",                    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_SHADOW",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_3D",                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER",                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY",                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY_SHADOW",                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_SHADOW",                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT",                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D",                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D_ARRAY",               RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D",                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_ARRAY",               RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE",         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY",   RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_RECT",                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_3D",                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_BUFFER",                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY",         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D_ARRAY",             RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_ARRAY",             RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE",       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY", RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_RECT",              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_3D",                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_BUFFER",               RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE",                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE_ARRAY",           RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC2",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC3",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC4",                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {"RAL_PROGRAM_VARIABLE_TYPE_VOID",                                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_UNKNOWN},
    };

    switch (property)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_CLASS:
        {
            *(ral_program_variable_type_class*) out_result_ptr = results[variable_type].type_class;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_variable_type_property value.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_utils_get_texture_type_property(ral_texture_type          texture_type,
                                                            ral_texture_type_property property,
                                                            void*                     out_result_ptr)
{
    static const struct
    {
        int n_dimensions;
    } results[] =
    {
        /* RAL_TEXTURE_TYPE_1D */
        { 1 },

        /* RAL_TEXTURE_TYPE_1D_ARRAY */
        { 1 },

        /* RAL_TEXTURE_TYPE_2D */
        { 2 },

        /* RAL_TEXTURE_TYPE_2D_ARRAY */
        { 2 },

        /* RAL_TEXTURE_TYPE_3D */
        { 3 },

        /* RAL_TEXTURE_TYPE_CUBE_MAP */
        { 2 },

        /* RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY */
        { 2 },

        /* RAL_TEXTURE_TYPE_MULTISAMPLE_2D */
        { 2 },

        /* RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY */
        { 2 },
    };

    static_assert(sizeof(results) / sizeof(results[0]) == RAL_TEXTURE_TYPE_COUNT,
                  "Predefined result array needs to be updated.");

    bool result = true;

    switch (property)
    {
        case RAL_TEXTURE_TYPE_PROPERTY_N_DIMENSIONS:
        {
            *(uint32_t*) out_result_ptr = results[texture_type].n_dimensions;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_type_property value.");

            result = false;
        }
    }

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
