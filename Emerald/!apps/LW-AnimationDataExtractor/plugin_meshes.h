/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_MESHES_H
#define PLUGIN_MESHES_H

#include "scene/scene_types.h"
#include "system/system_types.h"

/** TODO */
void DeinitMeshData();

/** TODO */
void FillSceneWithMeshData(__in __notnull scene            scene,
                           __in __notnull system_hash64map envelope_id_to_curve_container_map);

/** TODO */
void InitMeshData();

#endif /* PLUGIN_MESHES_H */