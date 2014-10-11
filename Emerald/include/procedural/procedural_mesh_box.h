/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO.
 *
 * Implementation is NOT culling-aware.
 */
#ifndef PROCEDURAL_MESH_BOX_H
#define PROCEDURAL_MESH_BOX_H

#include "ogl/ogl_types.h"
#include "procedural/procedural_types.h"

REFCOUNT_INSERT_DECLARATIONS(procedural_mesh_box, procedural_mesh_box)


/** TODO.
 *
 **/
PUBLIC EMERALD_API procedural_mesh_box procedural_mesh_box_create(__in __notnull ogl_context,
                                                                  __in           _procedural_mesh_data_bitmask,
                                                                  __in __notnull uint32_t, /* number of horizontal patches per plane */
                                                                  __in __notnull uint32_t, /* number of vertical patches per plane */
                                                                  __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_arrays_bo_id(__in __notnull procedural_mesh_box);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_elements_bo_id(__in __notnull procedural_mesh_box);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_box_get_arrays_bo_offsets(__in  __notnull procedural_mesh_box,
                                                                  __out __notnull GLuint* out_vertex_data_offset,
                                                                  __out __notnull GLuint* out_normals_data_offset);

/** TODO */
PUBLIC EMERALD_API void procedural_mesh_box_get_elements_bo_offsets(__in  __notnull procedural_mesh_box,
                                                                    __out __notnull GLuint* out_vertex_data_offset,
                                                                    __out __notnull GLuint* out_elements_data_offset,
                                                                    __out __notnull GLuint* out_normals_data_offset);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_number_of_points(__in __notnull procedural_mesh_box);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_number_of_triangles(__in __notnull procedural_mesh_box);

/** TODO */
PUBLIC EMERALD_API GLuint procedural_mesh_box_get_restart_index(__in __notnull procedural_mesh_box);

#endif /* PROCEDURAL_MESH_BOX_H */
