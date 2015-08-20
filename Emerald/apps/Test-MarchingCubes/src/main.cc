/**
 *
 * Marching cubes test app (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ogl/ogl_buffers.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_flyby.h"
#include "ogl/ogl_program.h"
#include "ogl/ogl_program_ssb.h"
#include "ogl/ogl_program_ub.h"
#include "ogl/ogl_rendering_handler.h"
#include "ogl/ogl_scene_renderer.h"
#include "ogl/ogl_shader.h"
#include "ogl/ogl_texture.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "scene/scene_mesh.h"
#include "system/system_assertions.h"
#include "system/system_event.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_pixel_format.h"
#include "system/system_screen_mode.h"
#include "system/system_window.h"
#include <algorithm>
#include "main.h"

PRIVATE const unsigned int _blob_size[] = {100, 100, 100};

PRIVATE GLuint              _blob_cube_properties_bo_id                                                 = 0;
PRIVATE unsigned int        _blob_cube_properties_bo_size                                               = 0;
PRIVATE unsigned int        _blob_cube_properties_bo_start_offset                                       = 0;
PRIVATE mesh                _blob_mesh                                                                  = NULL;
PRIVATE GLuint              _blob_scalar_field_bo_id                                                    = 0;
PRIVATE unsigned int        _blob_scalar_field_bo_size                                                  = 0;
PRIVATE unsigned int        _blob_scalar_field_bo_start_offset                                          = 0;
PRIVATE ogl_context         _context                                                                    = NULL;
PRIVATE ogl_flyby           _context_flyby                                                              = NULL;
PRIVATE unsigned int        _indirect_draw_call_args_bo_count_arg_offset                                = 0;
PRIVATE GLuint              _indirect_draw_call_args_bo_id                                              = 0;
PRIVATE unsigned int        _indirect_draw_call_args_bo_size                                            = 0;
PRIVATE unsigned int        _indirect_draw_call_args_bo_start_offset                                    = 0;
PRIVATE ogl_program         _po_scalar_field_generator                                                  = NULL;
PRIVATE ogl_program_ub      _po_scalar_field_generator_data_ub                                          = NULL;
PRIVATE ogl_program         _po_scalar_field_polygonizer                                                = NULL;
PRIVATE ogl_program_ssb     _po_scalar_field_polygonizer_cube_properties_ssb                            = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_cube_properties_ssb_bp                         = -1;
PRIVATE ogl_program_ub      _po_scalar_field_polygonizer_data_ub                                        = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_bo_id                                  = 0;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_bo_size                                = 0;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_bo_start_offset                        = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_bp                                     = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_isolevel_offset                        = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_mvp_offset                             = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_data_ub_normal_matrix_offset                   = -1;
PRIVATE unsigned int        _po_scalar_field_polygonizer_global_wg_size[3];
PRIVATE ogl_program_ssb     _po_scalar_field_polygonizer_indirect_draw_call_count_ssb                   = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp                = -1;
PRIVATE ogl_program_ub      _po_scalar_field_polygonizer_precomputed_tables_ub                          = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bo_id                    = 0;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bo_size                  = 0;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset          = 0;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = -1;
PRIVATE GLuint              _po_scalar_field_polygonizer_precomputed_tables_ub_bp                       = -1;
PRIVATE ogl_program_ssb     _po_scalar_field_polygonizer_result_data_ssb                                = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_result_data_ssb_bp                             = NULL;
PRIVATE ogl_program_ssb     _po_scalar_field_polygonizer_scalar_field_data_ssb                          = NULL;
PRIVATE GLuint              _po_scalar_field_polygonizer_scalar_field_data_ssb_bp                       = NULL;
PRIVATE ogl_program         _po_scalar_field_renderer                                                   = NULL;
PRIVATE GLuint              _polygonized_data_bo_id                                                     = 0;
PRIVATE unsigned int        _polygonized_data_bo_size                                                   = 0;
PRIVATE unsigned int        _polygonized_data_bo_start_offset                                           = 0;
PRIVATE system_matrix4x4    _projection_matrix                                                          = NULL;
PRIVATE bool                _scalar_field_data_updated                                                  = false;
PRIVATE scene               _scene                                                                      = NULL;
PRIVATE scene_graph_node    _scene_blob_node                                                            = NULL;
PRIVATE scene_camera        _scene_camera                                                               = NULL;
PRIVATE scene_graph_node    _scene_camera_node                                                          = NULL;
PRIVATE scene_graph         _scene_graph                                                                = NULL; /* do not release */
PRIVATE ogl_scene_renderer  _scene_renderer                                                             = NULL;
PRIVATE system_event        _window_closed_event                                                        = system_event_create(true); /* manual_reset */
PRIVATE int                 _window_size[2]                                                             = {1280, 720};
PRIVATE GLuint              _vao_id                                                                     = 0;
PRIVATE system_matrix4x4    _view_matrix                                                                = NULL;

