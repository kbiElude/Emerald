/**
 *
 * Emerald (kbi/elude @2012-2015)
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
    PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_ID,                  /* not settable, GLuint */
    PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_NORMALS_DATA_OFFSET, /* not settable, GLuint */
    PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_BO_VERTEX_DATA_OFFSET,  /* not settable, GLuint */
    PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_RAW_DATA,               /* not settable, GLfloat*. Will throw an assertion failure if the sphere
                                                                    *                         has not been created with DATA_RAW flag. */

    PROCEDURAL_MESH_SPHERE_PROPERTY_ARRAYS_TBO_ID,                 /* not settable, ogl_texture. The getter will spawn the object, when the
                                                                    *                            texture is requested for the 1st time.
                                                                    **/

    PROCEDURAL_MESH_SPHERE_PROPERTY_N_TRIANGLES,                  /* not settable, unsigned int */
} _procedural_mesh_sphere_property;

/** TODO. Does NOT support 
 *
 **/
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(__in __notnull ogl_context,
                                                                        __in           _procedural_mesh_data_bitmask,
                                                                        __in __notnull uint32_t                      n_latitude_splices, /* number of latitude splices */
                                                                        __in __notnull uint32_t                      n_longitude_splices, /* number of longitude splices */
                                                                        __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_property(__in  __notnull procedural_mesh_sphere           sphere,
                                                            __in            _procedural_mesh_sphere_property prop,
                                                            __out __notnull void*                            out_result);


#endif /* PROCEDURAL_MESH_SPHERE_H */
