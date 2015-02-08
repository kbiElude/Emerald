/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_COMMON_H
#define PLUGIN_COMMON_H

#include "plugin.h"
#include "curve/curve_types.h"
#include "system/system_types.h"
#include "plugin_curves.h"

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
    ITEM_PROPERTY_LIGHT_CONE_ANGLE,
    ITEM_PROPERTY_LIGHT_EDGE_ANGLE,
    ITEM_PROPERTY_LIGHT_RANGE,

    ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_R,
    ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_G,
    ITEM_PROPERTY_SURFACE_AMBIENT_COLOR_B,
    ITEM_PROPERTY_SURFACE_GLOSINESS,
    ITEM_PROPERTY_SURFACE_LUMINOSITY,
    ITEM_PROPERTY_SURFACE_REFLECTION_RATIO,
    ITEM_PROPERTY_SURFACE_SPECULAR_RATIO,

    ITEM_PROPERTY_F_STOP,
    ITEM_PROPERTY_FOCAL_DISTANCE,
    ITEM_PROPERTY_ZOOM_FACTOR,

    ITEM_PROPERTY_TRANSLATION_X,
    ITEM_PROPERTY_TRANSLATION_Y,
    ITEM_PROPERTY_TRANSLATION_Z,

    ITEM_PROPERTY_ROTATION_B,
    ITEM_PROPERTY_ROTATION_H,
    ITEM_PROPERTY_ROTATION_P,

    ITEM_PROPERTY_UNKNOWN
} _item_property;

/** TODO */
PUBLIC void AdjustCurveByDelta(__in __notnull curve_container curve,
                               __in           float           delta);

/** TODO */
PUBLIC LWEnvelopeID FindEnvelope(__in           LWChanGroupID  group_id,
                                 __in __notnull const char*    envelope_name);

/** TODO */
PUBLIC LWChanGroupID FindGroup(__in __notnull const char* group_name);

/** TODO */
PUBLIC void GetCurveContainerForProperty(__in                system_hashed_ansi_string object_name,
                                         __in                _item_property            property,
                                         __in      __notnull LWItemID                  item_id,
                                         __out_opt           curve_container*          out_curve_ptr,
                                         __out_opt           curve_id*                 out_curve_id_ptr);

#endif /* PLUGIN_LIGHTS_H */