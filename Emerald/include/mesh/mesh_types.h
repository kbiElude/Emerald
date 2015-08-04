/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef MESH_TYPES_H
#define MESH_TYPES_H

DECLARE_HANDLE(mesh);
DECLARE_HANDLE(mesh_material);

typedef enum
{
    /* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MAX,

    /* settable, float[4] */
    MESH_PROPERTY_MODEL_AABB_MIN,

    /* not settable, mesh_creation_flags */
    MESH_PROPERTY_CREATION_FLAGS,

    /* not settable, GLuint */
    MESH_PROPERTY_GL_BO_ID,

    /* not settable, unsigned int */
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
    MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET, /* not settable, uint32_t */
} mesh_layer_data_stream_property;

typedef enum
{
    /* settable, float* (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_MODEL_AABB_MAX,

    /* settable, float* (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_MODEL_AABB_MIN,

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
    /** NOTE: Re-arranging the order oir adding new stream types invalidates COLLADA mesh blobs. */
    MESH_LAYER_DATA_STREAM_TYPE_FIRST,

    MESH_LAYER_DATA_STREAM_TYPE_VERTICES = MESH_LAYER_DATA_STREAM_TYPE_FIRST, /* note: always keep vertices first */
    MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
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
    MESH_TYPE_REGULAR,
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

#endif /* MESH_TYPES_H */
