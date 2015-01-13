/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWRENDER.H -- LightWave Rendering State
 *
 * This header contains the basic declarations need to define the
 * simplest LightWave plug-in server.
 */
#ifndef LWSDK_RENDER_H
#define LWSDK_RENDER_H

#include <lwtypes.h>
#include <lwhandler.h>
#include <lwenvel.h>
#include <lwmeshes.h>

struct st_LWShaderAccess;
typedef void *       LWItemID;
#define LWITEM_NULL  ((LWItemID) 0)

typedef int     LWItemType;
#define LWI_OBJECT   0
#define LWI_LIGHT    1
#define LWI_CAMERA   2
#define LWI_BONE     3

typedef int      LWItemParam;
#define LWIP_POSITION    1
#define LWIP_RIGHT       2
#define LWIP_UP          3
#define LWIP_FORWARD     4
#define LWIP_ROTATION    5
#define LWIP_SCALING     6
#define LWIP_PIVOT       7
#define LWIP_W_POSITION  8
#define LWIP_W_RIGHT     9
#define LWIP_W_UP        10
#define LWIP_W_FORWARD   11
#define LWIP_PIVOT_ROT   12

typedef void* LWItemInstancerID;

typedef void* LWRenderData;

typedef double       LWRayCastFunc (const LWDVector position,
                    const LWDVector direction);

typedef double       LWRayTraceFunc (const LWDVector position,
                     const LWDVector direction,
                     LWDVector color);

typedef double       LWRayShadeFunc (const LWDVector position,
                     const LWDVector direction,
                     struct st_LWShaderAccess *shader_access);

typedef int      LWIlluminateFunc (LWItemID light, const LWDVector position,
                       LWDVector direction, LWDVector color);

typedef int      LWIlluminateNormalFunc (LWItemID light, LWRenderData rd, const LWDVector position,
                       LWDVector direction, const LWDVector normal_vec, LWDVector color);

typedef int      LightSampleFunc( void *data, LWItemID light, const LWDVector dir, const double color[4] );
typedef double   LWIlluminateSampleFunc( LWItemID light, const LWDVector pos, LWDVector dir,
                                     LightSampleFunc *sampler, void *data );

typedef double   LWIlluminateSampleNormalFunc( LWItemID light, LWRenderData rd, const LWDVector pos,
                                     const LWDVector normal_vec, LightSampleFunc *sampler, void *data );

typedef void     LWBufferGetValFunc( LWRenderData rd, int type, int num, double* );
typedef void     LWBufferSetValFunc( LWRenderData rd, int type, int num, double* );

#define RTMODE_DISTANCE     0
#define RTMODE_REFLECTION   1
#define RTMODE_REFRACTION   2
#define RTMODE_DISSOLVE     3
#define RTMODE_SHADOW       4
#define RTMODE_OCCLUSION    5
#define RTMODE_BACKDROP     6
#define RTMODE_OCCLUSION_DEPTH 7
#define RTMODE_MASK         0x000000FF

#define RTFLAG_CAMERA       0x04000000
#define RTFLAG_BACKSHADE    0x08000000
#define RTFLAG_BACKDROP     0x10000000
#define RTFLAG_OBJECT       0x20000000
#define RTFLAG_SAMPLEDRAY   0x40000000
#define RTFLAG_NOPOLYGON    0x80000000

typedef double       LWRayTraceModeFunc (const LWDVector position, const LWDVector direction,
                                         LWDVector color, const double eta, const int rtmode );

typedef void* LWRayData;

typedef struct st_LWRayTraceData
{
    // send
    LWDVector   rayStart;
    LWDVector   rayDir;
    LWDVector   backdrop; // Fill this if RTFLAG_BACKDROP is set.
    LWDVector   weight; // This color is used the weigh the contribution weight of the ray in the renderer.
    double      eta;
    int         flags;

    // receive
    LWRayData   ray; // This data needs to be filled by LWRayTraceData before calling LWRayTraceShade.
    double      len;
    LWDVector   result; // The result is filled with the backdrop color at LWRayTraceData if the ray doesn't hit anything.

} LWRayTraceData;

typedef void         LWRayTraceDataFunc( LWRenderData, LWRayTraceData* );
typedef void         LWRayTraceShadeFunc( LWRayTraceData* );

typedef void*        RandomFloatData;
typedef float        LWRandomFloatFunc (RandomFloatData);

typedef void*        LWSamplerState;
typedef float        LWGetSamplerRangeFunc (LWSamplerState, unsigned int*, unsigned int*);
typedef void         LWGetSample2DFunc (LWSamplerState, unsigned int, float*); // float* must point to 2 floats.
typedef float        LWGetSample1DFunc (LWSamplerState, unsigned int);

#define LWRT_SHADOW  (1<<0) // This flag is on if the incoming ray is a shadow ray
#define LWRT_PREVIEW (1<<1) // This flag is on if the spot is being rendered in a preview context, like the Viper.
#define LWRT_POLYSIDE (1<<2) // This flag is on if the incoming ray hit the back side of the polygon
#define LWRT_SAMPLEDRAY (1<<3) // This flag is on if the incoming ray was cast for sampling soft reflections, etc.
#define LWRT_PREPROCESS (1<<4) // This flag is on if the incoming ray is a radiosity preprocess ray.
#define LWRT_EXITING (1<<5) // This flag is on if the ray is going to exit to an undefined surface - Ie. vacuum/air, which needs to be handled by the material/shader.
#define LWRT_DOUBLESIDED (1<<6) // This flag is on if the surface is double sided.
#define LWRT_FRAME_PREPROCESS (1<<7) // This flag is on if the incoming ray is a frame preprocess ray.
#define LWRT_RADIOSITY (1<<8) // This flag is on if the incoming ray is a radiosity ray.
#define LWRT_CAMERA (1<<9) // This flag is on if the incoming ray is a camera ray.

