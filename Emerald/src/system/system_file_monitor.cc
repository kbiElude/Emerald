/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_event.h"
#include "system/system_file_monitor.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_threads.h"


typedef struct _system_file_monitor_directory_callback
{
    system_hashed_ansi_string directory;
    system_hash64map          filename_to_callback_map;
    system_event              wait_event;

    explicit _system_file_monitor_directory_callback(system_hashed_ansi_string in_directory,
                                                     system_event              in_wait_event);
            ~_system_file_monitor_directory_callback();
} _system_file_monitor_directory_callback;

typedef struct _system_file_monitor_file_callback
{
    PFNFILECHANGEDETECTEDPROC callback;
    void*                     callback_user_arg;
    system_hashed_ansi_string directory;
    system_hashed_ansi_string filename;
    FILETIME                  last_write_time;

    explicit _system_file_monitor_file_callback(PFNFILECHANGEDETECTEDPROC in_callback,
                                                void*                     in_callback_user_arg,
                                                system_hashed_ansi_string in_directory,
                                                system_hashed_ansi_string in_filename);

} _system_file_monitor_file_callback;

typedef struct _system_file_monitor
{
    system_critical_section cs;
    system_thread           monitor_thread;
    system_event            monitor_thread_please_die_event;
    system_event            monitor_thread_wait_event;
    system_hash64map        monitored_directory_name_hash_to_callback_map; /* maps directory name hashes to _system_file_monitor_directory_callback instances */

    system_event* wait_table;
    unsigned int  wait_table_n_entries;
    system_event  wait_table_needs_an_update_ack_event;
    system_event  wait_table_needs_an_update_event;
    unsigned int  wait_table_size;
    system_event  wait_table_updated_event;

     _system_file_monitor();
    ~_system_file_monitor();
} _system_file_monitor;


/** Private variables */
PRIVATE _system_file_monitor* file_monitor_ptr = NULL;


/** Forward declarations */
PRIVATE void _system_file_monitor_get_directory_and_file_for_file_name_with_path(system_hashed_ansi_string           file_name_with_path,
                                                                                 system_hashed_ansi_string*          out_file_path_ptr,
                                                                                 system_hashed_ansi_string*          out_file_name_ptr);
PRIVATE void _system_file_monitor_monitor_thread_entrypoint                     (void*                               unused);
PRIVATE bool _system_file_monitor_register_file_callback                        (system_hashed_ansi_string           file_name_with_path,
                                                                                 PFNFILECHANGEDETECTEDPROC           pfn_file_changed_proc,
                                                                                 void*                               file_changed_proc_user_arg);
PRIVATE void _system_file_monitor_unregister_file_callback                      (
                                                                                 _system_file_monitor_file_callback* callback_ptr);

#ifdef _WIN32
    PRIVATE bool _system_file_monitor_get_last_write_time(system_hashed_ansi_string file_name_with_path,
                                                          FILETIME*                 out_result_ptr);
#endif


/** TODO */
_system_file_monitor_directory_callback::_system_file_monitor_directory_callback(system_hashed_ansi_string in_directory,
                                                                                 system_event              in_wait_event)
{
    ASSERT_DEBUG_SYNC(in_directory != NULL,
                      "Directory is NULL");

    directory                = in_directory;
    filename_to_callback_map = system_hash64map_create(sizeof(_system_file_monitor_file_callback*) );
    wait_event               = in_wait_event;
}