/* Edge array for the marching cubes algorithm. */
const int _edge_table[256]={0x0  , 0x109, 0x203, 0x30a, 0x406, 0x50f, 0x605, 0x70c,
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

/* Blob triangle array. Adapted from http://paulbourke.net/geometry/polygonise/ */
const int _triangle_table[256 * 15] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
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

/* Forward declarations */
PRIVATE void _get_blob_bounding_box_aabb     (void*                             user_arg,
                                              float**                           out_aabb_model_vec4_min,
                                              float**                           out_aabb_model_vec4_max);
PRIVATE void _get_token_key_value_arrays     (const ogl_context_gl_limits*      limits_ptr,
                                              system_hashed_ansi_string**       out_token_key_array_ptr,
                                              system_hashed_ansi_string**       out_token_value_array_ptr,
                                              unsigned int*                     out_n_token_key_value_pairs_ptr,
                                              unsigned int*                     out_global_wg_size_uvec3_ptr);
PRIVATE void _init_scalar_field              (const ogl_context_gl_entrypoints* entry_points,
                                              const ogl_context_gl_limits*      limits);
PRIVATE void _init_scalar_field_polygonizer  (const ogl_context_gl_entrypoints* entry_points,
                                              const ogl_context_gl_limits*      limits_ptr);
PRIVATE void _init_scalar_field_renderer     (const ogl_context_gl_entrypoints* entry_points);
PRIVATE void _init_scene                     ();
PRIVATE void _render_blob                    (ogl_context                       context,
                                              const void*                       user_arg,
                                              const system_matrix4x4            model_matrix,
                                              const system_matrix4x4            vp_matrix,
                                              const system_matrix4x4            normal_matrix,
                                              bool                              is_depth_prepass);
PRIVATE void _rendering_handler              (ogl_context                       context,
                                              uint32_t                          n_frames_rendered,
                                              system_time                       frame_time,
                                              const int*                        rendering_area_px_topdown,
                                              void*                             renderer);
PRIVATE bool _rendering_rbm_callback_handler (system_window                     window,
                                              unsigned short                    x,
                                              unsigned short                    y,
                                              system_window_vk_status           new_status,
                                              void*);
PRIVATE void _window_closed_callback_handler (system_window                     window);
PRIVATE void _window_closing_callback_handler(system_window                     window);


/** TODO */
PRIVATE void _get_blob_bounding_box_aabb(const void* user_arg,
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
PRIVATE void _get_token_key_value_arrays(const ogl_context_gl_limits* limits_ptr,
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
    const uint32_t n_total_scalars  = _blob_size[0] * _blob_size[1] * _blob_size[2];
    const uint32_t wg_local_size_x  = limits_ptr->max_compute_work_group_size[0]; /* TODO: smarterize me */
    const uint32_t wg_local_size_y  = 1;
    const uint32_t wg_local_size_z  = 1;

    out_global_wg_size_uvec3_ptr[0] = 1 + n_total_scalars / wg_local_size_x;
    out_global_wg_size_uvec3_ptr[1] = 1;
    out_global_wg_size_uvec3_ptr[2] = 1;

    ASSERT_DEBUG_SYNC(wg_local_size_x * wg_local_size_y * wg_local_size_z <= limits_ptr->max_compute_work_group_invocations,
                      "Invalid local work-group size requested");
    ASSERT_DEBUG_SYNC(out_global_wg_size_uvec3_ptr[0] < limits_ptr->max_compute_work_group_count[0] &&
                      out_global_wg_size_uvec3_ptr[1] < limits_ptr->max_compute_work_group_count[1] &&
                      out_global_wg_size_uvec3_ptr[2] < limits_ptr->max_compute_work_group_count[2],
                      "Invalid global work-group size requested");

    /* Fill the token value array */
    char temp_buffer[64];

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_x);
    (*out_token_value_array_ptr)[0] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_y);
    (*out_token_value_array_ptr)[1] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             wg_local_size_z);
    (*out_token_value_array_ptr)[2] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[0]);
    (*out_token_value_array_ptr)[3] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[1]);
    (*out_token_value_array_ptr)[4] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             _blob_size[2]);
    (*out_token_value_array_ptr)[5] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             out_global_wg_size_uvec3_ptr[0]);
    (*out_token_value_array_ptr)[6] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             out_global_wg_size_uvec3_ptr[1]);
    (*out_token_value_array_ptr)[7] = system_hashed_ansi_string_create(temp_buffer);

    snprintf(temp_buffer,
             sizeof(temp_buffer),
             "%d",
             out_global_wg_size_uvec3_ptr[2]);
    (*out_token_value_array_ptr)[8] = system_hashed_ansi_string_create(temp_buffer);
}

