/**
 *
 * Emerald (kbi/elude @2014)
 */
#ifndef LW_CURVE_DATASET_H
#define LW_CURVE_DATASET_H

#include "curve/curve_types.h"
#include "scene/scene_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(lw_curve_dataset);

REFCOUNT_INSERT_DECLARATIONS(lw_curve_dataset, lw_curve_dataset);


typedef enum
{
    /* In order to maintain backward compatibility, always append new types */
    LW_CURVE_DATASET_OBJECT_TYPE_UNDEFINED,

    LW_CURVE_DATASET_OBJECT_TYPE_CAMERA,
    LW_CURVE_DATASET_OBJECT_TYPE_LIGHT,
    LW_CURVE_DATASET_OBJECT_TYPE_MESH
} lw_curve_dataset_object_type;

typedef enum
{
    LW_CURVE_DATASET_PROPERTY_FPS, /* settable, double */
} lw_curve_dataset_property;

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_add_curve(__in __notnull lw_curve_dataset             dataset,
                                                   __in __notnull system_hashed_ansi_string    object_name,
                                                   __in           lw_curve_dataset_object_type object_type,
                                                   __in __notnull system_hashed_ansi_string    curve_name,
                                                   __in __notnull curve_container              curve);

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_apply_to_scene(__in __notnull lw_curve_dataset dataset,
                                                        __in           scene            scene);

/** TODO */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_create(__in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool lw_curve_dataset_get_curve_by_curve_name(__in  __notnull lw_curve_dataset          dataset,
                                                                 __in  __notnull system_hashed_ansi_string curve_name,
                                                                 __out __notnull curve_container*          out_result);

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_get_property(__in  __notnull lw_curve_dataset          dataset,
                                                      __in            lw_curve_dataset_property property,
                                                      __out __notnull void*                     out_result);

/** TODO */
PUBLIC EMERALD_API lw_curve_dataset lw_curve_dataset_load(__in __notnull system_hashed_ansi_string name,
                                                          __in __notnull system_file_serializer    serializer);

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_save(__in __notnull lw_curve_dataset       dataset,
                                              __in __notnull system_file_serializer serializer);

/** TODO */
PUBLIC EMERALD_API void lw_curve_dataset_set_property(__in __notnull lw_curve_dataset          dataset,
                                                      __in           lw_curve_dataset_property property,
                                                      __in __notnull void*                     data);

#endif /* LW_CURVE_DATASET_H */
