/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWVOLUME.H -- LightWave Volumetric Elements
 *
 * This header defines the volumetric rendering element.
 */
#ifndef LWSDK_VOLUME_H
#define LWSDK_VOLUME_H

#include <lwrender.h>

#define LWVOLUMETRIC_HCLASS		"VolumetricHandler"
#define LWVOLUMETRIC_ICLASS		"VolumetricInterface"
#define LWVOLUMETRIC_GCLASS		"VolumetricGizmo"
#define LWVOLUMETRIC_VERSION	 7


/*
 * A volume sample is a single segment along a ray through a
 * volmetric function that has a uniform color and opacity.  The
 * dist and stride are the position and size of the sample and
 * the opacity and color are given as color vectors.
 */
typedef struct st_LWVolumeSample {
	double			dist;
	double			stride;
	double			opacity[3];
	double			color[3];
} LWVolumeSample;

/*
 * Volumetric ray access structure is passed to the volume rendering
 * server to add its contribution to a ray passing through space.  The
 * ray is given by a void pointer.
 *
 * flags	evaluation falgs.  Indicates whether color or opacity or
 *		both should be computed.
 *
 * source	origin of ray.  Can be a light, the camera, or an object
 *		(for surface rendering).
 *
 * o,dir	origin and direction of ray.
 *
 * rayColor		color that is viewed from the origin of the ray, before
 *		volumetric effects are applied.
 *
 * far,near	far and near clipping distances.
 *
 * oDist	distance from origin (>0 when raytracing reflections /
 *		refractions).
 *
 * frustum	pixel frustum.
 *
 * addSample	add a new volume sample to the ray.
 *
 * getOpacity	returns opacity (vector and scalar) at specified distance.
 *
 * sx, sy	the position of the pixel for which the volumetric is being rendered.
 *
 * bounces	the current number of ray recursion bounces.
 */
typedef	struct	st_LWVolumeAccess
{
	void			 *ray;
	int				  flags;
	LWItemID		  source;

	double			  o[3], dir[3], rayColor[3];
	double			  farClip,nearClip;
	double			  oDist, frustum;

	void			(*addSample)  (void *ray, LWVolumeSample *smp);
	double			(*getOpacity) (void *ray, double dist, double opa[3]);

	LWIlluminateFunc *illuminate;
	LWRayTraceFunc	 *rayTrace;
	LWRayCastFunc	 *rayCast;
	LWRayShadeFunc	 *rayShade;

	double			sx, sy;
	int				bounces;

	LWRayTraceModeFunc     *rayTraceMode;
	LWIlluminateSampleFunc *illuminateSample;
    RandomFloatData        randomData;
    LWRandomFloatFunc      *randomFloat;
    LWIlluminateNormalFunc *illuminateNormal;
    LWIlluminateSampleNormalFunc *illuminateSampleNormal;

    LWRayTraceDataFunc      *rayTraceData;
    LWRayTraceShadeFunc     *rayTraceShade;
    LWRenderData            render;

    LWBufferGetValFunc      *getVal;
    LWBufferSetValFunc      *setVal;

	LWSamplerState          shaderSamplerState;
    LWGetSamplerRangeFunc   *getShaderSamplerRange;
    LWGetSample2DFunc       *getShaderSample2D;
    LWGetSample1DFunc       *getShaderSample1D;
} LWVolumeAccess;


#define	LWVEF_OPACITY	 (1<<0)	// light attenuation is evaluated (shadows)
#define	LWVEF_COLOR		 (1<<1)	// light scattering is evaluated (shading)
#define	LWVEF_RAYTRACE	 (1<<2)	// raytracing context
#define LWVEF_CAMERA     (1<<3)  // the ray originates from the camera
#define LWVEF_RADIOSITY  (1<<4)  // the ray originates from radiosity
#define LWVEF_SAMPLEDRAY (1<<5)  // this flag is on if the incoming ray was cast for sampling soft reflections, etc.
#define LWVEF_PREPROCESS  (1<<6)  // this flag is on if the incoming ray is a radiosity preprocess ray
#define LWVEF_REFLECTION  (1<<7)  // this flag is on if the incoming ray is a reflection ray

typedef struct st_LWVolumetricHandler
{
	LWInstanceFuncs	 *inst;
	LWItemFuncs		 *item;
	LWRenderFuncs	 *rend;
	double          (*evaluate) (LWInstance, LWVolumeAccess *);
	unsigned int    (*flags)    (LWInstance);
} LWVolumetricHandler;

#define LWVOLF_SHADOWS		(1<<0)	// element will be evaluated for shadows
#define LWVOLF_REFLECTIONS	(1<<1)	// element will be evaluated for raytraced reflections
#define	LWVOLF_REFRACTIONS	(1<<2)	// element will be evaluated for raytraced refractions

