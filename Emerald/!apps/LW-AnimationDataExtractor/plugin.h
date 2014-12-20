/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_H
#define PLUGIN_H

#include <lwserver.h>  /* all plug-ins need this        */
#include <lwgeneric.h> /* for the LayoutGeneric class   */
#include <lwenvel.h>   /* Animation envelopes           */
#include <lwrender.h>  /* Scene & Item info             */
#include <lwhost.h>    /* for the LWMessageFuncs global */
#include <lwsurf.h>    /* for LWSurfaceFuncs global     */
#include <lwcamera.h>  /* for LWCameraInfo global       */
#include <lwimage.h>   /* for LWImageList global        */
#include <lwlight.h>   /* for LWLightInfo global        */
#include <lwtexture.h> /* for LWTextureFuncs global     */

#include "shared.h"

extern LWCameraInfo*    camera_info_ptr;
extern LWChannelInfo*   channel_info_ptr;
extern LWEnvelopeFuncs* envelope_ptr;
extern LWImageList*     image_list_ptr;
extern LWItemInfo*      item_info_ptr;
extern LWLightInfo*     light_info_ptr;
extern LWMessageFuncs*  message_funcs_ptr;
extern LWObjectFuncs*   object_funcs_ptr;
extern LWObjectInfo*    object_info_ptr;
extern LWSceneInfo*     scene_info_ptr;
extern LWSurfaceFuncs*  surface_funcs_ptr;
extern LWTextureFuncs*  texture_funcs_ptr;

#endif /* PLUGIN_H */