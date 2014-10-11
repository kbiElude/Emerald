/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OCL_KDTREE_H
#define OCL_KDTREE_H

#include "mesh/mesh_types.h"
#include "ocl/ocl_types.h"

REFCOUNT_INSERT_DECLARATIONS(ocl_kdtree, ocl_kdtree);

/* Callback function pointer definitions */
typedef void (*PFNKDTREEINTERSECTRAYSCALLBACKPROC)(uint32_t n_ray_origin, void* user_arg);

/** TODO */
PUBLIC EMERALD_API bool ocl_kdtree_add_executor(__in  __notnull ocl_kdtree                         instance,
                                                __in            _ocl_kdtree_executor_configuration configuration,
                                                __out __notnull ocl_kdtree_executor_id*            out_executor_id);

/** TODO. This should be only used for off-line kd tree generation! 
 *
 *  @param min_leaf_multiplier_volume 0 <= x <= 1
 **/
PUBLIC EMERALD_API ocl_kdtree ocl_kdtree_create_from_mesh(__in __notnull ocl_context               context,
                                                          __in           kdtree_creation_flags     creation_flags,
                                                          __in __notnull mesh                      mesh,
                                                          __in __notnull system_hashed_ansi_string name,
                                                          __in           float                     min_leaf_volume_multiplier,
                                                          __in           uint32_t                  max_triangles_per_leaf,
                                                          __in           _ocl_kdtree_usage         usage);

/** TODO */
PUBLIC EMERALD_API _ocl_kdtree_executor_configuration ocl_kdtree_get_float2_executor_configuration(__in bool use_vec3_for_ray_direction_source);

/** TODO */
PUBLIC EMERALD_API _ocl_kdtree_executor_configuration ocl_kdtree_get_uchar8_executor_configuration(__in bool use_vec3_for_ray_direction_source);

/** TODO */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_cl_buffer_size(__in __notnull ocl_kdtree instance);

/** TODO */
PUBLIC EMERALD_API void ocl_kdtree_get_cl_mesh_vertex_data_buffer(__in  __notnull   ocl_kdtree instance,
                                                                  __out __maybenull uint32_t*  out_size,
                                                                  __out __maybenull void**     out_data);

/** TODO */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_nodes_data_offset(__in __notnull ocl_kdtree);

/** Returns data that can be used for drawing kd-tree preview. The data consists of 3D lines that
 *  can be rendered in order to see the Kd-tree structure.
 *
 *  When no longer needed, the result buffer should be released using delete[] operator.
 *  TODO
 **/
PUBLIC EMERALD_API bool ocl_kdtree_get_preview_data(__in  __notnull ocl_kdtree instance,
                                                    __out __notnull uint32_t*  out_n_lines,
                                                    __out __notnull float**    out_result);

/** TODO */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_triangle_ids_offset(__in __notnull ocl_kdtree);

/** TODO */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_triangle_lists_offset(__in __notnull ocl_kdtree);

/** TODO 
 *
 *  NOTE: You are allowed to either cast multiple rays from multiple origins (performance hint: each origin is a separate kernel run!)
 *        *OR* a single ray from multiple origins (incl. a single origin; performance hint: a single kernel run is used) 
 *
 *  @param should_store_extended_results false to store uchar8 visibility info in target MO only; true to store hit's float2 barycentric coords
 *  @param target_triangle_hit_data_mo   only used if should_store_triangle_hit_data is true. Saves detailed triangle hit information
 *                                       for every sample.
 **/
PUBLIC EMERALD_API bool ocl_kdtree_intersect_rays(__in                         __notnull   ocl_kdtree,
                                                  __in                                     ocl_kdtree_executor_id             executor_id,
                                                  __in                                     uint32_t                           n_rays,
                                                  __in                                     uint32_t                           n_ray_origins,
                                                  __in_ecount(3*n_ray_origins) __notnull   const float*                       ray_origins,
                                                  __in                         __notnull   cl_mem                             ray_origins_mo,
                                                  __in                                     uint32_t                           ray_origins_mo_vec4_offset,
                                                  __in                                     cl_mem                             ray_directions_mo,
                                                  __in                                     uint32_t                           ray_directions_mo_offset,
                                                  __in                                     cl_mem                             target_mo,
                                                  __in                                     cl_mem                             target_triangle_hit_data_mo,
                                                  __in                                     int                                should_store_triangle_hit_data,
                                                  __in                                     int                                should_find_closest_intersection,
                                                  __in                         __maybenull PFNKDTREEINTERSECTRAYSCALLBACKPROC on_next_ray_origin_callback_proc,
                                                  __in                         __maybenull void*                              on_next_ray_origin_callback_user_arg);

/** TODO */
PUBLIC EMERALD_API bool ocl_kdtree_intersect_rays_with_data_upload(__in                         __notnull   ocl_kdtree,
                                                                   __in                                     ocl_kdtree_executor_id             executor_id,
                                                                   __in                                     uint32_t                           n_rays,
                                                                   __in                                     uint32_t                           n_ray_origins,
                                                                   __in_ecount(3*n_ray_origins) __notnull   const float*                       ray_origins,
                                                                   __in                         __notnull   cl_mem                             ray_origins_mo,
                                                                   __in                                     uint32_t                           ray_origins_mo_offset,
                                                                   __in_ecount(n_rays*3)        __notnull   const float*                       ray_directions, /* normalized */
                                                                   __in                         __notnull   uint32_t                           ray_directions_mo_vec4_offset,
                                                                   __in                                     cl_mem                             target_mo,
                                                                   __in                                     cl_mem                             target_triangle_hit_data_mo,
                                                                   __in                                     bool                               should_store_triangle_hit_data,
                                                                   __in                                     bool                               should_find_closest_intersection,
                                                                   __in                         __maybenull PFNKDTREEINTERSECTRAYSCALLBACKPROC on_next_ray_origin_callback_proc,
                                                                   __in                         __maybenull void*                              on_next_ray_origin_callback_user_arg);

/** TODO.
 *
 *  NOTE: A loaded Kd-tree will only work for intersection tests. No time has been spent on analysing 
 *        other codepaths.. so they will likely crash :)
 **/
PUBLIC EMERALD_API ocl_kdtree ocl_kdtree_load(__in __notnull ocl_context               context,
                                              __in __notnull system_hashed_ansi_string full_file_path,
                                              __in           _ocl_kdtree_usage         usage);

/** Saves CL buffer storage to a file. This allows to load the processed kd-tree representation later on,
 *  in order to skip the potentially time-consuming processing which can be done off-line.
 *
 *  @param ocl_kdtree                Kd tree to serialize.
 *  @param system_hashed_ansi_string Full target file path.
 *
 *  @return true if successful, false otherwise
 **/
PUBLIC EMERALD_API bool ocl_kdtree_save(__in __notnull ocl_kdtree, 
                                        __in __notnull system_hashed_ansi_string full_file_path);

#endif /* OCL_KDTREE_H */
