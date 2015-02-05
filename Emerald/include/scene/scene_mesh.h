/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Represents a single mesh instance.
 */
#ifndef SCENE_MESH_H
#define SCENE_MESH_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_mesh, scene_mesh)

enum scene_mesh_property
{
    SCENE_MESH_PROPERTY_ID,                 /*     settable, uint32_t                  */
    SCENE_MESH_PROPERTY_IS_SHADOW_CASTER,   /*     settable, bool                      */
    SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER, /*     settable, bool                      */
    SCENE_MESH_PROPERTY_MESH,               /* not settable, mesh                      */
    SCENE_MESH_PROPERTY_NAME,               /* not settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_mesh_create(__in __notnull system_hashed_ansi_string name,
                                                __in           mesh                      geometry);

/** TODO */
PUBLIC EMERALD_API void scene_mesh_get_property(__in  __notnull scene_mesh,
                                                __in            scene_mesh_property,
                                                __out __notnull void*);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_mesh scene_mesh_load(__in __notnull system_file_serializer serializer,
                                  __in __notnull system_hash64map       id_to_gpu_mesh_map);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *  NOTE: This function does NOT serialize owned meshes. They should be serialized separately.
 *
 **/
PUBLIC bool scene_mesh_save(__in __notnull system_file_serializer serializer,
                            __in __notnull const scene_mesh       mesh,
                            __in __notnull system_hash64map       gpu_mesh_to_id_map);

/** TODO */
PUBLIC EMERALD_API void scene_mesh_set_property(__in __notnull scene_mesh,
                                                __in           scene_mesh_property,
                                                __in __notnull const void*);

#endif /* SCENE_MESH_H */