#define LWITEM_RADIOSITY    ((LWItemID) 0x21000000)
#define LWITEM_CAUSTICS     ((LWItemID) 0x22000000)

/*
 * Animation item handler extensions.
 */

typedef struct st_LWNameChangeData {
    const char  *oldName;
    const char  *newName;
    LWItemID    id;
} LWNameChangeData;

typedef struct st_LWItemFuncs {
    const LWItemID *    (*useItems) (LWInstance);
    void        (*changeID) (LWInstance, const LWItemID *);
    void        (*changeName) (LWInstance, const LWNameChangeData *);
} LWItemFuncs;

typedef struct st_LWItemHandler {
    LWInstanceFuncs  *inst;
    LWItemFuncs  *item;
} LWItemHandler;

#define LWITEM_ALL  ((LWItemID) ~0)


/*
 * Render handler extensions.
 */
typedef struct st_LWRenderFuncs {
    LWError     (*init)    (LWInstance, int);
    void        (*cleanup) (LWInstance);
    LWError     (*newTime) (LWInstance, LWFrame, LWTime);
} LWRenderFuncs;

#define LWINIT_PREVIEW   0
#define LWINIT_RENDER    1

typedef struct st_LWRenderHandler {
    LWInstanceFuncs  *inst;
    LWItemFuncs      *item;
    LWRenderFuncs    *rend;
} LWRenderHandler;


/*
 * Globals.
 */
#define LWLISTINFO_GLOBAL   "LW List Info"

#define SPLICE_HEAD     (LWItemID)0
#define SPLICE_TAIL     (LWItemID)0xffffffff

enum
{
    LI_Failed = -1,     // returned only by index()
    LI_Success,
    LI_InvalidItemType,
    LI_ItemTypeMismatch,
    LI_InvalidPair,
    LI_InvalidBoneParent,
    LI_OutOfBounds,
    LI_MixedTypes,
    LI_MixedBones,
    LI_InvalidInsertionPoint,
    LI_CyclicInsertionPoint
};

typedef struct st_LWListInfo {
    int         (*index)(LWItemID);
    int         (*reorder)(LWItemID*);
    int         (*swap)(LWItemID*);
    int         (*splice)(LWItemID,LWItemID*);
} LWListInfo;

#define LWITEMINFO_GLOBAL   "LW Item Info 6"

typedef struct st_LWItemInfo {
    LWItemID        (*first)        (LWItemType, LWItemID);
    LWItemID        (*next)         (LWItemID);
    LWItemID        (*firstChild)   (LWItemID parent);
    LWItemID        (*nextChild)    (LWItemID parent, LWItemID prevChild);
    LWItemID        (*parent)       (LWItemID);
    LWItemID        (*target)       (LWItemID);
    LWItemID        (*goal)         (LWItemID);
    LWItemType      (*type)         (LWItemID);
    const char *    (*name)         (LWItemID);
    void            (*param)        (LWItemID, LWItemParam, LWTime, LWDVector);
    unsigned int    (*limits)       (LWItemID, LWItemParam, LWDVector min, LWDVector max);
    const char *    (*getTag)       (LWItemID, int);
    void            (*setTag)       (LWItemID, int, const char *);
    LWChanGroupID   (*chanGroup)    (LWItemID);
    const char *    (*server)       (LWItemID, const char *, int);
    unsigned int    (*serverFlags)  (LWItemID, const char *, int);
    void            (*controller)   (LWItemID, LWItemParam, int type[3]);
    unsigned int    (*flags)        (LWItemID);
    LWTime          (*lookAhead)    (LWItemID);
    double          (*goalStrength) (LWItemID, LWTime);
    void            (*stiffness)    (LWItemID, LWItemParam, LWDVector);
    unsigned int    (*axisLocks)    (LWItemID, LWItemParam);
    unsigned int    (*maxLookSteps) (LWItemID);
    double          (*reliableDistance)     (LWItemID);
    unsigned int    (*goalObjective)        (LWItemID);
    double          (*ikfkBlending)         (LWItemID, LWTime);
    unsigned int    (*ikInitialState)       (LWItemID);
    LWFrame         (*ikInitialStateFrame)  (LWItemID, LWTime);
    unsigned int    (*ikSoft)               (LWItemID, LWTime, unsigned int *distanceType, double *min, double *max);
    LWItemID        (*pole)                 (LWItemID);
    LWItemID        (*sameItem)             (LWItemID, LWItemParam);
    double          (*sameItemBlend)        (LWItemID, LWItemParam, LWTime);
    unsigned int    (*sameItemBlendMethod)  (LWItemID, LWItemParam);
    unsigned int    (*sameItemFlags)        (LWItemID, LWItemParam);
    unsigned int    (*selected)             (LWItemID);
    void            (*follow)               (LWItemID, LWItemParam, int follow[3], LWTextureID txtr[3]);
    void            (*controllerTrans)      (LWItemID, LWItemParam, double mul[3], double add[3]);
} LWItemInfo;

#define LWVECF_0    (1<<0)
#define LWVECF_1    (1<<1)
#define LWVECF_2    (1<<2)

#define LWSRVF_DISABLED (1<<0)
#define LWSRVF_HIDDEN   (1<<1)

