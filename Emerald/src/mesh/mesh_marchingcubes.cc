/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "mesh/mesh_marchingcubes.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_programs.h"
#include "ogl/ogl_shader.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "scene/scene_material.h"
#include "system/system_log.h"


typedef struct _mesh_marchingcubes
{
    raGL_backend              backend;
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

    ogl_program     po_scalar_field_polygonizer;
    ogl_program_ub  po_scalar_field_polygonizer_data_ub;
    ral_buffer      po_scalar_field_polygonizer_data_ub_bo;
    GLuint          po_scalar_field_polygonizer_data_ub_bo_size;
    GLuint          po_scalar_field_polygonizer_data_ub_bp;
    GLuint          po_scalar_field_polygonizer_data_ub_isolevel_offset;
    unsigned int    po_scalar_field_polygonizer_global_wg_size[3];
    ogl_program_ssb po_scalar_field_polygonizer_indirect_draw_call_count_ssb;
    GLuint          po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp;
    ogl_program_ub  po_scalar_field_polygonizer_precomputed_tables_ub;
    GLuint          po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset;
    ral_buffer      po_scalar_field_polygonizer_precomputed_tables_ub_bo;
    GLuint          po_scalar_field_polygonizer_precomputed_tables_ub_bo_size;
    GLuint          po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset;
    GLuint          po_scalar_field_polygonizer_precomputed_tables_ub_bp;
    ogl_program_ssb po_scalar_field_polygonizer_result_data_ssb;
    GLuint          po_scalar_field_polygonizer_result_data_ssb_bp;
    ogl_program_ssb po_scalar_field_polygonizer_scalar_field_data_ssb;
    GLuint          po_scalar_field_polygonizer_scalar_field_data_ssb_bp;

    REFCOUNT_INSERT_VARIABLES;


    explicit _mesh_marchingcubes()
    {
        memset(grid_size,
               0,
               sizeof(grid_size) );

        backend                         = NULL;
        context                         = NULL;
        isolevel                        = 0.0f;
        material                        = NULL;
        mesh_instance                   = NULL;
        needs_polygonization            = true;
        polygonized_data_size_reduction = 0;
        scalar_data_bo                  = NULL;

        indirect_draw_call_args_bo_count_arg_offset = 0;
        indirect_draw_call_args_bo                  = NULL;

        polygonized_data_bo                    = NULL;
        polygonized_data_normals_subregion_bo  = NULL;
        polygonized_data_vertices_subregion_bo = NULL;

        po_scalar_field_polygonizer                                                = NULL;
        po_scalar_field_polygonizer_data_ub                                        = NULL;
        po_scalar_field_polygonizer_data_ub_bo                                     = NULL;
        po_scalar_field_polygonizer_data_ub_bo_size                                = 0;
        po_scalar_field_polygonizer_data_ub_bp                                     = -1;
        po_scalar_field_polygonizer_data_ub_isolevel_offset                        = -1;
        po_scalar_field_polygonizer_indirect_draw_call_count_ssb                   = NULL;
        po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp                = -1;
        po_scalar_field_polygonizer_precomputed_tables_ub                          = NULL;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = -1;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo                       = NULL;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_size                  = 0;
        po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = -1;
        po_scalar_field_polygonizer_precomputed_tables_ub_bp                       = -1;
        po_scalar_field_polygonizer_result_data_ssb                                = NULL;
        po_scalar_field_polygonizer_result_data_ssb_bp                             = NULL;
        po_scalar_field_polygonizer_scalar_field_data_ssb                          = NULL;
        po_scalar_field_polygonizer_scalar_field_data_ssb_bp                       = NULL;
    }
} _mesh_marchingcubes;

/* Forward declarations */
PRIVATE void                      _mesh_marchingcubes_deinit_rendering_thread_callback (ogl_context                  context,
                                                                                        void*                        user_arg);
PRIVATE void                      _mesh_marchingcubes_get_aabb                         (const void*                  user_arg,
                                                                                        float*                       out_aabb_model_vec3_min,
                                                                                        float*                       out_aabb_model_vec3_max);
