/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWFILTER.H -- LightWave Image and Pixel Filters
 *
 * This header contains the basic declarations need to define the
 * simplest LightWave plug-in server.
 */
#ifndef LWSDK_FILTER_H
#define LWSDK_FILTER_H


#include <lwmonitor.h>
#include <lwrender.h>


#define LWIMAGEFILTER_HCLASS    "ImageFilterHandler"
#define LWIMAGEFILTER_ICLASS    "ImageFilterInterface"
#define LWIMAGEFILTER_GCLASS    "ImageFilterGizmo"
#define LWIMAGEFILTER_VERSION   5


#define LWPIXELFILTER_HCLASS    "PixelFilterHandler"
#define LWPIXELFILTER_ICLASS    "PixelFilterInterface"
#define LWPIXELFILTER_GCLASS    "PixelFilterGizmo"
#define LWPIXELFILTER_VERSION   8


//
// Buffer types, used with getLine(), getVal(), setVal()
//
enum
{
    LWBUF_SPECIAL,
    LWBUF_LUMINOUS,
    LWBUF_DIFFUSE,
    LWBUF_SPECULAR,
    LWBUF_MIRROR,
    LWBUF_TRANS,
    LWBUF_RAW_RED,
    LWBUF_RAW_GREEN,
    LWBUF_RAW_BLUE,
    LWBUF_SHADING,
    LWBUF_SHADOW,
    LWBUF_GEOMETRY,
    LWBUF_DEPTH,
    LWBUF_DIFFSHADE,
    LWBUF_SPECSHADE,
    LWBUF_MOTION_X,
    LWBUF_MOTION_Y,
    LWBUF_REFL_RED,
    LWBUF_REFL_GREEN,
    LWBUF_REFL_BLUE,
    LWBUF_DIFF_RED,
    LWBUF_DIFF_GREEN,
    LWBUF_DIFF_BLUE,
    LWBUF_SPEC_RED,
    LWBUF_SPEC_GREEN,
    LWBUF_SPEC_BLUE,
    LWBUF_BACKDROP_RED,
    LWBUF_BACKDROP_GREEN,
    LWBUF_BACKDROP_BLUE,
    LWBUF_PREEFFECT_RED,
    LWBUF_PREEFFECT_GREEN,
    LWBUF_PREEFFECT_BLUE,
    LWBUF_RED,
    LWBUF_GREEN,
    LWBUF_BLUE,
    LWBUF_ALPHA,
    LWBUF_REFR_RED,
    LWBUF_REFR_GREEN,
    LWBUF_REFR_BLUE,
    LWBUF_REFR_ALPHA,
    LWBUF_NORMAL_X,
    LWBUF_NORMAL_Y,
    LWBUF_NORMAL_Z,
    LWBUF_SURFACEID,
    LWBUF_OBJECTID,
    LWBUF_RADIOSITY_RED,
    LWBUF_RADIOSITY_GREEN,
    LWBUF_RADIOSITY_BLUE,
    LWBUF_AMBIENTOCCLUSION_RED,
    LWBUF_AMBIENTOCCLUSION_GREEN,
    LWBUF_AMBIENTOCCLUSION_BLUE,
    LWBUF_UV_TANGENTSPACE_T_X,
    LWBUF_UV_TANGENTSPACE_T_Y,
    LWBUF_UV_TANGENTSPACE_T_Z,
    LWBUF_UV_TANGENTSPACE_B_X,
    LWBUF_UV_TANGENTSPACE_B_Y,
    LWBUF_UV_TANGENTSPACE_B_Z,
    LWBUF_UV_TANGENTSPACE_N_X,
    LWBUF_UV_TANGENTSPACE_N_Y,
    LWBUF_UV_TANGENTSPACE_N_Z,
    LWBUF_CAMERA_TANGENTSPACE_X,
    LWBUF_CAMERA_TANGENTSPACE_Y,
    LWBUF_CAMERA_TANGENTSPACE_Z,
    // for LightWave internal use only
    LWBUF_INTERNAL_1,
    LWBUF_MAX_CHANNELS
};

/* Render flags. */

enum {
    LWPFF_MULTITHREADED = 1<<0,
    LWPFF_EVERYPIXEL    = 1<<1,
    LWPFF_BEFOREVOLUME  = 1<<2,
    LWPFF_RAYTRACE      = 1<<3
};

typedef struct st_LWFilterAccess {
        int               width, height;
        LWFrame           frame;
        LWTime            start, end;
        float *         (*getLine)  (int type, int y);
        void            (*setRGB)   (int x, int y, const LWFVector rgb);
        void            (*setAlpha) (int x, int y, float alpha);
        LWMonitor        *monitor;
} LWFilterAccess;

/* Note: Flags now returns an arry of index entries.  The first elements is the number of flags */
/* in the array ( count of entries ).  Followed by array of ( count ) of LWBUF_xxx entries.     */
/* static int flags[] = { 2, LWBUF_SPECIAL, LWBUF_LUMINOUS };                                   */

typedef struct st_LWImageFilterHandler {
        LWInstanceFuncs  *inst;
        LWItemFuncs      *item;
        void            (*process) (LWInstance, const LWFilterAccess *);
        int *           (*flags) (LWInstance);
} LWImageFilterHandler;


typedef struct st_LWPixelAccess {
        double                        sx, sy;
        void                        (*getVal) (int type, int num, float *);
        void                        (*setRGBA)(const float[4]);
        void                        (*setVal) (int type, int num, float *);
        LWIlluminateFunc             *illuminate;
        LWRayTraceFunc               *rayTrace;
        LWRayCastFunc                *rayCast;
        LWRayShadeFunc               *rayShade;
        LWRayTraceModeFunc           *rayTraceMode;
        LWIlluminateSampleFunc       *illuminateSample;
        RandomFloatData               randomData;
        LWRandomFloatFunc            *randomFloat;
        LWIlluminateNormalFunc       *illuminateNormal;
        LWIlluminateSampleNormalFunc *illuminateSampleNormal;
        LWRayTraceDataFunc           *rayTraceData;
        LWRayTraceShadeFunc          *rayTraceShade;
        LWRenderData                  render;
        LWSamplerState                samplerState;
        LWGetSamplerRangeFunc        *getSamplerRange;
        LWGetSample2DFunc            *getSample2D;
        LWGetSample1DFunc            *getSample1D;
} LWPixelAccess;

typedef struct st_LWPixelFilterHandler {
        LWInstanceFuncs  *inst;
        LWItemFuncs      *item;
        LWRenderFuncs    *rend;
        void            (*evaluate)    (LWInstance, const LWPixelAccess *);
        int *           (*flags)       (LWInstance);
        unsigned int    (*renderFlags) (LWInstance);
} LWPixelFilterHandler;


typedef unsigned int LWFilterContext;


#define LWFCF_PREPROCESS  (1<<0) /* Filter applied in image editor or as pre process */


#endif
