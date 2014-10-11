/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "ocl/ocl_context.h"
#include "ocl/ocl_general.h"
#include "ocl/ocl_kdtree.h"
#include "ocl/ocl_kernel.h"
#include "ocl/ocl_program.h"
#include "system/system_bst.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"


/** Internal type definitions */
typedef struct
{
    float x_min;
    float y_min;
    float z_min;
    float x_max;
    float y_max;
    float z_max;
} _ocl_kdtree_bounding_box;


const int AXIS_SPLIT_X = 1;
const int AXIS_SPLIT_Y = 2;
const int AXIS_SPLIT_Z = 3;
const int NODE_A       = 1;
const int NODE_B       = 2;
const int NODE_C       = 3; /* c-nodes are removed during post-processing: they represent a A-node with only one child */

#pragma pack(push)
#pragma pack(1)
    typedef struct _ocl_kdtree_temporary_node _ocl_kdtree_temporary_node;
    struct _ocl_kdtree_temporary_node
    {
        uint32_t                    axis_split; /* uses AXIS_SPLIT_* values */
        _ocl_kdtree_bounding_box    bbox;
        _ocl_kdtree_temporary_node* left;
        _ocl_kdtree_temporary_node* right;
        float                       split_value;
        system_resizable_vector     triangles; /* Refers to indexes to _ocl_kdtree_triangle descriptor indices */
    };
#pragma pack(pop)

typedef struct
{
    ocl_program cl_raycast_program_single;
    ocl_kernel  cl_raycast_program_single_kernel;
    ocl_program cl_raycast_program_multiple;
    ocl_kernel  cl_raycast_program_multiple_kernel;
} _ocl_kdtree_executor;

typedef struct
{
    _ocl_kdtree_bounding_box bbox;
    uint32_t                 unique_vertex_indices[3];
    uint32_t                 unique_normal_indices[3];
} _ocl_kdtree_triangle;


typedef struct
{
    cl_mem                      cl_raycast_helper_bo; /* stores ray direction vectors */
    uint32_t                    cl_raycast_helper_bo_size;
    uint32_t                    clgl_offset_nodes_data;
    uint32_t                    clgl_offset_triangle_ids;
    uint32_t                    clgl_offset_triangle_lists;
    kdtree_creation_flags       creation_flags;
    ocl_context                 context;
    system_resizable_vector     executors;
    void*                       kdtree_data_cl_buffer;
    cl_mem                      kdtree_data_cl_buffer_cl;
    uint32_t                    kdtree_data_cl_buffer_size;
    uint32_t                    max_triangles_per_leaf;
    cl_mem                      meshes_cl_data_buffer; /* CL buffer object created from meshes' GL BO parts storing vertex & normals data.  */
    uint32_t                    meshes_cl_data_buffer_normals_offset;
    uint32_t                    meshes_cl_data_buffer_vertices_offset;
    mesh                        mesh_data;
    float                       min_leaf_volume_multiplier;
    uint32_t                    n_executors;
    uint32_t                    n_split_planes; /* = number of a-nodes */
    system_resource_pool        node_pool;
    _ocl_kdtree_bounding_box    root_bb;
    _ocl_kdtree_temporary_node* root_node;
    _ocl_kdtree_triangle*       tree_triangles;
    _ocl_kdtree_usage           usage;

    /* Only used for serialization - stores normals & vertex data */
    void*  serialize_meshes_data_buffer;
    size_t serialize_meshes_data_buffer_size;

    REFCOUNT_INSERT_VARIABLES
} _ocl_kdtree;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ocl_kdtree, ocl_kdtree, _ocl_kdtree);

/* Private function prototypes */
PRIVATE void                        _ocl_kdtree_convert_to_cl_gl_representation(__in                         __notnull _ocl_kdtree*                data_ptr);
PRIVATE void                        _ocl_kdtree_deinit                         (__in                         __notnull _ocl_kdtree*                data_ptr);
PRIVATE void                        _ocl_kdtree_deinit_executor                (__in                         __notnull _ocl_kdtree_executor*);
PRIVATE void                        _ocl_kdtree_deinit_temporary_node          (__in                         __notnull _ocl_kdtree_temporary_node*);
PRIVATE void                        _ocl_kdtree_fill                           (__in                         __notnull _ocl_kdtree*                data_ptr);
PRIVATE bool                        _ocl_kdtree_float3_equal                   (__in_bcount(3*sizeof(float)) __notnull void*                       data1,      __in_bcount(3*sizeof(float)) __notnull void*                 data2);
PRIVATE bool                        _ocl_kdtree_float3_lower                   (__in_bcount(3*sizeof(float)) __notnull void*                       data1,      __in_bcount(3*sizeof(float)) __notnull void*                 data2);
PRIVATE void                        _ocl_kdtree_generate_kdtree_data_cl_buffer (__in                         __notnull _ocl_kdtree*                data_ptr);
PRIVATE void                        _ocl_kdtree_generate_cl_vertex_data_buffer (__in                         __notnull _ocl_kdtree*                data_ptr);
PRIVATE _ocl_kdtree_temporary_node* _ocl_kdtree_get_a_b_subnode                (__in                         __notnull _ocl_kdtree_temporary_node* curr_ptr);
PRIVATE void                        _ocl_kdtree_init                           (__in                         __notnull _ocl_kdtree*                data_ptr,   __in                         __notnull ocl_context           context,  __in kdtree_creation_flags, __in __maybenull mesh mesh_instance, __in float min_leaf_volume_multiplier, __in uint32_t max_triangles_per_leaf, __in _ocl_kdtree_usage usage);
PRIVATE void                        _ocl_kdtree_init_temporary_node            (__in                         __notnull _ocl_kdtree_temporary_node*);
PRIVATE void                        _ocl_kdtree_parse_a_node                   (__in                         __notnull char*                       traveller_ptr, __out __notnull uint32_t* out_node_type, __out __notnull uint32_t* out_node_axis, __out __notnull float* out_split_value, __out __notnull uint32_t* out_left_node_offset, __out __notnull uint32_t* out_right_node_offset);
PRIVATE void                        _ocl_kdtree_release                        (__in __notnull void*    arg);
PRIVATE void                        _ocl_kdtree_split_triangle                 (__in           uint32_t axis_split, __in __notnull _ocl_kdtree_triangle* triangle, __in  __notnull _ocl_kdtree_bounding_box* left_bb, __in  __notnull _ocl_kdtree_bounding_box* right_bb, __out __notnull bool* out_goes_left, __out __notnull bool* out_goes_right);
PRIVATE bool                        _ocl_kdtree_triple_uint32_lower            (__in __notnull void*    arg1,       __in __notnull void*                 arg2);
PRIVATE bool                        _ocl_kdtree_triple_uint32_equal            (__in __notnull void*    arg1,       __in __notnull void*                 arg2);

/** TODO. aabb_ray_intersection taken from http://gamedev.stackexchange.com/questions/18436/most-efficient-aabb-vs-ray-collision-algorithms (push-down traversal) */
const char* cl_kdtree_cast_kernel_body = "#define MAX_STACK_SIZE (1)\n"
                                         "\n"
                                         "__constant float epsilon = 1e-5;\n"
                                         "\n"
                                         "bool aabb_ray_intersection(          const float3 bb_min,\n"
                                         "                                     const float3 bb_max,\n"
                                         "                                     const float3 ray_origin,\n"
                                         "                                     const float3 ray_direction,\n"
                                         "                           __private       float* result_min,\n"
                                         "                           __private       float* result_max)\n"
                                         "{\n"
                                         "   bool is_x_ok = fabs(ray_direction.x) >= epsilon;\n"
                                         "   bool is_y_ok = fabs(ray_direction.y) >= epsilon;\n"
                                         "   bool is_z_ok = fabs(ray_direction.z) >= epsilon;\n"
                                         "\n"
                                         "   if (!is_x_ok)\n"
                                         "   {\n"
                                         "       if (ray_origin.x < bb_min.x || ray_origin.x > bb_max.x) return false;\n"
                                         "   }\n"
                                         "   if (!is_y_ok)\n"
                                         "   {\n"
                                         "       if (ray_origin.y < bb_min.y || ray_origin.y > bb_max.y) return false;\n"
                                         "   }\n"
                                         "   if (!is_z_ok)\n"
                                         "   {\n"
                                         "       if (ray_origin.z < bb_min.z || ray_origin.z > bb_max.z) return false;\n"
                                         "   }\n"
                                         "\n"
                                         "   float3 direction_inversed = 1.0 / ray_direction;\n"
                                         "   float  t1                 = is_x_ok ? (bb_min.x - ray_origin.x) * direction_inversed.x : ray_origin.x;\n"
                                         "   float  t2                 = is_x_ok ? (bb_max.x - ray_origin.x) * direction_inversed.x : ray_origin.x;\n"
                                         "   float  t3                 = is_y_ok ? (bb_min.y - ray_origin.y) * direction_inversed.y : ray_origin.y;\n"
                                         "   float  t4                 = is_y_ok ? (bb_max.y - ray_origin.y) * direction_inversed.y : ray_origin.y;\n"
                                         "   float  t5                 = is_z_ok ? (bb_min.z - ray_origin.z) * direction_inversed.z : ray_origin.z;\n"
                                         "   float  t6                 = is_z_ok ? (bb_max.z - ray_origin.z) * direction_inversed.z : ray_origin.z;\n"
                                         "   float  t_min              = max(max(min(t1, t2), min(t3, t4)), min(t5, t6));\n"
                                         "   float  t_max              = min(min(max(t1, t2), max(t3, t4)), max(t5, t6));\n"
                                         "\n"
                                         "   /* No intersection - AABB is behind */\n"
                                         "   if (t_max < 0)    { return false; }\n"
                                         "\n"
                                         "   /* No intersection at all */\n"
                                         "   if (t_min > t_max){ return false; }\n"
                                         "\n"
                                         "   /* Intersection! */\n"
                                         "   *result_min = t_min;\n"
                                         "   *result_max = t_max;\n"
                                         "\n"
                                         "   return true;\n"
                                         "}\n"
                                         "\n"
                                         /* Taken from http://www.blackpawn.com/texts/pointinpoly/default.html */
                                         "bool triangle_point_intersection(const float3 ray_origin,\n"
                                         "                                 const float3 vertexA,\n"
                                         "                                 const float3 vertexB,\n"
                                         "                                 const float3 vertexC)\n"
                                         "{\n"
                                         "   float3 vertex0 = vertexC    - vertexA;\n"
                                         "   float3 vertex1 = vertexB    - vertexA;\n"
                                         "   float3 vertex2 = ray_origin - vertexA;\n"
                                         "\n"
                                         "   float dot00 = dot(vertex0, vertex0);\n"
                                         "   float dot01 = dot(vertex0, vertex1);\n"
                                         "   float dot02 = dot(vertex0, vertex2);\n"
                                         "   float dot11 = dot(vertex1, vertex1);\n"
                                         "   float dot12 = dot(vertex1, vertex2);\n"
                                         "\n"
                                         "   float inv_denom = 1.0 / (dot00 * dot11 - dot01 * dot01);\n"
                                         "   float u         = (dot11 * dot02 - dot01 * dot12) * inv_denom;\n"
                                         "   float v         = (dot00 * dot12 - dot01 * dot02) * inv_denom;\n"
                                         "\n"
                                         "   return (u >= 0) && (v >= 0) && (u + v < 1);\n"
                                         "}\n"
                                         "\n"
                                         "bool triangle_ray_intersection(          const float3 ray_origin,\n"
                                         "                                         const float3 ray_direction,\n"
                                         "                                         const float3 vertex0,\n"
                                         "                                         const float3 vertex1,\n"
                                         "                                         const float3 vertex2,\n"
                                         "                               __private       float* out_result,\n"
                                         "                               __private       float* out_u,\n"
                                         "                               __private       float* out_v)\n"
                                         "{\n"
                                         "    float3 edge1, edge2, q, s, r;\n"
                                         "    float  a, f, u, v;\n"
                                         "\n"
                                         "    edge1 = vertex1 - vertex0;\n"
                                         "    edge2 = vertex2 - vertex0;\n"
                                         "\n"
                                         "    q = cross(ray_direction, edge2);\n"
                                         "    a = dot  (edge1,         q);\n"
                                         "\n"
                                         "    if (a < epsilon && a > -epsilon){ return false; }\n"
                                         "\n"
                                         "    f = 1.0 / a;\n"   
                                         "    s = ray_origin - vertex0;\n"
                                         "    u = f * dot(s, q);\n"
                                         "\n"
                                         "    if (u < 0 || u > 1.0) { return false; }\n"
                                         "\n"
                                         "    r = cross(s, edge1);\n"
                                         "    v = f * dot(ray_direction, r);\n"
                                         "\n"
                                         "    if (v < 0 || u+v > 1){ return false; }\n"
                                         "\n"
                                         "    *out_result = f * dot(edge2, r);\n"
                                         "    *out_u      = u;\n"
                                         "    *out_v      = v;\n"
                                         "\n"
                                         "    return (*out_result > epsilon);\n"
                                         "}\n"
                                         "\n"
                                         /* DO NOT change order of the arguments! If you have to, please update ocl_kdtree_intersect_rays(). Based on http://graphics.stanford.edu/papers/i3dkdtree/gpu-kd-i3d.pdf */
                                         "__kernel void kdtree_cast_ray(const    float3               scene_bb_min,\n"
                                         "                              const    float3               scene_bb_max,\n"
                                         "#ifdef USE_SINGLE_RAY_ORIGIN\n"
                                         "                              const    float3               ray_origin,\n"
                                         "#else\n"
                                         "                              __global const float*         ray_origins,\n"
                                         "#endif\n"
                                         "                              __global const float*         ray_directions,\n"
                                         "                              __global const unsigned char* kdtree_data,\n"
                                         "                              __global unsigned char*       result,\n"
                                         "                              const    unsigned int         n_rays,\n"
                                         "                              const    unsigned int         global_triangle_list_offset,\n"
                                         "                              const    unsigned int         global_triangle_data_offset,\n"
                                         "                              __global const unsigned char* unique_vertex_data,\n"
                                         "                              const    unsigned int         n_ray_origin,\n"
                                         "                              __global unsigned int*        result_triangle_hit_data,\n"
                                         "                              const    unsigned int         should_store_triangle_hit_data,\n"
                                         "                              const    unsigned int         should_find_closest_intersection,\n"
                                         "                              const    unsigned int         ray_directions_offset,\n"
                                         "                              const    unsigned int         unique_vertex_data_normals_offset\n"
                                         "#ifndef USE_SINGLE_RAY_ORIGIN\n"
                                         "                             ,const    unsigned int         ray_origins_offset\n"
                                         "#endif\n"
                                         ")\n"
                                         "{\n"
                                         "    unsigned int ray_index = get_local_size(0) * get_group_id(0) + get_local_id(0);\n"
                                         "\n"
                                         "    if (ray_index >= n_rays)\n"
                                         "    {\n"
                                         "        return;\n"
                                         "    }\n"
                                         "\n"
                                         /* NOTE: There is an implication here: you can either cast multiple rays from a single location *OR*
                                          *       cast a single ray from many locations.
                                          *
                                          *       C implementation has been adapted to correctly handle all possible cases.
                                          **/
                                         "#ifndef USE_SINGLE_RAY_ORIGIN\n"
                                         "    float3       ray_origin       = vload4(ray_index, ray_origins + ray_origins_offset).xyz;\n"
                                         "#endif\n"
                                         "    float3       ray_direction    = GET_RAY_DIRECTION;\n"
                                         "    unsigned int node_offset      = 0;\n"
                                         "    unsigned int root_node_offset = 0;\n"
                                         "    float        t_min;\n"
                                         "    float        t_max;\n"
                                         "    float        t_hit     = MAXFLOAT;\n"
                                         "    float        scene_min = 0.0;\n"
                                         "    float        scene_max = 1.0;\n"
                                         "    bool         push_down;\n"
                                         "\n"
                                         "    typedef struct\n"
                                         "    {\n"
                                         "        float offset;\n"
                                         "        float t_min;\n"
                                         "        float t_max;\n"
                                         "    } _stack_entry;\n"
                                         "\n"
                                         "    _stack_entry stack[MAX_STACK_SIZE];\n"
                                         "    unsigned int n_stack_entries = 0;\n"
                                         "\n"
                                         "    RESET_RESULT\n"
                                         "\n"
                                         "    if (should_store_triangle_hit_data)\n"
                                         "    {\n"
                                         "        result_triangle_hit_data[3 * n_ray_origin * n_rays + ray_index * 3    ] = 0;\n"
                                         "        result_triangle_hit_data[3 * n_ray_origin * n_rays + ray_index * 3 + 1] = 0;\n"
                                         "        result_triangle_hit_data[3 * n_ray_origin * n_rays + ray_index * 3 + 2] = 0;\n"
                                         "    }\n"
                                         "\n"
                                         // AABB/ray intersection test is broken. This can be easily tested by uncommenting the following lines,
                                         // re-exporting kula.lwo and running the PoC.
                                         //
                                         //"    if (!aabb_ray_intersection(scene_bb_min.xyz, scene_bb_max.xyz, ray_origin, ray_direction, &scene_min, &scene_max) )\n"
                                         //"    {\n"
                                         //"        return;\n"
                                         //"    }\n"
                                         "\n"
                                         "    t_max = scene_min;\n"
                                         "\n"
                                         "    while (t_max < scene_max)\n"
                                         "    {\n"
                                         "        if (n_stack_entries == 0)\n"
                                         "        {\n"
                                         "           node_offset = root_node_offset;\n"
                                         "           t_min       = t_max;\n"
                                         "           t_max       = scene_max;\n"
                                         "           push_down   = true;\n"
                                         "        }\n"
                                         "        else\n"
                                         "        {\n"
                                         "            n_stack_entries--;\n"
                                         "\n"
                                         "            node_offset = stack[n_stack_entries].offset;\n"
                                         "            t_min       = stack[n_stack_entries].t_min;\n"
                                         "            t_max       = stack[n_stack_entries].t_max;\n"
                                         "            push_down   = false;\n"
                                         "        }\n"
                                         "\n"
                                         "        while(true)\n"
                                         "        {\n"
                                         "            unsigned int node_type = *(__global unsigned int*)(kdtree_data + node_offset);\n"
                                         "\n"
                                         "            if (node_type == 2){ break;} /* it's a leaf */\n"
                                         "\n"
                                         "            unsigned int this_split_axis      = *(__global unsigned int*)          (kdtree_data + (node_offset + 4));\n"
                                         "            float        this_split_value     = *(__global float*)                 (kdtree_data + (node_offset + 8));\n"
                                         "            uint2        kid_offsets          = vload2(0, (__global unsigned int*) (kdtree_data + (node_offset + 12)) );\n"
                                         "            float        ray_this_origin_axis;\n"
                                         "            float        ray_this_dir_axis;\n"
                                         "\n"
                                         "            if (this_split_axis == 1)\n"
                                         "            {\n"
                                         "                ray_this_origin_axis = ray_origin.x;\n"
                                         "                ray_this_dir_axis    = ray_direction.x;\n"
                                         "            }\n"
                                         "            else\n"
                                         "            if (this_split_axis == 2)\n"
                                         "            {\n"
                                         "                ray_this_origin_axis = ray_origin.y;\n"
                                         "                ray_this_dir_axis    = ray_direction.y;\n"
                                         "            }\n"
                                         "            else\n"
                                         "            {\n"
                                         "                ray_this_origin_axis = ray_origin.z;\n"
                                         "                ray_this_dir_axis    = ray_direction.z;\n"
                                         "            }\n"
                                         "\n"
                                         "            float t_this_split      = (this_split_value - ray_this_origin_axis) / ray_this_dir_axis;\n"
                                         "            uint2 sorted_kid_offsets;\n"
                                         "\n"
                                         "            if (ray_this_origin_axis < this_split_value) { sorted_kid_offsets = kid_offsets.xy;}else\n"
                                         "                                                         { sorted_kid_offsets = kid_offsets.yx;}\n"
                                         "\n"         
                                         "            if (t_this_split >= t_max || t_this_split < 0)\n"
                                         "            {\n"
                                         "                node_offset = sorted_kid_offsets.x;\n"
                                         "            }\n"
                                         "            else\n"
                                         "            if (t_this_split <= t_min)\n"
                                         "            {\n"
                                         "                node_offset = sorted_kid_offsets.y;\n"
                                         "            }\n"
                                         "            else\n"
                                         "            {\n"
                                         "                if (n_stack_entries == MAX_STACK_SIZE)\n"
                                         "                {\n"
                                         "                    /* Wrap around */\n"
                                         "                    for (int n = 0; n < MAX_STACK_SIZE - 1; ++n) stack[n] = stack[n+1];\n"
                                         "\n"
                                         "                    n_stack_entries--;\n"
                                         "                }\n"
                                         "\n"
                                         "                stack[n_stack_entries].offset = sorted_kid_offsets.y;\n"
                                         "                stack[n_stack_entries].t_min  = t_this_split;\n"
                                         "                stack[n_stack_entries].t_max  = t_max;\n"
                                         "\n"
                                         "                n_stack_entries++;\n"
                                         "\n"
                                         "                node_offset = sorted_kid_offsets.x;\n"
                                         "                t_max       = t_this_split;\n"
                                         "                push_down   = false;\n"
                                         "            }\n"
                                         "            if (push_down)\n"
                                         "            {\n"
                                         "               root_node_offset = node_offset;\n"
                                         "            }\n"
                                         "            \n"
                                         "\n"
                                         "        }\n"
                                         "\n"
                                         "        /* We're at a leaf node. Extract vertex data */\n"
                                         "        uint2        node_b_data          = vload2(0, (__global unsigned int*)(kdtree_data + node_offset + 4) );\n"
                                         "        unsigned int triangle_list_offset = node_b_data.x;\n"
                                         "        unsigned int n_triangles          = node_b_data.y;\n"
                                         "        bool         can_leave            = false;        \n"
                                         "\n"
                                         "        for (unsigned int n_triangle = 0; n_triangle < n_triangles; ++n_triangle)\n"
                                         "        {\n"
                                         "            __private unsigned int triangle_id = *((__global unsigned int*) (kdtree_data + global_triangle_list_offset + triangle_list_offset + n_triangle * 4));\n"
                                         "            __private uint3        element_ids = vload3(0, (__global uint*) (kdtree_data + global_triangle_data_offset + triangle_id * 12) );\n"
                                         "\n"
                                         "            __global float* normal_a = ((__global float*) (unique_vertex_data + unique_vertex_data_normals_offset + element_ids.x * 12));\n"
                                         "            __global float* vertex_a = ((__global float*) (unique_vertex_data + element_ids.x * 12));\n"
                                         "            __global float* normal_b = ((__global float*) (unique_vertex_data + unique_vertex_data_normals_offset + element_ids.y * 12));\n"
                                         "            __global float* vertex_b = ((__global float*) (unique_vertex_data + element_ids.y * 12));\n"
                                         "            __global float* normal_c = ((__global float*) (unique_vertex_data + unique_vertex_data_normals_offset + element_ids.z * 12));\n"
                                         "            __global float* vertex_c = ((__global float*) (unique_vertex_data + element_ids.z * 12));\n"
                                         "\n"
                                         "            float3 vertex_a3 = vload3(0, vertex_a);\n"
                                         "            float3 vertex_b3 = vload3(0, vertex_b);\n"
                                         "            float3 vertex_c3 = vload3(0, vertex_c);\n"
                                         "            float  t_new     = t_hit;"
                                         "            float  u, v;\n"
                                         "\n"
                                         "            if (!(vertex_a3.x == ray_origin.x && vertex_a3.y == ray_origin.y && vertex_a3.z == ray_origin.z ||\n"
                                         "                  vertex_b3.x == ray_origin.x && vertex_b3.y == ray_origin.y && vertex_b3.z == ray_origin.z ||\n"
                                         "                  vertex_c3.x == ray_origin.x && vertex_c3.y == ray_origin.y && vertex_c3.z == ray_origin.z) )\n"
                                         "            {\n"
                                         "                if (triangle_ray_intersection(ray_origin, ray_direction, vertex_a3, vertex_b3, vertex_c3, &t_new, &u, &v) )\n"
                                         "                {\n"
                                         "                    float t_new_hit = min(t_hit, t_new);\n"
                                         "\n"
                                         "                    if (t_new_hit <= t_hit && t_new_hit < t_max)\n"
                                         "                    {\n"
                                         "                        t_hit = t_new_hit;\n"
                                         "\n"
                                         "                        UPDATE_RESULT\n"
                                         "\n"
                                         "                        if (should_store_triangle_hit_data != 0)\n"
                                         "                        {\n"
                                         "                            result_triangle_hit_data[3 * (n_ray_origin * n_rays + ray_index)    ] = element_ids.x;\n"
                                         "                            result_triangle_hit_data[3 * (n_ray_origin * n_rays + ray_index) + 1] = element_ids.y;\n"
                                         "                            result_triangle_hit_data[3 * (n_ray_origin * n_rays + ray_index) + 2] = element_ids.z;\n"
                                         "                        }\n"
                                         "\n"
                                         "                        can_leave = true;\n"
                                         "\n"
                                         "                        if (!should_find_closest_intersection)\n"
                                         "                        {\n"
                                         "                            break;\n"
                                         "                        }\n"
                                         "                    }\n"
                                         "               }\n"
                                         "            }\n"
                                         "        }\n"
                                         "\n"
                                         "        if (can_leave) break;\n"
                                         "    }\n"
                                         "}\n";

