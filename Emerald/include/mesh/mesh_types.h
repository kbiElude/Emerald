/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef MESH_TYPES_H
#define MESH_TYPES_H

DECLARE_HANDLE(mesh);
DECLARE_HANDLE(mesh_material);

typedef void (*PFNPREDRAWCALLCALLBACKPROC)(ogl_context context,
                                           mesh        mesh_instance,
                                           void*       user_arg);

typedef struct mesh_draw_call_arguments
{
    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS draw call type. */
    GLint count;

    /* ID of the BO which should be bound to the GL_DRAW_INDIRECT_BUFFER binding point before
     * the indirect draw call is dispatched.
     *
     * Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call type. */
    GLuint draw_indirect_bo_binding;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS draw call type. */
    GLint first;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call type. */
    const void* indirect;

    /* Must be filled for the MESH_DRAW_CALL_TYPE_ARRAYS and MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT draw call types. */
    GLenum mode;

    /* Optional. Will be called back from the rendering thread, right before the draw call is made. */
    PFNPREDRAWCALLCALLBACKPROC pfn_pre_draw_call_callback;

    /* Optional. Should be set to bits accepted by glMemoryBarrier(), or 0 if no synchronization is needed. */
    GLenum pre_draw_call_barriers;

    /* Optional. Argument passed to the pre-draw call callback. */
    void* pre_draw_call_callback_user_arg;

    mesh_draw_call_arguments()
    {
        count                           = 0;
        draw_indirect_bo_binding        = 0;
        first                           = 0;
        indirect                        = NULL;
        mode                            = 0xFFFFFFFF;
        pfn_pre_draw_call_callback      = NULL;
        pre_draw_call_barriers          = 0;
        pre_draw_call_callback_user_arg = NULL;
    }
} mesh_draw_call_arguments;

typedef enum
{
    /* TODO: Add the other draw call types when needed. */
    MESH_DRAW_CALL_TYPE_ARRAYS,
    MESH_DRAW_CALL_TYPE_ARRAYS_INDIRECT,

    /* Always last */
    MESH_DRAW_CALL_TYPE_UNKNOWN
} mesh_draw_call_type;

typedef enum
{
    /* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MAX,

    /* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MIN,

    /* not settable, mesh_creation_flags */
    MESH_PROPERTY_CREATION_FLAGS,

    /* not settable, GLuint .
     *
     * Only used for regular meshes
     */
    MESH_PROPERTY_GL_BO_ID,

    /* not settable, unsigned int
     *
     * Only used for regular meshes.
     */
    MESH_PROPERTY_GL_BO_START_OFFSET,

    /* not settable, _mesh_index_type */
    MESH_PROPERTY_GL_INDEX_TYPE,

    /* not settable, void* */
    MESH_PROPERTY_GL_PROCESSED_DATA,

    /* not settable, uint32_t */
    MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,

    /* settable, uint32_t */
    MESH_PROPERTY_GL_STRIDE,

    /* not settable, ogl_texture */
    MESH_PROPERTY_GL_TBO,

    /* settable, uint32_t */
    MESH_PROPERTY_GL_TOTAL_ELEMENTS,

    /* not settable, system_resizable_vector - DO NOT MODIFY OR RELEASE */
    MESH_PROPERTY_MATERIALS,

    /* not settable, system_hashed_ansi_string */
    MESH_PROPERTY_NAME,

    /* not settable, uint32_t */
    MESH_PROPERTY_N_GL_UNIQUE_VERTICES,

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

    /* not settable, bool.
     *
     * This property tells that a rendering thread should call mesh_fill_gl_buffers() ASAP
     * from the rendering thread. Raised, if CPU-side GL blob was updated and the rendering
     * thread was unavailable at the modification time.
     */
     MESH_PROPERTY_GL_THREAD_FILL_BUFFERS_CALL_NEEDED,

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
    /* not settable, unsigned int.
     *
     * Only used by GPU stream meshes.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_ID,

    /* not settable, unsigned int.
     *
     * Only used by GPU stream meshes.
     */
    MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE,

    /* not settable, uint32_t */
    MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET,
} mesh_layer_data_stream_property;

typedef enum
{
    MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY,
    MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY,
} mesh_layer_data_stream_source;

typedef enum
{
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

    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,

    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX,

    /* settable, uint32_t */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX,

    /* not settable, uint32_t* */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA,

    /* settable, uint32_t (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_GL_N_UNIQUE_ELEMENTS,

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

    /* Vertex data. 3-component floats per vertex, no padding in-between */
    MESH_LAYER_DATA_STREAM_TYPE_VERTICES = MESH_LAYER_DATA_STREAM_TYPE_FIRST,

    /* Normal data. 3-component floats per vertex, no padding in-between */
    MESH_LAYER_DATA_STREAM_TYPE_NORMALS,

    /* Texture coordinate data. 2-component floats per vertex, no padding in-between */
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

/* Mesh creation flags */
typedef int mesh_creation_flags;

/* Keeps mesh layer & layer pass data after GL buffers are created.
 * This allows to change eg. vertex smoothing angles for baked objects.
 */
const int MESH_CREATION_FLAGS_SAVE_SUPPORT              = 0x1;
const int MESH_CREATION_FLAGS_KDTREE_GENERATION_SUPPORT = 0x2;
//const int MESH_CREATION_FLAGS_MERGE_SUPPORT             = 0x4; <- support for merging was removed in Nov 2014.


typedef uint32_t mesh_layer_id;
typedef uint32_t mesh_layer_pass_id;

DECLARE_HANDLE(mesh_marchingcubes);


#endif /* MESH_TYPES_H */
