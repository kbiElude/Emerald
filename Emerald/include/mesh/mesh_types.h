/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef MESH_TYPES_H
#define MESH_TYPES_H

#include "ral/ral_types.h"


DECLARE_HANDLE(mesh);
DECLARE_HANDLE(mesh_material);

typedef struct mesh_draw_call_arguments
{
    /* ID of the BO which should be bound to the GL_DRAW_INDIRECT_BUFFER binding point before
     * the indirect draw call is dispatched.
     *
     * Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call type. */
    ral_buffer draw_indirect_bo;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS draw call type. */
    uint32_t n_base_vertex;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call type. */
    uint32_t indirect_offset;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS draw call type. */
    uint32_t n_vertices;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS and MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call types. */
    ral_primitive_type primitive_type;

    mesh_draw_call_arguments()
    {
        draw_indirect_bo = nullptr;
        indirect_offset  = 0;
        n_base_vertex    = 0;
        n_vertices       = 0;
        primitive_type   = RAL_PRIMITIVE_TYPE_UNKNOWN; /* ex mode */
    }
} mesh_draw_call_arguments;

typedef enum
{
    /* TODO: Add the other draw call types when needed. */
    MESH_DRAW_CALL_TYPE_NONINDEXED,
    MESH_DRAW_CALL_TYPE_NONINDEXED_INDIRECT,

    /* Always last */
    MESH_DRAW_CALL_TYPE_UNKNOWN
} mesh_draw_call_type;

typedef enum
{
    /* not settable, _mesh_index_type */
    MESH_PROPERTY_BO_INDEX_TYPE,

    /* not settable, void* */
    MESH_PROPERTY_BO_PROCESSED_DATA,

    /* not settable, uint32_t */
    MESH_PROPERTY_BO_PROCESSED_DATA_SIZE,

    /* not settable, ral_buffer
     *
     * Only used for regular meshes
     */
    MESH_PROPERTY_BO_RAL,

    /* settable, uint32_t */
    MESH_PROPERTY_BO_STRIDE,

    /* settable, uint32_t */
    MESH_PROPERTY_BO_TOTAL_ELEMENTS,

/* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MAX,

    /* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MIN,

    /* not settable, mesh_creation_flags */
    MESH_PROPERTY_CREATION_FLAGS,

    /* not settable, system_resizable_vector - DO NOT MODIFY OR RELEASE */
    MESH_PROPERTY_MATERIALS,

    /* not settable, system_hashed_ansi_string */
    MESH_PROPERTY_NAME,

    /* not settable, uint32_t */
    MESH_PROPERTY_N_BO_UNIQUE_VERTICES,

    /* not settable, uint32_t */
    MESH_PROPERTY_N_LAYERS,

    /* settable ONCE, uint32_t */
    MESH_PROPERTY_N_SH_BANDS,

    /* not settable, PFNRENDERCUSTOMMESH
     *
     * Property only valid for custom meshes.
     */
    MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_PTR,

    /* not settable, void*
     *
     * Property only valid for custom meshes.
     */
    MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_USER_ARG,

    /* not settable, system_time    */
    MESH_PROPERTY_TIMESTAMP_MODIFICATION,

    /* not settable, mesh_type */
    MESH_PROPERTY_TYPE,

    /* settable, mesh_vertex_ordering.
     *
     * Default value: MESH_VERTEX_ORDERING_CCW */
    MESH_PROPERTY_VERTEX_ORDERING,

    /* not settable, mesh.
     *
     * This property stores a non-null mesh pointer, if the mesh was created as, or marked
     * as instantiated. The pointer points to a mesh that holds the GL & layer data which
     * are useful for the rendering process.
     *
     * If the value is NULL, the mesh is not instantiated.
     */
     MESH_PROPERTY_INSTANTIATION_PARENT
} mesh_property;

