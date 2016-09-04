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
        ral_format_type           format_type;
        bool                      has_color_data;
        bool                      has_depth_data;
        bool                      has_stencil_data;
        system_hashed_ansi_string image_layout_qualifier;
        bool                      is_compressed;
        uint32_t                  n_components;
        const char*               name;
    } format_data[] =
    {
        /* format_type           | has_color | has_depth | has_stencil | image_layout_qualifier                            | is_compressed | n_components | name */
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           1,             "RAL_FORMAT_COMPRESSED_R11_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           1,             "RAL_FORMAT_COMPRESSED_RED_RGTC1_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           2,             "RAL_FORMAT_COMPRESSED_RG11_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           2,             "RAL_FORMAT_COMPRESSED_RG_RGTC2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        nullptr,                                            true,           3,             "RAL_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT"},
        {  RAL_FORMAT_TYPE_UFLOAT, true,       false,      false,        nullptr,                                            true,           3,             "RAL_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           3,             "RAL_FORMAT_COMPRESSED_RGB8_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_RGBA_BPTC_UNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            true,           1,             "RAL_FORMAT_COMPRESSED_R11_EAC_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            true,           1,             "RAL_FORMAT_COMPRESSED_RED_RGTC1_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            true,           2,             "RAL_FORMAT_COMPRESSED_RG11_EAC_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            true,           2,             "RAL_FORMAT_COMPRESSED_RG_RGTC2_SNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           3,             "RAL_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            true,           4,             "RAL_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        nullptr,                                            false,          1,             "RAL_FORMAT_DEPTH16_SNORM"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        nullptr,                                            false,          1,             "RAL_FORMAT_DEPTH24_SNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, false,      true,       false,        nullptr,                                            false,          1,             "RAL_FORMAT_DEPTH32_FLOAT"},
        {  RAL_FORMAT_TYPE_SNORM,  false,      true,       false,        nullptr,                                            false,          1,             "RAL_FORMAT_DEPTH32_SNORM"},
        {  RAL_FORMAT_TYPE_DS,     false,      true,       true,         nullptr,                                            false,          2,             "RAL_FORMAT_DEPTH24_STENCIL8"},
        {  RAL_FORMAT_TYPE_DS,     false,      true,       true,         nullptr,                                            false,          2,             "RAL_FORMAT_DEPTH32F_STENCIL8"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("r11f_g11f_b10f"), false,          3,             "RAL_FORMAT_R11FG11FB10F"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("r16f"),           false,          1,             "RAL_FORMAT_R16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("r16i"),           false,          1,             "RAL_FORMAT_R16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        system_hashed_ansi_string_create("r16_snorm"),      false,          1,             "RAL_FORMAT_R16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("r16ui"),          false,          1,             "RAL_FORMAT_R16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          1,             "RAL_FORMAT_R16_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_R3G3B2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("r32f"),           false,          1,             "RAL_FORMAT_R32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("r32i"),           false,          1,             "RAL_FORMAT_R32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("r32ui"),          false,          1,             "RAL_FORMAT_R32_UINT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("r8i"),            false,          1,             "RAL_FORMAT_R8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        system_hashed_ansi_string_create("r8_snorm"),       false,          1,             "RAL_FORMAT_R8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("r8ui"),           false,          1,             "RAL_FORMAT_R8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          1,             "RAL_FORMAT_R8_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("rg16f"),          false,          2,             "RAL_FORMAT_RG16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rg16i"),          false,          2,             "RAL_FORMAT_RG16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        system_hashed_ansi_string_create("rg16_snorm"),     false,          2,             "RAL_FORMAT_RG16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rg16ui"),         false,          2,             "RAL_FORMAT_RG16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          2,             "RAL_FORMAT_RG16_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("rg32f"),          false,          2,             "RAL_FORMAT_RG32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rg32i"),          false,          2,             "RAL_FORMAT_RG32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rg32ui"),         false,          2,             "RAL_FORMAT_RG32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          2,             "RAL_FORMAT_RG8_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rg8i"),           false,          2,             "RAL_FORMAT_RG8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            false,          2,             "RAL_FORMAT_RG8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rg8ui"),          false,          2,             "RAL_FORMAT_RG8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB10_UNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rgb10_a2ui"),     false,          4,             "RAL_FORMAT_RGB10A2_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGB10A2_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB12_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB16_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB4_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB5_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGB5A1_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB8_UNORM" },
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_RGB9E5_FLOAT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA12_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("rgba16f"),        false,          4,             "RAL_FORMAT_RGBA16_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rgba16i"),        false,          4,             "RAL_FORMAT_RGBA16_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        system_hashed_ansi_string_create("rgba16_snorm"),   false,          4,             "RAL_FORMAT_RGBA16_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA16_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA16_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA2_UNORM"},
        {  RAL_FORMAT_TYPE_SFLOAT, true,       false,      false,        system_hashed_ansi_string_create("rgba32f"),        false,          4,             "RAL_FORMAT_RGBA32_FLOAT"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rgba32i"),        false,          4,             "RAL_FORMAT_RGBA32_SINT"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rgba32ui"),       false,          4,             "RAL_FORMAT_RGBA32_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA4_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_RGBA8_UNORM"},
        {  RAL_FORMAT_TYPE_SINT,   true,       false,      false,        system_hashed_ansi_string_create("rgba8i"),         false,          4,             "RAL_FORMAT_RGBA8_SINT"},
        {  RAL_FORMAT_TYPE_SNORM,  true,       false,      false,        system_hashed_ansi_string_create("rgba8_snorm"),    false,          4,             "RAL_FORMAT_RGBA8_SNORM"},
        {  RAL_FORMAT_TYPE_UINT,   true,       false,      false,        system_hashed_ansi_string_create("rgba8ui"),        false,          4,             "RAL_FORMAT_RGBA8_UINT"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          3,             "RAL_FORMAT_SRGB8_UNORM"},
        {  RAL_FORMAT_TYPE_UNORM,  true,       false,      false,        nullptr,                                            false,          4,             "RAL_FORMAT_SRGBA8_UNORM"},
    };

    static_assert(sizeof(format_data) / sizeof(format_data[0]) == RAL_FORMAT_COUNT,
                  "Predefined data out of sync!");

    switch (property)
    {
        case RAL_FORMAT_PROPERTY_FORMAT_TYPE:
        {
            *reinterpret_cast<ral_format_type*>(out_result_ptr) = format_data[format].format_type;

            break;
        }

        case RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = format_data[format].has_color_data;

            break;
        }

        case RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = format_data[format].has_depth_data;

            break;
        }

        case RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = format_data[format].has_stencil_data;

            break;
        }

        case RAL_FORMAT_PROPERTY_IS_COMPRESSED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = format_data[format].is_compressed;

            break;
        }

        case RAL_FORMAT_PROPERTY_N_COMPONENTS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = format_data[format].n_components;

            break;
        }

        case RAL_FORMAT_PROPERTY_NAME:
        {
            *reinterpret_cast<const char**>(out_result_ptr) = format_data[format].name;

            break;
        }
        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_texture_format_property value");

            result = false;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_utils_get_ral_program_variable_type_class_property(ral_program_variable_type_class          variable_type_class,
                                                                               ral_program_variable_type_class_property property,
                                                                               void**                                   out_result_ptr)
{
    static const struct
    {
        system_hashed_ansi_string name;
    } results[] =
    {
        system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE"),
        system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX"),
        system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER"),
        system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR"),
        system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR"),
    };

    switch (property)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_CLASS_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[variable_type_class].name;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_variable_type_class_property value requested.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_utils_get_ral_program_variable_type_property(ral_program_variable_type          variable_type,
                                                                         ral_program_variable_type_property property,
                                                                         void**                             out_result_ptr)
{
    static const struct
    {
        system_hashed_ansi_string       name;
        ral_program_variable_type_class type_class;
    } results[] =
    {
        /* name                                                                                                 | type_class */
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_BOOL"),                                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC2"),                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC3"),                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC4"),                                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT"),                                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_MATRIX},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D_ARRAY"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_ARRAY"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE"),                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE_ARRAY"),                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_RECT"),                             RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_3D"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_BUFFER"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE_MAP_ARRAY"),                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT"),                                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D_ARRAY"),                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_ARRAY"),                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE"),                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE_ARRAY"),            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_RECT"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_3D"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_BUFFER"),                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE_MAP_ARRAY"),                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D_ARRAY"),                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_ARRAY"),                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE"),                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY"),          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_RECT"),                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_3D"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_BUFFER"),                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_CUBE"),                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_VEC2"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_VEC3"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_INT_VEC4"),                                  RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY"),                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY_SHADOW"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_SHADOW"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY"),                          RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY_SHADOW"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE"),                    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY"),              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT"),                           RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT_SHADOW"),                    RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_SHADOW"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_3D"),                                RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER"),                            RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY"),                        RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY_SHADOW"),                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_SHADOW"),                       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT"),                              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SCALAR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D"),                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D_ARRAY"),               RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D"),                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_ARRAY"),               RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE"),         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY"),   RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_RECT"),                RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_3D"),                     RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_BUFFER"),                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY"),         RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D_ARRAY"),             RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_ARRAY"),             RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE"),       RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY"), RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_RECT"),              RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_3D"),                   RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_BUFFER"),               RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE"),                 RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE_ARRAY"),           RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC2"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC3"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC4"),                         RAL_PROGRAM_VARIABLE_TYPE_CLASS_VECTOR},
        {system_hashed_ansi_string_create("RAL_PROGRAM_VARIABLE_TYPE_VOID"),                                      RAL_PROGRAM_VARIABLE_TYPE_CLASS_UNKNOWN},
    };

    switch (property)
    {
        case RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_CLASS:
        {
            *reinterpret_cast<ral_program_variable_type_class*>(out_result_ptr) = results[variable_type].type_class;

            break;
        }

        case RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[variable_type].name;

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
        system_hashed_ansi_string image_layout_float_qualifier;
        system_hashed_ansi_string image_layout_sint_qualifier;
        system_hashed_ansi_string image_layout_uint_qualifier;
        system_hashed_ansi_string sampler_float;
        system_hashed_ansi_string sampler_sint;
        system_hashed_ansi_string sampler_uint;
        int                       n_dimensions;
    } results[] =
    {
        /* RAL_TEXTURE_TYPE_1D */
        {
            system_hashed_ansi_string_create("image1D"),
            system_hashed_ansi_string_create("iimage1D"),
            system_hashed_ansi_string_create("uimage1D"),
            system_hashed_ansi_string_create("sampler1D"),
            system_hashed_ansi_string_create("isampler1D"),
            system_hashed_ansi_string_create("usampler1D"),
            1
        },

        /* RAL_TEXTURE_TYPE_1D_ARRAY */
        {
            system_hashed_ansi_string_create("image1DArray"),
            system_hashed_ansi_string_create("iimage1DArray"),
            system_hashed_ansi_string_create("uimage1DArray"),
            system_hashed_ansi_string_create("sampler1DArray"),
            system_hashed_ansi_string_create("isampler1DArray"),
            system_hashed_ansi_string_create("usampler1DArray"),
            1},

        /* RAL_TEXTURE_TYPE_2D */
        {
            system_hashed_ansi_string_create("image2D"),
            system_hashed_ansi_string_create("iimage2D"),
            system_hashed_ansi_string_create("uimage2D"),
            system_hashed_ansi_string_create("sampler2D"),
            system_hashed_ansi_string_create("isampler2D"),
            system_hashed_ansi_string_create("usampler2D"),
            2
        },

        /* RAL_TEXTURE_TYPE_2D_ARRAY */
        {
            system_hashed_ansi_string_create("image2DArray"),
            system_hashed_ansi_string_create("iimage2DArray"),
            system_hashed_ansi_string_create("uimage2DArray"),
            system_hashed_ansi_string_create("sampler2DArray"),
            system_hashed_ansi_string_create("isampler2DArray"),
            system_hashed_ansi_string_create("usampler2DArray"),
            2
        },

        /* RAL_TEXTURE_TYPE_3D */
        {
            system_hashed_ansi_string_create("image3D"),
            system_hashed_ansi_string_create("iimage3D"),
            system_hashed_ansi_string_create("uimage3D"),
            system_hashed_ansi_string_create("sampler3D"),
            system_hashed_ansi_string_create("isampler3D"),
            system_hashed_ansi_string_create("usampler3D"),
            3
        },

        /* RAL_TEXTURE_TYPE_CUBE_MAP */
        {
            system_hashed_ansi_string_create("imageCube"),
            system_hashed_ansi_string_create("iimageCube"),
            system_hashed_ansi_string_create("uimageCube"),
            system_hashed_ansi_string_create("samplerCube"),
            system_hashed_ansi_string_create("isamplerCube"),
            system_hashed_ansi_string_create("usamplerCube"),
            2
        },

        /* RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY */
        {
            system_hashed_ansi_string_create("imageCubeArray"),
            system_hashed_ansi_string_create("iimageCubeArray"),
            system_hashed_ansi_string_create("uimageCubeArray"),
            system_hashed_ansi_string_create("samplerCubeArray"),
            system_hashed_ansi_string_create("isamplerCubeArray"),
            system_hashed_ansi_string_create("usamplerCubeArray"),
            2
        },

        /* RAL_TEXTURE_TYPE_MULTISAMPLE_2D */
        {
            nullptr, /* not available in GLSL 450 */
            nullptr, /* not available in GLSL 450 */
            nullptr, /* not available in GLSL 450 */
            system_hashed_ansi_string_create("sampler2DMS"),
            system_hashed_ansi_string_create("isampler2DMS"),
            system_hashed_ansi_string_create("usampler2DMS"),
            2
        },

        /* RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY */
        {
            nullptr, /* not available in GLSL 450 */
            nullptr, /* not available in GLSL 450 */
            nullptr, /* not available in GLSL 450 */
            system_hashed_ansi_string_create("sampler2DMSArray"),
            system_hashed_ansi_string_create("isampler2DMSArray"),
            system_hashed_ansi_string_create("usampler2DMSArray"),
            2
        },
    };

    static_assert(sizeof(results) / sizeof(results[0]) == RAL_TEXTURE_TYPE_COUNT,
                  "Predefined result array needs to be updated.");

    bool result = true;

    switch (property)
    {
        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_IMAGE_LAYOUT_QUALIFIER:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].image_layout_float_qualifier != nullptr,
                              "Image layout unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].image_layout_float_qualifier;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_SINT_IMAGE_LAYOUT_QUALIFIER:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].image_layout_sint_qualifier != nullptr,
                              "Image layout unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].image_layout_sint_qualifier;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_UINT_IMAGE_LAYOUT_QUALIFIER:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].image_layout_uint_qualifier != nullptr,
                              "Image layout unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].image_layout_uint_qualifier;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_FLOAT_SAMPLER_TYPE:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].sampler_float != nullptr,
                              "Sampler type unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].sampler_float;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_SINT_SAMPLER_TYPE:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].sampler_sint != nullptr,
                              "Sampler type unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].sampler_sint;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_GLSL_UINT_SAMPLER_TYPE:
        {
            ASSERT_DEBUG_SYNC(results[texture_type].sampler_uint != nullptr,
                              "Sampler type unavailable in glsl 450");

            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = results[texture_type].sampler_uint;

            break;
        }

        case RAL_TEXTURE_TYPE_PROPERTY_N_DIMENSIONS:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = results[texture_type].n_dimensions;

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
