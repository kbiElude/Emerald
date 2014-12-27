/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_LIGHTS_H
#define PLUGIN_LIGHTS_H

#include "scene/scene_types.h"
#include "system/system_types.h"

typedef enum
{
    LIGHT_PROPERTY_OBJECT_ID,           /* void*              */
    LIGHT_PROPERTY_PARENT_OBJECT_ID,    /* void               */
    LIGHT_PROPERTY_ROTATION_HPB_CURVES, /* curve_container[3] */
    LIGHT_PROPERTY_TRANSLATION_CURVES,  /* curve_container[3] */
} LightProperty;


/** TODO */
PUBLIC void DeinitLightData();

/** TODO */
PUBLIC void FillSceneWithLightData(__in __notnull scene            in_scene);

/** TODO */
PUBLIC void GetLightPropertyValue(__in  __notnull scene_light   light,
                                  __in            LightProperty property,
                                  __out __notnull void*         out_result);

/** TODO */
PUBLIC void InitLightData();

#endif /* PLUGIN_LIGHTS_H */