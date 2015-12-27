/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "gfx/gfx_rgbe.h"
#include "gfx/gfx_image.h"
#include "ral/ral_types.h"
#include "system/system_assertions.h"
#include "system/system_file_enumerator.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

/* Private definitions */

/** TODO */
__forceinline void _convert_rgbe_to_float(float*         dst_ptr,
                                          unsigned char* rgbe)
{
    if (rgbe[3])
    {
        float f = (float) ldexp(1.0, (int) (rgbe[3]) - 136);

        dst_ptr[0] = rgbe[0] * f;
        dst_ptr[1] = rgbe[1] * f;
        dst_ptr[2] = rgbe[2] * f;
    }
    else
    {
        dst_ptr[0] = 0;
        dst_ptr[1] = 0;
        dst_ptr[2] = 0;
    }
}

/** TODO */
PRIVATE bool _gfx_rgbe_load_data_rle(float* dst_ptr,
                                     char*  src_ptr,
                                     int    width,
                                     int    height)
{
    bool          result              = true;
    unsigned char rgbe[4]             = {0};
    char*         scanline_buffer_ptr = new (std::nothrow) char[4 * width];
    char*         traveller_ptr       = NULL;
    char*         traveller_end_ptr   = NULL;

    if (scanline_buffer_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not allocate space for scanline buffer");

        result = false;
        goto end;
    }

    /* If width is outside supported range, call non-RLE loader */
    if (width < 8 ||
        width > 0x7FFF)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Non-RLE Radiance files not supported");

        result = false;
        goto end;
    }

    while (height > 0)
    {
        memcpy(rgbe,
               src_ptr,
               sizeof(rgbe) );

        src_ptr += sizeof(rgbe);

        if (rgbe[0] != 2   ||
            rgbe[1] != 2   ||
            rgbe[2] & 0x80)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Non-RLE Radiance files not supported");

            result = false;
            goto end;
        }

        if ( ((((int)rgbe[2]) << 8) | rgbe[3]) != width)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Invalid scanline width");

            result = false;
            goto end;
        }

        /* Read four channels into the buffer */
        traveller_ptr = scanline_buffer_ptr;

        for (int i = 0;
                 i < 4;
               ++i)
        {
            traveller_end_ptr = scanline_buffer_ptr + (i + 1) * width;

            while (traveller_ptr < traveller_end_ptr)
            {
                unsigned char mode  = src_ptr[0];
                unsigned char value = src_ptr[1];

                src_ptr += 2;

                if (mode > 128) /* run */
                {
                    unsigned char count = mode - 128;

                    if (count == 0                                     ||
                        count > int(traveller_end_ptr - traveller_ptr) )
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Invalid scanline data");

                        result = false;
                        goto end;
                    }

                    while (count-- > 0)
                    {
                        *traveller_ptr++ = value;
                    }
                }
                else /* non-run */
                {
                    unsigned char count = mode;

                    if (count == 0                                     ||
                        count > int(traveller_end_ptr - traveller_ptr) )
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Invalid scanline data");

                        result = false;
                        goto end;
                    }

                    *traveller_ptr++ = value;

                    if (--count > 0)
                    {
                        memcpy(traveller_ptr,
                               src_ptr,
                               count);

                        traveller_ptr += count;
                        src_ptr       += count;
                    }
                }
            } /* while (traveller_ptr < traveller_end_ptr)*/
        } /* for (int i = 0; i < 4; ++i) */

        /* Convert scanline data to floats */
        for (int i = 0;
                 i < width;
               ++i)
        {
            rgbe[0] = scanline_buffer_ptr[i];
            rgbe[1] = scanline_buffer_ptr[i + 1 * width];
            rgbe[2] = scanline_buffer_ptr[i + 2 * width];
            rgbe[3] = scanline_buffer_ptr[i + 3 * width];

            _convert_rgbe_to_float(dst_ptr,
                                   rgbe);

            dst_ptr += 3;
        }

        height--;
    }
end:
    if (scanline_buffer_ptr != NULL)
    {
        delete [] scanline_buffer_ptr;
    }

    return result;
}

