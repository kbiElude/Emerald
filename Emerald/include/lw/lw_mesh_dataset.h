/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_MESH_DATASET_H
#define LW_MESH_DATASET_H

#include "scene/scene_types.h"


DECLARE_HANDLE(lw_mesh_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_mesh_dataset, lw_mesh_dataset);

typedef enum
{

} lw_mesh_dataset_property;

/** TODO */
PUBLIC EMERALD_API void lw_mesh_dataset_apply_to_scene(__in __notnull lw_mesh_dataset dataset,
                                                       __in           scene           scene);

/** TODO */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void lw_mesh_dataset_get_property(__in  __notnull lw_mesh_dataset          dataset,
                                                     __in            lw_mesh_dataset_property property,
                                                     __out __notnull void*                    out_result);

/** TODO */
PUBLIC EMERALD_API lw_mesh_dataset lw_mesh_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                        __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_mesh_dataset_save(__in __notnull lw_mesh_dataset        dataset,
                                             __in __notnull system_file_serializer serializer);

#endif /* LW_MESH_DATASET_H */
