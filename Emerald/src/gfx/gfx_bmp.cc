/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "gfx/gfx_bmp.h"
#include "gfx/gfx_image.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

/* Private definitions */
#pragma pack(push)
    #pragma pack(1)

    typedef struct 
    {
        DWORD size;
        LONG  width;
        LONG  height;
        WORD  planes;
        WORD  bit_count;
        DWORD compression;
        DWORD size_image;
        LONG  x_pels_per_meter;
        LONG  y_pels_per_meter;
        DWORD clr_used;
        DWORD clr_important;
    } bitmap_info_header;
#pragma pack(pop)

/** Please see header for specification */
PRIVATE gfx_image gfx_bmp_shared_load_handler(__in bool                                 should_load_from_file,
                                              __in __notnull  system_hashed_ansi_string file_name,
                                              __in __maybenull const unsigned char*     in_data_ptr)
{
    /* Open file handle */
    unsigned char*   data_ptr     = NULL;
    FILE*            file_handle  = NULL;
    fpos_t           file_size    = 0;
    gfx_image        result       = NULL;

    if (should_load_from_file)
    {
        file_handle  = ::fopen( system_hashed_ansi_string_get_buffer(file_name), "rb");

        ASSERT_ALWAYS_SYNC(file_handle != NULL, "Could not load file [%s]", system_hashed_ansi_string_get_buffer(file_name) );
        if (file_handle == NULL)
        {
            LOG_FATAL("Could not load file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        /* ..and cache its contents */
        if (::fseek(file_handle, 0, SEEK_END) != 0)
        {
            LOG_FATAL("Could not seek to file [%s]'s end.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (::fgetpos(file_handle, &file_size) != 0)
        {
            LOG_FATAL("Could not retrieve file [%s]'s size.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (::fseek(file_handle, 0, SEEK_SET) != 0)
        {
            LOG_FATAL("Could not seek to file [%s]'s beginning.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        data_ptr = new (std::nothrow) unsigned char[ (unsigned int) file_size];
        
        ASSERT_ALWAYS_SYNC(data_ptr != NULL, "Could not allocate memory buffer for file [%s]", system_hashed_ansi_string_get_buffer(file_name) );
        if (data_ptr == NULL)
        {
            LOG_FATAL("Could not allocate memory buffer for file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (::fread(data_ptr, (size_t) file_size, 1, file_handle) != 1)
        {
            LOG_FATAL("Could not fill memory buffer with file [%s]'s contents.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        ::fclose(file_handle);

        /* Store the pointer in in_data_ptr so that we can refer to the same pointer in following shared parts of the code */
        in_data_ptr = data_ptr;
    }
    
    /* Input data pointer should not be NULL at this point */
    if (in_data_ptr == NULL)
    {
        LOG_FATAL("Input data pointer is NULL - cannot proceed with loading [%s]", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Do a few checks, limiting current bitmap support to raw 24-bits */
    bitmap_info_header* header_ptr = (bitmap_info_header*) (in_data_ptr + sizeof(BITMAPFILEHEADER) );

    ASSERT_ALWAYS_SYNC(header_ptr->bit_count == 24, "Only 24-bit .bmp files are supported");
    if (header_ptr->bit_count != 24)
    {
        LOG_FATAL("[%s] uses [%d] bits. Only 24-bit .bmps are supported", system_hashed_ansi_string_get_buffer(file_name), header_ptr->bit_count);

        goto end;
    }

    ASSERT_ALWAYS_SYNC(header_ptr->compression == 0, "Only non-compressed .bmp files are supported.");
    if (header_ptr->compression != 0)
    {
        LOG_FATAL("[%s] uses compression. Only non-compressed .bmps are supported", system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Create the result object */
    result = gfx_image_create(file_name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not create a gfx_image instance");
    if (result == NULL)
    {
        goto end;
    }

    gfx_image_add_mipmap(result,
                         header_ptr->width,
                         header_ptr->height,
                         4, /* row_alignment */
                         GL_RGB8,
                         false,
                         in_data_ptr + sizeof(BITMAPFILEHEADER) + sizeof(bitmap_info_header),
                         gfx_image_get_data_size(GL_RGB8,
                                                 header_ptr->width,
                                                 header_ptr->height,
                                                 4),
                        !should_load_from_file,  /* should_cache_data_ptr */
                         should_load_from_file); /* should_release_cached_data */

end:
    if (should_load_from_file)
    {
        if (file_handle != NULL)
        {
            fclose(file_handle);

            file_handle = NULL;
        }

        if (in_data_ptr != NULL)
        {
            delete in_data_ptr;

            in_data_ptr = NULL;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_file(__in __notnull system_hashed_ansi_string file_name)
{
    ASSERT_DEBUG_SYNC(file_name != NULL, "Cannot use NULL file name.");
    if (file_name != NULL)
    {
        return gfx_bmp_shared_load_handler(true, file_name, NULL);
    }
    else
    {
        return NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_memory(__in __notnull const unsigned char* data_ptr)
{
    return gfx_bmp_shared_load_handler(false, system_hashed_ansi_string_get_default_empty_string(), data_ptr);
}