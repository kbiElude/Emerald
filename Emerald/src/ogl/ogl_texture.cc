
/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_image.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_context_textures.h"
#include "ogl/ogl_texture.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"

#define DEFAULT_MIPMAPS_AMOUNT (4)


/* Private declarations */
typedef struct
{
    unsigned int               data_size;
    ogl_texture_dimensionality dimensionality;
    unsigned int               depth;
    unsigned int               height;
    bool                       was_configured;
    unsigned int               width;
} _ogl_texture_mipmap;

typedef struct _ogl_texture
{
    ogl_context context;

    ogl_texture_dimensionality dimensionality;
    bool                       fixed_sample_locations;
    GLuint                     gl_id;
    bool                       has_been_bound;
    bool                       has_had_mipmaps_generated;
    ogl_texture_internalformat internalformat;
    system_resizable_vector    mipmaps; /* stores _ogl_texture_mipmap instances, indexed by mipmap level */
    system_hashed_ansi_string  name;
    unsigned int               n_max_mipmaps;
    unsigned int               n_samples;
    system_hashed_ansi_string  src_filename;

    gfx_image src_image;

    explicit _ogl_texture(__in __notnull ogl_context               in_context,
                          __in __notnull system_hashed_ansi_string in_name)
    {
        context                   = in_context;
        dimensionality            = OGL_TEXTURE_DIMENSIONALITY_UNKNOWN;
        fixed_sample_locations    = false;
        gl_id                     = 0;
        has_been_bound            = false;
        has_had_mipmaps_generated = false;
        internalformat            = OGL_TEXTURE_INTERNALFORMAT_UNKNOWN;
        mipmaps                   = system_resizable_vector_create(1,
                                                                   sizeof(_ogl_texture_mipmap*) );
        name                      = in_name;
        n_samples                 = 0;
        n_max_mipmaps             = 0;
        src_filename              = NULL;
    }
    REFCOUNT_INSERT_VARIABLES
} _ogl_texture;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_texture,
                               ogl_texture,
                              _ogl_texture);

/* Forward declarations */
PRIVATE ogl_texture _ogl_texture_create_base                                (__in  __notnull                ogl_context               context,
                                                                             __in  __notnull                system_hashed_ansi_string name,
                                                                             __in_opt                       system_hashed_ansi_string file_name);
PRIVATE void        _ogl_texture_create_from_gfx_image_renderer_callback    (__in  __notnull                ogl_context               context,
                                                                             __in                           void*                     texture);
PRIVATE void        _ogl_texture_deinit_renderer_callback                   (__in  __notnull                ogl_context               context,
                                                                             __in                           void*                     texture);
PRIVATE void        _ogl_texture_generate_mipmaps                           (__in  __notnull                ogl_context               context,
                                                                             __in  __notnull                void*                     texture);
PRIVATE void        _ogl_texture_get_texture_format_type_from_internalformat(__in                           GLenum                    internalformat,
                                                                             __out __notnull                GLenum*                   out_format,
                                                                             __out __notnull                GLenum*                   out_type);
PRIVATE void        _ogl_texture_init_renderer_callback                     (__in  __notnull                ogl_context               context,
                                                                             __in                           void*                     texture);
PRIVATE void        _ogl_texture_mipmap_deinit                              (__in  __notnull                _ogl_texture_mipmap*      mipmap_ptr);
PRIVATE void        _ogl_texture_release                                    (__in  __notnull __post_invalid void*                     arg);


/** TODO */
PRIVATE ogl_texture _ogl_texture_create_base(__in __notnull ogl_context               context,
                                             __in __notnull system_hashed_ansi_string name,
                                             __in_opt       system_hashed_ansi_string file_name = NULL)
{
    _ogl_texture* new_texture = new (std::nothrow) _ogl_texture(context,
                                                                name);

    if (new_texture != NULL)
    {
        ogl_context_request_callback_from_context_thread(context,
                                                         _ogl_texture_init_renderer_callback,
                                                         new_texture);

        new_texture->src_filename = file_name;

        /* Add the texture object to texture manager */
        ogl_context_textures textures = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TEXTURES,
                                &textures);

        ogl_context_textures_add_texture(textures,
                                         (ogl_texture) new_texture);

        /* Initialize reference counting */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_texture,
                                                       _ogl_texture_release,
                                                       OBJECT_TYPE_OGL_TEXTURE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Textures\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_texture) new_texture;
}