/** TODO */
PRIVATE gfx_image _gfx_rgbe_shared_load_handler(system_hashed_ansi_string name,
                                                const char*               in_data_ptr)
{
    /* Open file handle */
    char*            data_buffer        = NULL;
    uint32_t         data_buffer_size   = 0;
    unsigned char*   data_ptr           = NULL;
    char*            data_ptr_traveller = NULL;
    int              height             = 0;
    const char*      magic_header       = "#?RADIANCE";
    gfx_image        result             = NULL;
    int              width              = 0;

    /* Input data pointer should not be NULL at this point */
    if (in_data_ptr == NULL)
    {
        LOG_FATAL("Input data pointer is NULL - cannot proceed with loading");

        goto end_with_dealloc;
    }

    /* Check header */
    if (memcmp(magic_header,
               in_data_ptr,
               strlen(magic_header) ) != 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Input data is not of RGBE format!");

        goto end_with_dealloc;
    }
    else
    {
        in_data_ptr += strlen(magic_header) + 1;
    }

    /* Parse header */
    data_ptr_traveller = (char*) in_data_ptr;

    while (true)
    {
        if (*data_ptr_traveller == 0 ||
            *data_ptr_traveller == '\n')
        {
            data_ptr_traveller++;

            break;
        }

        if (memcmp(data_ptr_traveller,
                   "FORMAT=",
                   strlen("FORMAT=") ) == 0)
        {
            data_ptr_traveller += strlen("FORMAT=");

            /* RGBE is only supported */
            if (memcmp(data_ptr_traveller,
                       "32-bit_rle_rgbe",
                       strlen("32-bit_rle_rgbe") ) != 0)
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Only RLE RGBE format is supported");

                goto end_with_dealloc;
            }
            else
            {
                data_ptr_traveller += strlen("32-bit_rle_rgbe") + 1; /* + 1 as the format is going to be ended with a newline */
            }
        }
        else
        {
            /* Ignore any other commands and just look for command end */
            while (*data_ptr_traveller != '\n')
            {
                data_ptr_traveller++;
            }

            data_ptr_traveller++;
        }
    }

    /* Following is the resolution string. For simplicity, we just og with the usual bottom-up storage. */
    if (memcmp(data_ptr_traveller,
               "-Y ",
               strlen("-Y ") ) != 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Input data is not bottom-up!");

        goto end_with_dealloc;
    }
    else
    {
        data_ptr_traveller += strlen("-Y ");
    }

    if (sscanf(data_ptr_traveller,
               "%d",
              &height) != 1)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Couldn't read input height");

        goto end_with_dealloc;
    }

    while (*data_ptr_traveller != ' ' )
    {
        data_ptr_traveller++;
    }
    data_ptr_traveller++;

    if (memcmp(data_ptr_traveller,
               "+X ",
               strlen("+X ") ) != 0)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Input data is not left-to-right");

        goto end_with_dealloc;
    }
    else
    {
        data_ptr_traveller += strlen("+X ");
    }

    if (sscanf(data_ptr_traveller,
               "%d",
              &width) != 1)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Couldn't read input width");

        goto end_with_dealloc;
    }

    while (*data_ptr_traveller != '\n')
    {
        data_ptr_traveller++;
    }
    data_ptr_traveller++; /* New-line */

    /* Data starts at this point */
    data_buffer_size = sizeof(float) * 3 * width * height;
    data_buffer      = new (std::nothrow) char[data_buffer_size];

    if (data_buffer == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not allocate %d bytes for data buffer",
                           data_buffer_size);

        goto end_with_dealloc;
    }

    if (!_gfx_rgbe_load_data_rle((float*) data_buffer,
                                 data_ptr_traveller,
                                 width,
                                 height) )
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Error loading RLE data.");

        goto end_with_dealloc;
    }

    /* Create the result object */
    result = gfx_image_create(name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "gfx_image_create() call failed");

    if (result == NULL)
    {
        goto end_with_dealloc;
    }

    /** TODO: Wait, under some back-ends we could actually pass this natively.. Right? */
    gfx_image_add_mipmap(result,
                         width,
                         height,
                         1,
                         RAL_TEXTURE_FORMAT_RGB32_FLOAT,
                         false,
                         (unsigned char*) data_buffer,
                         data_buffer_size,
                         RAL_TEXTURE_DATA_TYPE_FLOAT,
                         false);

    return result;

end_with_dealloc:
    if (data_buffer != NULL)
    {
        delete [] data_buffer;

        data_buffer = NULL;
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_file(system_hashed_ansi_string file_name,
                                                     system_file_unpacker      file_unpacker)
{
    gfx_image result = NULL;

    ASSERT_DEBUG_SYNC(file_name != NULL,
                      "Cannot use NULL file name.");

    if (file_name != NULL)
    {
        /* Instantiate a serializer for the file */
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
                              "File [%s] not found in provided file_unpacker instance.",
                              system_hashed_ansi_string_get_buffer(file_name) );

            system_file_unpacker_get_file_property(file_unpacker,
                                                   file_index,
                                                   SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER,
                                                  &serializer);

            should_release_serializer = false; /* serializer is owned by file_unpacker parent */
        }

        /* ..and cache its contents */
        char*        data_ptr  = NULL;
        unsigned int file_size = 0;

        system_file_serializer_get_property(serializer,
                                            SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,
                                           &file_size);

        ASSERT_DEBUG_SYNC(file_size != 0,
                          "File size is 0");

        data_ptr = new (std::nothrow) char[ (unsigned int) file_size];

        ASSERT_ALWAYS_SYNC(data_ptr != NULL,
                           "Could not allocate memory buffer for file [%s]",
                           system_hashed_ansi_string_get_buffer(file_name) );

        if (data_ptr == NULL)
        {
            LOG_FATAL("Could not allocate memory buffer for file [%s]",
                      system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }
        else
        {
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
            }

            serializer = NULL;

            /* Store the pointer in in_data_ptr so that we can refer to the same pointer in following shared parts of the code */
            result = _gfx_rgbe_shared_load_handler(file_name,
                                                   data_ptr);

            /* Release the data buffer */
            delete [] data_ptr;

            data_ptr = NULL;
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_rgbe_load_from_memory(system_hashed_ansi_string name,
                                                       const char*               data_ptr)
{
    return _gfx_rgbe_shared_load_handler(name, data_ptr);
}