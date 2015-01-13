/*
 * LWSDK Header File
 * Copyright 2005, NewTek, Inc.
 *
 * LWCAMERA.H -- LightWave Camera
 */
#ifndef LWSDK_CAMERA_H
#define LWSDK_CAMERA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lwhandler.h>
#include <lwrender.h>


#define LWCAMERA_HCLASS "CameraHandler"
#define LWCAMERA_ICLASS "CameraInterface"
#define LWCAMERA_GCLASS "CameraGizmo"
#define LWCAMERA_VERSION 2

typedef struct st_LWCameraAccess
{
	LWItemID cameraID;
	LWDVector worldPos;
	LWDVector toWorld[3];
	LWDVector toCamera[3];
	double filmSize[2];
} LWCameraAccess;

typedef struct st_LWCameraHandler
{
	LWInstanceFuncs* inst;
	LWItemFuncs* item;
	LWRenderFuncs* rend;
	int     (*preview)  (LWInstance, double lpx, double lpy, LWDMatrix4 projection, const LWCameraAccess* camaccess);
	LWError (*newFrame) (LWInstance, const LWFrameInfo* frameinfo, const LWCameraAccess* camaccess);
	int     (*evaluate) (LWInstance, double fpx, double fpy, double lpx, double lpy, double fractime, LWCameraRay* ray, const LWCameraAccess* camaccess);
	unsigned int (*flags)   (LWInstance);
} LWCameraHandler;

// Camera handler evaluation return values
#define LWCAMTYPEEVAL_NO_RAY        0
#define LWCAMTYPEEVAL_RAY           1
#define LWCAMTYPEEVAL_DO_DEF_INTERP 3

// Camera handler flags
#define LWCAMTYPEF_SUPPORTSDOF (1 << 0)
#define LWCAMTYPEF_SUPPORTSSTEREO (1 << 1)


#ifdef __cplusplus
}
#endif

#endif