#define LWMOTCTL_KEYFRAMES          0
#define LWMOTCTL_TARGETING          1
#define LWMOTCTL_ALIGN_TO_VELOCITY  2
#define LWMOTCTL_IK                 3
#define LWMOTCTL_ALIGN_TO_PATH      4
#define LWMOTCTL_POLE               5
#define LWMOTCTL_SAMEITEM           6

#define LWITEMF_ACTIVE          (1<<0)
#define LWITEMF_UNAFFECT_BY_IK  (1<<1)
#define LWITEMF_FULLTIME_IK     (1<<2)
#define LWITEMF_GOAL_ORIENT     (1<<3)
#define LWITEMF_REACH_GOAL      (1<<4)
#define LWITEMF_USE_IKCHAINVALS (1<<5)

#define LWMOTGOAL_POSITION  0
#define LWMOTGOAL_POINT     1
#define LWMOTGOAL_ONYZPLANE 2

#define LWMOTIK_INIT_FIRSTKEY       0
#define LWMOTIK_INIT_MOSTRECENTKEY  1
#define LWMOTIK_INIT_CURRENTCHAN    2
#define LWMOTIK_INIT_CUSTOMFRAME    3

#define LWMOTIK_SOFT_OFF            0
#define LWMOTIK_SOFT_EXPONENTIAL    1
#define LWMOTIK_SOFT_LINEAR         2
#define LWMOTIK_SOFT_CLAMP          3

#define LWMOTIK_SOFT_CHAINCHORD     0
#define LWMOTIK_SOFT_CHAINLENGTH    1
#define LWMOTIK_SOFT_CUSTOMDISTANCE 2

#define LWMOTCTL_SAMEITEM_INTERPOLATE 0
#define LWMOTCTL_SAMEITEM_COMPENSATE  1
#define LWMOTCTL_SAMEITEM_WORLD       (1<<7)


#define LWOBJECTINFO_GLOBAL "LW Object Info 9"

typedef struct st_LWObjectInfo {
    const char * (*filename)( LWItemID objectitem);
    int (*numPoints)( LWItemID objectitem);
    int (*numPolygons)( LWItemID objectitem);
    unsigned int (*shadowOpts)( LWItemID objectitem);
    double (*dissolve)( LWItemID objectitem, LWTime in_time);
    LWMeshInfoID (*meshInfo)( LWItemID objectitem, int frozen_flag);
    unsigned int (*flags)( LWItemID objectitem);
    double (*fog)( LWItemID objectitem, LWTime in_time);
    LWTextureID (*dispMap)( LWItemID objectitem);
    LWTextureID (*clipMap)( LWItemID objectitem);
    void (*patchLevel)( LWItemID objectitem, int *for_display, int *for_render);
    void (*metaballRes)( LWItemID objectitem, double *for_display, double *for_render);
    LWItemID (*boneSource)( LWItemID objectitem);
    LWItemID (*morphTarget)( LWItemID objectitem);
    double (*morphAmount)( LWItemID objectitem, LWTime in_time);
    unsigned int (*edgeOpts)( LWItemID objectitem);
    void (*edgeColor)( LWItemID objectitem, LWTime in_time, LWDVector color);
    int (*subdivOrder)( LWItemID objectitem);
    double (*polygonSize)( LWItemID objectitem, LWTime in_time);
    int (*excluded)( LWItemID objectitem, LWItemID lightitem);
    void (*matteColor)( LWItemID objectitem, LWTime in_time, LWDVector color);
    double (*thickness)( LWItemID objectitem, LWTime in_time, int type);
    double (*edgeZScale)( LWItemID objectitem, LWTime in_time);
    double (*shrinkEdgesNominalDistance)( LWItemID objectitem, LWTime in_time); /* added for version 5: obtains nominal distance value used when shrink edges is enabled. */
    double (*maxDissolveDistance)( LWItemID objectitem); /* added for version 6 */
    double (*bumpDistance)( LWItemID objectitem); /* added for version 6 */
    unsigned int (*getGroupIndex)( LWItemID objecitem); /* added for version 7 */
    int (*dispMapOrder)( LWItemID objectitem);
    int (*bumpOrder)( LWItemID objectitem);
    NodeEditorID (*getNodeEditor)( LWItemID objectitem);
    int (*nodeOrder)( LWItemID objectitem);
    void (*bounds)( LWItemID objectitem, LWDVector min, LWDVector max ); /* added for version 8 */
    double (*shadowoffsetdistance)( LWItemID );
    LWItemInstancerID (*instancer)( LWItemID ); /* added for version 9 */
} LWObjectInfo;

#define LWOSHAD_SELF        (1<<0)
#define LWOSHAD_CAST        (1<<1)
#define LWOSHAD_RECEIVE     (1<<2)