/** TODO */
PRIVATE void _init_scalar_field(const ogl_context_gl_entrypoints* entrypoints_ptr,
                                const ogl_context_gl_limits*      limits_ptr)
{
    ogl_buffers buffers          = NULL;
    ogl_shader  cs               = NULL;
    const char* cs_body_template = "#version 430 core\n"
                                   "\n"
                                   "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
                                   "\n"
                                   "layout(std430) writeonly buffer data\n"
                                   "{\n"
                                   /* TODO: SIMDify me */
                                   "    restrict float result[];\n"
                                   "};\n"
                                   "\n"
                                   /* TODO: Uniformify me */
                                   "const uvec3 blob_size             = uvec3(BLOB_SIZE_X,      BLOB_SIZE_Y,      BLOB_SIZE_Z);\n"
                                   "const uvec3 global_workgroup_size = uvec3(GLOBAL_WG_SIZE_X, GLOBAL_WG_SIZE_Y, GLOBAL_WG_SIZE_Z);\n"
                                   "\n"
                                   "void main()\n"
                                   "{\n"
                                   "    struct\n" /* TODO: Constifying this on Intel driver causes compilation errors? */
                                   "    {\n"
                                   "        vec3  xyz;\n"
                                   "        float radius;\n"
                                   "    } spheres[3] =\n"
                                   "    {\n"
                                   "        {vec3(0.17, 0.17, 0.17), 0.1 },\n"  /* corner case lol */
                                   "        {vec3(0.3,  0.3,  0.3),  0.2 },\n"
                                   "        {vec3(0.5,  0.5,  0.5),  0.4 } };\n"
                                   "    const uint n_spheres = 3;\n"
                                   "    \n"
                                   "    const uint global_invocation_id_flat = (gl_GlobalInvocationID.z * (LOCAL_WG_SIZE_X * LOCAL_WG_SIZE_Y) +\n"
                                   "                                            gl_GlobalInvocationID.y * (LOCAL_WG_SIZE_X)                   +\n"
                                   "                                            gl_GlobalInvocationID.x);\n"
                                   "    const uint cube_x =  global_invocation_id_flat                                % blob_size.x;\n"
                                   "    const uint cube_y = (global_invocation_id_flat /  blob_size.x)                % blob_size.y;\n"
                                   "    const uint cube_z = (global_invocation_id_flat / (blob_size.x * blob_size.y));\n"
                                   "\n"
                                   "    if (cube_z >= blob_size.z)\n"
                                   "    {\n"
                                   "        return;\n"
                                   "    }\n"
                                   "\n"
                                   "    const float cube_x_normalized = (float(cube_x) + 0.5) / float(blob_size.x - 1);\n"
                                   "    const float cube_y_normalized = (float(cube_y) + 0.5) / float(blob_size.y - 1);\n"
                                   "    const float cube_z_normalized = (float(cube_z) + 0.5) / float(blob_size.z - 1);\n"
                                   "\n"
                                   "    float max_power = 0.0;\n"
                                   "\n"
                                   "    for (uint n_sphere = 0;\n"
                                   "              n_sphere < n_spheres;\n"
                                   "            ++n_sphere)\n"
                                   "    {\n"
                                   "        vec3  delta = vec3(cube_x_normalized, cube_y_normalized, cube_z_normalized) - spheres[n_sphere].xyz;\n"
                                   "        float power = 1.0 - clamp(length(delta) / spheres[n_sphere].radius, 0.0, 1.0);\n"
                                   "\n"
                                   "        max_power = max(max_power, power);\n"
                                   "    }\n"
                                   "\n"
                                   "    result[global_invocation_id_flat] = max_power;\n"
                                   "}";

    unsigned int               global_wg_size[3]       = {0};
    unsigned int               n_token_key_value_pairs = 0;
    system_hashed_ansi_string* token_key_array_ptr     = NULL;
    system_hashed_ansi_string* token_value_array_ptr   = NULL;

    _get_token_key_value_arrays(limits_ptr,
                               &token_key_array_ptr,
                               &token_value_array_ptr,
                               &n_token_key_value_pairs,
                                global_wg_size);

    /* Create program & shader objects */
    cs = ogl_shader_create(_context,
                           SHADER_TYPE_COMPUTE,
                           system_hashed_ansi_string_create("Scalar field generator") );

    _po_scalar_field_generator = ogl_program_create(_context,
                                                    system_hashed_ansi_string_create("Scalar field generator"),
                                                    OGL_PROGRAM_SYNCABLE_UBS_MODE_DISABLE);

    /* Configure the shader object */
    ogl_shader_set_body_with_token_replacement(cs,
                                               cs_body_template,
                                               n_token_key_value_pairs,
                                               token_key_array_ptr,
                                               token_value_array_ptr);

    delete [] token_key_array_ptr;
    token_key_array_ptr = NULL;

    delete [] token_value_array_ptr;
    token_value_array_ptr = NULL;

    /* Configure & link the program object */
    ogl_program_attach_shader(_po_scalar_field_generator,
                              cs);

    ogl_program_link(_po_scalar_field_generator);

    /* Prepare a BO which is going to hold the scalar field data */
    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    _blob_scalar_field_bo_size = sizeof(float) * _blob_size[0] * _blob_size[1] * _blob_size[2];

    ogl_buffers_allocate_buffer_memory(buffers,
                                       _blob_scalar_field_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &_blob_scalar_field_bo_id,
                                      &_blob_scalar_field_bo_start_offset);

    /* Run the CS and generate the scalar field data */
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_po_scalar_field_generator) );
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        0, /* index */
                                        _blob_scalar_field_bo_id,
                                        _blob_scalar_field_bo_start_offset,
                                        _blob_scalar_field_bo_size);

    entrypoints_ptr->pGLDispatchCompute(global_wg_size[0],
                                        global_wg_size[1],
                                        global_wg_size[2]);

    _scalar_field_data_updated = true;

    /* All done! */
    ogl_shader_release(cs);
}