/** TODO */
PRIVATE void _ocl_kdtree_convert_to_cl_gl_representation(__in __notnull _ocl_kdtree* data_ptr)
{
    ASSERT_DEBUG_SYNC(data_ptr->root_node != NULL, "Root node is unavailable - cannot create CL/GL representation of the Kd tree!");
    if (data_ptr->root_node == NULL)
    {
        return;
    }

    /* Tree representation is no fun for GPU. The following buffer/memory object structure is therefore used instead:
     *
     * ##########                                          AB) node type               (uint32_t)       4b
     * ##NODES###   where each node is represented as:      A) split axis type         (uint32_t)       8b
     * ##########                                           A) split plane value       (float32_t)      12b
     *    ..                                                A) left split node offset  (uint32_t)       16b
     *    ..                                                A) right split node offset (uint32_t)       20b
     *    ..                                                B) triangle list offset    (uint32_t)       8b
     *    ..                                                B) number of triangles     (uint32_t)       12b
     *    ..
     * ########## 
     * #TRIANGLE#   triangle list consists of a raw list of triangle ids, as pointed to by B-nodes.
     * ##LISTS###   
     * ##########
     *    ..
     * ##########
     * #TRIANGLE#   3 vertex ids for any triangle id can be found by reading 3 uint32_t ids, starting from
     * ###DATA###   3*sizeof(uint32_t)*triangle_id byte. Normals and vertices data is available at a shared index.
     * ##########
     *
     * Furthermore, start offsets for the three different regions can be found by calling corresponding ocl_kdtree_*
     * functions.
     */
    
    /* Assign ids to nodes. */
    uint32_t                    current_id               = 0;
    _ocl_kdtree_temporary_node* current_node             = NULL;
    system_resizable_vector     a_nodes_vector           = system_resizable_vector_create(4, sizeof(_ocl_kdtree_temporary_node*) );
    system_resizable_vector     leaf_nodes_vector        = system_resizable_vector_create(4, sizeof(_ocl_kdtree_temporary_node*) );
    uint32_t                    n_a_nodes                = 0;
    uint32_t                    n_b_nodes                = 0;
    const int                   node_a_size              = sizeof(uint32_t) + sizeof(uint32_t)  + sizeof(float) + sizeof(uint32_t) + sizeof(uint32_t);
    const int                   node_b_size              = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint32_t);
    system_hash64map            node_ptr_to_uint32_id    = system_hash64map_create       (sizeof(uint32_t) );
    system_resizable_vector     nodes_to_traverse_vector = system_resizable_vector_create(4, sizeof(_ocl_kdtree_temporary_node*) );
    
    system_resizable_vector_push(nodes_to_traverse_vector, data_ptr->root_node);

    while (system_resizable_vector_pop(nodes_to_traverse_vector, &current_node) )
    {
        system_hash64map_insert(node_ptr_to_uint32_id, system_hash64(current_id), current_node, NULL, NULL);
        current_id++;

        /* Leaf nodes will also be assigned ids, but we'll do that in a loop to come. Thing is leaf nodes can point to multiple triangles,
         * and we need to process kd-tree data in order to prepare a buffer for later CL/GL usage */
        if (current_node->left == NULL && current_node->right == NULL)
        {
            /* It's a leaf node, obviously */
            system_resizable_vector_push(leaf_nodes_vector, current_node);

            n_b_nodes++;
        }
        else
        {
            system_resizable_vector_push(a_nodes_vector, current_node);

            if (current_node->left != NULL)
            {
                system_resizable_vector_push(nodes_to_traverse_vector, current_node->left);
            }

            if (current_node->right != NULL)
            {
                system_resizable_vector_push(nodes_to_traverse_vector, current_node->right);
            }

            n_a_nodes++;
        }
    }

    data_ptr->n_split_planes = n_a_nodes;

    /* Leaf node processing: first, map existing "triangles" vectors to small buffers we will use for building up the final data buffer later on.
     * The small buffers will be used to hold raw triangle id data */
    uint32_t n_leaf_nodes         = system_resizable_vector_get_amount_of_elements(leaf_nodes_vector);
    uint32_t n_triangles_in_lists = 0;

    for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node)
    {
        _ocl_kdtree_temporary_node* leaf_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_leaf_node, &leaf_node_ptr) )
        {
            uint32_t n_triangles_in_leaf_node = 0; 

            n_triangles_in_leaf_node  = system_resizable_vector_get_amount_of_elements(leaf_node_ptr->triangles);
            n_triangles_in_lists     += n_triangles_in_leaf_node;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve leaf node descriptor");
        }
    } /* for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node)*/

    /* Let's focus on mapping vertex index triples to triangles - we will need a triple<=>id mapping */
    uint32_t   triangle_id_counter                 = 0;
    system_bst vertex_index_triple_to_triangle_bst = NULL;

    for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node)
    {
        _ocl_kdtree_temporary_node* leaf_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_leaf_node, &leaf_node_ptr) )
        {
            uint32_t n_node_triangles = system_resizable_vector_get_amount_of_elements(leaf_node_ptr->triangles);

            for (uint32_t n_triangle = 0; n_triangle < n_node_triangles; ++n_triangle)
            {
                _ocl_kdtree_triangle* node_triangle = NULL;

                if (system_resizable_vector_get_element_at(leaf_node_ptr->triangles, n_triangle, &node_triangle) )
                {
                    /* Vertices */
                    if (vertex_index_triple_to_triangle_bst == NULL)
                    {                
                        vertex_index_triple_to_triangle_bst = system_bst_create(sizeof(uint32_t) * 3,  /* key size */
                                                                                sizeof(uint32_t),      /* value size */
                                                                                _ocl_kdtree_triple_uint32_lower,
                                                                                _ocl_kdtree_triple_uint32_equal,
                                                                                (system_bst_key)   node_triangle->unique_vertex_indices,
                                                                                (system_bst_value) &triangle_id_counter);
                        triangle_id_counter++;
                    } /* if (vertex_index_triple_to_triangle_bst == NULL)*/
                    else
                    {
                        uint32_t temp;

                        if (!system_bst_get(vertex_index_triple_to_triangle_bst, (system_bst_key) node_triangle->unique_vertex_indices, (system_bst_value*) &temp) )
                        {
                            system_bst_insert(vertex_index_triple_to_triangle_bst,
                                              (system_bst_key)   node_triangle->unique_vertex_indices,
                                              (system_bst_value) &triangle_id_counter);

                            triangle_id_counter++;
                        }
                    }
                } /* if (system_resizable_vector_get_element_at(leaf_node_ptr->triangles, n_triangle, &node_triangle) )*/
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve triangle descriptor");
                }
            } /* for (uint32_t n_triangle = 0; n_triangle < n_node_triangles; ++n_triangle)*/
        } /* if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_leaf_node, &leaf_node_ptr) )*/
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve leaf node descriptor");
        }
    } /* for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node) */
    
    if (vertex_index_triple_to_triangle_bst == NULL)   
    {
        ASSERT_DEBUG_SYNC(false, "Vertex index triple<=>triangle BST should have been initialized by this point");

        goto end;
    }

    /* Now that we know how many triangle ids we have, we can proceed and fill "triangle data" buffer part. */
    const uint32_t triangle_data_buffer_size = triangle_id_counter * sizeof(uint32_t) * 3;
    uint32_t*      triangle_data_buffer      = new (std::nothrow) uint32_t[triangle_data_buffer_size / sizeof(uint32_t)];

    ASSERT_ALWAYS_SYNC(triangle_data_buffer != NULL, "Out of memory");
    if (triangle_data_buffer == NULL)
    {
        goto end;
    }

    for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node)
    {
        _ocl_kdtree_temporary_node* leaf_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_leaf_node, &leaf_node_ptr) )
        {
            uint32_t n_node_triangles = system_resizable_vector_get_amount_of_elements(leaf_node_ptr->triangles);

            for (uint32_t n_triangle = 0; n_triangle < n_node_triangles; ++n_triangle)
            {
                _ocl_kdtree_triangle* node_triangle = NULL;

                if (system_resizable_vector_get_element_at(leaf_node_ptr->triangles, n_triangle, &node_triangle) )
                {
                    /* Retrieve triangle id for the triangle */
                    uint32_t triangle_id = 0;

                    if (!system_bst_get(vertex_index_triple_to_triangle_bst,
                                        (system_bst_key)     node_triangle->unique_vertex_indices,
                                        (system_bst_value*) &triangle_id) )
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve triangle id");

                        goto end;
                    }

                    /* Copy the ids to relevant place in the buffer */
                    memcpy(triangle_data_buffer + triangle_id * 3, node_triangle->unique_vertex_indices, sizeof(uint32_t) * 3);
                }
            } /* for (uint32_t n_triangle = 0; n_triangle < n_node_triangles; ++n_triangle) */
        } /* if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_leaf_node, &leaf_node_ptr) )*/
    } /* for (uint32_t n_leaf_node = 0; n_leaf_node < n_leaf_nodes; ++n_leaf_node) */

    /* Same for 'triangle list', but this time we can actually construct it! */
    system_hash64map node_ptr_to_triangle_list_offset_map = system_hash64map_create(sizeof(uint32_t) );
    const uint32_t   triangle_list_buffer_size            = n_triangles_in_lists * sizeof(uint32_t);
    uint32_t*        triangle_list_buffer                 = new (std::nothrow) uint32_t[triangle_list_buffer_size / sizeof(uint32_t)];
    uint32_t*        triangle_list_traveller              = triangle_list_buffer;

    ASSERT_ALWAYS_SYNC(triangle_list_buffer != NULL, "Out of memory");
    if (triangle_data_buffer == NULL)
    {
        goto end;
    }

    ASSERT_DEBUG_SYNC(triangle_list_buffer_size != 0, "Triangle list buffer size is zero.");

    for (uint32_t n_b_node = 0; n_b_node < n_b_nodes; ++n_b_node)
    {
        /* First, retrieve B node */
        _ocl_kdtree_temporary_node* current_b_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_b_node, &current_b_node_ptr) )
        {
            /* Map node pointer to triangle list offset */
            uint32_t data_offset = (char*)triangle_list_traveller - (char*)triangle_list_buffer;
            
            system_hash64map_insert(node_ptr_to_triangle_list_offset_map,
                                    system_hash64(current_b_node_ptr),
                                    (void*) data_offset,
                                    NULL,
                                    NULL);
                                    
            /* Iterate through all triangles on the list */
            uint32_t n_triangles_in_list = system_resizable_vector_get_amount_of_elements(current_b_node_ptr->triangles);

            for (uint32_t n_triangle_in_list = 0; n_triangle_in_list < n_triangles_in_list; ++n_triangle_in_list)
            {
                _ocl_kdtree_triangle* triangle_ptr = NULL;

                if (system_resizable_vector_get_element_at(current_b_node_ptr->triangles, n_triangle_in_list, &triangle_ptr) )
                {
                    /* Get triangle id for the vertex triple */
                    uint32_t triangle_id = 0;

                    if (system_bst_get(vertex_index_triple_to_triangle_bst, (system_bst_key) triangle_ptr->unique_vertex_indices, (system_bst_value*) &triangle_id) )
                    { 
                        *triangle_list_traveller = triangle_id;
                    } /* if (system_bst_get(vertex_index_triple_to_triangle_bst, triangle_ptr->unique_vertex_indices, &triangle_id) )*/
                    else
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve triangle id!");
                    }                    

                    triangle_list_traveller++;

                    ASSERT_ALWAYS_SYNC((uint32_t) (triangle_list_traveller - triangle_list_buffer) <= triangle_list_buffer_size / sizeof(uint32_t), "Triangle list buffer underflow/overflow detected!");
                } /* if (system_resizable_vector_get_element_at(current_b_node_ptr->triangles, n_triangle_in_list, &triangle_ptr) ) */
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve %dth triangle from B-node triangle list!", n_triangle_in_list);
                }
            } /* for (uint32_t n_triangle_in_list = 0; n_triangle_in_list < n_triangles_in_list; ++n_triangle_in_list) */
        } /* if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_b_node, &current_b_node_ptr) ) */
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve B-node descriptor");
        }
    } /* for (uint32_t n_b_node = 0; n_b_node < n_b_nodes; ++n_b_node)*/

    ASSERT_ALWAYS_SYNC((triangle_list_traveller - triangle_list_buffer) == triangle_list_buffer_size / sizeof(uint32_t), "Triangle list buffer underflow/overflow detected!");

    /* Start building the 'nodes part'. Offsets to children nodes will be filled at a later stage, but we are cool to create the basic
     * structure at this point */
    system_hash64map node_ptr_to_nodes_offset_map = system_hash64map_create(sizeof(uint32_t) );
    const uint32_t   nodes_buffer_size            = node_a_size * n_a_nodes + node_b_size * n_b_nodes;
    char*            nodes_buffer                 = new (std::nothrow) char[nodes_buffer_size];
    char*            nodes_buffer_traveller       = nodes_buffer;

    ASSERT_ALWAYS_SYNC(nodes_buffer != NULL, "Out of memory");
    if (nodes_buffer == NULL)
    {
        goto end;
    }

    for (uint32_t n_a_node = 0; n_a_node < n_a_nodes; ++n_a_node)
    {
        _ocl_kdtree_temporary_node* current_a_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(a_nodes_vector, n_a_node, &current_a_node_ptr) )
        {
            /* Create a node ptr<=>offset mapping which we will later use to fill left/right children offsets */
            uint32_t offset = (nodes_buffer_traveller - nodes_buffer);

            system_hash64map_insert(node_ptr_to_nodes_offset_map, system_hash64(current_a_node_ptr), (void*) offset, NULL, NULL);

            /* Fill node descriptor */
            if (current_a_node_ptr->left == NULL && current_a_node_ptr->right != NULL ||
                current_a_node_ptr->left != NULL && current_a_node_ptr->right == NULL)
            {
                *((uint32_t*) nodes_buffer_traveller) = NODE_C;
            }
            else
            {
                *((uint32_t*) nodes_buffer_traveller) = NODE_A;
            }
            nodes_buffer_traveller += sizeof(uint32_t);

            *((uint32_t*) nodes_buffer_traveller) = current_a_node_ptr->axis_split; nodes_buffer_traveller += sizeof(uint32_t);
            *((float*)    nodes_buffer_traveller) = current_a_node_ptr->split_value;nodes_buffer_traveller += sizeof(float);
            *((uint32_t*) nodes_buffer_traveller) = 0;                              nodes_buffer_traveller += sizeof(uint32_t); /* Left offset  */
            *((uint32_t*) nodes_buffer_traveller) = 0;                              nodes_buffer_traveller += sizeof(uint32_t); /* Right offset */
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve A node descriptor");
        }
    }

    for (uint32_t n_b_node = 0; n_b_node < n_b_nodes; ++n_b_node)
    {
        _ocl_kdtree_temporary_node* current_b_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(leaf_nodes_vector, n_b_node, &current_b_node_ptr) )
        {
            uint32_t n_triangles_on_triangle_list = system_resizable_vector_get_amount_of_elements(current_b_node_ptr->triangles);

            /* Create a node ptr<=>offset mapping.. again but this time for b-nodes */
            uint32_t offset = (nodes_buffer_traveller - nodes_buffer);

            system_hash64map_insert(node_ptr_to_nodes_offset_map, system_hash64(current_b_node_ptr), (void*) offset, NULL, NULL);

            /* Retrieve triangle list offset */
            uint32_t triangle_list_offset = 0;

            if (!system_hash64map_get(node_ptr_to_triangle_list_offset_map, system_hash64(current_b_node_ptr), &triangle_list_offset) )
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve triangle list offset!");
            }

            /* Fill node descriptor. */
            *((uint32_t*) nodes_buffer_traveller) = NODE_B;                       nodes_buffer_traveller += sizeof(uint32_t);
            *((uint32_t*) nodes_buffer_traveller) = triangle_list_offset;         nodes_buffer_traveller += sizeof(uint32_t);
            *((uint32_t*) nodes_buffer_traveller) = n_triangles_on_triangle_list; nodes_buffer_traveller += sizeof(uint32_t);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve B node descriptor");
        }
    }

    /* One of the assumptions is that root node can always be accessed at offset 0. Let's make sure this is actually the case */
    #ifdef _DEBUG
    {
        uint32_t root_node_offset = -1;

        if (system_hash64map_get(node_ptr_to_nodes_offset_map, system_hash64(data_ptr->root_node), &root_node_offset) )
        {
            ASSERT_DEBUG_SYNC(root_node_offset == 0, "Root node offset must be 0!");
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve root node index");
        }
    }
    #endif

    /* Fill left/right offsets for a-nodes */
    nodes_buffer_traveller = nodes_buffer;

    for (uint32_t n_a_node = 0; n_a_node < n_a_nodes; ++n_a_node)
    {
        _ocl_kdtree_temporary_node* current_a_node_ptr = NULL;

        if (system_resizable_vector_get_element_at(a_nodes_vector, n_a_node, &current_a_node_ptr) )
        {
            uint32_t node_type = *((uint32_t*) nodes_buffer_traveller);

            /* Move to left child offset */
            nodes_buffer_traveller += sizeof(uint32_t) + sizeof(uint32_t) + sizeof(float);

            if (node_type == NODE_A)
            {
                /* Retrieve offsets */
                uint32_t left_child_offset  = -1;
                uint32_t right_child_offset = -1;

                if (current_a_node_ptr->left != NULL)
                {
                    _ocl_kdtree_temporary_node* left_ptr = _ocl_kdtree_get_a_b_subnode(current_a_node_ptr->left);

                    if (!system_hash64map_get(node_ptr_to_nodes_offset_map, system_hash64(left_ptr), &left_child_offset) )
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve left child offset");
                    }
                }
                if (current_a_node_ptr->right != NULL)
                {
                    _ocl_kdtree_temporary_node* right_ptr = _ocl_kdtree_get_a_b_subnode(current_a_node_ptr->right);

                    if (!system_hash64map_get(node_ptr_to_nodes_offset_map, system_hash64(right_ptr), &right_child_offset) )
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve right child offset");
                    }
                }

                /* Fill the descriptor fields */
                *((uint32_t*)nodes_buffer_traveller) = left_child_offset;  nodes_buffer_traveller += sizeof(uint32_t);
                *((uint32_t*)nodes_buffer_traveller) = right_child_offset; nodes_buffer_traveller += sizeof(uint32_t);

                ASSERT_DEBUG_SYNC(left_child_offset != -1 && right_child_offset != -1, "Corrupt A node offsets");
            }
            else
            {
                /* It's a C-node, so let's just move on for now */
                nodes_buffer_traveller += sizeof(uint32_t) * 2;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve node descriptor");
        }
    } /* for (uint32_t n_a_node = 0; n_a_node < n_a_nodes; ++n_a_node) */

    /* Good! We can finally prepare the result buffer */
    data_ptr->kdtree_data_cl_buffer = new (std::nothrow) char[nodes_buffer_size + triangle_list_buffer_size + triangle_data_buffer_size];

    ASSERT_ALWAYS_SYNC(data_ptr->kdtree_data_cl_buffer != NULL, "Out of memory");
    if (data_ptr->kdtree_data_cl_buffer == NULL)
    {
        goto end;
    }

    char* clgl_buffer_ptr = (char*) data_ptr->kdtree_data_cl_buffer;

    memcpy(clgl_buffer_ptr, nodes_buffer,         nodes_buffer_size);         clgl_buffer_ptr += nodes_buffer_size;
    memcpy(clgl_buffer_ptr, triangle_list_buffer, triangle_list_buffer_size); clgl_buffer_ptr += triangle_list_buffer_size;
    memcpy(clgl_buffer_ptr, triangle_data_buffer, triangle_data_buffer_size);

    /* Store size */
    data_ptr->kdtree_data_cl_buffer_size = nodes_buffer_size + triangle_list_buffer_size + triangle_data_buffer_size;

    /* Store offsets */
    data_ptr->clgl_offset_nodes_data     = 0;
    data_ptr->clgl_offset_triangle_lists = nodes_buffer_size;
    data_ptr->clgl_offset_triangle_ids   = nodes_buffer_size + triangle_list_buffer_size;

    /* Clean up */
