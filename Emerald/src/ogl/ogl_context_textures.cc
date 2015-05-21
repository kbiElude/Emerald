/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_texture.h"
#include "system/system_log.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/* Private type definitions */
typedef struct _ogl_context_textures
{
    ogl_context      context;
    system_hash64map textures_by_filename;
    system_hash64map textures_by_name;

    system_hash64map        reusable_texture_key_to_ogl_texture_vector_map;
    system_resizable_vector reusable_textures;

    _ogl_context_textures()
    {
        context                                        = NULL;
        reusable_texture_key_to_ogl_texture_vector_map = system_hash64map_create       (sizeof(system_resizable_vector) );
        reusable_textures                              = system_resizable_vector_create(4, /* capacity */
                                                                                        sizeof(ogl_texture) );
        textures_by_filename                           = system_hash64map_create       (sizeof(ogl_texture) );
        textures_by_name                               = system_hash64map_create       (sizeof(ogl_texture) );
    }

    ~_ogl_context_textures()
    {
        LOG_INFO("Texture manager deallocating..");

        /* While releasing all allocated textures, make sure the number of entries found
         * in the map matches the total number of reusable textures that have been created.
         * A mismatch indicates texture leaks.
         */
        unsigned int n_reusable_textures_released = 0;

        if (reusable_texture_key_to_ogl_texture_vector_map != NULL)
        {
            const uint32_t n_map_vectors = system_hash64map_get_amount_of_elements(reusable_texture_key_to_ogl_texture_vector_map);

            for (uint32_t n_map_vector = 0;
                          n_map_vector < n_map_vectors;
                        ++n_map_vector)
            {
                system_resizable_vector map_vector           = NULL;
                uint32_t                n_map_vector_entries = 0;

                if (!system_hash64map_get_element_at(reusable_texture_key_to_ogl_texture_vector_map,
                                                     n_map_vector,
                                                    &map_vector,
                                                     NULL) ) /* outHash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve hash map entry at index [%d]",
                                      n_map_vector);

                    continue;
                }

                n_reusable_textures_released += system_resizable_vector_get_amount_of_elements(map_vector);

                system_resizable_vector_release(map_vector);
                map_vector = NULL;
            } /* for (all vectors stored in the map) */

            system_hash64map_release(reusable_texture_key_to_ogl_texture_vector_map);

            reusable_texture_key_to_ogl_texture_vector_map = NULL;
        } /* if (reusable_texture_key_to_ogl_texture_vector_map != NULL) */

        if (reusable_textures != NULL)
        {
            const uint32_t n_textures = system_resizable_vector_get_amount_of_elements(reusable_textures);

            ASSERT_DEBUG_SYNC(n_textures == n_reusable_textures_released,
                              "Reusable texture memory leak detected");

            for (uint32_t n_texture = 0;
                          n_texture < n_textures;
                        ++n_texture)
            {
                ogl_texture current_texture = NULL;

                if (!system_resizable_vector_get_element_at(reusable_textures,
                                                            n_texture,
                                                           &current_texture) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve re-usable texture at index [%d]",
                                      n_texture);

                    continue;
                }

                ogl_texture_release(current_texture);
                current_texture = NULL;
            } /* for (all reusable textures) */

            system_resizable_vector_release(reusable_textures);
            reusable_textures = NULL;
        }

        if (textures_by_filename != NULL)
        {
            system_hash64map_release(textures_by_filename);

            textures_by_filename = NULL;
        }

        if (textures_by_name != NULL)
        {
            system_hash64 texture_hash = 0;
            ogl_texture   texture      = NULL;

            while (system_hash64map_get_element_at(textures_by_name,
                                                   0,
                                                  &texture,
                                                  &texture_hash) )
            {
                ogl_texture_release(texture);

                system_hash64map_remove(textures_by_name,
                                        texture_hash);
            }

            system_hash64map_release(textures_by_name);

            textures_by_name = NULL;
        }
    }
} _ogl_context_textures;

typedef enum
{
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D_ARRAY,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_ARRAY,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE_ARRAY,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_3D,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_BUFFER,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP_ARRAY,
    OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_RECTANGLE
} _ogl_context_textures_key_dimensionality;

typedef enum
{
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT16,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT24,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT32,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT32F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R11F_G11F_B10F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R3_G3_B2,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB12,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB4,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5_A1,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB9_E5,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA12,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA2,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32F,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA4,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8_SNORM,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8I,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8UI,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8,
    OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8_ALPHA8
} _ogl_context_textures_key_internalformat;