PRIVATE system_hashed_ansi_string _mesh_marchingcubes_get_polygonizer_name             (_mesh_marchingcubes*         mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_get_token_key_value_arrays       (const _mesh_marchingcubes*   mesh_ptr,
                                                                                        const ogl_context_gl_limits* limits_ptr,
                                                                                        system_hashed_ansi_string**  out_token_key_array_ptr,
                                                                                        system_hashed_ansi_string**  out_token_value_array_ptr,
                                                                                        unsigned int*                out_n_token_key_value_pairs_ptr,
                                                                                        unsigned int*                out_global_wg_size_uvec3_ptr);
PRIVATE void                      _mesh_marchingcubes_init_mesh_instance               (_mesh_marchingcubes*         mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_init_polygonizer_po              (_mesh_marchingcubes*         mesh_ptr);
PRIVATE void                      _mesh_marchingcubes_init_rendering_thread_callback   (ogl_context                  context,
                                                                                        void*                        user_arg);
PRIVATE void                      _mesh_marchingcubes_release                          (void*                        arg);


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


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh_marchingcubes,
                               mesh_marchingcubes,
                              _mesh_marchingcubes);


/** TODO */
PRIVATE void _mesh_marchingcubes_deinit_rendering_thread_callback(ogl_context context,
                                                                  void*       user_arg)
{
    _mesh_marchingcubes* mesh_ptr = (_mesh_marchingcubes*) user_arg;

    if (mesh_ptr->indirect_draw_call_args_bo != NULL)
    {
        ral_context_delete_buffers(mesh_ptr->context,
                                   1, /* n_buffers */
                                  &mesh_ptr->indirect_draw_call_args_bo);

        mesh_ptr->indirect_draw_call_args_bo = NULL;
    }

    if (mesh_ptr->mesh_instance != NULL)
    {
        mesh_release(mesh_ptr->mesh_instance);

        mesh_ptr->mesh_instance = NULL;
    }

    if (mesh_ptr->polygonized_data_bo != NULL)
    {
        if (mesh_ptr->polygonized_data_normals_subregion_bo != NULL)
        {
            ral_context_delete_buffers(mesh_ptr->context,
                                       1, /* n_buffers */
                                       &mesh_ptr->polygonized_data_normals_subregion_bo);

            mesh_ptr->polygonized_data_normals_subregion_bo = NULL;
        }

        if (mesh_ptr->polygonized_data_vertices_subregion_bo != NULL)
        {
            ral_context_delete_buffers(mesh_ptr->context,
                                       1, /* n_buffers */
                                      &mesh_ptr->polygonized_data_vertices_subregion_bo);

            mesh_ptr->polygonized_data_vertices_subregion_bo = NULL;
        }

        ral_context_delete_buffers(mesh_ptr->context,
                                   1, /* n_buffers */
                                  &mesh_ptr->polygonized_data_bo);

        mesh_ptr->polygonized_data_bo = NULL;
    }

    if (mesh_ptr->po_scalar_field_polygonizer != NULL)
    {
        ogl_program_release(mesh_ptr->po_scalar_field_polygonizer);

        mesh_ptr->po_scalar_field_polygonizer = NULL;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_data_ub_bo != NULL)
    {
        ral_context_delete_buffers(mesh_ptr->context,
                                   1, /* n_buffers */
                                  &mesh_ptr->po_scalar_field_polygonizer_data_ub_bo);

        mesh_ptr->po_scalar_field_polygonizer_data_ub_bo = NULL;
    }

    if (mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo != NULL)
    {
        /* TODO: This data could be re-used across mesh_marchingcubes instances! */
        ral_context_delete_buffers(mesh_ptr->context,
                                   1, /* n_buffers */
                                   &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo);

        mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo = NULL;
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
PRIVATE void _mesh_marchingcubes_get_token_key_value_arrays(const _mesh_marchingcubes*   mesh_ptr,
                                                            const ogl_context_gl_limits* limits_ptr,
                                                            system_hashed_ansi_string**  out_token_key_array_ptr,
                                                            system_hashed_ansi_string**  out_token_value_array_ptr,
                                                            unsigned int*                out_n_token_key_value_pairs_ptr,
                                                            unsigned int*                out_global_wg_size_uvec3_ptr)
{
    *out_token_key_array_ptr   = new system_hashed_ansi_string[9];
    *out_token_value_array_ptr = new system_hashed_ansi_string[9];

    ASSERT_ALWAYS_SYNC(*out_token_key_array_ptr   != NULL &&
                       *out_token_value_array_ptr != NULL,
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
    const uint32_t wg_local_size_x  = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    const uint32_t wg_local_size_y  = 1;
    const uint32_t wg_local_size_z  = 1;

    out_global_wg_size_uvec3_ptr[0] = 1 + n_total_scalars / wg_local_size_x;
    out_global_wg_size_uvec3_ptr[1] = 1;
    out_global_wg_size_uvec3_ptr[2] = 1;

    ASSERT_DEBUG_SYNC(wg_local_size_x * wg_local_size_y * wg_local_size_z <= (uint32_t) limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(out_global_wg_size_uvec3_ptr[0] < (uint32_t) limits_ptr->max_compute_work_group_count[0] &&
                      out_global_wg_size_uvec3_ptr[1] < (uint32_t) limits_ptr->max_compute_work_group_count[1] &&
                      out_global_wg_size_uvec3_ptr[2] < (uint32_t) limits_ptr->max_compute_work_group_count[2],
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

    GLuint      indirect_draw_call_args_bo_id           = 0;
    raGL_buffer indirect_draw_call_args_bo_raGL         = NULL;
    uint32_t    indirect_draw_call_args_bo_start_offset = -1;
    GLuint      polygonized_data_bo_id                  = 0;
    raGL_buffer polygonized_data_bo_raGL                = NULL;
    uint32_t    polygonized_data_bo_start_offset        = -1;

    raGL_backend_get_buffer(mesh_ptr->backend,
                            mesh_ptr->indirect_draw_call_args_bo,
                  (void**) &indirect_draw_call_args_bo_raGL);
    raGL_backend_get_buffer(mesh_ptr->backend,
                            mesh_ptr->polygonized_data_bo,
                  (void**) &polygonized_data_bo_raGL);

    raGL_buffer_get_property(indirect_draw_call_args_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &indirect_draw_call_args_bo_id);
    raGL_buffer_get_property(indirect_draw_call_args_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &indirect_draw_call_args_bo_start_offset);
    raGL_buffer_get_property(polygonized_data_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &polygonized_data_bo_id);
    raGL_buffer_get_property(polygonized_data_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &polygonized_data_bo_start_offset);


    draw_call_arguments.draw_indirect_bo_binding        = indirect_draw_call_args_bo_id;
    draw_call_arguments.indirect                        = (const GLvoid*) indirect_draw_call_args_bo_start_offset;
    draw_call_arguments.mode                            = GL_TRIANGLES;
    draw_call_arguments.pre_draw_call_barriers          = GL_COMMAND_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;

    new_layer_pass_id = mesh_add_layer_pass_for_gpu_stream_mesh(mesh_ptr->mesh_instance,
                                                                new_layer_id,
                                                                mesh_ptr->material_gpu,
                                                                MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT,
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
                                                           // sizeof(unsigned int),
                                                           true); /* does_read_require_memory_barrier */

    mesh_set_layer_data_stream_property_with_buffer_memory(mesh_ptr->mesh_instance,
                                                           new_layer_id,
                                                           MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                                           MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,
                                                           mesh_ptr->indirect_draw_call_args_bo,
                                                           // sizeof(unsigned int),
                                                           true); /* does_read_require_memory_barrier */
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init_polygonizer_po(_mesh_marchingcubes* mesh_ptr)
{
    const char* cs_body = "#version 430 core\n"
                          "\n"
                          "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
                          "\n"
                          "const uint n_ids_per_row    = BLOB_SIZE_X;\n"
                          "const uint n_ids_per_slice  = BLOB_SIZE_X * BLOB_SIZE_Y;\n"
                          "\n"
                          "layout(packed) uniform dataUB\n"
                          "{\n"
                          "    float isolevel;\n"
                          "};\n"
                          "\n"
                          "layout(std430) buffer indirect_draw_callSSB\n"
                          "{\n"
                          "    restrict uint indirect_draw_call_count;\n"
                          "};\n"
                          "\n"
                          "layout(std140) uniform precomputed_tablesUB\n"
                          "{\n"
                          "    int edge_table    [256];\n"
                          "    int triangle_table[256 * 15];\n"
                          "};\n"
                          "\n"
                          "layout(std430) writeonly buffer result_dataSSB\n"
                          "{\n"
                          /* 4 floats: vertex data (model space). We need to include W since some of the vertices need to be discarded.
                           * 3 floats: normal data (model space) */
                          "    restrict float result_data[];\n"
                          "};\n"
                          "\n"
                          "layout(std430) readonly buffer scalar_field_dataSSB\n"
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

    /* Set up token key/value arrays */
    const ogl_context_gl_entrypoints* entrypoints_ptr         = NULL;
    const ogl_context_gl_limits*      limits_ptr              = NULL;
    unsigned int                      n_token_key_value_pairs = 0;
    system_hashed_ansi_string*        token_key_array_ptr     = NULL;
    system_hashed_ansi_string*        token_value_array_ptr   = NULL;

    ogl_context_get_property(ral_context_get_gl_context(mesh_ptr->context),
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(ral_context_get_gl_context(mesh_ptr->context),
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    _mesh_marchingcubes_get_token_key_value_arrays(mesh_ptr,
                                                   limits_ptr,
                                                  &token_key_array_ptr,
                                                  &token_value_array_ptr,
                                                  &n_token_key_value_pairs,
                                                   mesh_ptr->po_scalar_field_polygonizer_global_wg_size);

    /* Initialize the shader */
    ogl_shader                cs              = NULL;
    system_hashed_ansi_string polygonizer_has = _mesh_marchingcubes_get_polygonizer_name(mesh_ptr);

    cs = ogl_shader_create(mesh_ptr->context,
                           RAL_SHADER_TYPE_COMPUTE,
                           system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(polygonizer_has),
                                                                                   " CS") );

    ogl_shader_set_body(cs,
                        system_hashed_ansi_string_create_by_token_replacement(cs_body,
                                                                              n_token_key_value_pairs,
                                                                              token_key_array_ptr,
                                                                              token_value_array_ptr) );

    delete [] token_key_array_ptr;
    token_key_array_ptr = NULL;

    delete [] token_value_array_ptr;
    token_value_array_ptr = NULL;

    /* Prepare & link the program object */
    mesh_ptr->po_scalar_field_polygonizer = ogl_program_create(mesh_ptr->context,
                                                               system_hashed_ansi_string_create_by_merging_two_strings(system_hashed_ansi_string_get_buffer(polygonizer_has),
                                                                                                                                                 " PO"),
                                                               OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(mesh_ptr->po_scalar_field_polygonizer,
                              cs);
    ogl_program_link         (mesh_ptr->po_scalar_field_polygonizer);

    /* Set up a BO which is going to be used for holding indirect draw call arguments */
    const unsigned int indirect_draw_call_args[] =
    {
        0, /* count - will be filled by the polygonizer CS */
        1, /* primcount                                    */
        0, /* first                                        */
        0  /* baseInstance                                 */
    };
    ral_buffer_create_info indirect_draw_call_args_bo_create_info;
    GLuint                 indirect_draw_call_args_bo_id           = 0;
    raGL_buffer            indirect_draw_call_args_bo_raGL         = NULL;
    uint32_t               indirect_draw_call_args_bo_start_offset = -1;

    ASSERT_DEBUG_SYNC(sizeof(indirect_draw_call_args) == sizeof(unsigned int) * 4,
                      "Invalid invalid draw call arg structure instance size");

    indirect_draw_call_args_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    indirect_draw_call_args_bo_create_info.property_bits    = 0;
    indirect_draw_call_args_bo_create_info.parent_buffer    = NULL;
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

    raGL_backend_get_buffer(mesh_ptr->backend,
                            mesh_ptr->indirect_draw_call_args_bo,
                  (void**) &indirect_draw_call_args_bo_raGL);

    raGL_buffer_get_property(indirect_draw_call_args_bo_raGL,
                             RAGL_BUFFER_PROPERTY_ID,
                            &indirect_draw_call_args_bo_id);
    raGL_buffer_get_property(indirect_draw_call_args_bo_raGL,
                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                            &indirect_draw_call_args_bo_start_offset);

    mesh_ptr->indirect_draw_call_args_bo_count_arg_offset = indirect_draw_call_args_bo_start_offset; /* count is the first argument */

    entrypoints_ptr->pGLBindBuffer   (GL_DRAW_INDIRECT_BUFFER,
                                      indirect_draw_call_args_bo_id);
    entrypoints_ptr->pGLBufferSubData(GL_DRAW_INDIRECT_BUFFER,
                                      indirect_draw_call_args_bo_start_offset,
                                      indirect_draw_call_args_bo_create_info.size,
                                      indirect_draw_call_args);

    /* Set up a BO which is going to hold the polygonized data. At max, each cube is going to hold
     * five triangles. */
    ral_buffer_create_info polygonized_data_bo_create_info;

    polygonized_data_bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    polygonized_data_bo_create_info.parent_buffer    = NULL;
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
    const ogl_program_variable* uniform_isolevel_ptr = NULL;

    ogl_program_get_uniform_block_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                          system_hashed_ansi_string_create("dataUB"),
                                         &mesh_ptr->po_scalar_field_polygonizer_data_ub);

    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &mesh_ptr->po_scalar_field_polygonizer_data_ub_bo);
    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &mesh_ptr->po_scalar_field_polygonizer_data_ub_bo_size);
    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &mesh_ptr->po_scalar_field_polygonizer_data_ub_bp);

    ogl_program_get_uniform_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("isolevel"),
                                   &uniform_isolevel_ptr);

    mesh_ptr->po_scalar_field_polygonizer_data_ub_isolevel_offset = uniform_isolevel_ptr->block_offset;

    /* Set up precomputed tables UB contents */
    const ogl_program_variable* uniform_edge_table_ptr     = NULL;
    const ogl_program_variable* uniform_triangle_table_ptr = NULL;

    ogl_program_get_uniform_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("edge_table[0]"),
                                   &uniform_edge_table_ptr);
    ogl_program_get_uniform_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("triangle_table[0]"),
                                   &uniform_triangle_table_ptr);

    mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = uniform_edge_table_ptr->block_offset;
    mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = uniform_triangle_table_ptr->block_offset;

    ogl_program_get_uniform_block_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                          system_hashed_ansi_string_create("precomputed_tablesUB"),
                                         &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub);

    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_BUFFER_RAL,
                               &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo);
    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_size);
    ogl_program_ub_get_property(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bp);

    /* Set up indirect draw call <count> arg SSB */
    ogl_program_get_shader_storage_block_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("indirect_draw_callSSB"),
                                                &mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb);

    ogl_program_ssb_get_property(mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp);

    /* Set up result data SSB */
    ogl_program_get_shader_storage_block_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("result_dataSSB"),
                                                &mesh_ptr->po_scalar_field_polygonizer_result_data_ssb);

    ogl_program_ssb_get_property(mesh_ptr->po_scalar_field_polygonizer_result_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &mesh_ptr->po_scalar_field_polygonizer_result_data_ssb_bp);

    /* Set up scalar field data SSB */
    ogl_program_get_shader_storage_block_by_name(mesh_ptr->po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("scalar_field_dataSSB"),
                                                &mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb);

    ogl_program_ssb_get_property(mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb_bp);

    /* Release stuff */
    ogl_shader_release(cs);
}