end:
    if (node_ptr_to_triangle_list_offset_map != NULL)
    {
        system_hash64map_release(node_ptr_to_triangle_list_offset_map);

        node_ptr_to_triangle_list_offset_map = NULL;
    }

    if (node_ptr_to_nodes_offset_map != NULL)
    {
        system_hash64map_release(node_ptr_to_nodes_offset_map);

        node_ptr_to_nodes_offset_map = NULL;
    }

    if (a_nodes_vector != NULL)
    {
        system_resizable_vector_release(a_nodes_vector);

        a_nodes_vector = NULL;
    }

    if (nodes_buffer != NULL)
    {
        delete [] nodes_buffer;
        
        nodes_buffer = NULL;
    }

    if (triangle_list_buffer != NULL)
    {
        delete [] triangle_list_buffer;

        triangle_list_buffer = NULL;
    }

    if (triangle_data_buffer != NULL)
    {
        delete [] triangle_data_buffer;

        triangle_data_buffer = NULL;
    }

    if (leaf_nodes_vector != NULL)
    {
        system_resizable_vector_release(leaf_nodes_vector);

        leaf_nodes_vector = NULL;
    }

    if (node_ptr_to_uint32_id != NULL)
    {
        system_hash64map_release(node_ptr_to_uint32_id);

        node_ptr_to_uint32_id = NULL;
    }

    if (nodes_to_traverse_vector != NULL)
    {
        system_resizable_vector_release(nodes_to_traverse_vector);

        nodes_to_traverse_vector = NULL;
    }
}

/** TODO */
PRIVATE void _ocl_kdtree_deinit(__in __notnull _ocl_kdtree* data_ptr)
{
    /* Release CL data buffer */
    if (data_ptr->kdtree_data_cl_buffer_cl != NULL)
    {
        const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();
        cl_int                    result       = -1;

        result                             = entry_points->pCLReleaseMemObject(data_ptr->kdtree_data_cl_buffer_cl);
        data_ptr->kdtree_data_cl_buffer_cl = 0;

        ASSERT_DEBUG_SYNC(result == CL_SUCCESS, "Could not release CL memory buffer!");
    }

    /* Release CL vertex data buffer */
    if (data_ptr->meshes_cl_data_buffer != NULL)
    {
        const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();
        cl_int                    result       = -1;

        result                          = entry_points->pCLReleaseMemObject(data_ptr->meshes_cl_data_buffer);
        data_ptr->meshes_cl_data_buffer = NULL;

        ASSERT_DEBUG_SYNC(result == CL_SUCCESS, "Could not release CL memory buffer!");
    }

    /* Release executors */
    if (data_ptr->executors != NULL)
    {
        _ocl_kdtree_executor* executor_ptr = NULL;

        while (system_resizable_vector_pop(data_ptr->executors, &executor_ptr) )
        {
            _ocl_kdtree_deinit_executor(executor_ptr);

            delete executor_ptr;
            executor_ptr = NULL;
        } /* while (system_resizable_vector_pop(data_ptr->executors, &executor_ptr) ) */

        system_resizable_vector_release(data_ptr->executors);
        data_ptr->executors = NULL;
    } /* if (data_ptr->executors != NULL) */

    /* Release process-side data buffers */
    if (data_ptr->kdtree_data_cl_buffer != NULL)
    {
        delete [] data_ptr->kdtree_data_cl_buffer;

        data_ptr->kdtree_data_cl_buffer = NULL;
    }

    if (data_ptr->serialize_meshes_data_buffer != NULL)
    {
        delete [] data_ptr->serialize_meshes_data_buffer;

        data_ptr->serialize_meshes_data_buffer = NULL;
    }

    /* Release OpenCL context */
    if (data_ptr->context != NULL)
    {
        ocl_context_release(data_ptr->context);
    }
    
    /* Release meshes */
    if (data_ptr->mesh_data != NULL)
    {
        mesh_release(data_ptr->mesh_data);
    }

    /* If a Kd tree was generated, it's time we deallocated it */
    if (data_ptr->root_node != NULL)
    {
        _ocl_kdtree_deinit_temporary_node(data_ptr->root_node);
    }

    if (data_ptr->node_pool != NULL)
    {
        system_resource_pool_release(data_ptr->node_pool);

        data_ptr->node_pool = NULL;
    }
}