/* Forward declarations */
PRIVATE GLenum                                   _ogl_context_textures_get_glenum_internalformat_for_key_internalformat(__in _ogl_context_textures_key_internalformat internalformat);
PRIVATE ogl_texture_dimensionality               _ogl_context_textures_get_glenum_dimensionality_for_key_dimensionality(__in _ogl_context_textures_key_dimensionality dimensionality);
PRIVATE _ogl_context_textures_key_dimensionality _ogl_context_textures_get_key_dimensionality_for_glenum_dimensionality(__in ogl_texture_dimensionality               dimensionality);
PRIVATE _ogl_context_textures_key_internalformat _ogl_context_textures_get_key_internalformat_for_glenum_internalformat(__in GLenum                                   internalformat);

/** Private functions */

/** TODO */
PRIVATE ogl_texture_dimensionality _ogl_context_textures_get_glenum_dimensionality_for_key_dimensionality(__in _ogl_context_textures_key_dimensionality dimensionality)
{
    ogl_texture_dimensionality result;

    switch (dimensionality)
    {
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D:                   result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D;                   break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D_ARRAY:             result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D_ARRAY;             break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D:                   result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D;                   break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_ARRAY:             result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_ARRAY;             break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE:       result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE;       break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE_ARRAY: result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE_ARRAY; break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_3D:                   result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_3D;                   break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_BUFFER:               result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_BUFFER;               break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP:             result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP;             break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP_ARRAY:       result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP_ARRAY;       break;
        case OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_RECTANGLE:            result = OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_RECTANGLE;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_textures_key_dimensionality value");
        }
    } /* switch (dimensionality) */

    return result;
}

/** TODO */
PRIVATE GLenum _ogl_context_textures_get_glenum_internalformat_for_key_internalformat(__in _ogl_context_textures_key_internalformat internalformat)
{
    GLenum result;

    switch (internalformat)
    {
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT16:  result = GL_DEPTH_COMPONENT16;  break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT24:  result = GL_DEPTH_COMPONENT24;  break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT32F: result = GL_DEPTH_COMPONENT32F; break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R11F_G11F_B10F:     result = GL_R11F_G11F_B10F;     break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16:                result = GL_R16;                break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16_SNORM:          result = GL_R16_SNORM;          break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16F:               result = GL_R16F;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16I:               result = GL_R16I;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16UI:              result = GL_R16UI;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R3_G3_B2:           result = GL_R3_G3_B2;           break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32F:               result = GL_R32F;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32I:               result = GL_R32I;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32UI:              result = GL_R32UI;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8:                 result = GL_R8;                 break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8_SNORM:           result = GL_R8_SNORM;           break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8I:                result = GL_R8I;                break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8UI:               result = GL_R8UI;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16:               result = GL_RG16;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16_SNORM:         result = GL_RG16_SNORM;         break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16F:              result = GL_RG16F;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16I:              result = GL_RG16I;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16UI:             result = GL_RG16UI;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32F:              result = GL_RG32F;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32I:              result = GL_RG32I;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32UI:             result = GL_RG32UI;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8:                result = GL_RG8;                break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8_SNORM:          result = GL_RG8_SNORM;          break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8I:               result = GL_RG8I;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8UI:              result = GL_RG8UI;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10:              result = GL_RGB10;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2:           result = GL_RGB10_A2;           break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2UI:         result = GL_RGB10_A2UI;         break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB12:              result = GL_RGB12;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16_SNORM:        result = GL_RGB16_SNORM;        break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16F:             result = GL_RGB16F;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16I:             result = GL_RGB16I;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16UI:            result = GL_RGB16UI;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32F:             result = GL_RGB32F;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32I:             result = GL_RGB32I;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32UI:            result = GL_RGB32UI;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB4:               result = GL_RGB4;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5:               result = GL_RGB5;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5_A1:            result = GL_RGB5_A1;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8:               result = GL_RGB8;               break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8_SNORM:         result = GL_RGB8_SNORM;         break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8I:              result = GL_RGB8I;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8UI:             result = GL_RGB8UI;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB9_E5:            result = GL_RGB9_E5;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA12:             result = GL_RGBA12;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16:             result = GL_RGBA16;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16F:            result = GL_RGBA16F;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16I:            result = GL_RGBA16I;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16UI:           result = GL_RGBA16UI;           break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA2:              result = GL_RGBA2;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32F:            result = GL_RGBA32F;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32I:            result = GL_RGBA32I;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32UI:           result = GL_RGBA32UI;           break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA4:              result = GL_RGBA4;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8:              result = GL_RGBA8;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8_SNORM:        result = GL_RGBA8_SNORM;        break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8I:             result = GL_RGBA8I;             break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8UI:            result = GL_RGBA8UI;            break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8:              result = GL_SRGB8;              break;
        case OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8_ALPHA8:       result = GL_SRGB8_ALPHA8;       break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized _ogl_textures_key_internalformat value");
        }
    } /* switch (internalformat) */

    return result;
}