/** TODO */
PRIVATE void _init_scalar_field_polygonizer(const ogl_context_gl_entrypoints* entry_points,
                                            const ogl_context_gl_limits*      limits_ptr)
{
    const char* cs_body = "#version 430 core\n"
                          "\n"
                          "layout(local_size_x = LOCAL_WG_SIZE_X, local_size_y = LOCAL_WG_SIZE_Y, local_size_z = LOCAL_WG_SIZE_Z) in;\n"
                          "\n"
                          "const uint n_ids_per_row    = BLOB_SIZE_X;\n"
                          "const uint n_ids_per_slice  = BLOB_SIZE_X * BLOB_SIZE_Y;\n"
                          "\n"
                          "layout(std430) buffer cube_propertiesSSB\n"
                          "{\n"
                          "    int data_start_index[];\n"
                          "};\n"
                          "\n"
                          "layout(packed) uniform dataUB\n"
                          "{\n"
                          "    float isolevel;\n"
                          "    mat4  mvp;\n"
                          "    mat4  normal_matrix;\n"
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
                          /* 4 floats: vertex data (clip space)
                           * 2 floats: normal data */
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
                          "                           out vec4  result_vertex)\n"
                          "{\n"
                          /* TODO: Use the improved version? */
                          "    uvec3 vertex1_preceding_step_size  = uvec3(clamp(base_index1 - BLOB_SIZE_X / 10 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index1 - BLOB_SIZE_Y / 10 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index1 - BLOB_SIZE_Z / 10 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
                          "    uvec3 vertex1_proceeding_step_size = uvec3(clamp(base_index1 + BLOB_SIZE_X / 10 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index1 + BLOB_SIZE_Y / 10 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index1 + BLOB_SIZE_Z / 10 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
                          "    uvec3 vertex2_preceding_step_size  = uvec3(clamp(base_index2 - BLOB_SIZE_X / 10 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index2 - BLOB_SIZE_Y / 10 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index2 - BLOB_SIZE_Z / 10 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
                          "    uvec3 vertex2_proceeding_step_size = uvec3(clamp(base_index2 + BLOB_SIZE_X / 10 * 1,               0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index2 + BLOB_SIZE_Y / 10 * n_ids_per_row,   0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1),\n"
                          "                                               clamp(base_index2 + BLOB_SIZE_Z / 10 * n_ids_per_slice, 0, BLOB_SIZE_X * BLOB_SIZE_Y * BLOB_SIZE_Z - 1) );\n"
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
                          "        result_normal = normalize( (normal_matrix * vec4(vertex1_normal, 1.0) ).xyz);\n"
                          "        result_vertex = mvp * vec4(vertex1, 1.0);\n"
                          "    }\n"
                          "    else\n"
                          "    if (abs(isolevel - vertex2_value) < 1e-5)\n"
                          "    {\n"
                          "        result_normal = normalize( (normal_matrix * vec4(vertex2_normal, 1.0) ).xyz);\n"
                          "        result_vertex = mvp * vec4(vertex2, 1.0);\n"
                          "    }\n"
                          "    else\n"
                          "    {\n"
                          "        float coeff = (isolevel - vertex1_value) / (vertex2_value - vertex1_value);\n"
                          "\n"
                          "        result_normal = normalize((normal_matrix * vec4(vertex1_normal + vec3(coeff) * (vertex2_normal - vertex1_normal), 1.0)).xyz);\n"
                          "        result_vertex =            mvp           * vec4(vertex1        + vec3(coeff) * (vertex2        - vertex1),        1.0);\n"
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
                          "        data_start_index[global_invocation_id_flat] = -1;\n"
                          "\n"
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
                          "        data_start_index[global_invocation_id_flat] = -1;\n"
                          "\n"
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
                          "          vec4 lerped_vertex_list[12];\n"
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
                          "    data_start_index[global_invocation_id_flat] = int(n_result_triangle_base_vertex);\n"
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
                          "                result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 3] = 0.0;\n"
                          "            }\n"
                          "        }\n"
                          "        else\n"
                          "        for (uint n_vertex = 0;\n"
                          "                  n_vertex < 3;\n"
                          "                  n_vertex++)\n"
                          "        {\n"
                          "            const int  list_index     = triangle_table    [ n_triangle_base_vertex + n_vertex ];\n"
                          "            const vec3 current_normal = lerped_normal_list[ list_index ];\n"
                          "            const vec4 current_vertex = lerped_vertex_list[ list_index ];\n"
                          "\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 0] = current_vertex.x;\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 1] = current_vertex.y;\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 2] = current_vertex.z;\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 3] = current_vertex.w;\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 4] = current_normal.x;\n"
                          "            result_data[(n_result_triangle_start_vertex + n_vertex) * 6 + 5] = current_normal.y;\n"
                          "        }\n"
                          "    }\n"
                          "}\n";

    /* Set up token key/value arrays */
    unsigned int               n_token_key_value_pairs = 0;
    system_hashed_ansi_string* token_key_array_ptr     = NULL;
    system_hashed_ansi_string* token_value_array_ptr   = NULL;

    _get_token_key_value_arrays(limits_ptr,
                               &token_key_array_ptr,
                               &token_value_array_ptr,
                               &n_token_key_value_pairs,
                                _po_scalar_field_polygonizer_global_wg_size);

    /* Initialize the shader */
    ogl_shader cs = ogl_shader_create(_context,
                                      SHADER_TYPE_COMPUTE,
                                      system_hashed_ansi_string_create("Scalar field polygonizer CS"));

    ogl_shader_set_body_with_token_replacement(cs,
                                               cs_body,
                                               n_token_key_value_pairs,
                                               token_key_array_ptr,
                                               token_value_array_ptr);

    delete [] token_key_array_ptr;
    token_key_array_ptr = NULL;

    delete [] token_value_array_ptr;
    token_value_array_ptr = NULL;

    /* Prepare & link the program object */
    _po_scalar_field_polygonizer = ogl_program_create(_context,
                                                      system_hashed_ansi_string_create("Scalar field renderer"),
                                                      OGL_PROGRAM_SYNCABLE_UBS_MODE_ENABLE_GLOBAL);

    ogl_program_attach_shader(_po_scalar_field_polygonizer,
                              cs);
    ogl_program_link         (_po_scalar_field_polygonizer);

    /* Set up a BO which is going to be used for the indirect draw call in the rendering stage */
    ogl_buffers        buffers                   = NULL;
    const unsigned int indirect_draw_call_args[] =
    {
        0, /* count - will be filled later */
        1, /* primcount                    */
        0, /* first                        */
        0  /* baseInstance                 */
    };
    ASSERT_DEBUG_SYNC(sizeof(indirect_draw_call_args) == sizeof(unsigned int) * 4,
                      "Invalid invalid draw call arg structure instance size");

    ogl_context_get_property(_context,
                             OGL_CONTEXT_PROPERTY_BUFFERS,
                            &buffers);

    ogl_buffers_allocate_buffer_memory(buffers,
                                       sizeof(unsigned int) * 4,
                                       limits_ptr->shader_storage_buffer_offset_alignment, /* SSBO alignment is needed, since the compute shader modifies some of the args */
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &_indirect_draw_call_args_bo_id,
                                      &_indirect_draw_call_args_bo_start_offset);

    _indirect_draw_call_args_bo_count_arg_offset = _indirect_draw_call_args_bo_start_offset; /* count is the first argument */
    _indirect_draw_call_args_bo_size             = sizeof(unsigned int) * 4;

    entry_points->pGLBindBuffer   (GL_DRAW_INDIRECT_BUFFER,
                                  _indirect_draw_call_args_bo_id);
    entry_points->pGLBufferSubData(GL_DRAW_INDIRECT_BUFFER,
                                   _indirect_draw_call_args_bo_start_offset,
                                   _indirect_draw_call_args_bo_size,
                                   indirect_draw_call_args);

    /* Set up a BO which is going to hold the polygonized data. At max, each cube is going to hold
     * five triangles. */
    _polygonized_data_bo_size = _blob_size[0] * _blob_size[1] * _blob_size[2]         *
                                5 /* triangles */                                     *
                                3 /* vertices */                                      *
                                6 /* vertex + nonnormalized normal data components */ *
                                sizeof(float)                                         /
                                20; /* NOTE: This reduces the allocated memory but may cause parts of the blob not to be polygonized! */

    ogl_buffers_allocate_buffer_memory(buffers,
                                       _polygonized_data_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &_polygonized_data_bo_id,
                                      &_polygonized_data_bo_start_offset);

    /* Set up a BO which is going to hold voxel properties. We need 32 bits for each cube. */
    _blob_cube_properties_bo_size = _blob_size[0] * _blob_size[1] * _blob_size[2] * sizeof(int);

    ogl_buffers_allocate_buffer_memory(buffers,
                                       _blob_cube_properties_bo_size,
                                       limits_ptr->shader_storage_buffer_offset_alignment,
                                       OGL_BUFFERS_MAPPABILITY_NONE,
                                       OGL_BUFFERS_USAGE_MISCELLANEOUS,
                                       0, /* flags */
                                      &_blob_cube_properties_bo_id,
                                      &_blob_cube_properties_bo_start_offset);

    /* Retrieve data UB properties */
    const ogl_program_variable* uniform_isolevel_ptr      = NULL;
    const ogl_program_variable* uniform_mvp_ptr           = NULL;
    const ogl_program_variable* uniform_normal_matrix_ptr = NULL;

    ogl_program_get_uniform_block_by_name(_po_scalar_field_polygonizer,
                                          system_hashed_ansi_string_create("dataUB"),
                                         &_po_scalar_field_polygonizer_data_ub);

    ogl_program_ub_get_property(_po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &_po_scalar_field_polygonizer_data_ub_bo_id);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_po_scalar_field_polygonizer_data_ub_bo_size);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &_po_scalar_field_polygonizer_data_ub_bo_start_offset);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_data_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &_po_scalar_field_polygonizer_data_ub_bp);

    ogl_program_get_uniform_by_name(_po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("isolevel"),
                                   &uniform_isolevel_ptr);
    ogl_program_get_uniform_by_name(_po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("mvp"),
                                   &uniform_mvp_ptr);
    ogl_program_get_uniform_by_name(_po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("normal_matrix"),
                                   &uniform_normal_matrix_ptr);

    _po_scalar_field_polygonizer_data_ub_isolevel_offset      = uniform_isolevel_ptr->block_offset;
    _po_scalar_field_polygonizer_data_ub_mvp_offset           = uniform_mvp_ptr->block_offset;
    _po_scalar_field_polygonizer_data_ub_normal_matrix_offset = uniform_normal_matrix_ptr->block_offset;

    /* Set up precomputed tables UB contents */
    const ogl_program_variable* uniform_edge_table_ptr     = NULL;
    const ogl_program_variable* uniform_triangle_table_ptr = NULL;

    ogl_program_get_uniform_by_name(_po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("edge_table[0]"),
                                   &uniform_edge_table_ptr);
    ogl_program_get_uniform_by_name(_po_scalar_field_polygonizer,
                                    system_hashed_ansi_string_create("triangle_table[0]"),
                                   &uniform_triangle_table_ptr);

    _po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset     = uniform_edge_table_ptr->block_offset;
    _po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset = uniform_triangle_table_ptr->block_offset;

    ogl_program_get_uniform_block_by_name(_po_scalar_field_polygonizer,
                                          system_hashed_ansi_string_create("precomputed_tablesUB"),
                                         &_po_scalar_field_polygonizer_precomputed_tables_ub);

    ogl_program_ub_get_property(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_ID,
                               &_po_scalar_field_polygonizer_precomputed_tables_ub_bo_id);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_BLOCK_DATA_SIZE,
                               &_po_scalar_field_polygonizer_precomputed_tables_ub_bo_size);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_BO_START_OFFSET,
                               &_po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset);
    ogl_program_ub_get_property(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                OGL_PROGRAM_UB_PROPERTY_INDEXED_BP,
                               &_po_scalar_field_polygonizer_precomputed_tables_ub_bp);

    /* Set up cube properties SSB */
    ogl_program_get_shader_storage_block_by_name(_po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("cube_propertiesSSB"),
                                                &_po_scalar_field_polygonizer_cube_properties_ssb);

    ogl_program_ssb_get_property(_po_scalar_field_polygonizer_cube_properties_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &_po_scalar_field_polygonizer_cube_properties_ssb_bp);

    /* Set up indirect draw call <count> arg SSB */
    ogl_program_get_shader_storage_block_by_name(_po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("indirect_draw_callSSB"),
                                                &_po_scalar_field_polygonizer_indirect_draw_call_count_ssb);

    ogl_program_ssb_get_property(_po_scalar_field_polygonizer_indirect_draw_call_count_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &_po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp);

    /* Set up result data SSB */
    ogl_program_get_shader_storage_block_by_name(_po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("result_dataSSB"),
                                                &_po_scalar_field_polygonizer_result_data_ssb);

    ogl_program_ssb_get_property(_po_scalar_field_polygonizer_result_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &_po_scalar_field_polygonizer_result_data_ssb_bp);

    /* Set up scalar field data SSB */
    ogl_program_get_shader_storage_block_by_name(_po_scalar_field_polygonizer,
                                                 system_hashed_ansi_string_create("scalar_field_dataSSB"),
                                                &_po_scalar_field_polygonizer_scalar_field_data_ssb);

    ogl_program_ssb_get_property(_po_scalar_field_polygonizer_scalar_field_data_ssb,
                                 OGL_PROGRAM_SSB_PROPERTY_INDEXED_BP,
                                &_po_scalar_field_polygonizer_scalar_field_data_ssb_bp);

    /* Set up a VAO which is going to be used to render the metaballs */
    ASSERT_DEBUG_SYNC(_polygonized_data_bo_id != 0,
                      "Polygonized data BO id is 0");

    entry_points->pGLGenVertexArrays(1,
                                    &_vao_id);

    entry_points->pGLBindVertexArray(_vao_id);

    entry_points->pGLBindBuffer(GL_ARRAY_BUFFER,
                                _polygonized_data_bo_id);

    entry_points->pGLVertexAttribPointer(0,                 /* index      */
                                         4,                 /* size       */
                                         GL_FLOAT,          /* type       */
                                         GL_FALSE,          /* normalized */
                                         sizeof(float) * 6, /* stride     */
                                         (const GLvoid*) _polygonized_data_bo_start_offset);
    entry_points->pGLVertexAttribPointer(1,                 /* index      */
                                         2,                 /* size       */
                                         GL_FLOAT,          /* type       */
                                         GL_FALSE,          /* normalized */
                                         sizeof(float) * 6, /* stride     */
                                         (const GLvoid*) (_polygonized_data_bo_start_offset + sizeof(float) * 4) );

    entry_points->pGLEnableVertexAttribArray(0); /* index */
    entry_points->pGLEnableVertexAttribArray(1); /* index */

    /* Release stuff */
    ogl_shader_release(cs);
}

