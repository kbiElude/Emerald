/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "raGL/raGL_utils.h"
#include "system/system_log.h"


/** Please see header for sepcification */
PUBLIC ogl_texture_data_format raGL_utils_get_ogl_data_format_for_ral_format(ral_format in_format)
{
    ogl_texture_data_format result = OGL_TEXTURE_DATA_FORMAT_UNDEFINED;

    switch (in_format)
    {

        case RAL_FORMAT_COMPRESSED_BC4_SNORM:
        case RAL_FORMAT_COMPRESSED_BC4_UNORM:
        case RAL_FORMAT_COMPRESSED_R11_EAC_SNORM:
        case RAL_FORMAT_COMPRESSED_R11_EAC_UNORM:
        case RAL_FORMAT_DEPTH16_SNORM:
        case RAL_FORMAT_DEPTH24_SNORM:
        case RAL_FORMAT_DEPTH32_FLOAT:
        case RAL_FORMAT_DEPTH32_SNORM:
        case RAL_FORMAT_R16_FLOAT:
        case RAL_FORMAT_R16_SINT:
        case RAL_FORMAT_R16_SNORM:
        case RAL_FORMAT_R16_UINT:
        case RAL_FORMAT_R16_UNORM:
        case RAL_FORMAT_R32_FLOAT:
        case RAL_FORMAT_R32_SINT:
        case RAL_FORMAT_R32_UINT:
        case RAL_FORMAT_R8_SINT:
        case RAL_FORMAT_R8_SNORM:
        case RAL_FORMAT_R8_UINT:
        case RAL_FORMAT_R8_UNORM:
        {
            result = OGL_TEXTURE_DATA_FORMAT_RED;

            break;
        }

        case RAL_FORMAT_COMPRESSED_BC5_SNORM:
        case RAL_FORMAT_COMPRESSED_BC5_UNORM:
        case RAL_FORMAT_COMPRESSED_RG11_EAC_SNORM:
        case RAL_FORMAT_COMPRESSED_RG11_EAC_UNORM:
        case RAL_FORMAT_RG16_FLOAT:
        case RAL_FORMAT_RG16_SINT:
        case RAL_FORMAT_RG16_SNORM:
        case RAL_FORMAT_RG16_UINT:
        case RAL_FORMAT_RG16_UNORM:
        case RAL_FORMAT_RG32_FLOAT:
        case RAL_FORMAT_RG32_SINT:
        case RAL_FORMAT_RG32_UINT:
        case RAL_FORMAT_RG8_SINT:
        case RAL_FORMAT_RG8_SNORM:
        case RAL_FORMAT_RG8_UINT:
        case RAL_FORMAT_RG8_UNORM:
        {
            result = OGL_TEXTURE_DATA_FORMAT_RG;

            break;
        }

        case RAL_FORMAT_COMPRESSED_BC6_SFLOAT:
        case RAL_FORMAT_COMPRESSED_BC6_UFLOAT:
        case RAL_FORMAT_COMPRESSED_RGB8_ETC2_UNORM:
        case RAL_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM:
        case RAL_FORMAT_R11FG11FB10F:
        case RAL_FORMAT_R3G3B2_UNORM:
        case RAL_FORMAT_RGB10_UNORM:
        case RAL_FORMAT_RGB12_UNORM:
        case RAL_FORMAT_RGB16_FLOAT:
        case RAL_FORMAT_RGB16_SINT:
        case RAL_FORMAT_RGB16_SNORM:
        case RAL_FORMAT_RGB16_UINT:
        case RAL_FORMAT_RGB16_UNORM:
        case RAL_FORMAT_RGB32_FLOAT:
        case RAL_FORMAT_RGB32_SINT:
        case RAL_FORMAT_RGB32_UINT:
        case RAL_FORMAT_RGB4_UNORM:
        case RAL_FORMAT_RGB5A1_UNORM:
        case RAL_FORMAT_RGB5_UNORM:
        case RAL_FORMAT_RGB8_SINT:
        case RAL_FORMAT_RGB8_SNORM:
        case RAL_FORMAT_RGB8_UINT:
        case RAL_FORMAT_RGB8_UNORM:
        case RAL_FORMAT_SRGB8_UNORM:
        {
            result = OGL_TEXTURE_DATA_FORMAT_RGB;

            break;
        }

        case RAL_FORMAT_COMPRESSED_BC7_SRGB_UNORM:
        case RAL_FORMAT_COMPRESSED_BC7_UNORM:
        case RAL_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM:
        case RAL_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM:
        case RAL_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM:
        case RAL_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM:
        case RAL_FORMAT_RGB10A2_UINT:
        case RAL_FORMAT_RGB10A2_UNORM:
        case RAL_FORMAT_RGBA12_UNORM:
        case RAL_FORMAT_RGBA16_FLOAT:
        case RAL_FORMAT_RGBA16_SINT:
        case RAL_FORMAT_RGBA16_SNORM:
        case RAL_FORMAT_RGBA16_UINT:
        case RAL_FORMAT_RGBA16_UNORM:
        case RAL_FORMAT_RGBA2_UNORM:
        case RAL_FORMAT_RGBA32_FLOAT:
        case RAL_FORMAT_RGBA32_SINT:
        case RAL_FORMAT_RGBA32_UINT:
        case RAL_FORMAT_RGBA4_UNORM:
        case RAL_FORMAT_RGBA8_SINT:
        case RAL_FORMAT_RGBA8_SNORM:
        case RAL_FORMAT_RGBA8_UINT:
        case RAL_FORMAT_RGBA8_UNORM:
        {
            result = OGL_TEXTURE_DATA_FORMAT_RGBA;

            break;
        }

        case RAL_FORMAT_RGB9E5_FLOAT:
        {
            result = OGL_TEXTURE_DATA_FORMAT_RGBE;

            break;
        }

        case RAL_FORMAT_DEPTH24_STENCIL8:
        case RAL_FORMAT_DEPTH32F_STENCIL8:
        {
            result = OGL_TEXTURE_DATA_FORMAT_DEPTH_STENCIL;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_format specified.");
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_blend_factor(ral_blend_factor blend_factor)
{
    static const GLenum results[] =
    {
        GL_CONSTANT_ALPHA,
        GL_CONSTANT_COLOR,
        GL_DST_ALPHA,
        GL_DST_COLOR,
        GL_ONE,
        GL_ONE_MINUS_CONSTANT_ALPHA,
        GL_ONE_MINUS_CONSTANT_COLOR,
        GL_ONE_MINUS_DST_ALPHA,
        GL_ONE_MINUS_DST_COLOR,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE_MINUS_SRC_COLOR,
        GL_SRC_ALPHA,
        GL_SRC_ALPHA_SATURATE,
        GL_SRC_COLOR,
        GL_ZERO,
    };

    ASSERT_DEBUG_SYNC(blend_factor < sizeof(results) / sizeof(results[0]),
                      "Invalid ral_blend_factor value specified.");

    return results[blend_factor];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_blend_op(ral_blend_op blend_op)
{
    static const GLenum results[] =
    {
        GL_FUNC_ADD,
        GL_MAX,
        GL_MIN,
        GL_FUNC_SUBTRACT,
        GL_FUNC_REVERSE_SUBTRACT,
    };

    ASSERT_DEBUG_SYNC(blend_op < sizeof(results) / sizeof(results[0]),
                      "Invalid ral_blend_op value specified.");

    return results[blend_op];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_compare_op(ral_compare_op compare_op)
{
    static const GLenum results[] =
    {
        GL_ALWAYS,
        GL_EQUAL,
        GL_GEQUAL,
        GL_LEQUAL,
        GL_LESS,
        GL_NOTEQUAL,
        GL_NEVER,
        GL_GREATER
    };

    return results[compare_op];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_index_type(ral_index_type in_index_type)
{
    static const GLenum results[] =
    {
        GL_UNSIGNED_BYTE,
        GL_UNSIGNED_SHORT,
        GL_UNSIGNED_INT
    };

    ASSERT_DEBUG_SYNC(in_index_type < sizeof(results) / sizeof(results[0]),
                      "Invalid index type requested");

    return results[in_index_type];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_polygon_mode(ral_polygon_mode polygon_mode)
{
    GLenum result;

    switch (polygon_mode)
    {
        case RAL_POLYGON_MODE_FILL:   result = GL_FILL;  break;
        case RAL_POLYGON_MODE_LINES:  result = GL_LINE;  break;
        case RAL_POLYGON_MODE_POINTS: result = GL_POINT; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_polygon_mode value.");
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_primitive_type(ral_primitive_type in_primitive_type)
{
    static const GLenum result_array[] =
    {
        GL_LINE_LOOP,
        GL_LINE_STRIP,
        GL_LINE_STRIP_ADJACENCY,
        GL_LINES,
        GL_LINES_ADJACENCY,
        GL_POINTS,
        GL_TRIANGLE_FAN,
        GL_TRIANGLE_STRIP,
        GL_TRIANGLE_STRIP_ADJACENCY,
        GL_TRIANGLES,
        GL_TRIANGLES_ADJACENCY
    };

    static_assert(sizeof(result_array) / sizeof(result_array[0]) == RAL_PRIMITIVE_TYPE_COUNT, "");

    ASSERT_DEBUG_SYNC(in_primitive_type < RAL_PRIMITIVE_TYPE_COUNT,
                      "RAL primitive type not supported for OpenGL back-end");

    return result_array[in_primitive_type];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_program_variable_type(ral_program_variable_type in_type)
{
    static const GLenum result_array[] =
    {
        GL_BOOL,
        GL_BOOL_VEC2,
        GL_BOOL_VEC3,
        GL_BOOL_VEC4,
        GL_FLOAT,
        GL_FLOAT_MAT2,
        GL_FLOAT_MAT3,
        GL_FLOAT_MAT4,
        GL_FLOAT_MAT2x3,
        GL_FLOAT_MAT2x4,
        GL_FLOAT_MAT3x2,
        GL_FLOAT_MAT3x4,
        GL_FLOAT_MAT4x2,
        GL_FLOAT_MAT4x3,
        GL_FLOAT_VEC2,
        GL_FLOAT_VEC3,
        GL_FLOAT_VEC4,
        GL_IMAGE_1D,
        GL_IMAGE_1D_ARRAY,
        GL_IMAGE_2D,
        GL_IMAGE_2D_ARRAY,
        GL_IMAGE_2D_MULTISAMPLE,
        GL_IMAGE_2D_MULTISAMPLE_ARRAY,
        GL_IMAGE_2D_RECT,
        GL_IMAGE_3D,
        GL_IMAGE_BUFFER,
        GL_IMAGE_CUBE,
        GL_IMAGE_CUBE_MAP_ARRAY,
        GL_INT,
        GL_INT_IMAGE_1D,
        GL_INT_IMAGE_1D_ARRAY,
        GL_INT_IMAGE_2D,
        GL_INT_IMAGE_2D_ARRAY,
        GL_INT_IMAGE_2D_MULTISAMPLE,
        GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY,
        GL_INT_IMAGE_2D_RECT,
        GL_INT_IMAGE_3D,
        GL_INT_IMAGE_BUFFER,
        GL_INT_IMAGE_CUBE,
        GL_INT_IMAGE_CUBE_MAP_ARRAY,
        GL_INT_SAMPLER_1D,
        GL_INT_SAMPLER_1D_ARRAY,
        GL_INT_SAMPLER_2D,
        GL_INT_SAMPLER_2D_ARRAY,
        GL_INT_SAMPLER_2D_MULTISAMPLE,
        GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
        GL_INT_SAMPLER_2D_RECT,
        GL_INT_SAMPLER_3D,
        GL_INT_SAMPLER_BUFFER,
        GL_INT_SAMPLER_CUBE,
        GL_INT_VEC2,
        GL_INT_VEC3,
        GL_INT_VEC4,
        GL_SAMPLER_1D,
        GL_SAMPLER_1D_ARRAY,
        GL_SAMPLER_1D_ARRAY_SHADOW,
        GL_SAMPLER_1D_SHADOW,
        GL_SAMPLER_2D,
        GL_SAMPLER_2D_ARRAY,
        GL_SAMPLER_2D_ARRAY_SHADOW,
        GL_SAMPLER_2D_MULTISAMPLE,
        GL_SAMPLER_2D_MULTISAMPLE_ARRAY,
        GL_SAMPLER_2D_RECT,
        GL_SAMPLER_2D_RECT_SHADOW,
        GL_SAMPLER_2D_SHADOW,
        GL_SAMPLER_3D,
        GL_SAMPLER_BUFFER,
        GL_SAMPLER_CUBE,
        GL_SAMPLER_CUBE_MAP_ARRAY,
        GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW,
        GL_SAMPLER_CUBE_SHADOW,
        GL_UNSIGNED_INT,
        GL_UNSIGNED_INT_IMAGE_1D,
        GL_UNSIGNED_INT_IMAGE_1D_ARRAY,
        GL_UNSIGNED_INT_IMAGE_2D,
        GL_UNSIGNED_INT_IMAGE_2D_ARRAY,
        GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE,
        GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY,
        GL_UNSIGNED_INT_IMAGE_2D_RECT,
        GL_UNSIGNED_INT_IMAGE_3D,
        GL_UNSIGNED_INT_IMAGE_BUFFER,
        GL_UNSIGNED_INT_IMAGE_CUBE,
        GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_1D,
        GL_UNSIGNED_INT_SAMPLER_1D_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_2D,
        GL_UNSIGNED_INT_SAMPLER_2D_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE,
        GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY,
        GL_UNSIGNED_INT_SAMPLER_2D_RECT,
        GL_UNSIGNED_INT_SAMPLER_3D,
        GL_UNSIGNED_INT_SAMPLER_BUFFER,
        GL_UNSIGNED_INT_SAMPLER_CUBE,
        GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY,
        GL_UNSIGNED_INT_VEC2,
        GL_UNSIGNED_INT_VEC3,
        GL_UNSIGNED_INT_VEC4,
        0, /* GL_VOID */
    };
    static const uint32_t n_result_array_entries = sizeof(result_array) / sizeof(result_array[0]);

    static_assert(n_result_array_entries == RAL_PROGRAM_VARIABLE_TYPE_COUNT, "");

    ASSERT_DEBUG_SYNC( static_cast<uint32_t>(in_type) < n_result_array_entries,
                      "Invalid ral_uniform_type value.");

    return result_array[in_type];
};

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_stencil_op(ral_stencil_op stencil_op)
{
    static const GLenum results[] =
    {
        GL_DECR,
        GL_DECR_WRAP,
        GL_INCR,
        GL_INCR_WRAP,
        GL_KEEP,
        GL_REPLACE,
        GL_ZERO,
    };

    return results[stencil_op];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_aspect(ral_texture_aspect texture_view_aspect)
{
    GLenum result = GL_NONE;

    ASSERT_DEBUG_SYNC(texture_view_aspect == RAL_TEXTURE_ASPECT_DEPTH_BIT ||
                      texture_view_aspect == RAL_TEXTURE_ASPECT_STENCIL_BIT,
                      "Unsupported ral_texture_aspect value specified.");

    switch (texture_view_aspect)
    { 
        case RAL_TEXTURE_ASPECT_DEPTH_BIT:   result = GL_DEPTH_COMPONENT; break;
        case RAL_TEXTURE_ASPECT_STENCIL_BIT: result = GL_STENCIL_INDEX;   break;
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_data_type(ral_texture_data_type in_data_type)
{
    GLenum result = GL_NONE;

    switch (in_data_type)
    {
        case RAL_TEXTURE_DATA_TYPE_FLOAT:  result = GL_FLOAT;          break;
        case RAL_TEXTURE_DATA_TYPE_SBYTE:  result = GL_BYTE;           break;
        case RAL_TEXTURE_DATA_TYPE_SINT:   result = GL_INT;            break;
        case RAL_TEXTURE_DATA_TYPE_SSHORT: result = GL_SHORT;          break;
        case RAL_TEXTURE_DATA_TYPE_UBYTE:  result = GL_UNSIGNED_BYTE;  break;
        case RAL_TEXTURE_DATA_TYPE_UINT:   result = GL_UNSIGNED_INT;   break;
        case RAL_TEXTURE_DATA_TYPE_USHORT: result = GL_UNSIGNED_SHORT; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized input ral_texture_data_type value.");
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_filter_mag(ral_texture_filter in_texture_filter)
{
    GLenum result = GL_NONE;

    switch (in_texture_filter)
    {
        case RAL_TEXTURE_FILTER_LINEAR:
        {
            result = GL_LINEAR;

            break;
        }

        case RAL_TEXTURE_FILTER_NEAREST:
        {
            result = GL_NEAREST;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture filter requested.");
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_filter_min(ral_texture_filter      in_texture_filter,
                                                                 ral_texture_mipmap_mode in_mipmap_mode)
{
    GLenum result = GL_NONE;

    switch (in_texture_filter)
    {
        case RAL_TEXTURE_FILTER_LINEAR:
        {
            switch (in_mipmap_mode)
            {
                case RAL_TEXTURE_MIPMAP_MODE_LINEAR:  result = GL_LINEAR_MIPMAP_LINEAR;  break;
                case RAL_TEXTURE_MIPMAP_MODE_NEAREST: result = GL_LINEAR_MIPMAP_NEAREST; break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Mipmap mode not supported for OpenGL back-end");
                }
            }

            break;
        }

        case RAL_TEXTURE_FILTER_NEAREST:
        {
            switch (in_mipmap_mode)
            {
                case RAL_TEXTURE_MIPMAP_MODE_LINEAR:  result = GL_NEAREST_MIPMAP_LINEAR;  break;
                case RAL_TEXTURE_MIPMAP_MODE_NEAREST: result = GL_NEAREST_MIPMAP_NEAREST; break;

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Mipmap mode not supported for OpenGL back-end");
                }
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture filter requested.");
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_shader_type(ral_shader_type in_shader_type)
{
    static const GLenum result_array[] =
    {
        GL_COMPUTE_SHADER,
        GL_FRAGMENT_SHADER,
        GL_GEOMETRY_SHADER,
        GL_TESS_CONTROL_SHADER,
        GL_TESS_EVALUATION_SHADER,
        GL_VERTEX_SHADER
    };

    static_assert(sizeof(result_array) / sizeof(result_array[0]) == RAL_SHADER_TYPE_COUNT, "");

    ASSERT_DEBUG_SYNC(in_shader_type < RAL_SHADER_TYPE_COUNT,
                      "RAL shader stage unsupported for OpenGL back-end");

    return result_array[in_shader_type];
}

/** Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_format(ral_format in_format)
{
    static const GLenum result_array[] =
    {
        GL_COMPRESSED_R11_EAC,
        GL_COMPRESSED_RED_RGTC1,
        GL_COMPRESSED_RG11_EAC,
        GL_COMPRESSED_RG_RGTC2,
        GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB,
        GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB,
        GL_COMPRESSED_RGB8_ETC2,
        GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
        GL_COMPRESSED_RGBA8_ETC2_EAC,
        GL_COMPRESSED_RGBA_BPTC_UNORM_ARB,
        GL_COMPRESSED_SIGNED_R11_EAC,
        GL_COMPRESSED_SIGNED_RED_RGTC1,
        GL_COMPRESSED_SIGNED_RG11_EAC,
        GL_COMPRESSED_SIGNED_RG_RGTC2,
        GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
        GL_COMPRESSED_SRGB8_ETC2,
        GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
        GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB,
        GL_DEPTH_COMPONENT16,
        GL_DEPTH_COMPONENT24,
        GL_DEPTH_COMPONENT32F,
        GL_DEPTH_COMPONENT32,
        GL_DEPTH24_STENCIL8,
        GL_DEPTH32F_STENCIL8,
        GL_R11F_G11F_B10F,
        GL_R16F,
        GL_R16I,
        GL_R16_SNORM,
        GL_R16UI,
        GL_R16,
        GL_R3_G3_B2,
        GL_R32F,
        GL_R32I,
        GL_R32UI,
        GL_R8I,
        GL_R8_SNORM,
        GL_R8UI,
        GL_R8,
        GL_RG16F,
        GL_RG16I,
        GL_RG16_SNORM,
        GL_RG16UI,
        GL_RG16,
        GL_RG32F,
        GL_RG32I,
        GL_RG32UI,
        GL_RG8,
        GL_RG8I,
        GL_RG8_SNORM,
        GL_RG8UI,
        GL_RGB10,
        GL_RGB10_A2UI,
        GL_RGB10_A2,
        GL_RGB12,
        GL_RGB16F,
        GL_RGB16I,
        GL_RGB16_SNORM,
        GL_RGB16UI,
        GL_RGB16,
        GL_RGB32F,
        GL_RGB32I,
        GL_RGB32UI,
        GL_RGB4,
        GL_RGB5,
        GL_RGB5_A1,
        GL_RGB8I,
        GL_RGB8_SNORM,
        GL_RGB8UI,
        GL_RGB8,
        GL_RGB9_E5,
        GL_RGBA12,
        GL_RGBA16F,
        GL_RGBA16I,
        GL_RGBA16_SNORM,
        GL_RGBA16UI,
        GL_RGBA16,
        GL_RGBA2,
        GL_RGBA32F,
        GL_RGBA32I,
        GL_RGBA32UI,
        GL_RGBA4,
        GL_RGBA8,
        GL_RGBA8I,
        GL_RGBA8_SNORM,
        GL_RGBA8UI,
        GL_SRGB8,
        GL_SRGB8_ALPHA8,
    };

    ASSERT_DEBUG_SYNC(in_format < RAL_FORMAT_COUNT,
                      "RAL format not supported by the GL back-end");

    return result_array[in_format];
}

/* Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_type(ral_texture_type in_texture_type)
{
    static const GLenum result_array[] =
    {
        GL_TEXTURE_1D,
        GL_TEXTURE_1D_ARRAY,
        GL_TEXTURE_2D,
        GL_TEXTURE_2D_ARRAY,
        GL_TEXTURE_3D,
        GL_TEXTURE_BUFFER,
        GL_TEXTURE_CUBE_MAP,
        GL_TEXTURE_CUBE_MAP_ARRAY,
        GL_TEXTURE_RECTANGLE,

        GL_TEXTURE_2D_MULTISAMPLE,
        GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    };

    ASSERT_DEBUG_SYNC(in_texture_type < RAL_TEXTURE_TYPE_COUNT,
                      "RAL texture format not supported by the GL back-end");

    return result_array[in_texture_type];
}

/* Please see header for specification */
PUBLIC GLenum raGL_utils_get_ogl_enum_for_ral_texture_wrap_mode(ral_texture_wrap_mode in_texture_wrap_mode)
{
    static const GLenum result_array[] =
    {
        GL_CLAMP_TO_BORDER,
        GL_CLAMP_TO_EDGE,
        GL_MIRROR_CLAMP_TO_EDGE,
        GL_MIRRORED_REPEAT,
        GL_REPEAT,
    };

    ASSERT_DEBUG_SYNC(in_texture_wrap_mode < RAL_TEXTURE_WRAP_MODE_COUNT,
                      "RAL texture wrap mode not supported by the GL back-end");

    return result_array[in_texture_wrap_mode];
}

/* Please see header for specification */
PUBLIC ral_program_variable_type raGL_utils_get_ral_program_variable_type_for_ogl_enum(GLenum type)
{
    ral_program_variable_type result = RAL_PROGRAM_VARIABLE_TYPE_UNDEFINED;

    switch (type)
    {
        case GL_BOOL:                                      result = RAL_PROGRAM_VARIABLE_TYPE_BOOL;                                      break;
        case GL_BOOL_VEC2:                                 result = RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC2;                                 break;
        case GL_BOOL_VEC3:                                 result = RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC3;                                 break;
        case GL_BOOL_VEC4:                                 result = RAL_PROGRAM_VARIABLE_TYPE_BOOL_VEC4;                                 break;
        case GL_FLOAT:                                     result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT;                                     break;
        case GL_FLOAT_MAT2:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2;                                break;
        case GL_FLOAT_MAT3:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3;                                break;
        case GL_FLOAT_MAT4:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4;                                break;
        case GL_FLOAT_MAT2x3:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x3;                              break;
        case GL_FLOAT_MAT2x4:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT2x4;                              break;
        case GL_FLOAT_MAT3x2:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x2;                              break;
        case GL_FLOAT_MAT3x4:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT3x4;                              break;
        case GL_FLOAT_MAT4x2:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x2;                              break;
        case GL_FLOAT_MAT4x3:                              result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_MAT4x3;                              break;
        case GL_FLOAT_VEC2:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC2;                                break;
        case GL_FLOAT_VEC3:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC3;                                break;
        case GL_FLOAT_VEC4:                                result = RAL_PROGRAM_VARIABLE_TYPE_FLOAT_VEC4;                                break;
        case GL_IMAGE_1D:                                  result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D;                                  break;
        case GL_IMAGE_1D_ARRAY:                            result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_1D_ARRAY;                            break;
        case GL_IMAGE_2D:                                  result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D;                                  break;
        case GL_IMAGE_2D_ARRAY:                            result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_ARRAY;                            break;
        case GL_IMAGE_2D_MULTISAMPLE:                      result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE;                      break;
        case GL_IMAGE_2D_MULTISAMPLE_ARRAY:                result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_MULTISAMPLE_ARRAY;                break;
        case GL_IMAGE_2D_RECT:                             result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_2D_RECT;                             break;
        case GL_IMAGE_3D:                                  result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_3D;                                  break;
        case GL_IMAGE_BUFFER:                              result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_BUFFER;                              break;
        case GL_IMAGE_CUBE:                                result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE;                                break;
        case GL_IMAGE_CUBE_MAP_ARRAY:                      result = RAL_PROGRAM_VARIABLE_TYPE_IMAGE_CUBE_MAP_ARRAY;                      break;
        case GL_INT:                                       result = RAL_PROGRAM_VARIABLE_TYPE_INT;                                       break;
        case GL_INT_IMAGE_1D:                              result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D;                              break;
        case GL_INT_IMAGE_1D_ARRAY:                        result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_1D_ARRAY;                        break;
        case GL_INT_IMAGE_2D:                              result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D;                              break;
        case GL_INT_IMAGE_2D_ARRAY:                        result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_ARRAY;                        break;
        case GL_INT_IMAGE_2D_MULTISAMPLE:                  result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE;                  break;
        case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:            result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_MULTISAMPLE_ARRAY;            break;
        case GL_INT_IMAGE_2D_RECT:                         result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_2D_RECT;                         break;
        case GL_INT_IMAGE_3D:                              result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_3D;                              break;
        case GL_INT_IMAGE_BUFFER:                          result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_BUFFER;                          break;
        case GL_INT_IMAGE_CUBE:                            result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE;                            break;
        case GL_INT_IMAGE_CUBE_MAP_ARRAY:                  result = RAL_PROGRAM_VARIABLE_TYPE_INT_IMAGE_CUBE_MAP_ARRAY;                  break;
        case GL_INT_SAMPLER_1D:                            result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D;                            break;
        case GL_INT_SAMPLER_1D_ARRAY:                      result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_1D_ARRAY;                      break;
        case GL_INT_SAMPLER_2D:                            result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D;                            break;
        case GL_INT_SAMPLER_2D_ARRAY:                      result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_ARRAY;                      break;
        case GL_INT_SAMPLER_2D_MULTISAMPLE:                result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE;                break;
        case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:          result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_MULTISAMPLE_ARRAY;          break;
        case GL_INT_SAMPLER_2D_RECT:                       result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_2D_RECT;                       break;
        case GL_INT_SAMPLER_3D:                            result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_3D;                            break;
        case GL_INT_SAMPLER_BUFFER:                        result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_BUFFER;                        break;
        case GL_INT_SAMPLER_CUBE:                          result = RAL_PROGRAM_VARIABLE_TYPE_INT_SAMPLER_CUBE;                          break;
        case GL_INT_VEC2:                                  result = RAL_PROGRAM_VARIABLE_TYPE_INT_VEC2;                                  break;
        case GL_INT_VEC3:                                  result = RAL_PROGRAM_VARIABLE_TYPE_INT_VEC3;                                  break;
        case GL_INT_VEC4:                                  result = RAL_PROGRAM_VARIABLE_TYPE_INT_VEC4;                                  break;
        case GL_SAMPLER_1D:                                result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D;                                break;
        case GL_SAMPLER_1D_ARRAY:                          result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY;                          break;
        case GL_SAMPLER_1D_ARRAY_SHADOW:                   result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_ARRAY_SHADOW;                   break;
        case GL_SAMPLER_1D_SHADOW:                         result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_1D_SHADOW;                         break;
        case GL_SAMPLER_2D:                                result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D;                                break;
        case GL_SAMPLER_2D_ARRAY:                          result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY;                          break;
        case GL_SAMPLER_2D_ARRAY_SHADOW:                   result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_ARRAY_SHADOW;                   break;
        case GL_SAMPLER_2D_MULTISAMPLE:                    result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE;                    break;
        case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:              result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_MULTISAMPLE_ARRAY;              break;
        case GL_SAMPLER_2D_RECT:                           result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT;                           break;
        case GL_SAMPLER_2D_RECT_SHADOW:                    result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_RECT_SHADOW;                    break;
        case GL_SAMPLER_2D_SHADOW:                         result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_2D_SHADOW;                         break;
        case GL_SAMPLER_3D:                                result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_3D;                                break;
        case GL_SAMPLER_BUFFER:                            result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_BUFFER;                            break;
        case GL_SAMPLER_CUBE:                              result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE;                              break;
        case GL_SAMPLER_CUBE_MAP_ARRAY:                    result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY;                        break;
        case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW:             result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_ARRAY_SHADOW;                 break;
        case GL_SAMPLER_CUBE_SHADOW:                       result = RAL_PROGRAM_VARIABLE_TYPE_SAMPLER_CUBE_SHADOW;                       break;
        case GL_UNSIGNED_INT:                              result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT;                              break;
        case GL_UNSIGNED_INT_IMAGE_1D:                     result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D;                     break;
        case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:               result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_1D_ARRAY;               break;
        case GL_UNSIGNED_INT_IMAGE_2D:                     result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D;                     break;
        case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:               result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_ARRAY;               break;
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:         result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE;         break;
        case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:   result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY;   break;
        case GL_UNSIGNED_INT_IMAGE_2D_RECT:                result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_2D_RECT;                break;
        case GL_UNSIGNED_INT_IMAGE_3D:                     result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_3D;                     break;
        case GL_UNSIGNED_INT_IMAGE_BUFFER:                 result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_BUFFER;                 break;
        case GL_UNSIGNED_INT_IMAGE_CUBE:                   result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE;                   break;
        case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY:         result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY;         break;
        case GL_UNSIGNED_INT_SAMPLER_1D:                   result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D;                   break;
        case GL_UNSIGNED_INT_SAMPLER_2D:                   result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D;                   break;
        case GL_UNSIGNED_INT_SAMPLER_3D:                   result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_3D;                   break;
        case GL_UNSIGNED_INT_SAMPLER_CUBE:                 result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE;                 break;
        case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY:       result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_CUBE_ARRAY;           break;
        case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:             result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_1D_ARRAY;             break;
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:             result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_ARRAY;             break;
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:       result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE;       break;
        case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY; break;
        case GL_UNSIGNED_INT_SAMPLER_BUFFER:               result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_BUFFER;               break;
        case GL_UNSIGNED_INT_SAMPLER_2D_RECT:              result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_SAMPLER_2D_RECT;              break;
        case GL_UNSIGNED_INT_VEC2:                         result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC2;                         break;
        case GL_UNSIGNED_INT_VEC3:                         result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC3;                         break;
        case GL_UNSIGNED_INT_VEC4:                         result = RAL_PROGRAM_VARIABLE_TYPE_UNSIGNED_INT_VEC4;                         break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized variable type reported by OpenGL");
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC ral_format raGL_utils_get_ral_format_for_ogl_enum(GLenum internalformat)
{
    ral_format result = RAL_FORMAT_UNKNOWN;

    switch (internalformat)
    {
        case GL_COMPRESSED_R11_EAC:                        result = RAL_FORMAT_COMPRESSED_R11_EAC_UNORM;                        break;
        case GL_COMPRESSED_RED_RGTC1:                      result = RAL_FORMAT_COMPRESSED_BC4_UNORM;                            break;
        case GL_COMPRESSED_RG11_EAC:                       result = RAL_FORMAT_COMPRESSED_RG11_EAC_UNORM;                       break;
        case GL_COMPRESSED_RG_RGTC2:                       result = RAL_FORMAT_COMPRESSED_BC5_UNORM;                            break;
        case GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_ARB:      result = RAL_FORMAT_COMPRESSED_BC6_SFLOAT;                           break;
        case GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_ARB:    result = RAL_FORMAT_COMPRESSED_BC6_UFLOAT;                           break;
        case GL_COMPRESSED_RGB8_ETC2:                      result = RAL_FORMAT_COMPRESSED_RGB8_ETC2_UNORM;                      break;
        case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:  result = RAL_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM;  break;
        case GL_COMPRESSED_RGBA8_ETC2_EAC:                 result = RAL_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM;                 break;
        case GL_COMPRESSED_RGBA_BPTC_UNORM_ARB:            result = RAL_FORMAT_COMPRESSED_BC7_UNORM;                            break;
        case GL_COMPRESSED_SIGNED_R11_EAC:                 result = RAL_FORMAT_COMPRESSED_R11_EAC_SNORM;                        break;
        case GL_COMPRESSED_SIGNED_RED_RGTC1:               result = RAL_FORMAT_COMPRESSED_BC4_SNORM;                            break;
        case GL_COMPRESSED_SIGNED_RG11_EAC:                result = RAL_FORMAT_COMPRESSED_RG11_EAC_SNORM;                       break;
        case GL_COMPRESSED_SIGNED_RG_RGTC2:                result = RAL_FORMAT_COMPRESSED_BC5_SNORM;                            break;
        case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:          result = RAL_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM;          break;
        case GL_COMPRESSED_SRGB8_ETC2:                     result = RAL_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM;                     break;
        case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: result = RAL_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM; break;
        case GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_ARB:      result = RAL_FORMAT_COMPRESSED_BC7_SRGB_UNORM;                       break;

        case GL_DEPTH_COMPONENT16:           result = RAL_FORMAT_DEPTH16_SNORM;          break;
        case GL_DEPTH_COMPONENT24:           result = RAL_FORMAT_DEPTH24_SNORM;          break;
        case GL_DEPTH_COMPONENT32F:          result = RAL_FORMAT_DEPTH32_FLOAT;          break;
        case GL_DEPTH_COMPONENT32:           result = RAL_FORMAT_DEPTH32_SNORM;          break;
        case GL_DEPTH24_STENCIL8:            result = RAL_FORMAT_DEPTH24_STENCIL8;       break;
        case GL_DEPTH32F_STENCIL8:           result = RAL_FORMAT_DEPTH32F_STENCIL8;      break;
        case GL_R11F_G11F_B10F:              result = RAL_FORMAT_R11FG11FB10F;           break;
        case GL_R16F:                        result = RAL_FORMAT_R16_FLOAT;              break;
        case GL_R16I:                        result = RAL_FORMAT_R16_SINT;               break;
        case GL_R16_SNORM:                   result = RAL_FORMAT_R16_SNORM;              break;
        case GL_R16UI:                       result = RAL_FORMAT_R16_UINT;               break;
        case GL_R16:                         result = RAL_FORMAT_R16_UNORM;              break;
        case GL_R3_G3_B2:                    result = RAL_FORMAT_R3G3B2_UNORM;           break;
        case GL_R32F:                        result = RAL_FORMAT_R32_FLOAT;              break;
        case GL_R32I:                        result = RAL_FORMAT_R32_SINT;               break;
        case GL_R32UI:                       result = RAL_FORMAT_R32_UINT;               break;
        case GL_R8I:                         result = RAL_FORMAT_R8_SINT;                break;
        case GL_R8_SNORM:                    result = RAL_FORMAT_R8_SNORM;               break;
        case GL_R8UI:                        result = RAL_FORMAT_R8_UINT;                break;
        case GL_R8:                          result = RAL_FORMAT_R8_UNORM;               break;
        case GL_RG16F:                       result = RAL_FORMAT_RG16_FLOAT;             break;
        case GL_RG16I:                       result = RAL_FORMAT_RG16_SINT;              break;
        case GL_RG16_SNORM:                  result = RAL_FORMAT_RG16_SNORM;             break;
        case GL_RG16UI:                      result = RAL_FORMAT_RG16_UINT;              break;
        case GL_RG16:                        result = RAL_FORMAT_RG16_UNORM;             break;
        case GL_RG32F:                       result = RAL_FORMAT_RG32_FLOAT;             break;
        case GL_RG32I:                       result = RAL_FORMAT_RG32_SINT;              break;
        case GL_RG32UI:                      result = RAL_FORMAT_RG32_UINT;              break;
        case GL_RG8:                         result = RAL_FORMAT_RG8_UNORM;              break;
        case GL_RG8I:                        result = RAL_FORMAT_RG8_SINT;               break;
        case GL_RG8_SNORM:                   result = RAL_FORMAT_RG8_SNORM;              break;
        case GL_RG8UI:                       result = RAL_FORMAT_RG8_UINT;               break;
        case GL_RGB10:                       result = RAL_FORMAT_RGB10_UNORM;            break;
        case GL_RGB10_A2UI:                  result = RAL_FORMAT_RGB10A2_UINT;           break;
        case GL_RGB10_A2:                    result = RAL_FORMAT_RGB10A2_UNORM;          break;
        case GL_RGB12:                       result = RAL_FORMAT_RGB12_UNORM;            break;
        case GL_RGB16F:                      result = RAL_FORMAT_RGB16_FLOAT;            break;
        case GL_RGB16I:                      result = RAL_FORMAT_RGB16_SINT;             break;
        case GL_RGB16_SNORM:                 result = RAL_FORMAT_RGB16_SNORM;            break;
        case GL_RGB16UI:                     result = RAL_FORMAT_RGB16_UINT;             break;
        case GL_RGB16:                       result = RAL_FORMAT_RGB16_UNORM;            break;
        case GL_RGB32F:                      result = RAL_FORMAT_RGB32_FLOAT;            break;
        case GL_RGB32I:                      result = RAL_FORMAT_RGB32_SINT;             break;
        case GL_RGB32UI:                     result = RAL_FORMAT_RGB32_UINT;             break;
        case GL_RGB4:                        result = RAL_FORMAT_RGB4_UNORM;             break;
        case GL_RGB5:                        result = RAL_FORMAT_RGB5_UNORM;             break;
        case GL_RGB5_A1:                     result = RAL_FORMAT_RGB5A1_UNORM;           break;
        case GL_RGB8I:                       result = RAL_FORMAT_RGB8_SINT;              break;
        case GL_RGB8_SNORM:                  result = RAL_FORMAT_RGB8_SNORM;             break;
        case GL_RGB8UI:                      result = RAL_FORMAT_RGB8_UINT;              break;
        case GL_RGB8:                        result = RAL_FORMAT_RGB8_UNORM;             break;
        case GL_RGB9_E5:                     result = RAL_FORMAT_RGB9E5_FLOAT;           break;
        case GL_RGBA12:                      result = RAL_FORMAT_RGBA12_UNORM;           break;
        case GL_RGBA16F:                     result = RAL_FORMAT_RGBA16_FLOAT;           break;
        case GL_RGBA16I:                     result = RAL_FORMAT_RGBA16_SINT;            break;
        case GL_RGBA16_SNORM:                result = RAL_FORMAT_RGBA16_SNORM;           break;
        case GL_RGBA16UI:                    result = RAL_FORMAT_RGBA16_UINT;            break;
        case GL_RGBA16:                      result = RAL_FORMAT_RGBA16_UNORM;           break;
        case GL_RGBA2:                       result = RAL_FORMAT_RGBA2_UNORM;            break;
        case GL_RGBA32F:                     result = RAL_FORMAT_RGBA32_FLOAT;           break;
        case GL_RGBA32I:                     result = RAL_FORMAT_RGBA32_SINT;            break;
        case GL_RGBA32UI:                    result = RAL_FORMAT_RGBA32_UINT;            break;
        case GL_RGBA4:                       result = RAL_FORMAT_RGBA4_UNORM;            break;
        case GL_RGBA8:                       result = RAL_FORMAT_RGBA8_UNORM;            break;
        case GL_RGBA8I:                      result = RAL_FORMAT_RGBA8_SINT;             break;
        case GL_RGBA8_SNORM:                 result = RAL_FORMAT_RGBA8_SNORM;            break;
        case GL_RGBA8UI:                     result = RAL_FORMAT_RGBA8_UINT;             break;
        case GL_SRGB8:                       result = RAL_FORMAT_SRGB8_UNORM;            break;
        case GL_SRGB8_ALPHA8:                result = RAL_FORMAT_SRGBA8_UNORM;           break;

        default:
        {
            LOG_FATAL("OGL texture internal format [%u] not supported by RAL",
                      internalformat);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_enum(GLenum in_texture_target)
{
    ral_texture_type result = RAL_TEXTURE_TYPE_UNKNOWN;

    switch (in_texture_target)
    {
        case GL_TEXTURE_1D:                   result = RAL_TEXTURE_TYPE_1D;                   break;
        case GL_TEXTURE_1D_ARRAY:             result = RAL_TEXTURE_TYPE_1D_ARRAY;             break;
        case GL_TEXTURE_2D:                   result = RAL_TEXTURE_TYPE_2D;                   break;
        case GL_TEXTURE_2D_ARRAY:             result = RAL_TEXTURE_TYPE_2D_ARRAY;             break;
        case GL_TEXTURE_2D_MULTISAMPLE:       result = RAL_TEXTURE_TYPE_MULTISAMPLE_2D;       break;
        case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: result = RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY; break;
        case GL_TEXTURE_3D:                   result = RAL_TEXTURE_TYPE_3D;                   break;
        case GL_TEXTURE_CUBE_MAP:             result = RAL_TEXTURE_TYPE_CUBE_MAP;             break;
        case GL_TEXTURE_CUBE_MAP_ARRAY:       result = RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY;       break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Input OpenGL texture target is not recognized by RAL.");
        }
    }

    return result;
}