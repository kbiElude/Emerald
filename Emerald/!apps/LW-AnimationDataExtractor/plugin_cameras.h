/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_CAMERAS_H
#define PLUGIN_CAMERAS_H

#include "scene/scene_types.h"


void FillCameraDataset(__in __notnull scene            in_scene,
                       __in __notnull system_hash64map curve_id_to_curve_container_map);

#endif /* PLUGIN_CAMERAS_H */