/** TODO */
PRIVATE void _init_scalar_field_renderer(const ogl_context_gl_entrypoints* entry_points)
{
    const char* fs_code = "#version 430 core\n"
                          "\n"
                          "in  vec3 lerped_normal;\n"
                          "out vec3 out_result;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    out_result = abs(lerped_normal);\n"
                          "}\n";
    const char* vs_code = "#version 430 core\n"
                          "\n"
                          "layout(location = 1) in vec2 normal;\n"
                          "layout(location = 0) in vec4 vertex;\n"
                          "\n"
                          "out vec3 lerped_normal;\n"
                          "\n"
                          "void main()\n"
                          "{\n"
                          "    gl_Position   = vertex;\n"
                          "    lerped_normal = vec3(normal.xy, sqrt(1 - length(normal.xy)) );\n"
                          "}\n";

    /* Set up shader objects */
    ogl_shader fs = ogl_shader_create(_context,
                                      SHADER_TYPE_FRAGMENT,
                                      system_hashed_ansi_string_create("Scalar field renderer FS") );
    ogl_shader vs = ogl_shader_create(_context,
                                      SHADER_TYPE_VERTEX,
                                      system_hashed_ansi_string_create("Scalar field renderer VS") );

    ogl_shader_set_body(fs,
                        system_hashed_ansi_string_create(fs_code) );
    ogl_shader_set_body(vs,
                        system_hashed_ansi_string_create(vs_code) );

    /* Set up the program object */
    _po_scalar_field_renderer = ogl_program_create(_context,
                                                   system_hashed_ansi_string_create("Scalar field renderer PO") );

    ogl_program_attach_shader(_po_scalar_field_renderer,
                              fs);
    ogl_program_attach_shader(_po_scalar_field_renderer,
                              vs);

    ogl_program_link(_po_scalar_field_renderer);

    /* Clean up */
    ogl_shader_release(fs);
    ogl_shader_release(vs);

    /**/
}