#define LWVRAYFUNCS_GLOBAL	"VRayFuncs 5"

typedef struct st_LWVRayFuncs
{
	/// Create a new volume access structure, but does not initialize it.
	/// However, the member values are not garbage; so it is possible to 'Destroy' it
	/// without having 'Init'ialized it.
	LWVolumeAccess * (*New)( void);

	/// Destroys an existing volume access structure
	LWError (*Destroy)( LWVolumeAccess *ray);

	/// Initializes the ray
	void (*Init)(
		LWVolumeAccess	*ray, /// ray to initialize
		double o[3], /// ray source position (world coordinates)
		double dir[3], /// ray direction (world coordinates)
		double nearClip, /// near clipping plane distance
		double farClip, /// far clipping plane distance
		int evalFlag, /// evaluation flags
		int hres, /// boolean (1 = use hfov; 0 = assume square)
		double hfov, /// horizontal field of view
		double incMultiplier, /// frustrum multiplier (normally 1.0)
		LWIlluminateFunc *illuminate); /// Illumination callback

	/// Sets the render functions to be called when this ray is evaluated.
	void (*SetRenderFuncs)(
		LWVolumeAccess *ray, /// ray to be adjusted
		LWIlluminateFunc *illuminate, /// illumination callback
		LWRayTraceFunc *rayTrace, /// raytrace callback
		LWRayCastFunc *rayCast, /// raycast callback
		LWRayShadeFunc *rayShade, /// rayshade callback
		LWRayTraceModeFunc *rayTraceMode, /// raytracemode callback
		LWIlluminateSampleFunc *illuminateSample, /// Illumination sample callback
        LWRandomFloatFunc *randomFloat, /// RandomFloat function callback
        LWIlluminateNormalFunc *illuminateNormal, /// normal testing illumination callback
        LWIlluminateSampleNormalFunc *illuminateSampleNormal,  /// normal testing illumination sample callback
        LWRayTraceDataFunc           *rayTraceData,  /// raytrace data function.
        LWRayTraceShadeFunc          *rayTraceShade, /// raytrace shade function relies on the raytrace data.
        LWGetSamplerRangeFunc        *getShaderSamplerRange, /// sampler functions for evaluating shaders from within a volumetric plugin.
        LWGetSample2DFunc            *getShaderSample2D,
        LWGetSample1DFunc            *getShaderSample1D );

	/// Flattening a ray simpling combines all the volume samples contained and resolves
	/// them to a single color and opacity
	double (*Flatten)(
		LWVolumeAccess *ray, /// ray to be flattened
		double color[3], /// resulting rgb color
		double opa[3]); /// resulting opacity for each color channel.
} LWVRayFuncs;


#define LWVOLUMETRICEVALUATIONFUNCS_GLOBAL	"VolumetricEvaluationFuncs"

/**
In order to get a volumetric plugin to evaluate outside of a render context:

VolumetricEvaluationFuncs->init() to prepare the volumetric handlers to be used for rendering
	This will call each handler's "init" callback

VolumetricEvaluationFuncs->newtime() to obtain flags from the volumetric handlers and invoke a NEWTIME
notification to get them up to date with the current scrub time.
	This will call each handler's "newtime" callback

Create a ray structure using VRayFuncs->New()

For each ray to be evaluated for whatever purposes:
	VRayFuncs->Init() the ray with custom parameters
	VolumetricEvaluationFuncs->evaluate() the ray to figure each volumetric handler's contribution to the samples
	VRayFuncs->Flatten() the ray to obtain colors and color opacities for that ray

VRayFuncs->Destroy() the ray when done with it.

VolumetricEvaluationFuncs->cleanUp() to notify a render finishing to each volumetric handler.
	This will call each handler's "cleanup" callback
 */
typedef struct st_LWVolumetricEvaluationFuncs
{
	/// Prepare the volumetric handlers for being evaluated.
	/// pass slot of 0 to process all volumetric servers, otherwise pass specific 1-based slot index of desired server
	void (*init)( unsigned int slot, int LWINIT_context);

	/// Syncronize the volumetric handlers with current Layout time.
	/// pass slot of 0 to process all volumetric servers, otherwise pass specific 1-based slot index of desired server
	void (*newtime)( unsigned int slot);

	/// send the ray through to each volumetric handler for evaluation
	/// pass slot of 0 to process all volumetric servers, otherwise pass specific 1-based slot index of desired server
	double (*evaluate)( unsigned int slot, LWVolumeAccess *ray, unsigned short LWVOLF_raytype);

	/// Cleanup the volumetric handlers when done being evaluated.
	/// pass slot of 0 to process all volumetric servers, otherwise pass specific 1-based slot index of desired server
	void (*cleanup)( unsigned int slot);

} LWVolumetricEvaluationFuncs;

#endif