#define LWOBJF_UNSEEN_BY_CAMERA (1<<0)
#define LWOBJF_UNSEEN_BY_RAYS   (1<<1)
#define LWOBJF_UNAFFECT_BY_FOG  (1<<2)
#define LWOBJF_MORPH_MTSE   (1<<3)
#define LWOBJF_MORPH_SURFACES   (1<<4)
#define LWOBJF_MATTE        (1<<5)
#define LWOBJF_UNSEEN_BY_ALPHA  (1<<6) /* This is set when object alpha is set to 'Unaffected By Object' */
#define LWOBJF_ALPHA_CONSTANT_BLACK  (1<<7) /* This is set when object alpha is set to 'Constant Black' */
#define LWOBJF_DISTANCEDISSOLVE (1<<8) /* Added in version 6 */
#define LWOBJF_BUMPENABLE (1<<9) /* Added in version 6 */
#define LWOBJF_FASTERBONES (1<<10) /* Added in version 6 */
#define LWOBJF_USEMORPHEDPOSITIONS (1<<11) /* Added in version 6 */
#define LWOBJF_CONTAINS1POINTPOLYGONS (1<<12) /* Added in version 6 */
#define LWOBJF_CONTAINSPATCHES (1<<13) /* Added in version 6 */
#define LWOBJF_CONTAINSMETABALLS (1<<14) /* Added in version 6 */
#define LWOBJF_CONTAINSPARTIGONS (1<<15) /* Added in version 6 */
#define LWOBJF_CONTAINSCUSTOMPOLYGONS (1<<16) /* Added in version 6 */
#define LWOBJF_NODEDISPENABLED (1<<17) /* Added in version 7 */
#define LWOBJF_NODEFORMERSAPPLIED (1<<18) /* Added in version 7 */
#define LWOBJF_UNSEEN_BY_RADIOSITY (1<<19)

#define LWEDGEF_SILHOUETTE  (1<<0)
#define LWEDGEF_UNSHARED    (1<<1)
#define LWEDGEF_CREASE      (1<<2)
#define LWEDGEF_SURFACE     (1<<3)
#define LWEDGEF_OTHER       (1<<4)
#define LWEDGEF_SHRINK_DIST (1<<8)

#define LWTHICK_SILHOUETTE      0
#define LWTHICK_UNSHARED        1
#define LWTHICK_CREASE          2
#define LWTHICK_SURFACE         3
#define LWTHICK_OTHER           4
#define LWTHICK_LINE            5
#define LWTHICK_PARTICLE_HEAD   6
#define LWTHICK_PARTICLE_TAIL   7

#define LWBONEINFO_GLOBAL   "LW Bone Info 5"

typedef struct st_LWBoneInfo {
    unsigned int    (*flags)      (LWItemID);
    void            (*restParam)  (LWItemID, LWItemParam, LWDVector);
    double          (*restLength) (LWItemID);
    void            (*limits)     (LWItemID, double *inner, double *outer);
    const char *    (*weightMap)  (LWItemID);
    double          (*strength)   (LWItemID);
    int             (*falloff)    (LWItemID);
    void            (*jointComp)  (LWItemID, double *self, double *parent);
    void            (*muscleFlex) (LWItemID, double *self, double *parent);
    int             (*type)       (LWItemID);
    double          (*twist)      (LWItemID);
    int             (*transform)  (LWItemID, int transform, int relativeto, LWFMatrix3 m, LWFVector pos, LWFVector end);
    void            (*muscleBulge)(LWItemID, double *self, double *parent);
    void            (*muscleBulgeMap)   (LWItemID, LWTextureID *self, LWTextureID *parent);
    void            (*displacementMap)  (LWItemID, LWTextureID *self, LWTextureID *parent);
} LWBoneInfo;

#define LWBONEF_ACTIVE              (1<<0)
#define LWBONEF_LIMITED_RANGE       (1<<1)
#define LWBONEF_SCALE_STRENGTH      (1<<2)
#define LWBONEF_WEIGHT_MAP_ONLY     (1<<3)
#define LWBONEF_WEIGHT_NORM         (1<<4)
#define LWBONEF_JOINT_COMP          (1<<5)
#define LWBONEF_JOINT_COMP_PAR      (1<<6)
#define LWBONEF_MUSCLE_FLEX         (1<<7)
#define LWBONEF_MUSCLE_FLEX_PAR     (1<<8)
#define LWBONEF_TWIST               (1<<9)
#define LWBONEF_MUSCLE_BULGE        (1<<10)
#define LWBONEF_MUSCLE_BULGE_PAR    (1<<11)

#define LWBONETYPE_ZAXIS        0
#define LWBONETYPE_JOINT        1

#define LWBONETRANS_REST        0
#define LWBONETRANS_FINAL       1

#define LWBONETRANS_OBJECT      0
#define LWBONETRANS_PARENT      1
#define LWBONETRANS_WORLD       2


#define LWLIGHTINFO_GLOBAL  "LW Light Info 5"

typedef struct st_LWLightInfo {
    void        (*ambient)      (LWTime, LWDVector color);
    int         (*type)         (LWItemID);
    void        (*color)        (LWItemID, LWTime, LWDVector color);
    int         (*shadowType)   (LWItemID);
    void        (*coneAngles)   (LWItemID, LWTime, double *radius, double *edge);
    unsigned int    (*flags)    (LWItemID);
    double      (*range)        (LWItemID, LWTime);
    int         (*falloff)      (LWItemID);
    LWImageID   (*projImage)    (LWItemID);
    int         (*shadMapSize)  (LWItemID);
    double      (*shadMapAngle) (LWItemID, LWTime);
    double      (*shadMapFuzz)  (LWItemID, LWTime);
    int         (*quality)      (LWItemID, LWTime);
    void        (*rawColor)     (LWItemID, LWTime, LWDVector color);
    double      (*intensity)    (LWItemID, LWTime);
    void        (*shadowColor)  (LWItemID, LWTime, LWDVector color);
    double      (*ambientIntensity)  (LWTime);
    void        (*ambientRaw)   (LWTime, LWDVector color);
} LWLightInfo;

#define LWLIGHT_DISTANT  0
#define LWLIGHT_POINT    1
#define LWLIGHT_SPOT     2
#define LWLIGHT_LINEAR   3
#define LWLIGHT_AREA     4
#define LWLIGHT_CUSTOM   100

