/* */
#include "shared.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "raGL/raGL_ral.h"

/** Please see header for specification */
PUBLIC ogl_texture_internalformat raGL_get_ogl_texture_internalformat_for_ral_texture_format(ral_texture_format in_texture_format)
{
    ogl_texture_internalformat result = OGL_TEXTURE_INTERNALFORMAT_UNKNOWN;

    switch (in_texture_format)
    {
        case RAL_TEXTURE_FORMAT_COMPRESSED_RGTC1_SNORM: result = OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RED_RGTC1;        break;
        case RAL_TEXTURE_FORMAT_COMPRESSED_RGTC1_UNORM: result = OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RED_RGTC1; break;
        case RAL_TEXTURE_FORMAT_COMPRESSED_RGTC2_SNORM: result = OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_RG_RGTC2;         break;
        case RAL_TEXTURE_FORMAT_COMPRESSED_RGTC2_UNORM: result = OGL_TEXTURE_INTERNALFORMAT_GL_COMPRESSED_SIGNED_RG_RGTC2;  break;
        case RAL_TEXTURE_FORMAT_DEPTH16_SNORM:          result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT16;           break;
        case RAL_TEXTURE_FORMAT_DEPTH24_SNORM:          result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT24;           break;
        case RAL_TEXTURE_FORMAT_DEPTH32_FLOAT:          result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32F;          break;
        case RAL_TEXTURE_FORMAT_DEPTH32_SNORM:          result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH_COMPONENT32;           break;
        case RAL_TEXTURE_FORMAT_DEPTH24_STENCIL8:       result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH24_STENCIL8;            break;
        case RAL_TEXTURE_FORMAT_DEPTH32F_STENCIL8:      result = OGL_TEXTURE_INTERNALFORMAT_GL_DEPTH32F_STENCIL8;           break;
        case RAL_TEXTURE_FORMAT_R11FG11FB10F:           result = OGL_TEXTURE_INTERNALFORMAT_GL_R11F_G11F_B10F;              break;
        case RAL_TEXTURE_FORMAT_R16_FLOAT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_R16F;                        break;
        case RAL_TEXTURE_FORMAT_R16_SINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R16I;                        break;
        case RAL_TEXTURE_FORMAT_R16_SNORM:              result = OGL_TEXTURE_INTERNALFORMAT_GL_R16_SNORM;                   break;
        case RAL_TEXTURE_FORMAT_R16_UINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R16UI;                       break;
        case RAL_TEXTURE_FORMAT_R16_UNORM:              result = OGL_TEXTURE_INTERNALFORMAT_GL_R16;                         break;
        case RAL_TEXTURE_FORMAT_R3G3B2_UNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_R3_G3_B2;                    break;
        case RAL_TEXTURE_FORMAT_R32_FLOAT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_R32F;                        break;
        case RAL_TEXTURE_FORMAT_R32_SINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R32I;                        break;
        case RAL_TEXTURE_FORMAT_R32_UINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R32UI;                       break;
        case RAL_TEXTURE_FORMAT_R8_SINT:                result = OGL_TEXTURE_INTERNALFORMAT_GL_R8I;                         break;
        case RAL_TEXTURE_FORMAT_R8_SNORM:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R8_SNORM;                    break;
        case RAL_TEXTURE_FORMAT_R8_UINT:                result = OGL_TEXTURE_INTERNALFORMAT_GL_R8UI;                        break;
        case RAL_TEXTURE_FORMAT_R8_UNORM:               result = OGL_TEXTURE_INTERNALFORMAT_GL_R8;                          break;
        case RAL_TEXTURE_FORMAT_RG16_FLOAT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RG16F;                       break;
        case RAL_TEXTURE_FORMAT_RG16_SINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG16I;                       break;
        case RAL_TEXTURE_FORMAT_RG16_SNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RG16_SNORM;                  break;
        case RAL_TEXTURE_FORMAT_RG16_UINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG16UI;                      break;
        case RAL_TEXTURE_FORMAT_RG16_UNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RG16;                        break;
        case RAL_TEXTURE_FORMAT_RG32_FLOAT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RG32F;                       break;
        case RAL_TEXTURE_FORMAT_RG32_SINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG32I;                       break;
        case RAL_TEXTURE_FORMAT_RG32_UINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG32UI;                      break;
        case RAL_TEXTURE_FORMAT_RG8_UNORM:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG8;                         break;
        case RAL_TEXTURE_FORMAT_RG8_SINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_RG8I;                        break;
        case RAL_TEXTURE_FORMAT_RG8_SNORM:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RG8_SNORM;                   break;
        case RAL_TEXTURE_FORMAT_RG8_UINT:               result = OGL_TEXTURE_INTERNALFORMAT_GL_RG8UI;                       break;
        case RAL_TEXTURE_FORMAT_RGB10_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB10;                       break;
        case RAL_TEXTURE_FORMAT_RGB10A2_UINT:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2UI;                  break;
        case RAL_TEXTURE_FORMAT_RGB10A2_UNORM:          result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB10_A2;                    break;
        case RAL_TEXTURE_FORMAT_RGB12_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB12;                       break;
        case RAL_TEXTURE_FORMAT_RGB16_FLOAT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB16F;                      break;
        case RAL_TEXTURE_FORMAT_RGB16_SINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB16I;                      break;
        case RAL_TEXTURE_FORMAT_RGB16_SNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB16_SNORM;                 break;
        case RAL_TEXTURE_FORMAT_RGB16_UINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB16UI;                     break;
        case RAL_TEXTURE_FORMAT_RGB16_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB16;                       break;
        case RAL_TEXTURE_FORMAT_RGB32_FLOAT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB32F;                      break;
        case RAL_TEXTURE_FORMAT_RGB32_SINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB32I;                      break;
        case RAL_TEXTURE_FORMAT_RGB32_UINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB32UI;                     break;
        case RAL_TEXTURE_FORMAT_RGB4_UNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB4;                        break;
        case RAL_TEXTURE_FORMAT_RGB5_UNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB5;                        break;
        case RAL_TEXTURE_FORMAT_RGB5A1_UNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB5_A1;                     break;
        case RAL_TEXTURE_FORMAT_RGB8_SINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB8I;                       break;
        case RAL_TEXTURE_FORMAT_RGB8_SNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB8_SNORM;                  break;
        case RAL_TEXTURE_FORMAT_RGB8_UINT:              result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB8UI;                      break;
        case RAL_TEXTURE_FORMAT_RGB8_UNORM:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB8;                        break;
        case RAL_TEXTURE_FORMAT_RGB9E5_FLOAT:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGB9_E5;                     break;
        case RAL_TEXTURE_FORMAT_RGBA12_UNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA12;                      break;
        case RAL_TEXTURE_FORMAT_RGBA16_FLOAT:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16F;                     break;
        case RAL_TEXTURE_FORMAT_RGBA16_SINT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16I;                     break;
        case RAL_TEXTURE_FORMAT_RGBA16_SNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16_SNORM;                break;
        case RAL_TEXTURE_FORMAT_RGBA16_UINT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16UI;                    break;
        case RAL_TEXTURE_FORMAT_RGBA16_UNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA16;                      break;
        case RAL_TEXTURE_FORMAT_RGBA2_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA2;                       break;
        case RAL_TEXTURE_FORMAT_RGBA32_FLOAT:           result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32F;                     break;
        case RAL_TEXTURE_FORMAT_RGBA32_SINT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32I;                     break;
        case RAL_TEXTURE_FORMAT_RGBA32_UINT:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA32UI;                    break;
        case RAL_TEXTURE_FORMAT_RGBA4_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA4;                       break;
        case RAL_TEXTURE_FORMAT_RGBA8_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8;                       break;
        case RAL_TEXTURE_FORMAT_RGBA8_SINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8I;                      break;
        case RAL_TEXTURE_FORMAT_RGBA8_SNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8_SNORM;                 break;
        case RAL_TEXTURE_FORMAT_RGBA8_UINT:             result = OGL_TEXTURE_INTERNALFORMAT_GL_RGBA8UI;                     break;
        case RAL_TEXTURE_FORMAT_SRGB8_UNORM:            result = OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8;                       break;
        case RAL_TEXTURE_FORMAT_SRGBA8_UNORM:           result = OGL_TEXTURE_INTERNALFORMAT_GL_SRGB8_ALPHA8;                break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "RAL texture format not supported by OpenGL!");
        }
    } /* switch (in_texture_format) */

    return result;
}