/** TODO */
PRIVATE void _mesh_marchingcubes_init_rendering_thread_callback(ogl_context context,
                                                                void*       user_arg)
{
    ogl_programs              context_programs     = NULL;
    _mesh_marchingcubes*      new_mesh_ptr         = (_mesh_marchingcubes*) user_arg;
    system_hashed_ansi_string polygonizer_name_has = NULL;
    ogl_program               polygonizer_po       = NULL;

    ogl_context_get_property(ral_context_get_gl_context(new_mesh_ptr->context),
                             OGL_CONTEXT_PROPERTY_PROGRAMS,
                            &context_programs);

    /* Initialize the polygonizer, if one has not been already instantiated */
    polygonizer_name_has = _mesh_marchingcubes_get_polygonizer_name(new_mesh_ptr);
    polygonizer_po       = ogl_programs_get_program_by_name        (context_programs,
                                                                    polygonizer_name_has);

    if (polygonizer_po == NULL)
    {
        _mesh_marchingcubes_init_polygonizer_po(new_mesh_ptr);
    } /* if (polygonizer_po == NULL) */
    else
    {
        ogl_program_retain(polygonizer_po);
    }
}

/** TODO */
PRIVATE void _mesh_marchingcubes_release(void* arg)
{
    _mesh_marchingcubes* mesh_ptr = (_mesh_marchingcubes*) arg;

    if (mesh_ptr->material_gpu != NULL)
    {
        mesh_material_release(mesh_ptr->material_gpu);

        mesh_ptr->material_gpu = NULL;
    }
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
    system_hashed_ansi_string material_name = NULL;
    _mesh_marchingcubes*      new_mesh_ptr  = new (std::nothrow) _mesh_marchingcubes();

    ASSERT_DEBUG_SYNC(new_mesh_ptr != NULL,
                      "Out of memory");

    scene_material_get_property(material,
                                SCENE_MATERIAL_PROPERTY_NAME,
                               &material_name);

    if (new_mesh_ptr != NULL)
    {
        uint32_t scalar_data_bo_size = 0;

        ASSERT_DEBUG_SYNC(context != NULL,
                          "Rendering context is NULL");
        ASSERT_DEBUG_SYNC(grid_size_xyz[0] >= 1 &&
                          grid_size_xyz[1] >= 1 &&
                          grid_size_xyz[2] >= 1,
                          "Invalid grid size requested.");
        ASSERT_DEBUG_SYNC(material != NULL,
                          "Input material is NULL");
        ASSERT_DEBUG_SYNC(scalar_data_bo != NULL,
                          "Scalar data BO is NULL");

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
                                                                             NULL); /* object_manager_path */
        new_mesh_ptr->name                            = name;
        new_mesh_ptr->polygonized_data_size_reduction = polygonized_data_size_reduction;
        new_mesh_ptr->scalar_data_bo                  = scalar_data_bo;

        ral_context_get_property(new_mesh_ptr->context,
                                 RAL_CONTEXT_PROPERTY_BACKEND,
                                &new_mesh_ptr->backend);

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

        /* Request a rendering context call-back to set up more interesting stuff */
        ogl_context_request_callback_from_context_thread(ral_context_get_gl_context(new_mesh_ptr->context),
                                                         _mesh_marchingcubes_init_rendering_thread_callback,
                                                         new_mesh_ptr);

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
PUBLIC EMERALD_API void mesh_marchingcubes_get_property(mesh_marchingcubes          in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        void*                       out_result)
{
    const _mesh_marchingcubes* mesh_ptr = (_mesh_marchingcubes*) in_mesh;

    switch (property)
    {
        case MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL:
        {
            *(float*) out_result = mesh_ptr->isolevel;

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_MESH:
        {
            *(mesh*) out_result = mesh_ptr->mesh_instance;

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_SCALAR_DATA_BUFFER_RAL:
        {
            *(ral_buffer*) out_result = mesh_ptr->scalar_data_bo;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh_marchingcubes property requested.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL EMERALD_API void mesh_marchingcubes_polygonize(mesh_marchingcubes in_mesh,
                                                                             bool               has_scalar_field_changed)
{
    GLint                             current_po_id   = 0;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    _mesh_marchingcubes*              mesh_ptr        = (_mesh_marchingcubes*) in_mesh;

    if (mesh_ptr->needs_polygonization ||
        has_scalar_field_changed)
    {
        ogl_context_get_property(ral_context_get_gl_context(mesh_ptr->context),
                                 OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                                &entrypoints_ptr);

        ogl_program_ub_set_nonarrayed_uniform_value(mesh_ptr->po_scalar_field_polygonizer_data_ub,
                                                    mesh_ptr->po_scalar_field_polygonizer_data_ub_isolevel_offset,
                                                   &mesh_ptr->isolevel,
                                                    0, /* src_data_flags */
                                                    sizeof(float) );

        ogl_program_ub_set_arrayed_uniform_value(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                                 mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset,
                                                 _edge_table,
                                                 0, /* src_data_flags */
                                                 sizeof(_edge_table),
                                                 0, /* dst_array_start_index */
                                                 sizeof(_edge_table) / sizeof(_edge_table[0]) );
        ogl_program_ub_set_arrayed_uniform_value(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub,
                                                 mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset,
                                                 _triangle_table,
                                                 0, /* src_data_flags */
                                                 sizeof(_triangle_table),
                                                 0, /* dst_array_start_index */
                                                 sizeof(_triangle_table) / sizeof(_triangle_table[0]) );

        ogl_program_ub_sync(mesh_ptr->po_scalar_field_polygonizer_data_ub);
        ogl_program_ub_sync(mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub);

        /* Set up the SSBO & UBO bindings */
        GLuint      indirect_draw_call_args_bo_id                                     = 0;
        raGL_buffer indirect_draw_call_args_bo_raGL                                   = NULL;
        GLuint      po_scalar_field_polygonizer_data_ub_bo_id                         = 0;
        raGL_buffer po_scalar_field_polygonizer_data_ub_bo_raGL                       = NULL;
        uint32_t    po_scalar_field_polygonizer_data_ub_bo_start_offset               = -1;
        GLuint      po_scalar_field_polygonizer_precomputed_tables_ub_bo_id           = 0;
        raGL_buffer po_scalar_field_polygonizer_precomputed_tables_ub_bo_raGL         = NULL;
        uint32_t    po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset = -1;
        GLuint      polygonized_data_bo_id                                            = 0;
        raGL_buffer polygonized_data_bo_raGL                                          = NULL;
        uint32_t    polygonized_data_bo_size                                          = 0;
        uint32_t    polygonized_data_bo_start_offset                                  = -1;
        GLuint      scalar_data_bo_id                                                 = 0;
        raGL_buffer scalar_data_bo_raGL                                               = NULL;
        uint32_t    scalar_data_bo_size                                               = 0;
        uint32_t    scalar_data_bo_start_offset                                       = -1;

        ASSERT_DEBUG_SYNC(false,
                          "indirect draw call args BO start offset?");


        ral_buffer_get_property(mesh_ptr->polygonized_data_bo,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &polygonized_data_bo_size);
        ral_buffer_get_property(mesh_ptr->scalar_data_bo,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &scalar_data_bo_size);

        raGL_backend_get_buffer(mesh_ptr->backend,
                                mesh_ptr->indirect_draw_call_args_bo,
                      (void**) &indirect_draw_call_args_bo_raGL);
        raGL_backend_get_buffer(mesh_ptr->backend,
                                mesh_ptr->po_scalar_field_polygonizer_data_ub_bo,
                      (void**) &po_scalar_field_polygonizer_data_ub_bo_raGL);
        raGL_backend_get_buffer(mesh_ptr->backend,
                                mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo,
                      (void**) &po_scalar_field_polygonizer_precomputed_tables_ub_bo_raGL);
        raGL_backend_get_buffer(mesh_ptr->backend,
                                mesh_ptr->polygonized_data_bo,
                      (void**) &polygonized_data_bo_raGL);
        raGL_backend_get_buffer(mesh_ptr->backend,
                                mesh_ptr->scalar_data_bo,
                      (void**) &scalar_data_bo_raGL);

        raGL_buffer_get_property(indirect_draw_call_args_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &indirect_draw_call_args_bo_id);

        raGL_buffer_get_property(po_scalar_field_polygonizer_data_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &po_scalar_field_polygonizer_data_ub_bo_id);
        raGL_buffer_get_property(po_scalar_field_polygonizer_data_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &po_scalar_field_polygonizer_data_ub_bo_start_offset);

        raGL_buffer_get_property(po_scalar_field_polygonizer_precomputed_tables_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &po_scalar_field_polygonizer_precomputed_tables_ub_bo_id);
        raGL_buffer_get_property(po_scalar_field_polygonizer_precomputed_tables_ub_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset);

        raGL_buffer_get_property(polygonized_data_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &polygonized_data_bo_id);
        raGL_buffer_get_property(polygonized_data_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &polygonized_data_bo_start_offset);

        raGL_buffer_get_property(scalar_data_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_ID,
                                &scalar_data_bo_id);
        raGL_buffer_get_property(scalar_data_bo_raGL,
                                 RAGL_BUFFER_PROPERTY_START_OFFSET,
                                &scalar_data_bo_start_offset);

        entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            mesh_ptr->po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp,
                                            indirect_draw_call_args_bo_id,
                                            mesh_ptr->indirect_draw_call_args_bo_count_arg_offset,
                                            sizeof(int) );
        entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            mesh_ptr->po_scalar_field_polygonizer_scalar_field_data_ssb_bp,
                                            scalar_data_bo_id,
                                            scalar_data_bo_start_offset,
                                            scalar_data_bo_size);
        entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                            mesh_ptr->po_scalar_field_polygonizer_result_data_ssb_bp,
                                            polygonized_data_bo_id,
                                            polygonized_data_bo_start_offset,
                                            polygonized_data_bo_size);

        entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            mesh_ptr->po_scalar_field_polygonizer_data_ub_bp,
                                            po_scalar_field_polygonizer_data_ub_bo_id,
                                            po_scalar_field_polygonizer_data_ub_bo_start_offset,
                                            mesh_ptr->po_scalar_field_polygonizer_data_ub_bo_size);
        entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                            mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bp,
                                            po_scalar_field_polygonizer_precomputed_tables_ub_bo_id,
                                            po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset,
                                            mesh_ptr->po_scalar_field_polygonizer_precomputed_tables_ub_bo_size);

        /* Sync SSBO-based scalar field data. */
        if (has_scalar_field_changed)
        {
            entrypoints_ptr->pGLMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }

        /* Set up the indirect draw call's <count> SSB. Zero out the value before we run the polygonizer */
        static const int int_zero = 0;

        entrypoints_ptr->pGLBindBuffer        (GL_DRAW_INDIRECT_BUFFER,
                                               indirect_draw_call_args_bo_id);
        entrypoints_ptr->pGLClearBufferSubData(GL_DRAW_INDIRECT_BUFFER,
                                               GL_R32UI,
                                               mesh_ptr->indirect_draw_call_args_bo_count_arg_offset,
                                               sizeof(int),
                                               GL_RED_INTEGER,
                                               GL_UNSIGNED_INT,
                                              &int_zero);

        /* Polygonize the scalar field */
        entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(mesh_ptr->po_scalar_field_polygonizer) );
        entrypoints_ptr->pGLDispatchCompute(mesh_ptr->po_scalar_field_polygonizer_global_wg_size[0],
                                            mesh_ptr->po_scalar_field_polygonizer_global_wg_size[1],
                                            mesh_ptr->po_scalar_field_polygonizer_global_wg_size[2]);

        /* Restore the bound program object */
        entrypoints_ptr->pGLUseProgram(current_po_id);

        /* All done */
        mesh_ptr->needs_polygonization = false;
    } /* if (mesh_ptr->needs_polygonization) */
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_marchingcubes_set_property(mesh_marchingcubes          in_mesh,
                                                        mesh_marchingcubes_property property,
                                                        const void*                 data)
{
    _mesh_marchingcubes* mesh_ptr = (_mesh_marchingcubes*) in_mesh;

    switch (property)
    {
        case MESH_MARCHINGCUBES_PROPERTY_ISOLEVEL:
        {
            float new_isolevel = *(float*) data;

            if (abs(mesh_ptr->isolevel - new_isolevel) > 1e-5f)
            {
                mesh_ptr->isolevel             = new_isolevel;
                mesh_ptr->needs_polygonization = true;
            }

            break;
        }

        case MESH_MARCHINGCUBES_PROPERTY_SCALAR_DATA_BUFFER_RAL:
        {
            ral_buffer new_bo = *(ral_buffer*) data;

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
    } /* switch (property) */
}
