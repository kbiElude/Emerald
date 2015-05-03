/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWIMAGEIO.H -- LightWave Image Input/Ouput
 *
 * This header defines the structures required for basic image I/O.
 * This includes the different image matrix protocols and the local
 * structs for the image loader and image saver.
 */
#ifndef LWSDK_IMAGEIO_H
#define LWSDK_IMAGEIO_H

#include <lwmonitor.h>

#define LWIMAGELOADER_CLASS	"ImageLoader"
#define LWIMAGELOADER_VERSION	3

#define LWIMAGESAVER_CLASS	"ImageSaver"
#define LWIMAGESAVER_VERSION	3


/*
 * Image Pixel Datatypes.
 */
typedef enum en_LWImageType {
	LWIMTYP_RGB24 = 0,
	LWIMTYP_GREY8,
	LWIMTYP_INDEX8,
	LWIMTYP_GREYFP,
	LWIMTYP_RGBFP,
	LWIMTYP_RGBA32,
	LWIMTYP_RGBAFP,
	LWIMTYP_SPECIAL,
	LWIMTYP_GREYDBL,
	LWIMTYP_RGBDBL,
	LWIMTYP_RGBADBL
} LWImageType;

/*
 * Image Pixel Structures.
 */
typedef void *		LWPixelID;

typedef struct st_LWPixelRGB24 {
	unsigned char    r;
	unsigned char    g;
	unsigned char    b;
} LWPixelRGB24;

typedef struct st_LWPixelRGBFP {
	float            r;
	float            g;
	float            b;
} LWPixelRGBFP;

typedef struct st_LWPixelRGBDBL {
	double           r;
	double           g;
	double           b;
} LWPixelRGBDBL;

typedef struct st_LWPixelRGBA32 {
	unsigned char    r;
	unsigned char    g;
	unsigned char    b;
	unsigned char    a;
} LWPixelRGBA32;

typedef struct st_LWPixelRGBAFP {
	float            r;
	float            g;
	float            b;
	float            a;
} LWPixelRGBAFP;

typedef struct st_LWPixelRGBADBL {
	double           r;
	double           g;
	double           b;
    double           a;
} LWPixelRGBADBL;

/*
 * Image Buffer Protocol with parameter tags.
 */