/** TODO */
PRIVATE _ogl_context_textures_key_dimensionality _ogl_context_textures_get_key_dimensionality_for_glenum_dimensionality(__in ogl_texture_dimensionality dimensionality)
{
    _ogl_context_textures_key_dimensionality result;

    switch (dimensionality)
    {
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D:                   result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D;                   break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D_ARRAY:             result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_1D_ARRAY;             break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D:                   result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D;                   break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_ARRAY:             result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_ARRAY;             break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE:       result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE;       break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE_ARRAY: result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_2D_MULTISAMPLE_ARRAY; break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_3D:                   result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_3D;                   break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_BUFFER:               result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_BUFFER;               break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP:             result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP;             break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP_ARRAY:       result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_CUBE_MAP_ARRAY;       break;
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_RECTANGLE:            result = OGL_CONTEXT_TEXTURES_KEY_DIMENSIONALITY_RECTANGLE;            break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_texture_dimensionality value");
        }
    } /* switch (dimensionality) */

    return result;
}

/** TODO */
PRIVATE _ogl_context_textures_key_internalformat _ogl_context_textures_get_key_internalformat_for_glenum_internalformat(__in GLenum internalformat)
{
    _ogl_context_textures_key_internalformat result;

    switch (internalformat)
    {
        case GL_DEPTH_COMPONENT16:  result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT16;  break;
        case GL_DEPTH_COMPONENT24:  result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT24;  break;
        case GL_DEPTH_COMPONENT32:  result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT32;  break;
        case GL_DEPTH_COMPONENT32F: result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_DEPTH_COMPONENT32F; break;
        case GL_R11F_G11F_B10F:     result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R11F_G11F_B10F;     break;
        case GL_R16:                result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16;                break;
        case GL_R16_SNORM:          result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16_SNORM;          break;
        case GL_R16F:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16F;               break;
        case GL_R16I:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16I;               break;
        case GL_R16UI:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R16UI;              break;
        case GL_R3_G3_B2:           result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R3_G3_B2;           break;
        case GL_R32F:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32F;               break;
        case GL_R32I:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32I;               break;
        case GL_R32UI:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R32UI;              break;
        case GL_R8:                 result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8;                 break;
        case GL_R8_SNORM:           result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8_SNORM;           break;
        case GL_R8I:                result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8I;                break;
        case GL_R8UI:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_R8UI;               break;
        case GL_RG16:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16;               break;
        case GL_RG16_SNORM:         result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16_SNORM;         break;
        case GL_RG16F:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16F;              break;
        case GL_RG16I:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16I;              break;
        case GL_RG16UI:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG16UI;             break;
        case GL_RG32F:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32F;              break;
        case GL_RG32I:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32I;              break;
        case GL_RG32UI:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG32UI;             break;
        case GL_RG8:                result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8;                break;
        case GL_RG8_SNORM:          result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8_SNORM;          break;
        case GL_RG8I:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8I;               break;
        case GL_RG8UI:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RG8UI;              break;
        case GL_RGB10:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10;              break;
        case GL_RGB10_A2:           result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2;           break;
        case GL_RGB10_A2UI:         result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB10_A2UI;         break;
        case GL_RGB12:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB12;              break;
        case GL_RGB16_SNORM:        result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16_SNORM;        break;
        case GL_RGB16F:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16F;             break;
        case GL_RGB16I:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16I;             break;
        case GL_RGB16UI:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB16UI;            break;
        case GL_RGB32F:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32F;             break;
        case GL_RGB32I:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32I;             break;
        case GL_RGB32UI:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB32UI;            break;
        case GL_RGB4:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB4;               break;
        case GL_RGB5:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5;               break;
        case GL_RGB5_A1:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB5_A1;            break;
        case GL_RGB8:               result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8;               break;
        case GL_RGB8_SNORM:         result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8_SNORM;         break;
        case GL_RGB8I:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8I;              break;
        case GL_RGB8UI:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB8UI;             break;
        case GL_RGB9_E5:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGB9_E5;            break;
        case GL_RGBA12:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA12;             break;
        case GL_RGBA16:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16;             break;
        case GL_RGBA16F:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16F;            break;
        case GL_RGBA16I:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16I;            break;
        case GL_RGBA16UI:           result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA16UI;           break;
        case GL_RGBA2:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA2;              break;
        case GL_RGBA32F:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32F;            break;
        case GL_RGBA32I:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32I;            break;
        case GL_RGBA32UI:           result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA32UI;           break;
        case GL_RGBA4:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA4;              break;
        case GL_RGBA8:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8;              break;
        case GL_RGBA8_SNORM:        result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8_SNORM;        break;
        case GL_RGBA8I:             result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8I;             break;
        case GL_RGBA8UI:            result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_RGBA8UI;            break;
        case GL_SRGB8:              result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8;              break;
        case GL_SRGB8_ALPHA8:       result = OGL_CONTEXT_TEXTURES_KEY_INTERNALFORMAT_SRGB8_ALPHA8;       break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized internalformat GLenum value");
        }
    } /* switch (internalformat) */

    return result;
}

