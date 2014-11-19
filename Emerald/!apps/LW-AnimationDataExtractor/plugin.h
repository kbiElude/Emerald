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

#include "shared.h"
#include "lw/lw_dataset.h"
#include "lw/lw_curve_dataset.h"
#include "lw/lw_material_dataset.h"

extern LWChannelInfo*   channel_info_ptr;
extern LWEnvelopeFuncs* envelope_ptr;
extern LWItemInfo*      item_info_ptr;
extern LWMessageFuncs*  message_funcs_ptr;
extern LWSceneInfo*     scene_info_ptr;
extern LWSurfaceFuncs*  surface_funcs_ptr;

#endif /* PLUGIN_H */