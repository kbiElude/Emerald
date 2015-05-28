/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_global.h"
#include "system/system_resizable_vector.h"
#include <zlib.h>


typedef struct _system_file_unpacker_file
{
    void*                     data;              /* points at a certain offset within unpacker::unpacked_buffer_ptr */
    uint32_t                  data_start_offset; /* used during unpacker initialization */
    system_hashed_ansi_string filename;
    uint32_t                  filesize;
    system_file_serializer    serializer;

    explicit _system_file_unpacker_file(__in __notnull system_hashed_ansi_string in_filename)
    {
        data              = NULL;
        data_start_offset = 0;
        filename          = in_filename;
        filesize          = 0;
        serializer        = NULL;
    }

    ~_system_file_unpacker_file()
    {
        if (serializer != NULL)
        {
            system_file_serializer_release(serializer);

            serializer = NULL;
        }
    }
} _system_file_unpacker_file;

typedef struct _system_file_unpacker
{
    system_resizable_vector   files; /* _system_file_unpacker_file* */
    system_hashed_ansi_string packed_filename;
    unsigned char*            unpacked_buffer_ptr;

    explicit _system_file_unpacker(__in __notnull system_hashed_ansi_string in_packed_filename)
    {
        files               = system_resizable_vector_create(4,     /* capacity */
                                                             true); /* should_be_thread_safe */
        packed_filename     = in_packed_filename;
        unpacked_buffer_ptr = NULL;
    }

    ~_system_file_unpacker()
    {
        if (files != NULL)
        {
            _system_file_unpacker_file* file_ptr = NULL;

            while (system_resizable_vector_pop(files,
                                              &file_ptr) )
            {
                delete file_ptr;

                file_ptr = NULL;
            } /* for (all stored file descriptors) */

            system_resizable_vector_release(files);

            files = NULL;
        } /* if (files != NULL) */

        if (unpacked_buffer_ptr != NULL)
        {
            delete [] unpacked_buffer_ptr;

            unpacked_buffer_ptr = NULL;
        }
    }
} _system_file_unpacker;


/** TODO */
PRIVATE bool _system_file_unpacker_init(__in __notnull _system_file_unpacker* file_unpacker_ptr)
{
    unsigned char*              data_ptr               = NULL;
    int                         inflation_result       = 0;
    bool                        is_zlib_inited         = false;
    unsigned int                n_files_embedded       = 0;
    uint32_t                    n_total_decoded_bytes  = 0;
    const unsigned char*        packed_file_data_ptr   = NULL;
    uint32_t                    packed_file_data_size  = 0;
    system_file_serializer      packed_file_serializer = NULL;
    _system_file_unpacker_file* prev_file_ptr          = NULL;
    bool                        result                 = true;
    unsigned char*              traveller_ptr          = data_ptr;
    z_stream                    zlib_stream;

    /* Open the packed file */
    packed_file_serializer = system_file_serializer_create_for_reading(file_unpacker_ptr->packed_filename,
                                                                       false); /* async_read */

    if (packed_file_serializer == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not open packed file serializer");

        result = false;
        goto end;
    }

    /* First four bytes tell us the total number of bytes in the decoded data stream */
    if (!system_file_serializer_read(packed_file_serializer,
                                     sizeof(n_total_decoded_bytes),
                                    &n_total_decoded_bytes) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not read n_total_decoded_bytes");

        result = false;
        goto end;
    }

    system_file_serializer_get_property(packed_file_serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE,
                                        &packed_file_data_ptr);
    system_file_serializer_get_property(packed_file_serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE,
                                       &packed_file_data_size);

    /* Feed the packed data through zlib to unpack it */
    file_unpacker_ptr->unpacked_buffer_ptr = new (std::nothrow) unsigned char[n_total_decoded_bytes];

    if (file_unpacker_ptr->unpacked_buffer_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        result = false;
        goto end;
    }

    zlib_stream.avail_in  = packed_file_data_size;
    zlib_stream.avail_out = n_total_decoded_bytes;
    zlib_stream.next_in   = (Bytef*) packed_file_data_ptr + sizeof(uint32_t);
    zlib_stream.next_out  = file_unpacker_ptr->unpacked_buffer_ptr;
    zlib_stream.opaque    = Z_NULL;
    zlib_stream.zalloc    = Z_NULL;
    zlib_stream.zfree     = Z_NULL;

    if (inflateInit(&zlib_stream) != Z_OK)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not initialize zlib inflation");

        result = false;
        goto end;
    }

    is_zlib_inited   = true;
    inflation_result = inflate(&zlib_stream, Z_NO_FLUSH);

    if (inflation_result      != Z_STREAM_END ||
        zlib_stream.avail_out != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Inflation error");

        result = false;
        goto end;
    }

    /* Read the file table */
    n_files_embedded = *(unsigned int*) file_unpacker_ptr->unpacked_buffer_ptr;

    data_ptr      = file_unpacker_ptr->unpacked_buffer_ptr + sizeof(uint32_t);
    traveller_ptr = data_ptr;

    for (uint32_t n_file = 0;
                  n_file < n_files_embedded;
                ++n_file)
    {
        const unsigned int n_filename_characters = *(uint32_t*) traveller_ptr;

        traveller_ptr += sizeof(uint32_t);

        /* Extract the filename */
        system_hashed_ansi_string file_name = system_hashed_ansi_string_create_substring((const char*) traveller_ptr,
                                                                                         0, /* start_offset */
                                                                                         n_filename_characters);

        traveller_ptr += n_filename_characters;

        /* Extract the filesize */
        unsigned int file_start_offset = *(uint32_t*) traveller_ptr;

        traveller_ptr += sizeof(uint32_t);

        /* Spawn the descriptor */
        _system_file_unpacker_file* new_file_ptr = new (std::nothrow) _system_file_unpacker_file(file_name);

        new_file_ptr->data_start_offset = file_start_offset;

        /* Update previous file descriptor, now that we know the new offset */
        if (prev_file_ptr != NULL)
        {
            prev_file_ptr->filesize = file_start_offset - prev_file_ptr->data_start_offset;
        }

        prev_file_ptr = new_file_ptr;

        /* Store it */
        system_resizable_vector_push(file_unpacker_ptr->files,
                                     new_file_ptr);
    } /* for (all embedded files) */

    /* Update filesize for the last processed file, too. */
    if (prev_file_ptr != NULL)
    {
        prev_file_ptr->filesize = n_total_decoded_bytes - prev_file_ptr->data_start_offset;
    }

    /* At this point, actual data starts. Use this information to update all file descriptors
     * with actual data pointers */
    for (uint32_t n_file = 0;
                  n_file < n_files_embedded;
                ++n_file)
    {
        _system_file_unpacker_file* file_ptr = NULL;

        system_resizable_vector_get_element_at(file_unpacker_ptr->files,
                                               n_file,
                                              &file_ptr);

        ASSERT_DEBUG_SYNC(file_ptr != NULL,
                          "Could not extract file descriptor at index [%d]",
                          n_file);

        file_ptr->data = traveller_ptr + file_ptr->data_start_offset;
    } /* for (all embedded files) */