/** TODO */
PRIVATE system_hash64 _ogl_context_textures_get_reusable_texture_key(__in ogl_texture_dimensionality dimensionality,
                                                                     __in unsigned int               base_mipmap_depth,
                                                                     __in unsigned int               base_mipmap_height,
                                                                     __in unsigned int               base_mipmap_width,
                                                                     __in unsigned int               n_mipmaps,
                                                                     __in unsigned int               n_samples,
                                                                     __in GLenum                     internalformat,
                                                                     __in bool                       fixed_sample_locations)
{
    /* Key structure:
     *
     * 00-04bit: n mipmaps              (0-15)
     * 05-10bit: n samples              (0-31)
     * 11-19bit: internalformat         (0-63, internal enum that maps to a specific GLenum)
     * 20-24bit: texture dimensionality (0-15, internal enum that maps to a specific dimensionality)
     * 25-38bit: base mipmap width      (0-16383)
     * 39-52bit: base mipmap height     (0-16383)
     * 53-60bit: base mipmap depth      (0-255).
     *    61bit: fixed sample locations (0-1).
     */

    /* Some checks to make sure the crucial properties fit within the key.. */
    _ogl_context_textures_key_dimensionality key_dimensionality = _ogl_context_textures_get_key_dimensionality_for_glenum_dimensionality(dimensionality);
    _ogl_context_textures_key_internalformat key_internalformat = _ogl_context_textures_get_key_internalformat_for_glenum_internalformat(internalformat);

    ASSERT_DEBUG_SYNC((n_mipmaps          < (1 << 5)   &&
                       n_samples          < (1 << 6)   &&
                       key_internalformat < (1 << 10)  &&
                       key_dimensionality < (1 << 5)   &&
                       base_mipmap_depth  < (1 << 8)   &&
                       base_mipmap_height < (1 << 16)  &&
                       base_mipmap_width  < (1 << 16)),
                      "Texture properties overflow the texture pool key");

    return (( ((system_hash64) n_mipmaps)                      & ((1 << 5)  - 1)) << 0)  |
           (( ((system_hash64) n_samples)                      & ((1 << 6)  - 1)) << 5)  |
           (( ((system_hash64) key_internalformat)             & ((1 << 9)  - 1)) << 11) |
           (( ((system_hash64) key_dimensionality)             & ((1 << 5)  - 1)) << 20) |
           (( ((system_hash64) base_mipmap_width)              & ((1 << 14) - 1)) << 25) |
           (( ((system_hash64) base_mipmap_height)             & ((1 << 14) - 1)) << 39) |
           (( ((system_hash64) base_mipmap_depth)              & ((1 << 8)  - 1)) << 53) |
           (   (system_hash64)(fixed_sample_locations ? 1 : 0)                    << 61);
}