/** TODO */
PRIVATE void _ogl_texture_create_from_gfx_image_renderer_callback(__in __notnull ogl_context context,
                                                                  __in           void*       texture)
{
    ogl_context_type                               context_type                         = OGL_CONTEXT_TYPE_UNDEFINED;
    PFNWRAPPEDGLCOMPRESSEDTEXTURESUBIMAGE2DEXTPROC gl_pGLCompressedTextureSubImage2DEXT = NULL;
    PFNWRAPPEDGLTEXTURESTORAGE2DEXTPROC            gl_pGLTextureStorage2DEXT            = NULL;
    PFNWRAPPEDGLTEXTURESUBIMAGE2DEXTPROC           gl_pGLTextureSubImage2DEXT           = NULL;
    PFNGLBINDTEXTUREPROC                           pGLBindTexture                       = NULL;
    PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC               pGLCompressedTexSubImage2D           = NULL;
    PFNGLPIXELSTOREIPROC                           pGLPixelStorei                       = NULL;
    PFNGLTEXSTORAGE2DPROC                          pGLTexStorage2D                      = NULL;
    PFNGLTEXSUBIMAGE2DPROC                         pGLTexSubImage2D                     = NULL;
    unsigned int                                   n_mipmaps                            = 0;
    _ogl_texture*                                  texture_ptr                          = (_ogl_texture*) texture;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entry_points = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entry_points);

        pGLBindTexture             = entry_points->pGLBindTexture;
        pGLCompressedTexSubImage2D = entry_points->pGLCompressedTexSubImage2D;
        pGLPixelStorei             = entry_points->pGLPixelStorei;
        pGLTexStorage2D            = entry_points->pGLTexStorage2D;
        pGLTexSubImage2D           = entry_points->pGLTexSubImage2D;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
        const ogl_context_gl_entrypoints*                         entry_points     = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entry_points);
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                                &dsa_entry_points);

        gl_pGLCompressedTextureSubImage2DEXT = dsa_entry_points->pGLCompressedTextureSubImage2DEXT;
        gl_pGLTextureStorage2DEXT            = dsa_entry_points->pGLTextureStorage2DEXT;
        gl_pGLTextureSubImage2DEXT           = dsa_entry_points->pGLTextureSubImage2DEXT;
        pGLPixelStorei                       = entry_points->pGLPixelStorei;
    }

    gfx_image_get_property(texture_ptr->src_image,
                           GFX_IMAGE_PROPERTY_N_MIPMAPS,
                          &n_mipmaps);

    /* Retrieve base mip-map details. */
    unsigned int base_image_height         = 0;
    bool         base_image_is_compressed  = false;
    unsigned int base_image_row_alignment  = 0;
    unsigned int base_image_width          = 0;

    gfx_image_get_mipmap_property(texture_ptr->src_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                 &base_image_height);
    gfx_image_get_mipmap_property(texture_ptr->src_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_INTERNALFORMAT,
                                 &texture_ptr->internalformat);
    gfx_image_get_mipmap_property(texture_ptr->src_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED,
                                 &base_image_is_compressed);
    gfx_image_get_mipmap_property(texture_ptr->src_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT,
                                 &base_image_row_alignment);
    gfx_image_get_mipmap_property(texture_ptr->src_image,
                                  0, /* n_mipmap */
                                  GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                 &base_image_width);

    /* Set up texture storage */
    ASSERT_DEBUG_SYNC(base_image_row_alignment == 1 || base_image_row_alignment == 2 ||
                      base_image_row_alignment == 4 || base_image_row_alignment == 8,
                      "Invalid gfx_image row alignment");

    pGLPixelStorei(GL_UNPACK_ALIGNMENT,
                   base_image_row_alignment);

    /* Use immutable storage to avoid texture completeness checks during draw calls */
    const unsigned int levels = 1 + log2_uint32( (base_image_width > base_image_height) ? base_image_width : base_image_height);

    texture_ptr->n_max_mipmaps = levels;

    if (context_type == OGL_CONTEXT_TYPE_GL)
    {
        gl_pGLTextureStorage2DEXT((ogl_texture) texture,
                                  GL_TEXTURE_2D,
                                  levels,
                                  texture_ptr->internalformat,
                                  base_image_width,
                                  base_image_height);
    }
    else
    {
        pGLBindTexture (GL_TEXTURE_2D,
                        texture_ptr->gl_id);
        pGLTexStorage2D(GL_TEXTURE_2D,
                        levels,
                        texture_ptr->internalformat,
                        base_image_width,
                        base_image_height);
    }

    /* Set the mip-maps */
    unsigned int expected_mipmap_height = base_image_height;
    unsigned int expected_mipmap_width  = base_image_width;
    GLenum       texture_format         = GL_NONE;
    GLenum       texture_type           = GL_NONE;

    if (!base_image_is_compressed)
    {
        _ogl_texture_get_texture_format_type_from_internalformat(texture_ptr->internalformat,
                                                                &texture_format,
                                                                &texture_type);
    }

    for (unsigned int n_mipmap = 0;
                      n_mipmap < n_mipmaps;
                    ++n_mipmap)
    {
        const unsigned char* image_data_ptr       = NULL;
        unsigned int         image_data_size      = 0;
        unsigned int         image_height         = 0;
        GLenum               image_internalformat = GL_NONE;
        bool                 image_is_compressed  = false;
        unsigned int         image_row_alignment  = 0;
        unsigned int         image_width          = 0;

        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
                                     &image_data_ptr);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_DATA_SIZE,
                                     &image_data_size);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
                                     &image_height);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_INTERNALFORMAT,
                                     &image_internalformat);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED,
                                     &image_is_compressed);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT,
                                     &image_row_alignment);
        gfx_image_get_mipmap_property(texture_ptr->src_image,
                                      n_mipmap,
                                      GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,
                                     &image_width);

        /* Sanity checks */
        ASSERT_DEBUG_SYNC(texture_ptr->internalformat == image_internalformat,
                          "Non-base mipmap does not use the same internalformat as the base mipmap");
        ASSERT_DEBUG_SYNC(base_image_row_alignment  == image_row_alignment,
                          "Non-base mipmap does not use the same row alignment as the base mipmap");

        ASSERT_DEBUG_SYNC(image_width == expected_mipmap_width,
                          "Invalid mipmap width");
        ASSERT_DEBUG_SYNC(image_height == expected_mipmap_height,
                          "Invalid mipmap height");

        /* Set the mipmap contents */
        if (image_is_compressed)
        {
            if (context_type == OGL_CONTEXT_TYPE_ES)
            {
                pGLCompressedTexSubImage2D(GL_TEXTURE_2D,
                                           n_mipmap,
                                           0, /* xoffset */
                                           0, /* yoffset */
                                           image_width,
                                           image_height,
                                           image_internalformat,
                                           image_data_size,
                                           image_data_ptr);
            }
            else
            {
                gl_pGLCompressedTextureSubImage2DEXT((ogl_texture) texture,
                                                     GL_TEXTURE_2D,
                                                     n_mipmap,
                                                     0, /* xoffset */
                                                     0, /* yoffset */
                                                     image_width,
                                                     image_height,
                                                     image_internalformat,
                                                     image_data_size,
                                                     image_data_ptr);
            }
        } /* if (image_is_compressed) */
        else
        {
            if (context_type == OGL_CONTEXT_TYPE_ES)
            {
                pGLTexSubImage2D(GL_TEXTURE_2D,
                                 n_mipmap,
                                 0, /* xoffset */
                                 0, /* yoffset */
                                 image_width,
                                 image_height,
                                 texture_format,
                                 texture_type,
                                 image_data_ptr);
            }
            else
            {
                gl_pGLTextureSubImage2DEXT((ogl_texture) texture,
                                           GL_TEXTURE_2D,
                                           n_mipmap,
                                           0, /* xoffset */
                                           0, /* yoffset */
                                           image_width,
                                           image_height,
                                           texture_format,
                                           texture_type,
                                           image_data_ptr);
            }
        }

        expected_mipmap_width  = max(1, expected_mipmap_width  / 2);
        expected_mipmap_height = max(1, expected_mipmap_height / 2);
    } /* for (all mipmaps) */

    /* OK, we no longer need the gfx_image instance at this point */
    gfx_image_release(texture_ptr->src_image);
}

