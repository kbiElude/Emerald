/*
 * LWSDK Header File
 * Copyright 1999, NewTek, Inc.
 *
 * LWIMAGE.H -- LightWave Images
 */
#ifndef LWSDK_IMAGE_H
#define LWSDK_IMAGE_H

#include <lwtypes.h>
#include <lwimageio.h>
#include <lwio.h>


#define LWIMAGELIST_GLOBAL	"LW Image List 6"
#define LWIMAGEUTIL_GLOBAL	"Image Utility 3"

// change-event codes

enum {
        LWCEV_CHANGE = 0,
        LWCEV_REPLACE,
        LWCEV_DESTROY
};

// change-event callback prototype

typedef void (*LWImageEventFunc)(int eventCode, LWImageID );

// event regitration codes (second argument to changeEvent())

enum {
        LWERC_REGISTER = 0,
        LWERC_UNREGISTER
};

/* When surface plugins save an image in an LWO, it is necessare to call clipSetMark to tell the LWO saver which clips are being saved. */

typedef LWImageID (*LWImageSaverNotify)( LWInstance );  /* Prototype for the save notify callback. */

typedef struct st_LWImageList {
    LWImageID       (*first      )( void );
    LWImageID       (*next       )( LWImageID img );
    LWImageID       (*load       )( const char *filename);
    const char *    (*name       )( LWImageID img);
    const char *    (*filename   )( LWImageID img, LWFrame frame);
    int             (*isColor    )( LWImageID img );
    void            (*needAA     )( LWImageID img );
    void            (*size       )( LWImageID img, int *w, int *h );
    LWBufferValue   (*luma       )( LWImageID img, int x, int y );
    void            (*RGB        )( LWImageID img, int x, int y, LWBufferValue values[3] );
    double          (*lumaSpot   )( LWImageID img, double x, double y, double spotSize, int in_blend);
    void            (*RGBSpot    )( LWImageID img, double x, double y, double spotSize, int in_blend, double rgb[3]);
    void            (*clear      )( LWImageID img );
    LWImageID       (*sceneLoad  )( const LWLoadState *load );
    void            (*sceneSave  )( const LWSaveState *save, LWImageID img );
    int             (*hasAlpha   )( LWImageID img);
    LWBufferValue   (*alpha      )( LWImageID img, int x, int y );
    double          (*alphaSpot  )( LWImageID img, double x, double y, double spotSize, int in_blend);
    LWPixmapID      (*evaluate   )( LWImageID img, LWTime t);
    void            (*changeEvent)( LWImageEventFunc func, int code);
    int             (*replace    )( LWImageID img, const char *filename );
    LWPixmapID      (*create     )( const char* name, int width, int height, LWImageType type );
    void            (*saverNotifyAttach)( LWInstance, LWImageSaverNotify );
    void            (*saverNotifyDetach)( LWInstance );
    void            (*saverNotifyMarkUsage)( LWTextureID );
} LWImageList;

typedef struct st_LWImageUtil
{
    LWPixmapID    (*create     )( int w, int h, LWImageType type);
    void          (*destroy    )( LWPixmapID img);
    int           (*save       )( LWPixmapID img, int saver, const char *name );
    void          (*setPixel   )( LWPixmapID img, int x, int y,  void *pix );
    void          (*getPixel   )( LWPixmapID img, int x, int y,  void *pix );
    void          (*getInfo    )( LWPixmapID img, int *w, int *h, int *type );
    LWPixmapID    (*resample   )( LWPixmapID img, int w, int h, int mode );
    int           (*saverCount )( void);
    const char *  (*saverName  )( int saver);
    void          (*setPixelTyped )( LWPixmapID img, int x, int y, int type, void *pix );
    void          (*getPixelTyped )( LWPixmapID img, int x, int y, int type, void *pix );
    int           (*getIndex8Map  )( LWPixmapID img, LWPixelRGB24 *map );
    int           (*getAttr       )( LWPixmapID img, LWImageParam tag, void* data );
    int           (*getMakerNote  )( LWPixmapID img, LWMakerNote tag, void* data );
} LWImageUtil;

/* resample modes */
enum {
        LWISM_SUBSAMPLING = 0,
        LWISM_MEDIAN,		/* for shrinking */
        LWISM_SUPERSAMPLING,
        LWISM_BILINEAR,		/* for expanding */
        LWISM_BSPLINE,		/* for expanding */
        LWISM_BICUBIC		/* for expanding */
};

#endif