end:
    if (is_zlib_inited)
    {
        inflateEnd(&zlib_stream);
    }

    if (packed_file_serializer != NULL)
    {
        system_file_serializer_release(packed_file_serializer);

        packed_file_serializer = NULL;
    }

    return result;
}


/** Please see header for specification */
PUBLIC EMERALD_API system_file_unpacker system_file_unpacker_create(__in __notnull system_hashed_ansi_string packed_filename)
{
    _system_file_unpacker* file_unpacker = new (std::nothrow) _system_file_unpacker(packed_filename);

    ASSERT_DEBUG_SYNC(file_unpacker != NULL,
                      "Out of memory");

    /* Build the internal unpacked storage */
    if (!_system_file_unpacker_init(file_unpacker) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not initialize system_file_unpacker instance.");

        delete file_unpacker;

        file_unpacker = NULL;
    }

    /* Store the instance in the global storage */
    system_global_add_file_unpacker( (system_file_unpacker) file_unpacker);

    /* We don't need to initialize anything, so go ahead & return the descriptor as-is */
    return (system_file_unpacker) file_unpacker;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_file_unpacker_get_file_property(__in  __notnull system_file_unpacker               unpacker,
                                                               __in            uint32_t                           file_index,
                                                               __in            system_file_unpacker_file_property property,
                                                               __out __notnull void*                              out_result)
{
    _system_file_unpacker_file* file_ptr     = NULL;
    _system_file_unpacker*      unpacker_ptr = (_system_file_unpacker*) unpacker;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(unpacker != NULL,
                      "Input unpacker argument is NULL");
    ASSERT_DEBUG_SYNC(out_result != NULL,
                      "Output result value ptr is NULL");
    ASSERT_DEBUG_SYNC(file_index < system_resizable_vector_get_amount_of_elements(unpacker_ptr->files),
                      "Invalid file index requested");

    /* Retrieve the requested file descriptor */
    system_resizable_vector_get_element_at(unpacker_ptr->files,
                                           file_index,
                                          &file_ptr);

    ASSERT_DEBUG_SYNC(file_ptr != NULL,
                      "Could not retrieve _system_file_unpacker_file* descriptor");

    /* Retrieve the requested value */
    switch (property)
    {
        case SYSTEM_FILE_UNPACKER_FILE_PROPERTY_FILE_SERIALIZER:
        {
            /* Spawn a system_file_serializer, built around a memory region this unpacker hosts */
            if (file_ptr->serializer == NULL)
            {
                file_ptr->serializer = system_file_serializer_create_for_reading_memory_region(file_ptr->data,
                                                                                               file_ptr->filesize);

                system_file_serializer_set_property(file_ptr->serializer,
                                                    SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                                   &file_ptr->filename);
            }

            *(system_file_serializer*) out_result = file_ptr->serializer;

            ASSERT_DEBUG_SYNC(*(system_file_serializer*) out_result != NULL,
                              "Could not create a system_file_serializer instance for a memory region.");

            break;
        }

        case SYSTEM_FILE_UNPACKER_FILE_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result = file_ptr->filename;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_file_unpacker_file_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_file_unpacker_get_property(__in  __notnull system_file_unpacker          unpacker,
                                                          __in            system_file_unpacker_property property,
                                                          __out __notnull void*                         out_result)
{
    _system_file_unpacker* unpacker_ptr = (_system_file_unpacker*) unpacker;

    switch (property)
    {
        case SYSTEM_FILE_UNPACKER_PROPERTY_PACKED_FILENAME:
        {
            *(system_hashed_ansi_string*) out_result = unpacker_ptr->packed_filename;

            break;
        }

        case SYSTEM_FILE_UNPACKER_PROPERTY_N_OF_EMBEDDED_FILES:
        {
            *(uint32_t*) out_result = system_resizable_vector_get_amount_of_elements(unpacker_ptr->files);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_file_unpacker_property value");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_file_unpacker_release(__in __notnull __post_invalid system_file_unpacker unpacker)
{
    ASSERT_DEBUG_SYNC(unpacker != NULL,
                      "Input unpacker argument is NULL");

    /* Remove the unpacker from the global storage before proceeding */
    system_global_delete_file_unpacker(unpacker);

    /* The necessary magic happens in the destructor */
    delete (_system_file_unpacker*) unpacker;

    unpacker = NULL;
}