/** TODO */
PRIVATE void _ocl_kdtree_deinit_executor(__in __notnull _ocl_kdtree_executor* executor_ptr)
{
    if (executor_ptr->cl_raycast_program_multiple != NULL)
    {
        ocl_program_release(executor_ptr->cl_raycast_program_multiple);
    }

    if (executor_ptr->cl_raycast_program_single != NULL)
    {
        ocl_program_release(executor_ptr->cl_raycast_program_single);
    }

    if (executor_ptr->cl_raycast_program_single_kernel != NULL)
    {
        ocl_kernel_release(executor_ptr->cl_raycast_program_single_kernel);
    }
}

/** TODO */
PRIVATE void _ocl_kdtree_deinit_temporary_node(__in __notnull _ocl_kdtree_temporary_node* node_ptr)
{
    if (node_ptr->triangles != NULL)
    {
        system_resizable_vector_release(node_ptr->triangles);

        node_ptr->triangles = NULL;
    }

    if (node_ptr->left != NULL)
    {
        _ocl_kdtree_deinit_temporary_node(node_ptr->left);
    }

    if (node_ptr->right != NULL)
    {
        _ocl_kdtree_deinit_temporary_node(node_ptr->right);
    }
}

/** TODO */
PRIVATE void _ocl_kdtree_fill(__in __notnull _ocl_kdtree* data_ptr)
{
    /* If root node or node pool is not null, then the function has already been called! */
    if (data_ptr->root_node != NULL || data_ptr->node_pool != NULL)
    {
        ASSERT_DEBUG_SYNC(false, "Kd tree has already been filled!");

        return;
    }

    /* Map each triangle vertex set to an id */
    const float* mesh_unique_normals    = NULL;
    const float* mesh_unique_vertices   = NULL;
    uint32_t     mesh_n_unique_normals  = 0;
    uint32_t     mesh_n_unique_vertices = 0;
    {
        {
            mesh_unique_normals = mesh_get_unique_stream_data(data_ptr->mesh_data, MESH_LAYER_DATA_STREAM_TYPE_NORMALS);
            ASSERT_ALWAYS_SYNC(mesh_unique_normals != NULL, "Unique normals data (GL) is NULL");

            mesh_unique_vertices = mesh_get_unique_stream_data(data_ptr->mesh_data, MESH_LAYER_DATA_STREAM_TYPE_VERTICES);
            ASSERT_ALWAYS_SYNC(mesh_unique_vertices != NULL, "Unique vertex data (GL) is NULL");

            mesh_get_property(data_ptr->mesh_data, MESH_PROPERTY_N_GL_UNIQUE_VERTICES, &mesh_n_unique_vertices);
            ASSERT_ALWAYS_SYNC(mesh_n_unique_vertices != 0, "Unique vertices counter is 0");
        }
    }

    /* Create triangle descriptors. During the process, also calculate bounding boxes for each triangle */
    uint32_t n_mesh_layer_passes = 0;
    uint32_t n_layers            = mesh_get_amount_of_layers(data_ptr->mesh_data);

    for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_layers; ++n_mesh_layer)
    {
        n_mesh_layer_passes += mesh_get_amount_of_layer_passes(data_ptr->mesh_data, n_mesh_layer);
    }

    uint32_t** mesh_layer_passes_elements    = new (std::nothrow) uint32_t*[n_mesh_layer_passes];
    uint32_t*  mesh_layer_passes_n_triangles = new (std::nothrow) uint32_t [n_mesh_layer_passes];
    uint32_t   n_entry                       = 0;
    uint32_t   summed_n_triangles            = 0;

    if (mesh_layer_passes_elements == NULL || mesh_layer_passes_n_triangles == NULL)
    {
        ASSERT_ALWAYS_SYNC(false, "Out of memory");

        return;
    }
    {
        for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
        {
            uint32_t n_layer_passes = mesh_get_amount_of_layer_passes(data_ptr->mesh_data, n_layer);

            for (uint32_t n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass)
            {
                mesh_get_layer_property(data_ptr->mesh_data, n_layer, n_layer_pass, MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA, &mesh_layer_passes_elements[n_entry]);
                ASSERT_ALWAYS_SYNC(mesh_layer_passes_elements[n_entry] != NULL, "Elements pointer is NULL");

                mesh_get_layer_property(data_ptr->mesh_data, n_layer, n_layer_pass, MESH_LAYER_PROPERTY_N_TRIANGLES, &mesh_layer_passes_n_triangles[n_entry]);
                ASSERT_ALWAYS_SYNC(mesh_layer_passes_n_triangles[n_entry] != 0, "Triangles counter is 0");

                summed_n_triangles += mesh_layer_passes_n_triangles[n_entry];
                n_entry            ++;
            }
        }
    }

    data_ptr->tree_triangles = new (std::nothrow) _ocl_kdtree_triangle[summed_n_triangles];
    
    ASSERT_ALWAYS_SYNC(data_ptr->tree_triangles != NULL, "Out of memory");
    if (data_ptr->tree_triangles == NULL)
    {
        goto end;
    }

    {
        uint32_t n_triangles_so_far = 0;

        for (uint32_t n_layer_pass = 0; n_layer_pass < n_mesh_layer_passes; ++n_layer_pass)
        {
            for (uint32_t n_triangle = 0; n_triangle < mesh_layer_passes_n_triangles[n_layer_pass]; n_triangle++, n_triangles_so_far++)
            {
                _ocl_kdtree_bounding_box* bb_ptr = &data_ptr->tree_triangles[n_triangles_so_far].bbox;

                data_ptr->tree_triangles[n_triangles_so_far].unique_vertex_indices[0] = mesh_layer_passes_elements[n_layer_pass][3*n_triangle  ];
                data_ptr->tree_triangles[n_triangles_so_far].unique_vertex_indices[1] = mesh_layer_passes_elements[n_layer_pass][3*n_triangle+1];
                data_ptr->tree_triangles[n_triangles_so_far].unique_vertex_indices[2] = mesh_layer_passes_elements[n_layer_pass][3*n_triangle+2];

                /* First vertex */
                const float* vertex_ptr = mesh_unique_vertices + 3 * data_ptr->tree_triangles[n_triangles_so_far].unique_vertex_indices[0];

                bb_ptr->x_min = vertex_ptr[0];
                bb_ptr->y_min = vertex_ptr[1];
                bb_ptr->z_min = vertex_ptr[2];
                bb_ptr->x_max = bb_ptr->x_min;
                bb_ptr->y_max = bb_ptr->y_min;
                bb_ptr->z_max = bb_ptr->z_min;

                /* Two remaining vertices */
                for (uint32_t m = 0; m < 2; ++m)
                {
                    vertex_ptr = mesh_unique_vertices + 3 * data_ptr->tree_triangles[n_triangles_so_far].unique_vertex_indices[1 + m];

                    float vertex_x = vertex_ptr[0];
                    float vertex_y = vertex_ptr[1];
                    float vertex_z = vertex_ptr[2];

                    if (bb_ptr->x_min > vertex_x)
                    {
                        bb_ptr->x_min = vertex_x;
                    }
                    if (bb_ptr->x_max < vertex_x)
                    {
                        bb_ptr->x_max = vertex_x;
                    }

                    if (bb_ptr->y_min > vertex_y)
                    {
                        bb_ptr->y_min = vertex_y;
                    }
                    if (bb_ptr->y_max < vertex_y)
                    {
                        bb_ptr->y_max = vertex_y;
                    }

                    if (bb_ptr->z_min > vertex_z)
                    {
                        bb_ptr->z_min = vertex_z;
                    }
                    if (bb_ptr->z_max < vertex_z)
                    {
                        bb_ptr->z_max = vertex_z;
                    }
                } /* for (uint32_t m = 0; m < 2; ++m) */
            } /* for (uint32_t n_triangle = 0; n_triangle < all_meshes_n_triangles[n]; n_triangle++, n_triangles_so_far++) */
        } /* for (uint32_t n_mesh = 0; n_mesh < data_ptr->n_meshes; ++data_ptr) */
    }

    /* We will also need a BB for the whole scene */
    {
        uint32_t n_vertices_total = 0;

        {
            __analysis_assume(mesh_n_unique_vertices > 0);

            for (uint32_t n_vertex = 0; n_vertex < mesh_n_unique_vertices; ++n_vertex, ++n_vertices_total)
            {
                const float* vertex_data = mesh_unique_vertices + n_vertex*3;

                if (n_vertices_total == 0)
                {
                    data_ptr->root_bb.x_max = vertex_data[0];
                    data_ptr->root_bb.y_max = vertex_data[1];
                    data_ptr->root_bb.z_max = vertex_data[2];

                    data_ptr->root_bb.x_min = data_ptr->root_bb.x_max;
                    data_ptr->root_bb.y_min = data_ptr->root_bb.y_max;
                    data_ptr->root_bb.z_min = data_ptr->root_bb.z_max;
                } /* if (n_vertex == 0) */
                else
                {
                    float vertex_x = vertex_data[0];
                    float vertex_y = vertex_data[1];
                    float vertex_z = vertex_data[2];

                    if (data_ptr->root_bb.x_min > vertex_x)
                    {
                        data_ptr->root_bb.x_min = vertex_x;
                    }
                    if (data_ptr->root_bb.x_max < vertex_x)
                    {
                        data_ptr->root_bb.x_max = vertex_x;
                    }

                    if (data_ptr->root_bb.y_min > vertex_y)
                    {
                        data_ptr->root_bb.y_min = vertex_y;
                    }
                    if (data_ptr->root_bb.y_max < vertex_y)
                    {
                        data_ptr->root_bb.y_max = vertex_y;
                    }

                    if (data_ptr->root_bb.z_min > vertex_z)
                    {
                        data_ptr->root_bb.z_min = vertex_z;
                    }
                    if (data_ptr->root_bb.z_max < vertex_z)
                    {
                        data_ptr->root_bb.z_max = vertex_z;
                    }
                } /* if (n_vertex != 0) */
            } /* for (uint32_t n_vertex = 0; n_vertex < n_unique_vertices; ++n_vertex) */
        } /* for (uint32_t n_mesh = 0; n_mesh < data_ptr->n_meshes; ++n_mesh) */
    }

    /* Calculate global volume - this will be used for one of the criterias */
    float global_volume = (data_ptr->root_bb.x_max - data_ptr->root_bb.x_min) * (data_ptr->root_bb.y_max - data_ptr->root_bb.y_min) * (data_ptr->root_bb.z_max - data_ptr->root_bb.z_min);

    /* Iterative recursion follows.
     *
     * FIRST, we push all triangles to root node. 
     * 
     * Then, we split the tree recursively along x or y or z. Bounding box for the entire node is computed and the axis that has
     * the longest span is selected. Then, we split halfway, sending items to one of the child nodes depending on which side of 
     * the split they sit on. Items that straddle the split line go both ways (they should refer to the same triangle).
     *
     * Recursion stops for a given leaf when any of the conditions holds:
     *
     * a) Number of items goes below 2;
     * b) BBox encapsulating all items in the node has small volume (determine by 0.01% of total scene volume)
     * c) All the items go to both children nodes after a split (in other words: they straddle on both sides)
     */
    data_ptr->node_pool =                               system_resource_pool_create       (sizeof(_ocl_kdtree_temporary_node), summed_n_triangles, NULL, NULL);
    data_ptr->root_node = (_ocl_kdtree_temporary_node*) system_resource_pool_get_from_pool(data_ptr->node_pool);
   
    /* Prepare the root node descriptor */
    system_resizable_vector work_items = system_resizable_vector_create(4, sizeof(_ocl_kdtree_temporary_node*) );

    _ocl_kdtree_init_temporary_node(data_ptr->root_node);

    data_ptr->root_node->bbox      = data_ptr->root_bb;
    data_ptr->root_node->triangles = system_resizable_vector_create(summed_n_triangles, sizeof(_ocl_kdtree_triangle*) );

    ASSERT_ALWAYS_SYNC(data_ptr->root_node->triangles != NULL, "Out of memory");
    if (data_ptr->root_node->triangles == NULL)
    {
        goto end;
    }

    for (uint32_t n = 0; n < summed_n_triangles; ++n)
    {
        system_resizable_vector_push(data_ptr->root_node->triangles, data_ptr->tree_triangles + n);
    }

    /* Push it to the work items vector */
    system_resizable_vector_push(work_items, data_ptr->root_node);

    /* Loop */
    while (system_resizable_vector_get_amount_of_elements(work_items) > 0)
    {
        /* Pop the work item */
        _ocl_kdtree_temporary_node* work_item = NULL;

        if (!system_resizable_vector_pop(work_items, &work_item) )
        {
            ASSERT_DEBUG_SYNC(false, "Could not pop temporary nod ework item");

            goto end;
        }

        /* Which axis should we split ? */
        float x_span = work_item->bbox.x_max - work_item->bbox.x_min;
        float y_span = work_item->bbox.y_max - work_item->bbox.y_min;
        float z_span = work_item->bbox.z_max - work_item->bbox.z_min;

        if (x_span > y_span && x_span > z_span)
        {
            work_item->axis_split = AXIS_SPLIT_X;
        }
        else
        if (y_span > z_span)
        {
            work_item->axis_split = AXIS_SPLIT_Y;
        }
        else
        {
            work_item->axis_split = AXIS_SPLIT_Z;
        }


        /* Calculate new bboxes */
        _ocl_kdtree_bounding_box new_bbox_left  = work_item->bbox;
        _ocl_kdtree_bounding_box new_bbox_right = work_item->bbox;

        switch (work_item->axis_split)
        {
            case AXIS_SPLIT_X:
            {
                new_bbox_left.x_max    = new_bbox_left.x_min + x_span * 0.5f;
                new_bbox_right.x_min   = new_bbox_left.x_max;
                new_bbox_right.x_max   = work_item->bbox.x_max;
                work_item->split_value = new_bbox_left.x_max;

                ASSERT_DEBUG_SYNC(new_bbox_left.x_max <= new_bbox_right.x_min, "");
                ASSERT_DEBUG_SYNC(new_bbox_left.x_min <= new_bbox_left.x_max,  "");

                break;
            }

            case AXIS_SPLIT_Y:
            {
                new_bbox_left.y_max    = new_bbox_left.y_min + y_span * 0.5f;
                new_bbox_right.y_min   = new_bbox_left.y_max;
                new_bbox_right.y_max   = work_item->bbox.y_max;
                work_item->split_value = new_bbox_left.y_max;

                ASSERT_DEBUG_SYNC(new_bbox_left.y_min <= new_bbox_left.y_max,  "");
                ASSERT_DEBUG_SYNC(new_bbox_left.y_max <= new_bbox_right.y_min, "");

                break;
            }

            case AXIS_SPLIT_Z:
            {
                new_bbox_left.z_max    = new_bbox_left.z_min + z_span * 0.5f;
                new_bbox_right.z_min   = new_bbox_left.z_max;
                new_bbox_right.z_max   = work_item->bbox.z_max;
                work_item->split_value = new_bbox_left.z_max;

                ASSERT_DEBUG_SYNC(new_bbox_left.z_min <= new_bbox_left.z_max,  "");
                ASSERT_DEBUG_SYNC(new_bbox_left.z_max <= new_bbox_right.z_min, "");

                break;
            }

            default: ASSERT_DEBUG_SYNC(false, "Invalid axis split enum value!");
        } /* switch (work_item->axis_split) */

        /* Check if conditions to split are met */
        uint32_t n_children_triangles = system_resizable_vector_get_amount_of_elements(work_item->triangles);

        bool     condition_n_children_acceptable = (n_children_triangles     >= data_ptr->max_triangles_per_leaf);
        bool     condition_volume_acceptable     = (x_span * y_span * z_span >= global_volume * data_ptr->min_leaf_volume_multiplier);
        bool     condition_children_splittable   = false;
        uint32_t n_left_children                 = 0;
        uint32_t n_right_children                = 0;

        for (uint32_t n_triangle = 0; n_triangle < n_children_triangles; ++n_triangle)
        {
            _ocl_kdtree_triangle* child_triangle = NULL;            

            if (system_resizable_vector_get_element_at(work_item->triangles, n_triangle, &child_triangle) )
            {
                bool triangle_goes_left  = false;
                bool triangle_goes_right = false;

                _ocl_kdtree_split_triangle(work_item->axis_split, child_triangle, &new_bbox_left, &new_bbox_right, &triangle_goes_left, &triangle_goes_right);

                triangle_goes_left  ? n_left_children++  : 0;
                triangle_goes_right ? n_right_children++ : 0;                
            }
            else
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve child triangle descriptor");
            }
        } /* for (uint32_t n_triangle = 0; n_triangle < n_children_triangles && (halves_used != half_left_index + half_right_index); ++n_triangle) */

        condition_children_splittable = !(n_left_children == n_children_triangles && n_right_children == n_children_triangles);

        /* We should proceed with moving the nodes down ONLY if none of the conditions are met */
        if (condition_n_children_acceptable && condition_volume_acceptable && condition_children_splittable)
        {
            _ocl_kdtree_temporary_node* new_left_subnode  = (_ocl_kdtree_temporary_node*) system_resource_pool_get_from_pool(data_ptr->node_pool);
            _ocl_kdtree_temporary_node* new_right_subnode = (_ocl_kdtree_temporary_node*) system_resource_pool_get_from_pool(data_ptr->node_pool);
            _ocl_kdtree_triangle*       triangle_ptr      = NULL;
            
            _ocl_kdtree_init_temporary_node(new_left_subnode);
            _ocl_kdtree_init_temporary_node(new_right_subnode);

            new_left_subnode->bbox       = new_bbox_left;
            new_left_subnode->triangles  = system_resizable_vector_create(n_left_children, sizeof(_ocl_kdtree_triangle*) );
            new_right_subnode->bbox      = new_bbox_right;
            new_right_subnode->triangles = system_resizable_vector_create(n_right_children, sizeof(_ocl_kdtree_triangle*) );
            
            /* Update work item bindings */
            work_item->left  = new_left_subnode;
            work_item->right = new_right_subnode;

            /* Split the triangles! */
            while (system_resizable_vector_pop(work_item->triangles, &triangle_ptr) )
            {
                bool triangle_goes_left  = false;
                bool triangle_goes_right = false;

                _ocl_kdtree_split_triangle(work_item->axis_split, triangle_ptr, &new_bbox_left, &new_bbox_right, &triangle_goes_left, &triangle_goes_right);

                if (triangle_goes_left)
                {
                    system_resizable_vector_push(new_left_subnode->triangles, triangle_ptr);
                }

                if (triangle_goes_right)
                {
                    system_resizable_vector_push(new_right_subnode->triangles, triangle_ptr);
                }
            }

            /* Get rid of children nodes that contain no triangles */
            if (system_resizable_vector_get_amount_of_elements(new_left_subnode->triangles) == 0)
            {
                system_resizable_vector_release    (new_left_subnode->triangles);
                system_resource_pool_return_to_pool(data_ptr->node_pool, (system_resource_pool_block) new_left_subnode);

                new_left_subnode = NULL;
                work_item->left  = NULL;
            }

            if (system_resizable_vector_get_amount_of_elements(new_right_subnode->triangles) == 0)
            {
                system_resizable_vector_release    (new_right_subnode->triangles);
                system_resource_pool_return_to_pool(data_ptr->node_pool, (system_resource_pool_block) new_right_subnode);

                new_right_subnode = NULL;
                work_item->right  = NULL;
            }

            /* Push the new work items to work on them in further iterations */
            if (new_left_subnode != NULL)
            {
                system_resizable_vector_push(work_items, new_left_subnode);
            }
            if (new_right_subnode != NULL)
            {
                system_resizable_vector_push(work_items, new_right_subnode);
            }

            /* Release work item's resizable vector - we no longer need it */
            system_resizable_vector_release(work_item->triangles);
            
            work_item->triangles = NULL;

        } /* if (!condition_n_children_acceptable && !condition_volume_acceptable && condition_children_splittable)*/

    } /* while (system_resizable_vector_get_amount_of_elements(work_items) > 0) */

    /* Kd tree is now filled. It is up to the caller to decide, whether he/she is interested in retrieving a CL memory buffer
     * that could be used for OpenCL kernels or a corresponding GL resource. */

    /* Clean up */
