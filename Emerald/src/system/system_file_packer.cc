/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_file_packer.h"
#include "system/system_file_serializer.h"
#include "system/system_resizable_vector.h"
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#ifdef __linux
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif


/* This #define tells the size of the internal buffer. Two of those are allocated
 * at packer instantiation time. They then are used for two things:
 *
 * a) Forming input undecoded byte stream.
 * b) Storage of encrypted byte stream before it is written out to the
 *    result file.
 *
 */
#define INTERNAL_BUFFER_SIZE (4 * 1024768)


typedef struct _system_file_packer_file
{
    system_hashed_ansi_string filename;
    uint32_t                  filesize;

    explicit _system_file_packer_file(system_hashed_ansi_string in_filename,
                                      uint32_t                  in_filesize)
    {
        filename = in_filename;
        filesize = in_filesize;
    }
} _system_file_packer_file;

typedef struct _system_file_packer
{
    char*                   buffer_packed;
    char*                   buffer_unpacked;
    system_resizable_vector files; /* _system_file_packer_file* */
    uint32_t                total_filesize;

    _system_file_packer()
    {
        buffer_packed   = new (std::nothrow) char[INTERNAL_BUFFER_SIZE];
        buffer_unpacked = new (std::nothrow) char[INTERNAL_BUFFER_SIZE];
        files           = system_resizable_vector_create(4,     /* capacity */
                                                         true); /* should_be_thread_safe */
        total_filesize  = 0;

        ASSERT_ALWAYS_SYNC(buffer_packed   != NULL &&
                           buffer_unpacked != NULL,
                           "Out of memory");
    }

    ~_system_file_packer()
    {
        if (buffer_packed != NULL)
        {
            delete [] buffer_packed;

            buffer_packed = NULL;
        }

        if (buffer_unpacked != NULL)
        {
            delete [] buffer_unpacked;

            buffer_unpacked = NULL;
        }

        if (files != NULL)
        {
            _system_file_packer_file* file_ptr = NULL;

            while (system_resizable_vector_pop(files,
                                              &file_ptr) )
            {
                delete file_ptr;

                file_ptr = NULL;
            } /* for (all enqueued file descriptors) */

            system_resizable_vector_release(files);

            files = NULL;
        } /* if (files != NULL) */
    }
} _system_file_packer;


/** TODO */
PRIVATE uint32_t _system_file_packer_build_file_table(system_resizable_vector files,
                                                      uint32_t                result_data_bytes,
                                                      char*                   out_result_data)
{
    uint32_t n_files_to_use = 0;
    char*    traveller_ptr  = out_result_data;

    system_resizable_vector_get_property(files,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_files_to_use);

    /* Store the number of files we are describing */
    *(uint32_t*) traveller_ptr  = n_files_to_use;
    traveller_ptr              += sizeof(uint32_t);

    ASSERT_DEBUG_SYNC(traveller_ptr - out_result_data < (int32_t) result_data_bytes,
                      "Input container is too small.");

    /* Store an entry for each file enqueued */
    uint32_t summed_file_size = 0;

    for (uint32_t n_file = 0;
                  n_file < n_files_to_use;
                ++n_file)
    {
        _system_file_packer_file* file_ptr = NULL;

        system_resizable_vector_get_element_at(files,
                                               n_file,
                                              &file_ptr);

        ASSERT_DEBUG_SYNC(file_ptr != NULL,
                          "File descriptor is NULL");

        /* Write number of characters of the filename */
        *(uint32_t*) traveller_ptr  = system_hashed_ansi_string_get_length(file_ptr->filename);
        traveller_ptr              += sizeof(uint32_t);

        ASSERT_DEBUG_SYNC(traveller_ptr - out_result_data < (int32_t) result_data_bytes,
                          "Input container is too small.");

        /* Write the actual string */
        const uint32_t filename_length = system_hashed_ansi_string_get_length(file_ptr->filename);

        memcpy(traveller_ptr,
               system_hashed_ansi_string_get_buffer(file_ptr->filename),
               filename_length);

        traveller_ptr += filename_length;

        /* Write the offset, at which the file data starts (counting from the first file's data start offset) */
        *(uint32_t*) traveller_ptr  = summed_file_size;
        traveller_ptr              += sizeof(uint32_t);

        ASSERT_DEBUG_SYNC(traveller_ptr - out_result_data < (int32_t) result_data_bytes,
                          "Input container is too small.");

        /* Update the offset. */
        summed_file_size += file_ptr->filesize;
    } /* for (all enqueued files) */

    return (traveller_ptr - out_result_data);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_file_packer_add_file(system_file_packer        packer,
                                                    system_hashed_ansi_string filename)
{
    _system_file_packer_file* new_file_ptr = NULL;
    _system_file_packer*      packer_ptr   = (_system_file_packer*) packer;
    bool                      result       = true;

#ifdef _WIN32
    HANDLE   file      = INVALID_HANDLE_VALUE;
    uint32_t file_size = 0;

#else
    int         file      = -1;
    struct stat file_info;
    ssize_t     file_size = 0;
#endif

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(filename != NULL,
                      "Input filename argument is NULL");
    ASSERT_DEBUG_SYNC(system_hashed_ansi_string_get_length(filename) > 0,
                      "Input filename's length is 0");
    ASSERT_DEBUG_SYNC(packer != NULL,
                      "Input packer argument is NULL");

    /* Attempt to open the file. If successful, retrieve its size and bail out */
#ifdef _WIN32
    file = ::CreateFileA(system_hashed_ansi_string_get_buffer(filename),
                         GENERIC_READ,
                         FILE_SHARE_READ,
                         NULL, /* no security attribs */
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL); /* no template file */

    if (file == INVALID_HANDLE_VALUE)
#else
    file = open(system_hashed_ansi_string_get_buffer(filename),
                O_RDONLY,
                0); /* mode - not used */

    if (file == -1)
#endif
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not open file [%s]. Cannot add the file to the packer.",
                           system_hashed_ansi_string_get_buffer(filename) );

        result = false;
        goto end;
    }

