/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"
#include "system/system_threads.h"

system_critical_section log_file_handle_cs = NULL;
FILE*                   log_file_handle    = NULL;


/** Please see header for specification */
PUBLIC void _system_log_init()
{
    log_file_handle    = fopen                         ("log.txt",
                                                        "w");
    log_file_handle_cs = system_critical_section_create();
}

/** Please see header for spefification */
PUBLIC void _system_log_deinit()
{
    if (log_file_handle != NULL)
    {
        fclose(log_file_handle);

        log_file_handle = NULL;
    }

    if (log_file_handle_cs != NULL)
    {
        system_critical_section cs_cached = log_file_handle_cs;
        log_file_handle_cs = NULL;

        system_critical_section_release(cs_cached);
    }
}

/** Please see header for specification */
EMERALD_API void system_log_write(system_log_priority,
                                  const char*         text,
                                  bool                include_tid_info)
{
    {
        static char temp[16];

        if (include_tid_info)
        {
            snprintf(temp,
                     sizeof(temp),
                     "[tid:%08x] ",
                     system_threads_get_thread_id() );
        }
        else
        {
            memset(temp,
                   0,
                   sizeof(temp) );
        }
#ifdef _WIN32
        ::OutputDebugStringA(temp);
        ::OutputDebugStringA(text);
        ::OutputDebugStringA("\n");
#else
        printf("%s%s\n",
               temp,
               text);
#endif

        if (log_file_handle != NULL)
        {
            fwrite(text,
                   strlen(text), 
                   1, /* _Count */
                   log_file_handle);
            fwrite("\n",
                   1, /* _Size  */
                   1, /* _Count */
                   log_file_handle);

            fflush(log_file_handle);
        }
    }
}
