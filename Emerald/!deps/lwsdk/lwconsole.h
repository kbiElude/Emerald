/*
 * LWSDK Header File
 * Copyright 2012, NewTek, Inc.
 *
 * LWCONSOLE.H -- LightWave Console Access
 *
 */
#ifndef LWSDK_PCORECONSOLE_H
#define LWSDK_PCORECONSOLE_H

#define LWPCORECONSOLE_GLOBAL    "LW PCore Console"

typedef struct st_LWPCoreConsole
{
    void        (*info)(const char *);
    void        (*error)(const char *);
    void        (*clear)();
    void        (*show)();
    void        (*hide)();
    int         (*visible)();
} LWPCoreConsole;

#endif
