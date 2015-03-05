/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_file_serializer.h"
#include "system/system_file_unpacker.h"
#include "system/system_resizable_vector.h"
#include <zlib.h>


typedef struct _system_file_unpacker_file
{
    void*                     data;
    system_hashed_ansi_string filename;
    uint32_t                  filesize;

    ~_system_file_unpacker_file()
    {
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        } /* if (data != NULL) */
    }

    explicit _system_file_unpacker_file(__in __notnull system_hashed_ansi_string in_filename,
                                        __in           uint32_t                  in_filesize)
    {
        data     = NULL;
        filename = in_filename;
        filesize = in_filesize;
    }
} _system_file_unpacker_file;

typedef struct _system_file_unpacker
{
    system_resizable_vector   files; /* _system_file_unpacker_file* */
    system_hashed_ansi_string packed_filename;

    explicit _system_file_unpacker(__in __notnull system_hashed_ansi_string in_packed_filename)
    {
        files           = system_resizable_vector_create(4,     /* capacity */
                                                         sizeof(_system_file_unpacker_file*),
                                                         true); /* should_be_thread_safe */
        packed_filename = in_packed_filename;
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
    }
} _system_file_unpacker;


/** Please see header for specification */
PUBLIC EMERALD_API system_file_unpacker system_file_unpacker_create(__in __notnull system_hashed_ansi_string packed_filename)
{
    _system_file_unpacker* file_unpacker = new (std::nothrow) _system_file_unpacker(packed_filename);

    ASSERT_DEBUG_SYNC(file_unpacker != NULL,
                      "Out of memory");

    // TODO

    /* We don't need to initialize anything, so go ahead & return the descriptor as-is */
    return (system_file_unpacker) file_unpacker;
}

/** Please see header for specification */
PUBLIC EMERALD_API void system_file_unpacker_release(__in __notnull __post_invalid system_file_unpacker unpacker)
{
    ASSERT_DEBUG_SYNC(unpacker != NULL,
                      "Input unpacker argument is NULL");

    /* The necessary magic happens in the destructor */
    delete (_system_file_unpacker*) unpacker;

    unpacker = NULL;
}

