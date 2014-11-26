/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 * Mesh is defined by layers. Layers are Emerald's way of saying that mesh can consist of different sets of unique
 * vertex/normal data (similar to LW). 
 *
 * Each mesh layer by itself is not rendered in any way. Instead it is used to create buffer objects used as 
 * data source during rendering pass.
 *
 * In order to use mesh layer data, passes must be provided. Each mesh layer pass can be (and should be ;-) ) assigned different
 * surface id. Surface passes are created by providing elements data for normals, vertices, etc. 
 *
 * Each mesh is rendered layer by layer, and each layer pass is rendered in sequence. The cost of switching between layers
 * is the highest, where as switching between layer passes is relatively cheap (only updates uniforms used for defining
 * offsets in the vertex shader, unless a shader switch is involved if the surface configuration requires one to happen)
 *
 */
#ifndef MESH_H
#define MESH_H

#include "curve/curve_types.h"
#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "scene/scene_types.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(mesh, mesh)

typedef enum _mesh_index_type
{
    MESH_INDEX_TYPE_UNSIGNED_CHAR,
    MESH_INDEX_TYPE_UNSIGNED_SHORT,
    MESH_INDEX_TYPE_UNSIGNED_INT,

    MESH_INDEX_TYPE_UNKNOWN
};


/** TODO */
PUBLIC EMERALD_API mesh_layer_id mesh_add_layer(__in __notnull mesh);

/** TODO. Data is copied to internal storage, so all pointers can be freed after this func is called. */
PUBLIC EMERALD_API void mesh_add_layer_data_stream(__in __notnull mesh                        mesh,
                                                   __in __notnull mesh_layer_id               layer_id,
                                                   __in __notnull mesh_layer_data_stream_type type,
                                                   __in           unsigned int                n_items,
                                                   __in __notnull const void*                 data);

/** TODO */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass(__in __notnull mesh          mesh_instance,
                                                          __in           mesh_layer_id layer_id,
                                                          __in           mesh_material material,
                                                          __in           uint32_t      n_elements);

/** TODO.
 *
 *  A layer pass uses a material that is specified during creation time. For the specified material,
  * an arbitrary number of data streams can be defined. Each data stream is of user-specified type.
  * A number of data streams of the same type (eg. texcoord) can be defined - each such stream of
  * a specified type is called "a set".
  *
  */
PUBLIC EMERALD_API bool mesh_add_layer_pass_index_data(__in __notnull mesh                        mesh_instance,
                                                       __in           mesh_layer_id               layer_id,
                                                       __in           mesh_layer_pass_id          layer_pass_id,
                                                       __in           mesh_layer_data_stream_type stream_type,
                                                       __in           unsigned int                set_id,
                                                       __in           const void*                 index_data,
                                                       __in           unsigned int                min_index,
                                                       __in           unsigned int                max_index);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create(__in           mesh_creation_flags       flags,
                                    __in __notnull system_hashed_ansi_string name);

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_create_from_meshes(__in                            mesh_creation_flags       flags,
                                                __in                  __notnull system_hashed_ansi_string name,
                                                __in                            uint32_t                  n_meshes,
                                                __in_ecount(n_meshes) __notnull mesh*                     meshes);

/** TODO. Can be freed with mesh_free_single_indexed_representation() */
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance);

/** TODO */
PUBLIC EMERALD_API void mesh_delete_layer_data_stream(__in __notnull mesh                        mesh,
                                                      __in __notnull mesh_layer_id               layer_id,
                                                      __in __notnull mesh_layer_data_stream_type type);

/** TODO */
PUBLIC EMERALD_API bool mesh_delete_layer_pass_index_data(__in __notnull mesh                        mesh_instance,
                                                          __in           mesh_layer_id               layer_id,
                                                          __in           mesh_layer_pass_id          layer_pass_id,
                                                          __in           mesh_layer_data_stream_type stream_type,
                                                          __in           unsigned int                set_id);

/** TODO */
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(__in __notnull mesh,
                                             __in __notnull ogl_context);

/** TODO. */
PUBLIC EMERALD_API void mesh_free_single_indexed_representation(mesh instance);

/** TODO */
PUBLIC EMERALD_API void mesh_generate_normal_data(__in __notnull mesh);

