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
 * In a bigger picture, this lets you build a scene graph with meshes, which are then rendered in a customized manner.
 *
 * [GPU stream meshes]
 *
 * A special type of a mesh. Follows the layer & the layer pass patterns described above. However, the data stream contents
 * needs to be provided via buffer memory regions, which the user is obliged to fill with data.
 * Furthermore, the user must specify Graphics API draw call type and relevant draw call argument values which should
 * be used in order to render each mesh layer pass.
 *
 * TODO: Due to how VAO management is implemented, only ONE layer may be created for the GPU stream meshes. This should not
 *       be an extremely painful limitation and can be fixed when necessary.
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

typedef void (*PFNGETMESHAABBPROC)     (const void*            user_arg,
                                        float*                 out_aabb_model_vec3_min,
                                        float*                 out_aabb_model_vec3_max);
typedef void (*PFNRENDERCUSTOMMESHPROC)(ogl_context            context,
                                        const void*            user_arg,
                                        const system_matrix4x4 model_matrix,
                                        const system_matrix4x4 vp_matrix,
                                        const system_matrix4x4 normal_matrix,
                                        bool                   is_depth_prepass);


/** TODO
 *
 *  NOTE: Only one layer can be added for GPU stream meshes. Please see documentation
 *        at the top of this file for more details.
 *  NOTE: Can only be called against regular and GPU stream meshes.
 */
PUBLIC EMERALD_API mesh_layer_id mesh_add_layer(mesh);

/** TODO.
 *
 *  NOTE: Can only be called against GPU stream meshes.
 */
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_buffer_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      GLuint                      bo_id,
                                                                      unsigned int                bo_start_offset,
                                                                      unsigned int                bo_stride);

/** TODO. Data is copied to internal storage, so all pointers can be freed after this func is called.
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_client_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      unsigned int                n_items,
                                                                      const void*                 data);

/** TODO
 *
 *  NOTE: Can only be called against GPU stream meshes.
 *
 *  @param mesh_instance                 TODO
 *  @param layer_id                      TODO
 *  @param material                      TODO
 *  @param draw_call_type                Type of the draw call which should be used to render the layer pass.
 *  @param draw_call_argument_values_ptr Pointer to a structure specifying draw call argument values. These
 *                                       values can be changed later by calling mesh_set_layer_pass_property().
 *
 *  @return TODO
 */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass_for_gpu_stream_mesh(mesh                            mesh_instance,
                                                                              mesh_layer_id                   layer_id,
                                                                              mesh_material                   material,
                                                                              mesh_draw_call_type             draw_call_type,
                                                                              const mesh_draw_call_arguments* draw_call_argument_values_ptr);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass_for_regular_mesh(mesh          mesh_instance,
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
PUBLIC EMERALD_API bool mesh_add_layer_pass_index_data_for_regular_mesh(mesh                        mesh_instance,
                                                                        mesh_layer_id               layer_id,
                                                                        mesh_layer_pass_id          layer_pass_id,
                                                                        mesh_layer_data_stream_type stream_type,
                                                                        unsigned int                set_id,
                                                                        const void*                 index_data,
                                                                        unsigned int                min_index,
                                                                        unsigned int                max_index);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create_custom_mesh(PFNRENDERCUSTOMMESHPROC   pfn_render_custom_mesh_proc,
                                                void*                     render_custom_mesh_proc_user_arg,
                                                PFNGETMESHAABBPROC        pfn_get_custom_mesh_aabb_proc,
                                                void*                     get_custom_mesh_aabb_proc_user_arg,
                                                system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create_gpu_stream_mesh(PFNGETMESHAABBPROC        pfn_get_gpu_stream_mesh_aabb_proc,
                                                    void*                     get_gpu_stream_mesh_aabb_user_arg,
                                                    system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API mesh mesh_create_regular_mesh(mesh_creation_flags       flags,
                                                 system_hashed_ansi_string name);

/** TODO. Can be freed with mesh_free_single_indexed_representation().
 *
 *  NOTE: Can only be called against regular meshes.
 **/
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance);

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
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(mesh        instance,
                                             ogl_context context);

/** TODO.
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_free_single_indexed_representation(mesh instance);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_generate_normal_data(mesh mesh);

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
 *  NOTE: Can only be called against GPU stream & regular meshes.
 */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_property(mesh                            mesh,
                                                            mesh_layer_id                   layer_id,
                                                            mesh_layer_data_stream_type     type,
                                                            mesh_layer_data_stream_property property,
                                                            void*                           out_result_ptr);

/** TODO
 *
 *  NOTE: Can only be called against GPU stream & regular meshes.
 */
PUBLIC EMERALD_API bool mesh_get_layer_pass_property(mesh                instance,
                                                     uint32_t            n_layer,
                                                     uint32_t            n_pass,
                                                     mesh_layer_property property,
                                                     void*               out_result_ptr);

/** TODO
 *
 *  NOTE: Can only be called against GPU stream & regular meshes.
 */
PUBLIC EMERALD_API uint32_t mesh_get_number_of_layer_passes(mesh          instance,
                                                            mesh_layer_id layer_id);

/** TODO
 *
 *  NOTE: Most of the properties are only queriable for regular meshes.
 */
PUBLIC EMERALD_API bool mesh_get_property(mesh          instance,
                                          mesh_property property,
                                          void*         out_result_ptr);

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
PUBLIC EMERALD_API bool mesh_save(mesh                      instance,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_name_to_id_map);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API bool mesh_save_with_serializer(mesh                   instance,
                                                  system_file_serializer serializer,
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
PUBLIC EMERALD_API void mesh_set_layer_property(mesh                instance,
                                                uint32_t            n_layer,
                                                uint32_t            n_pass,
                                                mesh_layer_property property,
                                                const void*         data);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_processed_data_stream_start_offset(mesh                        mesh,
                                                                    mesh_layer_data_stream_type stream_type,
                                                                    unsigned int                start_offset);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_property(mesh          instance,
                                          mesh_property property,
                                          const void*   value);

/** TODO
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_set_single_indexed_representation(mesh             mesh,
                                                               _mesh_index_type index_type,
                                                               uint32_t         n_blob_data_bytes,
                                                               const void*      blob_data);

#endif /* MESH_H */
