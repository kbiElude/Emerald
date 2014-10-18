#define _MSWIN

#include <lwserver.h>  /* all plug-ins need this        */
#include <lwgeneric.h> /* for the LayoutGeneric class   */
#include <lwenvel.h>   /* Animation envelopes           */
#include <lwrender.h>  /* Item info                     */
#include <lwhost.h>    /* for the LWMessageFuncs global */
#include <stdio.h>     /* for NULL #define              */

#include "system/system_resizable_vector.h"

/* Forward declarations */
typedef struct _object;

/* Type declarations */

struct _object
{
    const char* name;
};


LWChannelInfo*   channel_info_ptr = NULL;
LWEnvelopeFuncs* envelope_ptr     = NULL;
LWItemInfo*      item_info_ptr    = NULL;

XCALL_(int) ExportAnimationData(int              version,
                                GlobalFunc*      global,
                                LWLayoutGeneric* local,
                                void*            serverData)
{
    struct _item
    {
        LWItemType             lw_type;
        _lw_plugin_object_type plugin_type;
    } items[] =
    {
        {LWI_OBJECT, LW_PLUGIN_OBJECT_TYPE_OBJECT},
        {LWI_LIGHT,  LW_PLUGIN_OBJECT_TYPE_LIGHT},
        {LWI_CAMERA, LW_PLUGIN_OBJECT_TYPE_CAMERA}
    };

          unsigned int n_item  = 0;
    const unsigned int n_items = sizeof(items) / sizeof(items[0]);

    if (version != LWLAYOUTGENERIC_VERSION)
    {
       return AFUNC_BADVERSION;
    }

    /* Retrieve func ptr structures */
    channel_info_ptr = (LWChannelInfo*)   global(LWCHANNELINFO_GLOBAL,   GFUSE_TRANSIENT);
    envelope_ptr     = (LWEnvelopeFuncs*) global(LWENVELOPEFUNCS_GLOBAL, GFUSE_TRANSIENT);
    item_info_ptr    = (LWItemInfo*)      global(LWITEMINFO_GLOBAL,      GFUSE_TRANSIENT);

    /* Iterate over all object types .. */
    for (  n_item = 0;
           n_item < n_items;
         ++n_item)
    {
        LWItemID   current_lw_item_id = 0;
        LWItemType current_lw_type    = items[n_item].lw_type;

        current_lw_item_id = item_info_ptr->first(current_lw_type, LWITEM_NULL);

        while (current_lw_item_id != LWITEM_NULL)
        {
            LWChanGroupID current_lw_channel_group      = item_info_ptr->chanGroup   (current_lw_item_id);
            const char*   current_lw_channel_group_name = channel_info_ptr->groupName(current_lw_channel_group);

            /* Iterate over all channels defined in the channel group */
            LWChannelID current_lw_channel = NULL;

            while ( (current_lw_channel = channel_info_ptr->nextChannel(current_lw_channel_group,
                                                                        current_lw_channel)) != NULL)
            {
                const char*        current_lw_channel_name     = channel_info_ptr->channelName(current_lw_channel);
                const LWEnvelopeID current_lw_channel_envelope = channel_info_ptr->channelEnvelope(current_lw_channel);

                int a = 1; a++;
            } /* while (channels exist) */

            /* Iterate to the next item of the current type.*/
            current_lw_item_id = item_info_ptr->next(current_lw_item_id);
        }
    } /* for (all items) */

    /* done! */
    return AFUNC_OK;
}


/*
======================================================================
This is the server description.  LightWave looks at this first to
determine what plug-ins the file contains.  It lists each plug-in's
class and internal name, along with a pointer to the activation
function.  You can optionally add a user name, or more than one in
different languages, if you like.
====================================================================== */
ServerRecord ServerDesc[] =
{
   {LWLAYOUTGENERIC_CLASS, "Emerald: Export Animation Data", ExportAnimationData},
   {NULL}
};
