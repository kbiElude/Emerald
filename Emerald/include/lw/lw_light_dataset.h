/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_LIGHT_DATASET_H
#define LW_LIGHT_DATASET_H

#include "curve/curve_types.h"
#include "scene/scene_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(lw_light_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_light_dataset, lw_light_dataset);


typedef enum
{
    LW_LIGHT_DATASET_PROPERTY_AMBIENT_COLOR, /* settable, float[3] */
} lw_light_dataset_property;


/** TODO */
PUBLIC EMERALD_API void lw_light_dataset_apply_to_scene(__in __notnull lw_light_dataset dataset,
                                                        __in           scene            scene);

/** TODO */
PUBLIC EMERALD_API lw_light_dataset lw_light_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void lw_light_dataset_get_property(__in  __notnull lw_light_dataset          dataset,
                                                      __in            lw_light_dataset_property property,
                                                      __out __notnull void*                     out_result);

/** TODO */
PUBLIC EMERALD_API lw_light_dataset lw_light_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_light_dataset_save(__in __notnull lw_light_dataset       dataset,
                                              __in __notnull system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void lw_light_dataset_set_property(__in __notnull lw_light_dataset          dataset,
                                                      __in           lw_light_dataset_property property,
                                                      __in __notnull void*                     data);

#endif /* LW_LIGHT_DATASET_H */
