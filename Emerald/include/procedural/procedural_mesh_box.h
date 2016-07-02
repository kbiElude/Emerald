/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * Generates a box spanning from <0, 0, 0> to <1, 1, 1>.
 *
 * Culling:     NOT taken into account (TODO).
 * Vertex data: vec3.
 *
 * TODO: The generated data streams should be laid out in an interleaving manner.
 *
 * Implementation is NOT culling-aware.
 */
#ifndef PROCEDURAL_MESH_BOX_H
#define PROCEDURAL_MESH_BOX_H

#include "procedural/procedural_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_box,
                             procedural_mesh_box)


typedef enum
{
    /* not settable; GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER,

    /* not settable; GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_INDEX_DATA_OFFSET,

    /* not settable; GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_NORMAL_DATA_OFFSET,

    /* not settable; GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_INDEXED_BUFFER_VERTEX_DATA_OFFSET,


    /* not settable; ral_buffer */
    PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER,

    /* not settable; unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER_NORMAL_DATA_OFFSET,

    /* not settable; unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_NONINDEXED_BUFFER_VERTEX_DATA_OFFSET,


    /* not settable; unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_N_TRIANGLES,

    /* not settable; unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_N_VERTICES,

    /* not settable; GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_RESTART_INDEX
} procedural_mesh_box_property;

/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(ral_context                    context,
                                                                  procedural_mesh_data_type_bits mesh_data_types,
                                                                  uint32_t                       n_horizontal_patches, /* number of horizontal patches per plane */
                                                                  uint32_t                       n_vertical_patches,   /* number of vertical patches per plane */
                                                                  system_hashed_ansi_string      name);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_box_get_property(procedural_mesh_box          box,
                                                         procedural_mesh_box_property property,
                                                         void*                        out_result);

#endif /* PROCEDURAL_MESH_BOX_H */
