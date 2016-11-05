/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "mesh/mesh_marchingcubes.h"
#include "ogl/ogl_context.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_present_task.h"
#include "ral/ral_program.h"
#include "ral/ral_program_block_buffer.h"
#include "ral/ral_shader.h"
#include "scene/scene_material.h"
#include "system/system_log.h"


typedef struct _mesh_marchingcubes
{
    ral_context               context;  /* DO NOT release */
    unsigned int              grid_size[3];
    float                     isolevel;
    scene_material            material;
    mesh_material             material_gpu;
    mesh                      mesh_instance;
    system_hashed_ansi_string name;
    bool                      needs_polygonization;
    unsigned int              polygonized_data_size_reduction;
    ral_buffer                scalar_data_bo;

    unsigned int    indirect_draw_call_args_bo_count_arg_offset;
    ral_buffer      indirect_draw_call_args_bo;

    ral_buffer      polygonized_data_bo;
    ral_buffer      polygonized_data_normals_subregion_bo;
    ral_buffer      polygonized_data_vertices_subregion_bo;

    ral_present_task present_task_with_compute;
    ral_present_task present_task_wo_compute;

    ral_program              po_scalar_field_polygonizer;
    ral_program_block_buffer po_scalar_field_polygonizer_data_ub;
    uint32_t                 po_scalar_field_polygonizer_data_ub_bo_size;
    uint32_t                 po_scalar_field_polygonizer_data_ub_isolevel_offset;
    unsigned int             po_scalar_field_polygonizer_global_wg_size[3];
    ral_program_block_buffer po_scalar_field_polygonizer_indirect_draw_call_count_ssb;
    ral_program_block_buffer po_scalar_field_polygonizer_precomputed_tables_ub;
    uint32_t                 po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset;
    uint32_t                 po_scalar_field_polygonizer_precomputed_tables_ub_bo_size;
    uint32_t                 po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset;
    ral_program_block_buffer po_scalar_field_polygonizer_result_data_ssb;
    ral_program_block_buffer po_scalar_field_polygonizer_scalar_field_data_ssb;

    REFCOUNT_INSERT_VARIABLES;


    explicit _mesh_marchingcubes()
    {
        memset(grid_size,
               0,
               sizeof(grid_size) );

        context                         = nullptr;
        isolevel                        = 0.0f;
        material                        = nullptr;
        mesh_instance                   = nullptr;
        needs_polygonization            = true;
        polygonized_data_size_reduction = 0;
        scalar_data_bo                  = nullptr;

        indirect_draw_call_args_bo_count_arg_offset = 0;
        indirect_draw_call_args_bo                  = nullptr;

        polygonized_data_bo                    = nullptr;
        polygonized_data_normals_subregion_bo  = nullptr;
        polygonized_data_vertices_subregion_bo = nullptr;

        po_scalar_field_polygonizer                                                = nullptr;
        po_scalar_field_polygonizer_data_ub                                        = nullptr;
        po_scalar_field_polygonizer_data_ub_bo_size                                = 0;
        po_scalar_field_polygonizer_data_ub_isolevel_offset                        = -1;
        po_scalar_field_polygonizer_data_ub                                        = nullptr;
        po_scalar_field_polygonizer_indirect_draw_call_count_ssb                   = nullptr;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = -1;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_size                  = 0;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = -1;
        po_scalar_field_polygonizer_precomputed_tables_ub                          = nullptr;
        po_scalar_field_polygonizer_result_data_ssb                                = nullptr;
        po_scalar_field_polygonizer_scalar_field_data_ssb                          = nullptr;
    }
} _mesh_marchingcubes;

/* Forward declarations */
PRIVATE void                      _mesh_marchingcubes_get_aabb                        (const void*                 user_arg,
                                                                                       float*                      out_aabb_model_vec3_min,
                                                                                       float*                      out_aabb_model_vec3_max);
