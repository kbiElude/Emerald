/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_DATASET_H
#define LW_DATASET_H

#include "scene/scene_types.h"


DECLARE_HANDLE(lw_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_dataset, lw_dataset);

typedef enum
{
    LW_DATASET_PROPERTY_CURVE_DATASET,    /* not settable, lw_curve_dataset    */
    LW_DATASET_PROPERTY_MATERIAL_DATASET, /* not settable, lw_material_dataset */
    LW_DATASET_PROPERTY_MESH_DATASET,     /* not settable, lw_mesh_dataset     */
} lw_dataset_property;

/** TODO */
PUBLIC EMERALD_API void lw_dataset_apply_to_scene(__in __notnull lw_dataset dataset,
                                                  __in           scene      scene);

/** TODO */
PUBLIC EMERALD_API lw_dataset lw_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void lw_dataset_get_property(__in  __notnull lw_dataset          dataset,
                                                __in            lw_dataset_property property,
                                                __out __notnull void*               out_result);

/** TODO */
PUBLIC EMERALD_API lw_dataset lw_dataset_load(__in __notnull system_hashed_ansi_string name,
                                              __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_dataset_save(__in __notnull lw_dataset             dataset,
                                        __in __notnull system_file_serializer serializer);

#endif /* LW_DATASET_H */