end:

    if (mesh_layer_passes_elements != NULL)
    {
        delete [] mesh_layer_passes_elements;

        mesh_layer_passes_elements = NULL;
    }

    if (mesh_layer_passes_n_triangles != NULL)
    {
        delete [] mesh_layer_passes_n_triangles;

        mesh_layer_passes_n_triangles = NULL;
    }
}

/** TODO */
PRIVATE bool _ocl_kdtree_float3_equal(__in_bcount(3*sizeof(float)) __notnull void* data1, __in_bcount(3*sizeof(float)) __notnull void* data2)
{
    float* data1_float = (float*) data1;
    float* data2_float = (float*) data2;

    return data1_float[0] == data2_float[0] && data1_float[1] == data2_float[1] && data1_float[2] == data2_float[2];
}

/** TODO */
PRIVATE bool _ocl_kdtree_float3_lower(__in_bcount(3*sizeof(float)) __notnull void* data1, __in_bcount(3*sizeof(float)) __notnull void* data2)
{
    float* data1_float = (float*) data1;
    float* data2_float = (float*) data2;

    return data1_float[0] < data2_float[0] && data1_float[1] < data2_float[1] && data1_float[2] < data2_float[2];
}

/** TODO */
PRIVATE void _ocl_kdtree_generate_kdtree_data_cl_buffer(__in __notnull _ocl_kdtree* data_ptr)
{
    const ocl_11_entrypoints* entry_points = ocl_get_entrypoints();
    cl_int                    error_code   = 0;

    data_ptr->kdtree_data_cl_buffer_cl = entry_points->pCLCreateBuffer(ocl_context_get_context(data_ptr->context),
                                                                       CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                                       data_ptr->kdtree_data_cl_buffer_size,
                                                                       data_ptr->kdtree_data_cl_buffer,
                                                                       &error_code);

    ASSERT_ALWAYS_SYNC(data_ptr->kdtree_data_cl_buffer_cl != 0, "Could not create CL memory buffer - error code [%d]", error_code);
    if (data_ptr->kdtree_data_cl_buffer_cl == 0)
    {
        goto end;
    }

    if ((data_ptr->creation_flags & KDTREE_SAVE_SUPPORT) == 0)
    {
        delete [] data_ptr->kdtree_data_cl_buffer;

        data_ptr->kdtree_data_cl_buffer = NULL;
    }
end: ;
}

/** TODO */
PRIVATE void _ocl_kdtree_generate_cl_vertex_data_buffer(__in __notnull _ocl_kdtree* data_ptr)
{
    cl_mem                                cl_bo_id                 = NULL;
    const ocl_11_entrypoints*             cl_entrypoints           = ocl_get_entrypoints();
    cl_int                                cl_error_code            = -1;
    uint32_t                              gl_bo_id                 = 0;
    uint32_t                              gl_bo_normals_offset     = 0;
    uint32_t                              gl_bo_normals_size       = 0;
    uint32_t                              gl_bo_vertices_offset    = 0;
    uint32_t                              gl_bo_vertices_size      = 0;
    const ocl_khr_gl_sharing_entrypoints* gl_sharing_entrypoints   = ocl_get_khr_gl_sharing_entrypoints();

    if (data_ptr->mesh_data != NULL)
    {
        {
            uint32_t n_vertices = 0;

            /* Retrieve mesh properties we'll need to create CL/GL mapped BO */
            if (!mesh_get_property(data_ptr->mesh_data, MESH_PROPERTY_GL_BO_ID,              &gl_bo_id)   ||
                !mesh_get_property(data_ptr->mesh_data, MESH_PROPERTY_N_GL_UNIQUE_VERTICES,  &n_vertices))
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve mesh property values");

                goto end;
            }

            gl_bo_normals_offset  = mesh_get_unique_stream_offset(data_ptr->mesh_data, MESH_LAYER_DATA_STREAM_TYPE_NORMALS);
            gl_bo_vertices_offset = mesh_get_unique_stream_offset(data_ptr->mesh_data, MESH_LAYER_DATA_STREAM_TYPE_VERTICES);

            gl_bo_normals_size  += n_vertices * 3 * sizeof(float);
            gl_bo_vertices_size += n_vertices * 3 * sizeof(float);
        }

        /* Create CL buffer objects from GL BO ids, so that we can copy all meshes' vertex data to one CL buffer object */
        {
            cl_bo_id = gl_sharing_entrypoints->pCLCreateFromGLBuffer(ocl_context_get_context(data_ptr->context), 
                                                                     CL_MEM_READ_ONLY,
                                                                     gl_bo_id,
                                                                     &cl_error_code);

            if (data_ptr == NULL || cl_error_code != CL_SUCCESS)
            {
                ASSERT_ALWAYS_SYNC(false, "Could not create a GL-based CL buffer object holding vertex data for mesh: error code [%d]", cl_error_code);

                goto end;
            }
        }

        {
            cl_error_code = gl_sharing_entrypoints->pCLEnqueueAcquireGLObjects(ocl_context_get_command_queue(data_ptr->context, 0),
                                                                               1,
                                                                               &cl_bo_id,
                                                                               0,
                                                                               NULL,
                                                                               NULL);

            if (cl_error_code != CL_SUCCESS)
            {
                ASSERT_ALWAYS_SYNC(false, "Could not acquire GL buffer object(s)");

                goto end;
            }
        }

        /* Allocate process-side buffer to store the data, if we will need to serialize it later on */
        if ((data_ptr->creation_flags & KDTREE_SAVE_SUPPORT) != 0)
        {
            data_ptr->serialize_meshes_data_buffer_size = gl_bo_normals_size + gl_bo_vertices_size;
            data_ptr->serialize_meshes_data_buffer      = new (std::nothrow) char[data_ptr->serialize_meshes_data_buffer_size];

            ASSERT_ALWAYS_SYNC(data_ptr->serialize_meshes_data_buffer != NULL, "Out of memory");
        }
    }

    /* Good, let's create the object */
    if (data_ptr->mesh_data != NULL)
    {
        data_ptr->meshes_cl_data_buffer = cl_entrypoints->pCLCreateBuffer(ocl_context_get_context(data_ptr->context), 
                                                                          CL_MEM_READ_WRITE,
                                                                          gl_bo_normals_size + gl_bo_vertices_size,
                                                                          NULL,
                                                                          &cl_error_code);
        if (data_ptr == NULL || cl_error_code != CL_SUCCESS)
        {
            ASSERT_ALWAYS_SYNC(false, "Could not create a CL buffer object to hold vertex data: error code [%d]", cl_error_code);

            goto end;
        }

        /* Fill it with data */
        uint32_t n_bytes_written = 0;

        {
            cl_error_code = cl_entrypoints->pCLEnqueueCopyBuffer(ocl_context_get_command_queue(data_ptr->context, 0),
                                                                 cl_bo_id,
                                                                 data_ptr->meshes_cl_data_buffer,
                                                                 gl_bo_vertices_offset,
                                                                 n_bytes_written,
                                                                 gl_bo_vertices_size,
                                                                 0,     /* no events in wait list */
                                                                 NULL,  /* no events in wait list */
                                                                 NULL); /* we don't need a wait evnet */

            data_ptr->meshes_cl_data_buffer_vertices_offset  = n_bytes_written;
            n_bytes_written                                 += gl_bo_vertices_size;

            if (cl_error_code == CL_SUCCESS)
            {
                cl_error_code = cl_entrypoints->pCLEnqueueCopyBuffer(ocl_context_get_command_queue(data_ptr->context, 0),
                                                                     cl_bo_id,
                                                                     data_ptr->meshes_cl_data_buffer,
                                                                     gl_bo_normals_offset,
                                                                     n_bytes_written,
                                                                     gl_bo_normals_size,
                                                                     0,     /* no events in wait list */
                                                                     NULL,  /* no events in wait list */
                                                                     NULL); /* we don't need a wait evnet */

                data_ptr->meshes_cl_data_buffer_normals_offset  = n_bytes_written;
                n_bytes_written                                += gl_bo_normals_size;
            }

            if (cl_error_code != CL_SUCCESS)
            {
                ASSERT_ALWAYS_SYNC(false, "clEnqueueCopyBuffer() failed");

                goto end;
            }
        }

        ASSERT_ALWAYS_SYNC(data_ptr->serialize_meshes_data_buffer == NULL ||
                           data_ptr->serialize_meshes_data_buffer != NULL && data_ptr->serialize_meshes_data_buffer_size == n_bytes_written,
                           "Sanity check failed");

        /* Copy the data to process-side vertex data buffer if one is maintained */
        if (data_ptr->serialize_meshes_data_buffer != NULL)
        {
            cl_error_code = cl_entrypoints->pCLEnqueueReadBuffer(ocl_context_get_command_queue(data_ptr->context, 0),
                                                                 data_ptr->meshes_cl_data_buffer,
                                                                 CL_TRUE,
                                                                 0,
                                                                 n_bytes_written,
                                                                 data_ptr->serialize_meshes_data_buffer,
                                                                 0,
                                                                 NULL,
                                                                 NULL);

            ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not read meshes data buffer");
        }
    }
    else
    {
        /* KD tree must have been loaded from a file, in which case we need to do a single copy operation */
        data_ptr->meshes_cl_data_buffer = cl_entrypoints->pCLCreateBuffer(ocl_context_get_context(data_ptr->context), 
                                                                          CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                                          data_ptr->serialize_meshes_data_buffer_size,
                                                                          data_ptr->serialize_meshes_data_buffer,
                                                                         &cl_error_code);
        if (data_ptr == NULL || cl_error_code != CL_SUCCESS)
        {
            ASSERT_ALWAYS_SYNC(false, "Could not create a CL buffer object to hold data: error code [%d]", cl_error_code);

            goto end;
        }
    }

end: ;
    if (cl_bo_id != NULL)
    {
        cl_error_code = gl_sharing_entrypoints->pCLEnqueueReleaseGLObjects(ocl_context_get_command_queue(data_ptr->context, 0),
                                                                           1,
                                                                           &cl_bo_id,
                                                                           0,
                                                                           NULL,
                                                                           NULL);
        ASSERT_ALWAYS_SYNC(cl_error_code == CL_SUCCESS, "Could not release acquired GL buffer object(s)");

        cl_error_code = cl_entrypoints->pCLReleaseMemObject(cl_bo_id);

        if (cl_error_code != CL_SUCCESS)
        {
            ASSERT_ALWAYS_SYNC(false, "Could not release memory object for mesh BO");
        }
        cl_bo_id = NULL;
    }
}

/** TODO */
PRIVATE _ocl_kdtree_temporary_node* _ocl_kdtree_get_a_b_subnode(__in __notnull _ocl_kdtree_temporary_node* curr_ptr)
{
    /* Move past C-nodes to either A-nodes or B-nodes*/
    while (curr_ptr != NULL)
    {
        if (curr_ptr->left != NULL && curr_ptr->right != NULL)
        {
            /* All right, it's a A-node */
            break; 
        }

        if (curr_ptr->left == NULL && curr_ptr->right == NULL)
        {
            /* Must be a B-node */
            break;
        }

        if (curr_ptr->left != NULL)
        {
            curr_ptr = curr_ptr->left;
        }
        else
        if (curr_ptr->right != NULL)
        {
            curr_ptr = curr_ptr->right;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Corrupt node encountered");
        }
    }

    return curr_ptr;
}


/** TODO */
PRIVATE void _ocl_kdtree_init(__in __notnull   _ocl_kdtree*          data_ptr,
                              __in __notnull   ocl_context           context,
                              __in             kdtree_creation_flags creation_flags, 
                              __in __maybenull mesh                  instance,
                              __in             float                 min_leaf_volume_multiplier,
                              __in             uint32_t              max_triangles_per_leaf,
                              __in             _ocl_kdtree_usage     usage)
{
    data_ptr->clgl_offset_nodes_data                   = -1;
    data_ptr->clgl_offset_triangle_ids                 = -1;
    data_ptr->clgl_offset_triangle_lists               = -1;
    data_ptr->context                                  = context;
    data_ptr->creation_flags                           = creation_flags;
    data_ptr->executors                                = system_resizable_vector_create(2 /* default capacity */, sizeof(_ocl_kdtree_executor*) );
    data_ptr->kdtree_data_cl_buffer                    = NULL;
    data_ptr->kdtree_data_cl_buffer_size               = 0;
    data_ptr->max_triangles_per_leaf                   = max_triangles_per_leaf;
    data_ptr->meshes_cl_data_buffer_normals_offset     = 0;
    data_ptr->meshes_cl_data_buffer_vertices_offset    = 0;
    data_ptr->mesh_data                                = NULL;
    data_ptr->min_leaf_volume_multiplier               = min_leaf_volume_multiplier;
    data_ptr->n_executors                              = 0;
    data_ptr->n_split_planes                           = 0;
    data_ptr->node_pool                                = NULL;
    data_ptr->cl_raycast_helper_bo                     = 0;
    data_ptr->cl_raycast_helper_bo_size                = 0;
    data_ptr->root_node                                = NULL;
    data_ptr->serialize_meshes_data_buffer             = NULL;
    data_ptr->serialize_meshes_data_buffer_size        = 0;
    data_ptr->tree_triangles                           = NULL;
    data_ptr->usage                                    = usage;

    ocl_context_retain(data_ptr->context);

    if (instance != NULL)
    {
        data_ptr->mesh_data = instance;

        mesh_retain(instance);
    }
    else
    {
        /* This will be executed if you load a kd tree */
    }
}

