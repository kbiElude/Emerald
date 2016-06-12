/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Image object implementation.
 */
#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

enum gfx_image_property
{
    /* global, settable, PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR */
    GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_FUNC_PTR,

    /* global, settable, void* */
    GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_USER_ARG,

    /* not settable, system_hashed_ansi_string */
    GFX_IMAGE_PROPERTY_FILENAME,

    /* not settable, system_hashed_ansi_string */
    GFX_IMAGE_PROPERTY_FILENAME_ACTUAL,

    /* not settable, unsigned int */
    GFX_IMAGE_PROPERTY_N_MIPMAPS,

    /* not settable, system_hashed_ansi_string */
    GFX_IMAGE_PROPERTY_NAME,

    /* Always last */
    GFX_IMAGE_PROPERTY_COUNT
};

enum gfx_image_mipmap_property
{
    GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER,
    GFX_IMAGE_MIPMAP_PROPERTY_DATA_SIZE,
    GFX_IMAGE_MIPMAP_PROPERTY_DATA_TYPE,
    GFX_IMAGE_MIPMAP_PROPERTY_FORMAT_RAL,
    GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT,
    GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED,
    GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT,
    GFX_IMAGE_MIPMAP_PROPERTY_WIDTH,

    /* Always last */
    GFX_IMAGE_MIPMAP_PROPERTY_COUNT
};

REFCOUNT_INSERT_DECLARATIONS(gfx_image, gfx_image)

typedef system_hashed_ansi_string (*PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR)(void*                     user_arg,
                                                                              system_hashed_ansi_string decompressed_filename,
                                                                              ral_format*               out_compressed_texture_format,  /* can be NULL if the result is not a compressed filename */
                                                                              system_file_unpacker*     out_file_unpacker);             /* can be NULL if the file does not come from a file unpacker */

/** TODO */
PUBLIC unsigned int gfx_image_add_mipmap(gfx_image             image,
                                         unsigned int          width,
                                         unsigned int          height,
                                         unsigned int          row_alignment,
                                         ral_format            format,
                                         bool                  is_compressed,
                                         const unsigned char*  data_ptr,
                                         unsigned int          data_size,
                                         ral_texture_data_type data_type,
                                         bool                  should_cache_data_ptr,
                                         bool                  should_release_cached_data = false);

/** Creates a new instance of gfx_image.
 *
 *  @return gfx_image instance or NULL if call failed.
 */
PUBLIC gfx_image gfx_image_create(system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API gfx_image gfx_image_create_from_file(system_hashed_ansi_string name,
                                                        system_hashed_ansi_string file_name,
                                                        bool                      use_alternative_filename_getter);

/** Retrieves number of bytes needed for data stored with specific combination of image properties.
 *
 *  @param format        RAL texture format
 *  @param width         Image width
 *  @param height        Image height
 *  @param row_alignment Row alignment.
 *
 *  @return Amount of bytes given image configuration needs to store full picture.
 **/
PUBLIC unsigned int gfx_image_get_data_size(ral_format   format,
                                            unsigned int width,
                                            unsigned int height,
                                            unsigned int row_alignment);

/** TODO */
PUBLIC EMERALD_API bool gfx_image_get_mipmap_property(gfx_image                 image,
                                                      unsigned int              n_mipmap,
                                                      gfx_image_mipmap_property mipmap_property,
                                                      void*                     out_result);

/** TODO */
PUBLIC EMERALD_API void gfx_image_get_property(const gfx_image          image,
                                                     gfx_image_property property,
                                                     void*              out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void gfx_image_set_global_property(gfx_image_property property_value,
                                                      void*              value);

#endif /* GFX_IMAGE_H */
