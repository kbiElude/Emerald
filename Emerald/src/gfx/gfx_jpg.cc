/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"

#include "gfx/gfx_jpg.h"
#include "gfx/gfx_image.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"

#ifdef __cplusplus
    extern "C" {
#endif // __cplusplus

    #include "jpeglib.h"

#ifdef __cplusplus
    }
    #endif // __cplusplus

#include <setjmp.h>


/* Private definitions */
struct _error_manager
{
  struct  jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct _error_manager* my_error_ptr;


METHODDEF(void) _handle_error(j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message)(cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}


/** Please see header for specification */
PRIVATE gfx_image gfx_jpg_shared_load_handler(__in bool                                  should_load_from_file,
                                              __in __notnull   system_hashed_ansi_string file_name,
                                              __in __maybenull const unsigned char*      in_data_ptr,
                                              __in             unsigned int              data_size)
{
    unsigned char* file_data   = NULL;
    HANDLE         file_handle = NULL;
    gfx_image      result      = NULL;

    if (should_load_from_file)
    {
        file_handle = ::CreateFileA(system_hashed_ansi_string_get_buffer(file_name),
                                    GENERIC_READ,
                                    FILE_SHARE_READ,
                                    NULL, /* no security attribs */
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL); /* no template file */

        if (file_handle == INVALID_HANDLE_VALUE)
        {
            LOG_FATAL("Could not open file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        /* ..and cache its contents */
        DWORD file_size    = ::GetFileSize(file_handle, NULL /* lpFileSizeHigh */);
        DWORD n_bytes_read = 0;

        if (file_size == INVALID_FILE_SIZE)
        {
            LOG_FATAL("Could not retrieve file [%s]'s size.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        file_data = new (std::nothrow) unsigned char[ (unsigned int) file_size];

        ASSERT_ALWAYS_SYNC(file_data != NULL, "Could not allocate memory buffer for file [%s]", system_hashed_ansi_string_get_buffer(file_name) );
        if (file_data == NULL)
        {
            LOG_FATAL("Could not allocate memory buffer for file [%s]", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        if (!::ReadFile(file_handle,
                        file_data,
                        file_size,
                        &n_bytes_read,
                        NULL) ||    /* overlapped access not needed */
            n_bytes_read != file_size) 
        {
            LOG_FATAL("Could not fill memory buffer with file [%s]'s contents.", system_hashed_ansi_string_get_buffer(file_name) );

            goto end;
        }

        /* Store the pointer in in_data_ptr so that we can refer to the same pointer in following shared parts of the code */
        data_size   = (unsigned int) file_size;
        in_data_ptr = file_data;
    }

    /* Input data pointer should not be NULL at this point */
    if (in_data_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot load .jpg file for [%s] - data pointer is NULL.",
                           system_hashed_ansi_string_get_buffer(file_name) );

        goto end;
    }

    /* Load the JPG data */
    JSAMPARRAY             buffer;
    jpeg_decompress_struct cinfo;
    _error_manager         jerr;
    int                    row_stride = 0;

    cinfo.err           = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = _handle_error;

    __int64 dataSize = 0;

    if (setjmp(jerr.setjmp_buffer))
    {
        // Handle potential error case!
        jpeg_destroy_decompress(&cinfo);

        ASSERT_ALWAYS_SYNC(false, "Could not set error handler for .jpg loader");

        goto end;
    } 

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src          (&cinfo, (void*) in_data_ptr, data_size);
    jpeg_read_header      (&cinfo, TRUE);
    jpeg_start_decompress (&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;
    buffer     = (*cinfo.mem->alloc_sarray) ( (j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    /* Allocate space for temporary representation */
    unsigned char* temp_buffer = new unsigned char[cinfo.image_width * cinfo.image_height * 3];

    ASSERT_ALWAYS_SYNC(temp_buffer != NULL, "Out of memory");
    if (temp_buffer == NULL)
    {
        goto end;
    }

    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, buffer, 1 /* max_lines */);

        memcpy(temp_buffer + (cinfo.output_height - (cinfo.output_scanline)) * cinfo.image_width * 3,
               buffer[0],
               row_stride);
    }

    jpeg_finish_decompress (&cinfo);
    jpeg_destroy_decompress(&cinfo);

    /* Create the result object */
    result = gfx_image_create(file_name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "gfx_image_create() call failed");
    if (result == NULL)
    {
        goto end;
    }

    gfx_image_add_mipmap(result,
                         cinfo.output_width,
                         cinfo.output_height,
                         1, /* row_alignment */
                         GL_SRGB8,
                         false,
                         temp_buffer,
                         row_stride * cinfo.output_height,
                         !should_load_from_file, /* should_cache_data_ptr */
                         should_load_from_file); /* should_release_cached_data */

    if (temp_buffer != NULL)
    {
        delete [] temp_buffer;

        temp_buffer = NULL;
    }
end:
    if (should_load_from_file)
    {
        if (file_data != NULL)
        {
            delete [] file_data;

            file_data = NULL;
        }

        if (file_handle != NULL)
        {
            ::CloseHandle(file_handle);

            file_handle = NULL;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_file(__in __notnull system_hashed_ansi_string file_name)
{
    ASSERT_DEBUG_SYNC(file_name != NULL, "Cannot use NULL file name.");
    if (file_name != NULL)
    {
        return gfx_jpg_shared_load_handler(true, /* should_load_from_file */
                                           file_name,
                                           NULL,
                                           0);
    }
    else
    {
        return NULL;
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API gfx_image gfx_jpg_load_from_memory(__in __notnull const unsigned char* data_ptr,
                                                      __in           unsigned int         data_size)
{
    return gfx_jpg_shared_load_handler(false, /* should_load_from_file */
                                       system_hashed_ansi_string_get_default_empty_string(),
                                       data_ptr,
                                       data_size);
}