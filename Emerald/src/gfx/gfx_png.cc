/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "png.h"
#include "gfx/gfx_image.h"
#include "system/system_assertions.h"
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
PRIVATE void _gfx_png_shared_load_handler_mem_load_helper(png_structp png_ptr, png_bytep data_ptr, png_size_t length)
{
    _mem_load_helper* in_data = (_mem_load_helper*) png_ptr->io_ptr;

    memcpy(data_ptr, in_data->src_ptr + in_data->offset, length);
    
    in_data->src_ptr += in_data->offset;
}


/** Please see header for specification */
PRIVATE gfx_image gfx_png_shared_load_handler(__in bool                                 should_load_from_file,
                                              __in __notnull  system_hashed_ansi_string file_name,
                                              __in __maybenull const unsigned char*     in_data_ptr)
{
    /* Open file handle */
    unsigned char*   data_ptr     = NULL;
    FILE*            file_handle  = NULL;
    _mem_load_helper mem_load_helper;
    unsigned int     n_components = 0;
    png_infop        png_info_ptr = NULL;
    png_structp      png_ptr      = NULL;
    gfx_image        result       = NULL;
    unsigned int     row_size     = 0;

    if (should_load_from_file)
    {
        file_handle  = ::fopen( system_hashed_ansi_string_get_buffer(file_name), "rb");

        ASSERT_ALWAYS_SYNC(file_handle != NULL, "Could not load file [%s]", system_hashed_ansi_string_get_buffer(file_name) );
        if (file_handle == NULL)
        {
            LOG_FATAL("Could not load file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }
    }

    /* Initialize libpng */
    png_ptr = ::png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

    ASSERT_ALWAYS_SYNC(png_ptr != NULL, "Could not create png_struct instance.");
    if (png_ptr == 0)
    {
        LOG_FATAL("Could not create png_struct instance.");

        goto end;
    }

    png_info_ptr = png_create_info_struct(png_ptr);

    ASSERT_ALWAYS_SYNC(png_info_ptr != NULL, "Could not create png_info instance.");
    if (png_info_ptr == NULL)
    {
        LOG_FATAL("Could not create png_info instance.");

        goto end;
    }

    if (setjmp(png_jmpbuf(png_ptr) ))
    {
        LOG_FATAL("Error occured in libpng while reading file [%s].", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    if (should_load_from_file)
    {
        /* Initialize IO */
        png_init_io(png_ptr, file_handle);

    }
    else
    {
        mem_load_helper.offset  = 0;
        mem_load_helper.src_ptr = in_data_ptr;

        png_set_read_fn(png_ptr, &mem_load_helper, _gfx_png_shared_load_handler_mem_load_helper);
    }

    /* Read the file */
    png_read_png(png_ptr, png_info_ptr, 0, png_voidp_NULL);

    /* Make sure we support the file */
    ASSERT_ALWAYS_SYNC(png_ptr->bit_depth == 8, "Only 8-bit PNG files are supported [%s]", system_hashed_ansi_string_get_buffer(file_name) );
    if (png_ptr->bit_depth != 8)
    {
        LOG_FATAL("Only 8-bit PNG files are supported [%s]", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    ASSERT_ALWAYS_SYNC(png_ptr->color_type == PNG_COLOR_TYPE_RGB || png_ptr->color_type == PNG_COLOR_TYPE_RGB_ALPHA, 
                       "File [%s] is not of RGB and RGBA format.", 
                       system_hashed_ansi_string_get_buffer(file_name) );
    if (png_ptr->color_type != PNG_COLOR_TYPE_RGB && png_ptr->color_type != PNG_COLOR_TYPE_RGB_ALPHA)
    {
        LOG_FATAL("File [%s] is not of RGB and RGBA format.", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Allocate space for the buffer */
    n_components = (png_ptr->color_type == PNG_COLOR_TYPE_RGB ? 3 : 4);
    data_ptr     = new (std::nothrow) unsigned char[n_components * png_info_ptr->width * png_info_ptr->height];
    row_size     = n_components * png_info_ptr->width;

    ASSERT_ALWAYS_SYNC(data_ptr != NULL, "Out of memory while allocating space for decompressed content storage of file [%s]", system_hashed_ansi_string_get_buffer(file_name) );
    if (data_ptr == NULL)
    {
        LOG_FATAL("Out of memory while allocating space for decompressed content storage of file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    for (unsigned int y = 0; y < png_info_ptr->height; ++y)
    {
        memcpy(data_ptr + y * row_size, png_info_ptr->row_pointers[y], row_size);
    }

    /* Create the result object */
    result = gfx_image_create(file_name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "gfx_image_create() call failed");
    if (result == NULL)
    {
        goto end;
    }

    gfx_image_add_mipmap(result,
                         png_info_ptr->width,
                         png_info_ptr->height,
                         1,
                         GL_SRGB8,
                         false,                  /* is_compressed */
                         data_ptr,
                        !should_load_from_file,  /* should_cache_data_ptr */
                         should_load_from_file); /* should_release_cached_data */

end:
    png_destroy_read_struct(&png_ptr, &png_info_ptr, png_infopp_NULL);

    if (should_load_from_file)
    {
        fclose(file_handle);
    }

    return result;
}

PUBLIC EMERALD_API gfx_image gfx_png_load_from_file(__in __notnull system_hashed_ansi_string file_name)
{
    ASSERT_DEBUG_SYNC(file_name != NULL, "Cannot use NULL file name.");
    if (file_name != NULL)
    {
        return gfx_png_shared_load_handler(true, file_name, NULL);
    }
    else
    {
        return NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_png_load_from_memory(__in __notnull const unsigned char* data_ptr)
{
    return gfx_png_shared_load_handler(false, system_hashed_ansi_string_get_default_empty_string(), data_ptr);
}