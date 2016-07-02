/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * TODO.
 *
 */
#ifndef PROCEDURAL_MESH_SPHERE_H
#define PROCEDURAL_MESH_SPHERE_H

#include "ogl/ogl_types.h"
#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_sphere,
                             procedural_mesh_sphere)

typedef enum
{
    /* not settable, ral_buffer */
    PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER,

    /* not settable, uint32_t */
    PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_NORMAL_DATA_OFFSET,

    /* not settable, uint32_t */
    PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET,

    /* not settable, float*.
     *
     * Will throw an assertion failure if the sphere has not been created with DATA_RAW flag. */
    PROCEDURAL_MESH_SPHERE_PROPERTY_NONINDEXED_RAW_DATA,

    /* not settable, unsigned int */
    PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES,
} _procedural_mesh_sphere_property;

/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(ral_context                    context,
                                                                        procedural_mesh_data_type_bits data_types,
                                                                        uint32_t                       n_latitude_splices,  /* number of latitude splices  */
                                                                        uint32_t                       n_longitude_splices, /* number of longitude splices */
                                                                        system_hashed_ansi_string      name);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_property(procedural_mesh_sphere           sphere,
                                                            _procedural_mesh_sphere_property prop,
                                                            void*                            out_result);


#endif /* PROCEDURAL_MESH_SPHERE_H */
