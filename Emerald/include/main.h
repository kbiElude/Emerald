/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef APPMAIN_H
#define APPMAIN_H

#include "system/system_types.h"

/** Entry point
 
 @param instance
 @param reason
 @param reserved

 @return

 **/ 
EMERALD_API BOOL WINAPI DllMain(__in HINSTANCE instance,
                                __in DWORD     reason,
                                __in LPVOID    reserved);

/** TODO */
EMERALD_API void main_force_deinit();

/** Initialized in DLL_PROCESS_ATTACH */
extern HINSTANCE _global_instance;

#endif /* APPMAIN_H */
