/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef APPMAIN_H
#define APPMAIN_H

#include "system/system_types.h"

/* Windows-specific bits */
#ifdef _WIN32
    /** Entry point

    @param instance
    @param reason
    @param reserved

    @return

    **/ 
    EMERALD_API BOOL WINAPI DllMain(HINSTANCE instance,
                                    DWORD     reason,
                                    LPVOID    reserved);

    /** Initialized in DLL_PROCESS_ATTACH */
    extern HINSTANCE _global_instance;
#endif /* _WIN32 */

/** TODO */
EMERALD_API void main_force_deinit();

#endif /* APPMAIN_H */