typedef enum en_LWImageParam {
    LWIMPAR_ASPECT = 1,                 /* x / y Pixel Aspect. (float)                  */
    LWIMPAR_NUMCOLS,                    /* Number of colors in palatte. (int)           */
    LWIMPAR_PIXELWIDTH,                 /* Actual (scanned)Pixel Width in (mm). (float) */
    LWIMPAR_FRAMESPERSECOND,            /* Number Of Frames Per Second. (float)         */
    LWIMPAR_BLACKPOINT,                 /* Black Point Of Layer. (float)                */
    LWIMPAR_WHITEPOINT,                 /* White Point Of Layer. (float)                */
    LWIMPAR_GAMMA,                      /* Linearity Of RGB Color. (float)              */
    LWIMPAR_COLORSPACE,                 /* 1 = sRGB, 2 = Adobe RGB, 65535 = Uncalibrated. (int) */
    LWIMPAR_PIXELXDIMENSION,            /* Valid image width. (int)  */
    LWIMPAR_PIXELYDIMENSION,            /* Valid image height. (int) */
    LWIMPAR_EXPOSURETIME,               /* Exposure time (reciprocal of shutter speed). Unit is seconds. (float) */
    LWIMPAR_FNUMBER,                    /* The actual F-number(F-stop) of lens when the image was taken. (float) */
    LWIMPAR_EXPOSUREPROGRAM,            /* Exposure program that the camera used when image was taken.
                                          '1' means manual control,
                                          '2' program normal,
                                          '3' aperture priority,
                                          '4' shutter priority,
                                          '5' program creative (slow program),
                                          '6' program action(high-speed program),
                                          '7' portrait mode, '8' landscape mode. (int) */
    LWIMPAR_ISOSPEEDRATINGS,            /* CCD sensitivity equivalent to Ag-Hr film speedrate. (int) */
    LWIMPAR_EXIFVERSION,                /* Exif version number. Stored as 4bytes of ASCII character (like "0210"). (char *)                */
    LWIMPAR_DATETIMEORIGINAL,           /* Date/Time of original image taken. This value should not be modified by user program. (char *)  */
    LWIMPAR_DATETIMEDIGITIZED,          /* Date/Time of image digitized. Usually, it contains the same value of DateTimeOriginal. (char *) */
    LWIMPAR_EXPOSUREBIASVALUE,          /* Exposure bias value of taking picture. Unit is EV. (float) */
    LWIMPAR_MAXAPERTUREVALUE,           /* Maximum aperture value of lens. You can convert to F-number by calculating power of root 2 (same process of ApertureValue. (float)      */
    LWIMPAR_METERINGMODE,               /* Exposure metering method. '1' means average, '2' center weighted average, '3' spot, '4' multi-spot, '5' multi-segment. (int)            */
    LWIMPAR_LIGHTSOURCE,                /* Light source, actually this means white balance setting. '0' means auto, '1' daylight, '2' fluorescent, '3' tungsten, '10' flash. (int) */
    LWIMPAR_FLASH,                      /* '1' means flash was used, '0' means not used. (int)                  */
    LWIMPAR_FOCALLENGTH,                /* Focal length of lens used to take image. Unit is millimeter. (float) */
    LWIMPAR_MAKERNOTE,                  /* Maker dependent internal data. Some of maker such as Olympus/Nikon/Sanyo etc. uses IFD format for this area. (char *) */
    LWIMPAR_USERCOMMENT,                /* Stores user comment. (char *)   */
    LWIMPAR_FLASHPIXVERSION,            /* Stores FlashPix version. Unknown but 4bytes of ASCII characters "0100" exists. (char *) */
    LWIMPAR_FILESOURCE,                 /* Unknown but value is '3'. (int) */
    LWIMPAR_SCENETYPE,                  /* Unknown but value is '1'. (int) */
    LWIMPAR_COMPONENTSCONFIGURATION,    /* Unknown. It seems value 0x00,0x01,0x02,0x03 always. (char *) */
    LWIMPAR_COMPRESSEDBITSPERPIXEL,     /* The average compression ratio of JPEG. (float)               */
    LWIMPAR_SHUTTERSPEEDVALUE,          /* Shutter speed. To convert this value to ordinary 'Shutter Speed'; calculate this value's power of 2,
                                           then reciprocal. For example, if value is '4', shutter speed is 1/(2^4)=1/16 second. (float) */
    LWIMPAR_APERTUREVALUE,              /* The actual aperture value of lens when the image was taken. To convert this value to ordinary F-number(F-stop),
                                           calculate this value's power of root 2 (=1.4142). For example, if value is '5', F-number is 1.4142^5 = F5.6. (float) */
    LWIMPAR_SUBJECTDISTANCE,            /* Distance to focus point, unit is meter. (float) */
    LWIMPAR_FOCALPLANEXRESOLUTION,      /* CCD's pixel density. (float) */
    LWIMPAR_FOCALPLANEYRESOLUTION,
    LWIMPAR_FOCALPLANERESOLUTIONUNIT,   /* Unit of FocalPlaneXResoluton/FocalPlaneYResolution. '1' means no-unit, '2' inch, '3' centimeter. (int)        */
    LWIMPAR_SENSINGMETHOD,              /* Shows type of image sensor unit. '2' means 1 chip color area sensor, most of all digicam use this type. (int) */
    LWIMPAR_BRIGHTNESSVALUE,            /* Brightness of taken subject, unit is EV. (float) */
    LWIMPAR_RELATEDSOUNDFILE,           /* If this digicam can record audio data with image, shows name of audio data. (char *) */
    LWIMPAR_SPECTRALSENSITIVITY,        /* Spectral sensitivity. (char *)           */
    LWIMPAR_OECF,                       /* Optoelectric conversion factor. (char *) */
    LWIMPAR_SUBSECTIME,                 /* DateTime subseconds. (char *)            */
    LWIMPAR_SUBSECTIMEORIGINAL,         /* DateTimeOriginal subseconds. (char *)    */
    LWIMPAR_SUBSECTIMEDIGITIZED,        /* DateTimeDigitized subseconds. (char *)   */
    LWIMPAR_FLASHENERGY,                /* Flash energy. (float)                    */
    LWIMPAR_SPATIALFREQUENCYRESPONSE,   /* Spatial frequency response. (int)        */
    LWIMPAR_SUBJECTLOCATION,            /* Subject location. (int)                  */
    LWIMPAR_EXPOSUREINDEX,              /* Exposure index. (float)                  */
    LWIMPAR_CFAPATTERN,                 /* CFA pattern. (char *)                    */
    LWIMPAR_MAKE,                       /* Manufacturer of recording equipment. (char *)               */
    LWIMPAR_MODEL,                      /* Model name or model number of recording equipment. (char *) */
    LWIMPAR_SOFTWARE,                   /* Name and version of the software used. (char *)             */
    LWIMPAR_SUBJECTAREA,                /* Subject area. (int)                      */
    LWIMPAR_CUSTOMRENDERED,             /* Custom image processing. (int)           */
    LWIMPAR_EXPOSUREMODE,               /* Exposure mode. (int)                     */
    LWIMPAR_WHITEBALANCE,               /* White balance. (int)                     */
    LWIMPAR_DIGITALZOOMRATIO,           /* Digital zoom ratio. (float)              */
    LWIMPAR_FOCALLENGTHIN35MMFILM,      /* Focal length in 35 mm film. (int)        */
    LWIMPAR_SCENECAPTURETYPE,           /* Scene capture type. (int)                */
    LWIMPAR_GAINCONTROL,                /* Gain control. (int)                      */
    LWIMPAR_CONTRAST,                   /* Contrast. (int)                          */
    LWIMPAR_SATURATION,                 /* Saturation. (int)                        */
    LWIMPAR_SHARPNESS,                  /* Sharpness. (int)                         */
    LWIMPAR_SUBJECTDISTANCERANGE,       /* Subject distance range. (int)            */
    LWIMPAR_SIZEOF
} LWImageParam;

