/**
 *
 * Emerald (kbi/elude @2012-2016)
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

#include "mesh/mesh_types.h"
#include "ral/ral_types.h"
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
typedef void (*PFNRENDERCUSTOMMESHPROC)(ral_context            context_ral,
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
 *  NOTE: The number of data stream items (eg. vertices, normals) is not defined at creation time.
 *        By default, it is assumed you will set it to either a specific value with a subsequent
 *        mesh_set_layer_data_stream_property() call, or point the property to a buffer memory
 *        location with a mesh_set_layer_data_stream_property_with_buffer_memory() call.
 *        Mind that not all Emerald modules may support both sources!
 *
 *  NOTE: Can only be called against GPU stream meshes.
 *
 *  @param mesh            Mesh instance to configure. Must not be NULL.
 *  @param layer_id        Mesh layer ID.
 *  @param type            Type of the data stream to configure.
 *  @param n_components    Number of components per single item. (eg. you'd usually go with 2 for UV coords)
 *  @param bo              Buffer object to source the data from.
 *  @param bo_stride       Stride between consecutive items in bytes. Must not be 0.
 *                         For floating-point UV coords, you'd want to pass sizeof(float) * 2.
 */
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_buffer_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      unsigned int                n_components,
                                                                      ral_buffer                  bo,
                                                                      unsigned int                bo_stride);

/** TODO. Data is copied to internal storage, so all pointers can be freed after this func is called.
 *
 *  NOTE: Can only be called against regular meshes.
 */
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_client_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      unsigned int                n_components,
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
PUBLIC EMERALD_API bool mesh_fill_ral_buffers(mesh        instance,
                                              ral_context context_ral,
                                              bool        async);

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

/** Retrieves properties of an already added layer data stream, defined for specified mesh layer.
 *
 *  NOTE: Can only be called against GPU stream & regular meshes.
 *  NOTE: Invalid calls will throw assertion failures.
 *
 *        For Regular Meshes, both @param out_n_items_ptr and @param out_data_ptr will be filled.
 *        NULL arguments will be skipped.
 *
 *        For GPU Stream Meshes, only @param out_n_items_ptr will be updated.
 *        Note that if the data stream uses buffer memory to store the number of items, the function
 *        will report 0. To retrieve ID of the BO or the start offset, use mesh_get_layer_data_stream_property().
 *
 *
 *  @param mesh            Mesh instance to issue the query against.
 *  @param layer_id        Mesh layer ID.
 *  @param type            Data stream type the query is to be ussed for.
 *  @param out_n_items_ptr Deref will be set to the number of items configured for the specified
 *                         data stream for both GPU Stream & Regular meshes . Also see NOTE above.
 *  @param out_data_ptr    Deref will be set to the raw stream data, assuming it has not yet been
 *                         released with eg. mesh_release_layer_datum() call. Will only be updated
 *                         for Regular Meshes. Also see NOTE above for further comments.
 *
 *  @return true if the mesh layer was identified and the non-NULL out_ parameters were updated.
 *          false otherwise.
 */
PUBLIC EMERALD_API bool mesh_get_layer_data_stream_data(mesh                        mesh,
                                                        mesh_layer_id               layer_id,
                                                        mesh_layer_data_stream_type type,
                                                        unsigned int*               out_n_items_ptr,
                                                        const void**                out_data_ptr);

/** TODO
 *
 *  NOTE: Can only be called against GPU stream & regular meshes.
 */
PUBLIC EMERALD_API bool mesh_get_layer_data_stream_property(mesh                            mesh,
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
PUBLIC EMERALD_API mesh mesh_load(ral_context               context_ral,
                                  mesh_creation_flags       flags,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_id_to_mesh_material_map,
                                  system_hash64map          mesh_name_to_mesh_map);

/** TODO */
PUBLIC EMERALD_API mesh mesh_load_with_serializer(ral_context            context_ral,
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

/** Updates a mesh layer data stream property with a value coming from client memory.
 *
 *  @param instance Mesh instance to update the property for.
 *  @param n_layer  Mesh layer index.
 *  @param type     Data stream type.
 *  @param property Property to update.
 *  @param data     Deref will be used to read the data to be assigned to the property.
 *
 *  @return true if the operation was successful, false otherwise.
 *  */
PUBLIC EMERALD_API bool mesh_set_layer_data_stream_property(mesh                            instance,
                                                            uint32_t                        n_layer,
                                                            mesh_layer_data_stream_type     type,
                                                            mesh_layer_data_stream_property property,
                                                            const void*                     data);

/** Updates a mesh layer data stream property by assigning it a value in buffer memory
 *
 *  This function does NOT copy the value, as stored in the pointed buffer memory location, at the time
 *  of the call. Instead, it stores the BO id and BO start offset, in order to let dependent modules to
 *  read it from that location whenever necessary.
 *  Dependent modules are expected to leverage rendering API capabilities to perform this in optimal
 *  manner (eg. indirect dispatch/draw calls).
 *
 *  Currently, the only property supported by the function are:
 *
 *  - MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS.
 *
 *  @param instance                         Mesh instance to update the property for.
 *  @param n_layer                          Mesh layer index.
 *  @param type                             Data stream type.
 *  @param property                         Property to update.
 *  @param does_read_require_memory_barrier true if a memory barrier should be issued before the data is read from
 *                                          the buffer memory; false if not.
 *
 *  @return true if the operation was successful, false otherwise.
 */
PUBLIC EMERALD_API bool mesh_set_layer_data_stream_property_with_buffer_memory(mesh                            instance,
                                                                               uint32_t                        n_layer,
                                                                               mesh_layer_data_stream_type     type,
                                                                               mesh_layer_data_stream_property property,
                                                                               ral_buffer                      bo,
                                                                               bool                            does_read_require_memory_barrier);

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
 *  NOTE: Can be called for any type of mesh for the MESH_PROPERTY_VERTEX_ORDERING property.
 *        For all others, this entry-point can only be called against regular meshes.
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