/** TODO */
_system_file_monitor_directory_callback::~_system_file_monitor_directory_callback()
{
    if (filename_to_callback_map != NULL)
    {
        unsigned int n_filenames = 0;

        system_hash64map_get_property(filename_to_callback_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_filenames);

        for (unsigned int n_filename = 0;
                          n_filename < n_filenames;
                        ++n_filename)
        {
            _system_file_monitor_file_callback* callback_ptr = NULL;

            if (!system_hash64map_get_element_at(filename_to_callback_map,
                                                 n_filename,
                                                &callback_ptr,
                                                 NULL) ) /* result_hash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve file call-back descriptor");

                continue;
            }

            delete callback_ptr;
            callback_ptr = NULL;
        } /* for (all stores key/value pairs) */

        system_hash64map_release(filename_to_callback_map);
        filename_to_callback_map = NULL;
    } /* if (filename_to_callback_map != NULL) */

    if (wait_event != NULL)
    {
        system_event_release(wait_event);

        wait_event = NULL;
    } /* if (wait_event != NULL) */
}

/** TODO */
_system_file_monitor_file_callback::_system_file_monitor_file_callback(PFNFILECHANGEDETECTEDPROC in_callback,
                                                                       void*                     in_callback_user_arg,
                                                                       system_hashed_ansi_string in_directory,
                                                                       system_hashed_ansi_string in_filename)
{
    ASSERT_DEBUG_SYNC(in_callback != NULL,
                      "Call-back function pointer is NULL");
    ASSERT_DEBUG_SYNC(in_directory != NULL,
                      "Directory is NULL");
    ASSERT_DEBUG_SYNC(in_filename != NULL,
                      "Filename is NULL");

    callback          = in_callback;
    callback_user_arg = in_callback_user_arg;
    directory         = in_directory;
    filename          = in_filename;
}

/** TODO */
_system_file_monitor::_system_file_monitor()
{
    cs                                            = system_critical_section_create();
    monitor_thread                                = (system_thread) 0;
    monitor_thread_please_die_event               = system_event_create    (true); /* manual_reset */
    monitor_thread_wait_event                     = NULL;
    monitored_directory_name_hash_to_callback_map = system_hash64map_create(sizeof(_system_file_monitor_directory_callback*) );

    wait_table_needs_an_update_ack_event = system_event_create(true); /* manual_reset */
    wait_table_needs_an_update_event     = system_event_create(true); /* manual_reset */
    wait_table_updated_event             = system_event_create(true); /* manual_reset */

    /* By default, the wait table should only consist of the "please die" and "wait table needs an update" event. */
    wait_table = new (std::nothrow) system_event[2];

    ASSERT_ALWAYS_SYNC(wait_table != NULL,
                       "Out of memory");

    wait_table[0]        = monitor_thread_please_die_event;
    wait_table[1]        = wait_table_needs_an_update_event;
    wait_table_n_entries = 2;
    wait_table_size      = 2;
}

/** TODO */
_system_file_monitor::~_system_file_monitor()
{
    /* Make sure the monitor thread is killed before we continue */
    system_event_set        (monitor_thread_please_die_event);
    system_event_wait_single(monitor_thread_wait_event,
                             SYSTEM_TIME_INFINITE);

    /* Release other stuff */
    if (cs != NULL)
    {
        system_critical_section_release(cs);

        cs = NULL;
    } /* if (cs != NULL) */

    if (monitor_thread_please_die_event != NULL)
    {
        system_event_release(monitor_thread_please_die_event);

        monitor_thread_please_die_event = NULL;
    } /* if (monitor_thread_please_die_event != NULL) */

    if (monitor_thread_wait_event != NULL)
    {
        system_event_release(monitor_thread_wait_event);

        monitor_thread_wait_event = NULL;
    } /* if (monitor_thread_wait_event != NULL) */

    if (monitored_directory_name_hash_to_callback_map != NULL)
    {
        unsigned int n_monitored_directory_names = 0;

        system_hash64map_get_property(monitored_directory_name_hash_to_callback_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_monitored_directory_names);

        for (unsigned int n_monitored_directory_name = 0;
                          n_monitored_directory_name < n_monitored_directory_names;
                        ++n_monitored_directory_name)
        {
            _system_file_monitor_directory_callback* callback_ptr = NULL;

            if (!system_hash64map_get_element_at(monitored_directory_name_hash_to_callback_map,
                                                 n_monitored_directory_name,
                                                &callback_ptr,
                                                 NULL) ) /* result_hash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve hash-map item at index [%d]",
                                  n_monitored_directory_name);

                continue;
            }

            delete callback_ptr;
            callback_ptr = NULL;
        } /* for (all monitored directory names) */

        system_hash64map_release(monitored_directory_name_hash_to_callback_map);
        monitored_directory_name_hash_to_callback_map = NULL;
    } /* if (monitored_directory_name_hash_to_callback_map != NULL) */

    if (wait_table != NULL)
    {
        delete [] wait_table;

        wait_table = NULL;
    }

    if (wait_table_needs_an_update_ack_event != NULL)
    {
        system_event_release(wait_table_needs_an_update_ack_event);

        wait_table_needs_an_update_ack_event = NULL;
    } /* if (wait_table_needs_an_update_ack_event != NULL) */

    if (wait_table_needs_an_update_event != NULL)
    {
        system_event_release(wait_table_needs_an_update_event);

        wait_table_needs_an_update_event = NULL;
    } /* if (wait_table_needs_an_update_event != NULL) */

    if (wait_table_updated_event != NULL)
    {
        system_event_release(wait_table_updated_event);

        wait_table_updated_event = NULL;
    } /* if (wait_table_updated_event != NULL) */
}


/** TODO */
PRIVATE void _system_file_monitor_get_directory_and_file_for_file_name_with_path(system_hashed_ansi_string  file_name_with_path,
                                                                                 system_hashed_ansi_string* out_file_path_ptr,
                                                                                 system_hashed_ansi_string* out_file_name_ptr)
{
    const char* file_name_with_path_raw  = system_hashed_ansi_string_get_buffer(file_name_with_path);
    DWORD       n_temp_buffer_bytes_used = 0;
    char        temp_buffer[MAX_PATH];
    char*       temp_buffer_file_name    = NULL;

    n_temp_buffer_bytes_used = ::GetFullPathName(file_name_with_path_raw,
                                                 sizeof(temp_buffer),
                                                 temp_buffer,
                                                &temp_buffer_file_name);

    ASSERT_DEBUG_SYNC(n_temp_buffer_bytes_used > 0 &&
                      n_temp_buffer_bytes_used < sizeof(temp_buffer),
                      "GetFullPathName() failed.");

    *out_file_name_ptr     = system_hashed_ansi_string_create(temp_buffer_file_name);
    *temp_buffer_file_name = 0;
    *out_file_path_ptr     = system_hashed_ansi_string_create(temp_buffer);
}

#ifdef _WIN32
    /** TODO */
    PRIVATE bool _system_file_monitor_get_last_write_time(system_hashed_ansi_string file_name_with_path,
                                                          FILETIME*                 out_result_ptr)
    {
        HANDLE      file_handle             = INVALID_HANDLE_VALUE;
        const char* file_name_with_path_raw = system_hashed_ansi_string_get_buffer(file_name_with_path);
        bool        result                  = false;

        memset(out_result_ptr,
               0,
               sizeof(*out_result_ptr) );

        /* Open a file handle fro the specified file */
        file_handle = ::CreateFileA(file_name_with_path_raw,
                                    GENERIC_READ,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, /* lpSecurityAttributes */
                                    OPEN_EXISTING,
                                    0,     /* dwFlagsAndAttributes */
                                    NULL); /* hTemplateFile */

        if (file_handle == INVALID_HANDLE_VALUE)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not open file [%s]",
                               file_name_with_path_raw);

            goto end;
        }

        if (::GetFileTime(file_handle,
                          NULL,           /* lpCreationTime */
                          NULL,           /* lpLastAccessTime */
                          out_result_ptr) /* lpLastWriteTime */ == FALSE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not query last write time for file [%s]",
                              file_name_with_path_raw);

            goto end;
        }

        result = true;

    end:
        if (file_handle != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(file_handle);

            file_handle = NULL;
        }

        return result;
    }