/** Please see header for specification */
PUBLIC void ogl_context_textures_add_texture(__in __notnull ogl_context_textures textures,
                                             __in __notnull ogl_texture          texture)
{
    system_hashed_ansi_string texture_name         = NULL;
    system_hashed_ansi_string texture_src_filename = NULL;

    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_NAME,
                            &texture_name);
    ogl_texture_get_property(texture,
                             OGL_TEXTURE_PROPERTY_SRC_FILENAME,
                            &texture_src_filename);

    system_hash64          texture_name_hash = system_hashed_ansi_string_get_hash(texture_name);
    _ogl_context_textures* textures_ptr      = (_ogl_context_textures*) textures;

    /* Make sure the texture has not already been added */
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(texture_name) > 0,
                      "Texture name is NULL");

    if (system_hash64map_contains(textures_ptr->textures_by_name,
                                  texture_name_hash) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot add texture [%s]: texture already added",
                           system_hashed_ansi_string_get_buffer(texture_name) );

        goto end;
    }

    /* Retain the object and store it */
    ogl_texture_retain(texture);

    system_hash64map_insert(textures_ptr->textures_by_name,
                            texture_name_hash,
                            texture,
                            NULL,
                            NULL);

    if (texture_src_filename != NULL)
    {
        system_hash64 texture_src_filename_hash = system_hashed_ansi_string_get_hash(texture_src_filename);

        system_hash64map_insert(textures_ptr->textures_by_filename,
                                texture_src_filename_hash,
                                texture,
                                NULL,
                                NULL);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC ogl_context_textures ogl_context_textures_create(__in __notnull ogl_context context)
{
    _ogl_context_textures* textures_ptr = new (std::nothrow) _ogl_context_textures;

    ASSERT_ALWAYS_SYNC(textures_ptr != NULL,
                       "Out of memory");

    if (textures_ptr != NULL)
    {
        textures_ptr->context = context;
    }

    return (ogl_context_textures) textures_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_from_pool(__in __notnull ogl_context                context,
                                                                          __in           ogl_texture_dimensionality dimensionality,
                                                                          __in           unsigned int               n_mipmaps,
                                                                          __in           GLenum                     internalformat,
                                                                          __in           unsigned int               base_mipmap_width,
                                                                          __in           unsigned int               base_mipmap_height,
                                                                          __in           unsigned int               base_mipmap_depth,
                                                                          __in           unsigned int               n_samples,
                                                                          __in           bool                       fixed_sample_locations)
{
    ogl_texture            result           = NULL;
    const system_hash64    texture_key      = _ogl_context_textures_get_reusable_texture_key(dimensionality,
                                                                                             base_mipmap_depth,
                                                                                             base_mipmap_height,
                                                                                             base_mipmap_width,
                                                                                             n_mipmaps,
                                                                                             n_samples,
                                                                                             internalformat,
                                                                                             fixed_sample_locations);
    _ogl_context_textures* textures_ptr     = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &textures_ptr);

    /* Is a re-usable texture already available? */
    system_resizable_vector reusable_textures = NULL;

    if (!system_hash64map_get(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                              texture_key,
                             &reusable_textures) )
    {
        /* No vector available. Let's spawn one. */
        reusable_textures = system_resizable_vector_create(4, /* capacity */
                                                           sizeof(ogl_texture) );

        system_hash64map_insert(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                                texture_key,
                                reusable_textures,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */
    }

    if (!system_resizable_vector_pop(reusable_textures,
                                    &result) )
    {
        static uint32_t spawned_textures_cnt    = 1;
               uint32_t spawned_texture_id      = 0;
               char     spawned_texture_id_text[16];

        spawned_texture_id = ::InterlockedIncrement(&spawned_textures_cnt);

        sprintf_s(spawned_texture_id_text,
                  sizeof(spawned_texture_id_text),
                  "%d",
                  spawned_texture_id);

        /* No free re-usable texture available. Spawn a new container. */
        LOG_INFO("Creating a new re-usable texture object [index:%d]..",
                 spawned_texture_id);

        result = ogl_texture_create_and_initialize(context,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("Re-usable texture ",
                                                                                                           spawned_texture_id_text),
                                                   dimensionality,
                                                   n_mipmaps,
                                                   internalformat,
                                                   base_mipmap_width,
                                                   base_mipmap_height,
                                                   base_mipmap_depth,
                                                   n_samples,
                                                   fixed_sample_locations);

        ASSERT_DEBUG_SYNC(result != NULL,
                          "ogl_texture_create_and_initialize() failed.");

        if (result != NULL)
        {
            system_resizable_vector_push(textures_ptr->reusable_textures,
                                         result);
        } /* if (result != NULL) */
    } /* if (no reusable texture available) */

    /* OK, "result" should hold the handle at this point */
    ASSERT_DEBUG_SYNC(result != NULL,
                      "Texture pool about to return a NULL ogl_texture instance");

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_textures_delete_texture(__in __notnull ogl_context_textures      textures,
                                        __in __notnull system_hashed_ansi_string texture_name)
{
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

    if (textures != NULL)
    {
        if (!system_hash64map_remove(textures_ptr->textures_by_name,
                                     system_hashed_ansi_string_get_hash(texture_name) ))
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not remove texture [%s]: not found",
                               system_hashed_ansi_string_get_buffer(texture_name) );
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_filename(__in __notnull ogl_context_textures      textures,
                                                                            __in __notnull system_hashed_ansi_string texture_filename)
{
    ogl_texture            result       = NULL;
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_filename,
                         system_hashed_ansi_string_get_hash(texture_filename),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_context_textures_get_texture_by_name(__in __notnull ogl_context_textures      textures,
                                                                        __in __notnull system_hashed_ansi_string texture_name)
{
    ogl_texture            result       = NULL;
    _ogl_context_textures* textures_ptr = (_ogl_context_textures*) textures;

    system_hash64map_get(textures_ptr->textures_by_name,
                         system_hashed_ansi_string_get_hash(texture_name),
                         &result);

    return result;
}

/** Please see header for specification */
PUBLIC void ogl_context_textures_release(__in __notnull ogl_context_textures textures)
{
    if (textures != NULL)
    {
        delete (_ogl_context_textures*) textures;

        textures = NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_context_textures_return_reusable(__in __notnull ogl_context context,
                                                             __in __notnull ogl_texture released_texture)
{
    _ogl_context_textures* textures_ptr = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &textures_ptr);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(system_resizable_vector_find(textures_ptr->reusable_textures,
                                                   released_texture) != ITEM_NOT_FOUND,
                      "Texture returned to the pool is NOT a re-usable texture!");

    /* Identify the texture key */
    unsigned int               texture_base_mipmap_depth  = 0;
    unsigned int               texture_base_mipmap_height = 0;
    unsigned int               texture_base_mipmap_width  = 0;
    ogl_texture_dimensionality texture_dimensionality     = OGL_TEXTURE_DIMENSIONALITY_UNKNOWN;
    bool                       texture_fixed_sample_locations;
    ogl_texture_internalformat texture_internalformat;
    unsigned int               texture_n_mipmaps          = 0;
    unsigned int               texture_n_samples          = 0;

    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                   &texture_base_mipmap_depth);
    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                   &texture_base_mipmap_height);
    ogl_texture_get_mipmap_property(released_texture,
                                    0, /* mipmap_level */
                                    OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                   &texture_base_mipmap_width);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_DIMENSIONALITY,
                                  &texture_dimensionality);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS,
                                  &texture_fixed_sample_locations);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_INTERNALFORMAT,
                                  &texture_internalformat);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_N_MIPMAPS,
                                  &texture_n_mipmaps);
    ogl_texture_get_property      (released_texture,
                                   OGL_TEXTURE_PROPERTY_N_SAMPLES,
                                  &texture_n_samples);

    system_hash64 reusable_texture_key = _ogl_context_textures_get_reusable_texture_key(texture_dimensionality,
                                                                                        texture_base_mipmap_depth,
                                                                                        texture_base_mipmap_height,
                                                                                        texture_base_mipmap_width,
                                                                                        texture_n_mipmaps,
                                                                                        texture_n_samples,
                                                                                        texture_internalformat,
                                                                                        texture_fixed_sample_locations);

    /* Look for the owner vector */
    system_resizable_vector owner_vector = NULL;

    if (!system_hash64map_get(textures_ptr->reusable_texture_key_to_ogl_texture_vector_map,
                              reusable_texture_key,
                             &owner_vector) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot put a reusable texture back into owner pool - owner vector not found");

        ogl_texture_release(released_texture);
        released_texture = NULL;

        goto end;
    }

    ASSERT_DEBUG_SYNC(owner_vector != NULL,
                      "Reusable texture owner vector is NULL");

    system_resizable_vector_push(owner_vector,
                                 released_texture);

    /* Finally, invalidate the texture contents */
    const ogl_context_gl_entrypoints* entrypoints = NULL;
    GLuint                            texture_id  = 0;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints);
    ogl_texture_get_property(released_texture,
                             OGL_TEXTURE_PROPERTY_ID,
                            &texture_id);

    for (unsigned int n_mipmap = 0;
                      n_mipmap < texture_n_mipmaps;
                    ++n_mipmap)
    {
        entrypoints->pGLInvalidateTexImage(texture_id,
                                           n_mipmap);
    } /* for (all mipmaps) */

end:
    ;
}