#define LWLSHAD_OFF      0
#define LWLSHAD_RAYTRACE 1
#define LWLSHAD_MAP      2

#define LWLFL_LIMITED_RANGE     (1<<0)
#define LWLFL_NO_DIFFUSE        (1<<1)
#define LWLFL_NO_SPECULAR       (1<<2)
#define LWLFL_NO_CAUSTICS       (1<<3)
#define LWLFL_LENS_FLARE        (1<<4)
#define LWLFL_VOLUMETRIC        (1<<5)
#define LWLFL_NO_OPENGL         (1<<6)
#define LWLFL_FIT_CONE          (1<<7)
#define LWLFL_CACHE_SHAD_MAP    (1<<8)

#define LWLFALL_OFF         0
#define LWLFALL_LINEAR      1
#define LWLFALL_INV_DIST    2
#define LWLFALL_INV_DIST_2  3


#define LWCAMERAINFO_GLOBAL "LW Camera Info 6"

typedef struct st_LWCameraRay
{
    LWDVector rayPos;
    LWDVector rayDir;
    LWDVector filmNorm;
    LWDVector filter[3];
} LWCameraRay;

typedef struct st_LWFrameInfo
{
    LWFrame frame;
    LWTime start;
    LWTime duration;
    double framesPerSecond;
    unsigned int motionSteps;
} LWFrameInfo;

typedef struct st_LWComponentTimingInfo
{
    unsigned int count;
    double offset;
    double duration;
    double stride;
} LWComponentTimingInfo;

typedef struct st_LWFrameTimingInfo
{
    LWComponentTimingInfo frame;
    LWComponentTimingInfo fields;
    LWComponentTimingInfo steps;
    LWComponentTimingInfo scanlines;
    LWComponentTimingInfo pixels;
} LWFrameTimingInfo;

typedef struct st_LWCameraInfo {
    double      (*zoomFactor)    (LWItemID, LWTime);
    double      (*focalLength)   (LWItemID, LWTime);
    double      (*focalDistance) (LWItemID, LWTime);
    double      (*fStop)         (LWItemID, LWTime);
    double      (*blurLength)    (LWItemID, LWTime);
    void        (*fovAngles)     (LWItemID, LWTime, double *horizontal, double *vertical);
    unsigned int    (*flags)     (LWItemID);
    void        (*resolution)    (LWItemID, int *width, int *height);
    double      (*pixelAspect)   (LWItemID, LWTime);
    double      (*separation)    (LWItemID, LWTime);
    void        (*regionLimits)  (LWItemID, int *out_x0, int *out_y0, int *out_x1, int *out_y1);
    void        (*maskLimits)    (LWItemID, int *out_x0, int *out_y0, int *out_x1, int *out_y1); // defunct
    void        (*maskColor)     (LWItemID, LWDVector color); // defunct
    unsigned int (*motionBlur)   (LWItemID); /* added for version 3: retrieve motion blur setting for a camera item, 1=normal, 2=dither */
    unsigned int (*fieldRendering)(LWItemID); /* added for version 3: retrieve field rendering state. */
    int         (*irisPos)        (LWItemID, LWTime, int pass, float *ix, float *iy);

    int         (*usingGlobalResolution)(LWItemID); // defunct
    int         (*usingGlobalBlur)(LWItemID); // defunct
    int         (*usingGlobalMask)(LWItemID); // defunct

    unsigned int (*motionBlurPasses)  (LWItemID, LWTime);
    double       (*shutterEfficiency) (LWItemID, LWTime);
    unsigned int (*noiseSampler)      (LWItemID);
    void         (*filmSize)          (LWItemID, double *width, double *height);
    unsigned int (*frameTiming)       (LWItemID, LWFrame, LWFrameTimingInfo *timingInfo);
    unsigned int (*antiAliasing)      (LWItemID, LWTime);
    double       (*overSampling)      (LWItemID, LWTime);
    void         (*diaphragm)         (LWItemID, LWTime, int *sides, double *rotation);
    double       (*convergencePoint)  (LWItemID, LWTime);
    int          (*usingConvergencePoint)(LWItemID);
    double       (*convergenceToeIn)  (LWItemID, LWTime);
} LWCameraInfo;


#define LWCAMF_STEREO           (1<<0)
#define LWCAMF_LIMITED_REGION   (1<<1) /* This indicates that some form of limited region is enabled */
#define LWCAMF_MASK             (1<<2) // defunct
#define LWCAMF_DOF              (1<<3)
#define LWCAMF_PARTICLE_BLUR    (1<<4)
#define LWCAMF_LR_NOBORDERS     (1<<5) /* This indicates that limited region is being used without borders when limited region is enabled */
#define LWCAMF_FIELD            (1<<6) /* In Field Rendering, This Indicates Which Field Is Being Processed. */
#define LWCAMF_USECAMTYPE       (1<<7) /* Indicates that the camera uses a non-classical camera type. (rev.4+) */
#define LWCAMF_SUPPORTS_DOF     (1<<8) /* Indicates that the camera supports Depth Of Field rendering */
#define LWCAMF_EYE              (1<<9) /* In Stereo Rendering, This Indicates Which Eye Is Being Processed. */
#define LWCAMF_SUPPORTS_STEREO  (1<<10) /* Indicates that the camera supports stereo rendering */

#define LWCAMMB_OFF 0
#define LWCAMMB_NORMAL 1
#define LWCAMMB_DITHERED 2
#define LWCAMMB_PHOTOREAL 3

#define LWCAMFIELDR_OFF 0
#define LWCAMFIELDR_EVENFIRST 1
#define LWCAMFIELDR_ODDFIRST 2