#endif

/** TODO */
PRIVATE void _system_file_monitor_monitor_thread_entrypoint(void* unused)
{
    LOG_INFO("File monitor thread started.");

    ASSERT_DEBUG_SYNC(file_monitor_ptr != NULL,
                      "File monitor instance is NULL");

    while (true)
    {
        size_t n_event = system_event_wait_multiple(file_monitor_ptr->wait_table,
                                                    file_monitor_ptr->wait_table_n_entries,
                                                    false, /* wait_on_all_objects */
                                                    SYSTEM_TIME_INFINITE,
                                                    NULL); /* out_has_timed_out_ptr */

        /* Zeroth event corresponds to the event set when it's high time this thread died */
        if (n_event == 0)
        {
            break;
        } /* if (n_event == 0) */
        else
        if (n_event == 1)
        {
            /* First event: need to update the wait array from another thread */
            system_event_set        (file_monitor_ptr->wait_table_needs_an_update_ack_event);
            system_event_wait_single(file_monitor_ptr->wait_table_updated_event);

            system_event_reset(file_monitor_ptr->wait_table_needs_an_update_ack_event);
            system_event_reset(file_monitor_ptr->wait_table_needs_an_update_event);
            system_event_reset(file_monitor_ptr->wait_table_updated_event);
        }
        else
        {
            /* This must be a system call-back.
             *
             * Under Windows, we are not informed about specifically which file has caused the notification.
             * We therefore need to enumerate all files monitored in the specified directory and check whose
             * timestamp has changed.
             */
            system_critical_section_enter(file_monitor_ptr->cs);
            {
                _system_file_monitor_directory_callback* callback_ptr = NULL;

                if (!system_hash64map_get_element_at(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                                     (n_event - 2),
                                                    &callback_ptr, /* result_element */
                                                     NULL) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve call-back descriptor");
                }
                else
                {
                    bool     has_found_modified_file = false;
                    uint32_t n_files_in_directory    = 0;
                    HANDLE   wait_event_raw_handle   = NULL;

                    system_hash64map_get_property(callback_ptr->filename_to_callback_map,
                                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                 &n_files_in_directory);

                    for (uint32_t n_file = 0;
                                  n_file < n_files_in_directory;
                                ++n_file)
                    {
                        _system_file_monitor_file_callback* current_file_callback_ptr = NULL;
                        FILETIME                            current_file_last_write_time;

                        if (!system_hash64map_get_element_at(callback_ptr->filename_to_callback_map,
                                                             n_file,
                                                            &current_file_callback_ptr,
                                                             NULL) ) /* result_hash */
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Could not retrieve file call-back descriptor at index [%d]",
                                              n_file);

                            continue;
                        }

                        if (!_system_file_monitor_get_last_write_time(current_file_callback_ptr->filename,
                                                                     &current_file_last_write_time) )
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Could not check the 'last write time' proprety of the file [%s]",
                                              system_hashed_ansi_string_get_buffer(current_file_callback_ptr->filename) );

                            continue;
                        }

                        if (current_file_callback_ptr->last_write_time.dwLowDateTime  != current_file_last_write_time.dwLowDateTime  &&
                            current_file_callback_ptr->last_write_time.dwHighDateTime != current_file_last_write_time.dwHighDateTime)
                        {
                            /* We have a match! */
                            current_file_callback_ptr->callback(current_file_callback_ptr->filename,
                                                                current_file_callback_ptr->callback_user_arg);

                            has_found_modified_file = true;
                            break;
                        }
                    } /* for (all files monitored in the directory) */

                    /* If has_found_modified_file is false at this point, it usually means that another file we're not currently modifying,
                     * but which is located in the monitored directory, has been altered. Ignore.
                     */
                }

                /* Restart the monitoring process */
                if (::FindNextChangeNotification(callback_ptr->wait_event) == FALSE)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not restart file monitoring process");
                }
            }
            system_critical_section_leave(file_monitor_ptr->cs);
        }
    } /* while (true) */

    LOG_INFO("File monitor thread exiting.");
}