PRIVATE system_hashed_ansi_string _mesh_marchingcubes_get_polygonizer_name            (_mesh_marchingcubes*        mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_get_token_key_value_arrays      (const _mesh_marchingcubes*  mesh_ptr,
                                                                                       system_hashed_ansi_string** out_token_key_array_ptr,
                                                                                       system_hashed_ansi_string** out_token_value_array_ptr,
                                                                                       unsigned int*               out_n_token_key_value_pairs_ptr,
                                                                                       unsigned int*               out_global_wg_size_uvec3_ptr);
PRIVATE void                      _mesh_marchingcubes_init                            (_mesh_marchingcubes*        mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_init_mesh_instance              (_mesh_marchingcubes*        mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_init_polygonizer_po             (_mesh_marchingcubes*        mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_init_present_tasks              (_mesh_marchingcubes*        mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_release                         (void*                       arg);
PRIVATE void                      _mesh_marchingcubes_update_ub_data_cpu_task_callback(void*                       mesh_raw_ptr);

/** Precomputed tables (adapted from http://paulbourke.net/geometry/polygonise/) */
PRIVATE const int _edge_table[256]={0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
                                    0x80c, 0x905, 0xa0f, 0xb06, 0xc0a, 0xd03, 0xe09, 0xf00,
                                    0x190, 0x99 , 0x393, 0x29a, 0x596, 0x49f, 0x795, 0x69c,
                                    0x99c, 0x895, 0xb9f, 0xa96, 0xd9a, 0xc93, 0xf99, 0xe90,
                                    0x230, 0x339, 0x33 , 0x13a, 0x636, 0x73f, 0x435, 0x53c,
                                    0xa3c, 0xb35, 0x83f, 0x936, 0xe3a, 0xf33, 0xc39, 0xd30,
                                    0x3a0, 0x2a9, 0x1a3, 0xaa , 0x7a6, 0x6af, 0x5a5, 0x4ac,
                                    0xbac, 0xaa5, 0x9af, 0x8a6, 0xfaa, 0xea3, 0xda9, 0xca0,
                                    0x460, 0x569, 0x663, 0x76a, 0x66 , 0x16f, 0x265, 0x36c,
                                    0xc6c, 0xd65, 0xe6f, 0xf66, 0x86a, 0x963, 0xa69, 0xb60,
                                    0x5f0, 0x4f9, 0x7f3, 0x6fa, 0x1f6, 0xff , 0x3f5, 0x2fc,
                                    0xdfc, 0xcf5, 0xfff, 0xef6, 0x9fa, 0x8f3, 0xbf9, 0xaf0,
                                    0x650, 0x759, 0x453, 0x55a, 0x256, 0x35f, 0x55 , 0x15c,
                                    0xe5c, 0xf55, 0xc5f, 0xd56, 0xa5a, 0xb53, 0x859, 0x950,
                                    0x7c0, 0x6c9, 0x5c3, 0x4ca, 0x3c6, 0x2cf, 0x1c5, 0xcc ,
                                    0xfcc, 0xec5, 0xdcf, 0xcc6, 0xbca, 0xac3, 0x9c9, 0x8c0,
                                    0x8c0, 0x9c9, 0xac3, 0xbca, 0xcc6, 0xdcf, 0xec5, 0xfcc,
                                    0xcc , 0x1c5, 0x2cf, 0x3c6, 0x4ca, 0x5c3, 0x6c9, 0x7c0,
                                    0x950, 0x859, 0xb53, 0xa5a, 0xd56, 0xc5f, 0xf55, 0xe5c,
                                    0x15c, 0x55 , 0x35f, 0x256, 0x55a, 0x453, 0x759, 0x650,
                                    0xaf0, 0xbf9, 0x8f3, 0x9fa, 0xef6, 0xfff, 0xcf5, 0xdfc,
                                    0x2fc, 0x3f5, 0xff , 0x1f6, 0x6fa, 0x7f3, 0x4f9, 0x5f0,
                                    0xb60, 0xa69, 0x963, 0x86a, 0xf66, 0xe6f, 0xd65, 0xc6c,
                                    0x36c, 0x265, 0x16f, 0x66 , 0x76a, 0x663, 0x569, 0x460,
                                    0xca0, 0xda9, 0xea3, 0xfaa, 0x8a6, 0x9af, 0xaa5, 0xbac,
                                    0x4ac, 0x5a5, 0x6af, 0x7a6, 0xaa , 0x1a3, 0x2a9, 0x3a0,
                                    0xd30, 0xc39, 0xf33, 0xe3a, 0x936, 0x83f, 0xb35, 0xa3c,
                                    0x53c, 0x435, 0x73f, 0x636, 0x13a, 0x33 , 0x339, 0x230,
                                    0xe90, 0xf99, 0xc93, 0xd9a, 0xa96, 0xb9f, 0x895, 0x99c,
                                    0x69c, 0x795, 0x49f, 0x596, 0x29a, 0x393, 0x99 , 0x190,
                                    0xf00, 0xe09, 0xd03, 0xc0a, 0xb06, 0xa0f, 0x905, 0x80c,
                                    0x70c, 0x605, 0x50f, 0x406, 0x30a, 0x203, 0x109, 0x0};

PRIVATE const int _triangle_table[256 * 15] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  1,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  8,  3,  9,  8,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  1,  2,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  2,  10, 0,  2,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               2,  8,  3,  2,  10, 8,  10, 9,  8,  -1, -1, -1, -1, -1, -1,
                                               3,  11, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  11, 2,  8,  11, 0,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  9,  0,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  11, 2,  1,  9,  11, 9,  8,  11, -1, -1, -1, -1, -1, -1,
                                               3,  10, 1,  11, 10, 3,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  10, 1,  0,  8,  10, 8,  11, 10, -1, -1, -1, -1, -1, -1,
                                               3,  9,  0,  3,  11, 9,  11, 10, 9,  -1, -1, -1, -1, -1, -1,
                                               9,  8,  10, 10, 8,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  7,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  3,  0,  7,  3,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  1,  9,  8,  4,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  1,  9,  4,  7,  1,  7,  3,  1,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, 8,  4,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               3,  4,  7,  3,  0,  4,  1,  2,  10, -1, -1, -1, -1, -1, -1,
                                               9,  2,  10, 9,  0,  2,  8,  4,  7,  -1, -1, -1, -1, -1, -1,
                                               2,  10, 9,  2,  9,  7,  2,  7,  3,  7,  9,  4,  -1, -1, -1,
                                               8,  4,  7,  3,  11, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               11, 4,  7,  11, 2,  4,  2,  0,  4,  -1, -1, -1, -1, -1, -1,
                                               9,  0,  1,  8,  4,  7,  2,  3,  11, -1, -1, -1, -1, -1, -1,
                                               4,  7,  11, 9,  4,  11, 9,  11, 2,  9,  2,  1,  -1, -1, -1,
                                               3,  10, 1,  3,  11, 10, 7,  8,  4,  -1, -1, -1, -1, -1, -1,
                                               1,  11, 10, 1,  4,  11, 1,  0,  4,  7,  11,  4, -1, -1, -1,
                                               4,  7,  8,  9,  0,  11, 9,  11, 10, 11, 0,  3,  -1, -1, -1,
                                               4,  7,  11, 4,  11, 9,  9,  11, 10, -1, -1, -1, -1, -1, -1,
                                               9,  5,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  5,  4,  0,  8,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  5,  4,  1,  5,  0,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               8,  5,  4,  8,  3,  5,  3,  1,  5,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, 9,  5,  4, -1, -1,  -1, -1, -1, -1, -1, -1, -1,
                                               3,  0,  8,  1,  2,  10, 4,  9,  5,  -1, -1, -1, -1, -1, -1,
                                               5,  2,  10, 5,  4,  2,  4,  0,  2,  -1, -1, -1, -1, -1, -1,
                                               2,  10, 5,  3,  2,  5,  3,  5,  4,  3,  4,  8,  -1, -1, -1,
                                               9,  5,  4,  2,  3,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  11, 2,  0,  8,  11, 4,  9,  5,  -1, -1, -1, -1, -1, -1,
                                               0,  5,  4,  0,  1,  5,  2,  3,  11, -1, -1, -1, -1, -1, -1,
                                               2,  1,  5,  2,  5,  8,  2,  8,  11, 4,  8,  5,  -1, -1, -1,
                                               10, 3,  11, 10, 1,  3,  9,  5,  4,  -1, -1, -1, -1, -1, -1,
                                               4,  9,  5,  0,  8,  1,  8,  10, 1,  8,  11, 10, -1, -1, -1,
                                               5,  4,  0,  5,  0,  11, 5,  11, 10, 11, 0,  3,  -1, -1, -1,
                                               5,  4,  8,  5,  8,  10, 10, 8,  11, -1, -1, -1, -1, -1, -1,
                                               9,  7,  8,  5,  7,  9,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  3,  0,  9,  5,  3,  5,  7,  3,  -1, -1, -1, -1, -1, -1,
                                               0,  7,  8,  0,  1,  7,  1,  5,  7,  -1, -1, -1, -1, -1, -1,
                                               1,  5,  3,  3,  5,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  7,  8,  9,  5,  7,  10, 1,  2,  -1, -1, -1, -1, -1, -1,
                                               10, 1,  2,  9,  5,  0,  5,  3,  0,  5,  7,  3,  -1, -1, -1,
                                               8,  0,  2,  8,  2,  5,  8,  5,  7,  10, 5,  2,  -1, -1, -1,
                                               2,  10, 5,  2,  5,  3,  3,  5,  7,  -1, -1, -1, -1, -1, -1,
                                               7,  9,  5,  7,  8,  9,  3,  11, 2,  -1, -1, -1, -1, -1, -1,
                                               9,  5,  7,  9,  7,  2,  9,  2,  0,  2,  7,  11, -1, -1, -1,
                                               2,  3,  11, 0,  1,  8,  1,  7,  8,  1,  5,  7,  -1, -1, -1,
                                               11, 2,  1,  11, 1,  7,  7,  1,  5,  -1, -1, -1, -1, -1, -1,
                                               9,  5,  8,  8,  5,  7,  10, 1,  3,  10, 3,  11, -1, -1, -1,
                                               5,  7,  0,  5,  0,  9,  7,  11, 0,  1,  0,  10, 11, 10,  0,
                                               11, 10, 0,  11, 0,  3,  10, 5,  0,  8,  0,  7,  5,  7,   0,
                                               11, 10, 5,  7,  11, 5,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               10, 6,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  5,  10, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  0,  1,  5,  10, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  8,  3,  1,  9,  8,  5,  10, 6,  -1, -1, -1, -1, -1, -1,
                                               1,  6,  5,  2,  6,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  6,  5,  1,  2,  6,  3,  0,  8,  -1, -1, -1, -1, -1, -1,
                                               9,  6,  5,  9,  0,  6,  0,  2,  6,  -1, -1, -1, -1, -1, -1,
                                               5,  9,  8,  5,  8,  2,  5,  2,  6,  3,  2,  8,  -1, -1, -1,
                                               2,  3,  11, 10, 6,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               11, 0,  8,  11, 2,  0,  10, 6,  5,  -1, -1, -1, -1, -1, -1,
                                               0,  1,  9,  2,  3,  11, 5,  10, 6,  -1, -1, -1, -1, -1, -1,
                                               5,  10, 6,  1,  9,  2,  9,  11, 2,  9,  8,  11, -1, -1, -1,
                                               6,  3,  11, 6,  5,  3,  5,  1,  3,  -1, -1, -1, -1, -1, -1,
                                               0,  8,  11, 0,  11, 5,  0,  5,  1,  5,  11, 6,  -1, -1, -1,
                                               3,  11, 6,  0,  3,  6,  0,  6,  5,  0,  5,  9,  -1, -1, -1,
                                               6,  5,  9,  6,  9,  11, 11, 9,  8,  -1, -1, -1, -1, -1, -1,
                                               5,  10, 6,  4,  7,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  3,  0,  4,  7,  3,  6,  5,  10, -1, -1, -1, -1, -1, -1,
                                               1,  9,  0,  5,  10, 6,  8,  4,  7,  -1, -1, -1, -1, -1, -1,
                                               10, 6,  5,  1,  9,  7,  1,  7,  3,  7,  9,  4,  -1, -1, -1,
                                               6,  1,  2,  6,  5,  1,  4,  7,  8,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  5,  5,  2,  6,  3,  0,  4,  3,  4,  7,  -1, -1, -1,
                                               8,  4,  7,  9,  0,  5,  0,  6,  5,  0,  2,  6,  -1, -1, -1,
                                               7,  3,  9,  7,  9,  4,  3,  2,  9,  5,  9,  6,  2,  6,   9,
                                               3,  11, 2,  7,  8,  4,  10, 6,  5,  -1, -1, -1, -1, -1, -1,
                                               5,  10, 6,  4,  7,  2,  4,  2,  0,  2,  7,  11, -1, -1, -1,
                                               0,  1,  9,  4,  7,  8,  2,  3,  11, 5,  10, 6,  -1, -1, -1,
                                               9,  2,  1,  9,  11, 2,  9,  4,  11, 7,  11, 4,  5,  10,  6,
                                               8,  4,  7,  3,  11, 5,  3,  5,  1,  5,  11, 6, -1,  -1, -1,
                                               5,  1,  11, 5,  11, 6,  1,  0,  11, 7,  11, 4, 0,   4,  11,
                                               0,  5,  9,  0,  6,  5,  0,  3,  6,  11, 6,  3, 8,   4,   7,
                                               6,  5,  9,  6,  9,  11, 4,  7,  9,  7,  11, 9, -1, -1,  -1,
                                               10, 4,  9,  6,  4,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  10, 6,  4,  9,  10, 0,  8,  3,  -1, -1, -1, -1, -1, -1,
                                               10, 0,  1,  10, 6,  0,  6,  4,  0,  -1, -1, -1, -1, -1, -1,
                                               8,  3,  1,  8,  1,  6,  8,  6,  4,  6,  1,  10, -1, -1, -1,
                                               1,  4,  9,  1,  2,  4,  2,  6,  4,  -1, -1, -1, -1, -1, -1,
                                               3,  0,  8,  1,  2,  9,  2,  4,  9,  2,  6,  4,  -1, -1, -1,
                                               0,  2,  4,  4,  2,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               8,  3,  2,  8,  2,  4,  4,  2,  6,  -1, -1, -1, -1, -1, -1,
                                               10, 4,  9,  10, 6,  4,  11, 2,  3,  -1, -1, -1, -1, -1, -1,
                                               0,  8,  2,  2,  8,  11, 4,  9,  10, 4,  10, 6,  -1, -1, -1,
                                               3,  11, 2,  0,  1,  6,  0,  6,  4,  6,  1,  10, -1, -1, -1,
                                               6,  4,  1,  6,  1,  10, 4,  8,  1,  2,  1,  11, 8,  11,  1,
                                               9,  6,  4,  9,  3,  6,  9,  1,  3,  11, 6,  3,  -1, -1, -1,
                                               8,  11, 1,  8,  1,  0,  11, 6,  1,  9,  1,  4,  6,  4,   1,
                                               3,  11, 6,  3,  6,  0,  0,  6,  4,  -1, -1, -1, -1, -1, -1,
                                               6,  4,  8,  11, 6,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               7,  10, 6,  7,  8,  10, 8,  9,  10, -1, -1, -1, -1, -1, -1,
                                               0,  7,  3,  0,  10, 7,  0,  9,  10, 6,  7,  10, -1, -1, -1,
                                               10, 6,  7,  1,  10, 7,  1,  7,  8,  1,  8,  0, -1, -1,  -1,
                                               10, 6,  7,  10, 7,  1,  1,  7,  3,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  6,  1,  6,  8,  1,  8,  9,  8,  6,  7,  -1, -1, -1,
                                               2,  6,  9,  2,  9,  1,  6,  7,  9,  0,  9,  3,  7,  3,   9,
                                               7,  8,  0,  7,  0,  6,  6,  0,  2,  -1, -1, -1, -1, -1, -1,
                                               7,  3,  2,  6,  7,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               2,  3,  11, 10, 6,  8,  10, 8,  9,  8,  6,  7,  -1, -1, -1,
                                               2,  0,  7,  2,  7,  11, 0,  9,  7,  6,  7,  10,  9,  10, 7,
                                               1,  8,  0,  1,  7,  8,  1,  10, 7,  6,  7,  10,  2,  3, 11,
                                               11, 2,  1,  11, 1,  7,  10, 6,  1,  6,  7,  1,  -1, -1, -1,
                                               8,  9,  6,  8,  6,  7,  9,  1,  6,  11, 6,  3,  1,  3,   6,
                                               0,  9,  1,  11, 6,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               7,  8,  0,  7,  0,  6,  3,  11, 0,  11, 6,  0,  -1, -1, -1,
                                               7,  11, 6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               7,  6,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               3,  0,  8,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  1,  9,  11, 7,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               8,  1,  9,  8,  3,  1,  11, 7,  6,  -1, -1, -1, -1, -1, -1,
                                               10, 1,  2,  6,  11, 7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, 3,  0,  8,  6,  11, 7,  -1, -1, -1, -1, -1, -1,
                                               2,  9,  0,  2,  10, 9,  6,  11, 7,  -1, -1, -1, -1, -1, -1,
                                               6,  11, 7,  2,  10, 3,  10, 8,  3,  10, 9,  8,  -1, -1, -1,
                                               7,  2,  3,  6,  2,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               7,  0,  8,  7,  6,  0,  6,  2,  0,  -1, -1, -1, -1, -1, -1,
                                               2,  7,  6,  2,  3,  7,  0,  1,  9,  -1, -1, -1, -1, -1, -1,
                                               1,  6,  2,  1,  8,  6,  1,  9,  8,  8,  7,  6,  -1, -1, -1,
                                               10, 7,  6,  10, 1,  7,  1,  3,  7,  -1, -1, -1, -1, -1, -1,
                                               10, 7,  6,  1,  7,  10, 1,  8,  7,  1,  0,  8,  -1, -1, -1,
                                               0,  3,  7,  0,  7,  10, 0,  10, 9,  6,  10, 7,  -1, -1, -1,
                                               7,  6,  10, 7,  10, 8,  8,  10, 9,  -1, -1, -1, -1, -1, -1,
                                               6,  8,  4,  11, 8,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               3,  6,  11, 3,  0,  6,  0,  4,  6,  -1, -1, -1, -1, -1, -1,
                                               8,  6,  11, 8,  4,  6,  9,  0,  1,  -1, -1, -1, -1, -1, -1,
                                               9,  4,  6,  9,  6,  3,  9,  3,  1,  11, 3,  6,  -1, -1, -1,
                                               6,  8,  4,  6,  11, 8,  2,  10, 1,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, 3,  0,  11, 0,  6,  11, 0,  4,  6,  -1, -1, -1,
                                               4,  11, 8,  4,  6,  11, 0,  2,  9,  2,  10, 9,  -1, -1, -1,
                                               10, 9,  3,  10, 3,  2,  9,  4,  3,  11, 3,  6,  4,  6,   3,
                                               8,  2,  3,  8,  4,  2,  4,  6,  2,  -1, -1, -1, -1, -1, -1,
                                               0,  4,  2,  4,  6,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  9,  0,  2,  3,  4,  2,  4,  6,  4,  3,  8,  -1, -1, -1,
                                               1,  9,  4,  1,  4,  2,  2,  4,  6,  -1, -1, -1, -1, -1, -1,
                                               8,  1,  3,  8,  6,  1,  8,  4,  6,  6,  10, 1,  -1, -1, -1,
                                               10, 1,  0,  10, 0,  6,  6,  0,  4,  -1, -1, -1, -1, -1, -1,
                                               4,  6,  3,  4,  3,  8,  6,  10, 3,  0,  3,  9,  10,  9,  3,
                                               10, 9,  4,  6,  10, 4,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  9,  5,  7,  6,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  4,  9,  5,  11, 7,  6,  -1, -1, -1, -1, -1, -1,
                                               5,  0,  1,  5,  4,  0,  7,  6,  11, -1, -1, -1, -1, -1, -1,
                                               11, 7,  6,  8,  3,  4,  3,  5,  4,  3,  1,  5,  -1, -1, -1,
                                               9,  5,  4,  10, 1,  2,  7,  6,  11, -1, -1, -1, -1, -1, -1,
                                               6,  11, 7,  1,  2,  10, 0,  8,  3,  4,  9,  5,  -1, -1, -1,
                                               7,  6,  11, 5,  4,  10, 4,  2,  10, 4,  0,  2,  -1, -1, -1,
                                               3,  4,  8,  3,  5,  4,  3,  2,  5,  10, 5,  2,  11, 7,   6,
                                               7,  2,  3,  7,  6,  2,  5,  4,  9,  -1, -1, -1, -1, -1, -1,
                                               9,  5,  4,  0,  8,  6,  0,  6,  2,  6,  8,  7,  -1, -1, -1,
                                               3,  6,  2,  3,  7,  6,  1,  5,  0,  5,  4,  0,  -1, -1, -1,
                                               6,  2,  8,  6,  8,  7,  2,  1,  8,  4,  8,  5,  1,  5,   8,
                                               9,  5,  4,  10, 1,  6,  1,  7,  6,  1,  3,  7,  -1, -1, -1,
                                               1,  6,  10, 1,  7,  6,  1,  0,  7,  8,  7,  0,  9,  5,   4,
                                               4,  0,  10, 4,  10, 5,  0,  3,  10, 6,  10, 7,  3,  7,  10,
                                               7,  6,  10, 7,  10, 8,  5,  4,  10, 4,  8,  10, -1, -1, -1,
                                               6,  9,  5,  6,  11, 9,  11, 8,  9,  -1, -1, -1, -1, -1, -1,
                                               3,  6,  11, 0,  6,  3,  0,  5,  6,  0,  9,  5,  -1, -1, -1,
                                               0,  11, 8,  0,  5,  11, 0,  1,  5,  5,  6,  11, -1, -1, -1,
                                               6,  11, 3,  6,  3,  5,  5,  3,  1,  -1, -1, -1, -1, -1, -1,
                                               1,  2,  10, 9,  5,  11, 9,  11, 8,  11, 5,  6,  -1, -1, -1,
                                               0,  11, 3,  0,  6,  11, 0,  9,  6,  5,  6,  9,  1,  2,  10,
                                               11, 8,  5,  11, 5,  6,  8,  0,  5,  10, 5,  2,  0,  2,   5,
                                               6,  11, 3,  6,  3,  5,  2,  10, 3,  10, 5,  3,  -1, -1, -1,
                                               5,  8,  9,  5,  2,  8,  5,  6,  2,  3,  8,  2,  -1, -1, -1,
                                               9,  5,  6,  9,  6,  0,  0,  6,  2,  -1, -1, -1, -1, -1, -1,
                                               1,  5,  8,  1,  8,  0,  5,  6,  8,  3,  8,  2,  6,  2,   8,
                                               1,  5,  6,  2,  1,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  3,  6,  1,  6,  10, 3,  8,  6,  5,  6,  9,  8,  9,   6,
                                               10, 1,  0,  10, 0,  6,  9,  5,  0,  5,  6,  0,  -1, -1, -1,
                                               0,  3,  8,  5,  6,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               10, 5,  6,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               11, 5,  10, 7,  5,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               11, 5,  10, 11, 7,  5,  8,  3,  0,  -1, -1, -1, -1, -1, -1,
                                               5,  11, 7,  5,  10, 11, 1,  9,  0,  -1, -1, -1, -1, -1, -1,
                                               10, 7,  5,  10, 11, 7,  9,  8,  1,  8,  3,  1,  -1, -1, -1,
                                               11, 1,  2,  11, 7,  1,  7,  5,  1,  -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  1,  2,  7,  1,  7,  5,  7,  2,  11, -1, -1, -1,
                                               9,  7,  5,  9,  2,  7,  9,  0,  2,  2,  11, 7,  -1, -1, -1,
                                               7,  5,  2,  7,  2,  11, 5,  9,  2,  3,  2,  8,  9,  8,   2,
                                               2,  5,  10, 2,  3,  5,  3,  7,  5,  -1, -1, -1, -1, -1, -1,
                                               8,  2,  0,  8,  5,  2,  8,  7,  5,  10, 2,  5,  -1, -1, -1,
                                               9,  0,  1,  5,  10, 3,  5,  3,  7,  3,  10, 2,  -1, -1, -1,
                                               9,  8,  2,  9,  2,  1,  8,  7,  2,  10, 2,  5,  7,  5,   2,
                                               1,  3,  5,  3,  7,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  8,  7,  0,  7,  1,  1,  7,  5,  -1, -1, -1, -1, -1, -1,
                                               9,  0,  3,  9,  3,  5,  5,  3,  7,  -1, -1, -1, -1, -1, -1,
                                               9,  8,  7,  5,  9,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               5,  8,  4,  5,  10, 8,  10, 11, 8,  -1, -1, -1, -1, -1, -1,
                                               5,  0,  4,  5,  11, 0,  5,  10, 11, 11, 3,  0,  -1, -1, -1,
                                               0,  1,  9,  8,  4,  10, 8,  10, 11, 10, 4,  5,  -1, -1, -1,
                                               10, 11, 4,  10, 4,  5,  11, 3,  4,  9,  4,  1,  3,   1,  4,
                                               2,  5,  1,  2,  8,  5,  2,  11, 8,  4,  5,  8,  -1, -1, -1,
                                               0,  4,  11, 0,  11, 3,  4,  5,  11, 2,  11, 1,  5,   1, 11,
                                               0,  2,  5,  0,  5,  9,  2,  11, 5,  4,  5,  8,  11,  8,  5,
                                               9,  4,  5,  2,  11, 3,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               2,  5,  10, 3,  5,  2,  3,  4,  5,  3,  8,  4,  -1, -1, -1,
                                               5,  10, 2,  5,  2,  4,  4,  2,  0,  -1, -1, -1, -1, -1, -1,
                                               3,  10, 2,  3,  5,  10, 3,  8,  5,  4,  5,  8,  0,  1,   9,
                                               5,  10, 2,  5,  2,  4,  1,  9,  2,  9,  4,  2, -1, -1,  -1,
                                               8,  4,  5,  8,  5,  3,  3,  5,  1,  -1, -1, -1, -1, -1, -1,
                                               0,  4,  5,  1,  0,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               8,  4,  5,  8,  5,  3,  9,  0,  5,  0,  3,  5,  -1, -1, -1,
                                               9,  4,  5,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  11, 7,  4,  9,  11, 9,  10, 11, -1, -1, -1, -1, -1, -1,
                                               0,  8,  3,  4,  9,  7,  9,  11, 7,  9,  10, 11, -1, -1, -1,
                                               1,  10, 11, 1,  11, 4,  1,  4,  0,  7,  4,  11, -1, -1, -1,
                                               3,  1,  4,  3,  4,  8,  1,  10, 4,  7,  4,  11, 10, 11,  4,
                                               4,  11, 7,  9,  11, 4,  9,  2,  11, 9,  1,  2,  -1, -1, -1,
                                               9,  7,  4,  9,  11, 7,  9,  1,  11, 2,  11, 1,  0,  8,   3,
                                               11, 7,  4,  11, 4,  2,  2,  4,  0,  -1, -1, -1, -1, -1, -1,
                                               11, 7,  4,  11, 4,  2,  8,  3,  4,  3,  2,  4,  -1, -1, -1,
                                               2,  9,  10, 2,  7,  9,  2,  3,  7,  7,  4,  9,  -1, -1, -1,
                                               9,  10, 7,  9,  7,  4,  10, 2,  7,  8,  7,  0,  2,  0,   7,
                                               3,  7,  10, 3,  10, 2,  7,  4,  10, 1,  10, 0,  4,  0,  10,
                                               1,  10, 2,  8,  7,  4,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  9,  1,  4,  1,  7,  7,  1,  3,  -1, -1, -1, -1, -1, -1,
                                               4,  9,  1,  4,  1,  7,  0,  8,  1,  8,  7,  1,  -1, -1, -1,
                                               4,  0,  3,  7,  4,  3,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               4,  8,  7,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               9,  10, 8,  10, 11, 8,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               3,  0,  9,  3,  9,  11, 11, 9,  10, -1, -1, -1, -1, -1, -1,
                                               0,  1,  10, 0,  10, 8,  8,  10, 11, -1, -1, -1, -1, -1, -1,
                                               3,  1,  10, 11, 3,  10, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  2,  11, 1,  11, 9,  9,  11, 8,  -1, -1, -1, -1, -1, -1,
                                               3,  0,  9,  3,  9,  11, 1,  2,  9,  2,  11,  9, -1, -1, -1,
                                               0,  2,  11, 8,  0,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               3,  2,  11, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               2,  3,  8,  2,  8,  10, 10, 8,  9,  -1, -1, -1, -1, -1, -1,
                                               9,  10, 2,  0,  9,  2,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               2,  3,  8,  2,  8,  10, 0,  1,  8,  1,  10,  8, -1, -1, -1,
                                               1,  10, 2,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               1,  3,  8,  9,  1,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  9,  1,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               0,  3,  8,  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                                               -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

static const char* _mesh_marchingcubes_cs_body =
    "#version 430 core\n"
    "\n"
    "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
    "\n"
    "const uint n_ids_per_row    = BLOB_SIZE_X;\n"
    "const uint n_ids_per_slice  = BLOB_SIZE_X * BLOB_SIZE_Y;\n"
    "\n"
    "layout(binding = 0, packed) uniform dataUB\n"
    "{\n"
    "    float isolevel;\n"
    "};\n"
    "\n"
    "layout(binding = 1, std430) buffer indirect_draw_callSSB\n"
    "{\n"
    "    restrict uint indirect_draw_call_count;\n"
    "};\n"
    "\n"
    "layout(binding = 2, std140) uniform precomputed_tablesUB\n"
    "{\n"
    "    int edge_table    [256];\n"
    "    int triangle_table[256 * 15];\n"
    "};\n"
    "\n"
    "layout(binding = 3, std430) writeonly buffer result_dataSSB\n"
    "{\n"
    /* 4 floats: vertex data (model space). We need to include W since some of the vertices need to be discarded.
     * 3 floats: normal data (model space) */
    "    restrict float result_data[];\n"
    "};\n"
    "\n"
    "layout(binding = 4, std430) readonly buffer scalar_field_dataSSB\n"
    "{\n"
    "    restrict vec4 scalar_field[];\n"
    "};\n"
    "\n"
    "void get_interpolated_data(in  vec3  vertex1,\n"
    "                           in  vec3  vertex2,\n"
    "                           in  float vertex1_value,\n"
    "                           in  float vertex2_value,\n"
    "                           in  uint  base_index1,\n"
    "                           in  uint  base_index2,"
    "                           out vec3  result_normal,\n"
    "                           out vec3  result_vertex)\n"
    "{\n"
    /* TODO: Use the improved version? */
    "    uvec3 vertex1_preceding_step_size  = uvec3(clamp(base_index1 - BLOB_SIZE_X / 25 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index1 - BLOB_SIZE_Y / 25 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index1 - BLOB_SIZE_Z / 25 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
    "    uvec3 vertex1_proceeding_step_size = uvec3(clamp(base_index1 + BLOB_SIZE_X / 25 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index1 + BLOB_SIZE_Y / 25 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index1 + BLOB_SIZE_Z / 25 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
    "    uvec3 vertex2_preceding_step_size  = uvec3(clamp(base_index2 - BLOB_SIZE_X / 25 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index2 - BLOB_SIZE_Y / 25 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index2 - BLOB_SIZE_Z / 25 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
    "    uvec3 vertex2_proceeding_step_size = uvec3(clamp(base_index2 + BLOB_SIZE_X / 25 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index2 + BLOB_SIZE_Y / 25 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
    "                                               clamp(base_index2 + BLOB_SIZE_Z / 25 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
    "\n"
    "    vec3 vertex1_scalar_preceeding = vec3(scalar_field[vertex1_preceding_step_size.x  / 4][vertex1_preceding_step_size.x  % 4],\n"
    "                                          scalar_field[vertex1_preceding_step_size.y  / 4][vertex1_preceding_step_size.y  % 4],\n"
    "                                          scalar_field[vertex1_preceding_step_size.z  / 4][vertex1_preceding_step_size.z  % 4]);\n"
    "    vec3 vertex1_scalar_proceeding = vec3(scalar_field[vertex1_proceeding_step_size.x / 4][vertex1_proceeding_step_size.x % 4],\n"
    "                                          scalar_field[vertex1_proceeding_step_size.y / 4][vertex1_proceeding_step_size.y % 4],\n"
    "                                          scalar_field[vertex1_proceeding_step_size.z / 4][vertex1_proceeding_step_size.z % 4]);\n"
    "    vec3 vertex2_scalar_preceeding = vec3(scalar_field[vertex2_preceding_step_size.x  / 4][vertex2_preceding_step_size.x  % 4],\n"
    "                                          scalar_field[vertex2_preceding_step_size.y  / 4][vertex2_preceding_step_size.y  % 4],\n"
    "                                          scalar_field[vertex2_preceding_step_size.z  / 4][vertex2_preceding_step_size.z  % 4]);\n"
    "    vec3 vertex2_scalar_proceeding = vec3(scalar_field[vertex2_proceeding_step_size.x / 4][vertex2_proceeding_step_size.x % 4],\n"
    "                                          scalar_field[vertex2_proceeding_step_size.y / 4][vertex2_proceeding_step_size.y % 4],\n"
    "                                          scalar_field[vertex2_proceeding_step_size.z / 4][vertex2_proceeding_step_size.z % 4]);\n"
    "\n"
    "    vec3 vertex1_normal = (vertex1_scalar_proceeding - vertex1_scalar_preceeding);\n"
    "    vec3 vertex2_normal = (vertex2_scalar_proceeding - vertex2_scalar_preceeding);\n"
    "\n"
    "    if (abs(isolevel      - vertex1_value) < 1e-5 ||\n"
    "        abs(vertex1_value - vertex2_value) < 1e-5)\n"
    "    {\n"
    "        result_normal = normalize(vertex1_normal);\n"
    "        result_vertex = vertex1;\n"
    "    }\n"
    "    else\n"
    "    if (abs(isolevel - vertex2_value) < 1e-5)\n"
    "    {\n"
    "        result_normal = normalize(vertex2_normal);\n"
    "        result_vertex = vertex2;\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        float coeff = (isolevel - vertex1_value) / (vertex2_value - vertex1_value);\n"
    "\n"
    "        result_normal = vertex1_normal + vec3(coeff) * (vertex2_normal - vertex1_normal);\n"
    "        result_vertex = vertex1        + vec3(coeff) * (vertex2        - vertex1);\n"
    "    }\n"
    "}\n"
    "\n"
    "void main()\n"
    "{\n"
    "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
    "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
    "                                            gl_GlobalInvocationID.x);\n"
    "\n"
    /* Extract scalar field values */
    "    const uvec3 cube_xyz = uvec3( global_invocation_id_flat                                % BLOB_SIZE_X,\n"
    "                                 (global_invocation_id_flat /  BLOB_SIZE_X)                % BLOB_SIZE_Y,\n"
    "                                  global_invocation_id_flat / (BLOB_SIZE_X * BLOB_SIZE_Y));\n"
    "\n"
    "    if (cube_xyz.z >= BLOB_SIZE_Z)\n"
    "    {\n"
    "        return;\n"
    "    }\n"
    "\n"
    "    const vec3 cube_x1y1z1         = vec3(cube_xyz) / vec3(BLOB_SIZE_X, BLOB_SIZE_Y, BLOB_SIZE_Z);\n"
    "    const vec3 cube_size           = vec3(1.0)      / vec3(BLOB_SIZE_X, BLOB_SIZE_Y, BLOB_SIZE_Z);\n"
    "    const vec3 cube_aabb_min_model = cube_x1y1z1;\n"
    "    const vec3 cube_aabb_max_model = cube_x1y1z1 + cube_size;\n"
    "\n"
    "    const uvec4 top_plane_ids = uvec4(global_invocation_id_flat,\n"
    "                                      global_invocation_id_flat                   + 1,\n"
    "                                      global_invocation_id_flat + n_ids_per_slice + 1,\n"
    "                                      global_invocation_id_flat + n_ids_per_slice);\n"
    "\n"
    /* The scalar_values vector array holds scalar field values for vertices in the following order: (XZ plane is assumed)
     *
     * [0]: BOTTOM PLANE, top-left     vertex
     * [1]: BOTTOM PLANE, top-right    vertex
     * [2]: BOTTOM PLANE, bottom-right vertex
     * [3]: BOTTOM PLANE, bottom-left  vertex
     * [4]: TOP    PLANE, top-left     vertex
     * [5]: TOP    PLANE, top-right    vertex
     * [6]: TOP    PLANE, bottom-right vertex
     * [7]: TOP    PLANE, bottom-left  vertex
     */
    "    vec4 scalar_values[2];\n"
    "\n"
    "    if (cube_xyz[0] > 0 && cube_xyz[0] < (BLOB_SIZE_X - 1)  &&\n"
    "        cube_xyz[1] > 0 && cube_xyz[1] < (BLOB_SIZE_Y - 1)  &&\n"
    "        cube_xyz[2] > 0 && cube_xyz[2] < (BLOB_SIZE_Z - 1) )\n"
    "    {\n"
    "        scalar_values[1].x = scalar_field[ top_plane_ids[0]                  / 4][ top_plane_ids[0]                  % 4];\n"
    "        scalar_values[1].y = scalar_field[ top_plane_ids[1]                  / 4][ top_plane_ids[1]                  % 4];\n"
    "        scalar_values[1].z = scalar_field[ top_plane_ids[2]                  / 4][ top_plane_ids[2]                  % 4];\n"
    "        scalar_values[1].w = scalar_field[ top_plane_ids[3]                  / 4][ top_plane_ids[3]                  % 4];\n"
    "        scalar_values[0].x = scalar_field[(top_plane_ids[0] + n_ids_per_row) / 4][(top_plane_ids[0] + n_ids_per_row) % 4];\n"
    "        scalar_values[0].y = scalar_field[(top_plane_ids[1] + n_ids_per_row) / 4][(top_plane_ids[1] + n_ids_per_row) % 4];\n"
    "        scalar_values[0].z = scalar_field[(top_plane_ids[2] + n_ids_per_row) / 4][(top_plane_ids[2] + n_ids_per_row) % 4];\n"
    "        scalar_values[0].w = scalar_field[(top_plane_ids[3] + n_ids_per_row) / 4][(top_plane_ids[3] + n_ids_per_row) % 4];\n"
    "    }\n"
    "    else\n"
    "    {\n"
    "        return;\n"
    "    }\n"
    "\n"
    /* Determine edge index */
    "    int edge_index = 0;\n"
    "\n"
    "    if (scalar_values[0].x < isolevel) edge_index |= 1;\n"
    "    if (scalar_values[0].y < isolevel) edge_index |= 2;\n"
    "    if (scalar_values[0].z < isolevel) edge_index |= 4;\n"
    "    if (scalar_values[0].w < isolevel) edge_index |= 8;\n"
    "    if (scalar_values[1].x < isolevel) edge_index |= 16;\n"
    "    if (scalar_values[1].y < isolevel) edge_index |= 32;\n"
    "    if (scalar_values[1].z < isolevel) edge_index |= 64;\n"
    "    if (scalar_values[1].w < isolevel) edge_index |= 128;\n"
    "\n"
    "    if (edge_table[edge_index] == 0)\n"
    "    {\n"
    "        return;\n"
    "    }\n"
    "\n"
    /* Build an array of interpolated vertices for the edge we're processing
     *
     * TOP PLANE:    0->1
     *                  |
     *               3<-2
     *
     * BOTTOM PLANE: 4->5
     *                  |
     *               7<-6
     */
    "          vec3 lerped_normal_list[12];\n"
    "          vec3 lerped_vertex_list[12];\n"
    "    const vec3 vertex_model      [8] =\n"
    "    {\n"
    /* 0: */
    "        vec3(cube_aabb_min_model.x,  cube_aabb_max_model.y,  cube_aabb_min_model.z),\n"
    /* 1: */
    "        vec3(cube_aabb_max_model.xy, cube_aabb_min_model.z),\n"
    /* 2: */
    "             cube_aabb_max_model,\n"
    /* 3: */
    "        vec3(cube_aabb_min_model.x,  cube_aabb_max_model.yz),\n"
    /* 4: */
    "             cube_aabb_min_model,\n"
    /* 5: */
    "        vec3(cube_aabb_max_model.x,  cube_aabb_min_model.yz),\n"
    /* 6: */
    "        vec3(cube_aabb_max_model.x,  cube_aabb_min_model.y,  cube_aabb_max_model.z),\n"
    /* 7: */
    "        vec3(cube_aabb_min_model.xy, cube_aabb_max_model.z),\n"
    "    };\n"
    "\n"
    "    if ((edge_table[edge_index] & 1) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[0],\n"
    "                              vertex_model[1],\n"
    "                              scalar_values[0].x,\n"
    "                              scalar_values[0].y,\n"
    "                              top_plane_ids[0] + n_ids_per_row,\n"
    "                              top_plane_ids[1] + n_ids_per_row,\n"
    "                              lerped_normal_list[0],\n"
    "                              lerped_vertex_list[0]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 2) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[1],\n"
    "                              vertex_model[2],\n"
    "                              scalar_values[0].y,\n"
    "                              scalar_values[0].z,\n"
    "                              top_plane_ids[1] + n_ids_per_row,\n"
    "                              top_plane_ids[2] + n_ids_per_row,\n"
    "                              lerped_normal_list[1],\n"
    "                              lerped_vertex_list[1]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 4) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[2],\n"
    "                              vertex_model[3],\n"
    "                              scalar_values[0].z,\n"
    "                              scalar_values[0].w,\n"
    "                              top_plane_ids[2] + n_ids_per_row,\n"
    "                              top_plane_ids[3] + n_ids_per_row,\n"
    "                              lerped_normal_list[2],\n"
    "                              lerped_vertex_list[2]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 8) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[3],\n"
    "                              vertex_model[0],\n"
    "                              scalar_values[0].w,\n"
    "                              scalar_values[0].x,\n"
    "                              top_plane_ids[3] + n_ids_per_row,\n"
    "                              top_plane_ids[0] + n_ids_per_row,\n"
    "                              lerped_normal_list[3],\n"
    "                              lerped_vertex_list[3]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 16) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[4],\n"
    "                              vertex_model[5],\n"
    "                              scalar_values[1].x,\n"
    "                              scalar_values[1].y,\n"
    "                              top_plane_ids[0],\n"
    "                              top_plane_ids[1],\n"
    "                              lerped_normal_list[4],\n"
    "                              lerped_vertex_list[4]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 32) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[5],\n"
    "                              vertex_model[6],\n"
    "                              scalar_values[1].y,\n"
    "                              scalar_values[1].z,\n"
    "                              top_plane_ids[1],\n"
    "                              top_plane_ids[2],\n"
    "                              lerped_normal_list[5],\n"
    "                              lerped_vertex_list[5]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 64) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[6],\n"
    "                              vertex_model[7],\n"
    "                              scalar_values[1].z,\n"
    "                              scalar_values[1].w,\n"
    "                              top_plane_ids[2],\n"
    "                              top_plane_ids[3],\n"
    "                              lerped_normal_list[6],\n"
    "                              lerped_vertex_list[6]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 128) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[7],\n"
    "                              vertex_model[4],\n"
    "                              scalar_values[1].w,\n"
    "                              scalar_values[1].x,\n"
    "                              top_plane_ids[3],\n"
    "                              top_plane_ids[0],\n"
    "                              lerped_normal_list[7],\n"
    "                              lerped_vertex_list[7]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 256) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[0],\n"
    "                              vertex_model[4],\n"
    "                              scalar_values[0].x,\n"
    "                              scalar_values[1].x,\n"
    "                              top_plane_ids[0] + n_ids_per_row,\n"
    "                              top_plane_ids[0],\n"
    "                              lerped_normal_list[8],\n"
    "                              lerped_vertex_list[8]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 512) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[1],\n"
    "                              vertex_model[5],\n"
    "                              scalar_values[0].y,\n"
    "                              scalar_values[1].y,\n"
    "                              top_plane_ids[1] + n_ids_per_row,\n"
    "                              top_plane_ids[1],\n"
    "                              lerped_normal_list[9],\n"
    "                              lerped_vertex_list[9]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 1024) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[2],\n"
    "                              vertex_model[6],\n"
    "                              scalar_values[0].z,\n"
    "                              scalar_values[1].z,\n"
    "                              top_plane_ids[2] + n_ids_per_row,\n"
    "                              top_plane_ids[2],\n"
    "                              lerped_normal_list[10],\n"
    "                              lerped_vertex_list[10]);\n"
    "    }\n"
    "\n"
    "    if ((edge_table[edge_index] & 2048) != 0)\n"
    "    {\n"
    "        get_interpolated_data(vertex_model[3],\n"
    "                              vertex_model[7],\n"
    "                              scalar_values[0].w,\n"
    "                              scalar_values[1].w,\n"
    "                              top_plane_ids[3] + n_ids_per_row,\n"
    "                              top_plane_ids[3],\n"
    "                              lerped_normal_list[11],\n"
    "                              lerped_vertex_list[11]);\n"
    "    }\n"
    "\n"
    /* Emit triangles. Note that we need to generate more triangles than we actually need in
     * order to make sure the flow remains uniform.
     *
     * NOTE: This atomic add op is required, since the number of triangles emitted by all
     *       workgroups depends on the scalar field configuration, as well as the isolevel,
     *       both of which may be changed between consecutive frames. If we replaced it with
     *       a constant number of triangles, derived from the blob size dimensions, some of
     *       the triangles, generated in previous passes, would pollute the renderbuffer.
     */
    "    const uint n_result_triangle_base_vertex = atomicAdd(indirect_draw_call_count, 3 * 5);\n"
    "\n"
    "    for (uint n_triangle = 0;\n"
    "              n_triangle < 5;\n"
    "              n_triangle++)\n"
    "    {\n"
    "        const uint n_result_triangle_start_vertex = n_result_triangle_base_vertex + n_triangle * 3;\n"
    "        const uint n_triangle_base_vertex         = edge_index * 15               + n_triangle * 3;\n"
    "\n"
    "        if (triangle_table[n_triangle_base_vertex] == -1)\n"
    "        {\n"
    "            for (uint n_vertex = 0;\n"
    "                      n_vertex < 3;\n"
    "                      n_vertex++)\n"
    "            {\n"
    /* Just discard the vertex..*/
    "                result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 3] = 0.0;\n"
    "            }\n"
    "        }\n"
    "        else\n"
    "        for (uint n_vertex = 0;\n"
    "                  n_vertex < 3;\n"
    "                  n_vertex++)\n"
    "        {\n"
    "            const int  list_index     = triangle_table    [ n_triangle_base_vertex + n_vertex ];\n"
    "            const vec3 current_normal = lerped_normal_list[ list_index ];\n"
    "            const vec3 current_vertex = lerped_vertex_list[ list_index ];\n"
    "\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 0] = current_vertex.x;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 1] = current_vertex.y;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 2] = current_vertex.z;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 3] = 1.0;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 4] = current_normal.x;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 5] = current_normal.y;\n"
    "            result_data[(n_result_triangle_start_vertex + n_vertex) * 7 + 6] = current_normal.z;\n"
    "        }\n"
    "    }\n"
    "}\n";

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_marchingcubes,
                               mesh_marchingcubes,
                              _mesh_marchingcubes);


/** TODO */
PRIVATE void _mesh_marchingcubes_deinit(_mesh_marchingcubes* mesh_ptr)
{
    ral_present_task* present_tasks[] =
    {
        &mesh_ptr->present_task_with_compute,
        &mesh_ptr->present_task_wo_compute
    };
    const uint32_t n_present_tasks = sizeof(present_tasks) / sizeof(present_tasks[0]);

    if (mesh_ptr->indirect_draw_call_args_bo != nullptr)
    {
        ral_context_delete_objects(mesh_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&mesh_ptr->indirect_draw_call_args_bo) );

        mesh_ptr->indirect_draw_call_args_bo = nullptr;
    }

    if (mesh_ptr->mesh_instance != nullptr)
    {
        mesh_release(mesh_ptr->mesh_instance);

        mesh_ptr->mesh_instance = nullptr;
    }

    if (mesh_ptr->polygonized_data_bo != nullptr)
    {
        if (mesh_ptr->polygonized_data_normals_subregion_bo != nullptr)
        {
            ral_context_delete_objects(mesh_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&mesh_ptr->polygonized_data_normals_subregion_bo) );

            mesh_ptr->polygonized_data_normals_subregion_bo = nullptr;
        }

        if (mesh_ptr->polygonized_data_vertices_subregion_bo != nullptr)
        {
            ral_context_delete_objects(mesh_ptr->context,
                                       RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&mesh_ptr->polygonized_data_vertices_subregion_bo) );

            mesh_ptr->polygonized_data_vertices_subregion_bo = nullptr;
        }

        ral_context_delete_objects(mesh_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&mesh_ptr->polygonized_data_bo) );

        mesh_ptr->polygonized_data_bo = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb != nullptr)
    {
        ral_program_block_buffer_release(mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb);

        mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_result_data_ssb != nullptr)
    {
        ral_program_block_buffer_release(mesh_ptr->po_scalar_field_polygonizer_result_data_ssb);

        mesh_ptr->po_scalar_field_polygonizer_result_data_ssb = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb != nullptr)
    {
        ral_program_block_buffer_release(mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb);

        mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer != nullptr)
    {
        ral_context_delete_objects(mesh_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&mesh_ptr->po_scalar_field_polygonizer) );

        mesh_ptr->po_scalar_field_polygonizer = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_data_ub != nullptr)
    {
        ral_program_block_buffer_release(mesh_ptr->po_scalar_field_polygonizer_data_ub);

        mesh_ptr->po_scalar_field_polygonizer_data_ub = nullptr;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub != nullptr)
    {
        /* TODO: This data could be re-used across mesh_marchingcubes instances! */
        ral_program_block_buffer_release(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub);

        mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub = nullptr;
    }

    for (uint32_t n_present_task = 0;
                  n_present_task < n_present_tasks;
                ++n_present_task)
    {
        if (*present_tasks[n_present_task] != nullptr)
        {
            ral_present_task_release(*present_tasks[n_present_task]);

            *present_tasks[n_present_task] = nullptr;
        }
    }

    if (mesh_ptr->material != nullptr)
    {
        scene_material_release(mesh_ptr->material);

        mesh_ptr->material = nullptr;
    }

    if (mesh_ptr->material_gpu != nullptr)
    {
        mesh_material_release(mesh_ptr->material_gpu);

        mesh_ptr->material_gpu = nullptr;
    }
}