/** TODO */
PRIVATE void _init_scene()
{
    scene_mesh blob_scene_mesh = NULL;

    /* Set up the test mesh instance */
    _blob_mesh = mesh_create_custom_mesh(&_render_blob,
                                          NULL, /* render_custom_mesh_proc_user_arg */
                                         &_get_blob_bounding_box_aabb,
                                          NULL, /* get_custom_mesh_aabb_proc_user_arg */
                                          system_hashed_ansi_string_create("Test blob") );

    /* Set up the scene */
    system_hashed_ansi_string blob_has       = system_hashed_ansi_string_create("Test blob");
    system_hashed_ansi_string camera_has     = system_hashed_ansi_string_create("Test camera");
    system_hashed_ansi_string test_scene_has = system_hashed_ansi_string_create("Test scene");

    _scene        = scene_create       (_context,
                                        test_scene_has);
    _scene_camera = scene_camera_create(camera_has,
                                        test_scene_has);

    scene_get_property(_scene,
                       SCENE_PROPERTY_GRAPH,
                      &_scene_graph);

    scene_add_mesh_instance(_scene,
                            _blob_mesh,
                            blob_has,
                           &blob_scene_mesh);

    /* Set up the scene graph */
    system_matrix4x4 identity_matrix = system_matrix4x4_create();

    system_matrix4x4_set_to_identity(identity_matrix);

    _scene_blob_node   = scene_graph_create_general_node                        (_scene_graph);
    _scene_camera_node = scene_graph_create_static_matrix4x4_transformation_node(_scene_graph,
                                                                                 identity_matrix,
                                                                                 SCENE_GRAPH_NODE_TAG_UNDEFINED);

    system_matrix4x4_release(identity_matrix);
    identity_matrix = NULL;

    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_blob_node,
                                      SCENE_OBJECT_TYPE_MESH,
                                      blob_scene_mesh);
    scene_graph_attach_object_to_node(_scene_graph,
                                      _scene_camera_node,
                                      SCENE_OBJECT_TYPE_CAMERA,
                                      _scene_camera);

    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_camera_node);
    scene_graph_add_node(_scene_graph,
                         scene_graph_get_root_node(_scene_graph),
                         _scene_blob_node);

    /* Set up the scene renderer */
    _scene_renderer = ogl_scene_renderer_create(_context,
                                                _scene);
}

