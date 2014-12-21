/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_LIGHTS_H
#define PLUGIN_LIGHTS_H

#include "scene/scene_types.h"
#include "system/system_types.h"

/** TODO */
void FillSceneWithLightData(__in __notnull scene            in_scene,
                            __in __notnull system_hash64map curve_id_to_curve_container_map);

#endif /* PLUGIN_LIGHTS_H */