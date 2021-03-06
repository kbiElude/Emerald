/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "curve/curve_segment.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_file_serializer.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "system/system_thread_pool.h"
#include "system/system_variant.h"

#ifdef __linux
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <unistd.h>
#endif

/** Internal type definitions */
typedef enum
{
    SYSTEM_FILE_SERIALIZER_TYPE_FILE,
    SYSTEM_FILE_SERIALIZER_TYPE_MEMORY_REGION

} _system_file_serializer_type;

#ifdef _WIN32
    static const HANDLE file_handle_invalid = INVALID_HANDLE_VALUE;
#else
    static const int file_handle_invalid = -1;
#endif

typedef struct 
{
#ifdef _WIN32
    HANDLE                       file_handle;
#else
    int                          file_handle;
#endif

    char*                        contents;               /* reading/writing */
    uint32_t                     current_index;          /* reading/writing */
    system_hashed_ansi_string    file_name;              /* reading/writing */
    system_hashed_ansi_string    file_path;              /* reading only */
    uint32_t                     file_size;              /* reading only */
    bool                         for_reading;            /* if false, the serializer is write-only; if true, it is read-only */
    system_event                 reading_finished_event; /* reading only */
    _system_file_serializer_type type;
    uint32_t                     writing_capacity;       /* writing only */

    REFCOUNT_INSERT_VARIABLES
} _system_file_serializer;

/* Forward declarations */
PRIVATE THREAD_POOL_TASK_HANDLER void _system_file_serializer_read_task_executor     (system_thread_pool_callback_argument argument);
PRIVATE                          void _system_file_serializer_release                (void*                                serializer);
PRIVATE                          void _system_file_serializer_write_down_data_to_file(_system_file_serializer*             serializer_ptr);

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(system_file_serializer,
                               system_file_serializer,
                              _system_file_serializer);


/** Function that reads the file and sets the internal event, so that normal function can make full use of the read data.
 *
 *  @param argument Pointer to _system_file_serializer instance.
 */
PRIVATE THREAD_POOL_TASK_HANDLER void _system_file_serializer_read_task_executor(system_thread_pool_callback_argument argument)
{
#ifdef _WIN32
    DWORD n_bytes_read = 0;
    BOOL  result       = FALSE;
#else
    struct stat file_info;
    ssize_t     n_bytes_read = 0;
#endif

    _system_file_serializer* serializer_ptr = (_system_file_serializer*) argument;

    ASSERT_DEBUG_SYNC(serializer_ptr->type == SYSTEM_FILE_SERIALIZER_TYPE_FILE,
                      "_system_file_serializer_read_task_executor() can only be called for file serializers.");

    /* Open the file */
#ifdef _WIN32
    serializer_ptr->file_handle = ::CreateFile(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                                               GENERIC_READ,
                                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                                               NULL,                   /* no specific security attributes */
                                               OPEN_EXISTING,          /* only open an existing file */
                                               FILE_ATTRIBUTE_NORMAL,
                                               NULL);                  /* no template file */
#else
    serializer_ptr->file_handle = open(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                                       O_RDONLY,
                                       0); /* mode - not used */
#endif

    if (serializer_ptr->file_handle == file_handle_invalid)
    {
        LOG_ERROR("File %s could not have been opened for reading",
                  system_hashed_ansi_string_get_buffer(serializer_ptr->file_name)
                 );
    }
    else
    {
        /* Retrieve the file size */
#ifdef _WIN32
        serializer_ptr->file_size = ::GetFileSize(serializer_ptr->file_handle,
                                                  NULL);
#else
        if (fstat(serializer_ptr->file_handle,
                 &file_info) != 0)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve file info structure");
        }

        serializer_ptr->file_size = file_info.st_size;
#endif

        /* Allocate a buffer to hold the file contents */
        serializer_ptr->contents = new (std::nothrow) char[serializer_ptr->file_size + 1];

        memset(serializer_ptr->contents,
               0,
               serializer_ptr->file_size + 1);

        /* Read the contents. */
#ifdef _WIN32
        n_bytes_read = 0;
        result       = ::ReadFile(serializer_ptr->file_handle,
                                  serializer_ptr->contents,
                                  serializer_ptr->file_size,
                                 &n_bytes_read,
                                  NULL);                           /* overlapped access not needed */

        ASSERT_ALWAYS_SYNC(result == TRUE,
                           "Could not read %d bytes for file [%s]",
                           serializer_ptr->file_size,
                           system_hashed_ansi_string_get_buffer(serializer_ptr->file_name)
                          );
#else
        ASSERT_DEBUG_SYNC(serializer_ptr->file_size < SSIZE_MAX,
                          "File too large to read.");

        n_bytes_read = read(serializer_ptr->file_handle,
                            serializer_ptr->contents,
                            serializer_ptr->file_size);

        ASSERT_ALWAYS_SYNC(n_bytes_read == serializer_ptr->file_size,
                           "Could not read %d bytes for file [%s]",
                           serializer_ptr->file_size,
                           system_hashed_ansi_string_get_buffer(serializer_ptr->file_name)
                          );
#endif

        /* Clsoe the file, won't need it anymore. */
#ifdef _WIN32
        ::CloseHandle(serializer_ptr->file_handle);

        serializer_ptr->file_handle = NULL;
#else
        if (close(serializer_ptr->file_handle) == -1)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not close file handle.");
        }

        serializer_ptr->file_handle = 0;
#endif

        LOG_INFO("Contents cached for file [%s]",
                 system_hashed_ansi_string_get_buffer(serializer_ptr->file_name) );

        /* Now retrieve path to the file */
#ifdef _WIN32
        DWORD path_length_wo_terminator = ::GetFullPathName(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                                                            0,     /* nBufferLength */
                                                            NULL,  /* lpBuffer */
                                                            NULL); /* lpFilePart */
        char* path                      = new (std::nothrow) char[path_length_wo_terminator + 1];

        ASSERT_ALWAYS_SYNC(path != NULL,
                           "Out of memory");

        if (path != NULL)
        {
            char* file_inside_path = NULL;

            memset(path,
                   0,
                   path_length_wo_terminator + 1);

            ::GetFullPathName(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                              path_length_wo_terminator + 1,
                              path,
                              &file_inside_path);

            path[file_inside_path - path] = 0;

            serializer_ptr->file_path = system_hashed_ansi_string_create(path);
        }

        delete [] path;
        path = NULL;