/** TODO */
PRIVATE bool _ocl_kdtree_init_programs(__in __notnull _ocl_kdtree*                       kdtree_ptr,
                                       __in __notnull _ocl_kdtree_executor*              executor_ptr,
                                       __in           _ocl_kdtree_executor_configuration configuration)
{
    const char* kernel_single_body_parts[]   = {"#define USE_SINGLE_RAY_ORIGIN\n", configuration.get_direction_vector_cl_code, configuration.reset_cl_code, configuration.update_cl_code, cl_kdtree_cast_kernel_body};
    const char* kernel_multiple_body_parts[] = {                                   configuration.get_direction_vector_cl_code, configuration.reset_cl_code, configuration.update_cl_code, cl_kdtree_cast_kernel_body};
    uint32_t    n_kernel_single_body_parts   = sizeof(kernel_single_body_parts)   / sizeof(kernel_single_body_parts  [0]);
    uint32_t    n_kernel_multiple_body_parts = sizeof(kernel_multiple_body_parts) / sizeof(kernel_multiple_body_parts[0]);
    bool        result                       = false;

    if ((kdtree_ptr->usage & OCL_KDTREE_USAGE_ONE_LOCATIONS_MANY_RAYS) != 0)
    {
        executor_ptr->cl_raycast_program_single = ocl_program_create_from_source(kdtree_ptr->context,
                                                                                 n_kernel_single_body_parts,
                                                                                 kernel_single_body_parts,
                                                                                 NULL,
                                                                                 system_hashed_ansi_string_create_by_merging_two_strings("CL raycast program single ", configuration.name) );

        ASSERT_ALWAYS_SYNC(executor_ptr->cl_raycast_program_single != NULL, "Could not instantiate CL raycast program (single)");
        if (executor_ptr->cl_raycast_program_single == NULL)
        {
            goto end;
        }
    }

    if ((kdtree_ptr->usage & OCL_KDTREE_USAGE_MANY_LOCATIONS_ONE_RAY) != 0)
    {
        executor_ptr->cl_raycast_program_multiple = ocl_program_create_from_source(kdtree_ptr->context,
                                                                                   n_kernel_multiple_body_parts,
                                                                                   kernel_multiple_body_parts,
                                                                                   NULL,
                                                                                   system_hashed_ansi_string_create_by_merging_two_strings("CL raycast program multiple ", configuration.name) );

        ASSERT_ALWAYS_SYNC(executor_ptr->cl_raycast_program_multiple != NULL, "Could not instantiate CL raycast program (multiple uchar8)");
        if (executor_ptr->cl_raycast_program_multiple == NULL)
        {
            goto end;
        }
    }

    if ((kdtree_ptr->usage & OCL_KDTREE_USAGE_ONE_LOCATIONS_MANY_RAYS) != 0)
    {
        executor_ptr->cl_raycast_program_single_kernel = ocl_program_get_kernel_by_name(executor_ptr->cl_raycast_program_single,
                                                                                        system_hashed_ansi_string_create("kdtree_cast_ray"));

        ASSERT_ALWAYS_SYNC(executor_ptr->cl_raycast_program_single_kernel != NULL, "Could not instantiate CL raycast program kernel (single)");
        if (executor_ptr->cl_raycast_program_single_kernel == NULL)
        {
            goto end;
        }
    }
    if ((kdtree_ptr->usage & OCL_KDTREE_USAGE_MANY_LOCATIONS_ONE_RAY) != 0)
    {
        executor_ptr->cl_raycast_program_multiple_kernel = ocl_program_get_kernel_by_name(executor_ptr->cl_raycast_program_multiple,
                                                                                          system_hashed_ansi_string_create("kdtree_cast_ray"));

        ASSERT_ALWAYS_SYNC(executor_ptr->cl_raycast_program_multiple_kernel != NULL, "Could not instantiate CL raycast program kernel (multiple)");
        if (executor_ptr->cl_raycast_program_multiple_kernel == NULL)
        {
            goto end;
        }
    }

    /* Done */
    result = true;

end:
    return result;
}

/** TODO */
PRIVATE void _ocl_kdtree_init_temporary_node(__in __notnull _ocl_kdtree_temporary_node* node_ptr)
{
    node_ptr->axis_split = 0; /* not a legit value */
    node_ptr->left       = NULL;
    node_ptr->right      = NULL;
    node_ptr->triangles  = NULL;
}

/** TODO */
PRIVATE void _ocl_kdtree_parse_a_node(__in  __notnull char*     traveller_ptr, 
                                      __out __notnull uint32_t* out_node_type,
                                      __out __notnull uint32_t* out_node_axis,
                                      __out __notnull float*    out_split_value,
                                      __out __notnull uint32_t* out_left_node_offset,
                                      __out __notnull uint32_t* out_right_node_offset)
{
    *out_node_type = *((uint32_t*)traveller_ptr);
    ASSERT_DEBUG_SYNC(*out_node_type == NODE_A, "Root node cannot be a leaf node!"); traveller_ptr += sizeof(uint32_t);

    *out_node_axis = *((uint32_t*)traveller_ptr);
    ASSERT_DEBUG_SYNC(*out_node_axis >= AXIS_SPLIT_X && *out_node_axis <= AXIS_SPLIT_Z, "Unrecognized split axis"); traveller_ptr += sizeof(uint32_t);

    *out_split_value  = *((float*)traveller_ptr);
    traveller_ptr    += sizeof(float);

    *out_left_node_offset = *((uint32_t*)traveller_ptr);
    traveller_ptr        += sizeof(uint32_t);

    *out_right_node_offset = *((uint32_t*)traveller_ptr);
    traveller_ptr         += sizeof(uint32_t);
}

/** TODO */
PRIVATE void _ocl_kdtree_release(void* arg)
{
    _ocl_kdtree* instance = (_ocl_kdtree*) arg;

    _ocl_kdtree_deinit(instance);
}

/** TODO */
PRIVATE void _ocl_kdtree_split_triangle(__in            uint32_t                  axis_split, 
                                        __in  __notnull _ocl_kdtree_triangle*     triangle,
                                        __in  __notnull _ocl_kdtree_bounding_box* left_bb, 
                                        __in  __notnull _ocl_kdtree_bounding_box* right_bb, 
                                        __out __notnull bool*                     out_goes_left,
                                        __out __notnull bool*                     out_goes_right)
 {
     float* triangle_bb_axis_max_ptr       = NULL;
     float* triangle_bb_axis_min_ptr       = NULL;
     float* new_node_bb_axis_left_max_ptr  = NULL;
     float* new_node_bb_axis_right_min_ptr = NULL;

     switch (axis_split)
     {
         case AXIS_SPLIT_X:
         {
             triangle_bb_axis_max_ptr       = &triangle->bbox.x_max;
             triangle_bb_axis_min_ptr       = &triangle->bbox.x_min;
             new_node_bb_axis_left_max_ptr  = &left_bb->x_max;
             new_node_bb_axis_right_min_ptr = &right_bb->x_min;

             break;
         }

         case AXIS_SPLIT_Y:
         {
             triangle_bb_axis_max_ptr       = &triangle->bbox.y_max;
             triangle_bb_axis_min_ptr       = &triangle->bbox.y_min;
             new_node_bb_axis_left_max_ptr  = &left_bb->y_max;
             new_node_bb_axis_right_min_ptr = &right_bb->y_min;

             break;
         }

         case AXIS_SPLIT_Z:
         {
             triangle_bb_axis_max_ptr       = &triangle->bbox.z_max;
             triangle_bb_axis_min_ptr       = &triangle->bbox.z_min;
             new_node_bb_axis_left_max_ptr  = &left_bb->z_max;
             new_node_bb_axis_right_min_ptr = &right_bb->z_min;

             break;
         }

         default: ASSERT_DEBUG_SYNC(false, "Invalid axis split enum value!");
     }

     *out_goes_left  = (*triangle_bb_axis_min_ptr <= *new_node_bb_axis_right_min_ptr);
     *out_goes_right = (*triangle_bb_axis_max_ptr >= *new_node_bb_axis_left_max_ptr);

     ASSERT_DEBUG_SYNC(*out_goes_left || *out_goes_right, "Triangle is about to get lost!");
 }

/** TODO */
PRIVATE bool _ocl_kdtree_triple_uint32_lower(__in __notnull void* arg1, __in __notnull void* arg2)
{
    uint32_t* arg1_uint32 = (uint32_t*) arg1;
    uint32_t* arg2_uint32 = (uint32_t*) arg2;

    return arg1_uint32[0] < arg2_uint32[0] && arg1_uint32[1] < arg2_uint32[1] && arg1_uint32[2] < arg2_uint32[2];
}

/** TODO */
PRIVATE bool _ocl_kdtree_triple_uint32_equal(__in __notnull void* arg1, __in __notnull void* arg2)
{
    uint32_t* arg1_uint32 = (uint32_t*) arg1;
    uint32_t* arg2_uint32 = (uint32_t*) arg2;

    return arg1_uint32[0] == arg2_uint32[0] && arg1_uint32[1] == arg2_uint32[1] && arg1_uint32[2] == arg2_uint32[2];
}