/** TODO */
PRIVATE void _render_blob(ogl_context             context,
                          const void*             user_arg,
                          const system_matrix4x4  model_matrix,
                          const system_matrix4x4  vp_matrix,
                          const system_matrix4x4  normal_matrix,
                          bool                    is_depth_prepass)
{
    static int                        counter         = 0;
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;

    float isolevel = sin(float((counter++) % 2500) / 10000.0f) * 0.5f + 0.1f;
    //float isolevel = 0.2f;

    /* Compute MVP matrix */
    system_matrix4x4 mvp = system_matrix4x4_create_by_mul(vp_matrix,
                                                          model_matrix);

    /* Set up the unifrom block contents */
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);

    ogl_program_ub_set_nonarrayed_uniform_value(_po_scalar_field_polygonizer_data_ub,
                                                _po_scalar_field_polygonizer_data_ub_mvp_offset,
                                                system_matrix4x4_get_column_major_data(mvp),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(_po_scalar_field_polygonizer_data_ub,
                                                _po_scalar_field_polygonizer_data_ub_normal_matrix_offset,
                                                system_matrix4x4_get_column_major_data(normal_matrix),
                                                0, /* src_data_flags */
                                                sizeof(float) * 16);
    ogl_program_ub_set_nonarrayed_uniform_value(_po_scalar_field_polygonizer_data_ub,
                                                _po_scalar_field_polygonizer_data_ub_isolevel_offset,
                                               &isolevel,
                                                0, /* src_data_flags */
                                                sizeof(float) );

    ogl_program_ub_set_arrayed_uniform_value(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                             _po_scalar_field_polygonizer_precomputed_tables_ub_bo_edge_table_offset,
                                             _edge_table,
                                             0, /* src_data_flags */
                                             sizeof(_edge_table),
                                             0, /* dst_array_start_index */
                                             sizeof(_edge_table) / sizeof(_edge_table[0]) );
    ogl_program_ub_set_arrayed_uniform_value(_po_scalar_field_polygonizer_precomputed_tables_ub,
                                             _po_scalar_field_polygonizer_precomputed_tables_ub_bo_triangle_table_offset,
                                             _triangle_table,
                                             0, /* src_data_flags */
                                             sizeof(_triangle_table),
                                             0, /* dst_array_start_index */
                                             sizeof(_triangle_table) / sizeof(_triangle_table[0]) );

    ogl_program_ub_sync(_po_scalar_field_polygonizer_data_ub);
    ogl_program_ub_sync(_po_scalar_field_polygonizer_precomputed_tables_ub);

    system_matrix4x4_release(mvp);
    mvp = NULL;

    /* Set up the SSBO & UBO bindings */
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        _po_scalar_field_polygonizer_cube_properties_ssb_bp,
                                        _blob_cube_properties_bo_id,
                                        _blob_cube_properties_bo_start_offset,
                                        _blob_cube_properties_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        _po_scalar_field_polygonizer_indirect_draw_call_count_ssb_bp,
                                        _indirect_draw_call_args_bo_id,
                                        _indirect_draw_call_args_bo_count_arg_offset,
                                        sizeof(int) );
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        _po_scalar_field_polygonizer_scalar_field_data_ssb_bp,
                                        _blob_scalar_field_bo_id,
                                        _blob_scalar_field_bo_start_offset,
                                        _blob_scalar_field_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                                        _po_scalar_field_polygonizer_result_data_ssb_bp,
                                        _polygonized_data_bo_id,
                                        _polygonized_data_bo_start_offset,
                                        _polygonized_data_bo_size);

    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        _po_scalar_field_polygonizer_data_ub_bp,
                                        _po_scalar_field_polygonizer_data_ub_bo_id,
                                        _po_scalar_field_polygonizer_data_ub_bo_start_offset,
                                        _po_scalar_field_polygonizer_data_ub_bo_size);
    entrypoints_ptr->pGLBindBufferRange(GL_UNIFORM_BUFFER,
                                        _po_scalar_field_polygonizer_precomputed_tables_ub_bp,
                                        _po_scalar_field_polygonizer_precomputed_tables_ub_bo_id,
                                        _po_scalar_field_polygonizer_precomputed_tables_ub_bo_start_offset,
                                        _po_scalar_field_polygonizer_precomputed_tables_ub_bo_size);

    /* Set up the indirect draw call's <count> SSB. Zero out the value before we run the polygonizer */
    static const int int_zero = 0;

    entrypoints_ptr->pGLBindBuffer        (GL_DRAW_INDIRECT_BUFFER,
                                           _indirect_draw_call_args_bo_id);
    entrypoints_ptr->pGLClearBufferSubData(GL_DRAW_INDIRECT_BUFFER,
                                           GL_R32UI,
                                           _indirect_draw_call_args_bo_count_arg_offset,
                                           sizeof(int),
                                           GL_RED_INTEGER,
                                           GL_UNSIGNED_INT,
                                          &int_zero);

    /* Sync SSBO-based scalar field data, if needed. */
    if (_scalar_field_data_updated)
    {
        entrypoints_ptr->pGLMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        _scalar_field_data_updated = false;
    }

    /* Polygonize the scalar field */
    entrypoints_ptr->pGLUseProgram     (ogl_program_get_id(_po_scalar_field_polygonizer) );
    entrypoints_ptr->pGLDispatchCompute(_po_scalar_field_polygonizer_global_wg_size[0],
                                        _po_scalar_field_polygonizer_global_wg_size[1],
                                        _po_scalar_field_polygonizer_global_wg_size[2]);

    /* Draw stuff */
    entrypoints_ptr->pGLUseProgram(ogl_program_get_id(_po_scalar_field_renderer) );

    entrypoints_ptr->pGLBindBuffer     (GL_DRAW_INDIRECT_BUFFER,
                                        _indirect_draw_call_args_bo_id);
    entrypoints_ptr->pGLBindVertexArray(_vao_id);

    entrypoints_ptr->pGLMemoryBarrier(GL_COMMAND_BARRIER_BIT              |
                                      GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    entrypoints_ptr->pGLDisable           (GL_CULL_FACE);
    entrypoints_ptr->pGLDrawArraysIndirect(GL_TRIANGLES,
                                           (const GLvoid*) _indirect_draw_call_args_bo_start_offset);
}

/** TODO */
PRIVATE void _rendering_handler(ogl_context context,
                                uint32_t    n_frames_rendered,
                                system_time frame_time,
                                const int*  rendering_area_px_topdown,
                                void*       renderer)
{
    const ogl_context_gl_entrypoints* entrypoints_ptr = NULL;
    const ogl_context_gl_limits*      limits_ptr      = NULL;
    static bool                       is_initialized  = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entrypoints_ptr);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    /* If this is the first frame we're rendering, initialize various stuff. */
    if (!is_initialized)
    {
        /* Initialize scene stuff */
        _init_scalar_field(entrypoints_ptr,
                           limits_ptr);

        _init_scalar_field_polygonizer(entrypoints_ptr,
                                       limits_ptr);
        _init_scalar_field_renderer   (entrypoints_ptr);
        _init_scene();

        /* Initialize projection & view matrices */
        _projection_matrix = system_matrix4x4_create_perspective_projection_matrix(45.0f,  /* fov_y */
                                                                                   float(_window_size[0]) / float(_window_size[1]),
                                                                                   0.01f,  /* z_near */
                                                                                   10.0f); /* z_far  */
        _view_matrix       = system_matrix4x4_create                              ();

        system_matrix4x4_set_to_identity(_view_matrix);

        /* Initialize the flyby */
        static const bool is_flyby_active = true;

        ogl_context_get_property(_context,
                                 OGL_CONTEXT_PROPERTY_FLYBY,
                                &_context_flyby);
        ogl_flyby_set_property  (_context_flyby,
                                 OGL_FLYBY_PROPERTY_IS_ACTIVE,
                                &is_flyby_active);

        /* All done */
        is_initialized = true;
    } /* if (!is_initialized) */

    /* Update the flyby */
    ogl_flyby_update(_context_flyby);

    /* Render the scene */
    ogl_flyby_get_property(_context_flyby,
                           OGL_FLYBY_PROPERTY_VIEW_MATRIX,
                          &_view_matrix);

    ogl_scene_renderer_render_scene_graph(_scene_renderer,
                                          _view_matrix,
                                          _projection_matrix,
                                          _scene_camera,
                                          RENDER_MODE_FORWARD_WITHOUT_DEPTH_PREPASS,
                                          false, /* apply_shadow_mapping */
                                          HELPER_VISUALIZATION_BOUNDING_BOXES,
                                          frame_time);
}

