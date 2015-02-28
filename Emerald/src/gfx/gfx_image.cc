/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "gfx/gfx_bmp.h"
#include "gfx/gfx_image.h"
#include "gfx/gfx_jpg.h"
#include "gfx/gfx_png.h"
#include "gfx/gfx_rgbe.h"
#include "gfx/gfx_types.h"
#include "ogl/ogl_context_texture_compression.h"
#include "system/system_assertions.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Global variables.
 *
 *  Yeah, yeah, globals suck. But assuming demo environment, this solution
 *  will do just fine.
 */
volatile PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR _alternative_filename_getter_proc_ptr = NULL;
         void*                                    _alternative_filename_getter_user_arg = NULL;

/** Private declarations */
typedef struct _gfx_image_mipmap
{
                        bool                 data_ptr_releaseable;
                        unsigned int         data_size;
    __bcount(data_size) const unsigned char* data_ptr;
                        unsigned int         height;
                        GLenum               internalformat;
                        bool                 is_compressed;
                        unsigned int         row_alignment;
                        unsigned int         width;

    _gfx_image_mipmap()
    {
        data_ptr             = NULL;
        data_ptr_releaseable = false;
        data_size            = 0;
        height               = 0;
        internalformat       = GL_NONE;
        is_compressed        = false;
        row_alignment        = 0;
        width                = 0;
    }
} _gfx_image_mipmap;

