/*
 * LWSDK Header File
 * Copyright 2007, NewTek, Inc.
 *
 * LWGIZMO.H -- Viewport widgets
 */
#ifndef LWSDK_GIZMO_H
#define LWSDK_GIZMO_H

#include <lwtool.h>
#include <lwcustobj.h>


#define LWGIZMO_VERSION 3

typedef struct st_LWGizmoFuncs {
    void            (*done)     (LWInstance);
    void            (*draw)     (LWInstance, LWCustomObjAccess *);
    const char *    (*help)     (LWInstance, LWToolEvent *);
    int             (*dirty)    (LWInstance);
    int             (*count)    (LWInstance, LWToolEvent *);
    int             (*handle)   (LWInstance, LWToolEvent *, int i, LWDVector pos);
    int             (*start)    (LWInstance, LWToolEvent *);
    int             (*adjust)   (LWInstance, LWToolEvent *, int i);
    int             (*down)     (LWInstance, LWToolEvent *);
    void            (*move)     (LWInstance, LWToolEvent *);
    void            (*up)       (LWInstance, LWToolEvent *);
    void            (*event)    (LWInstance, int code);
    LWXPanelID      (*panel)    (LWInstance);
} LWGizmoFuncs;

typedef struct st_LWGizmo {
    LWInstance       instance;
    LWGizmoFuncs     *gizmo;
    const LWItemID* (*pickItems) (LWInstance, const LWItemID* drawitems, const unsigned int* drawparts);
} LWGizmo;


#endif