/*
 *      Lightwave Maker Note.
 */

#define NEWTEKMANUFACTURENAME "NewTek"

typedef enum en_LWMakerNote {
    LWMN_COMPANYNAME = 0xC000,          /* Company Name.         */
    LWMN_FRAMENUMBER,                   /* Current frame number. */
    LWMN_FIRSTFRAME,                    /* First frame number.   */
    LWMN_LASTFRAME,                     /* Last  frame number.   */
    LWMN_RENDERTIME,                    /* Render time.          */
    LWMN_CAMERANAME,                    /* Camera Name.          */
    LWMN_CAMERATYPE,                    /* Camera Type.          */
    LWMN_ANTIALIASING,                  /* Antialiasing.         */
    LWMN_LENS,                          /* Camera lens name.     */
    LWMN_PARTICLEBLUR,                  /* Particle blur On/Off. */
    LWMN_MOTIONBLURTYPE,                /* Motion blur type      */
    LWMN_MOTIONBLURAMOUNT,              /* Motion blur amount.   */
    LWMN_MOTIONBLURPASSES,              /* Motion blur passes.   */
    LWMN_TOTALPOINTS,                   /* Total points.         */
    LWMN_TOTALPOLYGONS,                 /* Total polygons.       */
    LWMN_TOTALMEMORY,                   /* Total memory.         */
    LWMN_FRAMESTEP,                     /* Frame step.           */
    LWMN_RECONSTRUCTIONFILTER,          /* Reconstruction filter.*/
    LWMN_FIELDRENDERING,                /* Field rendering.      */
    LWMN_RTREFLECTIONS,                 /* RT reflections.       */
    LWMN_ADAPTIVESAMPLING,              /* Adaptive sampling.    */
    LWMN_STEREORENDER,                  /* Stereo render.        */
    LWMN_RTREFRACTIONS,                 /* RT refractions.       */
    LWMN_RADIOSITY,                     /* Radiosity.            */
    LWMN_RTTRANSPARENCY,                /* RT transparency.      */
    LWMN_CAUSTICS,                      /* Caustics.             */
    LWMN_RENDERTHREADS,                 /* Render threads.       */
    LWNM_DEPTHOFFIELD,                  /* Depth of field.       */
    LWMN_OUTPUTFILES,                   /* Output files.         */
    LWMN_COLORSPACE,                    /* Color Space.          */
    LWMN_CAMERAEYE,                     /* Center / Left / Right / Anaglyph.*/
    LWMN_BUFFERID,                      /* Buffer ID.            */
    LWMN_SIZEOF
} LWMakerNote;