#define LWCAMERAEVALUATIONFUNCS_GLOBAL "LW Camera Evaluation Funcs 2"

typedef void* LWCameraEvaluatorID;

typedef struct st_LWCameraEvaluationFuncs
{
    LWCameraEvaluatorID (*create) (LWItemID camera);
    void    (*destroy)  (LWCameraEvaluatorID);
    LWError (*init)     (LWCameraEvaluatorID, int mode);
    void    (*cleanUp)  (LWCameraEvaluatorID);
    LWError (*newTime)  (LWCameraEvaluatorID, LWFrame frame, LWTime in_time);
    int     (*preview)  (LWCameraEvaluatorID, double lpx, double lpy, LWDMatrix4 projection);
    LWError (*newFrame) (LWCameraEvaluatorID);
    int     (*evaluate) (LWCameraEvaluatorID, double fpx, double fpy, double lpx, double lpy, double fractime, LWCameraRay* camray);
} LWCameraEvaluationFuncs;


enum    // for use with LWSceneInfo->loadInProgress
{
    SI_NoLoad = 0,
    SI_LoadScene,
    SI_LoadFromScene
};

enum {
    LWRT_BackDrop = 0,  /* Throws rays in a random distribution, if a miss, used backdrop color. */
    LWRT_MonteCarlo,    /* Throws rays in a random distribution. */
    LWRT_FinalGather,   /* Final Gather implementation. */
    LWRT_Sizeof
};

typedef enum en_lwrenderingmode {
  lwrm_None = 0,        /*!< not currently rendering. */
  lwrm_SceneBake,       /*!< a scene frame baking.    */
  lwrm_Scene,           /*!< a scene.                 */
  lwrm_FrameBake,       /*!< a single frame baking.   */
  lwrm_Frame,           /*!< a single frame.          */
  lwrm_Inspire,         /*!< a scene but with dimensions greater than 800 x 600 for Inspire. */
  lwrm_SelectedObject,  /*!< selected objects only.   */
  lwrm_sizeof
} LWRENDERINGMODE;

/* Stereoscopic eyes. */

typedef enum
{
    LWCAMEYE_CENTER = 0,/*!< Stereoscopic center eye. */
    LWCAMEYE_LEFT,      /*!< Stereoscopic left eye.   */
    LWCAMEYE_RIGHT,     /*!< Stereoscopic right eye.  */
    LWCAMEYE_sizeof
} LWCameraEye;

typedef enum
{
    LWANIMPASS_MAIN = 0,
    LWANIMPASS_PRELIMINARY,
    LWANIMPASS_BLUR,
    LWANIMPASS_RESTORE
} LWAnimPass;

#define LWSCENEINFO_GLOBAL "LW Scene Info 9"

typedef struct st_LWSceneInfo {
    const char *name;
    const char *filename;
    int numPoints;
    int numPolygons;
    int renderType;
    int renderOpts;
    LWFrame frameStart;
    LWFrame frameEnd;
    LWFrame frameStep;
    double framesPerSecond;
    int frameWidth;
    int frameHeight;
    double pixelAspect;
    int minSamplesPerPixel;
    int maxSamplesPerPixel;
    int limitedRegion[4];   /* x0, y0, x1, y1 */
    int recursionDepth;
    LWItemID (*renderCamera) (LWTime);
    int numThreads;
    const char *animFilename;
    const char *RGBPrefix;
    const char *alphaPrefix;
    int antialiasing; /* added for version 4: current antialiasing setting 0=off, 1-lowest, increasing for higher values (currently up to 4=extreme)*/
        /* added 12/29/2004: 100 to 114; use PLD (with level 1 being 100 and level 15 being 114) */
    int enhancedAA; /* added for version 4: 1=enabled, 0=disabled. Only valid when antialiasing is 1 thru 4*/
    int adaptiveSampling; /* added for version 4: 1=enabled, 0=disabled */
    float adaptiveThreshold; /* added for version 4: only valid when adaptiveSampling is enabled */
    int filter; /* added for version 4: bit 0: 1=soft filter, 0=no filter */
        /* added 12/29/2004: bits 1thru5 indicate reconstruction filter.
            1-3=box(std,sharp,soft); 4-6=Gaussian(std,sharp,soft); 7-9=Mitchell(std,sharp,soft); 10-12=Lanczos(std,sharp,soft) */
    int dblSidedAreaLights;
    int loadInProgress;         /* version 5: 0=no scene loading; 1=scene load is in progress; 2=load-from-scene in progress */
    int         radiosityType;
    int         radiosityIndirectBounceCount;
    int         radiosityRaysPerEvaluation1;
    int         radiosityRaysPerEvaluation2;
    double      radiosityIntensity;
    double      radiosityAngularTolerance;
    double      radiosityMinimumPixelSpacing;
    double      radiosityMaximumPixelSpacing;
    double      radiosityMultiplier;
    const char *radiosityFileName;
    /* Added for LW9.6 */
    double      causticsIntensity;
    int         causticsAccuracy;
    int         causticsSoftness;
    int         radiosityFlags;
    double      dynamicsRangeMin;
    double      dynamicsRangeMax;
    double      saturation;

    int         pixelFilterForceMT;
    int         has_multithreaded_filters;      /* All active pixel filters are capable of multi-threading. */
    int         useZminmax;
    double      zmin;
    double      zmax;
    int         rendermode;                     /* LWRENDERINGMODE enum. */
    int         calculateallnormals;            /* Forces the calculation of normals, in layout or while rendering. */
    /* Added for LW9.7 */
    int         enableLenseFlares;
    double      lenseFlareIntensity;
    /* Added for LW10.1 */
    int        *buffersInUse;                   /* Pointer to the flags array of the buffers currently in use. */
    int         animationPass;                  /* Current phase of animation calculation (LWANIMPASS_). */

    /* Added for LW11 */
    float           raycutoff;
    unsigned int    shadingsamples;
    unsigned int    lightsamples;

} LWSceneInfo;