typedef enum
{
    /* settable, ral_buffer
     *
     * Only used by GPU stream meshes.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL,

    /* settable, unsigned int.
     *
     * Only used by GPU stream meshes.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_BUFFER_RAL_STRIDE,

    /* not settable, uint32_t */
    MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS,

    /* settable with mesh_set_layer_data_stream_property() or mesh_set_layer_data_stream_property_with_buffer_memory(), uint32_t.
     *
     * NOTE: This property is only assigned a value if MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE is
     *       set to MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY. If not, querying this property WILL result
     *       in an assertion failure.
     *       The described situation can only occur for GPU Stream meshes. Regular meshes never store the
     *       number of vertices/normals in buffer memory, as they are baked in advance.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS,

    /* indirectly settable, GLuint.
     *
     * To change the value, use mesh_set_layer_data_stream_property_with_buffer_memory() with
     * MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS pname.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL,

    /* indirectly settable, bool.
     *
     * To change the value, use mesh_set_layer_data_stream_property_with_buffer_memory() with
     * MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS pname.
     */
     MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER,

    /* not settable, mesh_layer_data_stream_source.
     *
     * Please read MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS documentation for more details. */
    MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE,

    /* not settable, uint32_t */
    MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
} mesh_layer_data_stream_property;

typedef enum
{
    MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY,
    MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY,

    MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED
} mesh_layer_data_stream_source;

typedef enum
{
    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_BO_ELEMENTS_OFFSET,

    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_BO_ELEMENTS_DATA_MAX_INDEX,

    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_BO_ELEMENTS_DATA_MIN_INDEX,

    /* not settable, uint32_t* */
    MESH_LAYER_PROPERTY_BO_ELEMENTS_DATA,

    /* settable, uint32_t (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_BO_N_UNIQUE_ELEMENTS,

    /* settable, mesh_draw_call_arguments
     *
     * Only meaningful for GPU stream meshes.
     */
    MESH_LAYER_PROPERTY_DRAW_CALL_ARGUMENTS,

    /* settable, mesh_draw_call_type
     *
     * Only meaningful for GPU stream meshes.
     */
    MESH_LAYER_PROPERTY_DRAW_CALL_TYPE,

    /* not settable, mesh_material */
    MESH_LAYER_PROPERTY_MATERIAL,

    /* settable, float* (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_MODEL_AABB_MAX,

    /* settable, float* (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_MODEL_AABB_MIN,

    /* not settable, uint32_t */
    MESH_LAYER_PROPERTY_N_ELEMENTS,

    /* not settable, uint32_t */
    MESH_LAYER_PROPERTY_N_TRIANGLES,

    /*  settable, float. setter does not invoke GL blob re-generation! */
    MESH_LAYER_PROPERTY_VERTEX_SMOOTHING_ANGLE,
} mesh_layer_property;

typedef enum
{
    MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT,

    MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN
} mesh_layer_data_stream_data_type;

typedef enum
{
    /** NOTE: Re-arranging the order oir adding new stream types invalidates COLLADA mesh blobs.
     *
     * _VERTICES must be the first defined enum.
     */
    MESH_LAYER_DATA_STREAM_TYPE_FIRST,

    /* Vertex data. User-specified number of floats per vertex, no padding in-between. */
    MESH_LAYER_DATA_STREAM_TYPE_VERTICES = MESH_LAYER_DATA_STREAM_TYPE_FIRST,

    /* Normal data. User-specified number of floats per vertex, no padding in-between */
    MESH_LAYER_DATA_STREAM_TYPE_NORMALS,

    /* Texture coordinate data. User-specified number of floats per vertex, no padding in-between */
    MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS,

    /* note: recommended to keep sh at the end */
    MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS,
    MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS,

    /* Always last */
    MESH_LAYER_DATA_STREAM_TYPE_COUNT,
    MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN = MESH_LAYER_DATA_STREAM_TYPE_COUNT
} mesh_layer_data_stream_type;

typedef enum
{
    MESH_TYPE_CUSTOM,
    MESH_TYPE_GPU_STREAM,
    MESH_TYPE_REGULAR,

    /* Always last */
    MESH_TYPE_UNKNOWN
} mesh_type;

typedef enum
{
    MESH_VERTEX_ORDERING_CW,
    MESH_VERTEX_ORDERING_CCW,

} mesh_vertex_ordering;


/* Mesh creation flags */
typedef int mesh_creation_flags;

/* Keeps mesh layer & layer pass data after GL buffers are created.
 * This allows to change eg. vertex smoothing angles for baked objects.
 */
const int MESH_CREATION_FLAGS_SAVE_SUPPORT              = 0x1;
const int MESH_CREATION_FLAGS_KDTREE_GENERATION_SUPPORT = 0x2;
const int MESH_CREATION_FLAGS_LOAD_ASYNC                = 0x4;

typedef uint32_t mesh_layer_id;
typedef uint32_t mesh_layer_pass_id;

DECLARE_HANDLE(mesh_marchingcubes);


#endif /* MESH_TYPES_H */
