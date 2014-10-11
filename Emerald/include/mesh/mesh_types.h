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
    MESH_PROPERTY_AABB_MAX,                                      /*     settable,  float[4]                */
    MESH_PROPERTY_AABB_MIN,                                      /*     settable,  float[4]                */
    MESH_PROPERTY_CREATION_FLAGS,                                /* not settable,  mesh_creation_flags     */
    MESH_PROPERTY_GL_BO_ID,                                      /* not settable,  GLuint                  */
    MESH_PROPERTY_GL_INDEX_TYPE,                                 /* not settable,  _mesh_index_type        */
    MESH_PROPERTY_GL_PROCESSED_DATA,                             /* not settable,  void*                   */
    MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,                        /* not settable,  uint32_t                */
    MESH_PROPERTY_GL_STRIDE,                                     /*     settable,  uint32_t                */
    MESH_PROPERTY_GL_TBO,                                        /* not settable,  ogl_texture             */
    MESH_PROPERTY_GL_TOTAL_ELEMENTS,                             /*     settable,  uint32_t                */
    MESH_PROPERTY_MATERIALS,                                     /* not settable,  system_resizable_vector - DO NOT MODIFY OR RELEASE */
    MESH_PROPERTY_NAME,                                          /* not settable,  system_hashed_ansi_string */
    MESH_PROPERTY_N_GL_UNIQUE_VERTICES,                          /* not settable,  uint32_t                */
    MESH_PROPERTY_N_LAYERS,                                      /* not settable,  uint32_t                */
    MESH_PROPERTY_N_MESHES_MERGED,                               /* not settable,  uint32_t                */
    MESH_PROPERTY_N_SH_BANDS,                                    /* settable ONCE, uint32_t                */
    MESH_PROPERTY_TIMESTAMP_MODIFICATION,                        /* not settable,  system_timeline_time    */
} mesh_property;

typedef enum
{
    MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET, /* not settable, uint32_t */
} mesh_layer_data_stream_property;

typedef enum
{
    MESH_LAYER_PROPERTY_AABB_MAX,                   /*     settable, float*        (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_AABB_MIN,                   /*     settable, float*        (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,      /*     settable, uint32_t                                                        */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX, /*     settable, uint32_t                                                        */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX, /*     settable, uint32_t                                                        */
    MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA,           /* not settable, uint32_t*                                                       */
    MESH_LAYER_PROPERTY_GL_N_UNIQUE_ELEMENTS,       /*     settable, uint32_t      (this property has the same value for all passes) */
    MESH_LAYER_PROPERTY_MATERIAL,                   /* not settable, mesh_material                                                   */
    MESH_LAYER_PROPERTY_N_ELEMENTS,                 /* not settable, uint32_t                                                        */
    MESH_LAYER_PROPERTY_N_TRIANGLES,                /* not settable, uint32_t                                                        */
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

    MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS, /* note: recommended to keep sh at the end */
    MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS, /* note: recommended to keep sh at the end */

    /* Always last */
    MESH_LAYER_DATA_STREAM_TYPE_COUNT,
    MESH_LAYER_DATA_STREAM_TYPE_UNKNOWN = MESH_LAYER_DATA_STREAM_TYPE_COUNT
} mesh_layer_data_stream_type;

/* Mesh creation flags */
typedef int mesh_creation_flags;

const int MESH_SAVE_SUPPORT              = 0x1;
const int MESH_KDTREE_GENERATION_SUPPORT = 0x2;
const int MESH_MERGE_SUPPORT             = 0x4;


typedef uint32_t mesh_layer_id;
typedef uint32_t mesh_layer_pass_id;

#endif /* MESH_TYPES_H */