/** TODO */
PRIVATE void _mesh_marchingcubes_get_aabb(const void* user_arg,
                                          float*      out_aabb_model_vec3_min,
                                          float*      out_aabb_model_vec3_max)
{
    out_aabb_model_vec3_max[0] = 1.0f;
    out_aabb_model_vec3_max[1] = 1.0f;
    out_aabb_model_vec3_max[2] = 1.0f;
    out_aabb_model_vec3_min[0] = 0.0f;
    out_aabb_model_vec3_min[1] = 0.0f;
    out_aabb_model_vec3_min[2] = 0.0f;
}

/** TODO */
PRIVATE system_hashed_ansi_string _mesh_marchingcubes_get_polygonizer_name(_mesh_marchingcubes* mesh_ptr)
{
    char temp[128];

    snprintf(temp,
             sizeof(temp),
             "Marching Cubes polygonizer [%ux%ux%u]",
             mesh_ptr->grid_size[0],
             mesh_ptr->grid_size[1],
             mesh_ptr->grid_size[2]);

    return system_hashed_ansi_string_create(temp);
}

/** TODO */
PRIVATE void _mesh_marchingcubes_get_token_key_value_arrays(const _mesh_marchingcubes*  mesh_ptr,
                                                            system_hashed_ansi_string** out_token_key_array_ptr,
                                                            system_hashed_ansi_string** out_token_value_array_ptr,
                                                            unsigned int*               out_n_token_key_value_pairs_ptr,
                                                            unsigned int*               out_global_wg_size_uvec3_ptr)
{
    const uint32_t* max_compute_work_group_count;
    const uint32_t* max_compute_work_group_size;
    uint32_t        max_compute_work_group_invocations;

    ral_context_get_property(mesh_ptr->context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_COUNT,
                            &max_compute_work_group_count);
    ral_context_get_property(mesh_ptr->context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_INVOCATIONS,
                            &max_compute_work_group_invocations);
    ral_context_get_property(mesh_ptr->context,
                             RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                            &max_compute_work_group_size);

    *out_token_key_array_ptr   = new system_hashed_ansi_string[9];
    *out_token_value_array_ptr = new system_hashed_ansi_string[9];

    ASSERT_ALWAYS_SYNC(*out_token_key_array_ptr   != nullptr &&
                       *out_token_value_array_ptr != nullptr,
                       "Out of memory");

    (*out_token_key_array_ptr)[0] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_X"),
    (*out_token_key_array_ptr)[1] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Y"),
    (*out_token_key_array_ptr)[2] = system_hashed_ansi_string_create("LOCAL_WG_SIZE_Z"),
    (*out_token_key_array_ptr)[3] = system_hashed_ansi_string_create("BLOB_SIZE_X"),
    (*out_token_key_array_ptr)[4] = system_hashed_ansi_string_create("BLOB_SIZE_Y"),
    (*out_token_key_array_ptr)[5] = system_hashed_ansi_string_create("BLOB_SIZE_Z"),
    (*out_token_key_array_ptr)[6] = system_hashed_ansi_string_create("GLOBAL_WG_SIZE_X"),
    (*out_token_key_array_ptr)[7] = system_hashed_ansi_string_create("GLOBAL_WG_SIZE_Y"),
    (*out_token_key_array_ptr)[8] = system_hashed_ansi_string_create("GLOBAL_WG_SIZE_Z"),

    *out_n_token_key_value_pairs_ptr = 9;

    /* Compute global work-group size */
    const uint32_t n_total_scalars  = mesh_ptr->grid_size[0] * mesh_ptr->grid_size[1] * mesh_ptr->grid_size[2];
    const uint32_t wg_local_size_x  = max_compute_work_group_size[0]; /* TODO: smarterize me */
    const uint32_t wg_local_size_y  = 1;
    const uint32_t wg_local_size_z  = 1;

    out_global_wg_size_uvec3_ptr[0] = 1 + n_total_scalars / wg_local_size_x;
    out_global_wg_size_uvec3_ptr[1] = 1;
    out_global_wg_size_uvec3_ptr[2] = 1;

    ASSERT_DEBUG_SYNC(wg_local_size_x * wg_local_size_y * wg_local_size_z <= max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(out_global_wg_size_uvec3_ptr[0] < max_compute_work_group_count[0] &&
                      out_global_wg_size_uvec3_ptr[1] < max_compute_work_group_count[1] &&
                      out_global_wg_size_uvec3_ptr[2] < max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    /* Fill the token value array */
    char temp_buffer[64];

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             wg_local_size_x);
    (*out_token_value_array_ptr)[0] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             wg_local_size_y);
    (*out_token_value_array_ptr)[1] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             wg_local_size_z);
    (*out_token_value_array_ptr)[2] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             mesh_ptr->grid_size[0]);
    (*out_token_value_array_ptr)[3] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             mesh_ptr->grid_size[1]);
    (*out_token_value_array_ptr)[4] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             mesh_ptr->grid_size[2]);
    (*out_token_value_array_ptr)[5] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             out_global_wg_size_uvec3_ptr[0]);
    (*out_token_value_array_ptr)[6] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             out_global_wg_size_uvec3_ptr[1]);
    (*out_token_value_array_ptr)[7] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%u",
             out_global_wg_size_uvec3_ptr[2]);
    (*out_token_value_array_ptr)[8] = system_hashed_ansi_string_create(temp_buffer);
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init(_mesh_marchingcubes* mesh_ptr)
{
    system_hashed_ansi_string polygonizer_name_has = nullptr;
    ral_program               polygonizer_po       = nullptr;

    /* Initialize the polygonizer, if one has not been already instantiated */
    polygonizer_name_has = _mesh_marchingcubes_get_polygonizer_name(mesh_ptr);
    polygonizer_po       = ral_context_get_program_by_name         (mesh_ptr->context,
                                                                    polygonizer_name_has);

    if (polygonizer_po == nullptr)
    {
        _mesh_marchingcubes_init_polygonizer_po(mesh_ptr);
    }
    else
    {
        ral_context_retain_object(mesh_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                  polygonizer_po);
    }

    /* Initialize present tasks */
    _mesh_marchingcubes_init_present_tasks(mesh_ptr);
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init_mesh_instance(_mesh_marchingcubes* mesh_ptr)
{
    char name_buffer[128];

    /* Create a new mesh instance */
    snprintf(name_buffer,
             sizeof(name_buffer),
             "Internal mesh instance for mesh_marchingcubes named [%s]",
             system_hashed_ansi_string_get_buffer(mesh_ptr->name) );

    mesh_ptr->mesh_instance = mesh_create_gpu_stream_mesh(_mesh_marchingcubes_get_aabb,
                                                          mesh_ptr,
                                                          system_hashed_ansi_string_create(name_buffer) );

    /* Marching cubes generates CW-ordered vertices. */
    static const mesh_vertex_ordering vertex_ordering = MESH_VERTEX_ORDERING_CW;

    mesh_set_property(mesh_ptr->mesh_instance,
                      MESH_PROPERTY_VERTEX_ORDERING,
                     &vertex_ordering);

    /* Configure it, so that it uses normal & vertex data taken from the buffer memory we manage in mesh_marchingcubes */
    mesh_draw_call_arguments draw_call_arguments;
    const unsigned int       max_normals_data_stream_size  = sizeof(float) * 3 /* vertices */ * 5 /* triangles per voxel */ * mesh_ptr->grid_size[0] * mesh_ptr->grid_size[1] * mesh_ptr->grid_size[2];
    const unsigned int       max_vertices_data_stream_size = sizeof(float) * 3 /* vertices */ * 5 /* triangles per voxel */ * mesh_ptr->grid_size[0] * mesh_ptr->grid_size[1] * mesh_ptr->grid_size[2];
    mesh_layer_id            new_layer_id                  = mesh_add_layer(mesh_ptr->mesh_instance);
    mesh_layer_pass_id       new_layer_pass_id;

    draw_call_arguments.draw_indirect_bo = mesh_ptr->indirect_draw_call_args_bo;
    draw_call_arguments.indirect_offset  = 0;
    draw_call_arguments.primitive_type   = RAL_PRIMITIVE_TYPE_TRIANGLES;

    new_layer_pass_id = mesh_add_layer_pass_for_gpu_stream_mesh(mesh_ptr->mesh_instance,
                                                                new_layer_id,
                                                                mesh_ptr->material_gpu,
                                                                MESH_DRAW_CALL_TYPE_NONINDEXED_INDIRECT,
                                                               &draw_call_arguments);

    /* Create new normals & vertices subregion buffers */
    ral_buffer_create_info normals_subregion_bo_create_info;
    ral_buffer_create_info vertices_subregion_bo_create_info;

    normals_subregion_bo_create_info.parent_buffer = mesh_ptr->polygonized_data_bo;
    normals_subregion_bo_create_info.size          = max_normals_data_stream_size;
    normals_subregion_bo_create_info.start_offset  = sizeof(float) * 4;

    vertices_subregion_bo_create_info.parent_buffer = mesh_ptr->polygonized_data_bo;
    vertices_subregion_bo_create_info.size          = max_vertices_data_stream_size;
    vertices_subregion_bo_create_info.start_offset  = 0;

    ral_context_create_buffers(mesh_ptr->context,
                               1, /* n_buffers */
                              &normals_subregion_bo_create_info,
                              &mesh_ptr->polygonized_data_normals_subregion_bo);
    ral_context_create_buffers(mesh_ptr->context,
                               1, /* n_buffers */
                              &vertices_subregion_bo_create_info,
                              &mesh_ptr->polygonized_data_vertices_subregion_bo);

    mesh_add_layer_data_stream_from_buffer_memory(mesh_ptr->mesh_instance,
                                                  new_layer_id,
                                                  MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                  3, /* n_components */
                                                  mesh_ptr->polygonized_data_normals_subregion_bo,
                                                  sizeof(float) * 7); /* bo_stride */
    mesh_add_layer_data_stream_from_buffer_memory(mesh_ptr->mesh_instance,
                                                  new_layer_id,
                                                  MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                  4, /* n_components */
                                                  mesh_ptr->polygonized_data_vertices_subregion_bo,
                                                  sizeof(float) * 7); /* bo_stride */

    mesh_set_layer_data_stream_property_with_buffer_memory(mesh_ptr->mesh_instance,
                                                           new_layer_id,
                                                           MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                           MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,
                                                           mesh_ptr->indirect_draw_call_args_bo,
                                                           true); /* does_read_require_memory_barrier */

    mesh_set_layer_data_stream_property_with_buffer_memory(mesh_ptr->mesh_instance,
                                                           new_layer_id,
                                                           MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                           MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,
                                                           mesh_ptr->indirect_draw_call_args_bo,
                                                           true); /* does_read_require_memory_barrier */
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init_polygonizer_po(_mesh_marchingcubes* mesh_ptr)
{
    /* Set up token key/value arrays */
    unsigned int               n_token_key_value_pairs = 0;
    system_hashed_ansi_string* token_key_array_ptr     = nullptr;
    system_hashed_ansi_string* token_value_array_ptr   = nullptr;

    _mesh_marchingcubes_get_token_key_value_arrays(mesh_ptr,
                                                  &token_key_array_ptr,
                                                  &token_value_array_ptr,
                                                  &n_token_key_value_pairs,
                                                   mesh_ptr->po_scalar_field_polygonizer_global_wg_size);

    /* Initialize the shader */
    ral_shader                      cs              = nullptr;
    const system_hashed_ansi_string cs_body_final   = system_hashed_ansi_string_create_by_token_replacement(_mesh_marchingcubes_cs_body,
                                                                                                            n_token_key_value_pairs,
                                                                                                            token_key_array_ptr,
                                                                                                            token_value_array_ptr);
    system_hashed_ansi_string       polygonizer_has = _mesh_marchingcubes_get_polygonizer_name             (mesh_ptr);

    const ral_shader_create_info cs_create_info =
    {
        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(polygonizer_has),
                                                                " CS"),
        RAL_SHADER_TYPE_COMPUTE
    };

    if (!ral_context_create_shaders(mesh_ptr->context,
                                    1, /* n_create_info_items */
                                   &cs_create_info,
                                   &cs) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL shader creation failed.");
    }

    ral_shader_set_property(cs,
                            RAL_SHADER_PROPERTY_GLSL_BODY,
                           &cs_body_final);

    delete [] token_key_array_ptr;
    token_key_array_ptr = nullptr;

    delete [] token_value_array_ptr;
    token_value_array_ptr = nullptr;

    /* Prepare & link the program object */
    const ral_program_create_info po_create_info =
    {
        RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE,
        system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(polygonizer_has),
                                                                " PO")
    };

    if (!ral_context_create_programs(mesh_ptr->context,
                                     1, /* n_create_info_items */
                                    &po_create_info,
                                    &mesh_ptr->po_scalar_field_polygonizer) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "RAL program creation failed");
    }

    if (!ral_program_attach_shader(mesh_ptr->po_scalar_field_polygonizer,
                                   cs,
                                   true /* async */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Failed to attach a RAL shader to a RAL program");
    }

    /* Set up a BO which is going to be used for holding indirect draw call arguments */
    const unsigned int indirect_draw_call_args[] =
    {
        0, /* count - will be filled by the polygonizer CS */
        1, /* primcount                                    */
        0, /* first                                        */
        0  /* baseInstance                                 */
    };
    ral_buffer_create_info                                               indirect_draw_call_args_bo_create_info;
    ral_buffer_client_sourced_update_info                                indirect_draw_call_args_bo_data_update;
    std::vector<std::shared_ptr<ral_buffer_client_sourced_update_info> > indirect_draw_call_args_bo_data_updates;

    ASSERT_DEBUG_SYNC(sizeof(indirect_draw_call_args) == sizeof(unsigned int) * 4,
                      "Invalid invalid draw call arg structure instance size");

    indirect_draw_call_args_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    indirect_draw_call_args_bo_create_info.property_bits    = 0;
    indirect_draw_call_args_bo_create_info.parent_buffer    = nullptr;
    indirect_draw_call_args_bo_create_info.size             = sizeof(unsigned int) * 4;
    indirect_draw_call_args_bo_create_info.start_offset     = 0;
    indirect_draw_call_args_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT  |
                                                              RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT |
                                                              RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    indirect_draw_call_args_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(mesh_ptr->context,
                               1, /* n_buffers */
                              &indirect_draw_call_args_bo_create_info,
                              &mesh_ptr->indirect_draw_call_args_bo);


    mesh_ptr->indirect_draw_call_args_bo_count_arg_offset = 0; /* count is the first argument */

    indirect_draw_call_args_bo_data_update.data         = indirect_draw_call_args;
    indirect_draw_call_args_bo_data_update.data_size    = indirect_draw_call_args_bo_create_info.size;
    indirect_draw_call_args_bo_data_update.start_offset = 0;

    indirect_draw_call_args_bo_data_updates.push_back(std::shared_ptr<ral_buffer_client_sourced_update_info>(&indirect_draw_call_args_bo_data_update,
                                                                                                             NullDeleter<ral_buffer_client_sourced_update_info>() ));

    ral_buffer_set_data_from_client_memory(mesh_ptr->indirect_draw_call_args_bo,
                                           indirect_draw_call_args_bo_data_updates,
                                           false, /* async               */
                                           true   /* sync_other_contexts */);

    /* Set up a BO which is going to hold the polygonized data. At max, each cube is going to hold
     * five triangles. */
    ral_buffer_create_info polygonized_data_bo_create_info;

    polygonized_data_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    polygonized_data_bo_create_info.parent_buffer    = nullptr;
    polygonized_data_bo_create_info.property_bits    = 0;
    polygonized_data_bo_create_info.size             = mesh_ptr->grid_size[0] * mesh_ptr->grid_size[1] * mesh_ptr->grid_size[2] *
                                                       5 /* triangles */                                                        *
                                                       3 /* vertices */                                                         *
                                                       7 /* vertex + model space normal data components */                      *
                                                       sizeof(float)                                                            /
                                                       mesh_ptr->polygonized_data_size_reduction;
    polygonized_data_bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT |
                                                       RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    polygonized_data_bo_create_info.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(mesh_ptr->context,
                               1, /* n_buffers */
                              &polygonized_data_bo_create_info,
                              &mesh_ptr->polygonized_data_bo);

    /* Retrieve data UB properties */
    const ral_program_variable* uniform_isolevel_ptr       = nullptr;
    ral_buffer                  polygonizer_data_ub_bo_ral = nullptr;

    mesh_ptr->po_scalar_field_polygonizer_data_ub = ral_program_block_buffer_create(mesh_ptr->context,
                                                                                    mesh_ptr->po_scalar_field_polygonizer,
                                                                                    system_hashed_ansi_string_create("dataUB") );

    ral_program_block_buffer_get_property  (mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                            RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                           &polygonizer_data_ub_bo_ral);
    ral_buffer_get_property                (polygonizer_data_ub_bo_ral,
                                            RAL_BUFFER_PROPERTY_SIZE,
                                           &mesh_ptr->po_scalar_field_polygonizer_data_ub_bo_size);
    ral_program_get_block_variable_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                           system_hashed_ansi_string_create("dataUB"),
                                           system_hashed_ansi_string_create("isolevel"),
                                          &uniform_isolevel_ptr);

    mesh_ptr->po_scalar_field_polygonizer_data_ub_isolevel_offset = uniform_isolevel_ptr->block_offset;

    /* Set up precomputed tables UB contents */
    ral_buffer                  precomputed_tables_bo_ral      = nullptr;
    const ral_program_variable* uniform_edge_table_ral_ptr     = nullptr;
    const ral_program_variable* uniform_triangle_table_ral_ptr = nullptr;

    ral_program_get_block_variable_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                           system_hashed_ansi_string_create("precomputed_tablesUB"),
                                           system_hashed_ansi_string_create("edge_table[0]"),
                                          &uniform_edge_table_ral_ptr);
    ral_program_get_block_variable_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                           system_hashed_ansi_string_create("precomputed_tablesUB"),
                                           system_hashed_ansi_string_create("triangle_table[0]"),
                                          &uniform_triangle_table_ral_ptr);

    mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = uniform_edge_table_ral_ptr->block_offset;
    mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = uniform_triangle_table_ral_ptr->block_offset;

    mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub = ral_program_block_buffer_create(mesh_ptr->context,
                                                                                                  mesh_ptr->po_scalar_field_polygonizer,
                                                                                                  system_hashed_ansi_string_create("precomputed_tablesUB") );

    ral_program_block_buffer_get_property(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                          RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                         &precomputed_tables_bo_ral);
    ral_buffer_get_property              (precomputed_tables_bo_ral,
                                          RAL_BUFFER_PROPERTY_SIZE,
                                         &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_size);

    /* Set up indirect draw call <count> arg SSB */
    mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb = ral_program_block_buffer_create(mesh_ptr->context,
                                                                                                         mesh_ptr->po_scalar_field_polygonizer,
                                                                                                         system_hashed_ansi_string_create("indirect_draw_callSSB") );

    /* Set up result data SSB */
    mesh_ptr->po_scalar_field_polygonizer_result_data_ssb = ral_program_block_buffer_create(mesh_ptr->context,
                                                                                            mesh_ptr->po_scalar_field_polygonizer,
                                                                                            system_hashed_ansi_string_create("result_dataSSB") );

    /* Set up scalar field data SSB */
    mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb = ral_program_block_buffer_create(mesh_ptr->context,
                                                                                                  mesh_ptr->po_scalar_field_polygonizer,
                                                                                                  system_hashed_ansi_string_create("scalar_field_dataSSB") );

    /* Set up precomputed table contents */
    ral_program_block_buffer_set_arrayed_variable_value(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                                        mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset,
                                                        _edge_table,
                                                        sizeof(_edge_table),
                                                        0, /* dst_array_start_index */
                                                        sizeof(_edge_table) / sizeof(_edge_table[0]) );
    ral_program_block_buffer_set_arrayed_variable_value(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                                        mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset,
                                                        _triangle_table,
                                                        sizeof(_triangle_table),
                                                        0, /* dst_array_start_index */
                                                        sizeof(_triangle_table) / sizeof(_triangle_table[0]) );

    ral_program_block_buffer_sync_immediately(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub);

    /* Release stuff */
    ral_context_delete_objects(mesh_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_SHADER,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&cs) );
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init_present_tasks(_mesh_marchingcubes* mesh_ptr)
{
    ral_buffer po_scalar_field_polygonizer_data_ub_bo_ral               = nullptr;
    ral_buffer po_scalar_field_polygonizer_precomputed_tables_ub_bo_ral = nullptr;

    /* Prepare the "full bake" present task first. */
    {
        ral_command_buffer             command_buffer;
        ral_command_buffer_create_info command_buffer_create_info;

        command_buffer_create_info.compatible_queues                       = RAL_QUEUE_COMPUTE_BIT;
        command_buffer_create_info.is_executable                           = true;
        command_buffer_create_info.is_invokable_from_other_command_buffers = false;
        command_buffer_create_info.is_resettable                           = false;
        command_buffer_create_info.is_transient                            = false;

        ral_context_create_command_buffers(mesh_ptr->context,
                                           1, /* n_command_buffers */
                                          &command_buffer_create_info,
                                          &command_buffer);

        ral_command_buffer_start_recording(command_buffer);
        {
            /* Set up the SSBO & UBO bindings */
            uint32_t polygonized_data_bo_size = 0;
            uint32_t scalar_data_bo_size      = 0;

            ral_program_block_buffer_get_property(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &po_scalar_field_polygonizer_data_ub_bo_ral);
            ral_program_block_buffer_get_property(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                                  RAL_PROGRAM_BLOCK_BUFFER_PROPERTY_BUFFER_RAL,
                                                 &po_scalar_field_polygonizer_precomputed_tables_ub_bo_ral);

            ral_buffer_get_property(mesh_ptr->polygonized_data_bo,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &polygonized_data_bo_size);
            ral_buffer_get_property(mesh_ptr->scalar_data_bo,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &scalar_data_bo_size);


            ral_command_buffer_set_binding_command_info binding_info[5];
            ral_command_buffer_fill_buffer_command_info fill_op_info;

            binding_info[0].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
            binding_info[0].name                          = system_hashed_ansi_string_create("indirect_draw_callSSB");
            binding_info[0].storage_buffer_binding.buffer = mesh_ptr->indirect_draw_call_args_bo;
            binding_info[0].storage_buffer_binding.offset = mesh_ptr->indirect_draw_call_args_bo_count_arg_offset;
            binding_info[0].storage_buffer_binding.size   = sizeof(int);

            binding_info[1].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
            binding_info[1].name                          = system_hashed_ansi_string_create("scalar_field_dataSSB");
            binding_info[1].storage_buffer_binding.buffer = mesh_ptr->scalar_data_bo;
            binding_info[1].storage_buffer_binding.offset = 0;
            binding_info[1].storage_buffer_binding.size   = scalar_data_bo_size;

            binding_info[2].binding_type                  = RAL_BINDING_TYPE_STORAGE_BUFFER;
            binding_info[2].name                          = system_hashed_ansi_string_create("result_dataSSB");
            binding_info[2].storage_buffer_binding.buffer = mesh_ptr->polygonized_data_bo;
            binding_info[2].storage_buffer_binding.offset = 0;
            binding_info[2].storage_buffer_binding.size   = polygonized_data_bo_size;

            binding_info[3].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            binding_info[3].name                          = system_hashed_ansi_string_create("dataUB");
            binding_info[3].uniform_buffer_binding.buffer = po_scalar_field_polygonizer_data_ub_bo_ral;
            binding_info[3].uniform_buffer_binding.offset = 0;
            binding_info[3].uniform_buffer_binding.size   = mesh_ptr->po_scalar_field_polygonizer_data_ub_bo_size;

            binding_info[4].binding_type                  = RAL_BINDING_TYPE_UNIFORM_BUFFER;
            binding_info[4].name                          = system_hashed_ansi_string_create("precomputed_tablesUB");
            binding_info[4].uniform_buffer_binding.buffer = po_scalar_field_polygonizer_precomputed_tables_ub_bo_ral;
            binding_info[4].uniform_buffer_binding.offset = 0;
            binding_info[4].uniform_buffer_binding.size   = mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_size;

            fill_op_info.buffer       = mesh_ptr->indirect_draw_call_args_bo;
            fill_op_info.dword_value  = 0;
            fill_op_info.n_dwords     = 1;
            fill_op_info.start_offset = mesh_ptr->indirect_draw_call_args_bo_count_arg_offset;


            /* Set up the indirect draw call's <count> SSB. Zero out the value before we run the polygonizer */
            ral_command_buffer_record_fill_buffer(command_buffer,
                                                  1, /* n_fill_ops */
                                                 &fill_op_info);

            /* Prepare for & fire the dispatch call */
            ral_command_buffer_record_set_program (command_buffer,
                                                   mesh_ptr->po_scalar_field_polygonizer);
            ral_command_buffer_record_set_bindings(command_buffer,
                                                   sizeof(binding_info) / sizeof(binding_info[0]),
                                                   binding_info);
            ral_command_buffer_record_dispatch    (command_buffer,
                                                   mesh_ptr->po_scalar_field_polygonizer_global_wg_size);
        }
        ral_command_buffer_stop_recording(command_buffer);

        /* Form present tasks */
        ral_present_task                    task_cpu;
        ral_present_task_cpu_create_info    task_cpu_create_info;
        ral_present_task_io                 task_cpu_unique_output;
        ral_present_task                    task_gpu;
        ral_present_task_gpu_create_info    task_gpu_create_info;
        ral_present_task_io                 task_gpu_unique_inputs[2];
        ral_present_task_io                 task_gpu_unique_output;
        ral_present_task_group_create_info  task_result_create_info;
        ral_present_task_ingroup_connection task_result_ingroup_connection;
        ral_present_task_group_mapping      task_result_input_mapping;
        ral_present_task_group_mapping      task_result_output_mapping;
        ral_present_task                    task_result_present_tasks[2];

        task_cpu_unique_output.buffer      = po_scalar_field_polygonizer_data_ub_bo_ral;
        task_cpu_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        task_cpu_create_info.cpu_task_callback_user_arg = mesh_ptr;
        task_cpu_create_info.n_unique_inputs            = 0;
        task_cpu_create_info.n_unique_outputs           = 1;
        task_cpu_create_info.pfn_cpu_task_callback_proc = _mesh_marchingcubes_update_ub_data_cpu_task_callback;
        task_cpu_create_info.unique_outputs             = &task_cpu_unique_output;

        task_cpu = ral_present_task_create_cpu(system_hashed_ansi_string_create("Marching cubes mesh: UB update"),
                                              &task_cpu_create_info);


        task_gpu_unique_inputs[0].buffer       = po_scalar_field_polygonizer_data_ub_bo_ral;
        task_gpu_unique_inputs[0].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;
        task_gpu_unique_inputs[1].buffer       = mesh_ptr->scalar_data_bo;
        task_gpu_unique_inputs[1].object_type  = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        task_gpu_unique_output.buffer      = mesh_ptr->polygonized_data_bo;
        task_gpu_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        task_gpu_create_info.command_buffer   = command_buffer;
        task_gpu_create_info.n_unique_inputs  = sizeof(task_gpu_unique_inputs) / sizeof(task_gpu_unique_inputs[0]);
        task_gpu_create_info.n_unique_outputs = 1;
        task_gpu_create_info.unique_inputs    = task_gpu_unique_inputs;
        task_gpu_create_info.unique_outputs   = &task_gpu_unique_output;

        task_gpu = ral_present_task_create_gpu(system_hashed_ansi_string_create("Marching cubes mesh: Polygonization"),
                                              &task_gpu_create_info);


        task_result_present_tasks[0] = task_cpu;
        task_result_present_tasks[1] = task_gpu;

        task_result_ingroup_connection.input_present_task_index     = 1;
        task_result_ingroup_connection.input_present_task_io_index  = 0;
        task_result_ingroup_connection.output_present_task_index    = 0;
        task_result_ingroup_connection.output_present_task_io_index = 0;

        task_result_input_mapping.group_task_io_index   = 0;
        task_result_input_mapping.n_present_task        = 1;
        task_result_input_mapping.present_task_io_index = 1;

        task_result_output_mapping.group_task_io_index   = 0;
        task_result_output_mapping.n_present_task        = 1;
        task_result_output_mapping.present_task_io_index = 0;

        task_result_create_info.ingroup_connections                      = &task_result_ingroup_connection;
        task_result_create_info.n_ingroup_connections                    = 1;
        task_result_create_info.n_present_tasks                          = sizeof(task_result_present_tasks) / sizeof(task_result_present_tasks[0]);
        task_result_create_info.n_total_unique_inputs                    = 1;
        task_result_create_info.n_total_unique_outputs                   = 1;
        task_result_create_info.n_unique_input_to_ingroup_task_mappings  = 1;
        task_result_create_info.n_unique_output_to_ingroup_task_mappings = 1;
        task_result_create_info.present_tasks                            = task_result_present_tasks;
        task_result_create_info.unique_input_to_ingroup_task_mapping     = &task_result_input_mapping;
        task_result_create_info.unique_output_to_ingroup_task_mapping    = &task_result_output_mapping;

        mesh_ptr->present_task_with_compute = ral_present_task_create_group(system_hashed_ansi_string_create("Marching cubes: Data generation"),
                                                                            &task_result_create_info);

        ral_present_task_release(task_cpu);
        ral_present_task_release(task_gpu);

        ral_context_delete_objects(mesh_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&command_buffer) );
    }

    /* Also prepare a "dummy" present task, which only exposes the data which have been precalculated
     * in previous frames.
     */
    {
        ral_present_task_gpu_create_info task_create_info;
        ral_present_task_io              task_unique_output;

        task_unique_output.buffer      = mesh_ptr->polygonized_data_bo;
        task_unique_output.object_type = RAL_CONTEXT_OBJECT_TYPE_BUFFER;

        task_create_info.command_buffer   = nullptr;
        task_create_info.n_unique_inputs  = 0;
        task_create_info.n_unique_outputs = 1;
        task_create_info.unique_inputs    = nullptr;
        task_create_info.unique_outputs   = &task_unique_output;

        mesh_ptr->present_task_wo_compute = ral_present_task_create_gpu(system_hashed_ansi_string_create("Marching cubes mesh: Dummy"),
                                                                       &task_create_info);
    }
}

