/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * TODO
 *
 */
#ifndef SH_TOOLS_H
#define SH_TOOLS_H

#include "shared.h"
#include "ocl/ocl_context.h"
#include "procedural/procedural_types.h"
#include "sh/sh_types.h"
#include "config.h"

/** TODO */
PUBLIC EMERALD_API void sh_tools_deinit();

#ifdef INCLUDE_OPENCL
    typedef void (*PFNSHTOOLSGENERATESHOPENCLCALLBACKPROC)(uint32_t n_normal, void* user_arg);

    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_unshadowed_transfer_constant_albedo_input_data(__in                                __notnull ocl_context   context,
                                                                                                                            __in                                          sh_components n_result_sh_components,
                                                                                                                            __in_ecount(n_result_sh_components)           const float*  albedo,
                                                                                                                            __in                                          uint32_t      n_normals,
                                                                                                                            __in                                __notnull const float*  normals,
                                                                                                                            __in                                          cl_mem        result_cl_buffer);    
    
    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_unshadowed_transfer_per_vertex_albedo_input_data(__in                                           __notnull ocl_context   context,
                                                                                                                              __in                                                     sh_components n_result_sh_components,
                                                                                                                              __in                                                     uint32_t      n_vertices,
                                                                                                                              __in_ecount(n_result_sh_components*n_vertices) __notnull const float*  albedo,
                                                                                                                              __in_ecount(3*n_vertices)                      __notnull const float*  normals,
                                                                                                                              __in                                           __notnull cl_mem        result_cl_buffer);

    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_transfer_constant_albedo_input_data(__in                                __notnull ocl_context   context,
                                                                                                                          __in                                          sh_components n_result_sh_components,
                                                                                                                          __in_ecount(n_result_sh_components)           const float*  albedo,
                                                                                                                          __in                                          uint32_t      n_normals,
                                                                                                                          __in                                __notnull const float*  normals,
                                                                                                                          __in                                          uint32_t      n_samples,
                                                                                                                          __in                                          uint32_t      n_sh_bands,
                                                                                                                          __in                                          cl_mem        visibility_cl_buffer,
                                                                                                                          __in                                          cl_mem        result_cl_buffer);

    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_transfer_per_vertex_albedo_input_data(__in                                           __notnull ocl_context   context,
                                                                                                                            __in                                                     sh_components n_result_sh_components,
                                                                                                                            __in                                                     uint32_t      n_vertices,
                                                                                                                            __in_ecount(n_result_sh_components*n_vertices) __notnull const float*  albedo,
                                                                                                                            __in_ecount(3*n_vertices)                      __notnull const float*  normals,
                                                                                                                            __in                                                     uint32_t      n_samples,
                                                                                                                            __in                                                     cl_mem        visibility_cl_buffer,
                                                                                                                            __in                                           __notnull cl_mem        result_cl_buffer);

    /** TODO. Remember to call sh_tools_deinit() if you use this function.
     *
     *  @param visibility_cl_buffer Must provide extended float2 info!
     **/
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_diffuse_shadowed_interreflected_transfer_per_vertex_albedo_input_data(__in                                            __notnull ocl_context   context,
                                                                                                                                           __in                                                      sh_components n_result_sh_components,
                                                                                                                                           __in                                                      uint32_t      n_vertices,
                                                                                                                                           __in_ecount(n_result_sh_components*n_vertices) __notnull  const float*  albedo,
                                                                                                                                           __in_ecount(3*n_vertices)                      __notnull  const float*  normals,
                                                                                                                                           __in                                                      uint32_t      n_samples,
                                                                                                                                           __in                                                      cl_mem        visibility_cl_buffer,
                                                                                                                                           __in                                                      cl_mem        triangle_hit_data_buffer,
                                                                                                                                           __in                                           __notnull  cl_mem        result_cl_buffer);

    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    RENDERING_CONTEXT_CALL PUBLIC EMERALD_API bool sh_tools_generate_sh_opencl(__in __notnull   ocl_context                            context,
                                                                               __in __notnull   cl_mem                                 input_data,
                                                                               __in __notnull   cl_mem                                 input_sh_coeffs_data,      /* input sh coeffs data */
                                                                               __in __notnull   cl_mem                                 input_sh_coeffs_unit_vecs, /* input sh coeffs unit vector data */
                                                                               __in __maybenull cl_mem                                 input_prev_pass_data,      /* optional, only used for interreflected diffuse transfer */
                                                                               __in __notnull   cl_mem                                 output_sh_coeffs,          /* result sh coeffs data */
                                                                               __in             uint32_t                               n_bands,
                                                                               __in             uint32_t                               n_samples,
                                                                               __in             uint32_t                               n_vertices,
                                                                               __in __maybenull PFNSHTOOLSGENERATESHOPENCLCALLBACKPROC on_processed_vertex_index_update_callback_proc,
                                                                               __in __maybenull void*                                  on_processed_vertex_index_update_callback_user_arg,
                                                                               __in             sh_components                          n_result_sh_components,
                                                                               __in             sh_integration_logic                   result_integration_logic);

    /** TODO */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_unshadowed_transfer_constant_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                         __in uint32_t      n_normals);
    
    /** TODO */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_unshadowed_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                           __in uint32_t      n_vertices);

    /** TODO */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_transfer_constant_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                       __in uint32_t      n_normals,
                                                                                                       __in uint32_t      n_samples,
                                                                                                       __in uint32_t      n_sh_bands);

    /** TODO */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                         __in uint32_t      n_vertices,
                                                                                                         __in uint32_t      n_samples);

    /** TODO */
    PUBLIC EMERALD_API uint32_t sh_tools_get_diffuse_shadowed_interreflected_transfer_per_vertex_albedo_input_data_size(__in sh_components n_result_sh_components,
                                                                                                                        __in uint32_t      n_vertices,
                                                                                                                        __in uint32_t      n_samples);

    /** TODO */
    PUBLIC EMERALD_API sh_projector_property_datatype sh_tools_get_sh_projector_property_datatype(__in sh_projector_property);

    /** TODO */
    PUBLIC EMERALD_API system_hashed_ansi_string sh_tools_get_sh_projector_property_string(__in sh_projector_property);

    /** TODO */
    PUBLIC EMERALD_API system_hashed_ansi_string sh_tools_get_sh_projection_type_string(__in sh_projection_type);

    /** TODO. Remember to call sh_tools_deinit() if you use this function. */
    PUBLIC EMERALD_API bool sh_tools_sum_sh(__in __notnull        ocl_context   context,
                                            __in                  uint32_t      n_passes,
                                            __in_ecount(n_passes) cl_mem*       pass_buffers,
                                            __in                  uint32_t      n_sh_bands,
                                            __in                  sh_components n_sh_components,
                                            __in                  uint32_t      n_sh_samples);
#endif

/** Converts Spherical Harmonics data back to non-SH representation. Assumes RGB layout of input SH data.
 *
 *  @param sh_data_tbo_id
 *  @param n_bands
 *  @param n_channels
 *  @param reference_sphere
 *  @param out_vertex_color_bo_id
 **/
RENDERING_CONTEXT_CALL PUBLIC EMERALD_API void sh_tools_generate_preview(__in  __notnull ogl_context            context,
                                                                         __in  __notnull ogl_texture            input_light_sh_data_tbo,
                                                                         __in            uint32_t               n_bands,
                                                                         __in  __notnull procedural_mesh_sphere reference_sphere,
                                                                         __out __notnull GLuint*                out_vertex_color_bo_id);
                                              
#endif /* SH_TOOLS_H */