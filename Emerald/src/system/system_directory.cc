/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "system/system_directory.h"

#ifdef __linux
    #include <sys/stat.h>
    #include <sys/types.h>
#endif

/** Please see header for specification */
PUBLIC EMERALD_API bool system_directory_create(system_hashed_ansi_string directory_name)
{
    bool result = false;

    #ifdef _WIN32
    {
        BOOL api_result = ::CreateDirectory(system_hashed_ansi_string_get_buffer(directory_name),
                                            NULL); /* lpSecurityAttributes */

        result = (api_result != 0);
    }
    #else
    {
        int api_result = mkdir(system_hashed_ansi_string_get_buffer(directory_name),
                         0777);

        result = (api_result == 0);
    }
    #endif

    return result;
}