/** TODO */
PRIVATE bool _system_file_monitor_register_file_callback(system_hashed_ansi_string file_name_with_path,
                                                         PFNFILECHANGEDETECTEDPROC pfn_file_changed_proc,
                                                         void*                     file_changed_proc_user_arg)
{
    _system_file_monitor_directory_callback* directory_callback_ptr          = NULL;
    _system_file_monitor_file_callback*      file_callback_ptr               = NULL;
    system_hashed_ansi_string                file_name_without_path_has      = NULL;
    system_hash64                            file_name_without_path_has_hash;
    const system_hash64                      file_name_with_path_hash        = system_hashed_ansi_string_get_hash  (file_name_with_path);
    const char*                              file_name_with_path_raw         = system_hashed_ansi_string_get_buffer(file_name_with_path);
    system_hashed_ansi_string                file_path_has                   = NULL;
    system_hash64                            file_path_has_hash;
    const char*                              file_path_raw                   = NULL;
    bool                                     result                          = false;
    system_event                             result_wait_event               = NULL;

    /* Retrieve last write time for the specified file */
    FILETIME last_write_time;

    if (!_system_file_monitor_get_last_write_time(file_name_with_path,
                                                 &last_write_time) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot retrieve last write timestamp for file [%s]",
                          file_name_with_path_raw);

        goto end;
    }

    /* Extract path and the file name from the input argument */
    _system_file_monitor_get_directory_and_file_for_file_name_with_path(file_name_with_path,
                                                                       &file_path_has,
                                                                       &file_name_without_path_has);

     file_name_without_path_has_hash = system_hashed_ansi_string_get_hash  (file_name_without_path_has);
     file_path_has_hash              = system_hashed_ansi_string_get_hash  (file_path_has);
     file_path_raw                   = system_hashed_ansi_string_get_buffer(file_path_has);

    /* Ensure we are not already subscribed to receive directory change notifications for
     * this specific folder. */
     if (!system_hash64map_get(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                               system_hashed_ansi_string_get_hash(file_path_has),
                              &directory_callback_ptr) )
    {
        /* We're not. Sign up now. */
        system_event new_wait_event  = NULL;
        HANDLE       new_wait_handle = ::FindFirstChangeNotification(file_path_raw,
                                                                     false, /* bWatchSubtree */
                                                                     FILE_NOTIFY_CHANGE_LAST_WRITE);

        ASSERT_DEBUG_SYNC(new_wait_handle != INVALID_HANDLE_VALUE,
                          "Could not sign up for directory change notifications - INVALID_HANDLE_VALUE returned.");

        /* Create an event out of the handle */
        result_wait_event = system_event_create_from_change_notification_handle(new_wait_handle);

        ASSERT_DEBUG_SYNC(result_wait_event != NULL,
                          "Could not create an event out of directory change notification handle.");

        /* Instantiate a directory descriptor */
        directory_callback_ptr = new (std::nothrow) _system_file_monitor_directory_callback(file_path_has,
                                                                                            result_wait_event);

        ASSERT_DEBUG_SYNC(directory_callback_ptr != NULL,
                          "Out of memory");

        system_hash64map_insert(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                file_path_has_hash,
                                directory_callback_ptr,
                                NULL,  /* callback */
                                NULL); /* callback_argument */
    } /* if (directory is not already being monitored) */

    /* Ensure the requested file does not have a corresponding descriptor already. */
    if (system_hash64map_contains(directory_callback_ptr->filename_to_callback_map,
                                  file_name_without_path_has_hash) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "File [%s] is already monitored for changes.",
                          file_name_with_path_raw);

        goto end;
    }

    /* Sign up for change notifications of the specified file */
    file_callback_ptr = new (std::nothrow) _system_file_monitor_file_callback(pfn_file_changed_proc,
                                                                              file_changed_proc_user_arg,
                                                                              file_path_has,
                                                                              file_name_without_path_has);

    ASSERT_ALWAYS_SYNC(file_callback_ptr != NULL,
                       "Out of memory");

    file_callback_ptr->last_write_time = last_write_time;

    system_hash64map_insert(directory_callback_ptr->filename_to_callback_map,
                            file_name_without_path_has_hash,
                            file_callback_ptr,
                            NULL,  /* callback */
                            NULL); /* callback_user_argument */

    /* All done */
    result = true;

