/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef PLUGIN_MESHES_H
#define PLUGIN_MESHES_H

#include "scene/scene_types.h"
#include "system/system_types.h"

typedef enum
{
    MESH_PROPERTY_OBJECT_ID,              /* void*       */
    MESH_PROPERTY_PARENT_OBJECT_ID,       /* void*       */
    MESH_PROPERTY_PARENT_SCENE_MESH,      /* scene_mesh  */
    MESH_PROPERTY_PIVOT,                  /* float   [3] */
    MESH_PROPERTY_ROTATION_HPB_CURVE_IDS, /* curve_id[3] */
    MESH_PROPERTY_TRANSLATION_CURVE_IDS,  /* curve_id[3] */
} MeshProperty;

/** TODO */
PUBLIC void DeinitMeshData();

/** TODO */
PUBLIC void FillSceneWithMeshData(scene scene);

/** TODO */
PUBLIC void GetMeshProperty(scene_mesh   mesh_instance,
                            MeshProperty property,
                            void*        out_result);

/** TODO */
PUBLIC void InitMeshData();

#endif /* PLUGIN_MESHES_H */