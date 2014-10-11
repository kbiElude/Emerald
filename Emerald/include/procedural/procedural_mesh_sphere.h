/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO.
 *
 */
#ifndef PROCEDURAL_MESH_SPHERE_H
#define PROCEDURAL_MESH_SPHERE_H

#include "ogl/ogl_types.h"
#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_sphere, procedural_mesh_sphere)


/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_sphere procedural_mesh_sphere_create(__in __notnull ogl_context,
                                                                        __in           _procedural_mesh_data_bitmask,
                                                                        __in __notnull uint32_t                      n_latitude_splices, /* number of latitude splices */
                                                                        __in __notnull uint32_t                      n_longitude_splices, /* number of longitude splices */
                                                                        __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_arrays_bo_id(__in __notnull procedural_mesh_sphere);

/** TODO */
PUBLIC EMERALD_API ogl_texture procedural_mesh_sphere_get_arrays_tbo(__in __notnull procedural_mesh_sphere);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_elements_bo_id(__in __notnull procedural_mesh_sphere);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_arrays_bo_offsets(__in  __notnull procedural_mesh_sphere,
                                                                     __out __notnull GLuint* out_vertex_data_offset,
                                                                     __out __notnull GLuint* out_normals_data_offset);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_sphere_get_elements_bo_offsets(__in  __notnull procedural_mesh_sphere,
                                                                       __out __notnull GLuint* out_vertex_data_offset,
                                                                       __out __notnull GLuint* out_elements_data_offset,
                                                                       __out __notnull GLuint* out_normals_data_offset);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_number_of_points(__in __notnull procedural_mesh_sphere);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_sphere_get_number_of_triangles(__in __notnull procedural_mesh_sphere);

#endif /* PROCEDURAL_MESH_SPHERE_H */
