/*
 * LWSDK Startup File
 * Copyright 1995,1997  NewTek, Inc.
 */
#define _MSWIN

#include <lwmodule.h>

extern void *        Startup (void);
extern void        Shutdown (void *serverData);
extern ServerRecord     ServerDesc[];

#if defined(__BORLANDC__)
ModuleDescriptor mod_descrip =
#elif defined __GNUC__
__attribute__ ((visibility("default"))) ModuleDescriptor _mod_descrip =
#else
ModuleDescriptor _mod_descrip =
#endif
{
    MOD_SYSSYNC,
    MOD_SYSVER,
    MOD_MACHINE,
    Startup,
    Shutdown,
    ServerDesc
};
