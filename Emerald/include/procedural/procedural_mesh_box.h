/**
 *
 * Emerald (kbi/elude @2012-2015)
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
    PROCEDURAL_MESH_BOX_PROPERTY_N_POINTS,

    /* unsigned int */
    PROCEDURAL_MESH_BOX_PROPERTY_N_TRIANGLES,

    /* GLuint */
    PROCEDURAL_MESH_BOX_PROPERTY_RESTART_INDEX
} procedural_mesh_box_property;

/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(__in __notnull ogl_context,
                                                                  __in           _procedural_mesh_data_bitmask,
                                                                  __in __notnull uint32_t, /* number of horizontal patches per plane */
                                                                  __in __notnull uint32_t, /* number of vertical patches per plane */
                                                                  __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_box_get_property(__in  __notnull procedural_mesh_box          box,
                                                         __in            procedural_mesh_box_property property,
                                                         __out __notnull void*                        out_result);

#endif /* PROCEDURAL_MESH_BOX_H */
