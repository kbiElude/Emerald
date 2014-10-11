/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef COLLADA_DATA_H
#define COLLADA_DATA_H

#include "collada/collada_types.h"
#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "scene/scene_types.h"

REFCOUNT_INSERT_DECLARATIONS(collada_data, collada_data)

enum collada_data_property
{
    COLLADA_DATA_PROPERTY_CACHE_BINARY_BLOBS_MODE,
    COLLADA_DATA_PROPERTY_CAMERAS_BY_ID_MAP,        /* not settable, system_hash64map */
    COLLADA_DATA_PROPERTY_FILE_NAME,
    COLLADA_DATA_PROPERTY_GEOMETRIES_BY_ID_MAP,     /* not settable, system_hash64map */
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
PUBLIC EMERALD_API collada_data collada_data_load(__in __notnull system_hashed_ansi_string filename,
                                                  __in __notnull system_hashed_ansi_string name,
                                                  __in           bool                      should_generate_cache_blobs = false);

/** TODO */
PUBLIC EMERALD_API bool collada_data_get_camera_by_index(__in  __notnull collada_data         data,
                                                         __in            uint32_t             n_camera,
                                                         __out __notnull collada_data_camera* out_camera);

/** TODO */
PUBLIC EMERALD_API collada_data_camera collada_data_get_camera_by_name(__in __notnull collada_data              data,
                                                                       __in           system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_effect(__in __notnull  collada_data         data,
                                                __in            unsigned int         n_effect,
                                                __out __notnull collada_data_effect* out_effect);

/** TODO.
 *
 *  mesh = renderer+geometrical mesh representation
 **/
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh(__in __notnull collada_data data,
                                                      __in __notnull ogl_context  context,
                                                                     unsigned int n_mesh);

/** TODO */
PUBLIC EMERALD_API mesh collada_data_get_emerald_mesh_by_name(__in __notnull collada_data              data,
                                                              __in __notnull ogl_context               context,
                                                              __in           system_hashed_ansi_string name);

/** TODO. */
PUBLIC EMERALD_API scene collada_data_get_emerald_scene(__in __notnull collada_data data,
                                                        __in __notnull ogl_context  context,
                                                                       unsigned int n_scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_geometry(__in __notnull  collada_data           data,
                                                  __in            unsigned int           n_geometry,
                                                  __out __notnull collada_data_geometry* out_geometry);


/** TODO */
PUBLIC EMERALD_API void collada_data_get_image(__in  __notnull collada_data        data,
                                               __in            unsigned int        n_image,
                                               __out __notnull collada_data_image* out_image);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_material(__in __notnull  collada_data           data,
                                                  __in            unsigned int           n_material,
                                                  __out __notnull collada_data_material* out_material);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_material_by_name(__in  __notnull collada_data              data,
                                                          __in  __notnull system_hashed_ansi_string name,
                                                          __out __notnull collada_data_material*    out_material);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_property(__in  __notnull collada_data          data,
                                                  __in            collada_data_property property,
                                                  __out __notnull void*                 out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_get_scene(__in  __notnull collada_data        data,
                                               __in            unsigned int        n_scene,
                                               __out __notnull collada_data_scene* out_scene);

/** TODO */
PUBLIC EMERALD_API void collada_data_set_property(__in __notnull collada_data          data,
                                                  __in           collada_data_property property,
                                                  __in __notnull void*                 data_ptr);

#endif /* COLLADA_LOADER_H */