/* SceneInfo.renderType */
#define LWRTYPE_WIRE         0
#define LWRTYPE_QUICK        1
#define LWRTYPE_REALISTIC    2

/* SceneInfo.renderOpts */
#define LWROPT_SHADOWTRACE (1<<0)
#define LWROPT_REFLECTTRACE (1<<1)
#define LWROPT_REFRACTTRACE (1<<2)
#define LWROPT_FIELDS (1<<3)
#define LWROPT_EVENFIELDS (1<<4)
#define LWROPT_MOTIONBLUR (1<<5)
#define LWROPT_DEPTHOFFIELD (1<<6)
#define LWROPT_LIMITEDREGION (1<<7)
#define LWROPT_PARTICLEBLUR (1<<8)
#define LWROPT_ENHANCEDAA (1<<9)
#define LWROPT_SAVEANIM (1<<10)
#define LWROPT_SAVERGB (1<<11)
#define LWROPT_SAVEALPHA (1<<12)
#define LWROPT_ZBUFFERAA (1<<13) /* added 12/29/2004 */
#define LWROPT_RTTRANSPARENCIES (1<<14) /* added 11/01/2006 */
#define LWROPT_RADIOSITY (1<<15)
#define LWROPT_CAUSTICS (1<<16)
#define LWROPT_OCCLUSION (1<<17)
#define LWROPT_RENDERLINES (1<<18)
#define LWROPT_INTERPOLATED (1<<19)
#define LWROPT_BLURBACKGROUND (1<<20)
#define LWROPT_USETRANSPARENCY (1<<21)
#define LWROPT_VOLUMETRICRADIOSITY  (1<<22)
#define LWROPT_USEAMBIENT           (1<<23)
#define LWROPT_DIRECTIONALRAYS      (1<<24)
#define LWROPT_LIMITDYNAMICRANGE    (1<<25)
#define LWROPT_CACHERADIOSITY       (1<<26)
#define LWROPT_USEGRADIENTS         (1<<27)
#define LWROPT_USEBEHINDTEST        (1<<28)
#define LWROPT_CAUSTICSCACHE        (1<<29)
#define LWROPT_EYECAMERA            (1<<30)
#define LWROPT_UNPREMULTIPLIEDALPHA (1<<31)

/* SceneInfo.radiosityFlags */
#define LWRDFLG_SHOW_NODES                        (1 << 0)
#define LWRDFLG_SHOW_CELLS                        (1 << 1)
#define LWRDFLG_SHOW_COLOR_CELLS                  (1 << 2)
#define LWRDFLG_SHOW_SAMPLES                      (1 << 3)
#define LWRDFLG_SHOW_MISSING_PREPROCESS_SAMPLES   (1 << 4)
#define LWRDFLG_SHOW_MISSING_RENDER_SAMPLES       (1 << 5)
#define LWRDFLG_SHOW_SECOND_BOUNCE                (1 << 6)
#define LWRDFLG_SHOW_BEHIND                       (1 << 7)
#define LWRDFLG_USE_BUMPS                         (1 << 31)

#define LWTIMEINFO_GLOBAL   "LW Time Info"

typedef struct st_LWTimeInfo {
    LWTime       time;
    LWFrame      frame;
} LWTimeInfo;


#define LWCOMPINFO_GLOBAL   "LW Compositing Info"

typedef struct st_LWCompInfo {
    LWImageID    bg;
    LWImageID    fg;
    LWImageID    fgAlpha;
} LWCompInfo;


#define LWBACKDROPINFO_GLOBAL   "LW Backdrop Info 2"

typedef struct st_LWBackdropInfo {
    void        (*backdrop) (LWTime, const double ray[3], double color[3]);
    int       type;
    void        (*color)    (LWTime, double zenith[3], double sky[3], double ground[3], double nadir[3]);
    void        (*squeeze)  (LWTime, double *sky, double *ground);
} LWBackdropInfo;

#define LWBACK_SOLID        0
#define LWBACK_GRADIENT     1


#define LWFOGINFO_GLOBAL    "LW Fog Info"

typedef struct st_LWFogInfo {
    int       type;
    unsigned int      flags;
    double      (*minDist) (LWTime);
    double      (*maxDist) (LWTime);
    double      (*minAmt)  (LWTime);
    double      (*maxAmt)  (LWTime);
    void        (*color)   (LWTime, double col[3]);
} LWFogInfo;

#define LWFOG_NONE          0
#define LWFOG_LINEAR        1
#define LWFOG_NONLINEAR1    2
#define LWFOG_NONLINEAR2    3
#define LWFOG_REALISTIC     4

#define LWFOGF_BACKGROUND   (1<<0)


#define LWINTERFACEINFO_GLOBAL  "LW Interface Info 7"

