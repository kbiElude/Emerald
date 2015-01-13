/*
 * LWSDK Header File
 * Copyright 2008,  NewTek, Inc.
 *
 * LWEXTRENDERER.H -- LightWave Renderer
 *
 * Jamie L. Finch
 * Senile Programmer
 */
#ifndef LWSDK_EXTRENDERER_H
#define LWSDK_EXTRENDERER_H

#include <lwserver.h>
#include <lwgeneric.h>
#include <lwhandler.h>
#include <lwrender.h>

#define LWEXTRENDERER_HCLASS  "ExtRendererHandler"
#define LWEXTRENDERER_VERSION 1

typedef int EXTRENDERERIMAGE( int frame, int eye, LWPixmapID displayimage );

typedef struct st_LWExtRendererHandler {
    LWInstanceFuncs *inst;
    LWItemFuncs     *item;
    int            (*options)( LWInstance );
    int            (*render )( LWInstance, int first_frame, int last_frame, int frame_step, EXTRENDERERIMAGE *render_image, int render_mode );
} LWExtRendererHandler;

#endif
