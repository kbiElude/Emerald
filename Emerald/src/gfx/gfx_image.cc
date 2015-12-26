/**
 *
 * Emerald (kbi/elude @2012-2015)
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
#include "raGL/raGL_utils.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Global variables.
 *
 *  Yeah, yeah, globals suck. But assuming demo environment, this solution
 *  will do just fine.
 */
static volatile PFNGFXIMAGEGETALTERNATIVEFILENAMEPROCPTR _alternative_filename_getter_proc_ptr = NULL;
static          void*                                    _alternative_filename_getter_user_arg = NULL;

/** Private declarations */
typedef struct _gfx_image_mipmap
{
    bool                  data_ptr_releaseable;
    unsigned int          data_size;
    ral_texture_data_type data_type;
    const unsigned char*  data_ptr;
    ral_texture_format    format;
    unsigned int          height;
    bool                  is_compressed;
    unsigned int          row_alignment;
    unsigned int          width;

    _gfx_image_mipmap()
    {
        data_ptr             = NULL;
        data_ptr_releaseable = false;
        data_size            = 0;
        data_type            = RAL_TEXTURE_DATA_TYPE_UNKNOWN;
        format               = RAL_TEXTURE_FORMAT_UNKNOWN;
        height               = 0;
        is_compressed        = false;
        row_alignment        = 0;
        width                = 0;
    }
} _gfx_image_mipmap;

typedef struct
{
    system_file_unpacker      file_unpacker;
    system_hashed_ansi_string filename;
    system_hashed_ansi_string filename_actual;
    system_resizable_vector   mipmaps;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _gfx_image;

typedef gfx_image (*PFNLOADIMAGEPROC)(system_hashed_ansi_string file_name,
                                      system_file_unpacker      file_unpacker);

typedef struct
{
    system_hashed_ansi_string extension;
    PFNLOADIMAGEPROC          file_loader;
} _gfx_image_format;


const _gfx_image_format supported_image_formats[] =
{
    {
        system_hashed_ansi_string_create("bmp"),
        gfx_bmp_load_from_file
    },

    {
        system_hashed_ansi_string_create("hdr"),
        gfx_rgbe_load_from_file
    },

    {
        system_hashed_ansi_string_create("jpg"),
        gfx_jpg_load_from_file
    },

    {
        system_hashed_ansi_string_create("jpeg"),
        gfx_jpg_load_from_file
    },

    {
        system_hashed_ansi_string_create("png"),
        gfx_png_load_from_file
    }
};
const uint32_t n_supported_image_formats = sizeof(supported_image_formats) /
                                           sizeof(supported_image_formats[0]);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(gfx_image,
                               gfx_image,
                              _gfx_image);


/** TODO */
PRIVATE gfx_image _gfx_image_create_from_alternative_file(system_hashed_ansi_string name,
                                                          system_hashed_ansi_string alternative_filename,
                                                          ral_texture_format        alternative_filename_texture_format,
                                                          system_file_unpacker      file_unpacker)
{

    const unsigned char*                                   data                   = NULL;
    unsigned int                                           data_total_n_bytes     = 0;
    size_t                                                 file_size              = 0;
    ogl_context_texture_compression_compressed_blob_header header;
    gfx_image                                              result                    = NULL;
    const unsigned char*                                   serializer_raw_storage    = NULL;
    bool                                                   should_release_serializer = false;

    /* Load the file contents.
     *
     * NOTE: Collada file loader initializes texture data asynchronously, eating up
     *       all threads in the thread pool. If we did not create a synchronous
     *       serializer here, we would have ended up with a deadlock.
     */
    system_file_serializer serializer = NULL;

    if (file_unpacker == NULL)
    {
        serializer                = system_file_serializer_create_for_reading(alternative_filename,
                                                                              false);              /* async_read */
        should_release_serializer = true;
    }
    else
    {
        unsigned int alternative_filename_index = -1;
        bool         found_file                 = system_file_enumerator_is_file_present_in_system_file_unpacker(file_unpacker,
                                                                                                                 alternative_filename,
                                                                                                                 true, /* use_exact_match */
                                                                                                                &alternative_filename_index);

        ASSERT_ALWAYS_SYNC(found_file,
                          "Alternative file was not found in a file_unpacker instance");

        system_file_unpacker_get_file_property(file_unpacker,
                                               alternative_filename_index,
                                               SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
                                              &serializer);

        /* Do not release the serializer - it is owned by the parent of the file_unpacker! */
        should_release_serializer = false;
    }

    /* Load in the header */
    if (!system_file_serializer_read(serializer,
                                     sizeof(header),
                                    &header))
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not load header of [%s]",
                           system_hashed_ansi_string_get_buffer(alternative_filename) );

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

        ASSERT_DEBUG_SYNC(false,
                          "TODO?");

        gfx_image_add_mipmap(result,
                             mipmap_header_ptr->width,
                             mipmap_header_ptr->height,
                             1,      /* row_alignment */
                             alternative_filename_texture_format,
                             true,   /* is_compressed */
                             data,
                             mipmap_header_ptr->data_size,
                             RAL_TEXTURE_DATA_TYPE_UNKNOWN, /* TODO TODO TODO */
                             false,  /* should_cache_data_ptr */
                             true); /* should_release_cached_data */

        /* Move on the data offset */
        data_total_n_bytes += mipmap_header_ptr->data_size;
    }