/** TODO */
PRIVATE void _ogl_texture_deinit_renderer_callback(__in __notnull ogl_context context,
                                                   __in           void*       texture)
{
    _ogl_texture* texture_ptr = (_ogl_texture*) texture;

    /* Delete the texture object */
    ogl_context_type        context_type      = OGL_CONTEXT_TYPE_UNDEFINED;
    PFNGLDELETETEXTURESPROC pGLDeleteTextures = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entrypoints = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entrypoints);

        pGLDeleteTextures = entrypoints->pGLDeleteTextures;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        const ogl_context_gl_entrypoints* entrypoints = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints);

        pGLDeleteTextures = entrypoints->pGLDeleteTextures;
    }

    pGLDeleteTextures(1,
                     &texture_ptr->gl_id);

    texture_ptr->gl_id = 0;
}

/** TODO */
PRIVATE void _ogl_texture_generate_mipmaps(ogl_context context,
                                           void*       texture)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entrypoints = NULL;
    _ogl_texture*                                             texture_ptr     = (_ogl_texture*) texture;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entrypoints);

    dsa_entrypoints->pGLGenerateTextureMipmapEXT( (ogl_texture) texture,
                                                  GL_TEXTURE_2D);
    dsa_entrypoints->pGLTextureParameteriEXT    ( (ogl_texture) texture,
                                                  GL_TEXTURE_2D,
                                                  GL_TEXTURE_MAX_LEVEL,
                                                  texture_ptr->n_max_mipmaps);

    texture_ptr->has_had_mipmaps_generated = true;
}