/** Event callback handlers */
PRIVATE bool _rendering_rbm_callback_handler(system_window           window,
                                             unsigned short          x,
                                             unsigned short          y,
                                             system_window_vk_status new_status,
                                             void*)
{
    system_event_set(_window_closed_event);

    return true;
}

PRIVATE void _window_closed_callback_handler(system_window window)
{
    system_event_set(_window_closed_event);
}

PRIVATE void _window_closing_callback_handler(system_window window)
{
    if (_blob_mesh != NULL)
    {
        mesh_release(_blob_mesh);

        _blob_mesh = NULL;
    }

    if (_po_scalar_field_generator != NULL)
    {
        ogl_program_release(_po_scalar_field_generator);

        _po_scalar_field_generator = NULL;
    }

    if (_po_scalar_field_polygonizer != NULL)
    {
        ogl_program_release(_po_scalar_field_polygonizer);

        _po_scalar_field_polygonizer = NULL;
    }

    if (_po_scalar_field_renderer != NULL)
    {
        ogl_program_release(_po_scalar_field_renderer);

        _po_scalar_field_renderer = NULL;
    }

    if (_projection_matrix != NULL)
    {
        system_matrix4x4_release(_projection_matrix);

        _projection_matrix = NULL;
    }

    if (_scene != NULL)
    {
        scene_release(_scene);

        _scene = NULL;
    }

    if (_scene_camera != NULL)
    {
        scene_camera_release(_scene_camera);

        _scene_camera = NULL;
    }

    if (_scene_renderer != NULL)
    {
        ogl_scene_renderer_release(_scene_renderer);

        _scene_renderer = NULL;
    }

    if (_view_matrix != NULL)
    {
        system_matrix4x4_release(_view_matrix);

        _view_matrix = NULL;
    }

    /* All done */
    system_event_set(_window_closed_event);
}


/** Entry point */
#ifdef _WIN32
    int WINAPI WinMain(HINSTANCE instance_handle,
                       HINSTANCE,
                       LPTSTR,
                       int)
#else
    int main()
#endif
{
    system_screen_mode    screen_mode              = NULL;
    system_window         window                   = NULL;
    system_pixel_format   window_pf                = NULL;
    ogl_rendering_handler window_rendering_handler = NULL;
    int                   window_x1y1x2y2[4]       = {0};

    system_screen_mode_get         (0,
                                   &screen_mode);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_WIDTH,
                                    _window_size + 0);
    system_screen_mode_get_property(screen_mode,
                                    SYSTEM_SCREEN_MODE_PROPERTY_HEIGHT,
                                    _window_size + 1);

    window_pf = system_pixel_format_create(8,  /* color_buffer_red_bits   */
                                           8,  /* color_buffer_green_bits */
                                           8,  /* color_buffer_blue_bits  */
                                           0,  /* color_buffer_alpha_bits */
                                           16, /* depth_buffer_bits       */
                                           SYSTEM_PIXEL_FORMAT_USE_MAXIMUM_NUMBER_OF_SAMPLES,
                                           0); /* stencil_buffer_bits     */

#if 0
    window = system_window_create_fullscreen(OGL_CONTEXT_TYPE_GL,
                                             screen_mode,
                                             true, /* vsync_enabled */
                                             window_pf);
#else
    _window_size[0] /= 2;
    _window_size[1] /= 2;

    system_window_get_centered_window_position_for_primary_monitor(_window_size,
                                                                   window_x1y1x2y2);

    window = system_window_create_not_fullscreen(OGL_CONTEXT_TYPE_GL,
                                                 window_x1y1x2y2,
                                                 system_hashed_ansi_string_create("Test window"),
                                                 false, /* scalable */
                                                 false, /* vsync_enabled */
                                                 true,  /* visible */
                                                 window_pf);
#endif

    /* Set up the rendering contxt */
    window_rendering_handler = ogl_rendering_handler_create_with_fps_policy(system_hashed_ansi_string_create("Default rendering handler"),
                                                                            60,
                                                                            _rendering_handler,
                                                                            NULL); /* user_arg */

    /* Kick off the rendering process */
    system_window_get_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_CONTEXT,
                              &_context);
    system_window_set_property(window,
                               SYSTEM_WINDOW_PROPERTY_RENDERING_HANDLER,
                              &window_rendering_handler);

    /* Set up mouse click & window tear-down callbacks */
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_RIGHT_BUTTON_DOWN,
                                        (void*) _rendering_rbm_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSED,
                                        (void*) _window_closed_callback_handler,
                                        NULL);
    system_window_add_callback_func    (window,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_PRIORITY_NORMAL,
                                        SYSTEM_WINDOW_CALLBACK_FUNC_WINDOW_CLOSING,
                                        (void*) _window_closing_callback_handler,
                                        NULL);

    /* Launch the rendering process and wait until the window is closed. */
    ogl_rendering_handler_play(window_rendering_handler,
                               0);

    system_event_wait_single(_window_closed_event);

    /* Clean up - DO NOT release any GL objects here, no rendering context is bound
     * to the main thread!
     */
    system_window_close (window);
    system_event_release(_window_closed_event);

    main_force_deinit();

    return 0;
}