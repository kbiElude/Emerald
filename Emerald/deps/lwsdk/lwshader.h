/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWSHADER.H -- LightWave Surface Shaders
 */
#ifndef LWSDK_SHADER_H
#define LWSDK_SHADER_H

#include <lwrender.h>
#include <lwinstancing.h>

#define LWSHADER_HCLASS     "ShaderHandler"
#define LWSHADER_ICLASS     "ShaderInterface"
#define LWSHADER_GCLASS     "ShaderGizmo"
#define LWSHADER_VERSION    7


typedef struct st_LWShaderAccess {
    int                           sx, sy;
    double                        oPos[3], wPos[3];
    double                        gNorm[3];
    double                        spotSize;
    double                        raySource[3];
    double                        rayLength;
    double                        cosine;
    double                        oXfrm[9], wXfrm[9];
    LWItemID                      objID;
    int                           polNum;

    double                        wNorm[3];
    double                        color[3];
    double                        luminous;
    double                        diffuse;
    double                        specular;
    double                        mirror;
    double                        transparency;
    double                        eta;
    double                        roughness;

    LWIlluminateFunc             *illuminate;
    LWRayTraceFunc               *rayTrace;
    LWRayCastFunc                *rayCast;
    LWRayShadeFunc               *rayShade;

    int                           flags;
    int                           bounces;
    LWItemID                      sourceID;
    double                        wNorm0[3];
    double                        bumpHeight;
    double                        translucency;
    double                        colorHL;
    double                        colorFL;
    double                        addTransparency;
    double                        difSharpness;

    LWPntID                       verts[4];         // surrounding vertex IDs
    float                         weights[4];       // vertex weigths
    float                         vertsWPos[4][3];  // vertex world positions
    LWPolID                       polygon;          // polygon ID

    double                        replacement_percentage;
    double                        replacement_color[3];
    double                        reflectionBlur;
    double                        refractionBlur;

    LWRayTraceModeFunc           *rayTraceMode;
    double                        incomingEta;
    int                           ResourceContextIndex;
    double                        diffuse_shade[3];
    double                        specular_shade[3];
    double                        refract_shade[3];
    double                        reflect_shade[3];

    LWIlluminateSampleFunc       *illuminateSample;
    RandomFloatData               randomData;
    LWRandomFloatFunc            *randomFloat;

    LWIlluminateNormalFunc       *illuminateNormal;
    LWIlluminateSampleNormalFunc *illuminateSampleNormal;

    double                        subsx, subsy;
    double                        bumpDropoff;

    LWDVector                     rayDir;

    LWRayTraceDataFunc           *rayTraceData;
    LWRayTraceShadeFunc          *rayTraceShade;
    LWRenderData                  render;

    LWBufferGetValFunc           *getVal;
    LWBufferSetValFunc           *setVal;

    LWItemInstanceID              instanceID;

    LWSamplerState                samplerState;
    LWGetSamplerRangeFunc        *getSamplerRange;
    LWGetSample2DFunc            *getSample2D;
    LWGetSample1DFunc            *getSample1D;

} LWShaderAccess;

typedef struct st_LWShaderHandler {
    LWInstanceFuncs *inst;
    LWItemFuncs     *item;
    LWRenderFuncs   *rend;
    void           (*evaluate) (LWInstance, LWShaderAccess *);
    unsigned int   (*flags)    (LWInstance);
} LWShaderHandler;

#define LWSHF_NORMAL            (1<<0)
#define LWSHF_COLOR             (1<<1)
#define LWSHF_LUMINOUS          (1<<2)
#define LWSHF_DIFFUSE           (1<<3)
#define LWSHF_SPECULAR          (1<<4)
#define LWSHF_MIRROR            (1<<5)
#define LWSHF_TRANSP            (1<<6)
#define LWSHF_ETA               (1<<7)
#define LWSHF_ROUGH             (1<<8)
#define LWSHF_TRANSLUCENT       (1<<9)
#define LWSHF_RAYTRACE          (1<<10)
#define LWSHF_DIFFUSE_SHADE     (1<<11)
#define LWSHF_SPECULAR_SHADE    (1<<12)
#define LWSHF_REFRACT_SHADE     (1<<13)
#define LWSHF_REFLECT_SHADE     (1<<14)
#define LWSHF_MAX_CHANNELS      15
#endif
