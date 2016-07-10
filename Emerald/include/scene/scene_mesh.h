/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Represents a single mesh instance.
 */
#ifndef SCENE_MESH_H
#define SCENE_MESH_H

#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(scene_mesh,
                             scene_mesh)

enum scene_mesh_property
{
    /* settable, uint32_t */
    SCENE_MESH_PROPERTY_ID,

    /* settable, bool */
    SCENE_MESH_PROPERTY_IS_SHADOW_CASTER,

    /* settable, bool */
    SCENE_MESH_PROPERTY_IS_SHADOW_RECEIVER,

    /* not settable, mesh */
    SCENE_MESH_PROPERTY_MESH,

    /* not settable, system_hashed_ansi_string */
    SCENE_MESH_PROPERTY_NAME,
};

/** TODO */
PUBLIC EMERALD_API scene_mesh scene_mesh_create(system_hashed_ansi_string name,
                                                system_hashed_ansi_string object_manager_path,
                                                mesh                      geometry);

/** TODO */
PUBLIC EMERALD_API void scene_mesh_get_property(scene_mesh          mesh_instance,
                                                scene_mesh_property property,
                                                void*               out_result_ptr);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *
 */
PUBLIC scene_mesh scene_mesh_load(system_file_serializer    serializer,
                                  system_hashed_ansi_string object_manager_path,
                                  system_hash64map          id_to_gpu_mesh_map);

/** TODO.
 *
 *  NOTE: This function is for internal use only.
 *  NOTE: This function does NOT serialize owned meshes. They should be serialized separately.
 *
 **/
PUBLIC bool scene_mesh_save(system_file_serializer serializer,
                            const scene_mesh       mesh,
                            system_hash64map       gpu_mesh_to_id_map);

/** TODO */
PUBLIC EMERALD_API void scene_mesh_set_property(scene_mesh          mesh,
                                                scene_mesh_property property,
                                                const void*         data);

#endif /* SCENE_MESH_H */
