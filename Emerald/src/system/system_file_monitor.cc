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

    explicit _system_file_monitor_directory_callback(system_hashed_ansi_string in_directory);
            ~_system_file_monitor_directory_callback();
};

typedef struct _system_file_monitor_file_callback
{
    PFNFILECHANGEDETECTEDPROC callback;
    void*                     callback_user_arg;
    system_hashed_ansi_string directory;
    system_hashed_ansi_string filename;

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
    system_hash64map        monitored_filename_hash_to_callback_map;       /* maps file      name hashes to _system_file_monitor_file_callback      instances */

    system_event* wait_table;
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
PRIVATE void _system_file_monitor_monitor_thread_entrypoint (void*                               unused);
PRIVATE bool _system_file_monitor_register_system_callback  (system_hashed_ansi_string           file_name,
                                                             system_event*                       out_wait_event_ptr);
PRIVATE void _system_file_monitor_unregister_system_callback(_system_file_monitor_file_callback* callback_ptr);


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
    monitored_filename_hash_to_callback_map       = system_hash64map_create(sizeof(_system_file_monitor_file_callback*) );

    wait_table_needs_an_update_ack_event     = system_event_create(true); /* manual_reset */
    wait_table_needs_an_update_event         = system_event_create(true); /* manual_reset */
    wait_table_updated_event                 = system_event_create(true); /* manual_reset */

    /* By default, the wait table should only consist of the "please die" and "wait table needs an update" event. */
    wait_table = new (std::nothrow) system_event[2];

    ASSERT_ALWAYS_SYNC(wait_table != NULL,
                       "Out of memory");

    wait_table[0]   = monitor_thread_please_die_event;
    wait_table[1]   = wait_table_needs_an_update_event;
    wait_table_size = 2;
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

    if (monitored_filename_hash_to_callback_map != NULL)
    {
        unsigned int n_monitored_filenames = 0;

        system_hash64map_get_property(monitored_filename_hash_to_callback_map,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_monitored_filenames);

        for (unsigned int n_monitored_filename = 0;
                          n_monitored_filename < n_monitored_filenames;
                        ++n_monitored_filename)
        {
            _system_file_monitor_file_callback* callback_ptr = NULL;

            if (!system_hash64map_get_element_at(monitored_filename_hash_to_callback_map,
                                                 n_monitored_filename,
                                                &callback_ptr,
                                                 NULL) ) /* result_hash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve hash-map item at index [%d]",
                                  n_monitored_filename);

                continue;
            }

            delete callback_ptr;
            callback_ptr = NULL;
        } /* for (all monitored filenames) */

        system_hash64map_release(monitored_filename_hash_to_callback_map);
        monitored_filename_hash_to_callback_map = NULL;
    } /* if (monitored_filename_hash_to_callback_map != NULL) */

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
PRIVATE void _system_file_monitor_monitor_thread_entrypoint(void* unused)
{
    LOG_INFO("File monitor thread started.");

    ASSERT_DEBUG_SYNC(file_monitor_ptr != NULL,
                      "File monitor instance is NULL");

    while (true)
    {
        size_t n_event = system_event_wait_multiple(file_monitor_ptr->wait_table,
                                                    file_monitor_ptr->wait_table_size,
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
            /* This must be a system call-back */
            system_critical_section_enter(file_monitor_ptr->cs);
            {
                _system_file_monitor_file_callback* callback_ptr = NULL;

                if (!system_hash64map_get_element_at(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                                     (n_event - 2),
                                                    &callback_ptr, /* result_element */
                                                     NULL) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve call-back descriptor");
                }
                else
                {
                    callback_ptr->callback(callback_ptr->filename,
                                           callback_ptr->callback_user_arg);
                }
            }
            system_critical_section_leave(file_monitor_ptr->cs);
        }
    } /* while (true) */

    LOG_INFO("File monitor thread exiting.");
}

/** TODO */
PRIVATE bool _system_file_monitor_register_system_callback(system_hashed_ansi_string file_name,
                                                           system_event*             out_wait_event_ptr)
{
    const char*               file_name_raw            = system_hashed_ansi_string_get_buffer(file_name);
    char*                     file_path                = NULL;
    system_hashed_ansi_string file_path_has            = NULL;
    DWORD                     n_temp_buffer_bytes_used = 0;
    bool                      result                   = false;
    system_event              result_wait_event        = NULL;
    char                      temp_buffer[MAX_PATH];
    char*                     temp_buffer_file_name    = NULL;

    /* Extract path name from the file name */
    n_temp_buffer_bytes_used = ::GetFullPathName(file_name_raw,
                                                 sizeof(temp_buffer),
                                                 temp_buffer,
                                                &temp_buffer_file_name);

    ASSERT_DEBUG_SYNC(n_temp_buffer_bytes_used > 0 &&
                      n_temp_buffer_bytes_used < sizeof(temp_buffer),
                      "GetFullPathName() failed.");

    *temp_buffer_file_name = 0;
     file_path             = temp_buffer;
     file_path_has         = system_hashed_ansi_string_create(file_path);

    /* Ensure we are not already subscribed to receive directory change notifications for
     * this specific folder. */
     if (!system_hash64map_contains(file_monitor_ptr->monitored_directory_name_hash_to_callback_map,
                                    system_hashed_ansi_string_get_hash(file_path_has)) == ITEM_NOT_FOUND)
    {
        /* We're not. Sign up now. */
        system_event new_wait_event  = NULL;
        HANDLE       new_wait_handle = ::FindFirstChangeNotification(file_path,
                                                                     false, /* bWatchSubtree */
                                                                     FILE_NOTIFY_CHANGE_LAST_WRITE);

        ASSERT_DEBUG_SYNC(new_wait_handle != NULL,
                          "Could not sign up for directory change notifications - NULL event returned.");

        /* Create an event out of the handle */
        result_wait_event = system_event_create_from_handle(new_wait_handle);

        ASSERT_DEBUG_SYNC(result_wait_event != NULL,
                          "Could not create an event out of directory change notification handle.");

        /* Instantiate a directory descriptor */
       todo;
    } /* if (directory is not already being monitored) */

    /* Sign up for change notifications of the specified file */
    return true;
}

/** TODO */
PRIVATE void _system_file_monitor_unregister_system_callback(_system_file_monitor_file_callback* callback_ptr)
{
    ASSERT_DEBUG_SYNC(callback_ptr != NULL,
                      "Input argument is NULL");

    // todo
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
    const system_hash64 file_name_hash = system_hashed_ansi_string_get_hash(file_name);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(file_monitor_ptr != NULL,
                      "File monitor has not been initialized.");

    /* Chances are this is the first file we are asked to look after. If this is the case,
     * spawn the monitoring thread */
    system_critical_section_enter(file_monitor_ptr->cs);
    {
        uint32_t n_files_monitored = 0;
        bool     should_continue   = true;

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
            _system_file_monitor_file_callback* callback_ptr = NULL;

            if (!system_hash64map_get(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                      file_name_hash,
                                     &callback_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve call-back descriptor for file [%s]",
                                  system_hashed_ansi_string_get_buffer(file_name) );
            } /* if (call-back descriptor was found) */
            else
            {
                _system_file_monitor_unregister_system_callback(callback_ptr);

                system_hash64map_remove(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                        file_name_hash);

                delete callback_ptr;
                callback_ptr = NULL;
            }
        } /* if (!should_enable) */
        else
        {
            /* Register a new call-back */
            _system_file_monitor_file_callback* new_callback_ptr = NULL;

            new_callback_ptr = new (std::nothrow) _system_file_monitor_file_callback(pfn_file_changed_proc,
                                                                                     user_arg,
                                                                                     file_name);

            ASSERT_DEBUG_SYNC(new_callback_ptr != NULL,
                              "Out of memory");

            if (!_system_file_monitor_register_system_callback(file_name,
                                                              &new_callback_ptr->wait_event) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not register a system call-back for file [%s]",
                                  system_hashed_ansi_string_get_buffer(file_name) );

                should_continue = false;
            }

            ASSERT_DEBUG_SYNC(!system_hash64map_contains(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                                         file_name_hash),
                              "Re-registering a call-back for the same file name [%s] !",
                              system_hashed_ansi_string_get_buffer(file_name) );

            system_hash64map_insert(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                    file_name_hash,
                                    new_callback_ptr,
                                    NULL,  /* callback */
                                    NULL); /* callback_argument */
        } /* if (should_enable) */

        /* Re-build the wait table */
        if (should_continue)
        {
            system_hash64map_get_property(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_files_monitored);

            if (file_monitor_ptr->wait_table_size < (n_files_monitored + 1) )
            {
                /* Need to resize the array */
                delete [] file_monitor_ptr->wait_table;

                file_monitor_ptr->wait_table_size = (n_files_monitored + 1) * 2;
                file_monitor_ptr->wait_table      = new (std::nothrow) system_event[file_monitor_ptr->wait_table_size];

                ASSERT_ALWAYS_SYNC(file_monitor_ptr->wait_table != NULL,
                                   "Out of memory");
            } /* if (file_monitor_ptr->wait_table_size < (n_files_monitored + 1) ) */

            /* Update the array entries */
            file_monitor_ptr->wait_table[0] = file_monitor_ptr->monitor_thread_please_die_event;

            for (unsigned int n_filename = 0;
                              n_filename < n_files_monitored;
                            ++n_filename)
            {
                _system_file_monitor_file_callback* current_callback_ptr = NULL;

                if (!system_hash64map_get_element_at(file_monitor_ptr->monitored_filename_hash_to_callback_map,
                                                     n_filename,
                                                    &current_callback_ptr,
                                                     NULL /* result_hash */) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve call-back descriptor at index [%d]",
                                      n_filename);
                }
                else
                {
                    file_monitor_ptr->wait_table[1 + n_filename] = current_callback_ptr->wait_event;
                }
            } /* for (all filenames) */
        } /* if (should_continue) */

        /* Wake up the monitor thread */
        system_event_set(file_monitor_ptr->wait_table_updated_event);
    }
    system_critical_section_leave(file_monitor_ptr->cs);
}