/** TODO */
PRIVATE void _ogl_texture_get_texture_format_type_from_internalformat(__in            GLenum  internalformat,
                                                                      __out __notnull GLenum* out_format,
                                                                      __out __notnull GLenum* out_type)
{
    switch (internalformat)
    {
        case GL_RGB8:
        case GL_SRGB8:
        {
            *out_format = GL_RGB;
            *out_type   = GL_UNSIGNED_BYTE;

            break;
        }

        case GL_RGBA32F:
        {
            *out_format = GL_RGBA;
            *out_type   = GL_FLOAT;

            break;
        }

        case GL_RGBA8:
        {
            *out_format = GL_RGBA;
            *out_type   = GL_UNSIGNED_BYTE;

            break;
        }

        default:
        {
            /* TODO: Add support for any compressed texture formats we use in the future */
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized image internal format");
        } /* default:*/
    }
} /* if (!image_is_compressed) */

/** TODO */
PRIVATE void _ogl_texture_init_renderer_callback(__in __notnull ogl_context context,
                                                 __in           void*       texture)
{
    ogl_context_type     context_type   = OGL_CONTEXT_TYPE_UNDEFINED;
    PFNGLGENTEXTURESPROC pGLGenTextures = NULL;
    _ogl_texture*        texture_ptr    = (_ogl_texture*) texture;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_TYPE,
                            &context_type);

    if (context_type == OGL_CONTEXT_TYPE_ES)
    {
        const ogl_context_es_entrypoints* entrypoints = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_ES,
                                &entrypoints);

        pGLGenTextures = entrypoints->pGLGenTextures;
    }
    else
    {
        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "Unrecognized context type");

        const ogl_context_gl_entrypoints* entrypoints = NULL;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints);

        pGLGenTextures = entrypoints->pGLGenTextures;
    }

    /* Generate a new texture id */
    pGLGenTextures(1, &texture_ptr->gl_id);
}