#else
    char* file_path = realpath(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                               NULL); /* resolved_path */

    ASSERT_DEBUG_SYNC(file_path != NULL,
                      "Could not determine file path for the file [%s]",
                      system_hashed_ansi_string_get_buffer(serializer_ptr->file_name) );

    if (file_path != NULL)
    {
        /* Find the last / character and put the terminator right after it */
        char* file_path_last_slash = strrchr(file_path, '/');

        ASSERT_DEBUG_SYNC(file_path_last_slash != NULL,
                          "Could not find the slash character in the file path for file [%s]",
                          system_hashed_ansi_string_get_buffer(file_path) );

        *(file_path_last_slash + 1) = 0;

        serializer_ptr->file_path = system_hashed_ansi_string_create(file_path);

        free(file_path);
        file_path = NULL;
    }
#endif
    }

    /* Set the event so that cache-based functions can follow. */
    system_event_set(serializer_ptr->reading_finished_event);
}

/** Please see header file for specification */
PRIVATE void _system_file_serializer_release(void* serializer)
{
    _system_file_serializer* serializer_ptr = (_system_file_serializer*) serializer;

    if (serializer_ptr->for_reading)
    {
        /* Wait till reading finishes */
        system_event_wait_single(serializer_ptr->reading_finished_event);

        /* Release reading-specific fields */
        system_event_release(serializer_ptr->reading_finished_event);

        if (serializer_ptr->type == SYSTEM_FILE_SERIALIZER_TYPE_FILE)
        {
            delete [] serializer_ptr->contents;

            serializer_ptr->contents = NULL;
        }
    }
    else
    {
        _system_file_serializer_write_down_data_to_file(serializer_ptr);

#ifdef _WIN32
        if (serializer_ptr->file_handle != file_handle_invalid &&
            serializer_ptr->file_handle != NULL)
#else
        if (serializer_ptr->file_handle != file_handle_invalid)
#endif
        {
            /* Cool to close the file now. */
#ifdef _WIN32
            ::CloseHandle(serializer_ptr->file_handle);
#else
            close(serializer_ptr->file_handle);
#endif

            serializer_ptr->file_handle = file_handle_invalid;
        }

        /* Release all occupied blocks */
        delete [] serializer_ptr->contents;

        serializer_ptr->contents = NULL;
    }
}

/** TODO */
PRIVATE void _system_file_serializer_write_down_data_to_file(_system_file_serializer* serializer_ptr)
{
    ASSERT_DEBUG_SYNC(serializer_ptr->type == SYSTEM_FILE_SERIALIZER_TYPE_FILE,
                      "_system_file_serializer_write_down_data_to_file() can only be called for file serializers.");

#ifdef _WIN32
    DWORD n_bytes_written = 0;
    BOOL  result          = FALSE;
#else
    ssize_t n_bytes_written = 0;
    ssize_t result          = 0;
#endif

    if (serializer_ptr->file_handle == file_handle_invalid)
    {
        /* Open the file for writing. If a file already exists, overwrite it */
#ifdef _WIN32
        serializer_ptr->file_handle = ::CreateFile(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                                                   GENERIC_WRITE,
                                                   0,                      /* no sharing */
                                                   NULL,                   /* no specific security attributes */
                                                   CREATE_ALWAYS,          /* always create a new file*/
                                                   FILE_ATTRIBUTE_NORMAL,
                                                   NULL);                  /* no template file */
#else
        serializer_ptr->file_handle = open(system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                                           O_CREAT | O_WRONLY,
                                           S_IRUSR | S_IWUSR); /* read/write permissions for the owner */
#endif

        ASSERT_ALWAYS_SYNC(serializer_ptr->file_handle != file_handle_invalid,
                           "File %s could not have been created for writing",
                           system_hashed_ansi_string_get_buffer(serializer_ptr->file_name)
                          );
    }

    if (serializer_ptr->file_handle != file_handle_invalid)
    {
#ifdef _WIN32
        result = ::WriteFile(serializer_ptr->file_handle,
                             serializer_ptr->contents,
                             serializer_ptr->file_size,
                            &n_bytes_written,
                             NULL);              /* no overlapped behavior needed */

        ASSERT_ALWAYS_SYNC(result == TRUE,
                          "Could not write file [%s]",
                          system_hashed_ansi_string_get_buffer(serializer_ptr->file_name) );
#else
        result = write(serializer_ptr->file_handle,
                       serializer_ptr->contents,
                       serializer_ptr->file_size);

        n_bytes_written = (result >= 0) ? result : 0;

        ASSERT_ALWAYS_SYNC(result != -1,
                          "Could not write file [%s]",
                          system_hashed_ansi_string_get_buffer(serializer_ptr->file_name) );
#endif

        ASSERT_ALWAYS_SYNC(n_bytes_written == serializer_ptr->file_size,
                           "Could not fully write file [%s] (%d bytes written out of %db)",
                           system_hashed_ansi_string_get_buffer(serializer_ptr->file_name),
                           n_bytes_written,
                           serializer_ptr->file_size);

        /* Reset the index used for storing incoming data*/
        serializer_ptr->file_size = 0;
    }
}


