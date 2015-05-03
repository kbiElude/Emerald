/*
 * LWSDK Header File
 * Copyright 2007, NewTek, Inc.
 *
 * LWCHANGE.H -- LightWave change.
 */

#ifndef LWSDK_CHANGE_H
#define LWSDK_CHANGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lwrender.h>

#define LWCHANGEFUNCS_GLOBAL    "Change Functions"

typedef struct st_lwchangefuncs {
    int (*getObjectChangedState)( LWItemID );   /* Gets object save state. */
    int (*getSceneChangedState )( void );       /* Gets scene  save state. */
    int (*anyChangesAtAll      )( void );       /* Checks for any changes at all in scene. */
} LWCHANGEFUNCS;

#ifdef __cplusplus
}
#endif

#endif