typedef struct
{
    system_hashed_ansi_string filename;
    system_hashed_ansi_string filename_actual;
    system_resizable_vector   mipmaps;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _gfx_image;

typedef gfx_image (*PFNLOADIMAGEPROC)(system_hashed_ansi_string file_name);

typedef struct
{
    system_hashed_ansi_string extension;
    PFNLOADIMAGEPROC          file_loader;
} _gfx_image_format;


const _gfx_image_format supported_image_formats[] =
{
    {system_hashed_ansi_string_create("bmp"),  gfx_bmp_load_from_file},
    {system_hashed_ansi_string_create("jpg"),  gfx_jpg_load_from_file},
    {system_hashed_ansi_string_create("jpeg"), gfx_jpg_load_from_file},
    {system_hashed_ansi_string_create("png"),  gfx_png_load_from_file}
};
const uint32_t n_supported_image_formats = sizeof(supported_image_formats) / sizeof(supported_image_formats[0]);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(gfx_image, gfx_image, _gfx_image);


/** TODO */
PRIVATE gfx_image _gfx_image_create_from_compressed_file(__in __notnull system_hashed_ansi_string name,
                                                         __in __notnull system_hashed_ansi_string compressed_filename,
                                                         __in           GLenum                    compressed_filename_glenum)
{
    gfx_image result = NULL;

    /* Load the file contents.
     *
     * NOTE: Collada file loader initializes texture data asynchronously, eating up
     *       all threads in the thread pool. If we did not create a synchronous
     *       serializer here, we would have ended up with a deadlock.
     */
    system_file_serializer serializer = system_file_serializer_create_for_reading(compressed_filename,
                                                                                  false);

    /* Load in the header */
    ogl_context_texture_compression_compressed_blob_header header;

    if (!system_file_serializer_read(serializer,
                                     sizeof(header),
                                    &header))
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not load header of [%s]",
                           system_hashed_ansi_string_get_buffer(compressed_filename) );

        goto end;
    }

    /* Create a new gfx_image instance */
    result = gfx_image_create(name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Failed to create a gfx_image instance");
    if (result == NULL)
    {
        goto end;
    }

    /* Load in the contents */
    size_t               file_size              = 0;
    const unsigned char* data                   = NULL;
    unsigned int         data_total_n_bytes     = 0;
    const unsigned char* serializer_raw_storage = NULL;

    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                       &serializer_raw_storage);
    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,
                                       &file_size);

    for (unsigned int n_mipmap = 0;
                      n_mipmap < header.n_mipmaps;
                    ++n_mipmap)
    {
        data = serializer_raw_storage                                                                   +
               sizeof(ogl_context_texture_compression_compressed_blob_header)                           +
               sizeof(ogl_context_texture_compression_compressed_blob_mipmap_header) * header.n_mipmaps +
               data_total_n_bytes;

        const ogl_context_texture_compression_compressed_blob_mipmap_header* mipmap_header_ptr;

        mipmap_header_ptr = (const ogl_context_texture_compression_compressed_blob_mipmap_header*)
                             (serializer_raw_storage                                                           +
                              sizeof(ogl_context_texture_compression_compressed_blob_header)                   +
                              sizeof(ogl_context_texture_compression_compressed_blob_mipmap_header) * n_mipmap);

        gfx_image_add_mipmap(result,
                             mipmap_header_ptr->width,
                             mipmap_header_ptr->height,
                             1,
                             compressed_filename_glenum,
                             true,   /* is_compressed */
                             data,
                             mipmap_header_ptr->data_size,
                             false,  /* should_cache_data_ptr */
                             true); /* should_release_cached_data */

        /* Move on the data offset */
        data_total_n_bytes += mipmap_header_ptr->data_size;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC void _gfx_image_release(void* image)
{
    _gfx_image* image_ptr = (_gfx_image*) image;

    if (image_ptr->mipmaps != NULL)
    {
        const unsigned int n_mipmaps = system_resizable_vector_get_amount_of_elements(image_ptr->mipmaps);

        for (unsigned int n_mipmap = 0;
                          n_mipmap < n_mipmaps;
                        ++n_mipmap)
        {
            _gfx_image_mipmap* mipmap_ptr = NULL;

            if (system_resizable_vector_get_element_at(image_ptr->mipmaps,
                                                       n_mipmap,
                                                      &mipmap_ptr) )
            {
                if (mipmap_ptr->data_ptr != NULL && mipmap_ptr->data_ptr_releaseable)
                {
                    delete [] mipmap_ptr->data_ptr;

                    mipmap_ptr->data_ptr = NULL;
                }

                delete mipmap_ptr;
                mipmap_ptr = NULL;
            }
        } /* for (all mipmaps) */

        system_resizable_vector_release(image_ptr->mipmaps);
        image_ptr->mipmaps = NULL;
    }
}

/** Please see header for specification */
PUBLIC unsigned int gfx_image_add_mipmap(__in __notnull                   gfx_image            image,
                                         __in                             unsigned int         width,
                                         __in                             unsigned int         height,
                                         __in                             unsigned int         row_alignment,
                                         __in                             GLenum               internalformat,
                                         __in                             bool                 is_compressed,
                                         __in_bcount(data_size) __notnull const unsigned char* data_ptr,
                                         __in                             unsigned int         data_size,
                                         __in                             bool                 should_cache_data_ptr,
                                         __in                             bool                 should_release_cached_data)
{
    _gfx_image*  image_ptr = (_gfx_image*) image;
    unsigned int result    = -1;

    /* Spawn new mipmap descriptor */
    _gfx_image_mipmap* mipmap_ptr = new (std::nothrow) _gfx_image_mipmap;

    ASSERT_ALWAYS_SYNC(mipmap_ptr != NULL,
                       "Out of memory");
    if (mipmap_ptr == NULL)
    {
        goto end;
    }

    mipmap_ptr->data_size      = data_size;
    mipmap_ptr->height         = height;
    mipmap_ptr->internalformat = internalformat;
    mipmap_ptr->is_compressed  = is_compressed;
    mipmap_ptr->row_alignment  = row_alignment;
    mipmap_ptr->width          = width;

    if (should_cache_data_ptr)
    {
        mipmap_ptr->data_ptr             = data_ptr;
        mipmap_ptr->data_ptr_releaseable = should_release_cached_data;
    }
    else
    {
        mipmap_ptr->data_ptr             = new (std::nothrow) unsigned char[mipmap_ptr->data_size];
        mipmap_ptr->data_ptr_releaseable = true;

        ASSERT_DEBUG_SYNC(mipmap_ptr->data_ptr != NULL,
                          "Out of memory while allocating [%d] bytes for gfx_image mipmap",
                          mipmap_ptr->data_size);

        if (mipmap_ptr->data_ptr == NULL)
        {
            delete mipmap_ptr;
            mipmap_ptr = NULL;

            goto end;
        }
        else
        {
            memcpy((void*) mipmap_ptr->data_ptr,
                   data_ptr,
                   mipmap_ptr->data_size);
        }
    }

    result = system_resizable_vector_get_amount_of_elements(image_ptr->mipmaps);

    system_resizable_vector_push(image_ptr->mipmaps, mipmap_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC gfx_image gfx_image_create(__in __notnull system_hashed_ansi_string name)
{
    _gfx_image* new_gfx_image = new (std::nothrow) _gfx_image;

    ASSERT_DEBUG_SYNC(new_gfx_image != NULL, "Out of memory while allocating _gfx_image");
    if (new_gfx_image != NULL)
    {
        new_gfx_image->filename        = NULL;
        new_gfx_image->filename_actual = NULL;
        new_gfx_image->mipmaps         = system_resizable_vector_create(1, sizeof(_gfx_image_mipmap*) );
        new_gfx_image->name            = name;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_gfx_image,
                                                       _gfx_image_release,
                                                       OBJECT_TYPE_GFX_IMAGE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\GFX Images\\", system_hashed_ansi_string_get_buffer(name)) );
    }

    return (gfx_image) new_gfx_image;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_image_create_from_file(__in __notnull system_hashed_ansi_string name,
                                                        __in __notnull system_hashed_ansi_string file_name,
                                                        __in           bool                      use_alternative_filename_getter)
{
    const char*               file_name_raw_ptr           = system_hashed_ansi_string_get_buffer(file_name);
    GLenum                    compressed_file_name_glenum = GL_NONE;
    system_hashed_ansi_string in_file_name                = file_name;
    gfx_image                 result                      = NULL;

    /* If an alternative filename getter was configured, use it against input filename */
    if (_alternative_filename_getter_proc_ptr != NULL && use_alternative_filename_getter)
    {
        system_hashed_ansi_string proposed_file_name = _alternative_filename_getter_proc_ptr(_alternative_filename_getter_user_arg,
                                                                                             file_name,
                                                                                            &compressed_file_name_glenum);

        if (proposed_file_name != NULL)
        {
            LOG_INFO("Loading [%s] instead of [%s]",
                     system_hashed_ansi_string_get_buffer(proposed_file_name),
                     system_hashed_ansi_string_get_buffer(file_name) );

            file_name         = proposed_file_name;
            file_name_raw_ptr = system_hashed_ansi_string_get_buffer(proposed_file_name);
        }
    }

    /* Look for extension in the file name */
    const char* file_name_last_dot_ptr = NULL;

    if ( (file_name_last_dot_ptr = strrchr(file_name_raw_ptr, '.')) != NULL)
    {
        bool has_found_handler = false;

        if (compressed_file_name_glenum == GL_NONE)
        {
            /* Browse available file handlers and see if we can load the original file */
            for (uint32_t n_extension = 0;
                          n_extension < n_supported_image_formats;
                        ++n_extension)
            {
                if (strcmp(file_name_last_dot_ptr + 1,
                           system_hashed_ansi_string_get_buffer(supported_image_formats[n_extension].extension)) == 0)
                {
                    has_found_handler = true;
                    result            = supported_image_formats[n_extension].file_loader(file_name);

                    break;
                }
            }
        } /* if (compressed_file_name_glenum == GL_NONE) */
        else
        {
            /* This is a compressed file, so need to use a special internal handler */
            result            = _gfx_image_create_from_compressed_file(name,
                                                                       file_name,
                                                                       compressed_file_name_glenum);
            has_found_handler = (result != NULL);
        }

        if (!has_found_handler)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Failed to load [%s]",
                               file_name_raw_ptr);
        }

        if (result != NULL)
        {
            LOG_INFO("Created gfx_image instance for file [%s]", file_name_raw_ptr);

            /* Set the gfx_image's filename */
            _gfx_image* image_ptr = (_gfx_image*) result;

            image_ptr->filename        = in_file_name;
            image_ptr->filename_actual = file_name;
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "File name [%s] is invalid - no extension found",
                           file_name_raw_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC unsigned int gfx_image_get_data_size(__in GLenum       internalformat,
                                            __in unsigned int width,
                                            __in unsigned int height,
                                            __in unsigned int row_alignment)
{
    unsigned int pixel_size = 0;

    switch (internalformat)
    {
        case GL_RGB8:  pixel_size = 24; break;
        case GL_RGBA8: pixel_size = 32; break;

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized internal format [%x]",
                               internalformat);
        }
    }

    /* Align width before we return the result */
    if (width % row_alignment != 0)
    {
        width += row_alignment - (width % row_alignment);
    }

    return pixel_size * width * height;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool gfx_image_get_mipmap_property(__in  __notnull gfx_image                 image,
                                                      __in            unsigned int              n_mipmap,
                                                      __in            gfx_image_mipmap_property mipmap_property,
                                                      __out __notnull void*                     out_result)
{
    _gfx_image* image_ptr = (_gfx_image*) image;
    bool        result    = false;

    if (image_ptr->mipmaps != NULL)
    {
        _gfx_image_mipmap* mipmap_ptr = NULL;

        if (system_resizable_vector_get_element_at(image_ptr->mipmaps, n_mipmap, &mipmap_ptr) )
        {
            switch (mipmap_property)
            {
                case GFX_IMAGE_MIPMAP_PROPERTY_DATA_POINTER:
                {
                    *((const unsigned char**) out_result) = mipmap_ptr->data_ptr;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_DATA_SIZE:
                {
                    *(unsigned int*) out_result = mipmap_ptr->data_size;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT:
                {
                    *((unsigned int*) out_result) = mipmap_ptr->height;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_INTERNALFORMAT:
                {
                    *((GLenum*) out_result) = mipmap_ptr->internalformat;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_IS_COMPRESSED:
                {
                    *((bool*) out_result) = mipmap_ptr->is_compressed;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_ROW_ALIGNMENT:
                {
                    *((unsigned int*) out_result) = mipmap_ptr->row_alignment;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_WIDTH:
                {
                    *((unsigned int*) out_result) = mipmap_ptr->width;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized gfx_image mipmap property");
                }
            } /* switch (mipmap_property) */
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mipmap descriptor for mipmap at index [%d]",
                              n_mipmap);
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void gfx_image_get_property(__in __notnull const gfx_image          image,
                                               __in                 gfx_image_property property,
                                               __in __notnull       void*              out_result_ptr)
{
    const _gfx_image* image_ptr = (const _gfx_image*) image;

    switch(property)
    {
        case GFX_IMAGE_PROPERTY_FILENAME_ACTUAL:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = image_ptr->filename_actual;

            break;
        }

        case GFX_IMAGE_PROPERTY_FILENAME:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = image_ptr->filename;

            break;
        }

        case GFX_IMAGE_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = image_ptr->name;

            break;
        }

        case GFX_IMAGE_PROPERTY_N_MIPMAPS:
        {
            *(unsigned int*) out_result_ptr = system_resizable_vector_get_amount_of_elements(image_ptr->mipmaps);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized GFX image property");
        }
    } /* switch (property) */
}

/** TODO */
PUBLIC EMERALD_API void gfx_image_set_global_property(__in     gfx_image_property property_value,
                                                      __in_opt void*              value)
{
    switch (property_value)
    {
        case GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_FUNC_PTR:
        {
            /* NOTE: This assertion will fail if more than one rendering context is spawned.
             *       Take this as a warning - everything should be fine *but* this scenario
             *       was not tested.
             */
            ASSERT_DEBUG_SYNC(_alternative_filename_getter_proc_ptr == NULL ||
                              _alternative_filename_getter_proc_ptr == value,
                              "TODO: Unsupported scenario");

            _alternative_filename_getter_proc_ptr = (PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR) value;

            break;
        }

        case GFX_IMAGE_PROPERTY_ALTERNATIVE_FILENAME_PROVIDER_USER_ARG:
        {
            _alternative_filename_getter_user_arg = value;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized global gfx_image property");
        }
    } /* switch (property_value) */
}