/** Please see header file for specification */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_reading_memory_region(void*        data,
                                                                                                  unsigned int data_size)
{
    static unsigned int      n_instances_created = 0;
    unsigned int             n_this_instance     = system_atomics_increment(&n_instances_created);
    _system_file_serializer* serializer_ptr      = new _system_file_serializer;
    char                     temp_buffer[128]    = {0};

    serializer_ptr->contents               = (char*) data;
    serializer_ptr->current_index          = 0;
    serializer_ptr->file_handle            = file_handle_invalid;
    serializer_ptr->file_name              = NULL;
    serializer_ptr->file_size              = data_size;
    serializer_ptr->for_reading            = true;
    serializer_ptr->reading_finished_event = system_event_create(true); /* manual_reset */
    serializer_ptr->type                   = SYSTEM_FILE_SERIALIZER_TYPE_MEMORY_REGION;

    system_event_set(serializer_ptr->reading_finished_event);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "Memory region serializer %d",
             n_this_instance);

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(serializer_ptr,
                                                   _system_file_serializer_release,
                                                   OBJECT_TYPE_SYSTEM_FILE_SERIALIZER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\File Serializers\\",
                                                                                                           temp_buffer) );

    return (system_file_serializer) serializer_ptr;
}

/** Please see header file for specification */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_reading(system_hashed_ansi_string file_name,
                                                                                    bool                      async_read)
{
    _system_file_serializer* serializer_ptr = new _system_file_serializer;

    serializer_ptr->contents               = NULL;
    serializer_ptr->current_index          = 0;
    serializer_ptr->file_handle            = file_handle_invalid;
    serializer_ptr->file_name              = file_name;
    serializer_ptr->file_size              = 0;
    serializer_ptr->for_reading            = true;
    serializer_ptr->reading_finished_event = system_event_create(true); /* manual_reset */
    serializer_ptr->type                   = SYSTEM_FILE_SERIALIZER_TYPE_FILE;

    REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(serializer_ptr,
                                                   _system_file_serializer_release,
                                                   OBJECT_TYPE_SYSTEM_FILE_SERIALIZER,
                                                   system_hashed_ansi_string_create_by_merging_two_strings("\\File Serializers\\",
                                                                                                           system_hashed_ansi_string_get_buffer(file_name) ) );

    if (async_read)
    {
        /* Submit a file reading task */
        system_thread_pool_task task = system_thread_pool_create_task_handler_with_event_signal(THREAD_POOL_TASK_PRIORITY_IDLE,
                                                                                                _system_file_serializer_read_task_executor,
                                                                                                serializer_ptr,
                                                                                                serializer_ptr->reading_finished_event);

        system_thread_pool_submit_single_task(task);
    }
    else
    {
        _system_file_serializer_read_task_executor(serializer_ptr);

        system_event_set(serializer_ptr->reading_finished_event);
    }

    return (system_file_serializer) serializer_ptr;
}

/** Please see header file for specification */
PUBLIC EMERALD_API system_file_serializer system_file_serializer_create_for_writing(system_hashed_ansi_string file_name)
{
    _system_file_serializer* serializer_ptr = new (std::nothrow) _system_file_serializer;

    ASSERT_ALWAYS_SYNC(serializer_ptr != NULL,
                       "Could not alloc new serializer");

    if (serializer_ptr != NULL)
    {
        serializer_ptr->contents               = new (std::nothrow) char[FILE_SERIALIZER_START_CAPACITY];
        serializer_ptr->current_index          = 0;
        serializer_ptr->file_handle            = file_handle_invalid;
        serializer_ptr->file_name              = file_name;
        serializer_ptr->file_size              = 0;
        serializer_ptr->for_reading            = false;
        serializer_ptr->reading_finished_event = NULL;
        serializer_ptr->type                   = SYSTEM_FILE_SERIALIZER_TYPE_FILE;
        serializer_ptr->writing_capacity       = FILE_SERIALIZER_START_CAPACITY;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(serializer_ptr,
                                                       _system_file_serializer_release,
                                                       OBJECT_TYPE_SYSTEM_FILE_SERIALIZER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\File Serializers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(file_name) ) );

        ASSERT_ALWAYS_SYNC(serializer_ptr->contents != NULL,
                           "Could not allocate writing buffer.");
    }

    return (system_file_serializer) serializer_ptr;
}

/** Please see header file for specification */
PUBLIC EMERALD_API void system_file_serializer_flush_writes(system_file_serializer serializer)
{
    _system_file_serializer_write_down_data_to_file( (_system_file_serializer*) serializer);
}

/** Please see header file for specification */
PUBLIC EMERALD_API void system_file_serializer_get_property(system_file_serializer          serializer,
                                                            system_file_serializer_property property,
                                                            void*                           out_data)
{
    _system_file_serializer* serializer_ptr = (_system_file_serializer*) serializer;

    switch (property)
    {
        case SYSTEM_FILE_SERIALIZER_PROPERTY_CURRENT_OFFSET:
        {
            *(uint32_t*) out_data = serializer_ptr->current_index;

            break;
        }

        case SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME:
        {
            *(system_hashed_ansi_string*) out_data = serializer_ptr->file_name;

            break;
        }

        case SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH:
        {
            ASSERT_DEBUG_SYNC(serializer_ptr->for_reading,
                              "SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH property is only available for serializers instantiated for reading");

            if (serializer_ptr->for_reading)
            {
                LOG_ERROR("Performance hit: waiting for the read op to finish.");

                system_event_wait_single(serializer_ptr->reading_finished_event);
            }

            *(system_hashed_ansi_string*) out_data = serializer_ptr->file_path;
            break;
        }

        case SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH_AND_NAME:
        {
            system_hashed_ansi_string file_name = NULL;
            system_hashed_ansi_string file_path = NULL;

            system_file_serializer_get_property(serializer,
                                                SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                               &file_name);
            system_file_serializer_get_property(serializer,
                                                SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_PATH,
                                               &file_path);

            *(system_hashed_ansi_string*) out_data = system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(file_path),
                                                                                                             system_hashed_ansi_string_get_buffer(file_name) );

            break;
        }

        case SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE:
        {
            ASSERT_DEBUG_SYNC(serializer_ptr->for_reading,
                              "SYSTEM_FILE_SERIALIZER_PROPERTY_RAW_STORAGE property is only available for serializers instantiated for reading");

            if (serializer_ptr->for_reading)
            {
                LOG_ERROR("Performance hit: waiting for the read op to finish.");

                system_event_wait_single(serializer_ptr->reading_finished_event);
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Serializer was not created for reading purposes");
            }

            *(char**) out_data = serializer_ptr->contents;
            break;
        }

        case SYSTEM_FILE_SERIALIZER_PROPERTY_SIZE:
        {
            *(uint32_t*) out_data = serializer_ptr->file_size;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_file_serializer_property value.");
        }
    } /* switch (property) */
}

/** Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_read(system_file_serializer serializer,
                                                    uint32_t               n_bytes,
                                                    void*                  out_result)
{
    bool                     result         = false;
    _system_file_serializer* serializer_ptr = (_system_file_serializer*) serializer;

    if (serializer_ptr->for_reading)
    {
        system_event_wait_single(serializer_ptr->reading_finished_event);

        if (serializer_ptr->current_index + n_bytes <= serializer_ptr->file_size)
        {
            if (out_result != NULL)
            {
                memcpy(out_result,
                       serializer_ptr->contents + serializer_ptr->current_index,
                       n_bytes);
            }

            serializer_ptr->current_index += n_bytes;
            result                         = true;
        }
    }

    if (!result)
    {
        LOG_ERROR("Reading operation failed");
    }

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_read_curve_container(system_file_serializer    serializer,
                                                                    system_hashed_ansi_string object_manager_path,
                                                                    curve_container*          result_container)
{
    system_hashed_ansi_string                  curve_name              = NULL;
    bool                                       is_pre_post_behavior_on = false;
    uint32_t                                   n_segments              = 0;
    curve_container_envelope_boundary_behavior pre_behavior            = (curve_container_envelope_boundary_behavior) -1;
    curve_container_envelope_boundary_behavior post_behavior           = (curve_container_envelope_boundary_behavior) -1;
    bool                                       result                  = false;
    system_variant                             temp_variant            = NULL;
    system_variant                             temp_variant2           = NULL;
    system_variant                             temp_variant3           = NULL;
    system_variant_type                        variant_type            = (system_variant_type) -1;

    /* Read generic properties */
    if (!system_file_serializer_read_hashed_ansi_string(serializer,
                                                       &curve_name)          ||
        !system_file_serializer_read                   (serializer,
                                                        sizeof(variant_type),
                                                       &variant_type))
    {
        ASSERT_DEBUG_SYNC(false, "Reading operation failed");

        goto end;
    }

    /* Prepare variants we'll need later */
    temp_variant  = system_variant_create(variant_type);
    temp_variant2 = system_variant_create(variant_type);
    temp_variant3 = system_variant_create(variant_type);

    /* Instantiate the curve */
    if (*result_container == NULL)
    {
        *result_container = curve_container_create(curve_name,
                                                   GET_OBJECT_PATH(curve_name,
                                                                   OBJECT_TYPE_CURVE_CONTAINER,
                                                                   object_manager_path),
                                                   variant_type);

        if (*result_container == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Out of memory");

            goto end;
        }
    }

    /* Iterate through all dimensions */
    /* Read the number of segments and the default value */
    if (!system_file_serializer_read        (serializer,
                                             sizeof(n_segments),
                                            &n_segments)        ||
        !system_file_serializer_read_variant(serializer,
                                             temp_variant) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Reading operation failed");

        goto end;
    }

    /* Set the default value */
    if (!curve_container_set_default_value(*result_container,
                                           temp_variant) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not set default value");

        goto end;
    }

    /* Get pre-/post-boundary behavior properties */
    if (!system_file_serializer_read(serializer,
                                     sizeof(is_pre_post_behavior_on),
                                    &is_pre_post_behavior_on)        ||
        !system_file_serializer_read(serializer,
                                     sizeof(pre_behavior),
                                    &pre_behavior)                   ||
        !system_file_serializer_read(serializer,
                                     sizeof(post_behavior),
                                    &post_behavior) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Reading operation failed");

        goto end;
    }

    /* Iterate through all segments */
    for (uint32_t n_segment = 0;
                  n_segment < n_segments;
                ++n_segment)
    {
        /* Read general curve segment data */
        system_time        segment_start_time = 0;
        system_time        segment_end_time   = 0;
        curve_segment_type segment_type       = (curve_segment_type) -1;

        if (!system_file_serializer_read(serializer,
                                         sizeof(segment_start_time),
                                        &segment_start_time) ||
            !system_file_serializer_read(serializer,
                                         sizeof(segment_end_time),
                                        &segment_end_time)   ||
            !system_file_serializer_read(serializer,
                                         sizeof(segment_type),
                                        &segment_type) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Reading operation failed");

            goto end;
        }

        /* If this is a boolean-based curve, read per-segment threshold value. We will set it later. */
        float segment_threshold = -1;

        if (variant_type == SYSTEM_VARIANT_BOOL)
        {
            if (!system_file_serializer_read(serializer,
                                             sizeof(segment_threshold),
                                            &segment_threshold) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Reading operation failed");

                goto end;
            }
        }

        /* Read segment-specific data and instantiate the segment. */
        curve_segment_id spawned_segment_id = (curve_segment_id) -1;

        switch (segment_type)
        {
            case CURVE_SEGMENT_LERP:
            {
                /* Read start/end values */
                if (!system_file_serializer_read_variant(serializer,
                                                         temp_variant)  ||
                    !system_file_serializer_read_variant(serializer,
                                                         temp_variant2))
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");

                    goto end;
                }

                /* Spawn the segment */
                if (!curve_container_add_lerp_segment(*result_container,
                                                      segment_start_time,
                                                      segment_end_time,
                                                      temp_variant,
                                                      temp_variant2,
                                                     &spawned_segment_id) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not spawn LERP segment");

                    goto end;
                }

                break;
            }

            case CURVE_SEGMENT_STATIC:
            {
                /* Read value */
                if (!system_file_serializer_read_variant(serializer,
                                                         temp_variant))
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");

                    goto end;
                }

                /* Spawn the segment */
                if (!curve_container_add_static_value_segment(*result_container,
                                                              segment_start_time,
                                                              segment_end_time,
                                                              temp_variant,
                                                             &spawned_segment_id) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not spawn static value segment");

                    goto end;
                }

                break;
            }

            case CURVE_SEGMENT_TCB:
            {
                /* Iterate through all nodes and store properties */
                typedef struct
                {
                    float          bias;
                    float          continuity;
                    float          tension;
                    system_time    time;
                    system_variant value;
                } _node_descriptor;

                uint32_t                n_segment_nodes = 0;
                system_resizable_vector nodes           = system_resizable_vector_create(4);

                if (!system_file_serializer_read(serializer,
                                                 sizeof(n_segment_nodes),
                                                &n_segment_nodes) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");

                    goto end;
                }

                for (uint32_t n_node = 0;
                              n_node < n_segment_nodes;
                            ++n_node)
                {
                    curve_segment_node_id node_id = (curve_segment_node_id) - 1;

                    if (!system_file_serializer_read_variant(serializer,
                                                             temp_variant  /* bias */) ||
                        !system_file_serializer_read_variant(serializer,
                                                             temp_variant2 /* cont */) ||
                        !system_file_serializer_read_variant(serializer,
                                                             temp_variant3 /* tens */) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Reading operation failed");

                        goto end;
                    }

                    /* Node time / value */
                    system_time    node_time  = (system_time) -1;
                    system_variant node_value = system_variant_create(variant_type);

                    if (!system_file_serializer_read        (serializer,
                                                             sizeof(node_time),
                                                            &node_time)                ||
                        !system_file_serializer_read_variant(serializer,
                                                             node_value /* value */) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Reading operation failed");

                        goto end;
                    }

                    /* Store the node description */
                    _node_descriptor* new_node = new (std::nothrow) _node_descriptor;

                    if (new_node == NULL)
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Out of memory");

                        goto end;
                    }

                    system_variant_get_float(temp_variant,
                                            &new_node->bias);
                    system_variant_get_float(temp_variant2,
                                            &new_node->continuity);
                    system_variant_get_float(temp_variant3,
                                            &new_node->tension);

                    new_node->time  = node_time;
                    new_node->value = node_value;

                    system_resizable_vector_push(nodes,
                                                 new_node);
                } /* for (uint32_t n_node = 0; n_node < n_segment_nodes; ++n_node) */

                /* Now that we've read all the nodes, we can retrieve start & end node descriptors */
                _node_descriptor* first_node  = NULL;
                _node_descriptor* second_node = NULL;

                if (!system_resizable_vector_get_element_at(nodes,
                                                            0,
                                                           &first_node) ||
                    !system_resizable_vector_get_element_at(nodes,
                                                            1,
                                                           &second_node) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve node descriptors");

                    goto end;
                }

                /* Spawn the segment */
                if (!curve_container_add_tcb_segment(*result_container,
                                                     first_node->time,
                                                     second_node->time,
                                                     first_node->value,
                                                     first_node->tension,
                                                     first_node->continuity,
                                                     first_node->bias,
                                                     second_node->value,
                                                     second_node->tension,
                                                     second_node->continuity,
                                                     second_node->bias,
                                                    &spawned_segment_id) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not create TCB curve");

                    goto end;
                }

                /* Add all remaining nodes */
                curve_segment tcb_segment = curve_container_get_segment(*result_container,
                                                                         spawned_segment_id);

                if (tcb_segment == NULL)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve spawned TCB curve segment");

                    goto end;
                }

                for (uint32_t n_node = 2;
                              n_node < n_segment_nodes;
                            ++n_node)
                {
                    _node_descriptor* current_node = NULL;

                    if (!system_resizable_vector_get_element_at(nodes,
                                                                n_node,
                                                               &current_node) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve node descriptor");

                        goto end;
                    }

                    /* Add the node first */
                    curve_segment_node_id new_node_id = (curve_segment_node_id) -1;

                    if (!curve_segment_add_node(tcb_segment,
                                                current_node->time,
                                                current_node->value,
                                               &new_node_id) )
                    {
#if 0
                        /* It is OK if we get here, assuming that the node has already been created. */
                        if (!curve_segment_get_node_at_time(tcb_segment,
                                                            current_node->time,
                                                           &new_node_id) )
#endif
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Could not add a node to TCB curve segment");

                            goto end;
                        } /* if (node does not exist) */
                    } /* if (failed to add a node) */

                    /* Adjust TCB properties for the node */
                    system_variant_set_float(temp_variant,
                                             current_node->bias);
                    system_variant_set_float(temp_variant2,
                                             current_node->continuity);
                    system_variant_set_float(temp_variant3,
                                             current_node->tension);

                    if (!curve_segment_modify_node_property(tcb_segment,
                                                            new_node_id,
                                                            CURVE_SEGMENT_NODE_PROPERTY_BIAS,
                                                            temp_variant)                           ||
                        !curve_segment_modify_node_property(tcb_segment,
                                                            new_node_id,
                                                            CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY,
                                                            temp_variant2)                          ||
                        !curve_segment_modify_node_property(tcb_segment,
                                                            new_node_id,
                                                            CURVE_SEGMENT_NODE_PROPERTY_TENSION,
                                                            temp_variant3) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not adjust TCB settings for a TCB curve segment node.");

                        goto end;
                    }
                } /* for (int n_node = 2; n_node < n_segment_nodes; ++n_node)*/

                /* All right - we're safe to release the resizable vector now */
                if (nodes != NULL)
                {
                    while (true)
                    {
                        _node_descriptor* node = NULL;

                        if (!system_resizable_vector_pop(nodes,
                                                        &node) )
                        {
                            break;
                        }

                        delete node;
                    }

                    system_resizable_vector_release(nodes);

                    nodes = NULL;
                }

                break;
            } /* case CURVE_SEGMENT_TCB: */

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized segment type [%d]",
                                  segment_type);
            }
        } /* switch (segment_type) */

        /* If the segment relies on boolean values, make sure the threshold is set */
        if (variant_type == SYSTEM_VARIANT_BOOL)
        {
            curve_container_set_segment_property(*result_container,
                                                 spawned_segment_id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_THRESHOLD,
                                                &segment_threshold);
        }
    } /* for (uint32_t n_segment = 0; n_segment < n_segments; ++n_segment)*/

    /* Enforce the properties */
    if (is_pre_post_behavior_on)
    {
        curve_container_set_property(*result_container,
                                     CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR,
                                    &pre_behavior);
        curve_container_set_property(*result_container,
                                     CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR,
                                    &post_behavior);
        curve_container_set_property(*result_container,
                                     CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS,
                                    &is_pre_post_behavior_on);
    }

    result = true;

