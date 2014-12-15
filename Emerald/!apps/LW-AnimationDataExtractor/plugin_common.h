/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

#include "plugin.h"

typedef enum
{
    ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_R,
    ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_G,
    ITEM_PROPERTY_LIGHT_AMBIENT_COLOR_B,
    ITEM_PROPERTY_LIGHT_AMBIENT_INTENSITY,
    ITEM_PROPERTY_LIGHT_COLOR_R,
    ITEM_PROPERTY_LIGHT_COLOR_G,
    ITEM_PROPERTY_LIGHT_COLOR_B,
    ITEM_PROPERTY_LIGHT_COLOR_INTENSITY,

    ITEM_PROPERTY_F_STOP,
    ITEM_PROPERTY_FOCAL_DISTANCE,

    ITEM_PROPERTY_TRANSLATION_X,
    ITEM_PROPERTY_TRANSLATION_Y,
    ITEM_PROPERTY_TRANSLATION_Z,

    ITEM_PROPERTY_ROTATION_B,
    ITEM_PROPERTY_ROTATION_H,
    ITEM_PROPERTY_ROTATION_P,
} _item_property;

/** TODO */
PUBLIC LWEnvelopeID FindEnvelope(__in           LWChanGroupID  group_id,
                                 __in __notnull const char*    envelope_name);

/** TODO */
PUBLIC LWChanGroupID FindGroup(__in __notnull const char* group_name);

/** TODO */
PUBLIC curve_container GetCurveContainerForProperty(__in           system_hashed_ansi_string object_name,
                                                    __in           _item_property            property,
                                                    __in __notnull LWItemID                  item_id,
                                                    __in __notnull system_hash64map          envelope_id_to_curve_container_map);

#endif /* PLUGIN_LIGHTS_H */