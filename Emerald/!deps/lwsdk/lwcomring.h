#ifndef LWSDK_COMRING_H
#define LWSDK_COMRING_H

/*
 * LWSDK Header File
 * Copyright 2003, NewTek, Inc.
 *
 * LWCOMRING.H -- LightWave Communications Ring
 *
 * This header contains declarations necessary to engage in a
 * communications ring among plug-ins
 */

#define LWCOMRING_GLOBAL "LW Communication Ring"

typedef void    (*RingEvent)    (void *clientData,void *portData,int eventCode,void *eventData);


typedef struct st_LWComRing
{
    int         (*ringAttach)   (char *topic,LWInstance pidata,RingEvent eventCallback);
    void        (*ringDetach)   (char *topic,LWInstance pidata);
    void        (*ringMessage)  (char *topic,int eventCode,void *eventData);
} LWComRing;

#define LW_PLUGIN_LIMBO_STATES "LW Plugin Limbo States"

#define LW_LIMBO_START 0x00000001
#define LW_LIMBO_END   0x00000002

/* Color space events. */

#define LW_PORT_COLORSPACE "color-space-change"

enum {
    LWCSEV_CHANGE = 0,
    LWCSEV_SIZEOF
};

/**
 *      For applications with multiple non-modal windows, it is often useful for
 *      the host and the client windows to be able to broadcast state information
 *      pertinent to those windows.
 *
 *      If the window data is NULL, then open or close your window.
 *      If the window data is not NULL, then the data is the name of the window that was opened or closed.
 */

#define LW_PORT_WINSTATE   "window-state"

enum {
    LWWSEV_CLOSE = 0,   /* Close window. */
    LWWSEV_OPEN,        /* Open  window. */
    LWWSEV_SIZEOF
};

#endif