/** TODO */
PRIVATE void _ogl_texture_mipmap_deinit(__in __notnull _ogl_texture_mipmap* mipmap_ptr)
{
    /* Nothing to do */
}

/* TODO */
PRIVATE void _ogl_texture_release(__in __notnull __post_invalid void* arg)
{
    _ogl_texture* texture_ptr = (_ogl_texture*) arg;

    /* Request a renderer thread call-back */
    ogl_context_request_callback_from_context_thread(texture_ptr->context,
                                                     _ogl_texture_deinit_renderer_callback,
                                                     texture_ptr);

    /* Inform the textures manager that we're about to go down */
    ogl_context_textures textures = NULL;

    ogl_context_get_property(texture_ptr->context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &textures);

    ogl_context_textures_delete_texture(textures,
                                        texture_ptr->name);

    /* Release other sub-instances */
    system_resizable_vector_release(texture_ptr->mipmaps);
}

/* Please see header for specification */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL ogl_texture ogl_texture_create_and_initialize(__in __notnull ogl_context                context,
                                                                                        __in __notnull system_hashed_ansi_string  name,
                                                                                        __in           ogl_texture_dimensionality dimensionality,
                                                                                        __in           unsigned int               n_mipmaps,
                                                                                        __in           GLenum                     internalformat,
                                                                                        __in           unsigned int               base_mipmap_width,
                                                                                        __in           unsigned int               base_mipmap_height,
                                                                                        __in           unsigned int               base_mipmap_depth,
                                                                                        __in           unsigned int               n_samples,
                                                                                        __in           bool                       fixed_sample_locations)
{
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    ogl_texture                                               result           = NULL;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    #ifdef _DEBUG
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
            "Only GL contexts are supported by ogl_textures_get_texture_from_pool()");
    }
    #endif

    result = _ogl_texture_create_base(context,
                                      name);

    if (result == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_texture_create() failure.");

        goto end;
    }

    switch (dimensionality)
    {
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D:
        {
            dsa_entry_points->pGLTextureStorage1DEXT(result,
                                                     dimensionality,
                                                     n_mipmaps,
                                                     internalformat,
                                                     base_mipmap_width);

            break;
        }

        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_1D_ARRAY:
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D:
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP:
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_RECTANGLE:
        {
            dsa_entry_points->pGLTextureStorage2DEXT(result,
                                                     dimensionality,
                                                     n_mipmaps,
                                                     internalformat,
                                                     base_mipmap_width,
                                                     base_mipmap_height);

            break;
        }

        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE:
        {
            dsa_entry_points->pGLTextureStorage2DMultisampleEXT(result,
                                                                dimensionality,
                                                                n_samples,
                                                                internalformat,
                                                                base_mipmap_width,
                                                                base_mipmap_height,
                                                                fixed_sample_locations);

            break;
        }

        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_ARRAY:
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_3D:
        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_CUBE_MAP_ARRAY:
        {
            dsa_entry_points->pGLTextureStorage3DEXT(result,
                                                     dimensionality,
                                                     n_mipmaps,
                                                     internalformat,
                                                     base_mipmap_width,
                                                     base_mipmap_height,
                                                     base_mipmap_depth);

            break;
        }

        case OGL_TEXTURE_DIMENSIONALITY_GL_TEXTURE_2D_MULTISAMPLE_ARRAY:
        {
            dsa_entry_points->pGLTextureStorage3DMultisampleEXT(result,
                                                                dimensionality,
                                                                n_samples,
                                                                internalformat,
                                                                base_mipmap_width,
                                                                base_mipmap_height,
                                                                base_mipmap_depth,
                                                                fixed_sample_locations);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized texture dimensionality");
        }
    } /* switch (dimensionality) */

    /* Store the properties */
    _ogl_texture* result_ptr = (_ogl_texture*) result;

    result_ptr->dimensionality         = dimensionality;
    result_ptr->fixed_sample_locations = fixed_sample_locations;
    result_ptr->internalformat         = (ogl_texture_internalformat) internalformat;
    result_ptr->n_samples              = n_samples;

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_empty(__in __notnull ogl_context               context,
                                                        __in __notnull system_hashed_ansi_string name)
{
    return _ogl_texture_create_base(context,
                                    name);
}

