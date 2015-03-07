/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 * Image object implementation.
 */
#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H

#include "gfx/gfx_types.h"
#include "system/system_types.h"

enum gfx_image_property
{
    GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_FUNC_PTR, /* global,     settable, PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR */
    GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_USER_ARG, /* global,     settable, void* */
    GFX_IMAGE_PROPERTY_FILENAME,                               /*         not settable, system_hashed_ansi_string */
    GFX_IMAGE_PROPERTY_FILENAME_ACTUAL,                        /*         not settable, system_hashed_ansi_string */
    GFX_IMAGE_PROPERTY_N_MIPMAPS,                              /*         not settable, unsigned int */
    GFX_IMAGE_PROPERTY_NAME,                                   /*         not settable, system_hashed_ansi_string */

    /* Always last */
    GFX_IMAGE_PROPERTY_COUNT
};

enum gfx_image_mipmap_property
{
    GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
    GFX_IMAGE_MIPMAP_PROPERTY_DATA_SIZE,
    GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
    GFX_IMAGE_MIPMAP_PROPERTY_INTERNALFORMAT,
    GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED,
    GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT,
    GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,

    /* Always last */
    GFX_IMAGE_MIPMAP_PROPERTY_COUNT
};

REFCOUNT_INSERT_DECLARATIONS(gfx_image, gfx_image)

typedef system_hashed_ansi_string (*PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR)(void*                     user_arg,
                                                                              system_hashed_ansi_string decompressed_filename,
                                                                              GLenum*                   out_compressed_gl_enum,  /* can be NULL if the result is not a compressed filename */
                                                                              system_file_unpacker*     out_file_unpacker);      /* can be NULL if the file does not come from a file unpacker */

/** TODO */
PUBLIC unsigned int gfx_image_add_mipmap(__in __notnull                   gfx_image            image,
                                         __in                             unsigned int         width,
                                         __in                             unsigned int         height,
                                         __in                             unsigned int         row_alignment,
                                         __in                             GLenum               internalformat,
                                         __in                             bool                 is_compressed,
                                         __in_bcount(data_size) __notnull const unsigned char* data_ptr,
                                         __in                             unsigned int         data_size,
                                         __in                             bool                 should_cache_data_ptr,
                                         __in                             bool                 should_release_cached_data = false);

/** Creates a new instance of gfx_image.
 *
 *  @return gfx_image instance or NULL if call failed.
 */
PUBLIC gfx_image gfx_image_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_image_create_from_file(__in __notnull system_hashed_ansi_string name,
                                                        __in __notnull system_hashed_ansi_string file_name,
                                                        __in           bool                      use_alternative_filename_getter);

/** Retrieves amount of bytes needed for data stored with specific combination of image properties.
 *
 *  @param GLenum       TODO
 *  @param unsigned int Image width
 *  @param unsigned int Image height
 *  @param unsigned int Row alignment.
 *
 *  @return Amount of bytes given image configuration needs to store full picture.
 **/
PUBLIC unsigned int gfx_image_get_data_size(__in GLenum       internalformat,
                                            __in unsigned int width,
                                            __in unsigned int height,
                                            __in unsigned int row_alignment);

/** TODO */
PUBLIC EMERALD_API bool gfx_image_get_mipmap_property(__in  __notnull gfx_image                 image,
                                                      __in            unsigned int              n_mipmap,
                                                      __in            gfx_image_mipmap_property mipmap_property,
                                                      __out __notnull void*                     out_result);

/** TODO */
PUBLIC EMERALD_API void gfx_image_get_property(__in __notnull const gfx_image          image,
                                               __in                 gfx_image_property property,
                                               __in __notnull       void*              out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void gfx_image_set_global_property(__in     gfx_image_property property_value,
                                                      __in_opt void*              value);

#endif /* GFX_IMAGE_H */