/** TODO */
PUBLIC EMERALD_API bool ocl_kdtree_add_executor(__in  __notnull ocl_kdtree                         instance,
                                                __in            _ocl_kdtree_executor_configuration configuration,
                                                __out __notnull ocl_kdtree_executor_id*            out_executor_id)
{
    _ocl_kdtree*           kdtree_ptr = (_ocl_kdtree*) instance;
    bool                   result     = false;
    ocl_kdtree_executor_id result_id  = -1;

    ASSERT_ALWAYS_SYNC(instance != NULL, "KDtree instance cannot be NULL");
    if (instance == NULL)
    {
        goto end;
    }

    /* Retrieve new executor ID */
    result_id = kdtree_ptr->n_executors;

    /* Initialize new executors */
    _ocl_kdtree_executor* executor_ptr = new (std::nothrow) _ocl_kdtree_executor;

    ASSERT_ALWAYS_SYNC(executor_ptr != NULL, "Out of memory");
    if (executor_ptr == NULL)
    {
        goto end;
    }

    /* Initialize executor fields */
    memset(executor_ptr, 0, sizeof(*executor_ptr) );

    result = _ocl_kdtree_init_programs(kdtree_ptr, executor_ptr, configuration);

    ASSERT_ALWAYS_SYNC(result, "Could not initialize Kd tree CL programs.");
    if (!result)
    {
        goto end;
    }

    /* Store the new executor */
    ASSERT_DEBUG_SYNC(system_resizable_vector_get_amount_of_elements(kdtree_ptr->executors) == kdtree_ptr->n_executors, "Executor counter is corrupt");

    system_resizable_vector_push(kdtree_ptr->executors, executor_ptr);

    /* Good to return a valid executor id */
    *out_executor_id = result_id;
    result           = true;

    kdtree_ptr->n_executors ++;

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API ocl_kdtree ocl_kdtree_create_from_mesh(__in __notnull ocl_context               context,
                                                          __in           kdtree_creation_flags     creation_flags,
                                                          __in __notnull mesh                      mesh,
                                                          __in __notnull system_hashed_ansi_string name,
                                                          __in           float                     min_leaf_volume_multiplier,
                                                          __in           uint32_t                  max_triangles_per_leaf,
                                                          __in           _ocl_kdtree_usage         usage)
{
    _ocl_kdtree* new_instance = new (std::nothrow) _ocl_kdtree;

    ASSERT_ALWAYS_SYNC(min_leaf_volume_multiplier >= 0 && min_leaf_volume_multiplier <= 1, "Multiplier is not normalized");
    ASSERT_ALWAYS_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        /* initialize */
        _ocl_kdtree_init(new_instance, context, creation_flags, mesh, min_leaf_volume_multiplier, max_triangles_per_leaf, usage);

        /* Fill with data */
        _ocl_kdtree_fill(new_instance);

        /* Create CL/GL-friendly representation */
        _ocl_kdtree_convert_to_cl_gl_representation(new_instance);

        /* Create CL buffers and initialize them */
        _ocl_kdtree_generate_kdtree_data_cl_buffer(new_instance);

        /* Create CL buffer object from a GL buffer used by the mesh instance to hold vertex data */
        _ocl_kdtree_generate_cl_vertex_data_buffer(new_instance);

        /* Release triangle storage */
        if (new_instance->tree_triangles != NULL)
        {
            delete [] new_instance->tree_triangles;

            new_instance->tree_triangles = NULL;
        }

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance, 
                                                       _ocl_kdtree_release,
                                                       OBJECT_TYPE_OCL_KDTREE, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenCL Kd Trees\\", system_hashed_ansi_string_get_buffer(name) )
                                                       );

    }
    
    return (ocl_kdtree) new_instance;
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_cl_buffer_size(__in __notnull ocl_kdtree instance)
{
    return ((_ocl_kdtree*)instance)->kdtree_data_cl_buffer_size;
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_nodes_data_offset(__in __notnull ocl_kdtree instance)
{
    return ((_ocl_kdtree*) instance)->clgl_offset_nodes_data;
}

/* Please see header for specification */
PUBLIC EMERALD_API void ocl_kdtree_get_cl_mesh_vertex_data_buffer(__in  __notnull   ocl_kdtree instance,
                                                                  __out __maybenull uint32_t*  out_size,
                                                                  __out __maybenull void**     out_data)
{
    _ocl_kdtree* kdtree_ptr = (_ocl_kdtree*) instance;

    if (out_size != NULL)
    {
        *out_size = kdtree_ptr->serialize_meshes_data_buffer_size;
    }

    if (out_data != NULL)
    {
        *out_data = kdtree_ptr->serialize_meshes_data_buffer;
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API _ocl_kdtree_executor_configuration ocl_kdtree_get_float2_executor_configuration(__in bool use_vec3_for_ray_direction_source)
{
    static const char*                 float2_name        = "float2";
    static const char*                 float2_reset_code  = "#define RESET_RESULT  vstore2((float2)(-1), (n_ray_origin * n_rays + ray_index), (__global float*) result);\n";
    static const char*                 float2_update_code = "#define UPDATE_RESULT ((__global float*)result)[2*(n_ray_origin * n_rays + ray_index)    ] = u;\\\n"
                                                            "                      ((__global float*)result)[2*(n_ray_origin * n_rays + ray_index) + 1] = v;\n";
    _ocl_kdtree_executor_configuration result             = {0};

    result.get_direction_vector_cl_code = (use_vec3_for_ray_direction_source) ? "#define GET_RAY_DIRECTION normalize(vload3(ray_index, ray_directions + ray_directions_offset));\n"
                                                                              : "#define GET_RAY_DIRECTION normalize(vload4(ray_index, ray_directions + ray_directions_offset).xyz);\n";
    result.name                         = float2_name;
    result.reset_cl_code                = float2_reset_code;
    result.update_cl_code               = float2_update_code;

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API _ocl_kdtree_executor_configuration ocl_kdtree_get_uchar8_executor_configuration(__in bool use_vec3_for_ray_direction_source)
{
    _ocl_kdtree_executor_configuration result             = {0};
    static const char*                 uchar8_name        = "uchar8";
    static const char*                 uchar8_reset_code  = "#define RESET_RESULT  result[n_ray_origin * n_rays + ray_index] = 0;\n";
    static const char*                 uchar8_update_code = "#define UPDATE_RESULT result[n_ray_origin * n_rays + ray_index] = 1;\n";

    result.get_direction_vector_cl_code = (use_vec3_for_ray_direction_source) ? "#define GET_RAY_DIRECTION normalize(vload3(ray_index, ray_directions + ray_directions_offset));\n"
                                                                              : "#define GET_RAY_DIRECTION normalize(vload4(ray_index, ray_directions + ray_directions_offset).xyz);\n";
    result.name           = uchar8_name;
    result.reset_cl_code  = uchar8_reset_code;
    result.update_cl_code = uchar8_update_code;

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_kdtree_get_preview_data(__in  __notnull ocl_kdtree instance,
                                                    __out __notnull uint32_t*  out_n_lines,
                                                    __out __notnull float**    out_result)
{
    _ocl_kdtree* data_ptr = (_ocl_kdtree*) instance;
    bool         result   = false;

    ASSERT_DEBUG_SYNC(data_ptr   != NULL, "Kd tree instance is NULL");
    ASSERT_DEBUG_SYNC(out_result != NULL, "out_result is NULL");
    if (data_ptr != NULL && out_result != NULL)
    {
        ASSERT_DEBUG_SYNC(data_ptr->n_split_planes != 0, "Number of split planes must not be equal to 0 when generating preview data");

        /* Compute number of nodes. Each node splits the bounding box, so using this information we should be able
         * to predict how large the result buffer should be. */
        uint32_t n_nodes            = (data_ptr->clgl_offset_triangle_lists - data_ptr->clgl_offset_nodes_data) / sizeof(_ocl_kdtree_temporary_node);
        uint32_t result_buffer_size = ((data_ptr->n_split_planes)                                       * 4 /* number of lines */ * 2 /* each line consists of 2 points */ +
                                       12                         /* number of lines to form root BB */ * 2 /* as above */                                                  ) * 3 /* 3D */ * sizeof(float);

        /* Allocate result buffer */
        *out_result  = new (std::nothrow) float[result_buffer_size / sizeof(float)];
        *out_n_lines = result_buffer_size / sizeof(float) / 3;

        ASSERT_ALWAYS_SYNC(*out_result != NULL, "Out of memory");
        if (*out_result == NULL)
        {
            goto end;
        }

        /* Start with global bounding box */
        float* result_traveller = *out_result;

        /* front quad */
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;

        /* rear quad */
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        /* top quad */
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_min; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        /* bottom quad */
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_max; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_min; result_traveller+=3;
        *(result_traveller) = data_ptr->root_bb.x_min; *(result_traveller+1) = data_ptr->root_bb.y_max; *(result_traveller+2) = data_ptr->root_bb.z_max; result_traveller+=3;

        /* Split planes follow */
        typedef struct 
        {
            _ocl_kdtree_bounding_box  bb;
            uint32_t                  this_node_offset;
        } kdtree_preview_node;

        system_resource_pool    allocator =                        system_resource_pool_create       (sizeof(kdtree_preview_node), n_nodes, NULL, NULL);
        system_resizable_vector nodes     =                        system_resizable_vector_create    (4, sizeof(void*) );
        kdtree_preview_node*    root      = (kdtree_preview_node*) system_resource_pool_get_from_pool(allocator);
        
        /* Read root node data */
        uint32_t  node_type             = -1;
        uint32_t  node_split_axis       = -1;
        float     node_split_value      = 0;
        uint32_t  node_left_offset      = 0;
        uint32_t  node_right_offset     = 0;

        _ocl_kdtree_parse_a_node((char*) data_ptr->kdtree_data_cl_buffer, 
                                 &node_type,
                                 &node_split_axis,
                                 &node_split_value,
                                 &node_left_offset,
                                 &node_right_offset);        

        /* Construct root node's descriptor */
        root->bb                = data_ptr->root_bb;
        root->this_node_offset  = 0;

        system_resizable_vector_push(nodes, root);

        /* Loop until we run out of nodes */
        kdtree_preview_node* current_node = NULL;

        while (system_resizable_vector_pop(nodes, &current_node) )
        {
            char* this_node_ptr = (char*)data_ptr->kdtree_data_cl_buffer + current_node->this_node_offset;

            /* Parse current node */
            _ocl_kdtree_parse_a_node(this_node_ptr, 
                                     &node_type,
                                     &node_split_axis,
                                     &node_split_value,
                                     &node_left_offset,
                                     &node_right_offset);

            ASSERT_DEBUG_SYNC(node_type == NODE_A, "Leaf nodes should never be analysed!");

            char*    left_node_ptr   = (node_left_offset  != 0 ? (char*)data_ptr->kdtree_data_cl_buffer + node_left_offset  : NULL);
            uint32_t left_node_type  = (node_left_offset  != 0 ? *((uint32_t*) left_node_ptr)                               : 0);
            char*    right_node_ptr  = (node_right_offset != 0 ? (char*)data_ptr->kdtree_data_cl_buffer + node_right_offset : NULL);
            uint32_t right_node_type = (node_right_offset != 0 ? *((uint32_t*) right_node_ptr)                              : 0);

            /* Determine children bounding boxes */
            _ocl_kdtree_bounding_box current_node_bb_left_splitted  = current_node->bb;
            _ocl_kdtree_bounding_box current_node_bb_right_splitted = current_node->bb;
            float                    plane_points[3*4*2];

            switch (node_split_axis)
            {
                case AXIS_SPLIT_X:
                {
                    ASSERT_DEBUG_SYNC(current_node->bb.x_min < node_split_value, "Split error");
                    ASSERT_DEBUG_SYNC(current_node->bb.x_max > node_split_value, "Split error");

                    current_node_bb_left_splitted.x_max  = node_split_value;
                    current_node_bb_right_splitted.x_min = node_split_value;

                    plane_points[0 ] = node_split_value; plane_points[1]  = current_node->bb.y_min; plane_points[2]  = current_node->bb.z_min;
                    plane_points[3 ] = node_split_value; plane_points[4]  = current_node->bb.y_min; plane_points[5]  = current_node->bb.z_max;

                    plane_points[6 ] = node_split_value; plane_points[7]  = current_node->bb.y_min; plane_points[8]  = current_node->bb.z_max;
                    plane_points[9 ] = node_split_value; plane_points[10] = current_node->bb.y_max; plane_points[11] = current_node->bb.z_max;

                    plane_points[12] = node_split_value; plane_points[13] = current_node->bb.y_max; plane_points[14] = current_node->bb.z_max;
                    plane_points[15] = node_split_value; plane_points[16] = current_node->bb.y_max; plane_points[17] = current_node->bb.z_min;

                    plane_points[18] = node_split_value; plane_points[19] = current_node->bb.y_max; plane_points[20] = current_node->bb.z_min;
                    plane_points[21] = node_split_value; plane_points[22] = current_node->bb.y_min; plane_points[23] = current_node->bb.z_min;

                    ASSERT_DEBUG_SYNC(current_node_bb_left_splitted.x_max  > current_node_bb_left_splitted.x_min, "Split error");
                    ASSERT_DEBUG_SYNC(current_node_bb_right_splitted.x_max > current_node_bb_right_splitted.x_min,"Split error");

                    break;
                }

                case AXIS_SPLIT_Y:
                {
                    ASSERT_DEBUG_SYNC(current_node->bb.y_min < node_split_value, "Split error");
                    ASSERT_DEBUG_SYNC(current_node->bb.y_max > node_split_value, "Split error");

                    current_node_bb_left_splitted.y_max  = node_split_value;
                    current_node_bb_right_splitted.y_min = node_split_value;

                    plane_points[0 ] = current_node->bb.x_min; plane_points[1]  = node_split_value; plane_points[2]  = current_node->bb.z_min;
                    plane_points[3 ] = current_node->bb.x_min; plane_points[4]  = node_split_value; plane_points[5]  = current_node->bb.z_max;

                    plane_points[6 ] = current_node->bb.x_min; plane_points[7]  = node_split_value; plane_points[8]  = current_node->bb.z_max;
                    plane_points[9 ] = current_node->bb.x_max; plane_points[10] = node_split_value; plane_points[11] = current_node->bb.z_max;

                    plane_points[12] = current_node->bb.x_max; plane_points[13] = node_split_value; plane_points[14] = current_node->bb.z_max;
                    plane_points[15] = current_node->bb.x_max; plane_points[16] = node_split_value; plane_points[17] = current_node->bb.z_min;

                    plane_points[18] = current_node->bb.x_max; plane_points[19] = node_split_value; plane_points[20] = current_node->bb.z_min;
                    plane_points[21] = current_node->bb.x_min; plane_points[22] = node_split_value; plane_points[23] = current_node->bb.z_min;

                    ASSERT_DEBUG_SYNC(current_node_bb_left_splitted.y_max  > current_node_bb_left_splitted.y_min, "Split error");
                    ASSERT_DEBUG_SYNC(current_node_bb_right_splitted.y_max > current_node_bb_right_splitted.y_min,"Split error");

                    break;
                }

                case AXIS_SPLIT_Z:
                {
                    ASSERT_DEBUG_SYNC(current_node->bb.z_min < node_split_value, "Split error");
                    ASSERT_DEBUG_SYNC(current_node->bb.z_max > node_split_value, "Split error");

                    current_node_bb_left_splitted.z_max  = node_split_value;
                    current_node_bb_right_splitted.z_min = node_split_value;

                    plane_points[0 ] = current_node->bb.x_min; plane_points[1]  = current_node->bb.y_min; plane_points[2]  = node_split_value;
                    plane_points[3 ] = current_node->bb.x_min; plane_points[4]  = current_node->bb.y_max; plane_points[5]  = node_split_value;

                    plane_points[6 ] = current_node->bb.x_min; plane_points[7]  = current_node->bb.y_max; plane_points[8]  = node_split_value;
                    plane_points[9 ] = current_node->bb.x_max; plane_points[10] = current_node->bb.y_max; plane_points[11] = node_split_value;

                    plane_points[12] = current_node->bb.x_max; plane_points[13] = current_node->bb.y_max; plane_points[14] = node_split_value;
                    plane_points[15] = current_node->bb.x_max; plane_points[16] = current_node->bb.y_min; plane_points[17] = node_split_value;

                    plane_points[18] = current_node->bb.x_max; plane_points[19] = current_node->bb.y_min; plane_points[20] = node_split_value;
                    plane_points[21] = current_node->bb.x_min; plane_points[22] = current_node->bb.y_min; plane_points[23] = node_split_value;

                    ASSERT_DEBUG_SYNC(current_node_bb_left_splitted.z_max  > current_node_bb_left_splitted.z_min, "Split error");
                    ASSERT_DEBUG_SYNC(current_node_bb_right_splitted.z_max > current_node_bb_right_splitted.z_min,"Split error");

                    break;
                }

                default: ASSERT_DEBUG_SYNC(false, "Unrecognized split axis encountered [%d]", node_split_axis);
            }

            /* Store the split plane */
            memcpy(result_traveller, plane_points, sizeof(float) * 24);
            result_traveller += 24;

            /* We ignore b-subnodes as we are not that much interested in triangle lists */
            if (left_node_type == NODE_A)
            {
                kdtree_preview_node* new_left_node = (kdtree_preview_node*) system_resource_pool_get_from_pool(allocator);

                new_left_node->bb               = current_node_bb_left_splitted;
                new_left_node->this_node_offset = node_left_offset;

                system_resizable_vector_push(nodes, new_left_node);
            }

            if (right_node_type == NODE_A)
            {
                kdtree_preview_node* new_right_node = (kdtree_preview_node*) system_resource_pool_get_from_pool(allocator);

                new_right_node->bb               = current_node_bb_right_splitted;
                new_right_node->this_node_offset = node_right_offset;

                system_resizable_vector_push(nodes, new_right_node);
            }

            /* We won't be needing the node we have been working on up to this point, so put it back in the pool */
            system_resource_pool_return_to_pool(allocator, (system_resource_pool_block) current_node);
        }

        /* Make sure we did not corrupt the buffer or the heap in general */
        ASSERT_DEBUG_SYNC((uint32_t) (result_traveller - *out_result) <= result_buffer_size / sizeof(float), "Buffer overflow/underflow detected");

        /* Clean up */
        if (allocator != NULL)
        {
            system_resource_pool_release(allocator);

            allocator = NULL;
        }

        if (nodes != NULL)
        {
            system_resizable_vector_release(nodes);

            nodes = NULL;
        }
    }
    
end: ;
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_triangle_ids_offset(__in __notnull ocl_kdtree instance)
{
    return ((_ocl_kdtree*) instance)->clgl_offset_triangle_ids;
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t ocl_kdtree_get_triangle_lists_offset(__in __notnull ocl_kdtree instance)
{
    return ((_ocl_kdtree*) instance)->clgl_offset_triangle_lists;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_kdtree_intersect_rays(__in                         __notnull   ocl_kdtree                         instance,
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
                                                  __in                         __maybenull void*                              on_next_ray_origin_callback_user_arg)

{
    const ocl_11_entrypoints*             entry_points            = ocl_get_entrypoints();
    const ocl_khr_gl_sharing_entrypoints* gl_sharing_entry_points = ocl_get_khr_gl_sharing_entrypoints();
    _ocl_kdtree*                          kdtree_ptr              = (_ocl_kdtree*) instance;

    /* Sanity checks */
    if (target_triangle_hit_data_mo != NULL && !should_store_triangle_hit_data)
    {
        ASSERT_ALWAYS_SYNC(false, "Sanity check failed");

        return false;
    }

    ASSERT_DEBUG_SYNC(n_rays != 1 || n_rays == 1 && ray_origins_mo != NULL, "Ray origins must be passed in memory object if you intend to shoot a single ray for multiple locations");
    ASSERT_DEBUG_SYNC(n_rays == 1 || n_rays != 1 && ray_origins_mo == NULL, "Ray origins must be passed in process-side array if you intend to shoot multiple rays for multiple locations");

    /* Retrieve executor */
    _ocl_kdtree_executor* executor_ptr = NULL;

    if (!system_resizable_vector_get_element_at(kdtree_ptr->executors, executor_id, &executor_ptr) )
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve executor descriptor");

        return false;
    }

    /* Set kernel arguments */
    cl_int cl_result       = -1;
    float  scene_bb_min[4] = {kdtree_ptr->root_bb.x_min, kdtree_ptr->root_bb.y_min, kdtree_ptr->root_bb.z_min, 0};
    float  scene_bb_max[4] = {kdtree_ptr->root_bb.x_max, kdtree_ptr->root_bb.y_max, kdtree_ptr->root_bb.z_max, 0};

    ASSERT_DEBUG_SYNC(scene_bb_min[0] < scene_bb_max[0] && scene_bb_min[1] < scene_bb_max[1] && scene_bb_min[2] < scene_bb_max[2], "BB corrupt");

    /* Set kernel arguments: min BB */
    ocl_kernel kernel_handle_emerald = (ray_origins_mo != NULL) ? (executor_ptr->cl_raycast_program_multiple_kernel) : (executor_ptr->cl_raycast_program_single_kernel);
    cl_kernel  kernel_handle         = ocl_kernel_get_kernel(kernel_handle_emerald);

    cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                              0,
                                              sizeof(float) * 4,
                                              scene_bb_min);

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: max BB */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  1,
                                                  sizeof(float) * 4,
                                                  scene_bb_max);
    }

    /* NOTE: This is a memory object only if we're using multiple ray origins! */
    if (cl_result == CL_SUCCESS && ray_origins_mo != NULL)
    {
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  2,
                                                  sizeof(cl_mem),
                                                  &ray_origins_mo);
        ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not set ray origins' memory object");
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: ray directions */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  3, 
                                                  sizeof(cl_mem),
                                                  &ray_directions_mo);
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: kdtree data */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  4,
                                                  sizeof(cl_mem),
                                                  &kdtree_ptr->kdtree_data_cl_buffer_cl);
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: result data */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  5,
                                                  sizeof(cl_mem),
                                                  &target_mo);
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: n rays */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  6,
                                                  sizeof(uint32_t),
                                                  (ray_origins == NULL) ? &n_ray_origins : &n_rays);
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: triangle list offset */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  7,
                                                  sizeof(uint32_t),
                                                  &kdtree_ptr->clgl_offset_triangle_lists);
    }

    if (cl_result == CL_SUCCESS)
    {
        /* Set kernel argument: triangle data offset */
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  8,
                                                  sizeof(uint32_t),
                                                  &kdtree_ptr->clgl_offset_triangle_ids);
    }

    ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not set kernel arguments");
    if (cl_result == CL_SUCCESS)
    {
        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  9,
                                                  sizeof(uint32_t),
                                                  &kdtree_ptr->meshes_cl_data_buffer);
        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not bind data buffer to the kernel");

        if (target_triangle_hit_data_mo != NULL)
        {
            /* Set up the target triangle hit data buffer, too */
            cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                      11,
                                                      sizeof(cl_mem),
                                                      &target_triangle_hit_data_mo);
            ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not bind target triangle hit data buffer to the kernel");
        }
        else
        {
            /* Use any of the buffers so that CL does not scowl about invalid argument */
            cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                      11,
                                                      sizeof(cl_mem),
                                                      &kdtree_ptr->meshes_cl_data_buffer);
            ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set helper triangle hit data buffer");
        }

        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  12,
                                                  sizeof(uint32_t),
                                                  &should_store_triangle_hit_data);
        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set 'should store triangle hit data' argument");

        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  13,
                                                  sizeof(uint32_t),
                                                  &should_find_closest_intersection);
        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set 'should find closest intersection' argument");

        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  14,
                                                  sizeof(uint32_t),
                                                  &ray_directions_mo_offset);
        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set 'ray directions mo vec4 offset' argument");

        cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                  15,
                                                  sizeof(uint32_t),
                                                  &kdtree_ptr->meshes_cl_data_buffer_normals_offset);
        ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set 'unique vertex data normals offset' argument");

        if (ray_origins_mo != NULL)
        {
            cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                      16,
                                                      sizeof(uint32_t),
                                                      &ray_origins_mo_vec4_offset);
            ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not set 'ray origins mo vec4 offset' argument");
        } /* if (n_rays == 1) */

        /* Good - we can finally do the processing! First, retrieve device properties. */
        cl_platform_id         platform_id            = ocl_context_get_platform_id(kdtree_ptr->context);
        cl_device_id           device_id              = NULL;
        const ocl_device_info* device_info_ptr        = NULL;
        uint32_t               kernel_work_group_size = 0;
        uint32_t               max_compute_units      = 0;
        size_t                 max_work_group_size    = 0;
        size_t*                max_work_item_sizes    = NULL;

        if (!ocl_get_device_id(platform_id, 0, &device_id) )
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve device id");

            goto end;
        }

        if ( (device_info_ptr = ocl_get_device_info(platform_id, device_id)) == NULL)
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve device properties");

            goto end;
        }

        kernel_work_group_size = ocl_kernel_get_work_group_size(kernel_handle_emerald);
        max_compute_units      = device_info_ptr->device_max_compute_units;
        max_work_group_size    = device_info_ptr->device_max_work_group_size;
        max_work_item_sizes    = device_info_ptr->device_max_work_item_sizes;

        /* Calculate work size */
        uint32_t global_work_size = 0;
        uint32_t local_work_size  = 0;

        local_work_size  = (max_work_group_size > kernel_work_group_size ? kernel_work_group_size : max_work_group_size);
        global_work_size = (((ray_origins_mo != NULL) ? n_ray_origins : n_rays) / local_work_size + 1) * local_work_size;

        /* For each ray origin, enqueue the kernel execution. The reason for which we're doing this in a batched manner, instead
         * of a single huge request is that we do not want to block the driver for a longer period of time. This could cause driver reset
         * in case of NVIDIA
         */
        cl_event wait_event = NULL;

        for (uint32_t n_ray_origin = 0; n_ray_origin < ((ray_origins_mo != NULL) ? 1 : n_ray_origins); ++n_ray_origin)
        {
            /* Call back first */
            if (on_next_ray_origin_callback_proc != NULL)
            {
                on_next_ray_origin_callback_proc(n_ray_origin, on_next_ray_origin_callback_user_arg);
            }

            /* Set kernel argument: ray origin */
            if (ray_origins_mo == NULL)
            {
                float ray_origin4[4] = {ray_origins[n_ray_origin*3], ray_origins[n_ray_origin*3+1], ray_origins[n_ray_origin*3+2], 0};

                cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                          2,
                                                          sizeof(float) * 4,
                                                          ray_origin4);
                ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not update ray origin");
            }

            /* Set kernel argument: ray origin index */
            if (cl_result == CL_SUCCESS)
            {
                cl_result = entry_points->pCLSetKernelArg(kernel_handle,
                                                          10,
                                                          sizeof(uint32_t),
                                                          &n_ray_origin);
                ASSERT_ALWAYS_SYNC(cl_result == CL_SUCCESS, "Could not update ray origin index");
            }

            /* Execute. Use a wait event, because otherwise the driver the requests are cached <as expected>, but after
             * exceeding a certain threshold, the driver locks up and waits till all queued tasks are processed.. */
            if (cl_result == CL_SUCCESS)
            {
                cl_result = entry_points->pCLEnqueueNDRangeKernel(ocl_context_get_command_queue(kdtree_ptr->context, 0),
                                                                  kernel_handle,
                                                                  1,
                                                                  NULL, /* no global work offset  */
                                                                  &global_work_size,
                                                                  &local_work_size,
                                                                  0, 
                                                                  NULL,
                                                                  &wait_event);

                entry_points->pCLWaitForEvents(1, &wait_event);
            }

            ASSERT_DEBUG_SYNC(cl_result == CL_SUCCESS, "Could not enqueue ND range kernel");
            if (cl_result != CL_SUCCESS)
            {
                goto end;
            }
        }
    }