#ifdef _WIN32
    file_size = ::GetFileSize(file,
                              NULL); /* lpFileSizeHight */

    if (file_size == INVALID_FILE_SIZE)
#else
    if (fstat(file,
             &file_info) == 0)
    {
        file_size = file_info.st_size;
    }
    else
#endif

    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve size of file [%s]. Cannot add the file to the packer.",
                           system_hashed_ansi_string_get_buffer(filename) );

        result = false;
        goto end;
    }

    /* Create a descriptor for the file and push it to the internal vector */
    new_file_ptr = new (std::nothrow) _system_file_packer_file(filename,
                                                               file_size);

    if (new_file_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        result = false;
        goto end;
    }

    system_resizable_vector_push(packer_ptr->files,
                                 new_file_ptr);

    /* Also update packer-wide summed up file size info */
    packer_ptr->total_filesize += file_size;

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_file_packer system_file_packer_create()
{
    _system_file_packer* file_packer = new (std::nothrow) _system_file_packer();

    ASSERT_DEBUG_SYNC(file_packer != NULL,
                      "Out of memory");

    /* We don't need to initialize anything, so go ahead & return the descriptor as-is */
    return (system_file_packer) file_packer;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_file_packer_release(system_file_packer packer)
{
    ASSERT_DEBUG_SYNC(packer != NULL,
                      "Input packer argument is NULL");

    /* The necessary magic happens in the destructor */
    delete (_system_file_packer*) packer;

    packer = NULL;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool system_file_packer_save(system_file_packer        packer,
                                                system_hashed_ansi_string target_packed_filename)
{
    uint32_t                   file_table_size               = 0;
    system_file_serializer     in_file_serializer            = NULL;
    bool                       is_zlib_inited                = false;
    uint32_t                   n_current_file                = 0;
    uint32_t                   n_total_decoded_bytes         = 0;
    system_file_serializer     out_file_serializer           = NULL;
    const _system_file_packer* packer_ptr                    = (const _system_file_packer*) packer;
    uint32_t                   n_files_to_pack               = 0;
    bool                       result                        = true;
    char*                      unpacked_buffer_traveller_ptr = NULL;
    z_stream                   zlib_stream;

    /* Set up output file serializer */
    out_file_serializer = system_file_serializer_create_for_writing(target_packed_filename);

    ASSERT_ALWAYS_SYNC(out_file_serializer != NULL,
                       "Could not set up output file serializer.");

    /* Build a file table */
    file_table_size = _system_file_packer_build_file_table(packer_ptr->files,
                                                           INTERNAL_BUFFER_SIZE,
                                                           packer_ptr->buffer_unpacked);

    /* Initialize zlib */
    zlib_stream.next_in = (Bytef*) packer_ptr->buffer_unpacked;
    zlib_stream.opaque  = Z_NULL;
    zlib_stream.zalloc  = Z_NULL;
    zlib_stream.zfree   = Z_NULL;

    if (deflateInit(&zlib_stream,
                    Z_DEFAULT_COMPRESSION) != Z_OK)
    {
        ASSERT_DEBUG_SYNC(false,
                          "zlib failed to initialize");

        result = false;
        goto end;
    }

    is_zlib_inited = true;

    /* Store total number of bytes in the decoded stream */
    n_total_decoded_bytes = file_table_size + packer_ptr->total_filesize;

    system_file_serializer_write(out_file_serializer,
                                 sizeof(n_total_decoded_bytes),
                                &n_total_decoded_bytes);

    /* Start constructing input data for the result blob */
    unpacked_buffer_traveller_ptr = packer_ptr->buffer_unpacked + file_table_size;

    while (n_current_file < n_files_to_pack)
    {
        /* Retrieve file descriptor */
        _system_file_packer_file* file_ptr = NULL;

        system_resizable_vector_get_element_at(packer_ptr->files,
                                               n_current_file,
                                              &file_ptr);

        ASSERT_DEBUG_SYNC(file_ptr != NULL,
                          "File descriptor at index [%d] is NULL.",
                          n_current_file);

        /* Open the file and read as much data as:
         *
         * a) can fit within the block we have preallocated
         * b) is available.
         */
        unsigned int n_bytes_available_for_writing = INTERNAL_BUFFER_SIZE - (unpacked_buffer_traveller_ptr - packer_ptr->buffer_unpacked);
        unsigned int n_bytes_read                  = 0;
        unsigned int n_bytes_to_read               = file_ptr->filesize;

        in_file_serializer = system_file_serializer_create_for_reading(file_ptr->filename,
                                                                       false); /* async_read - not needed */

        ASSERT_ALWAYS_SYNC(in_file_serializer != NULL,
                           "Could not spawn file serializer for file [%s]",
                           system_hashed_ansi_string_get_buffer(file_ptr->filename) );

        while (n_bytes_to_read != 0)
        {
            const unsigned int n_remaining_bytes_to_read = file_ptr->filesize - n_bytes_read;

            n_bytes_to_read = (n_remaining_bytes_to_read > n_bytes_available_for_writing) ? n_bytes_available_for_writing
                                                                                          : n_remaining_bytes_to_read;

            if (!system_file_serializer_read(in_file_serializer,
                                             n_bytes_to_read,
                                             unpacked_buffer_traveller_ptr) )
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Could not read [%d] bytes from [%s].",
                                   n_bytes_to_read,
                                   system_hashed_ansi_string_get_buffer(file_ptr->filename) );
            }

            unpacked_buffer_traveller_ptr += n_bytes_to_read;

            /* Pack the data buffer and stream the result data out to the serializer */
            zlib_stream.avail_in = unpacked_buffer_traveller_ptr - packer_ptr->buffer_unpacked;

            do
            {
                int deflate_result = 0;

                zlib_stream.avail_out = INTERNAL_BUFFER_SIZE;
                zlib_stream.next_out  = (Bytef*) packer_ptr->buffer_packed;

                deflate_result = deflate(&zlib_stream,
                                         (((n_remaining_bytes_to_read - n_bytes_to_read) == 0) &&
                                           (n_current_file                               == (n_files_to_pack - 1) )) ? Z_FINISH : Z_NO_FLUSH);

                ASSERT_DEBUG_SYNC(deflate_result != Z_STREAM_ERROR,
                                  "deflate() failed.");

                if (!system_file_serializer_write(out_file_serializer,
                                                  INTERNAL_BUFFER_SIZE - zlib_stream.avail_out,
                                                  packer_ptr->buffer_packed) )
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Could not write [%d] bytes to result packed file.",
                                       INTERNAL_BUFFER_SIZE - zlib_stream.avail_out);
                }

            }
            while (zlib_stream.avail_out == 0);

            /* Update the helper variables */
            n_bytes_available_for_writing  = INTERNAL_BUFFER_SIZE;
            n_bytes_read                  += n_bytes_to_read;
            n_bytes_to_read                = file_ptr->filesize - n_bytes_read;
            unpacked_buffer_traveller_ptr  = packer_ptr->buffer_unpacked;
            zlib_stream.next_in            = (Bytef*) packer_ptr->buffer_unpacked;
        } /* while (n_bytes_read != n_bytes_to_read) */

        /* Move to the next file */
        system_file_serializer_release(in_file_serializer);
        in_file_serializer = NULL;

        ++n_current_file;
    } /* while (n_current_file < n_files_to_pack) */

    /* All done */
end:
    if (out_file_serializer != NULL)
    {
        system_file_serializer_release(out_file_serializer);

        out_file_serializer = NULL;
    }

    if (is_zlib_inited)
    {
        deflateEnd(&zlib_stream);
    }

    return result;
}