/** TODO */
PRIVATE void _mesh_marchingcubes_release(void* arg)
{
    _mesh_marchingcubes* mesh_ptr = reinterpret_cast<_mesh_marchingcubes*>(arg);

    _mesh_marchingcubes_deinit(mesh_ptr);
}

/** TODO */
PRIVATE void _mesh_marchingcubes_update_ub_data_cpu_task_callback(void* mesh_raw_ptr)
{
    _mesh_marchingcubes* mesh_ptr = reinterpret_cast<_mesh_marchingcubes*>(mesh_raw_ptr);

    ral_program_block_buffer_set_nonarrayed_variable_value(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                                           mesh_ptr->po_scalar_field_polygonizer_data_ub_isolevel_offset,
                                                          &mesh_ptr->isolevel,
                                                           sizeof(float) );

    ral_program_block_buffer_sync_immediately(mesh_ptr->po_scalar_field_polygonizer_data_ub);
}


/** Please see header for specification */
PUBLIC EMERALD_API mesh_marchingcubes mesh_marchingcubes_create(ral_context               context,
                                                                const unsigned int*       grid_size_xyz,
                                                                ral_buffer                scalar_data_bo,
                                                                float                     isolevel,
                                                                scene_material            material,
                                                                system_hashed_ansi_string name,
                                                                unsigned int              polygonized_data_size_reduction)
{
    system_hashed_ansi_string material_name = nullptr;
    _mesh_marchingcubes*      new_mesh_ptr  = new (std::nothrow) _mesh_marchingcubes();

    ASSERT_DEBUG_SYNC(new_mesh_ptr != nullptr,
                      "Out of memory");

    scene_material_get_property(material,
                                SCENE_MATERIAL_PROPERTY_NAME,
                               &material_name);

    if (new_mesh_ptr != nullptr)
    {
        uint32_t scalar_data_bo_size = 0;

        ASSERT_DEBUG_SYNC(context != nullptr,
                          "Rendering context is nullptr");
        ASSERT_DEBUG_SYNC(grid_size_xyz[0] >= 1 &&
                          grid_size_xyz[1] >= 1 &&
                          grid_size_xyz[2] >= 1,
                          "Invalid grid size requested.");
        ASSERT_DEBUG_SYNC(material != nullptr,
                          "Input material is nullptr");
        ASSERT_DEBUG_SYNC(scalar_data_bo != nullptr,
                          "Scalar data BO is nullptr");

        ral_buffer_get_property(scalar_data_bo,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &scalar_data_bo_size);

        ASSERT_DEBUG_SYNC(scalar_data_bo_size == sizeof(float) * grid_size_xyz[0] * grid_size_xyz[1] * grid_size_xyz[2],
                          "Invalid scalar data BO size detected.");

        memcpy(new_mesh_ptr->grid_size,
               grid_size_xyz,
               sizeof(new_mesh_ptr->grid_size) );

        new_mesh_ptr->context                         = context;
        new_mesh_ptr->isolevel                        = isolevel;
        new_mesh_ptr->material                        = material;
        new_mesh_ptr->material_gpu                    = mesh_material_create(material_name,
                                                                             context,
                                                                             nullptr); /* object_manager_path */
        new_mesh_ptr->name                            = name;
        new_mesh_ptr->present_task_with_compute       = nullptr;
        new_mesh_ptr->present_task_wo_compute         = nullptr;
        new_mesh_ptr->polygonized_data_size_reduction = polygonized_data_size_reduction;
        new_mesh_ptr->scalar_data_bo                  = scalar_data_bo;

        scene_material_retain(material);

        /* Set up the GPU material instance */
        const float color_ambient[] = {0.1f, 0.1f,  0.1f, 0.0f};
        const float color_diffuse[] = {0.0f, 0.75f, 0.2f, 1.0f};

        mesh_material_set_shading_property_to_vec4 (new_mesh_ptr->material_gpu,
                                                    MESH_MATERIAL_SHADING_PROPERTY_AMBIENT,
                                                    color_ambient);
        mesh_material_set_shading_property_to_vec4 (new_mesh_ptr->material_gpu,
                                                    MESH_MATERIAL_SHADING_PROPERTY_DIFFUSE,
                                                    color_diffuse);
        mesh_material_set_shading_property_to_float(new_mesh_ptr->material_gpu,
                                                    MESH_MATERIAL_SHADING_PROPERTY_SPECULAR,
                                                    1.0f);
        mesh_material_set_shading_property_to_float(new_mesh_ptr->material_gpu,
                                                    MESH_MATERIAL_SHADING_PROPERTY_SHININESS,
                                                    2.0f);

        _mesh_marchingcubes_init(new_mesh_ptr);

        /* Configure the mesh instance */
        _mesh_marchingcubes_init_mesh_instance(new_mesh_ptr);

        /* Register the object */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_mesh_ptr,
                                                       _mesh_marchingcubes_release,
                                                       OBJECT_TYPE_MESH_MARCHINGCUBES,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Mesh (Marching-Cubes)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (mesh_marchingcubes) new_mesh_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_marchingcubes_get_property(const mesh_marchingcubes   in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        void*                       out_result_ptr)
{
    const _mesh_marchingcubes* mesh_ptr = reinterpret_cast<_mesh_marchingcubes*>(in_mesh);

    switch (property)
    {
        case MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL:
        {
            *reinterpret_cast<float*>(out_result_ptr) = mesh_ptr->isolevel;

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_MESH:
        {
            *reinterpret_cast<mesh*>(out_result_ptr) = mesh_ptr->mesh_instance;

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_SCALAR_DATA_BUFFER_RAL:
        {
            *reinterpret_cast<ral_buffer*>(out_result_ptr) = mesh_ptr->scalar_data_bo;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_marchingcubes property requested.");
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task mesh_marchingcubes_get_polygonize_present_task(mesh_marchingcubes in_mesh,
                                                                                   bool               has_scalar_field_changed)
{
    _mesh_marchingcubes* mesh_ptr = reinterpret_cast<_mesh_marchingcubes*>(in_mesh);
    ral_present_task     result;

    if (mesh_ptr->needs_polygonization ||
        has_scalar_field_changed)
    {
        result                         = mesh_ptr->present_task_with_compute;
        mesh_ptr->needs_polygonization = false;
    }
    else
    {
        result = mesh_ptr->present_task_wo_compute;
    }

    ral_present_task_retain(result);

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_marchingcubes_set_property(mesh_marchingcubes          in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        const void*                 data)
{
    _mesh_marchingcubes* mesh_ptr = reinterpret_cast<_mesh_marchingcubes*>(in_mesh);

    switch (property)
    {
        case MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL:
        {
            float new_isolevel = *reinterpret_cast<const float*>(data);

            if (abs(mesh_ptr->isolevel - new_isolevel) > 1e-5f)
            {
                mesh_ptr->isolevel             = new_isolevel;
                mesh_ptr->needs_polygonization = true;
            }

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_SCALAR_DATA_BUFFER_RAL:
        {
            ral_buffer new_bo = *reinterpret_cast<const ral_buffer*>(data);

            if (mesh_ptr->scalar_data_bo != new_bo)
            {
                mesh_ptr->needs_polygonization = true;
                mesh_ptr->scalar_data_bo       = new_bo;
            }

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_marchingcubes_property value requested.");
        }
    }
}
