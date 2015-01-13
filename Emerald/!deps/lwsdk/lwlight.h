/*
 * LWSDK Header File
 * Copyright 2007, NewTek, Inc.
 *
 * LWLIGHT.H -- LightWave Lighting
 */
#ifndef LWSDK_LIGHT_H
#define LWSDK_LIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lwhandler.h>
#include <lwrender.h>


#define LWLIGHT_HCLASS "LightHandler"
#define LWLIGHT_ICLASS "LightInterface"
#define LWLIGHT_GCLASS "LightGizmo"
#define LWLIGHT_VERSION 2


typedef struct st_LWLightAccess
{
    LWItemID lightID;

    LWDVector worldPos;

    LWDVector toWorld[3];
    LWDVector toLight[3];

    unsigned int maxIlluminations;

    LWRayCastFunc*      rayCast;
    RandomFloatData     randomData;
    LWRandomFloatFunc*  randomFloat;

    unsigned int bounces;
    unsigned int flags;

    LWDVector surfaceNormal;

    LWSamplerState          samplerState;
    LWGetSamplerRangeFunc   *getSamplerRange;
    LWGetSample2DFunc       *getSample2D;
    LWGetSample1DFunc       *getSample1D;

} LWLightAccess;

// Light access flags
#define LWLF_SAMPLEDRAY  (1<<0)
#define LWLF_HAVESURFACE (1<<1)
#define LWLF_PREPROCESS  (1<<2)


// Light plugins can return an array of standard lights
// to create a preview. The array is terminated with a
// preview light type of LWPREVIEWLIGHT_END.
typedef struct st_LWPreviewLight
{
    int previewtype;        // One of the LWPREVIEWLIGHT_ values below

    LWDVector worldPos;     // The light to world transformation matrix
    LWDVector toWorld[3];

    LWDVector color;        // Color and intensity of the light
    double intensity;

    double range;           // Falloff
    int falloff;

    double coneAngle;       // Cone for spot lights
    double coneEdge;
} LWPreviewLight;

// Preview light types
#define LWPREVIEWLIGHT_DISTANT 0
#define LWPREVIEWLIGHT_POINT   1
#define LWPREVIEWLIGHT_SPOT    2
#define LWPREVIEWLIGHT_AMBIENT 3

#define LWPREVIEWLIGHT_END     -1


// Light evaluation fills in an array of illuminations
typedef struct st_LWIllumination
{
    LWDVector color;                // Color, not including intensity or shadow effects
    double intensity;               // Intensity, not including shadow effects
    LWDVector direction;            // Direction in which the illumination is going, normalized
    double distance;                // Distance to origin of illumination along ray; DBL_MAX if infinite
    LWDVector shadowAttenuation;    // Degree of shadowing to apply
} LWIllumination;


// Description of a photon shot from a light
typedef struct st_LWPhoton
{
    LWDVector color;
    double intensity;
    LWDVector direction;
    LWDVector worldPos;
} LWPhoton;


// A sample of the illumination along a ray
typedef struct st_LWRayIllumination
{
    LWDVector color;
    double intensity;
    LWDVector distance;
} LWRayIllumination;


typedef struct st_LWLightHandler
{
    LWInstanceFuncs* inst;
    LWItemFuncs* item;
    LWRenderFuncs* rend;
    unsigned int (*flags)      (LWInstance);
    LWError      (*newFrame)   (LWInstance, const LWFrameInfo* frameinfo, unsigned int* maxilluminations, const LWLightAccess* lightaccess);
    int          (*evaluate)   (LWInstance, const LWDVector spot, double fractime, LWIllumination illumination[], const LWLightAccess* lightaccess);
    unsigned int (*getPhotons) (LWInstance, unsigned int maxphotons, LWPhoton photons[], const LWLightAccess* lightaccess);
    unsigned int (*getRayIlluminations) (LWInstance, LWDVector raystart, LWDVector raydir, double farclip, unsigned int maxrayilluminations, LWRayIllumination rayilluminations[], const LWLightAccess* lightaccess);
    const LWPreviewLight* (*preview) (LWInstance);
} LWLightHandler;

// Light handler flags
#define LWLIGHTTYPEF_NO_PHOTONS           (1<<0)    // Photon generation not supported (e.g. caustics)
#define LWLIGHTTYPEF_NO_RAYILLUM          (1<<1)    // Ray illumination not supported (e.g. volumetric lights)
#define LWLIGHTTYPEF_OCCLUSION_SHADOWED   (1<<2)    // Returned illumination includes occlusion shadow effects
#define LWLIGHTTYPEF_VOLUMETRICS_SHADOWED (1<<3)    // Returned illumination includes volumetric shadow effects

#define LWLIGHTTYPEF_NO_FALLOFF           (1<<4)    // Do not show standard falloff gadgets, and do not apply
#define LWLIGHTTYPEF_SHOW_FALLOFF         (1<<5)    // Show the standard falloff gadgets, but do not apply


#define LWLIGHTEVALUATIONFUNCS_GLOBAL "LW Light Evaluation Funcs"

typedef void* LWLightEvaluatorID;

typedef struct st_LWLightEvaluationFuncs
{
    LWLightEvaluatorID (*create) (LWItemID light);
    void    (*destroy)    (LWLightEvaluatorID);
    unsigned int (*flags) (LWLightEvaluatorID);
    LWError (*init)       (LWLightEvaluatorID, int mode);
    void    (*cleanUp)    (LWLightEvaluatorID);
    LWError (*newTime)    (LWLightEvaluatorID, LWFrame frame, LWTime in_time);
    LWError (*newFrame)   (LWLightEvaluatorID, const LWFrameInfo* frameinfo, unsigned int* maxilluminations, const LWLightAccess* lightaccess);
    int     (*evaluate)   (LWLightEvaluatorID, const LWDVector spot, double fractime, LWIllumination illumination[], const LWLightAccess* lightaccess);
    unsigned int (*getPhotons) (LWLightEvaluatorID, unsigned int maxphotons, LWPhoton photons[], const LWLightAccess* lightaccess);
    unsigned int (*getRayIlluminations) (LWLightEvaluatorID, LWDVector raystart, LWDVector raydir, double farclip, unsigned int maxrayilluminations, LWRayIllumination rayilluminations[], const LWLightAccess* lightaccess);
} LWLightEvaluationFuncs;


#ifdef __cplusplus
}
#endif

#endif