end:
    if (!result)
    {
        if (directory_callback_ptr != NULL)
        {
            delete directory_callback_ptr;

            directory_callback_ptr = NULL;
        } /* if (directory_callback_ptr != NULL) */
    } /* if (!result) */

    return result;
}

/** TODO */
PRIVATE void _system_file_monitor_unregister_file_callback(_system_file_monitor_directory_callback* directory_callback_ptr,
                                                           _system_file_monitor_file_callback*      file_callback_ptr)
{
    const system_hash64 file_hash = system_hashed_ansi_string_get_hash(file_callback_ptr->filename);
    uint32_t            n_files   = 0;

    ASSERT_DEBUG_SYNC(directory_callback_ptr != NULL &&
                      file_callback_ptr      != NULL,
                      "Directory and/or file callback descriptors are NULL");

    ASSERT_DEBUG_SYNC(directory_callback_ptr->wait_event != NULL,
                      "Directory call-back's wait event is NULL");

    /* Find the file call-back descriptor in the directory call-back descriptor */
    ASSERT_DEBUG_SYNC(system_hash64map_contains(directory_callback_ptr->filename_to_callback_map,
                                                file_hash),
                      "The provided directory callback descriptor does not own the specified file callback descriptor");

    system_hash64map_remove(directory_callback_ptr->filename_to_callback_map,
                            file_hash);

    /* Release the file call-back descriptor */
    delete file_callback_ptr;
    file_callback_ptr = NULL;

    /* If there are no more file callback descriptors embedded in the directory descriptor, release it */
    system_hash64map_get_property(directory_callback_ptr->filename_to_callback_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_files);

    if (n_files == 0)
    {
        const system_hash64 directory_name_hash = system_hashed_ansi_string_get_hash(directory_callback_ptr->directory);

        ASSERT_DEBUG_SYNC(system_hash64map_contains(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                                    directory_name_hash),
                          "Could not find the specified directory call-back descriptor");

        system_hash64map_remove(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                directory_name_hash);

        delete directory_callback_ptr;
        directory_callback_ptr = NULL;
    }
}


