/**
 *
 * Emerald (kbi/elude @2013-2015)
 *
 */
#ifndef SCENE_CURVE_H
#define SCENE_CURVE_H

#include "scene/scene_types.h"

typedef enum
{
    SCENE_CURVE_PROPERTY_ID,      /* scene_curve_id */
    SCENE_CURVE_PROPERTY_INSTANCE /* curve_container - reference counter NOT retained with a get() request! */
} scene_curve_property;

REFCOUNT_INSERT_DECLARATIONS(scene_curve, scene_curve)

/** TODO */
PUBLIC EMERALD_API scene_curve scene_curve_create(__in __notnull system_hashed_ansi_string name,
                                                  __in           scene_curve_id            id,
                                                  __in __notnull curve_container           instance);

/** TODO */
PUBLIC EMERALD_API void scene_curve_get(__in  __notnull scene_curve          curve,
                                        __in            scene_curve_property property,
                                        __out __notnull void*                out_result);

/** TODO */
PUBLIC EMERALD_API scene_curve scene_curve_load(__in     __notnull system_file_serializer    serializer,
                                                __in_opt           system_hashed_ansi_string object_manager_path);

/** TODO */
PUBLIC EMERALD_API bool scene_curve_save(__in __notnull system_file_serializer serializer,
                                         __in __notnull scene_curve            curve);


#endif /* SCENE_CURVE_H */
