/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_bmp.h"
#include "gfx/gfx_image.h"
#include "ral/ral_types.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

/* Private definitions */
#pragma pack(push)
    #pragma pack(1)

    typedef struct
    {
        uint32_t size;
        int32_t  width;
        int32_t  height;
        uint16_t planes;
        uint16_t bit_count;
        uint32_t compression;
        uint32_t size_image;
        int32_t  x_pels_per_meter;
        int32_t  y_pels_per_meter;
        uint32_t clr_used;
        uint32_t clr_important;
    } bitmap_info_header;
#pragma pack(pop)

/** Please see header for specification */
PRIVATE gfx_image gfx_bmp_shared_load_handler(bool                      should_load_from_file,
                                              system_hashed_ansi_string file_name,
                                              system_file_unpacker      file_unpacker,
                                              const unsigned char*      in_data_ptr)
{
    /* Create or retrieve the file serializer */
    unsigned char*      data_ptr   = NULL;
    bitmap_info_header* header_ptr = NULL;
    gfx_image           result     = NULL;

    if (should_load_from_file)
    {
        unsigned int           file_size                 = 0;
        system_file_serializer serializer                = NULL;
        bool                   should_release_serializer = false;

        if (file_unpacker == NULL)
        {
            serializer = system_file_serializer_create_for_reading(file_name,
                                                                   false); /* async_read */

            should_release_serializer = true;
        }
        else
        {
            unsigned int file_index = -1;
            bool         file_found = false;

            file_found = system_file_enumerator_is_file_present_in_system_file_unpacker(file_unpacker,
                                                                                        file_name,
                                                                                        true, /* use_exact_match */
                                                                                       &file_index);

            ASSERT_DEBUG_SYNC(file_found,
                              "Requested file [%s] not found in a file_unpacker instance.",
                              system_hashed_ansi_string_get_buffer(file_name) );

            system_file_unpacker_get_file_property(file_unpacker,
                                                   file_index,
                                                   SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
                                                  &serializer);

            should_release_serializer = false; /* owned by parent of the file_unpacker */
        }

        if (serializer == NULL)
        {
            LOG_FATAL("Could not load file [%s]",
                      system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        /* ..and cache its contents */
        system_file_serializer_get_property(serializer,
                                            SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,
                                           &file_size);

        ASSERT_DEBUG_SYNC(file_size != 0,
                          "File size is 0");

        data_ptr = new (std::nothrow) unsigned char[ (unsigned int) file_size];

        ASSERT_ALWAYS_SYNC(data_ptr != NULL,
                           "Could not allocate memory buffer for file [%s]",
                           system_hashed_ansi_string_get_buffer(file_name) );

        if (data_ptr == NULL)
        {
            LOG_FATAL("Could not allocate memory buffer for file [%s]",
                      system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (!system_file_serializer_read(serializer,
                                         file_size,
                                         data_ptr) )
        {
            LOG_FATAL("Could not fill memory buffer with file [%s]'s contents.",
                      system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (should_release_serializer)
        {
            system_file_serializer_release(serializer);

            serializer = NULL;
        }

        /* Store the pointer in in_data_ptr so that we can refer to the same pointer in following shared parts of the code */
        in_data_ptr = data_ptr;
    }

    /* Input data pointer should not be NULL at this point */
    if (in_data_ptr == NULL)
    {
        LOG_FATAL("Input data pointer is NULL - cannot proceed with loading [%s]",
                  system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Do a few checks, limiting current bitmap support to raw 24-bits */
    header_ptr = (bitmap_info_header*) (in_data_ptr + 14 /* sizeof(BITMAPFILEHEADER) */);

    ASSERT_ALWAYS_SYNC(header_ptr->bit_count == 24,
                       "Only 24-bit .bmp files are supported");

    if (header_ptr->bit_count != 24)
    {
        LOG_FATAL("[%s] uses [%d] bits. Only 24-bit .bmps are supported",
                  system_hashed_ansi_string_get_buffer(file_name), header_ptr->bit_count);

        goto end;
    }

    ASSERT_ALWAYS_SYNC(header_ptr->compression == 0,
                       "Only non-compressed .bmp files are supported.");

    if (header_ptr->compression != 0)
    {
        LOG_FATAL("[%s] uses compression. Only non-compressed .bmps are supported",
                  system_hashed_ansi_string_get_buffer(file_name) );

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
                         RAL_TEXTURE_FORMAT_SRGB8_UNORM,
                         false,
                         in_data_ptr + 14 /* sizeof(BITMAPFILEHEADER) */ + sizeof(bitmap_info_header),
                         gfx_image_get_data_size(RAL_TEXTURE_FORMAT_RGB8_UNORM,
                                                 header_ptr->width,
                                                 header_ptr->height,
                                                 4),
                         RAL_TEXTURE_DATA_TYPE_UBYTE,
                        !should_load_from_file,  /* should_cache_data_ptr */
                         should_load_from_file); /* should_release_cached_data */

end:
    if (should_load_from_file)
    {
        if (in_data_ptr != NULL)
        {
            delete in_data_ptr;

            in_data_ptr = NULL;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_file(system_hashed_ansi_string file_name,
                                                    system_file_unpacker      file_unpacker)
{
    ASSERT_DEBUG_SYNC(file_name != NULL,
                      "Cannot use NULL file name.");

    if (file_name != NULL)
    {
        return gfx_bmp_shared_load_handler(true,     /* should_load_from_file */
                                           file_name,
                                           file_unpacker,
                                           NULL);    /* in_data_ptr */
    }
    else
    {
        return NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_bmp_load_from_memory(const unsigned char* data_ptr)
{
    return gfx_bmp_shared_load_handler(false, /* should_load_from_file */
                                       system_hashed_ansi_string_get_default_empty_string(),
                                       NULL, /* file_unpacker */
                                       data_ptr);
}