/** Please see header for spec */
PUBLIC void system_file_monitor_deinit()
{
    if (file_monitor_ptr != NULL)
    {
        delete file_monitor_ptr;

        file_monitor_ptr = NULL;
    } /* if (file_monitor_ptr != NULL) */
}

/** Please see header for spec */
PUBLIC void system_file_monitor_init()
{
    /* Initialize the global file monitor instance */
    file_monitor_ptr = new (std::nothrow) _system_file_monitor;

    ASSERT_DEBUG_SYNC(file_monitor_ptr != NULL,
                      "Out of memory");
}

/** Please see header for spec */
PUBLIC EMERALD_API void system_file_monitor_monitor_file_changes(system_hashed_ansi_string file_name,
                                                                 bool                      should_enable,
                                                                 PFNFILECHANGEDETECTEDPROC pfn_file_changed_proc,
                                                                 void*                     user_arg)
{
    system_hashed_ansi_string file_name_without_path_has      = NULL;
    system_hash64             file_name_without_path_has_hash;
    const system_hash64       file_name_hash                  = system_hashed_ansi_string_get_hash(file_name);
    system_hashed_ansi_string file_path_has                   = NULL;
    system_hash64             file_path_has_hash;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(file_monitor_ptr != NULL,
                      "File monitor has not been initialized.");

    /* Chances are this is the first file we are asked to look after. If this is the case,
     * spawn the monitoring thread */
    _system_file_monitor_get_directory_and_file_for_file_name_with_path(file_name,
                                                                       &file_path_has,
                                                                       &file_name_without_path_has);

    ASSERT_DEBUG_SYNC(file_path_has              != NULL &&
                      file_name_without_path_has != NULL,
                      "Could not extract file path & name from the input file name argument");

    file_path_has_hash              = system_hashed_ansi_string_get_hash(file_path_has);
    file_name_without_path_has_hash = system_hashed_ansi_string_get_hash(file_name_without_path_has);

    system_critical_section_enter(file_monitor_ptr->cs);
    {
        bool should_continue = true;

        if (file_monitor_ptr->monitor_thread == NULL)
        {
            system_threads_spawn(_system_file_monitor_monitor_thread_entrypoint,
                                 NULL, /* callback_func_argument */
                                &file_monitor_ptr->monitor_thread_wait_event,
                                 system_hashed_ansi_string_create("File monitor thread"),
                                &file_monitor_ptr->monitor_thread);

            ASSERT_DEBUG_SYNC(file_monitor_ptr->monitor_thread != NULL,
                              "Could not spawn the file monitor thread");
        } /* if (file_monitor_ptr->monitor_thread == NULL) */

        /* Lock the monitor thread */
        system_event_set        (file_monitor_ptr->wait_table_needs_an_update_event);
        system_event_wait_single(file_monitor_ptr->wait_table_needs_an_update_ack_event,
                                 SYSTEM_TIME_INFINITE);

        /* If the caller requested a certain callback to be removed, remove & release it now */
        if (!should_enable)
        {
            _system_file_monitor_directory_callback* directory_callback_ptr = NULL;
            _system_file_monitor_file_callback*      file_callback_ptr      = NULL;

            if (!system_hash64map_get(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                      file_path_has_hash,
                                     &directory_callback_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve directory call-back's descriptor for file [%s]",
                                  system_hashed_ansi_string_get_buffer(file_name) );

                should_continue = false;
            } /* if (directory call-back descriptor was found) */
            else
            {
                if (!system_hash64map_get(directory_callback_ptr->filename_to_callback_map,
                                          file_name_without_path_has_hash,
                                         &file_callback_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve file call-back's descriptor for file [%s]",
                                      system_hashed_ansi_string_get_buffer(file_name) );

                    should_continue = false;
                } /* if (file call-back descriptor was found) */
                else
                {
                    _system_file_monitor_unregister_file_callback(directory_callback_ptr,
                                                                  file_callback_ptr);
                }
            }
        } /* if (!should_enable) */
        else
        {
            /* Register a new call-back */
            if (!_system_file_monitor_register_file_callback(file_name,
                                                             pfn_file_changed_proc,
                                                             user_arg) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not register a system call-back for file [%s]",
                                  system_hashed_ansi_string_get_buffer(file_name) );

                should_continue = false;
            }
        } /* if (should_enable) */

        /* Re-build the wait table */
        if (should_continue)
        {
            uint32_t n_monitored_directories = 0;

            /* Run in two iterations:
             *
             * 1. During the first iteration, count all the monitored files.
             * 2. During the second iteration, store them in the wait array.
             */
            system_hash64map_get_property(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_monitored_directories);

            if (file_monitor_ptr->wait_table_size <  (n_monitored_directories + 2) )
            {
                /* Need to resize the array */
                delete [] file_monitor_ptr->wait_table;

                file_monitor_ptr->wait_table_size = (n_monitored_directories + 2) * 2;
                file_monitor_ptr->wait_table      = new (std::nothrow) system_event[file_monitor_ptr->wait_table_size];

                ASSERT_ALWAYS_SYNC(file_monitor_ptr->wait_table != NULL,
                                   "Out of memory");

                /* Update the array entries */
                file_monitor_ptr->wait_table[0] = file_monitor_ptr->monitor_thread_please_die_event;
                file_monitor_ptr->wait_table[1] = file_monitor_ptr->wait_table_needs_an_update_event;
            } /* if (file_monitor_ptr->wait_table_size < (n_files_monitored + 1) ) */

            /* Traverse all directories */
            file_monitor_ptr->wait_table_n_entries = 2 + n_monitored_directories;

            for (unsigned int n_directory = 0;
                              n_directory < n_monitored_directories;
                            ++n_directory)
            {
                _system_file_monitor_directory_callback* current_directory_callback_ptr = NULL;
                unsigned int                             n_current_directory_files      = 0;

                if (!system_hash64map_get_element_at(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                                     n_directory,
                                                    &current_directory_callback_ptr,
                                                     NULL) ) /* result_hash */
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve [%d]-th directory from the directory call-back hash-map.",
                                      n_directory);

                    continue;
                }

                file_monitor_ptr->wait_table[2 + n_directory] = current_directory_callback_ptr->wait_event;
            } /* for (all directories) */
        } /* if (should_continue) */

        /* Wake up the monitor thread */
        system_event_set(file_monitor_ptr->wait_table_updated_event);
    }
    system_critical_section_leave(file_monitor_ptr->cs);
}