end:
    if (temp_variant != NULL)
    {
        system_variant_release(temp_variant);
    }

    if (temp_variant2 != NULL)
    {
        system_variant_release(temp_variant2);
    }

    if (temp_variant3 != NULL)
    {
        system_variant_release(temp_variant3);
    }

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_read_hashed_ansi_string(system_file_serializer     serializer,
                                                                       system_hashed_ansi_string* result_string)
{
    int  n_characters = 0;
    bool result       = false;

    if (system_file_serializer_read(serializer,
                                    sizeof(n_characters),
                                   &n_characters) )
    {
        char* buffer = new (std::nothrow) char[n_characters + 1];

        ASSERT_ALWAYS_SYNC(buffer != NULL,
                           "Out of memory");

        if (buffer != NULL)
        {
            if (system_file_serializer_read(serializer,
                                            n_characters,
                                            buffer))
            {
                buffer[n_characters] = 0;

                *result_string = system_hashed_ansi_string_create(buffer);
                result         = true;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Reading operation failed");
            }

            /* Release the buffer */
            delete [] buffer;
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Reading operation failed");
    }

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_read_matrix4x4(system_file_serializer serializer,
                                                              system_matrix4x4*      out_result)
{
    float matrix_row_major_data[16];
    bool  result = true;

    if (!system_file_serializer_read(serializer,
                                     sizeof(matrix_row_major_data),
                                     matrix_row_major_data) )
    {
        result = false;

        goto end;
    }

    *out_result = system_matrix4x4_create();

    if (*out_result == NULL)
    {
        goto end;
    }

    system_matrix4x4_set_from_row_major_raw(*out_result,
                                            matrix_row_major_data);

end:
    ASSERT_DEBUG_SYNC(result,
                      "4x4 matrix data serialization failed");

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_read_variant(system_file_serializer serializer,
                                                            system_variant         out_result)
{
    bool                result = false;
    system_variant_type variant_type;

    if (system_file_serializer_read(serializer,
                                    sizeof(system_variant_type),
                                   &variant_type) )
    {
        switch(variant_type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                int length = 0;

                if (system_file_serializer_read(serializer,
                                                sizeof(length),
                                               &length) )
                {
                    char* buffer = new (std::nothrow) char[length + 1];

                    if (buffer != NULL)
                    {
                        if (system_file_serializer_read(serializer,
                                                        length,
                                                        buffer) )
                        {
                            buffer[length] = 0;

                            system_variant_set_ansi_string(out_result,
                                                           system_hashed_ansi_string_create(buffer),
                                                           false);

                            result = true;
                        }
                        else
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Reading operation failed");
                        }

                        delete [] buffer;
                        buffer = NULL;
                    }
                    else
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Out of memory");
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");
                }

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                bool value = false;

                if (system_file_serializer_read(serializer,
                                                sizeof(value),
                                               &value) )
                {
                    result = true;

                    system_variant_set_boolean(out_result,
                                               value);
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");
                }

                break;
            }

            case SYSTEM_VARIANT_INTEGER:
            case SYSTEM_VARIANT_FLOAT:
            {
                char buffer[4];

                if (system_file_serializer_read(serializer,
                                                sizeof(buffer),
                                                buffer) )
                {
                    result = true;

                    if (variant_type == SYSTEM_VARIANT_INTEGER)
                    {
                        system_variant_set_integer(out_result,
                                                   *((int*)buffer),
                                                   false);
                    }
                    else
                    {
                        system_variant_set_float(out_result,
                                                 *((float*)buffer) );
                    }

                    break;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Reading operation failed");
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized variant type [%d]",
                                  variant_type);
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Reading operation failed");
    }

    return result;
}

/** Please see header file for specification */
PUBLIC EMERALD_API void system_file_serializer_set_property(system_file_serializer          serializer,
                                                            system_file_serializer_property property,
                                                            void*                           data)
{
    _system_file_serializer* serializer_ptr = (_system_file_serializer*) serializer;

    switch (property)
    {
        case SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME:
        {
            serializer_ptr->file_name = *(system_hashed_ansi_string*) data;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized system_file_serializer_property value.");
        }
    } /* switch (property) */
}

/** Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_write(system_file_serializer serializer,
                                                     uint32_t               n_bytes,
                                                     const void*            data_to_write)
{
    bool                     result         = false;
    _system_file_serializer* serializer_ptr = (_system_file_serializer*) serializer;

    if (!serializer_ptr->for_reading)
    {
        uint32_t new_capacity = serializer_ptr->writing_capacity;

        if (serializer_ptr->file_size + n_bytes >= new_capacity)
        {
            while (new_capacity < (serializer_ptr->file_size + n_bytes) )
            {
                new_capacity <<= 1;
            }

            /* Got to realloc */
            char* new_contents = new (std::nothrow) char[new_capacity];

            if (new_contents != NULL)
            {
                /* Copy existing data */
                memcpy(new_contents,
                       serializer_ptr->contents,
                       serializer_ptr->file_size);

                delete [] serializer_ptr->contents;
                serializer_ptr->contents         = new_contents;
                serializer_ptr->writing_capacity = new_capacity;
            }
            else
            {
                /* Sigh, we ran out of memory. Write down what we have queued up so far,
                 * and reset the offsets.
                 *
                 * NOTE: This happens in LW exporter plug-in for large scenes,
                 *       so until we move to AWE, we need this.
                 */
                _system_file_serializer_write_down_data_to_file(serializer_ptr);
            }
        }

        /* Append new data */
        memcpy(serializer_ptr->contents + serializer_ptr->file_size,
               data_to_write,
               n_bytes);

        serializer_ptr->file_size += n_bytes;
        result                     = true;
    }

    ASSERT_DEBUG_SYNC(result,
                      "Writing operation failed");

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_write_curve_container(      system_file_serializer serializer,
                                                                     const curve_container        curve)
{
    system_hashed_ansi_string                  curve_name              = NULL;
    bool                                       is_pre_post_behavior_on = false;
    uint32_t                                   n_segments              = 0;
    curve_container_envelope_boundary_behavior pre_behavior            = (curve_container_envelope_boundary_behavior) -1;
    curve_container_envelope_boundary_behavior post_behavior           = (curve_container_envelope_boundary_behavior) -1;
    bool                                       result                  = false;
    system_variant_type                        variant_type            = (system_variant_type) -1;

    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_DATA_TYPE,
                                &variant_type);

    system_variant temp_variant  = system_variant_create(variant_type);
    system_variant temp_variant2 = system_variant_create(variant_type);
    system_variant temp_variant3 = system_variant_create(variant_type);

    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_NAME,
                                &curve_name);

    /* Store generic properties */
    if (!system_file_serializer_write_hashed_ansi_string(serializer,
                                                         curve_name)            ||
        !system_file_serializer_write                   (serializer,
                                                         sizeof(variant_type),
                                                        &variant_type))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Writing operation failed");

        goto end;
    }

    /* Iterate through all dimensions */
    /* Get amount of segments */
    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_N_SEGMENTS,
                                &n_segments);

    /* Get default value */
    if (!curve_container_get_default_value(curve,
                                           false,
                                           temp_variant) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot query default curve dimension value");

        goto end;
    }

    /* Get pre-/post-boundary behavior properties */
    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_PRE_BEHAVIOR,
                                &pre_behavior);
    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_POST_BEHAVIOR,
                                &post_behavior);
    curve_container_get_property(curve,
                                 CURVE_CONTAINER_PROPERTY_PRE_POST_BEHAVIOR_STATUS,
                                &is_pre_post_behavior_on);

    /* Stash them */
    if (!system_file_serializer_write        (serializer,
                                              sizeof(n_segments),
                                             &n_segments)                     ||
        !system_file_serializer_write_variant(serializer,
                                              temp_variant)                   ||
        !system_file_serializer_write        (serializer,
                                              sizeof(is_pre_post_behavior_on),
                                             &is_pre_post_behavior_on)        ||
        !system_file_serializer_write        (serializer,
                                              sizeof(pre_behavior),
                                             &pre_behavior)                   ||
        !system_file_serializer_write        (serializer,
                                              sizeof(post_behavior),
                                             &post_behavior))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Writing operation failed");

        goto end;
    }

    /* Iterate through all segments */
    for (uint32_t n_segment = 0;
                  n_segment < n_segments;
                ++n_segment)
    {
        /* Retrieve curve segment id */
        curve_segment_id segment_id = -1;

        if (!curve_container_get_segment_id_for_nth_segment(curve,
                                                            n_segment,
                                                           &segment_id) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot query curve segment id");

            goto end;
        }

        /* Retrieve curve segment details */
        system_time        segment_start_time = 0;
        system_time        segment_end_time   = 0;
        curve_segment_type segment_type       = (curve_segment_type) -1;

        curve_container_get_segment_property(curve,
                                             segment_id,
                                             CURVE_CONTAINER_SEGMENT_PROPERTY_END_TIME,
                                            &segment_end_time);
        curve_container_get_segment_property(curve,
                                             segment_id,
                                             CURVE_CONTAINER_SEGMENT_PROPERTY_START_TIME,
                                            &segment_start_time);
        curve_container_get_segment_property(curve,
                                             segment_id,
                                             CURVE_CONTAINER_SEGMENT_PROPERTY_TYPE,
                                            &segment_type);

        /* Stash them */
        if (!system_file_serializer_write(serializer,
                                          sizeof(segment_start_time),
                                         &segment_start_time)        ||
            !system_file_serializer_write(serializer,
                                          sizeof(segment_end_time),
                                         &segment_end_time)          ||
            !system_file_serializer_write(serializer,
                                          sizeof(segment_type),
                                         &segment_type) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Writing operation failed");

            goto end;
        }

        /* If this is a boolean-based curve, store per-segment threshold */
        if (variant_type == SYSTEM_VARIANT_BOOL)
        {
            float segment_threshold = -1;

            curve_container_get_segment_property(curve,
                                                 segment_id,
                                                 CURVE_CONTAINER_SEGMENT_PROPERTY_THRESHOLD,
                                                &segment_threshold);

            if (!system_file_serializer_write(serializer,
                                              sizeof(segment_threshold),
                                             &segment_threshold) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Writing operation failed");

                goto end;
            }
        }

        /* Retrieve and store segment-specific details */
        curve_segment segment = curve_container_get_segment(curve,
                                                            segment_id);

        if (segment == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve curve segment instance");

            goto end;
        }

        switch (segment_type)
        {
            case CURVE_SEGMENT_LERP:
            {
                system_time temp;

                if (!curve_segment_get_node(segment,
                                            0,
                                           &temp,
                                            temp_variant)   ||
                    !curve_segment_get_node(segment,
                                            1,
                                           &temp,
                                            temp_variant2) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Cannot query LERP segment node values");

                    goto end;
                }

                if (!system_file_serializer_write_variant(serializer,
                                                          temp_variant) ||
                    !system_file_serializer_write_variant(serializer,
                                                          temp_variant2))
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");

                    goto end;
                }

                break;
            }

            case CURVE_SEGMENT_STATIC:
            {
                system_time temp;

                if (!curve_segment_get_node(segment,
                                            0,
                                           &temp,
                                           temp_variant) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Cannot query static segment node value");

                    goto end;
                }

                if (!system_file_serializer_write_variant(serializer,
                                                          temp_variant) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");

                    goto end;
                }

                break;
            }

            case CURVE_SEGMENT_TCB:
            {
                /* Iterate through all nodes and store properties */
                uint32_t n_segment_nodes = 0;

                curve_container_get_segment_property(curve,
                                                     segment_id,
                                                     CURVE_CONTAINER_SEGMENT_PROPERTY_N_NODES,
                                                    &n_segment_nodes);

                if (!system_file_serializer_write(serializer,
                                                  sizeof(n_segment_nodes),
                                                 &n_segment_nodes) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");

                    goto end;
                }

                for (uint32_t n_node = 0;
                              n_node < n_segment_nodes;
                            ++n_node)
                {
                    curve_segment_node_id node_id = (curve_segment_node_id) - 1;

                    if (!curve_container_get_node_id_for_node_at(curve,
                                                                 segment_id,
                                                                 n_node,
                                                                &node_id)                                 ||
                        !curve_container_get_node_property      (curve,
                                                                 segment_id,
                                                                 node_id,
                                                                 CURVE_SEGMENT_NODE_PROPERTY_BIAS,
                                                                 temp_variant)                            ||
                        !curve_container_get_node_property      (curve,
                                                                 segment_id,
                                                                 node_id,
                                                                 CURVE_SEGMENT_NODE_PROPERTY_CONTINUITY,
                                                                 temp_variant2)                           ||
                        !curve_container_get_node_property      (curve,
                                                                 segment_id,
                                                                 node_id,
                                                                 CURVE_SEGMENT_NODE_PROPERTY_TENSION,
                                                                 temp_variant3) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Cannot query TCB curve segment node property");

                        goto end;
                    }

                    if (!system_file_serializer_write_variant(serializer,
                                                              temp_variant)  ||
                        !system_file_serializer_write_variant(serializer,
                                                              temp_variant2) ||
                        !system_file_serializer_write_variant(serializer,
                                                              temp_variant3))
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Writing operation failed");

                        goto end;
                    }

                    /* Node time / value */
                    system_time node_time = (system_time) -1;

                    if (!curve_segment_get_node(segment,
                                                node_id,
                                               &node_time,
                                                temp_variant) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Cannot query TCB curve segment node general properties");

                        goto end;
                    }

                    if (!system_file_serializer_write        (serializer,
                                                              sizeof(node_time),
                                                             &node_time)         ||
                        !system_file_serializer_write_variant(serializer,
                                                              temp_variant) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Writing operation failed");

                        goto end;
                    }
                } /* for (uint32_t n_node = 0; n_node < n_segment_nodes; ++n_node) */

                break;
            } /* case CURVE_SEGMENT_TCB: */

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized segment type [%d]",
                                  segment_type);
            }
        } /* switch (segment_type) */
    } /* for (uint32_t n_segment = 0; n_segment < n_segments; ++n_segment)*/

    result = true;

