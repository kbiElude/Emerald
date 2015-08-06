/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Generates a box spanning from <0, 0, 0> to <1, 1, 1>.
 *
 * Culling:     NOT taken into account (TODO).
 * Vertex data: vec3.
 *
 * TODO: The generated data sets should be stride-based.
 *
 * Implementation is NOT culling-aware.
 */
#ifndef PROCEDURAL_MESH_BOX_H
#define PROCEDURAL_MESH_BOX_H

#include "ogl/ogl_types.h"
#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_box,
                             procedural_mesh_box)


typedef enum
{
    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_ID,

    /* unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_NORMALS_DATA_OFFSET,

    /* unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET,


    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_ID,

    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_INDICES_DATA_OFFSET,

    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_NORMALS_DATA_OFFSET,

    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_ELEMENTS_BO_VERTEX_DATA_OFFSET,


    /* unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_N_TRIANGLES,

    /* unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_N_VERTICES,

    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_RESTART_INDEX
} procedural_mesh_box_property;

/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(ogl_context                   context,
                                                                  _procedural_mesh_data_bitmask mesh_data_bitmask,
                                                                  uint32_t                      n_horizontal_patches, /* number of horizontal patches per plane */
                                                                  uint32_t                      n_vertical_patches,   /* number of vertical patches per plane */
                                                                  system_hashed_ansi_string     name);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_box_get_property(procedural_mesh_box          box,
                                                         procedural_mesh_box_property property,
                                                         void*                        out_result);

#endif /* PROCEDURAL_MESH_BOX_H */