/** TODO */
PUBLIC EMERALD_API uint32_t mesh_get_amount_of_layer_passes(__in __notnull mesh,
                                                            __in           mesh_layer_id);

/** TODO */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_data(__in      __notnull mesh                        mesh,
                                                        __in      __notnull mesh_layer_id               layer_id,
                                                        __in      __notnull mesh_layer_data_stream_type type,
                                                        __out_opt __notnull unsigned int*               out_n_items_ptr,
                                                        __out_opt __notnull const void**                out_data_ptr);

/** TODO */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_property(__in  __notnull mesh                            mesh,
                                                            __in            mesh_layer_data_stream_type     type,
                                                            __in            mesh_layer_data_stream_property property,
                                                            __out __notnull void*                           out_result);

/** TODO */
PUBLIC EMERALD_API bool mesh_get_layer_pass_property(__in  __notnull mesh,
                                                     __in            uint32_t            n_layer,
                                                     __in            uint32_t            n_pass,
                                                     __in  __notnull mesh_layer_property,
                                                     __out __notnull void*);

/** TODO */
PUBLIC EMERALD_API bool mesh_get_property(__in  __notnull mesh,
                                          __in  __notnull mesh_property,
                                          __out __notnull void*);


/** TODO */
PUBLIC EMERALD_API mesh mesh_load(__in __notnull ogl_context               context,
                                  __in           mesh_creation_flags       flags,
                                  __in __notnull system_hashed_ansi_string full_file_path,
                                  __in __notnull system_hash64map          material_id_to_mesh_material_map,
                                  __in __notnull system_hash64map          mesh_name_to_mesh_map);

/** TODO */
PUBLIC EMERALD_API mesh mesh_load_with_serializer(__in __notnull ogl_context            context,
                                                  __in           mesh_creation_flags    flags,
                                                  __in __notnull system_file_serializer serializer,
                                                  __in __notnull system_hash64map       material_id_to_mesh_material_map,
                                                  __in __notnull system_hash64map       mesh_name_to_mesh_map);

/** TODO */
PUBLIC EMERALD_API bool mesh_save(__in __notnull mesh,
                                  __in __notnull system_hashed_ansi_string full_file_path,
                                  __in __notnull system_hash64map          material_name_to_id_map);

/** TODO */
PUBLIC EMERALD_API bool mesh_save_with_serializer(__in __notnull mesh,
                                                  __in __notnull system_file_serializer,
                                                  __in __notnull system_hash64map       material_name_to_id_map);

/** TODO.
 *
 *  Releases any layer/layer pass data that was set for the mesh and marks the mesh
 *  as an instance of @param source_mesh . Not only does this prevents from inefficient
 *  GL blob construction, but also provides a mean for draw call batching.
 *
 *  A mesh that's been marked as instantiated cannot have its layer/layer pass data
 *  changed.
 *
 *  @param mesh_to_modify TODO
 *  @param source_mesh    TODO
 *
 **/
PUBLIC EMERALD_API void mesh_set_as_instantiated(__in __notnull mesh mesh_to_modify,
                                                 __in __notnull mesh source_mesh);

/** TODO */
PUBLIC EMERALD_API void mesh_set_layer_property(__in __notnull mesh,
                                                __in           uint32_t            n_layer,
                                                __in           uint32_t            n_pass,
                                                __in __notnull mesh_layer_property,
                                                __in __notnull const void*         data);

/** TODO */
PUBLIC EMERALD_API void mesh_set_processed_data_stream_start_offset(__in __notnull mesh,
                                                                    __in           mesh_layer_data_stream_type stream_type,
                                                                    __in           unsigned int                offset);

/** TODO */
PUBLIC EMERALD_API void mesh_set_property(__in __notnull mesh,
                                          __in           mesh_property,
                                          __in __notnull void*);

/** TODO */
PUBLIC EMERALD_API void mesh_set_single_indexed_representation(__in __notnull                 mesh             mesh,
                                                               __in                           _mesh_index_type index_type,
                                                               __in                           uint32_t         n_blob_data_bytes,
                                                               __in_bcount(n_blob_data_bytes) const void*      blob_data);

#endif /* MESH_H */
