/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_MATERIAL_DATASET_H
#define LW_MATERIAL_DATASET_H

#include "scene/scene_types.h"


DECLARE_HANDLE(lw_material_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_material_dataset, lw_material_dataset);

/* Material ID handle */
typedef unsigned int lw_material_dataset_material_id;

typedef enum
{
    LW_MATERIAL_DATASET_MATERIAL_PROPERTY_NAME,            /* not settable, system_hashed_ansi_string */
    LW_MATERIAL_DATASET_MATERIAL_PROPERTY_SMOOTHING_ANGLE, /* settable, float */

} lw_material_dataset_material_property;

/** TODO */
PUBLIC EMERALD_API lw_material_dataset_material_id lw_material_dataset_add_material(__in __notnull lw_material_dataset       dataset,
                                                                                    __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void lw_material_dataset_apply_to_scene(__in __notnull lw_material_dataset dataset,
                                                           __in           scene               scene);

/** TODO */
PUBLIC EMERALD_API lw_material_dataset lw_material_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool lw_material_dataset_get_material_by_name(__in  __notnull lw_material_dataset              dataset,
                                                                 __in  __notnull system_hashed_ansi_string        name,
                                                                 __out __notnull lw_material_dataset_material_id* out_result_id);

/** TODO */
PUBLIC EMERALD_API void lw_material_dataset_get_material_property(__in  __notnull lw_material_dataset                   dataset,
                                                                  __in            lw_material_dataset_material_id       material_id,
                                                                  __in  __notnull lw_material_dataset_material_property property,
                                                                  __out __notnull void*                                 out_result);

/** TODO */
PUBLIC EMERALD_API lw_material_dataset lw_material_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                                __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_material_dataset_save(__in __notnull lw_material_dataset    dataset,
                                                 __in __notnull system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void lw_material_dataset_set_material_property(__in __notnull lw_material_dataset                   dataset,
                                                                  __in           lw_material_dataset_material_id       material_id,
                                                                  __in           lw_material_dataset_material_property property,
                                                                  __in __notnull const void*                           data);

#endif /* LW_MATERIAL_DATASET_H */
