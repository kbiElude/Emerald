/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "system/system_critical_section.h"
#include "system/system_log.h"

system_critical_section log_file_handle_cs = NULL;
FILE*                   log_file_handle    = NULL;


/** Please see header for specification */
PUBLIC void _system_log_init()
{
    log_file_handle    = fopen                         ("log.txt", "w");    
    log_file_handle_cs = system_critical_section_create();
}

/** Please see header for spefification */
PUBLIC void _system_log_deinit()
{
    fclose(log_file_handle);

    system_critical_section_release(log_file_handle_cs);
}

/** Please see header for specification */
EMERALD_API void system_log_write(system_log_priority,
                                  const char* text)
{
    {
        static char temp[16];

        sprintf_s(temp,
                  sizeof(temp),
                  "[tid:%08x] ",
                ::GetCurrentThreadId() );

        ::OutputDebugStringA(temp);
        ::OutputDebugStringA(text);
        ::OutputDebugStringA("\n");

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