typedef struct st_LWInterfaceInfo {
    LWTime            curTime;
    const LWItemID   *selItems;
    unsigned int    (*itemFlags)            (LWItemID item);
    LWFrame           previewStart;
    LWFrame           previewEnd;
    LWFrame           previewStep;
    int               dynaUpdate;
    void            (*schemaPos)            (LWItemID item, double *x, double *y);
    int             (*itemVis)              (LWItemID item);
    unsigned int      displayFlags;
    unsigned int      generalFlags;
    int               boxThreshold;
    int             (*itemColor)            (LWItemID item);
    void            (*setItemColorIndex)    (LWItemID item, int color);
    void            (*setItemColorCustom)   (LWItemID item, double *color);
    int               alertLevel;
    int               autoKeyCreate;
    void            (*defaultItemTypeColor) ( LWItemType itemtype, float *color, int set);
    void            (*itemColorRgba)        (LWItemID item, unsigned int state, float rgba[4]);
    float           (*itemIconScale)        (LWItemID item);
} LWInterfaceInfo;

#define LWITEMF_SELECTED    (1<<0)
#define LWITEMF_SHOWCHILDREN    (1<<1)
#define LWITEMF_SHOWCHANNELS    (1<<2)
#define LWITEMF_LOCKED      (1<<3)

#define LWDYNUP_OFF     0
#define LWDYNUP_DELAYED     1
#define LWDYNUP_INTERACTIVE 2

#define LWIVIS_HIDDEN       0
#define LWIVIS_VISIBLE      1

#define LWOVIS_HIDDEN        0
#define LWOVIS_BOUNDINGBOX   1
#define LWOVIS_VERTICES      2
#define LWOVIS_WIREFRAME     3
#define LWOVIS_FFWIREFRAME   4
#define LWOVIS_SHADED        5
#define LWOVIS_TEXTURED      6
#define LWOVIS_TEXTURED_WIRE 7
#define LWOVIS_VIEWPORTOBJ   8

#define LWAKC_OFF       0
#define LWAKC_MODIFIED      1
#define LWAKC_ALL       2

#define LWDISPF_MOTIONPATHS (1<<0)
#define LWDISPF_HANDLES     (1<<1)
#define LWDISPF_IKCHAINS    (1<<2)
#define LWDISPF_CAGES       (1<<3)
#define LWDISPF_SAFEAREAS   (1<<4)
#define LWDISPF_FIELDCHART  (1<<5)

#define LWGENF_HIDETOOLBAR          (1<<0)
#define LWGENF_RIGHTTOOLBAR         (1<<1)
#define LWGENF_PARENTINPLACE        (1<<2)
#define LWGENF_FRACTIONALFRAME      (1<<3)
#define LWGENF_KEYSINSLIDER         (1<<4)
#define LWGENF_PLAYEXACTRATE        (1<<5)
#define LWGENF_AUTOKEY              (1<<6)
#define LWGENF_IKENABLE             (1<<7)
#define LWGENF_LMBITEMSELECT        (1<<8)
#define LWGENF_AUTOCONF             (1<<9)
#define LWGENF_DOUBLECLICKBONEMODE  (1<<10)
#define LWGENF_MCENABLE             (1<<11)

#define LWALERT_BEGINNER    0
#define LWALERT_INTERMEDIATE    1
#define LWALERT_EXPERT      2

#define LWITEMCOL_CURRENT   0
#define LWITEMCOL_NORMAL    1
#define LWITEMCOL_SELECTED  2

#define LWVIEWPORTINFO_GLOBAL   "LW Viewport Info 5"

typedef struct st_LWViewportInfo {
    int         numViewports;
    int         (*type)      (int);
    unsigned int(*flags)     (int);
    void        (*pos)       (int, LWDVector);
    void        (*xfrm)      (int, double mat[9]);
    void        (*clip)      (int, double *hither, double *yon);
    void        (*rect)      (int, int *left, int *top, int *width, int *height);
    int         (*viewLevel) (int);
    int         (*projection)(int, LWDMatrix4 projection, LWDMatrix4 inverse_projection);
    int         (*modelview) (int, LWDMatrix4 modelview, LWDMatrix4 inverse_modelview);
    int         (*project)   (int, LWDVector world, double* winx, double* winy, double* winz);
    int         (*unproject) (int, double winx, double winy, double winz, LWDVector world);
    double      (*pixelSize) (int, double pixels, LWDVector refpos);
    int         (*handleSize)(int);
    double      (*gridSize)  (int);
    LWItemID    (*viewItem)  (int);
    int         (*gridType)  (int);
} LWViewportInfo;

#define LVVIEWT_NONE        0
#define LVVIEWT_TOP         1
#define LVVIEWT_BOTTOM      2
#define LVVIEWT_BACK        3
#define LVVIEWT_FRONT       4
#define LVVIEWT_RIGHT       5
#define LVVIEWT_LEFT        6
#define LVVIEWT_PERSPECTIVE 7
#define LVVIEWT_LIGHT       8
#define LVVIEWT_CAMERA      9
#define LVVIEWT_SCHEMATIC   10

#define LWVIEWF_CENTER      (1<<0)
#define LWVIEWF_WEIGHTSHADE (1<<1)
#define LWVIEWF_XRAY        (1<<2)
#define LWVIEWF_HEADLIGHT   (1<<3)


#define LWGLOBALPOOL_RENDER_GLOBAL  "Global Render Memory"
#define LWGLOBALPOOL_GLOBAL         "Global Memory"

typedef void *LWMemChunk;

typedef struct st_LWGlobalPool {
    LWMemChunk  (*first) (void);
    LWMemChunk  (*next)  (LWMemChunk);
    const char *(*ID)    (LWMemChunk);
    int         (*size)  (LWMemChunk);

    LWMemChunk  (*find)  (const char *in_ID);
    LWMemChunk  (*create)(const char *in_ID, int in_size);
} LWGlobalPool;

#endif