/* Please see header for specification */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_file_name(__in __notnull ogl_context               context,
                                                                 __in __notnull system_hashed_ansi_string name,
                                                                 __in __notnull system_hashed_ansi_string src_filename)
{
    ASSERT_DEBUG_SYNC (src_filename != NULL,
                       "Source filename is NULL");

    /* Create a new gfx_image instance for the input file name */
    gfx_image new_gfx_image = gfx_image_create_from_file(name,
                                                         src_filename,
                                                         true); /* use_alternative_filename_getter */

    ASSERT_ALWAYS_SYNC(new_gfx_image != NULL,
                       "gfx_image_create_from_file() called failed.");

    /* Use one of the other constructors to set up the ogl_texture instance */
    _ogl_texture* result = (_ogl_texture*) ogl_texture_create_from_gfx_image(context,
                                                                             new_gfx_image,
                                                                             name);

    /* All done */
    gfx_image_release(new_gfx_image); /* ownership taken by ogl_texture */

    return (ogl_texture) result;
}

/* Please see header for specificaiton */
PUBLIC EMERALD_API ogl_texture ogl_texture_create_from_gfx_image(__in __notnull ogl_context               context,
                                                                 __in __notnull gfx_image                 image,
                                                                 __in __notnull system_hashed_ansi_string name)
{
    ogl_texture result = NULL;

    if (image == NULL)
    {
        ASSERT_ALWAYS_SYNC(image != NULL,
                           "Cannot create an OGL texture instance from a NULL GFX image instance");

        goto end;
    }

    /* Retrieve image file name - we'll store it in ogl_texture instance
     * in order to make sure we do not load the same image more than once.
     */
    system_hashed_ansi_string image_filename = NULL;

    gfx_image_get_property(image,
                           GFX_IMAGE_PROPERTY_FILENAME,
                          &image_filename);

    /* First create a regular texture container */
    result = _ogl_texture_create_base(context,
                                      name,
                                      image_filename);

    if (result == NULL)
    {
        ASSERT_ALWAYS_SYNC(result != NULL,
                           "Could not create an empty texture container");

        goto end;
    }

    /* Set the base mip-map - we need a renderer thread call-back to make this happen. */
    ((_ogl_texture*) result)->src_image = image;

    gfx_image_retain(image);

    ogl_context_request_callback_from_context_thread(context,
                                                     _ogl_texture_create_from_gfx_image_renderer_callback,
                                                     result);

    /* Done */
end:
    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_texture_generate_mipmaps(__in __notnull ogl_texture texture)
{
    _ogl_texture* texture_ptr = (_ogl_texture*) texture;

    ogl_context_request_callback_from_context_thread(texture_ptr->context,
                                                     _ogl_texture_generate_mipmaps,
                                                     texture);

}

/* Please see header for spec */
PUBLIC EMERALD_API bool ogl_texture_get_mipmap_property(__in  __notnull ogl_texture                 texture,
                                                        __in            unsigned int                mipmap_level,
                                                        __in            ogl_texture_mipmap_property property_value,
                                                        __out __notnull void*                       out_result)
{
    _ogl_texture* texture_ptr         = (_ogl_texture*) texture;
    unsigned int  n_mipmaps_allocated = system_resizable_vector_get_amount_of_elements(texture_ptr->mipmaps);
    bool          result              = false;

    /* Retrieve mipmap container for requested index */
    _ogl_texture_mipmap* mipmap_ptr = NULL;

    if (system_resizable_vector_get_element_at(texture_ptr->mipmaps, (size_t) mipmap_level, &mipmap_ptr) )
    {
        ASSERT_DEBUG_SYNC(mipmap_ptr != NULL, "Mipmap container is NULL");

        switch (property_value)
        {
            case OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE:
            {
                ASSERT_DEBUG_SYNC(mipmap_ptr->data_size != 0,
                                  "Mipmap data size is 0"); /* only valid for decompressed textures - you shouldn't be using this query for those */

                *(unsigned int*) out_result = mipmap_ptr->data_size;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH:
            {
                *(unsigned int*) out_result = mipmap_ptr->depth;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT:
            {
                *(unsigned int*) out_result = mipmap_ptr->height;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH:
            {
                *(unsigned int*) out_result = mipmap_ptr->width;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ogl_texture mipmap property");
            }
        } /* switch (property_value) */

        result = true;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve mipmap container for mipmap level [%d]",
                          mipmap_level);
    }

    return result;
}

/* Please see header for spec */
PUBLIC EMERALD_API void ogl_texture_get_property(__in  __notnull const ogl_texture    texture,
                                                 __in            ogl_texture_property property,
                                                 __out           void*                out_result)
{
    const _ogl_texture* texture_ptr = (const _ogl_texture*) texture;

    if (texture_ptr == NULL)
    {
        if (property == OGL_TEXTURE_PROPERTY_ID)
        {
            *((GLuint*) out_result) = 0;
        }

        goto end;
    }

    switch (property)
    {
        case OGL_TEXTURE_PROPERTY_DIMENSIONALITY:
        {
            *(ogl_texture_dimensionality*) out_result = texture_ptr->dimensionality;

            break;
        }

        case OGL_TEXTURE_PROPERTY_FIXED_SAMPLE_LOCATIONS:
        {
            *(bool*) out_result = texture_ptr->fixed_sample_locations;

            break;
        }

        case OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND:
        {
            *((bool*) out_result) = texture_ptr->has_been_bound;

            break;
        }

        case OGL_TEXTURE_PROPERTY_HAS_HAD_MIPMAPS_GENERATED:
        {
            *((bool*) out_result) = texture_ptr->has_had_mipmaps_generated;

            break;
        }

        case OGL_TEXTURE_PROPERTY_INTERNALFORMAT:
        {
            *(GLint*) out_result = texture_ptr->internalformat;

            break;
        }

        case OGL_TEXTURE_PROPERTY_ID:
        {
            *((GLuint*) out_result) = texture_ptr->gl_id;

            break;
        }

        case OGL_TEXTURE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result) = texture_ptr->name;

            break;
        }

        case OGL_TEXTURE_PROPERTY_N_MIPMAPS:
        {
            unsigned int       n_defined_mipmaps = 0;
            const unsigned int n_mipmaps         = system_resizable_vector_get_amount_of_elements(texture_ptr->mipmaps);

            for (unsigned int n_mipmap = 0;
                              n_mipmap < n_mipmaps;
                              n_mipmap ++)
            {
                _ogl_texture_mipmap* mipmap_ptr = NULL;

                if (system_resizable_vector_get_element_at(texture_ptr->mipmaps,
                                                           n_mipmap,
                                                          &mipmap_ptr) )
                {
                    if (mipmap_ptr->was_configured)
                    {
                        n_defined_mipmaps++;
                    }
                }
            } /* for (all mipmaps) */

            *(unsigned int*) out_result = n_defined_mipmaps;

            break;
        }

        case OGL_TEXTURE_PROPERTY_N_SAMPLES:
        {
            *((unsigned int*) out_result) = texture_ptr->n_samples;

            break;
        }

        case OGL_TEXTURE_PROPERTY_SRC_FILENAME:
        {
            *((system_hashed_ansi_string*) out_result) = texture_ptr->src_filename;

            break;
        }

        case OGL_TEXTURE_PROPERTY_TARGET:
        {
            *((ogl_texture_dimensionality*) out_result) = texture_ptr->dimensionality;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_texture property requested");
        }
    } /* switch (property) */

end:
    ;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ogl_texture_set_mipmap_property(__in __notnull ogl_texture                 texture,
                                                        __in           unsigned int                n_mipmap,
                                                        __in           ogl_texture_mipmap_property property_value,
                                                        __in           void*                       value_ptr)
{
    _ogl_texture* texture_ptr         = (_ogl_texture*) texture;
    unsigned int  n_mipmaps_allocated = system_resizable_vector_get_amount_of_elements(texture_ptr->mipmaps);

    if (n_mipmaps_allocated <= n_mipmap)
    {
        ASSERT_DEBUG_SYNC(n_mipmap < 128,
                          "Insane mipmap level [%d] about to be used",
                          n_mipmap);

        LOG_INFO("Performance hit: need to allocate space for mipmap storage [index:%d]",
                 n_mipmap);

        for (;
             n_mipmaps_allocated <= n_mipmap;
           ++n_mipmaps_allocated)
        {
            _ogl_texture_mipmap* mipmap = new (std::nothrow) _ogl_texture_mipmap;

            ASSERT_ALWAYS_SYNC(mipmap != NULL,
                               "Out of memory");

            if (mipmap != NULL)
            {
                memset(mipmap,
                       0,
                       sizeof(*mipmap) );

                system_resizable_vector_push(texture_ptr->mipmaps,
                                             mipmap);
            } /* if (mipmap != NULL) */
        } /* for (; n_mipmaps_allocated < n_needed_mipmaps; ++n_mipmaps_allocated) */
    } /* if (n_mipmaps_allocated < mipmap_level) */

    /* Retrieve mipmap container for requested index */
    _ogl_texture_mipmap* mipmap_ptr = NULL;

    if (system_resizable_vector_get_element_at(texture_ptr->mipmaps,
                                               (size_t) n_mipmap,
                                              &mipmap_ptr) )
    {
        ASSERT_DEBUG_SYNC(mipmap_ptr != NULL,
                          "Mipmap container is NULL");

        switch (property_value)
        {
            case OGL_TEXTURE_MIPMAP_PROPERTY_DATA_SIZE:
            {
                mipmap_ptr->data_size = *(unsigned int*) value_ptr;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_DEPTH:
            {
                mipmap_ptr->depth = *(unsigned int*) value_ptr;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_HEIGHT:
            {
                mipmap_ptr->height = *(unsigned int*) value_ptr;

                break;
            }

            case OGL_TEXTURE_MIPMAP_PROPERTY_WIDTH:
            {
                mipmap_ptr->width = *(unsigned int*) value_ptr;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized texture mipmap property");
            }
        } /* switch (property_value) */

        mipmap_ptr->was_configured = true;
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve mipmap container for mipmap level [%d]",
                          n_mipmap);
    }
}

/** Please see header for spec */
PUBLIC EMERALD_API void ogl_texture_set_property(__in  __notnull ogl_texture          texture,
                                                 __in            ogl_texture_property property,
                                                 __in            void*                data)
{
    _ogl_texture* texture_ptr = (_ogl_texture*) texture;

    switch (property)
    {
        case OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND:
        {
            ASSERT_DEBUG_SYNC( *(bool*) data == true,
                               "OGL_TEXTURE_PROPERTY_HAS_BEEN_BOUND about to be set to a non-true value!");

            texture_ptr->has_been_bound = *(bool*) data;
            break;
        }

        case OGL_TEXTURE_PROPERTY_INTERNALFORMAT:
        {
            /* Emerald only supports immutable textures. */
            ASSERT_DEBUG_SYNC(texture_ptr->internalformat == OGL_TEXTURE_INTERNALFORMAT_UNKNOWN ||
                              texture_ptr->internalformat == *(GLint*) data,
                              "Invalid setter call!");

            texture_ptr->internalformat = *(ogl_texture_internalformat*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ogl_texture_property value");
        }
    } /* switch (property) */
}
