/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef COLLADA_DATA_H
#define COLLADA_DATA_H

#include "collada/collada_types.h"
#include "mesh/mesh_types.h"
#include "ral/ral_types.h"
#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(collada_data, collada_data)

enum collada_data_property
{
    COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
    COLLADA_DATA_PROPERTY_CAMERAS_BY_ID_MAP,        /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_FILE_NAME,
    COLLADA_DATA_PROPERTY_GEOMETRIES_BY_ID_MAP,     /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_IS_LW10_COLLADA_FILE,     /* not settable, bool. Tells whether the source COLLADA file came from
                                                     *                     LW 10. If so, a few work-arounds will be applied
                                                     *                     to the imported data structures. */
    COLLADA_DATA_PROPERTY_LIGHTS_BY_ID_MAP,         /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_MATERIALS_BY_ID_MAP,      /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_MAX_ANIMATION_DURATION,   /* not settable, float */
    COLLADA_DATA_PROPERTY_NODES_BY_ID_MAP,          /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_N_CAMERAS,
    COLLADA_DATA_PROPERTY_N_EFFECTS,
    COLLADA_DATA_PROPERTY_N_IMAGES,
    COLLADA_DATA_PROPERTY_N_MATERIALS,
    COLLADA_DATA_PROPERTY_N_MESHES,
    COLLADA_DATA_PROPERTY_N_SCENES,
    COLLADA_DATA_PROPERTY_OBJECT_TO_ANIMATION_VECTOR_MAP, /* not settable, system_hash64map */

    /* Always last */
    COLLADA_DATA_PROPERTY_COUNT
};

/** TODO */
PUBLIC EMERALD_API collada_data collada_data_load(system_hashed_ansi_string filename,
                                                  system_hashed_ansi_string name,
                                                  bool                      should_generate_cache_blobs = false);

/** TODO */
PUBLIC EMERALD_API bool collada_data_get_camera_by_index(collada_data         data,
                                                         uint32_t             n_camera,
                                                         collada_data_camera* out_camera);

/** TODO */
PUBLIC EMERALD_API collada_data_camera collada_data_get_camera_by_name(collada_data              data,
                                                                       system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_effect(collada_data         data,
                                                unsigned int         n_effect,
                                                collada_data_effect* out_effect);

/** TODO.
 *
 *  mesh = renderer+geometrical mesh representation
 **/
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh(collada_data data,
                                                      ral_context  context,
                                                      unsigned int n_mesh);

/** TODO */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh_by_name(collada_data              data,
                                                              ral_context               context,
                                                              system_hashed_ansi_string name);

/** TODO. */
PUBLIC EMERALD_API scene collada_data_get_emerald_scene(collada_data data,
                                                        ral_context  context,
                                                        unsigned int n_scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_geometry(collada_data           data,
                                                  unsigned int           n_geometry,
                                                  collada_data_geometry* out_geometry);


/** TODO */
PUBLIC EMERALD_API void collada_data_get_image(collada_data        data,
                                               unsigned int        n_image,
                                               collada_data_image* out_image);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_material(collada_data           data,
                                                  unsigned int           n_material,
                                                  collada_data_material* out_material);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_material_by_name(collada_data              data,
                                                          system_hashed_ansi_string name,
                                                          collada_data_material*    out_material);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_property(collada_data          data,
                                                  collada_data_property property,
                                                  void*                 out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_scene(collada_data        data,
                                               unsigned int        n_scene,
                                               collada_data_scene* out_scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_set_property(collada_data          data,
                                                  collada_data_property property,
                                                  void*                 data_ptr);

#endif /* COLLADA_LOADER_H */
