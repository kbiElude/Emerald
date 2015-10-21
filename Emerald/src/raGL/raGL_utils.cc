/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "raGL/raGL_utils.h"
#include "system/system_log.h"


/** Please see header for specification */
PUBLIC EMERALD_API ogl_primitive_type raGL_utils_get_ogl_primive_type_for_ral_primitive_type(ral_primitive_type in_primitive_type)
{
    static const ogl_primitive_type result_array[] =
    {
        OGL_PRIMITIVE_TYPE_LINE_LOOP,
        OGL_PRIMITIVE_TYPE_LINE_STRIP,
        OGL_PRIMITIVE_TYPE_LINE_STRIP_ADJACENCY,
        OGL_PRIMITIVE_TYPE_LINES,
        OGL_PRIMITIVE_TYPE_LINES_ADJACENCY,
        OGL_PRIMITIVE_TYPE_POINTS,
        OGL_PRIMITIVE_TYPE_TRIANGLE_FAN,
        OGL_PRIMITIVE_TYPE_TRIANGLE_STRIP,
        OGL_PRIMITIVE_TYPE_TRIANGLE_STRIP_ADJACENCY,
        OGL_PRIMITIVE_TYPE_TRIANGLES,
        OGL_PRIMITIVE_TYPE_TRIANGLES_ADJACENCY
    };

    ASSERT_DEBUG_SYNC(in_primitive_type < RAL_PRIMITIVE_TYPE_COUNT,
                      "RAL primitive type not supported for OpenGL back-end");

    return result_array[in_primitive_type];
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader_type raGL_utils_get_ogl_shader_type_for_ral_shader_type(ral_shader_type in_shader_type)
{
    static const ogl_shader_type result_array[] =
    {
        OGL_SHADER_TYPE_COMPUTE,
        OGL_SHADER_TYPE_FRAGMENT,
        OGL_SHADER_TYPE_GEOMETRY,
        OGL_SHADER_TYPE_TESSELLATION_CONTROL,
        OGL_SHADER_TYPE_TESSELLATION_EVALUATION,
        OGL_SHADER_TYPE_VERTEX
    };

    ASSERT_DEBUG_SYNC(in_shader_type < RAL_SHADER_TYPE_COUNT,
                      "RAL shader stage unsupported for OpenGL back-end");

    return result_array[in_shader_type];
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture_internalformat raGL_utils_get_ogl_texture_internalformat_for_ral_texture_format(ral_texture_format in_texture_format)
{
    static const ogl_texture_internalformat result_array[] =
    {
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_R11_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RED_RGTC1,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG11_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG_RGTC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB8_ETC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGBA8_ETC2_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGBA_BPTC_UNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_R11_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RED_RGTC1,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG11_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG_RGTC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_ETC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
        OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32F,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH24_STENCIL8,
        OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH32F_STENCIL8,
        OGL_TEXTURE_INTERNALFORMAT_GL_R11F_G11F_B10F,
        OGL_TEXTURE_INTERNALFORMAT_GL_R16F,
        OGL_TEXTURE_INTERNALFORMAT_GL_R16I,
        OGL_TEXTURE_INTERNALFORMAT_GL_R16_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_R16UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_R16,
        OGL_TEXTURE_INTERNALFORMAT_GL_R3_G3_B2,
        OGL_TEXTURE_INTERNALFORMAT_GL_R32F,
        OGL_TEXTURE_INTERNALFORMAT_GL_R32I,
        OGL_TEXTURE_INTERNALFORMAT_GL_R32UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_R8I,
        OGL_TEXTURE_INTERNALFORMAT_GL_R8_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_R8UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_R8,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG16F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG16I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG16_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG16UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG16,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG32F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG32I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG32UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG8,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG8I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG8_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RG8UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB10,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB12,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB16F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB16I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB16_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB16UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB16,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB32F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB32I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB32UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB4,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB5,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB5_A1,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB8I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB8_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB8UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB8,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGB9_E5,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA12,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA2,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32F,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA4,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8I,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8_SNORM,
        OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8UI,
        OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8,
        OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8_ALPHA8,
    };

    ASSERT_DEBUG_SYNC(in_texture_format < RAL_TEXTURE_FORMAT_COUNT,
                      "RAL texture format not supported by the GL back-ed");

    return result_array[in_texture_format];
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture_target raGL_utils_get_ogl_texture_target_for_ral_texture_type(ral_texture_type in_texture_type)
{
    static const ogl_texture_target result_array[] =
    {
        OGL_TEXTURE_TARGET_GL_TEXTURE_1D,
        OGL_TEXTURE_TARGET_GL_TEXTURE_1D_ARRAY,
        OGL_TEXTURE_TARGET_GL_TEXTURE_2D,
        OGL_TEXTURE_TARGET_GL_TEXTURE_2D_ARRAY,
        OGL_TEXTURE_TARGET_GL_TEXTURE_3D,
        OGL_TEXTURE_TARGET_GL_TEXTURE_BUFFER,
        OGL_TEXTURE_TARGET_GL_TEXTURE_CUBE_MAP,
        OGL_TEXTURE_TARGET_GL_TEXTURE_CUBE_MAP_ARRAY,
        OGL_TEXTURE_TARGET_GL_TEXTURE_RECTANGLE,

        OGL_TEXTURE_TARGET_GL_TEXTURE_2D_MULTISAMPLE,
        OGL_TEXTURE_TARGET_GL_TEXTURE_2D_MULTISAMPLE_ARRAY,
    };

    ASSERT_DEBUG_SYNC(in_texture_type < RAL_TEXTURE_TYPE_COUNT,
                      "RAL texture format not supported by the GL back-end");

    return result_array[in_texture_type];
}

/* Please see header for specification */
PUBLIC EMERALD_API ral_texture_format raGL_utils_get_ral_texture_format_for_ogl_enum(GLenum internalformat)
{
    return raGL_utils_get_ral_texture_format_for_ogl_texture_internalformat( (ogl_texture_internalformat) internalformat);
}

/* Please see header for specification */
PUBLIC EMERALD_API ral_texture_format raGL_utils_get_ral_texture_format_for_ogl_texture_internalformat(ogl_texture_internalformat internalformat)
{
    ral_texture_format result = RAL_TEXTURE_FORMAT_UNKNOWN;

    switch (internalformat)
    {
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_R11_EAC:                        result = RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_UNORM;                        break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RED_RGTC1:                      result = RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_UNORM;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG11_EAC:                       result = RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_UNORM;                       break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG_RGTC2:                       result = RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_UNORM;                       break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT:          result = RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_SFLOAT;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT:        result = RAL_TEXTURE_FORMAT_COMPRESSED_RGB_BPTC_UFLOAT;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB8_ETC2:                      result = RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_ETC2_UNORM;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:  result = RAL_TEXTURE_FORMAT_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM;  break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGBA8_ETC2_EAC:                 result = RAL_TEXTURE_FORMAT_COMPRESSED_RGBA8_ETC2_EAC_UNORM;                 break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RGBA_BPTC_UNORM:                result = RAL_TEXTURE_FORMAT_COMPRESSED_RGBA_BPTC_UNORM;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_R11_EAC:                 result = RAL_TEXTURE_FORMAT_COMPRESSED_R11_EAC_SNORM;                        break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RED_RGTC1:               result = RAL_TEXTURE_FORMAT_COMPRESSED_RED_RGTC1_SNORM;                      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG11_EAC:                result = RAL_TEXTURE_FORMAT_COMPRESSED_RG11_EAC_SNORM;                       break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG_RGTC2:                result = RAL_TEXTURE_FORMAT_COMPRESSED_RG_RGTC2_SNORM;                       break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:          result = RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC_UNORM;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_ETC2:                     result = RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_ETC2_UNORM;                     break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2: result = RAL_TEXTURE_FORMAT_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2_UNORM; break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM:          result = RAL_TEXTURE_FORMAT_COMPRESSED_SRGB_ALPHA_BPTC_UNORM;                break;

        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16:           result = RAL_TEXTURE_FORMAT_DEPTH16_SNORM;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24:           result = RAL_TEXTURE_FORMAT_DEPTH24_SNORM;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32F:          result = RAL_TEXTURE_FORMAT_DEPTH32_FLOAT;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32:           result = RAL_TEXTURE_FORMAT_DEPTH32_SNORM;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH24_STENCIL8:            result = RAL_TEXTURE_FORMAT_DEPTH24_STENCIL8;       break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH32F_STENCIL8:           result = RAL_TEXTURE_FORMAT_DEPTH32F_STENCIL8;      break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R11F_G11F_B10F:              result = RAL_TEXTURE_FORMAT_R11FG11FB10F;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R16F:                        result = RAL_TEXTURE_FORMAT_R16_FLOAT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R16I:                        result = RAL_TEXTURE_FORMAT_R16_SINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R16_SNORM:                   result = RAL_TEXTURE_FORMAT_R16_SNORM;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R16UI:                       result = RAL_TEXTURE_FORMAT_R16_UINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R16:                         result = RAL_TEXTURE_FORMAT_R16_UNORM;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R3_G3_B2:                    result = RAL_TEXTURE_FORMAT_R3G3B2_UNORM;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R32F:                        result = RAL_TEXTURE_FORMAT_R32_FLOAT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R32I:                        result = RAL_TEXTURE_FORMAT_R32_SINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R32UI:                       result = RAL_TEXTURE_FORMAT_R32_UINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R8I:                         result = RAL_TEXTURE_FORMAT_R8_SINT;                break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R8_SNORM:                    result = RAL_TEXTURE_FORMAT_R8_SNORM;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R8UI:                        result = RAL_TEXTURE_FORMAT_R8_UINT;                break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_R8:                          result = RAL_TEXTURE_FORMAT_R8_UNORM;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG16F:                       result = RAL_TEXTURE_FORMAT_RG16_FLOAT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG16I:                       result = RAL_TEXTURE_FORMAT_RG16_SINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG16_SNORM:                  result = RAL_TEXTURE_FORMAT_RG16_SNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG16UI:                      result = RAL_TEXTURE_FORMAT_RG16_UINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG16:                        result = RAL_TEXTURE_FORMAT_RG16_UNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG32F:                       result = RAL_TEXTURE_FORMAT_RG32_FLOAT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG32I:                       result = RAL_TEXTURE_FORMAT_RG32_SINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG32UI:                      result = RAL_TEXTURE_FORMAT_RG32_UINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG8:                         result = RAL_TEXTURE_FORMAT_RG8_UNORM;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG8I:                        result = RAL_TEXTURE_FORMAT_RG8_SINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG8_SNORM:                   result = RAL_TEXTURE_FORMAT_RG8_SNORM;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RG8UI:                       result = RAL_TEXTURE_FORMAT_RG8_UINT;               break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB10:                       result = RAL_TEXTURE_FORMAT_RGB10_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2UI:                  result = RAL_TEXTURE_FORMAT_RGB10A2_UINT;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2:                    result = RAL_TEXTURE_FORMAT_RGB10A2_UNORM;          break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB12:                       result = RAL_TEXTURE_FORMAT_RGB12_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB16F:                      result = RAL_TEXTURE_FORMAT_RGB16_FLOAT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB16I:                      result = RAL_TEXTURE_FORMAT_RGB16_SINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB16_SNORM:                 result = RAL_TEXTURE_FORMAT_RGB16_SNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB16UI:                     result = RAL_TEXTURE_FORMAT_RGB16_UINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB16:                       result = RAL_TEXTURE_FORMAT_RGB16_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB32F:                      result = RAL_TEXTURE_FORMAT_RGB32_FLOAT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB32I:                      result = RAL_TEXTURE_FORMAT_RGB32_SINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB32UI:                     result = RAL_TEXTURE_FORMAT_RGB32_UINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB4:                        result = RAL_TEXTURE_FORMAT_RGB4_UNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB5:                        result = RAL_TEXTURE_FORMAT_RGB5_UNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB5_A1:                     result = RAL_TEXTURE_FORMAT_RGB5A1_UNORM;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB8I:                       result = RAL_TEXTURE_FORMAT_RGB8_SINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB8_SNORM:                  result = RAL_TEXTURE_FORMAT_RGB8_SNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB8UI:                      result = RAL_TEXTURE_FORMAT_RGB8_UINT;              break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB8:                        result = RAL_TEXTURE_FORMAT_RGB8_UNORM;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGB9_E5:                     result = RAL_TEXTURE_FORMAT_RGB9E5_FLOAT;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA12:                      result = RAL_TEXTURE_FORMAT_RGBA12_UNORM;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16F:                     result = RAL_TEXTURE_FORMAT_RGBA16_FLOAT;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16I:                     result = RAL_TEXTURE_FORMAT_RGBA16_SINT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16_SNORM:                result = RAL_TEXTURE_FORMAT_RGBA16_SNORM;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16UI:                    result = RAL_TEXTURE_FORMAT_RGBA16_UINT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16:                      result = RAL_TEXTURE_FORMAT_RGBA16_UNORM;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA2:                       result = RAL_TEXTURE_FORMAT_RGBA2_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32F:                     result = RAL_TEXTURE_FORMAT_RGBA32_FLOAT;           break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32I:                     result = RAL_TEXTURE_FORMAT_RGBA32_SINT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32UI:                    result = RAL_TEXTURE_FORMAT_RGBA32_UINT;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA4:                       result = RAL_TEXTURE_FORMAT_RGBA4_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8:                       result = RAL_TEXTURE_FORMAT_RGBA8_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8I:                      result = RAL_TEXTURE_FORMAT_RGBA8_SINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8_SNORM:                 result = RAL_TEXTURE_FORMAT_RGBA8_SNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8UI:                     result = RAL_TEXTURE_FORMAT_RGBA8_UINT;             break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8:                       result = RAL_TEXTURE_FORMAT_SRGB8_UNORM;            break;
        case OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8_ALPHA8:                result = RAL_TEXTURE_FORMAT_SRGBA8_UNORM;           break;

        default:
        {
            LOG_FATAL("OGL texture internal format [%d] not supported by RAL",
                      internalformat);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_enum(GLenum in_texture_target_glenum)
{
    return raGL_utils_get_ral_texture_type_for_ogl_texture_target( (ogl_texture_target) in_texture_target_glenum);
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_texture_type raGL_utils_get_ral_texture_type_for_ogl_texture_target(ogl_texture_target in_texture_target)
{
    ral_texture_type result = RAL_TEXTURE_TYPE_UNKNOWN;

    switch (in_texture_target)
    {
        case OGL_TEXTURE_TARGET_GL_TEXTURE_1D:                   result = RAL_TEXTURE_TYPE_1D;                   break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_1D_ARRAY:             result = RAL_TEXTURE_TYPE_1D_ARRAY;             break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_2D:                   result = RAL_TEXTURE_TYPE_2D;                   break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_2D_ARRAY:             result = RAL_TEXTURE_TYPE_2D_ARRAY;             break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_2D_MULTISAMPLE:       result = RAL_TEXTURE_TYPE_MULTISAMPLE_2D;       break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_2D_MULTISAMPLE_ARRAY: result = RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY; break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_3D:                   result = RAL_TEXTURE_TYPE_3D;                   break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_BUFFER:               result = RAL_TEXTURE_TYPE_BUFFER;               break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_CUBE_MAP:             result = RAL_TEXTURE_TYPE_CUBE_MAP;             break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_CUBE_MAP_ARRAY:       result = RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY;       break;
        case OGL_TEXTURE_TARGET_GL_TEXTURE_RECTANGLE:            result = RAL_TEXTURE_TYPE_RECTANGLE;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Input OpenGL texture target is not recognized by RAL.");
        }
    } /* switch (in_texture_target) */

    return result;
}