end:
    return (cl_result == CL_SUCCESS);
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_kdtree_intersect_rays_with_data_upload(__in                  __notnull   ocl_kdtree                         instance,
                                                                   __in                              ocl_kdtree_executor_id             executor_id,
                                                                   __in                              uint32_t                           n_rays,
                                                                   __in                              uint32_t                           n_ray_origins,
                                                                   __in_ecount(3)        __notnull   const float*                       ray_origin,
                                                                   __in                  __notnull   cl_mem                             ray_origins_mo,
                                                                   __in                              uint32_t                           ray_origins_mo_vec4_offset,
                                                                   __in_ecount(n_rays*3) __notnull   const float*                       ray_directions, /* normalized */
                                                                   __in                              uint32_t                           ray_directions_mo_offset,
                                                                   __in                              cl_mem                             target_mo,
                                                                   __in                  __maybenull cl_mem                             target_triangle_hit_data_mo,
                                                                   __in                              bool                               should_store_triangle_hit_data,
                                                                   __in                              bool                               should_find_closest_intersection,
                                                                   __in                  __maybenull PFNKDTREEINTERSECTRAYSCALLBACKPROC on_next_ray_origin_callback_proc,
                                                                   __in                  __maybenull void*                              on_next_ray_origin_callback_user_arg)
{
    cl_int                    error_code     = 0;
    const ocl_11_entrypoints* entry_points   = ocl_get_entrypoints();
    _ocl_kdtree*              kdtree_ptr     = (_ocl_kdtree*) instance;
    uint32_t                  n_bytes_needed = sizeof(unsigned char) * n_rays * n_ray_origins;
    bool                      result         = false;

    /* We don't want this function to do anything except preparing the vram-based data buffer. The gritty details
     * will be taken care of by ocl_kdtree_intersect_rays() */
    if (kdtree_ptr->cl_raycast_helper_bo != 0 && kdtree_ptr->cl_raycast_helper_bo_size < n_bytes_needed)
    {
        /* The BO is too small! This is the worst case - we need to release the BO and create a new one */
        error_code = entry_points->pCLReleaseMemObject(kdtree_ptr->cl_raycast_helper_bo);

        ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Raycast helper BO could not have been released!");

        kdtree_ptr->cl_raycast_helper_bo      = 0;
        kdtree_ptr->cl_raycast_helper_bo_size = 0;
    }

    if (kdtree_ptr->cl_raycast_helper_bo == 0)
    {
        /* No BO available - create one */
        kdtree_ptr->cl_raycast_helper_bo = entry_points->pCLCreateBuffer(ocl_context_get_context(kdtree_ptr->context),
                                                                         CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                                                         n_bytes_needed,
                                                                         (void*) ray_directions,
                                                                         &error_code);

        ASSERT_ALWAYS_SYNC(kdtree_ptr->cl_raycast_helper_bo == NULL || error_code != CL_SUCCESS,
                           "Could not create ray-cast helper BO [error code:%d]",
                           error_code);
        if (kdtree_ptr->cl_raycast_helper_bo == NULL || error_code != CL_SUCCESS)
        {
            goto end;
        }

        /* Update BO size */
        kdtree_ptr->cl_raycast_helper_bo_size = sizeof(float) * 3 * n_rays;
        result                                = true;
    }
    else
    {
        /* BO already exists. The only thing we need to do is upload the new data */
        error_code = entry_points->pCLEnqueueWriteBuffer(ocl_context_get_command_queue(kdtree_ptr->context, 0),
                                                         kdtree_ptr->cl_raycast_helper_bo,
                                                         CL_FALSE, /* we don't want the implementation to copy the data! */
                                                         0,       /* start offset */
                                                         n_bytes_needed,
                                                         ray_directions,
                                                         NULL,    /* no events */
                                                         0,       /* no events */
                                                         NULL);   /* don't return an event we could wait on - this request will have been fulfilled before intersection kernel executes */

        ASSERT_DEBUG_SYNC(error_code == CL_SUCCESS, "Could not enqueue 'write buffer' operation");
        result = (error_code == CL_SUCCESS);        
    }

    if (result)
    {
        /* Okay, the memory object is ready. Let's pass the control to actual handler */
        result = ocl_kdtree_intersect_rays(instance,
                                           executor_id,
                                           n_rays,
                                           n_ray_origins,
                                           ray_origin,
                                           ray_origins_mo,
                                           ray_origins_mo_vec4_offset,
                                           kdtree_ptr->cl_raycast_helper_bo,
                                           ray_directions_mo_offset,
                                           target_mo,
                                           target_triangle_hit_data_mo,
                                           should_store_triangle_hit_data,
                                           should_find_closest_intersection,
                                           on_next_ray_origin_callback_proc,
                                           on_next_ray_origin_callback_user_arg);
    }

end: return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API ocl_kdtree ocl_kdtree_load(__in __notnull ocl_context               context,
                                              __in __notnull system_hashed_ansi_string full_file_path,
                                              __in           _ocl_kdtree_usage         usage)
{
    _ocl_kdtree*           result     = new (std::nothrow) _ocl_kdtree;
    system_file_serializer serializer = system_file_serializer_create_for_reading(full_file_path);

    ASSERT_ALWAYS_SYNC(result != NULL, "Out of memory");
    if (result != NULL)
    {
        uint32_t read_max_triangles_per_leaf     = 0;
        float    read_min_leaf_volume_multiplier = 0;

        ASSERT_DEBUG_SYNC(sizeof(read_max_triangles_per_leaf)     == sizeof(result->max_triangles_per_leaf),     "Debug for explanation");
        ASSERT_DEBUG_SYNC(sizeof(read_min_leaf_volume_multiplier) == sizeof(result->min_leaf_volume_multiplier), "Debug for explanation");

        memset(result, 0, sizeof(_ocl_kdtree) );

        result->usage = usage;

        /* Read fields we need to initialize Kdtree */
        if (!system_file_serializer_read(serializer, sizeof(result->min_leaf_volume_multiplier), &read_min_leaf_volume_multiplier) ||
            !system_file_serializer_read(serializer, sizeof(result->max_triangles_per_leaf),     &read_max_triangles_per_leaf) )
        {
            LOG_FATAL("Cannot read Kd tree data (1)");

            goto end_with_error;
        }

        /* initialize the result instance with default values */
        _ocl_kdtree_init(result, context, 0, NULL, read_min_leaf_volume_multiplier, read_max_triangles_per_leaf, usage);

        /* Initialize Kd-tree data CL buffer (process space only) */
        if (!system_file_serializer_read(serializer, sizeof(result->kdtree_data_cl_buffer_size), &result->kdtree_data_cl_buffer_size) )
        {
            LOG_FATAL("Cannot read Kd tree data (2)");

            goto end_with_error;
        }

        result->kdtree_data_cl_buffer = new (std::nothrow) char[result->kdtree_data_cl_buffer_size];

        if (result->kdtree_data_cl_buffer == NULL)
        {
            LOG_FATAL("Out of memory while allocating Kd tree data CL buffer");

            goto end_with_error;
        }

        if (!system_file_serializer_read(serializer, result->kdtree_data_cl_buffer_size, result->kdtree_data_cl_buffer) )
        {
            LOG_FATAL("Cannot read Kd tree data (3)");

            goto end_with_error;
        }

        /* Read other fields */
        if (!system_file_serializer_read(serializer, sizeof(result->clgl_offset_nodes_data),                &result->clgl_offset_nodes_data)                ||
            !system_file_serializer_read(serializer, sizeof(result->clgl_offset_triangle_ids),              &result->clgl_offset_triangle_ids)              ||
            !system_file_serializer_read(serializer, sizeof(result->clgl_offset_triangle_lists),            &result->clgl_offset_triangle_lists)            ||
            !system_file_serializer_read(serializer, sizeof(result->root_bb),                               &result->root_bb)                               ||
            !system_file_serializer_read(serializer, sizeof(result->meshes_cl_data_buffer_normals_offset),  &result->meshes_cl_data_buffer_normals_offset)  ||
            !system_file_serializer_read(serializer, sizeof(result->meshes_cl_data_buffer_vertices_offset), &result->meshes_cl_data_buffer_vertices_offset))
        {
            LOG_FATAL("Cannot read Kd tree data (4)");

            goto end_with_error;
        }

        /* Load raw unique vertex data, ordered for the kd tree */
        if (!system_file_serializer_read(serializer, sizeof(result->serialize_meshes_data_buffer_size), &result->serialize_meshes_data_buffer_size))
        {
            LOG_FATAL("Cannot read Kd tree data (5)");

            goto end_with_error;
        }

        result->serialize_meshes_data_buffer = new (std::nothrow) char[result->serialize_meshes_data_buffer_size];
        if (result->serialize_meshes_data_buffer == NULL)
        {
            LOG_FATAL("Out of memory");

            goto end_with_error;
        }

        if (!system_file_serializer_read(serializer, result->serialize_meshes_data_buffer_size, result->serialize_meshes_data_buffer))
        {
            LOG_FATAL("Cannot read Kd tree data (6)");

            goto end_with_error;
        }

        /* Init CL buffers */
        _ocl_kdtree_generate_kdtree_data_cl_buffer(result);

        /* Create CL buffer object from a GL buffer used by the mesh instance to hold vertex data */
        _ocl_kdtree_generate_cl_vertex_data_buffer(result);

        /* Register the instance */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result, 
                                                       _ocl_kdtree_release,
                                                       OBJECT_TYPE_OCL_KDTREE, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenCL Kd Trees\\", system_hashed_ansi_string_get_buffer(full_file_path) )
                                                       );

        /* That's all */
        system_file_serializer_release(serializer);
    }

    return (ocl_kdtree) result;

end_with_error:
    if (result != NULL)
    {
        if (result->kdtree_data_cl_buffer != NULL)
        {
            delete [] (char*) result->kdtree_data_cl_buffer;

            result->kdtree_data_cl_buffer = NULL;
        }

        delete result;
        result = NULL;
    }

    return (ocl_kdtree) result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool ocl_kdtree_save(__in __notnull ocl_kdtree instance, __in __notnull system_hashed_ansi_string full_file_path)
{
    _ocl_kdtree*           data_ptr   = (_ocl_kdtree*) instance;
    bool                   result     = false;
    system_file_serializer serializer = NULL;

    ASSERT_DEBUG_SYNC(data_ptr->kdtree_data_cl_buffer                  != NULL &&
                      data_ptr->kdtree_data_cl_buffer_size             != 0    &&
                      (data_ptr->creation_flags & KDTREE_SAVE_SUPPORT) != 0,
                      "Save is either not enabled for Kd tree instance or the buffer has not been initialized correctly");
    if (data_ptr->kdtree_data_cl_buffer                  == NULL ||
        data_ptr->kdtree_data_cl_buffer_size             == 0    ||
        (data_ptr->creation_flags & KDTREE_SAVE_SUPPORT) == 0)
    {
        goto end;
    }

    /* Instantiate the serializer */
    serializer = system_file_serializer_create_for_writing(full_file_path);
    
    ASSERT_DEBUG_SYNC(serializer != NULL, "Could not instantiate file serializer");
    if (serializer == NULL)
    {
        goto end;
    }

    /* Write the contents */
    if (!system_file_serializer_write(serializer, sizeof(data_ptr->min_leaf_volume_multiplier),            &data_ptr->min_leaf_volume_multiplier)            ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->max_triangles_per_leaf),                &data_ptr->max_triangles_per_leaf)                ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->kdtree_data_cl_buffer_size),            &data_ptr->kdtree_data_cl_buffer_size)            ||
        !system_file_serializer_write(serializer, data_ptr->kdtree_data_cl_buffer_size,                     data_ptr->kdtree_data_cl_buffer)                 ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->clgl_offset_nodes_data),                &data_ptr->clgl_offset_nodes_data)                ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->clgl_offset_triangle_ids),              &data_ptr->clgl_offset_triangle_ids)              ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->clgl_offset_triangle_lists),            &data_ptr->clgl_offset_triangle_lists)            ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->root_bb),                               &data_ptr->root_bb)                               ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->meshes_cl_data_buffer_normals_offset),  &data_ptr->meshes_cl_data_buffer_normals_offset)  ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->meshes_cl_data_buffer_vertices_offset), &data_ptr->meshes_cl_data_buffer_vertices_offset) ||
        !system_file_serializer_write(serializer, sizeof(data_ptr->serialize_meshes_data_buffer_size),     &data_ptr->serialize_meshes_data_buffer_size)     ||
        !system_file_serializer_write(serializer, data_ptr->serialize_meshes_data_buffer_size,              data_ptr->serialize_meshes_data_buffer))
        
    {
        ASSERT_DEBUG_SYNC(false, "Could not save kdtree data.");

        goto end;
    }

    /* Close the serializer */
    system_file_serializer_release(serializer);

    result = true;
end:
    return result;
}
