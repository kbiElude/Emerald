/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_MESHES_H
#define PLUGIN_MESHES_H

#include "scene/scene_types.h"
#include "system/system_types.h"

void FillSceneWithMeshData(__in __notnull scene            scene,
                           __in __notnull system_hash64map envelope_id_to_curve_container_map);

#endif /* PLUGIN_MESHES_H */