typedef struct st_LWImageProtocol {
    int       type;
    void     *priv_data;
    int     (*done    )( void *, int );
    void    (*setSize )( void *, int, int );
    int     (*setParam)( void *, LWImageParam, const char * );
    int     (*sendLine)( void *, int, const LWPixelID );
    void    (*setMap  )( void *, int, const unsigned char[ 3 ] );
} LWImageProtocol, *LWImageProtocolID;


/*
 * "ImageLoader" local struct.
 */

typedef struct st_LWImageLoaderLocal {
	void               *priv_data;
	int                 result;
	const char         *filename;
	LWMonitor          *monitor;
	LWImageProtocolID (*begin) (void *, LWImageType);
	void              (*done)  (void *, LWImageProtocolID);
} LWImageLoaderLocal;

/*
 * "ImageSaver" local struct.
 */

typedef struct st_LWImageSaverLocal {
	void               *priv_data;
	int                 result;
	LWImageType         type;
	const char         *filename;
	LWMonitor          *monitor;
	int                (*sendData) (void *, LWImageProtocolID, int flags);
} LWImageSaverLocal;


/*
        Result Value

        The result value indicates the status of the loader or saver upon 
        completion.  If the load or save was sucessful, the value should be 
        IPSTAT_OK.  If a loader fails to recognize a file as something it can load
        it should set the result to IPSTAT_NOREC.  If the server could not open
        the file it should return IPSTAT_BADFILE.  Any other error is just a
        generic failure of the loader or saver and so should set the result to
        IPSTAT_FAILED.  Other failure modes might be possible if required 
        in the future.
*/

#define IPSTAT_OK	 0
#define IPSTAT_NOREC	 1
#define IPSTAT_BADFILE	 2
#define IPSTAT_ABORT	 3
#define IPSTAT_FAILED	99

/* Flags to be passed to 'setSize' and 'sendData' callbacks. */

#define IMGF_REVERSE	(1<<0)

/* There are also some protocol macros defined
   to get the whole calling interface right. */

#define LWIP_SETSIZE(p,w,h)     (*(p)->setSize) ((p)->priv_data,w,h)
#define LWIP_SETPARAM(p,t,c)    (*(p)->setParam) ((p)->priv_data,t,c)
#define LWIP_ASPECT( p, a )     {                                                \
                                    char string [ 256 ];                         \
                                    sprintf( string, "%g", a );                  \
                                    LWIP_SETPARAM( p, LWIMPAR_ASPECT, string );  \
                                }
#define LWIP_NUMCOLORS( p, n )  {                                                \
                                    char string [ 256 ];                         \
                                    sprintf( string, "%d", n );                  \
                                    LWIP_SETPARAM( p, LWIMPAR_NUMCOLS, string ); \
                                }
#define LWIP_SENDLINE(p,ln,d)   (*(p)->sendLine) ((p)->priv_data,ln,d)
#define LWIP_SETMAP(p,i,val)    (*(p)->setMap) ((p)->priv_data,i,val)
#define LWIP_DONE(p,err)        (*(p)->done) ((p)->priv_data,err)

/*
        Compatibility macros.

        These types are obsolete, but are included to make it easier to convert 
        to the new image format.  The IMG_* value have been replaced with the
        LWImageType type codes, although the values of 0, 1 and 2 map to equivalent
        types.  The ImageValue type is gone completely, and a Pixel* structure
        type should be used, or an explicit reference to unsigned char.
 */

#define IMG_RGB24	 0
#define IMG_GREY8	 1
#define IMG_INDEX8	 2
typedef unsigned char   ImageValue;

#endif