end:
    if (temp_variant != NULL)
    {
        system_variant_release(temp_variant);
    }

    if (temp_variant2 != NULL)
    {
        system_variant_release(temp_variant2);
    }

    if (temp_variant3 != NULL)
    {
        system_variant_release(temp_variant3);
    }

    return result;
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_write_hashed_ansi_string(system_file_serializer    serializer,
                                                                        system_hashed_ansi_string string)
{
    bool result        = false;
    int  string_length = system_hashed_ansi_string_get_length(string);

    if (system_file_serializer_write(serializer,
                                     sizeof(string_length),
                                    &string_length))
    {
        if (system_file_serializer_write(serializer,
                                         string_length,
                                         system_hashed_ansi_string_get_buffer(string) ))
        {
            result = true;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Writing operation failed");
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Writing operation failed");
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool system_file_serializer_write_matrix4x4(system_file_serializer serializer,
                                                               system_matrix4x4       matrix)
{
    const float* matrix_data = system_matrix4x4_get_row_major_data(matrix);

    return system_file_serializer_write(serializer,
                                        sizeof(float) * 16,
                                        matrix_data);
}

/* Please see header file for specification */
PUBLIC EMERALD_API bool system_file_serializer_write_variant(system_file_serializer serializer,
                                                             system_variant         variant)
{
    bool                result       = false;
    system_variant_type variant_type = system_variant_get_type(variant);

    if (system_file_serializer_write(serializer,
                                     sizeof(variant_type),
                                    &variant_type) )
    {
        switch(variant_type)
        {
            case SYSTEM_VARIANT_ANSI_STRING:
            {
                system_hashed_ansi_string string        = NULL;
                int                       string_length = 0;

                system_variant_get_ansi_string(variant,
                                               false,
                                              &string);

                string_length = system_hashed_ansi_string_get_length(string);

                if (system_file_serializer_write(serializer,
                                                 sizeof(string_length),
                                                &string_length) )
                {
                    if (system_file_serializer_write(serializer,
                                                     string_length,
                                                     system_hashed_ansi_string_get_buffer(string) ))
                    {
                        result = true;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Writing operation failed");
                    }
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");
                }

                break;
            }

            case SYSTEM_VARIANT_BOOL:
            {
                bool variant_value = false;

                system_variant_get_boolean(variant,
                                          &variant_value);

                if (system_file_serializer_write(serializer,
                                                 sizeof(variant_value),
                                                &variant_value) )
                {
                    result = true;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");
                }

                break;
            }

            case SYSTEM_VARIANT_FLOAT:
            case SYSTEM_VARIANT_INTEGER:
            {
                char buffer[4] = {0};

                if (variant_type == SYSTEM_VARIANT_FLOAT)
                {
                    system_variant_get_float(variant,
                                             (float*)buffer);
                }
                else
                {
                    system_variant_get_integer(variant,
                                               (int*)buffer);
                }

                if (system_file_serializer_write(serializer,
                                                 sizeof(buffer),
                                                 buffer))
                {
                    result = true;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Writing operation failed");
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized variant type [%d]",
                                  variant_type);
            }
        }
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Writing operation failed");
    }

    return result;
}
