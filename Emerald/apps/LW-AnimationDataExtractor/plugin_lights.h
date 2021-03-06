/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef PLUGIN_LIGHTS_H
#define PLUGIN_LIGHTS_H

#include "scene/scene_types.h"
#include "system/system_types.h"

typedef enum
{
    LIGHT_PROPERTY_OBJECT_ID,              /* void*       */
    LIGHT_PROPERTY_PARENT_OBJECT_ID,       /* void        */
    LIGHT_PROPERTY_PIVOT,                  /* float   [3] */
    LIGHT_PROPERTY_ROTATION_HPB_CURVE_IDS, /* curve_id[3] */
    LIGHT_PROPERTY_TRANSLATION_CURVE_IDS,  /* curve_id[3] */
} LightProperty;


/** TODO */
PUBLIC void DeinitLightData();

/** TODO */
PUBLIC void GetLightPropertyValue(scene_light   light,
                                  LightProperty property,
                                  void*         out_result);

/** TODO */
PUBLIC void InitLightData();

/** TODO */
PUBLIC system_event StartLightDataExtraction(scene in_scene);


#endif /* PLUGIN_LIGHTS_H */