end:
    if (should_release_serializer)
    {
        system_file_serializer_release(serializer);

        serializer = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC void _gfx_image_release(void* image)
{
    _gfx_image* image_ptr = (_gfx_image*) image;

    if (image_ptr->mipmaps != NULL)
    {
        unsigned int n_mipmaps = 0;

        system_resizable_vector_get_property(image_ptr->mipmaps,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_mipmaps);

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
PUBLIC unsigned int gfx_image_add_mipmap(gfx_image             image,
                                         unsigned int          width,
                                         unsigned int          height,
                                         unsigned int          row_alignment,
                                         ral_texture_format    format,
                                         bool                  is_compressed,
                                         const unsigned char*  data_ptr,
                                         unsigned int          data_size,
                                         ral_texture_data_type data_type,
                                         bool                  should_cache_data_ptr,
                                         bool                  should_release_cached_data)
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
    mipmap_ptr->data_type      = data_type;
    mipmap_ptr->format         = format;
    mipmap_ptr->height         = height;
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

    system_resizable_vector_get_property(image_ptr->mipmaps,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    system_resizable_vector_push(image_ptr->mipmaps,
                                 mipmap_ptr);

end:
    return result;
}

/** Please see header for specification */
PUBLIC gfx_image gfx_image_create(system_hashed_ansi_string name)
{
    _gfx_image* new_gfx_image = new (std::nothrow) _gfx_image;

    ASSERT_DEBUG_SYNC(new_gfx_image != NULL,
                      "Out of memory while allocating _gfx_image");

    if (new_gfx_image != NULL)
    {
        new_gfx_image->filename        = NULL;
        new_gfx_image->filename_actual = NULL;
        new_gfx_image->mipmaps         = system_resizable_vector_create(1 /* capacity */);
        new_gfx_image->name            = name;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_gfx_image,
                                                       _gfx_image_release,
                                                       OBJECT_TYPE_GFX_IMAGE,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\GFX Images\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (gfx_image) new_gfx_image;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_image_create_from_file(system_hashed_ansi_string name,
                                                        system_hashed_ansi_string file_name,
                                                        bool                      use_alternative_filename_getter)
{
    const char*               file_name_raw_ptr               = system_hashed_ansi_string_get_buffer(file_name);
    ral_texture_format        alternative_file_texture_format = RAL_TEXTURE_FORMAT_UNKNOWN;
    system_file_unpacker      alternative_file_unpacker       = NULL;
    system_hashed_ansi_string in_file_name                    = file_name;
    gfx_image                 result                          = NULL;

    /* If an alternative filename getter was configured, use it against input filename */
    if (_alternative_filename_getter_proc_ptr != NULL &&
        use_alternative_filename_getter)
    {
        system_hashed_ansi_string proposed_file_name = _alternative_filename_getter_proc_ptr(_alternative_filename_getter_user_arg,
                                                                                             file_name,
                                                                                            &alternative_file_texture_format,
                                                                                            &alternative_file_unpacker);

        if (proposed_file_name != NULL)
        {
            system_hashed_ansi_string file_unpacker_name = system_hashed_ansi_string_create("none");

            if (alternative_file_unpacker != NULL)
            {
                system_file_unpacker_get_property(alternative_file_unpacker,
                                                  SYSTEM_FILE_UNPACKER_PROPERTY_PACKED_FILENAME,
                                                 &file_unpacker_name);
            }

            LOG_INFO("Loading [%s] (file_unpacker name:[%s]) instead of [%s]",
                     system_hashed_ansi_string_get_buffer(proposed_file_name),
                     system_hashed_ansi_string_get_buffer(file_unpacker_name),
                     system_hashed_ansi_string_get_buffer(file_name) );

            file_name         = proposed_file_name;
            file_name_raw_ptr = system_hashed_ansi_string_get_buffer(proposed_file_name);
        }

        /* TODO: The legacy code used to use GLenums instead of RAL texture types to represent the texture format.
         *       When we no longer need the backward compatibility, we really should switch to RAL enums.
         */
        if (alternative_file_texture_format == GL_NONE)
        {
            alternative_file_texture_format = RAL_TEXTURE_FORMAT_UNKNOWN;
        }
        else
        {
            alternative_file_texture_format = raGL_utils_get_ral_texture_format_for_ogl_enum( (GLenum) alternative_file_texture_format);
        }
    }

    /* Look for extension in the file name */
    const char* file_name_last_dot_ptr = NULL;

    if ( (file_name_last_dot_ptr = strrchr(file_name_raw_ptr,
                                           '.')) != NULL)
    {
        bool has_found_handler = false;

        if (alternative_file_texture_format == RAL_TEXTURE_FORMAT_UNKNOWN)
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
                    result            = supported_image_formats[n_extension].file_loader(file_name,
                                                                                         alternative_file_unpacker);

                    break;
                }
            }
        } /* if (alternative_file_texture_format == RAL_TEXTURE_FORMAT_UNKNOWN) */
        else
        {
            /* This is a compressed file, so need to use a special internal handler */
            result            = _gfx_image_create_from_alternative_file(name,
                                                                        file_name,
                                                                        alternative_file_texture_format,
                                                                        alternative_file_unpacker);
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

            image_ptr->file_unpacker   = alternative_file_unpacker;
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
PUBLIC unsigned int gfx_image_get_data_size(ral_texture_format format,
                                            unsigned int       width,
                                            unsigned int       height,
                                            unsigned int       row_alignment)
{
    unsigned int pixel_size = 0;

    switch (format)
    {
        case RAL_TEXTURE_FORMAT_RGB8_UNORM:
        {
            pixel_size = 24;

            break;
        }

        case RAL_TEXTURE_FORMAT_RGBA8_UNORM:
        {
            pixel_size = 32;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Unrecognized texture format format [%x]",
                               format);
        }
    } /* switch (format) */

    /* Align width before we return the result */
    if (width % row_alignment != 0)
    {
        width += row_alignment - (width % row_alignment);
    }

    return pixel_size * width * height;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool gfx_image_get_mipmap_property(gfx_image                 image,
                                                      unsigned int              n_mipmap,
                                                      gfx_image_mipmap_property mipmap_property,
                                                      void*                     out_result)
{
    _gfx_image* image_ptr = (_gfx_image*) image;
    bool        result    = false;

    if (image_ptr->mipmaps != NULL)
    {
        _gfx_image_mipmap* mipmap_ptr = NULL;

        if (system_resizable_vector_get_element_at(image_ptr->mipmaps,
                                                   n_mipmap,
                                                  &mipmap_ptr) )
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

                case GFX_IMAGE_MIPMAP_PROPERTY_DATA_TYPE:
                {
                    *((ral_texture_data_type*) out_result) = mipmap_ptr->data_type;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_FORMAT_RAL:
                {
                    *((ral_texture_format*) out_result) = mipmap_ptr->format;

                    break;
                }

                case GFX_IMAGE_MIPMAP_PROPERTY_HEIGHT:
                {
                    *((unsigned int*) out_result) = mipmap_ptr->height;

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
PUBLIC EMERALD_API void gfx_image_get_property(const gfx_image          image,
                                                     gfx_image_property property,
                                                     void*              out_result_ptr)
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
            system_resizable_vector_get_property(image_ptr->mipmaps,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized GFX image property");
        }
    } /* switch (property) */
}

/** TODO */
PUBLIC EMERALD_API void gfx_image_set_global_property(gfx_image_property property_value,
                                                      void*              value)
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
