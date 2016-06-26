/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#include "shared.h"
#include "png.h"
#include "gfx/gfx_image.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

typedef struct
{
    unsigned int         offset;
    const unsigned char* src_ptr;
} _mem_load_helper;


/** Function that is called back by libpng whenever new data chunk is needed. The loading proceeds from beginning to the end
 *  of the file, so we maintain a helper structure throughout the loading process, which keeps track of current offset and
 *  of the original pointer we started from.
 */
PRIVATE void _gfx_png_shared_load_handler_mem_load_helper(png_structp png_ptr,
                                                          png_bytep   data_ptr,
                                                          png_size_t  length)
{
    _mem_load_helper* in_data = (_mem_load_helper*) png_ptr->io_ptr;

    memcpy(data_ptr,
           in_data->src_ptr + in_data->offset,
           length);

    in_data->src_ptr += in_data->offset;
}


/** Please see header for specification */
PRIVATE gfx_image gfx_png_shared_load_handler(bool                      should_load_from_file,
                                              system_hashed_ansi_string file_name,
                                              system_file_unpacker      file_unpacker,
                                              const unsigned char*      in_data_ptr)
{
    /* Open file handle */
    unsigned char*         data_ptr                  = nullptr;
    _mem_load_helper       mem_load_helper;
    unsigned int           n_components              = 0;
    png_infop              png_info_ptr              = nullptr;
    png_structp            png_ptr                   = nullptr;
    gfx_image              result                    = nullptr;
    unsigned int           row_size                  = 0;
    system_file_serializer serializer                = nullptr;
    bool                   should_release_serializer = false;

    if (should_load_from_file)
    {
        if (file_unpacker == nullptr)
        {
            serializer                = system_file_serializer_create_for_reading(file_name,
                                                                                  false); /* async_read */
            should_release_serializer = true;
        }
        else
        {
            unsigned int file_index = 0;
            bool         file_found = false;

            file_found = system_file_enumerator_is_file_present_in_system_file_unpacker(file_unpacker,
                                                                                        file_name,
                                                                                        true, /* use_exact_match */
                                                                                       &file_index);

            ASSERT_DEBUG_SYNC(file_found,
                              "File [%s] was not found in file_unpacker.",
                              system_hashed_ansi_string_get_buffer(file_name) );

            system_file_unpacker_get_file_property(file_unpacker,
                                                   file_index,
                                                   SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
                                                  &serializer);

            should_release_serializer = false; /* serializer is owned by parent of file_unpacker */
        }

        system_file_serializer_get_property(serializer,
                                            SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                           &in_data_ptr);
    }

    /* Initialize libpng */
    png_ptr = ::png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                       0,  /* error_ptr */
                                       0,  /* error_fn */
                                       0); /* warn_fn */

    ASSERT_ALWAYS_SYNC(png_ptr != nullptr,
                       "Could not create png_struct instance.");

    if (png_ptr == nullptr)
    {
        LOG_FATAL("Could not create png_struct instance.");

        goto end;
    }

    png_info_ptr = png_create_info_struct(png_ptr);

    ASSERT_ALWAYS_SYNC(png_info_ptr != nullptr,
                       "Could not create png_info instance.");

    if (png_info_ptr == nullptr)
    {
        LOG_FATAL("Could not create png_info instance.");

        goto end;
    }

    if (setjmp(png_jmpbuf(png_ptr) ))
    {
        LOG_FATAL("Error occured in libpng while reading file [%s].",
                  system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Set up mem-based PNG loading. */
    mem_load_helper.offset  = 0;
    mem_load_helper.src_ptr = in_data_ptr;

    png_set_read_fn(png_ptr,
                   &mem_load_helper,
                    _gfx_png_shared_load_handler_mem_load_helper);

    /* Read the file */
    png_read_png(png_ptr,
                 png_info_ptr,
                 0, /* transforms */
                 nullptr);

    /* Make sure we support the file */
    ASSERT_ALWAYS_SYNC(png_ptr->bit_depth == 8,
                       "Only 8-bit PNG files are supported [%s]",
                       system_hashed_ansi_string_get_buffer(file_name) );

    if (png_ptr->bit_depth != 8)
    {
        LOG_FATAL("Only 8-bit PNG files are supported [%s]",
                  system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    ASSERT_ALWAYS_SYNC(png_ptr->color_type == PNG_COLOR_TYPE_RGB        ||
                       png_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA,
                       "File [%s] is not of RGB and RGBA format.",
                       system_hashed_ansi_string_get_buffer(file_name) );

    if (png_ptr->color_type != PNG_COLOR_TYPE_RGB        &&
        png_ptr->color_type != PNG_COLOR_TYPE_RGB_ALPHA)
    {
        LOG_FATAL("File [%s] is not of RGB and RGBA format.",
                  system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Allocate space for the buffer */
    n_components = (png_ptr->color_type == PNG_COLOR_TYPE_RGB ? 3 : 4);
    data_ptr     = new (std::nothrow) unsigned char[n_components * png_info_ptr->width * png_info_ptr->height];
    row_size     = n_components * png_info_ptr->width;

    ASSERT_ALWAYS_SYNC(data_ptr != nullptr,
                       "Out of memory while allocating space for decompressed content storage of file [%s]",
                       system_hashed_ansi_string_get_buffer(file_name) );

    if (data_ptr == nullptr)
    {
        LOG_FATAL("Out of memory while allocating space for decompressed content storage of file [%s]",
                  system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    for (unsigned int y = 0;
                      y < png_info_ptr->height;
                    ++y)
    {
        memcpy(data_ptr + y * row_size,
               png_info_ptr->row_pointers[y],
               row_size);
    }

    /* Create the result object */
    result = gfx_image_create(file_name);

    ASSERT_DEBUG_SYNC(result != nullptr,
                      "gfx_image_create() call failed");

    if (result == nullptr)
    {
        goto end;
    }

    gfx_image_add_mipmap(result,
                         png_info_ptr->width,
                         png_info_ptr->height,
                         1,
                         RAL_FORMAT_SRGB8_UNORM,
                         false,                  /* is_compressed */
                         data_ptr,
                         n_components * png_info_ptr->width * png_info_ptr->height,
                         RAL_TEXTURE_DATA_TYPE_UBYTE,
                        !should_load_from_file,  /* should_cache_data_ptr */
                         should_load_from_file); /* should_release_cached_data */

end:
    png_destroy_read_struct(&png_ptr,
                            &png_info_ptr,
                             nullptr);

    if (should_release_serializer)
    {
        system_file_serializer_release(serializer);

        serializer = nullptr;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_png_load_from_file(system_hashed_ansi_string file_name,
                                                    system_file_unpacker      file_unpacker)
{
    ASSERT_DEBUG_SYNC(file_name != nullptr,
                      "Cannot use NULL file name.");

    if (file_name != nullptr)
    {
        return gfx_png_shared_load_handler(true, /* should_load_from_file */
                                           file_name,
                                           file_unpacker,
                                           nullptr);
    }
    else
    {
        return nullptr;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_png_load_from_memory(const unsigned char* data_ptr)
{
    return gfx_png_shared_load_handler(false, /* should_load_from_file */
                                       system_hashed_ansi_string_get_default_empty_string(),
                                       nullptr, /* file_unpacker */
                                       data_ptr);
}