/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * [Regular meshes]
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
 * [Custom meshes]
 *
 * A special type of a mesh. It does not support any of the logic described above. However, at creation time, a func ptr
 * must be provided, which will be used at draw call time to render the object.
 * In a bigger picture, this lets you build a scene graph with meshes, who are rendered in a customized manner.
 *
 */
#ifndef MESH_H
#define MESH_H

#include "ogl/ogl_types.h"
#include "mesh/mesh_types.h"
#include "scene/scene_types.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(mesh, mesh)

typedef enum
{
    MESH_INDEX_TYPE_UNSIGNED_CHAR,
    MESH_INDEX_TYPE_UNSIGNED_SHORT,
    MESH_INDEX_TYPE_UNSIGNED_INT,

    MESH_INDEX_TYPE_UNKNOWN
} _mesh_index_type;

typedef void (*PFNRENDERCUSTOMMESHPROC)(ogl_context            context,
                                        const system_matrix4x4 model_matrix,
                                        const system_matrix4x4 vp_matrix,
                                        const system_matrix4x4 normal_matrix,
                                        bool                   is_depth_prepass);


/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API mesh_layer_id mesh_add_layer(mesh);

/** TODO. Data is copied to internal storage, so all pointers can be freed after this func is called.
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_add_layer_data_stream(mesh                        mesh,
                                                   mesh_layer_id               layer_id,
                                                   mesh_layer_data_stream_type type,
                                                   unsigned int                n_items,
                                                   const void*                 data);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass(mesh          mesh_instance,
                                                          mesh_layer_id layer_id,
                                                          mesh_material material,
                                                          uint32_t      n_elements);

/** TODO.
 *
 *  A layer pass uses a material that is specified during creation time. For the specified material,
  * an arbitrary number of data streams can be defined. Each data stream is of user-specified type.
  * A number of data streams of the same type (eg. texcoord) can be defined - each such stream of
  * a specified type is called "a set".
  *
  *  NOTE: Can only be called against regular meshes.
  */
PUBLIC EMERALD_API bool mesh_add_layer_pass_index_data(mesh                        mesh_instance,
                                                       mesh_layer_id               layer_id,
                                                       mesh_layer_pass_id          layer_pass_id,
                                                       mesh_layer_data_stream_type stream_type,
                                                       unsigned int                set_id,
                                                       const void*                 index_data,
                                                       unsigned int                min_index,
                                                       unsigned int                max_index);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create_custom_mesh(PFNRENDERCUSTOMMESHPROC   pfn_render_mesh,
                                                system_hashed_ansi_string name);

/** TODO
 *
 *  NOTE: Can only be called for regular meshes.
 */
PUBLIC EMERALD_API mesh mesh_create_from_meshes(mesh_creation_flags       flags,
                                                system_hashed_ansi_string name,
                                                uint32_t                  n_meshes,
                                                mesh*                     meshes);

/** TODO. Can be freed with mesh_free_single_indexed_representation().
 *
 *  NOTE: Can only be called against regular meshes.
 **/
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create_regular_mesh(mesh_creation_flags       flags,
                                                 system_hashed_ansi_string name);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_delete_layer_data_stream(mesh                        mesh,
                                                      mesh_layer_id               layer_id,
                                                      mesh_layer_data_stream_type type);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_delete_layer_pass_index_data(mesh                        mesh_instance,
                                                          mesh_layer_id               layer_id,
                                                          mesh_layer_pass_id          layer_pass_id,
                                                          mesh_layer_data_stream_type stream_type,
                                                          unsigned int                set_id);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(mesh,
                                             ogl_context);

/** TODO.
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_free_single_indexed_representation(mesh instance);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_generate_normal_data(mesh);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API uint32_t mesh_get_amount_of_layer_passes(mesh,
                                                            mesh_layer_id);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_data(mesh                        mesh,
                                                        mesh_layer_id               layer_id,
                                                        mesh_layer_data_stream_type type,
                                                        unsigned int*               out_n_items_ptr,
                                                        const void**                out_data_ptr);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_property(mesh                            mesh,
                                                            mesh_layer_data_stream_type     type,
                                                            mesh_layer_data_stream_property property,
                                                            void*                           out_result);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_get_layer_pass_property(mesh,
                                                     uint32_t            n_layer,
                                                     uint32_t            n_pass,
                                                     mesh_layer_property,
                                                     void*);

/** TODO
 *
 *  NOTE: Most of the properties only be queried for regular meshes.
 */
PUBLIC EMERALD_API bool mesh_get_property(mesh,
                                          mesh_property,
                                          void*);

/** TODO */
PUBLIC EMERALD_API mesh mesh_load(ogl_context               context,
                                  mesh_creation_flags       flags,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_id_to_mesh_material_map,
                                  system_hash64map          mesh_name_to_mesh_map);

/** TODO */
PUBLIC EMERALD_API mesh mesh_load_with_serializer(ogl_context            context,
                                                  mesh_creation_flags    flags,
                                                  system_file_serializer serializer,
                                                  system_hash64map       material_id_to_mesh_material_map,
                                                  system_hash64map       mesh_name_to_mesh_map);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_release_layer_datum(mesh);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_save(mesh,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_name_to_id_map);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_save_with_serializer(mesh,
                                                  system_file_serializer,
                                                  system_hash64map       mesh_material_to_id_map);

/** TODO.
 *
 *  Releases any layer/layer pass data that was set for the mesh and marks the mesh
 *  as an instance of @param source_mesh . Not only does this prevent from inefficient
 *  GL blob storage, but also provides a mean for draw call batching.
 *
 *  A mesh that's been marked as instantiated cannot have its layer/layer pass data
 *  changed.
 *
 *  @param mesh_to_modify TODO
 *  @param source_mesh    TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_as_instantiated(mesh mesh_to_modify,
                                                 mesh source_mesh);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_layer_property(mesh,
                                                uint32_t            n_layer,
                                                uint32_t            n_pass,
                                                mesh_layer_property,
                                                const void*         data);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_processed_data_stream_start_offset(mesh,
                                                                    mesh_layer_data_stream_type stream_type,
                                                                    unsigned int                offset);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_property(mesh,
                                          mesh_property,
                                          void*);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_single_indexed_representation(mesh             mesh,
                                                               _mesh_index_type index_type,
                                                               uint32_t         n_blob_data_bytes,
                                                               const void*      blob_data);

#endif /* MESH_H */
