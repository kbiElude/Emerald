
/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_texture.h"
#include "sh/sh_types.h"
#include "system/system_bst.h"
#include "system/system_callback_manager.h"
#include "system/system_file_serializer.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

#define START_LAYERS (4)


/* Magic combination, prefixing mesh data */
const char* header_magic = "eld";


/* Private declarations */

typedef struct
{
    system_hashed_ansi_string name;
    uint32_t                  n_gl_unique_vertices;

    /* GL storage.
     *
     * NOTE for offsets: these refer to gl_processed_data, not used if -1.
     *                   Each offset stands for the first entry. Subsequent entries are
     *                   separated with a mesh-specific stride.
     *                   SH MUST be aligned to 32 (as it's used by means of a RGBA32F texture buffer.
     */
    GLuint gl_bo_id;

    /* refers to TBO created for a BO containing gl_processed data. Uses GL_RGBA32F type for 4 SH bands, GL_RGB32F for 3 SH bands*/
    ogl_texture      gl_tbo;

    float*           gl_processed_data;
    uint32_t         gl_processed_data_size; /* tells size of gl_processed_data */
    uint32_t         gl_processed_data_stream_start_offset[MESH_LAYER_DATA_STREAM_TYPE_COUNT];
    uint32_t         gl_processed_data_stride;
    uint32_t         gl_processed_data_total_elements;
    ogl_context      gl_context;
    _mesh_index_type gl_index_type;
    bool             gl_storage_initialized;
    bool             gl_thread_fill_gl_buffers_call_needed;
    mesh             instantiation_parent; /* If not NULL, instantiation parent's GL data should be used instead */

    /* Properties */
    float                aabb_max   [4]; /* model-space */
    float                aabb_min   [4]; /* model-space */
    mesh_creation_flags  creation_flags;
    uint32_t             n_sh_bands;      /* can be 0 ! */
    sh_components        n_sh_components; /* can be 0 ! */
    system_timeline_time timestamp_last_modified;

    /* Other */
    system_resizable_vector layers;    /* contains _mesh_layer instances */
    system_resizable_vector materials; /* cache of all materials used by the mesh. queried by ogl_scene_renderer */
    unsigned int            set_id_counter;

    REFCOUNT_INSERT_VARIABLES
} _mesh;

typedef struct
{
    system_hash64map data_streams;   /* contains _mesh_layer_data_stream* elements, indexed by _mesh_layer_data_stream_type */
    uint32_t         passes_counter; /* used for id assignment */

    float            aabb_max[4];
    float            aabb_min[4];

    uint32_t n_gl_unique_elements;

    system_resizable_vector passes; /* contains _mesh_layer_pass* elements */
} _mesh_layer;

typedef struct _mesh_layer_data_stream
{
    void*                            data;
    mesh_layer_data_stream_data_type data_type;
    unsigned int                     n_components;          /* number of components stored in data per each entry */
    unsigned int                     n_items;               /* number of (potentially multi-component!) entries in data */
    unsigned int                     required_bit_alignment;

    ~_mesh_layer_data_stream()
    {
        delete [] data;

        data = NULL;
    }
} _mesh_layer_data_stream;

typedef struct _mesh_layer_pass_index_data
{
    uint32_t* data;
    uint32_t  max_index;
    uint32_t  min_index;

    _mesh_layer_pass_index_data()
    {
        data      = NULL;
        max_index = -1;
        min_index = -1;
    }

    ~_mesh_layer_pass_index_data()
    {
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }
    }
} _mesh_layer_pass_index_data;

typedef struct
{
    mesh_material material;
    uint32_t      n_elements;
    float         smoothing_angle; /* As was used for generating the normals data. */

    /** Some stream types can be assigned multiple sets in COLLADA.
     *  The hash-map maps stream set index to _mesh_layer_pass_index_data*,
     *  which stores a set of indices (each set storing n_elements entries)
     *  of type uint32_t*, as well as minimum/maximum index used in that
     *  store.
     *
     *  Map uses set index as keys.
     */
    system_hash64map index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_COUNT];

    /** Maps stream set index to an unique set id. The uniqueness is guaranteed
     *  across all passes defined by the owner mesh.
     *  This is mostly used during key creation for BST configuration.
     *
     *  TODO: THIS IMPLIES ONLY ONE SET IS EVER USED. FIX IF NEEDED.
     */
    system_hash64map set_id_to_unique_set_id[MESH_LAYER_DATA_STREAM_TYPE_COUNT];

    uint32_t  gl_bo_elements_offset; /* refers to gl_processed_data */
    uint32_t* gl_elements;
    uint32_t  gl_elements_max_index;
    uint32_t  gl_elements_min_index;
} _mesh_layer_pass;

/** TODO: Used for normal data generation */
typedef struct _mesh_polygon
{
    unsigned int n_mesh_layer;
    unsigned int n_mesh_layer_pass;
    unsigned int triangle_index;

    _mesh_layer*      layer_ptr;
    _mesh_layer_pass* layer_pass_ptr;
    float             polygon_normal[3];
    float*            vertex_data   [3];
} _mesh_polygon;

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh, mesh, _mesh);

/** Forward declarations */
PRIVATE void _mesh_deinit_mesh_layer     (__in __notnull const _mesh* mesh_ptr, __in __notnull _mesh_layer*      layer_ptr, bool do_full_deinit);
PRIVATE void _mesh_deinit_mesh_layer_pass(__in __notnull const _mesh* mesh_ptr, __in __notnull _mesh_layer_pass* pass_ptr,  bool do_full_deinit);
PRIVATE bool _mesh_is_key_uint32_equal   (                     void*  arg1,                    void*             arg2);
PRIVATE bool _mesh_is_key_uint32_lower   (                     void*  arg1,                    void*             arg2);
PRIVATE void _mesh_release_normals_data  (__in __notnull       _mesh* mesh_ptr);

#ifdef _DEBUG
    PRIVATE void _mesh_verify_context_type(__in __notnull ogl_context);
#else
    #define _mesh_verify_context_type(x)
#endif


/** TODO */
PRIVATE void _mesh_get_index_key(__out          uint32_t*               out_result,
                                 __in __notnull const _mesh_layer_pass* layer_pass_ptr,
                                 __in           unsigned int            n_element)
{
    unsigned int n_current_word = 0;

    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                    ++n_data_stream_type)
    {
        if (layer_pass_ptr->index_data_maps[n_data_stream_type] != NULL)
        {
            _mesh_layer_pass_index_data* index_data_ptr = NULL;
            const uint32_t               n_sets         = system_hash64map_get_amount_of_elements(layer_pass_ptr->index_data_maps[n_data_stream_type]);

            for (uint32_t n_set = 0;
                          n_set < n_sets;
                        ++n_set)
            {
                uint32_t unique_set_id = 0;

                ASSERT_DEBUG_SYNC(n_element < layer_pass_ptr->n_elements,
                                  "Invalid element index requested");

                if (system_hash64map_get(layer_pass_ptr->index_data_maps        [n_data_stream_type], n_set, &index_data_ptr) &&
                    system_hash64map_get(layer_pass_ptr->set_id_to_unique_set_id[n_data_stream_type], n_set, &unique_set_id) )
                {
                    out_result[n_current_word] = unique_set_id;
                    n_current_word ++;

                    ASSERT_DEBUG_SYNC(index_data_ptr->data[n_element] != 0xcdcdcdcd,
                                      "Invalid index data");

                    out_result[n_current_word] = index_data_ptr->data[n_element];
                    n_current_word ++;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve index data.");

                    out_result[n_current_word] = 0;
                    n_current_word++;

                    out_result[n_current_word] = 0;
                    n_current_word++;
                }
            } /* for (all sets) */
        } /* if (index data available for given data stream type) */
    } /* for (all data stream types) */

    ASSERT_DEBUG_SYNC(sizeof(void*) == sizeof(uint32_t),
                      "32-bit only logic");

    memcpy(out_result + n_current_word,
           layer_pass_ptr->material,
           sizeof(layer_pass_ptr->material) );
}

/** TODO */
PRIVATE uint32_t _mesh_get_source_index_from_index_key(__in __notnull const uint32_t*             key_ptr,
                                                       __in __notnull const _mesh_layer_pass*     layer_pass_ptr,
                                                       __in           mesh_layer_data_stream_type data_stream_type,
                                                       __in           unsigned int                set_id)
{
    unsigned int n_current_word = 0;
    uint32_t     result         = -1;

    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                    ++n_data_stream_type)
    {
        if (layer_pass_ptr->index_data_maps[n_data_stream_type] != NULL)
        {
            const uint32_t n_sets = system_hash64map_get_amount_of_elements(layer_pass_ptr->index_data_maps[n_data_stream_type]);

            for (uint32_t n_set = 0; n_set < n_sets; ++n_set)
            {
                if (n_set == set_id && n_data_stream_type == data_stream_type)
                {
                    result = key_ptr[2 * n_current_word + 1];

                    ASSERT_DEBUG_SYNC(result != 0xcdcdcdcd,
                                      "Whoops?");

                    goto end;
                }

                n_current_word++;
            }
        }
    }

end:
    ASSERT_DEBUG_SYNC(result != -1, "Invalid request");

    return result;
}

/** TODO */
PRIVATE uint32_t _mesh_get_total_number_of_sets(_mesh_layer* layer_ptr)
{
    const uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);
          uint32_t result   = 0;

    for (uint32_t n_pass = 0;
                  n_pass < n_passes;
                ++n_pass)
    {
        const _mesh_layer_pass* pass_ptr = NULL;

        if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                   n_pass,
                                                  &pass_ptr) )
        {
            /* Count how many words BST keys will consist of for this pass */
            for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                              n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                            ++n_data_stream_type)
            {
                if (pass_ptr->index_data_maps[n_data_stream_type] != NULL)
                {
                    const uint32_t n_sets = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[n_data_stream_type]);

                    result += n_sets;
                }
            } /* for (all data streams) */
        }
    } /* for (all layer passes) */

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_float_lower(size_t key_size, void* arg1, void* arg2)
{
    float*        arg1_float = (float*) arg1;
    system_hash64 arg1_hash  = system_hash64_calculate((const char*) arg1, key_size);
    float*        arg2_float = (float*) arg2;
    system_hash64 arg2_hash  = system_hash64_calculate((const char*) arg2, key_size);
    bool          result     = true;

    if (arg1_hash != arg2_hash)
    {
        result = (arg1_hash < arg2_hash);
    }
    else
    {
        /* Take the slow route */
        for (unsigned int n_key_item = 0;
                          n_key_item < key_size / sizeof(float);
                        ++n_key_item)
        {
            if (arg1_float[n_key_item] >= arg2_float[n_key_item])
            {
                result = false;

                break;
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_float_equal(size_t key_size, void* arg1, void* arg2)
{
    float* arg1_float = (float*) arg1;
    float* arg2_float = (float*) arg2;
    bool   result     = true;

    for (unsigned int n_key_item = 0;
                      n_key_item < key_size / sizeof(float);
                    ++n_key_item)
    {
        if (fabs(arg1_float[n_key_item] - arg2_float[n_key_item]) > 1e-5f)
        {
            result = false;

            break;
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_uint32_lower(size_t key_size, void* arg1, void* arg2)
{
    uint32_t*     arg1_uint = (uint32_t*) arg1;
    system_hash64 arg1_hash = system_hash64_calculate( (const char*) arg1, key_size);
    uint32_t*     arg2_uint = (uint32_t*) arg2;
    system_hash64 arg2_hash = system_hash64_calculate( (const char*) arg2, key_size);
    bool          result    = true;

    if (arg1_hash != arg2_hash)
    {
        result = (arg1_hash < arg2_hash);
    }
    else
    {
        /* Take the slow route */
        for (unsigned int n_key_item = 0;
                          n_key_item < key_size / sizeof(uint32_t);
                        ++n_key_item)
        {
            if (!(arg1_uint[n_key_item] < arg2_uint[n_key_item]))
            {
                result = false;

                break;
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_uint32_equal(size_t key_size, void* arg1, void* arg2)
{
    uint32_t* arg1_uint = (uint32_t*) arg1;
    uint32_t* arg2_uint = (uint32_t*) arg2;
    bool      result    = true;

    for (unsigned int n_key_item = 0;
                      n_key_item < key_size / sizeof(uint32_t);
                    ++n_key_item)
    {
        if (!(arg1_uint[n_key_item] == arg2_uint[n_key_item]))
        {
            result = false;

            break;
        }
    }

    return result;
}

/** TODO */
PRIVATE void _mesh_deinit_mesh_layer(const _mesh* mesh_ptr,
                                     _mesh_layer* layer_ptr,
                                     bool         do_full_deinit)
{
    /* Release data streams. These should not be released at this point if KD tree
     * generation functionality is required
     */
    if (layer_ptr->data_streams != NULL)
    {
        if (!do_full_deinit && !(mesh_ptr->creation_flags & MESH_KDTREE_GENERATION_SUPPORT) ||
             do_full_deinit)
        {
            _mesh_layer_data_stream* data_stream_ptr  = NULL;
            system_hash64            temp;

            while (system_hash64map_get_element_at(layer_ptr->data_streams,
                                                   0,
                                                  &data_stream_ptr,
                                                  &temp) )
            {
                delete data_stream_ptr;
                data_stream_ptr = NULL;

                system_hash64map_remove(layer_ptr->data_streams,
                                        temp);
            }

            system_hash64map_release(layer_ptr->data_streams);

            layer_ptr->data_streams = NULL;
        }
    } /* if (layer_ptr->data_streams != NULL) */

    /* Do not remove pass data if not full deinitialization is needed.
     *
     * Pieces of the data that are transferred to VRAM could be released but for now let's
     * just let them be */
    if (do_full_deinit)
    {
        if (layer_ptr->passes != NULL)
        {
            while (system_resizable_vector_get_amount_of_elements(layer_ptr->passes) > 0)
            {
                _mesh_layer_pass* pass_ptr = NULL;

                if (system_resizable_vector_pop(layer_ptr->passes,
                                               &pass_ptr) )
                {
                    _mesh_deinit_mesh_layer_pass(mesh_ptr,
                                                 pass_ptr,
                                                 do_full_deinit);

                    delete pass_ptr;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not pop mesh layer pass");
                }
            }
        }
    }
}

/** TODO */
PRIVATE void _mesh_deinit_mesh_layer_pass(__in __notnull const _mesh*            mesh_ptr,
                                          __in __notnull       _mesh_layer_pass* pass_ptr,
                                                                bool             do_full_deinit)
{
    for (unsigned int n_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                      n_stream_type ++)
    {
        /* Vertex elements should not be deleted at this point if KD tree generation functionality is required. */
        if (n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES && !do_full_deinit && !(mesh_ptr->creation_flags & MESH_KDTREE_GENERATION_SUPPORT) ||
            n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES &&  do_full_deinit                                                                 ||
            n_stream_type != MESH_LAYER_DATA_STREAM_TYPE_VERTICES)
        {
            if (pass_ptr->index_data_maps[n_stream_type] != NULL)
            {
                _mesh_layer_pass_index_data* index_data = NULL;
                uint32_t                     n_sets     = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[n_stream_type]);

                for (uint32_t n_set = 0; n_set < n_sets; ++n_set)
                {
                    if (system_hash64map_get(pass_ptr->index_data_maps[n_stream_type], n_set, &index_data) )
                    {
                        delete index_data;

                        index_data = NULL;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve set data descriptor");
                    }
                } /* for (all sets) */

                system_hash64map_release(pass_ptr->index_data_maps[n_stream_type]);
            }
        }

        if (pass_ptr->set_id_to_unique_set_id[n_stream_type] != NULL)
        {
            system_hash64map_release(pass_ptr->set_id_to_unique_set_id[n_stream_type]);

            pass_ptr->set_id_to_unique_set_id[n_stream_type] = NULL;
        }
    } /* for (all stream types) */

    if (do_full_deinit && pass_ptr->gl_elements != NULL)
    {
        delete [] pass_ptr->gl_elements;

        pass_ptr->gl_elements = NULL;
    }

    if (pass_ptr->material != NULL)
    {
        mesh_material_release(pass_ptr->material);

        pass_ptr->material = NULL;
    }
}

/** TODO */
PRIVATE void _mesh_fill_gl_buffers_renderer_callback(ogl_context context,
                                                     void*        arg)
{
    _mesh*                                                    mesh_ptr         = (_mesh*) arg;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;

    _mesh_verify_context_type(context);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* Generate BO to hold the data */
    bool is_buffer_storage_supported = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,
                            &is_buffer_storage_supported);

    if (is_buffer_storage_supported      &&
        mesh_ptr->gl_bo_id          != 0)
    {
        entry_points->pGLDeleteBuffers(1,
                                      &mesh_ptr->gl_bo_id);

        mesh_ptr->gl_bo_id = 0;
    }

    if (mesh_ptr->gl_bo_id == 0)
    {
        entry_points->pGLGenBuffers(1,
                                   &mesh_ptr->gl_bo_id);
    }

    /* Copy the data to VRAM.
     *
     * Use immutable storage if supported. Mesh data will not be changed
     * throughout the life-time of the object.
     */
    if (is_buffer_storage_supported)
    {
        dsa_entry_points->pGLNamedBufferStorageEXT(mesh_ptr->gl_bo_id,
                                                   mesh_ptr->gl_processed_data_size,
                                                   mesh_ptr->gl_processed_data,
                                                   0); /* flags */
    }
    else
    {
        dsa_entry_points->pGLNamedBufferDataEXT(mesh_ptr->gl_bo_id,
                                                mesh_ptr->gl_processed_data_size,
                                                mesh_ptr->gl_processed_data,
                                                GL_STATIC_DRAW);
    }

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR,
                      "Could not copy mesh data to VRAM");

    /* Create TBO for the BO */
    if (mesh_ptr->n_sh_bands != 0)
    {
        ASSERT_DEBUG_SYNC(mesh_ptr->n_sh_bands == 3 ||
                          mesh_ptr->n_sh_bands == 4,
                          "Only 3 or 4 SH bands are supported");

        if (mesh_ptr->gl_tbo != NULL)
        {
            ogl_texture_release(mesh_ptr->gl_tbo);

            mesh_ptr->gl_tbo = NULL;
        }

        mesh_ptr->gl_tbo = ogl_texture_create(context,
                                              system_hashed_ansi_string_create_by_merging_two_strings("Mesh ",
                                                                                                      system_hashed_ansi_string_get_buffer(mesh_ptr->name) ));

        dsa_entry_points->pGLTextureBufferEXT(mesh_ptr->gl_tbo,
                                              GL_TEXTURE_BUFFER,
                                              (mesh_ptr->n_sh_bands == 4 ? GL_RGBA32F : GL_RGB32F),
                                              mesh_ptr->gl_bo_id);

        ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not create mesh TBO");
    }

    /* Safe to release GL processed data buffer now! */
    if (!(mesh_ptr->creation_flags & MESH_SAVE_SUPPORT) )
    {
        delete [] mesh_ptr->gl_processed_data;

        mesh_ptr->gl_processed_data      = NULL;
        mesh_ptr->gl_processed_data_size = 0;
    }

    /* Mark mesh as GL-initialized */
    mesh_ptr->gl_thread_fill_gl_buffers_call_needed = false;
    mesh_ptr->gl_storage_initialized                = true;
}

/** TODO */
PRIVATE void _mesh_get_amount_of_stream_data_sets(__in      __notnull _mesh*                      mesh_ptr,
                                                  __in                mesh_layer_data_stream_type stream_type,
                                                  __in                mesh_layer_id               layer_id,
                                                  __in                mesh_layer_pass_id          layer_pass_id,
                                                  __out_opt           uint32_t*                   out_n_stream_sets)
{
    if (out_n_stream_sets != NULL)
    {
        _mesh_layer* layer_ptr = NULL;

        *out_n_stream_sets = 0;

        if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                   layer_id,
                                                  &layer_ptr) )
        {
            _mesh_layer_pass* pass_ptr = NULL;

            if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                       layer_pass_id,
                                                      &pass_ptr) )
            {
                if (pass_ptr->index_data_maps[stream_type] != NULL)
                {
                    const uint32_t n_sets = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[stream_type]);

                    if (n_sets > 0)
                    {
                        *out_n_stream_sets += n_sets;
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Cannot retrieve pass descriptor at index [%d]",
                                  layer_pass_id);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Cannot retrieve layer descriptor at index [%d]",
                              layer_id);
        }
    }
}

/** TODO */
PRIVATE void _mesh_get_stream_data_properties(__in      _mesh*                            mesh_ptr,
                                              __in      mesh_layer_data_stream_type       stream_type,
                                              __out_opt mesh_layer_data_stream_data_type* out_data_type,
                                              __out_opt uint32_t*                         out_n_components,
                                              __out_opt uint32_t*                         out_required_bit_alignment)
{
    mesh_layer_data_stream_data_type result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN;
    uint32_t                         result_n_components           = 0;
    uint32_t                         result_required_bit_alignment = 0;

    switch (stream_type)
    {
        case MESH_LAYER_DATA_STREAM_TYPE_NORMALS:
        case MESH_LAYER_DATA_STREAM_TYPE_VERTICES:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            result_n_components           = 3;
            result_required_bit_alignment = 0;

            break;
        }

        case MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            result_n_components           = 2;
            result_required_bit_alignment = 0;

            break;
        }

        case MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS:
        case MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            result_n_components           = mesh_ptr->n_sh_bands * mesh_ptr->n_sh_bands * mesh_ptr->n_sh_components;
            result_required_bit_alignment = (stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS) ? 96 : 128;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized stream type");
        }
    }

    if (out_data_type != NULL)
    {
        *out_data_type = result_data_type;
    }

    if (out_n_components != NULL)
    {
        *out_n_components = result_n_components;
    }

    if (out_required_bit_alignment != NULL)
    {
        *out_required_bit_alignment = result_required_bit_alignment;
    }
}

/** TODO */
PRIVATE void _mesh_get_total_number_of_stream_sets_for_mesh(__in      __notnull _mesh*                      mesh_ptr,
                                                            __in                mesh_layer_data_stream_type stream_type,
                                                            __out_opt           uint32_t*                   out_n_stream_sets)
{
    if (out_n_stream_sets != NULL)
    {
        _mesh_layer*       layer_ptr = NULL;
        const unsigned int n_layers  = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

        *out_n_stream_sets = 0;

        for (unsigned int n_layer = 0; n_layer < n_layers; ++n_layer)
        {
            if (system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr) )
            {
                const unsigned int n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);
                _mesh_layer_pass*  pass_ptr       = NULL;

                for (unsigned int n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass)
                {
                    if (system_resizable_vector_get_element_at(layer_ptr->passes, n_layer_pass, &pass_ptr) )
                    {
                        if (pass_ptr->index_data_maps[stream_type] != NULL)
                        {
                            const uint32_t n_sets = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[stream_type]);

                            if (n_sets > 0)
                            {
                                *out_n_stream_sets += n_sets;
                            }
                        }
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false, "Cannot retrieve pass descriptor at index [%d]", n_layer_pass);
                    }
                } /* for (all layer passes) */
            } /* if (layer found) */
            else
            {
                ASSERT_DEBUG_SYNC(false, "Cannot retrieve layer descriptor at index [%d]", n_layer);
            }
        } /* for (all layers) */
    } /* if (out_n_stream_sets != NULL) */
}

/** TODO */
PRIVATE void _mesh_init_mesh(__in __notnull _mesh*                    new_mesh,
                             __in __notnull system_hashed_ansi_string name,
                                            mesh_creation_flags       flags)
{
    memset(new_mesh->aabb_max, 0, sizeof(new_mesh->aabb_max) );
    memset(new_mesh->aabb_min, 0, sizeof(new_mesh->aabb_min) );

    new_mesh->aabb_max[3]                           = 1;
    new_mesh->aabb_min[3]                           = 1;
    new_mesh->creation_flags                        = flags;
    new_mesh->gl_bo_id                              = 0;
    new_mesh->gl_context                            = NULL;
    new_mesh->gl_index_type                         = MESH_INDEX_TYPE_UNKNOWN;
    new_mesh->gl_processed_data                     = NULL;
    new_mesh->gl_processed_data_size                = 0;
    new_mesh->gl_storage_initialized                = false;
    new_mesh->gl_tbo                                = 0;
    new_mesh->gl_thread_fill_gl_buffers_call_needed = false;
    new_mesh->instantiation_parent                  = NULL;
    new_mesh->layers                                = system_resizable_vector_create(START_LAYERS, sizeof(void*) );
    new_mesh->materials                             = system_resizable_vector_create(4 /* capacity */, sizeof(mesh_material) );
    new_mesh->n_gl_unique_vertices                  = 0;
    new_mesh->n_sh_bands                            = 0;
    new_mesh->n_sh_components                       = SH_COMPONENTS_UNDEFINED;
    new_mesh->name                                  = name;
    new_mesh->set_id_counter                        = 0;
    new_mesh->timestamp_last_modified               = system_time_now();

    for (unsigned int n_stream_type = 0;
                      n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                      n_stream_type ++)
    {
        new_mesh->gl_processed_data_stream_start_offset[n_stream_type] = -1;
    }
    new_mesh->gl_processed_data_stride = -1;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer(_mesh_layer* new_mesh_layer)
{
    new_mesh_layer->data_streams         = system_hash64map_create(4 /* capacity */);
    new_mesh_layer->n_gl_unique_elements = 0;
    new_mesh_layer->passes_counter       = 1;

    memset(new_mesh_layer->aabb_max, 0, sizeof(new_mesh_layer->aabb_max) );
    memset(new_mesh_layer->aabb_min, 0, sizeof(new_mesh_layer->aabb_min) );

    new_mesh_layer->aabb_max[3] = 1;
    new_mesh_layer->aabb_min[3] = 1;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer_data_stream(_mesh_layer_data_stream* new_mesh_layer_data_stream)
{
    new_mesh_layer_data_stream->data                   = NULL;
    new_mesh_layer_data_stream->data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN;
    new_mesh_layer_data_stream->n_components           = 0;
    new_mesh_layer_data_stream->n_items                = 0;
    new_mesh_layer_data_stream->required_bit_alignment = 0;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer_pass(_mesh_layer_pass* new_mesh_layer_pass)
{
    new_mesh_layer_pass->gl_bo_elements_offset = 0;
    new_mesh_layer_pass->gl_elements           = NULL;
    new_mesh_layer_pass->material              = NULL;
    new_mesh_layer_pass->n_elements            = 0;
    new_mesh_layer_pass->smoothing_angle       = 0.0f;

    for (mesh_layer_data_stream_type stream_type  = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                     stream_type != MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                             ++(int&)stream_type)
    {
        new_mesh_layer_pass->index_data_maps        [stream_type] = system_hash64map_create(sizeof(_mesh_layer_pass_index_data*) );
        new_mesh_layer_pass->set_id_to_unique_set_id[stream_type] = system_hash64map_create(sizeof(uint32_t) );
    }
}

/** TODO */
PRIVATE void _mesh_material_setting_changed(__in __notnull const void* callback_data,
                                            __in __notnull       void* user_arg)
{
    _mesh*        mesh_ptr     = (_mesh*)        user_arg;
    float         material_vsa = 0.0f;
    mesh_material src_material = (mesh_material) callback_data;

    mesh_material_get_property(src_material,
                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                              &material_vsa);

    /* Is the Vertex Smoothing Angle setting different we used to generate normals data
     * different from the new value? */
    bool           found_dependant_layers   = false;
    const uint32_t n_layers                 = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);
    bool           needs_normals_data_regen = false;

    for (uint32_t n_layer = 0;
                  n_layer < n_layers && !needs_normals_data_regen;
                ++n_layer)
    {
        _mesh_layer* layer_ptr      = NULL;
        uint32_t     n_layer_passes = 0;

        if (!system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                    n_layer,
                                                   &layer_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh layer descriptor");

            continue;
        }

        n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

        for (uint32_t n_layer_pass = 0;
                      n_layer_pass < n_layer_passes && !needs_normals_data_regen;
                    ++n_layer_pass)
        {
            _mesh_layer_pass* layer_pass_ptr = NULL;

            if (!system_resizable_vector_get_element_at(layer_ptr->passes,
                                                        n_layer_pass,
                                                       &layer_pass_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh layer pass descriptor");

                continue;
            }

            if (layer_pass_ptr->material == src_material)
            {
                found_dependant_layers = true;

                if (fabs(layer_pass_ptr->smoothing_angle - material_vsa) > 1e-5f)
                {
                    needs_normals_data_regen = true;
                }
            }
        } /* for (all mesh layer passes) */
    } /* for (all mesh layers) */

    ASSERT_DEBUG_SYNC(found_dependant_layers,
                      "Redundant call-back detected!");

    if (needs_normals_data_regen)
    {
        LOG_INFO("Mesh [%s] is being re-configured to use a different smoothing angle. Re-generating normal data.",
                 system_hashed_ansi_string_get_buffer(mesh_ptr->name) );

        /* Release any data that's already been created */
        mesh_free_single_indexed_representation( (mesh) mesh_ptr);
        _mesh_release_normals_data             (mesh_ptr);

        /* Generate normal data */
        mesh_generate_normal_data( (mesh) mesh_ptr);

        /* Update GL blob */
        mesh_fill_gl_buffers( (mesh) mesh_ptr,
                             mesh_ptr->gl_context);
    } /* if (needs_normals_data_regen) */
}

/** TODO */
PRIVATE void _mesh_release_normals_data(__in __notnull _mesh* mesh_ptr)
{
    const uint32_t n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        _mesh_layer* layer_ptr      = NULL;
        uint32_t     n_layer_passes = 0;

        if (!system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                    n_layer,
                                                   &layer_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh layer descriptor");

            continue;
        }

        /* Release data stream, if there's any defined for normal data */
        _mesh_layer_data_stream* normals_data_stream_ptr = NULL;

        if (system_hash64map_contains(layer_ptr->data_streams,
                                      (system_hash64) MESH_LAYER_DATA_STREAM_TYPE_NORMALS) )
        {
            if (!system_hash64map_get_element_at(layer_ptr->data_streams,
                                                 (system_hash64) MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                &normals_data_stream_ptr,
                                                 NULL) ) /* outHash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve normals data stream descriptor");

                continue;
            }

            delete normals_data_stream_ptr;
            normals_data_stream_ptr = NULL;

            system_hash64map_remove(layer_ptr->data_streams,
                                    (system_hash64) MESH_LAYER_DATA_STREAM_TYPE_NORMALS);
        } /* if (system_hash64map_contains(layer_ptr->data_streams, (system_hash64) MESH_LAYER_DATA_STREAM_TYPE_NORMALS) */

        /* Release layer pass data */
        n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

        for (uint32_t n_layer_pass = 0;
                      n_layer_pass < n_layer_passes;
                    ++n_layer_pass)
        {
            _mesh_layer_pass* pass_ptr = NULL;

            if (!system_resizable_vector_get_element_at(layer_ptr->passes,
                                                        n_layer_pass,
                                                       &pass_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh layer pass descriptor");

                continue;
            }

            /* Any index data streams defined for normals data? */
            if (pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS] != NULL)
            {
                const uint32_t n_index_data_entries = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS]);

                for (uint32_t n_index_data_entry = 0;
                              n_index_data_entry < n_index_data_entries;
                            ++n_index_data_entry)
                {
                    system_hash64                index_data_hash = -1;
                    _mesh_layer_pass_index_data* index_data_ptr  = NULL;

                    if (!system_hash64map_get_element_at(pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS],
                                                         0, /* n_entry */
                                                        &index_data_ptr,
                                                        &index_data_hash) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve index data descriptor");

                        continue;
                    }

                    delete index_data_ptr;
                    index_data_ptr = NULL;

                    system_hash64map_remove(pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS],
                                            index_data_hash);
                } /* for (all index data entries) */
            } /* if (pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS] != NULL) */
        } /* for (all mesh layer passes) */
    } /* for (all mesh layers) */
}

/** TODO */
PRIVATE void _mesh_release_renderer_callback(ogl_context context, void* arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _mesh*                            mesh         = (_mesh*) arg;

    _mesh_verify_context_type(context);

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (mesh->gl_bo_id != -1)
    {
        entry_points->pGLDeleteBuffers(1,
                                      &mesh->gl_bo_id);
    }

    if (mesh->gl_tbo != NULL)
    {
        ogl_texture_release(mesh->gl_tbo);
    }
}

/** TODO */
PRIVATE void _mesh_release(__in __notnull __post_invalid void* arg)
{
    _mesh* mesh = (_mesh*) arg;

    /* Sign out of material call-backs */
    const uint32_t n_materials = system_resizable_vector_get_amount_of_elements(mesh->materials);

    for (uint32_t n_material = 0;
                  n_material < n_materials;
                ++n_material)
    {
        system_callback_manager callback_manager = NULL;
        mesh_material           material         = NULL;

        if (!system_resizable_vector_get_element_at(mesh->materials,
                                                    n_material,
                                                   &material) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh material at index [%d]",
                              n_material);

            continue;
        }

        mesh_material_get_property(material,
                                   MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,
                                  &callback_manager);

        system_callback_manager_unsubscribe_from_callbacks(callback_manager,
                                                           MESH_MATERIAL_CALLBACK_ID_VSA_CHANGED,
                                                           _mesh_material_setting_changed,
                                                           mesh);
    } /* for (all mesh materials) */

    /* Carry on with the usual release process */
    if (mesh->layers != NULL)
    {
        /* Release layers */
        while (system_resizable_vector_get_amount_of_elements(mesh->layers) > 0)
        {
            _mesh_layer* layer = NULL;

            system_resizable_vector_pop(mesh->layers,
                                        &layer);

            ASSERT_DEBUG_SYNC(layer != NULL, "Cannot release layer info - layer is NULL");
            if (layer != NULL)
            {
                _mesh_deinit_mesh_layer(mesh,
                                        layer,
                                        true);

                delete layer;
            }
        }

        system_resizable_vector_release(mesh->layers);
    } /* if (mesh->layers != NULL) */

    /* Release GL stuff */
    if (mesh->gl_processed_data != NULL)
    {
        delete [] mesh->gl_processed_data;

        mesh->gl_processed_data = NULL;
    }

    /* Request rendering thread call-back */
    ogl_context_request_callback_from_context_thread(mesh->gl_context,
                                                     _mesh_release_renderer_callback,
                                                     mesh);

    if (mesh->gl_context != NULL)
    {
        ogl_context_release(mesh->gl_context);

        mesh->gl_context = NULL;
    }

    /* Release other helper structures */
    if (mesh->instantiation_parent != NULL)
    {
        mesh_release(mesh->instantiation_parent);

        mesh->instantiation_parent = NULL;
    }

    if (mesh->materials != NULL)
    {
        system_resizable_vector_release(mesh->materials);

        mesh->materials = NULL;
    }
}

/** TODO */
PRIVATE void _mesh_update_aabb(__in __notnull _mesh* mesh_ptr)
{
    const uint32_t n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

    for (int n_dimension = 0;
             n_dimension < 3; /* x, y, z */
           ++n_dimension)
    {
        _mesh_layer* layer_ptr = NULL;

        mesh_ptr->aabb_max[n_dimension] = 0;
        mesh_ptr->aabb_min[n_dimension] = 0;
    }

    for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                   n_layer,
                                                  &layer_ptr) )
        {
            for (int n_dimension = 0;
                     n_dimension < 3; /* x, y, z */
                   ++n_dimension)
            {
                if (n_layer == 0                                                                       ||
                    n_layer != 0 && mesh_ptr->aabb_max[n_dimension] < layer_ptr->aabb_max[n_dimension])
                {
                    mesh_ptr->aabb_max[n_dimension] = layer_ptr->aabb_max[n_dimension];
                }

                if (n_layer == 0                                                                       ||
                    n_layer != 0 && mesh_ptr->aabb_min[n_dimension] > layer_ptr->aabb_min[n_dimension])
                {
                    mesh_ptr->aabb_min[n_dimension] = layer_ptr->aabb_min[n_dimension];
                }
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh layer descriptor");
        }
    } /* for (all layers) */
}

/** TODO */
#ifdef _DEBUG
    /* TODO */
    PRIVATE void _mesh_verify_context_type(__in __notnull ogl_context context)
    {
        ogl_context_type context_type = OGL_CONTEXT_TYPE_UNDEFINED;

        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_TYPE,
                                &context_type);

        ASSERT_DEBUG_SYNC(context_type == OGL_CONTEXT_TYPE_GL,
                          "mesh is only supported under GL contexts")
    }
#endif


/* Please see header for specification */
PUBLIC EMERALD_API mesh_layer_id mesh_add_layer(__in __notnull mesh instance)
{
    _mesh*        mesh_instance = (_mesh*) instance;
    mesh_layer_id result        = -1;

    ASSERT_ALWAYS_SYNC(!mesh_instance->gl_storage_initialized,    "Cannot add mesh layers after GL storage has been initialized");\
    //ASSERT_ALWAYS_SYNC( mesh_instance->n_gl_unique_vertices == 0, "GL buffer's contents already initialized");

    /* Create new layer */
    _mesh_layer* new_layer = new (std::nothrow) _mesh_layer;

    ASSERT_DEBUG_SYNC(new_layer != NULL, "Out of memory");
    if (new_layer != NULL)
    {
        _mesh_init_mesh_layer(new_layer);

        new_layer->passes = system_resizable_vector_create(4, sizeof(_mesh_layer_pass*) );
        ASSERT_DEBUG_SYNC(new_layer->passes != NULL, "Out of memory");

        /* Add the new layer */
        system_resizable_vector_push(mesh_instance->layers, new_layer);

        result = system_resizable_vector_get_amount_of_elements(mesh_instance->layers)-1;

        /* Update modification timestamp */
        mesh_instance->timestamp_last_modified = system_time_now();
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_add_layer_data_stream(__in __notnull mesh                        mesh,
                                                   __in __notnull mesh_layer_id               layer_id,
                                                   __in           mesh_layer_data_stream_type type,
                                                   __in           unsigned int                n_items,
                                                   __in __notnull const void*                 data)
{
    _mesh_layer* layer_ptr     = NULL;
    _mesh*       mesh_instance = (_mesh*) mesh;

    /* Store amount of SH bands if this is a SH data stream. */
    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS ||
        type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS)
    {
        /* We only support RGB spherical harmonics */
        mesh_instance->n_sh_components = SH_COMPONENTS_RGB;
    }

    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS)
    {
        if (mesh_instance->n_sh_bands == 0)
        {
            mesh_instance->n_sh_bands = 3;
        }
        else
        {
            ASSERT_ALWAYS_SYNC(mesh_instance->n_sh_bands == 3, "Amount of SH bands must be shared between all mesh layers");
        }
    }

    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS)
    {
        if (mesh_instance->n_sh_bands == 0)
        {
            mesh_instance->n_sh_bands = 4;
        }
        else
        {
            ASSERT_ALWAYS_SYNC(mesh_instance->n_sh_bands == 4, "Amount of SH bands must be shared between all mesh layers");
        }
    }

    if (system_resizable_vector_get_element_at(mesh_instance->layers,
                                               layer_id,
                                              &layer_ptr) )
    {
        /* Make sure the stream has not already been added */
        if (!system_hash64map_contains(layer_ptr->data_streams,
                                       type) )
        {
            _mesh_layer_data_stream* data_stream_ptr = new (std::nothrow) _mesh_layer_data_stream;

            if (data_stream_ptr != NULL)
            {
                _mesh_init_mesh_layer_data_stream(data_stream_ptr);
                _mesh_get_stream_data_properties (mesh_instance,
                                                  type,
                                                  &data_stream_ptr->data_type,
                                                  &data_stream_ptr->n_components,
                                                  &data_stream_ptr->required_bit_alignment);

                ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT,
                                  "TODO");

                unsigned int component_size = (data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT) ? 4 : 1;

                data_stream_ptr->data    = new (std::nothrow) unsigned char[data_stream_ptr->n_components * component_size * n_items];
                data_stream_ptr->n_items = n_items;

                if (data_stream_ptr->data == NULL)
                {
                    ASSERT_ALWAYS_SYNC(false, "Out of memory");

                    delete data_stream_ptr;
                    data_stream_ptr = NULL;
                }
                else
                {
                    memcpy(data_stream_ptr->data,
                           data,
                           data_stream_ptr->n_components * component_size * n_items);

                    /* Store the stream */
                    system_hash64 temp = type;

                    ASSERT_DEBUG_SYNC(!system_hash64map_contains(layer_ptr->data_streams,
                                                                 (system_hash64) temp),
                                      "Streeam already defined");

                    system_hash64map_insert(layer_ptr->data_streams,
                                            temp,
                                            data_stream_ptr,
                                            NULL,
                                            NULL);

                    /* Update modification timestamp */
                    mesh_instance->timestamp_last_modified = system_time_now();
                }
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false,
                                   "Out of memory");
            }
        }
        else
        {
            /** TODO: This will be hit for multiple texcoord streams and needs to be expanded! */
            ASSERT_ALWAYS_SYNC(false,
                               "Data stream [%x] has already been added to mesh",
                               type);
        }
    } /* if (system_resizable_vector_get_element_at(mesh_instance->layers, layer_id, &layer_ptr) ) */
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve layer at index [%d]",
                           layer_id);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass(__in __notnull mesh          mesh_instance,
                                                          __in           mesh_layer_id layer_id,
                                                          __in           mesh_material material,
                                                          __in           uint32_t      n_elements)
{
    _mesh*             mesh_ptr       = (_mesh*) mesh_instance;
    _mesh_layer*       mesh_layer_ptr = NULL;
    mesh_layer_pass_id result_id      = -1;

    ASSERT_DEBUG_SYNC(n_elements % 3 == 0,
                      "n_elements is not divisible by three which is invalid");

    /* Sanity checks */
    ASSERT_ALWAYS_SYNC(!mesh_ptr->gl_storage_initialized,
                       "Cannot add mesh layer passes after GL storage has been initialized");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               layer_id,
                                              &mesh_layer_ptr) &&
        mesh_layer_ptr != NULL)
    {
        _mesh_layer_pass* new_layer_pass_ptr = new (std::nothrow) _mesh_layer_pass;

        if (new_layer_pass_ptr != NULL)
        {
            _mesh_init_mesh_layer_pass(new_layer_pass_ptr);

            new_layer_pass_ptr->material   = material;
            new_layer_pass_ptr->n_elements = n_elements;
            result_id                      = system_resizable_vector_get_amount_of_elements(mesh_layer_ptr->passes);

            mesh_material_retain(material);

            if (material != NULL)
            {
                /* Cache the material in a helper structure */
                bool               is_material_cached = false;
                const unsigned int n_materials_cached = system_resizable_vector_get_amount_of_elements(mesh_ptr->materials);

                for (unsigned int n_material = 0;
                                  n_material < n_materials_cached;
                                ++n_material)
                {
                    mesh_material cached_material = NULL;

                    if (system_resizable_vector_get_element_at(mesh_ptr->materials,
                                                               n_material,
                                                              &cached_material) )
                    {
                        if (cached_material == material)
                        {
                            is_material_cached = true;

                            break;
                        }
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false, "Could not retrieve cached material at index [%d]", n_material);
                    }
                }

                if (!is_material_cached)
                {
                    system_resizable_vector_push(mesh_ptr->materials,
                                                 material);

                    /* Sign up for important call-backs */
                    system_callback_manager callback_manager = NULL;

                    mesh_material_get_property(material,
                                               MESH_MATERIAL_PROPERTY_CALLBACK_MANAGER,
                                              &callback_manager);

                    system_callback_manager_subscribe_for_callbacks(callback_manager,
                                                                    MESH_MATERIAL_CALLBACK_ID_VSA_CHANGED,
                                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                                    _mesh_material_setting_changed,
                                                                    mesh_ptr);
                }
            } /* if (material != NULL) */

            /* Add new pass */
            system_resizable_vector_push(mesh_layer_ptr->passes,
                                         new_layer_pass_ptr);

            /* Update modification timestamp */
            mesh_ptr->timestamp_last_modified = system_time_now();
        } /* if (new_layer_pass_ptr != NULL) */
        else
        {
            ASSERT_ALWAYS_SYNC(false, "Out of memory");
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve mesh layer with id [%d]", layer_id);
    }

    return result_id;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_add_layer_pass_index_data(__in __notnull mesh                        mesh_instance,
                                                       __in           mesh_layer_id               layer_id,
                                                       __in           mesh_layer_pass_id          layer_pass_id,
                                                       __in           mesh_layer_data_stream_type stream_type,
                                                       __in           unsigned int                set_id,
                                                       __in           const void*                 index_data,
                                                       __in           unsigned int                min_index,
                                                       __in           unsigned int                max_index)
{
    /* TODO: This implementation is not currently able to handle multiple texcoord streams. Please improve */
    _mesh*             mesh_ptr       = (_mesh*) mesh_instance;
    _mesh_layer*       mesh_layer_ptr = NULL;
    bool               result         = false;
    mesh_layer_pass_id result_id      = -1;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &mesh_layer_ptr) &&
        mesh_layer_ptr                 != NULL                                              &&
        mesh_ptr->n_gl_unique_vertices == 0)
    {
        _mesh_layer_pass* pass_ptr = NULL;

        if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes,
                                                   layer_pass_id,
                                                  &pass_ptr))
        {
            if (!system_hash64map_contains(pass_ptr->index_data_maps[stream_type],
                                           set_id) )
            {
                _mesh_layer_pass_index_data* new_data_set = new (std::nothrow) _mesh_layer_pass_index_data;

                ASSERT_ALWAYS_SYNC(new_data_set != NULL, "Out of memory");
                if (new_data_set != NULL)
                {
                    ASSERT_DEBUG_SYNC(new_data_set->max_index >= new_data_set->min_index,
                                      "Invalid min/max mesh indices");

                    new_data_set->data      = new (std::nothrow) uint32_t[pass_ptr->n_elements];
                    new_data_set->max_index = max_index;
                    new_data_set->min_index = min_index;

                    ASSERT_ALWAYS_SYNC(new_data_set->data != NULL,
                                       "Out of memory");

                    if (new_data_set->data != NULL)
                    {
                        memcpy(new_data_set->data,
                               index_data,
                               pass_ptr->n_elements * sizeof(unsigned int) );

                        system_hash64map_insert(pass_ptr->index_data_maps[stream_type],
                                                set_id,
                                                new_data_set,
                                                NULL,  /* on_remove_callback */
                                                NULL); /* on_remove_callback_user_arg */

                        if (system_hash64map_contains(pass_ptr->set_id_to_unique_set_id[stream_type],
                                                      set_id) )
                        {
                            system_hash64map_remove(pass_ptr->set_id_to_unique_set_id[stream_type],
                                                    set_id);
                        }

                        system_hash64map_insert(pass_ptr->set_id_to_unique_set_id[stream_type],
                                                set_id,
                                                (void*) mesh_ptr->set_id_counter,
                                                NULL,  /* on_remove_callback */
                                                NULL); /* on_remove_callback_user_arg */

                        mesh_ptr->set_id_counter++;
                        result = true;

                        /* Update modification timestamp */
                        mesh_ptr->timestamp_last_modified = system_time_now();
                    }
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(pass_ptr->index_data_maps[stream_type] == NULL,
                                  "Index data for user-specified set already configured");
            }
        }
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Layer pass [%x] was not recognized",
                               layer_pass_id);
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve mesh layer with id [%d]",
                           layer_id);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_create(__in           mesh_creation_flags       flags,
                                    __in __notnull system_hashed_ansi_string name)
{
    _mesh* new_mesh = new (std::nothrow) _mesh;

    ASSERT_DEBUG_SYNC(new_mesh != NULL, "Out of memory");
    if (new_mesh != NULL)
    {
        _mesh_init_mesh(new_mesh,
                        name,
                        flags);

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_mesh, 
                                                       _mesh_release, 
                                                       OBJECT_TYPE_MESH, 
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Meshes\\", system_hashed_ansi_string_get_buffer(name)) );

        LOG_INFO("Mesh [%s] created.", system_hashed_ansi_string_get_buffer(name) );
    }

    return (mesh) new_mesh;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance)
{
          _mesh*   mesh_ptr              = (_mesh*) instance;
          uint32_t n_layers              = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);
          uint32_t n_indices_used_so_far = 0;
    const uint32_t sh_alignment          = mesh_ptr->n_sh_bands * 4;

    ASSERT_DEBUG_SYNC(mesh_ptr->n_gl_unique_vertices == 0,
                      "Number of unique vertices must be 0");

    /* Reset amount of unique vertices and total number of elements */
    mesh_ptr->gl_processed_data_total_elements = 0;
    mesh_ptr->n_gl_unique_vertices             = 0;

    /* Iterate through layers */
    system_bst datastreams_surfaceid_bst                      = NULL;
    uint32_t   n_datastreams_surfaceid_key_words              = 0;
    uint32_t   n_different_layer_elements                     = 0;
    bool       stream_usage[MESH_LAYER_DATA_STREAM_TYPE_COUNT];

    memset(stream_usage,
           0,
           sizeof(stream_usage) );

    /* Iterate through vertex data streams and calculate axis-aligned bounding boxes for each layer. */
    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL,
                           "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            const _mesh_layer_data_stream* stream_data_ptr = NULL;

            if (system_hash64map_get(layer_ptr->data_streams,
                                     MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                    &stream_data_ptr) )
            {
                for (unsigned int n_item = 0;
                                  n_item < stream_data_ptr->n_items;
                                ++n_item)
                {
                    const float* vertex_data = (const float*) stream_data_ptr->data +
                                                              stream_data_ptr->n_components * n_item;

                    for (int n_dimension = 0;
                             n_dimension < 3; /* x, y, z */
                           ++n_dimension)
                    {
                        if (n_item == 0 ||
                            n_item != 0 && (layer_ptr->aabb_max[n_dimension] < vertex_data[n_dimension]))
                        {
                            layer_ptr->aabb_max[n_dimension] = vertex_data[n_dimension];
                        }

                        if (n_item == 0 ||
                            n_item != 0 && (layer_ptr->aabb_min[n_dimension] > vertex_data[n_dimension]))
                        {
                            layer_ptr->aabb_min[n_dimension] = vertex_data[n_dimension];
                        }
                    }
                } /* for (all vertex data stream items) */
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve vertex data stream descriptor.");
            }
        } /* if (layer_ptr != NULL) */
    } /* for (all layers) */

    /* Also update global AABB state */
    _mesh_update_aabb(mesh_ptr);

    /* Iterate through all data streams and assign unique ids to unique combinations we encounter.
     * We will later on use this information to create an index buffer and corresponding vertex/normals/texcoords arrays.
     *
     * Vertices may also use different surface ids. This must also be taken into consideration.
     **/
    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            /* 1. Determine how many words BST key needs to consist of */
            uint32_t n_pass_key_words = _mesh_get_total_number_of_sets(layer_ptr) + 1; /* Count in material representation */

            if (n_pass_key_words > n_datastreams_surfaceid_key_words)
            {
                n_datastreams_surfaceid_key_words = n_pass_key_words;
            }
        } /* if (layer_ptr != NULL) */
    } /* for (all layers) */

    /* 2. Allocate space for a BST key */
    uint32_t* key_ptr  = new (std::nothrow) uint32_t[2 /* unique set id + index */ * n_datastreams_surfaceid_key_words];
    uint32_t  key_size = 2 /* unique set id + index */ * n_datastreams_surfaceid_key_words * sizeof(uint32_t);

    ASSERT_ALWAYS_SYNC(key_ptr != NULL,
                       "Out of memory");

    /* 3. Build BST. Also count total number of indices the mesh uses */
    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            const uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            for (uint32_t n_pass = 0;
                          n_pass < n_passes;
                        ++n_pass)
            {
                const _mesh_layer_pass* pass_ptr = NULL;

                if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                           n_pass,
                                                          &pass_ptr) )
                {
                    for (uint32_t n_element = 0;
                                  n_element < pass_ptr->n_elements;
                                ++n_element)
                    {
                        /* Form the key.
                         *
                         * TODO: There is a potential flaw in here, that separate layers will be indexed with
                         *       keys of different layouts. Say, one layer may feature 3 texcoord stream sets,
                         *       but another one will only use 1 texcoord stream set. This will cause the
                         *       BST to malfunction and will potentially create duplicate entries; no memory
                         *       overflow risk since we're already using a key of maximum size.
                         *       If necessary, please fix - putting a debug-only assertion check to guard against
                         *       the glitch.
                         *
                         * NOTE: This does not seem to cause any visual glitches, however mind that the final GL
                         *       blob will waste some space for attribute data that is present for one layer, but
                         *       not for the other one. This can probably be improved to a great extent, but requires
                         *       some refactoring.
                         */
                        if ((_mesh_get_total_number_of_sets(layer_ptr) + 1) != n_datastreams_surfaceid_key_words)
                        {
                            static bool has_warned = false;

                            if (!has_warned)
                            {
                                LOG_ERROR("Key size mismatch while generating merged GL blob - generated blob will not be size-effective.");

                                has_warned = true;
                            }
                        }

                        memset(key_ptr,
                               0,
                               key_size);

                        _mesh_get_index_key(key_ptr,
                                            pass_ptr,
                                            n_element);

                        /* Insert the entry into the binary search tree */
                        if (datastreams_surfaceid_bst == NULL)
                        {
                            datastreams_surfaceid_bst = system_bst_create(key_size,
                                                                          sizeof(uint32_t),
                                                                          _mesh_is_key_uint32_lower,
                                                                          _mesh_is_key_uint32_equal,
                                                                          (system_bst_key)   key_ptr,
                                                                          (system_bst_value) &n_different_layer_elements);

                            n_different_layer_elements ++;
                        }
                        else
                        {
                            uint32_t temp = 0;

                            if (!system_bst_get(datastreams_surfaceid_bst,
                                                (system_bst_key) key_ptr,
                                                (system_bst_value*) &temp) )
                            {
                                system_bst_insert(datastreams_surfaceid_bst,
                                                  (system_bst_key)   key_ptr,
                                                  (system_bst_value) &n_different_layer_elements);

                                n_different_layer_elements ++;
                            }
                        }
                    } /* for (all indices) */

                    mesh_ptr->gl_processed_data_total_elements += pass_ptr->n_elements;
                } /* if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) ) */
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve pass [%d] descriptor", n_pass);
                }
            } /* for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass) */

            layer_ptr->n_gl_unique_elements = n_different_layer_elements;

            /* Determine which data streams are used */
            for (unsigned int n_stream_data_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                              n_stream_data_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                            ++n_stream_data_type)
            {
                if (system_hash64map_contains(layer_ptr->data_streams, n_stream_data_type) )
                {
                    stream_usage[n_stream_data_type] = true;
                }
            }
        } /* if (layer_ptr != NULL) */
    } /* for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer) */

    ASSERT_DEBUG_SYNC(datastreams_surfaceid_bst != NULL, "Data Stream + SurfaceID BST is not allowed to be NULL at this point.");
    if (datastreams_surfaceid_bst != NULL)
    {
        /* We now know how many entries we need in our index table. Let's allocate & fill it, as well as the data buffer. */
        uint32_t current_offset = 0;

        /* First reset all the offsets and the stride.*/
        for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                          n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                        ++n_data_stream_type)
        {
            mesh_ptr->gl_processed_data_stream_start_offset[n_data_stream_type] = -1;
        }

        mesh_ptr->gl_processed_data_size   = 0;
        mesh_ptr->gl_processed_data_stride = 0;

        /* Update offsets for those data streams that have been determined to be actually used */
        for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                          n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                        ++n_data_stream_type)
        {
            if (stream_usage[n_data_stream_type])
            {
                uint32_t                         n_sets                        = 0;
                mesh_layer_data_stream_data_type stream_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN;
                unsigned int                     stream_n_components           = 0;
                unsigned int                     stream_required_bit_alignment = 0;

                _mesh_get_stream_data_properties(mesh_ptr,
                                                 (mesh_layer_data_stream_type) n_data_stream_type,
                                                 &stream_data_type,
                                                 &stream_n_components,
                                                 &stream_required_bit_alignment);

                _mesh_get_total_number_of_stream_sets_for_mesh(mesh_ptr,
                                                               (mesh_layer_data_stream_type) n_data_stream_type,
                                                               &n_sets);

                if (n_sets == 0)
                {
                    /* We will use vertex indices in this case */
                    ASSERT_DEBUG_SYNC(stream_usage[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                      "No vertex data defined.");
                }

                ASSERT_DEBUG_SYNC(stream_data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT,
                                  "TODO: Unsupported stream data type");

                /* Align if necessary */
                uint32_t padding = 0;

                if (stream_required_bit_alignment != 0)
                {
                    ASSERT_DEBUG_SYNC(stream_required_bit_alignment % 32 == 0, "32-bit multiple alignment only supported");
                    stream_required_bit_alignment /= 32;

                    padding = (stream_required_bit_alignment - current_offset % stream_required_bit_alignment) % stream_required_bit_alignment;
                }

                /* Store the offset */
                current_offset                                                      += padding;
                mesh_ptr->gl_processed_data_stream_start_offset[n_data_stream_type]  = current_offset;

                current_offset                                                      += sizeof(float) * stream_n_components;
                mesh_ptr->gl_processed_data_stride                                  += sizeof(float) * stream_n_components;

                mesh_ptr->gl_processed_data_size                                    += padding + n_different_layer_elements * sizeof(float) * stream_n_components;
            } /* if (stream_usage[n_data_stream_type]) */
        } /* for (all data stream types) */

        /* We now know how much bytes we are going to need. Count in space for indices. */
        unsigned int index_size = 0;

        if (mesh_ptr->gl_processed_data_total_elements <= 255)
        {
            index_size              = sizeof(unsigned char);
            mesh_ptr->gl_index_type = MESH_INDEX_TYPE_UNSIGNED_CHAR;
        }
        else
        if (mesh_ptr->gl_processed_data_total_elements <= 65535)
        {
            index_size              = sizeof(unsigned short);
            mesh_ptr->gl_index_type = MESH_INDEX_TYPE_UNSIGNED_SHORT;
        }
        else
        {
            index_size              = sizeof(unsigned int);
            mesh_ptr->gl_index_type = MESH_INDEX_TYPE_UNSIGNED_INT;
        }

        mesh_ptr->gl_processed_data_size += mesh_ptr->gl_processed_data_total_elements * index_size;

        /* Allocate space for GL data */
        if (mesh_ptr->gl_processed_data != NULL)
        {
            delete [] mesh_ptr->gl_processed_data;

            mesh_ptr->gl_processed_data = NULL;
        }

        mesh_ptr->gl_processed_data = new (std::nothrow) float[mesh_ptr->gl_processed_data_size / sizeof(float)];

        ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL,
                           "Out of memory");
        if (mesh_ptr->gl_processed_data != NULL)
        {
            /* Insert indices data */
            void* elements_traveller_ptr = (uint32_t*) ((char*) mesh_ptr->gl_processed_data +
                                                        (mesh_ptr->gl_processed_data_size - mesh_ptr->gl_processed_data_total_elements * index_size) );

            for (uint32_t n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
            {
                _mesh_layer* layer_ptr = NULL;

                system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                       n_layer,
                                                      &layer_ptr);

                ASSERT_ALWAYS_SYNC(layer_ptr != NULL,
                                   "Could not retrieve mesh layer descriptor");

                if (layer_ptr != NULL)
                {
                    uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

                    for (uint32_t n_pass = 0;
                                  n_pass < n_passes;
                                ++n_pass)
                    {
                        uint32_t          max_index = -1;
                        uint32_t          min_index = -1;
                        _mesh_layer_pass* pass_ptr  = NULL;

                        if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                                   n_pass,
                                                                  &pass_ptr) &&
                            pass_ptr != NULL)
                        {
                            pass_ptr->gl_bo_elements_offset = (char*) elements_traveller_ptr -
                                                              (char*) mesh_ptr->gl_processed_data;

                            for (uint32_t n_element = 0;
                                          n_element < pass_ptr->n_elements;
                                        ++n_element)
                            {
                                uint32_t final_index = 0;

                                /* Form the key */
                                memset(key_ptr,
                                       0,
                                       key_size);

                                _mesh_get_index_key(key_ptr,
                                                    pass_ptr,
                                                    n_element);

                                /* Find the final index */
                                if (system_bst_get(datastreams_surfaceid_bst,
                                                   (system_bst_key) key_ptr,
                                                   (system_bst_value*) &final_index) )
                                {
                                    /* Update max/min index values for the pass */
                                    if (max_index == -1                            ||
                                        max_index != -1 && max_index < final_index)
                                    {
                                        max_index = final_index;
                                    }

                                    if (min_index == -1                            ||
                                        min_index != -1 && min_index > final_index)
                                    {
                                        min_index = final_index;
                                    }

                                    /* Store the index */
                                    switch (index_size)
                                    {
                                        case sizeof(unsigned char):
                                        {
                                            *(uint8_t*) elements_traveller_ptr = (unsigned char) final_index;

                                            (unsigned char*&) elements_traveller_ptr += sizeof(unsigned char);

                                            break;
                                        }

                                        case sizeof(unsigned short):
                                        {
                                            *(uint16_t*) elements_traveller_ptr = (unsigned short) final_index;

                                            (unsigned char*&) elements_traveller_ptr += sizeof(unsigned short);

                                            break;
                                        }

                                        case sizeof(unsigned int):
                                        {
                                            *(uint32_t*) elements_traveller_ptr = final_index;

                                            (unsigned char*&) elements_traveller_ptr += sizeof(unsigned int);

                                            break;
                                        }

                                        default:
                                        {
                                            ASSERT_DEBUG_SYNC(false, "Unrecognized index size");
                                        }
                                    } /* switch (index_size) */

                                    /* Fill the attribute data buffer */
                                    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                                    ++n_data_stream_type)
                                    {
                                        _mesh_layer_data_stream* data_stream_ptr = NULL;

                                        system_hash64map_get(layer_ptr->data_streams,
                                                             n_data_stream_type,
                                                            &data_stream_ptr);

                                        if (data_stream_ptr != NULL)
                                        {
                                            ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT, "TODO");

                                            mesh_layer_data_stream_type actual_stream_type  = (mesh_layer_data_stream_type) n_data_stream_type;
                                            uint32_t                    stream_n_components = 0;
                                            uint32_t                    stream_n_sets       = 0;

                                            _mesh_get_stream_data_properties(mesh_ptr,
                                                                             (mesh_layer_data_stream_type) n_data_stream_type,
                                                                             NULL,  /* out_data_type */
                                                                             &stream_n_components,
                                                                             NULL); /* out_required_bit_alignment */

                                            _mesh_get_amount_of_stream_data_sets(mesh_ptr,
                                                                                 (mesh_layer_data_stream_type) n_data_stream_type,
                                                                                 n_layer,
                                                                                 n_pass,
                                                                                &stream_n_sets);

                                            if (stream_n_sets == 0)
                                            {
                                                actual_stream_type = MESH_LAYER_DATA_STREAM_TYPE_VERTICES;
                                                stream_n_sets      = 1;
                                            }

                                            for (unsigned int n_set = 0;
                                                              n_set < stream_n_sets;
                                                            ++n_set)
                                            {
                                                unsigned int pass_index_data = 0;

                                                ASSERT_DEBUG_SYNC(pass_ptr->index_data_maps[actual_stream_type] != NULL,
                                                                  "");
                                                if (pass_ptr->index_data_maps[actual_stream_type] != NULL)
                                                {
                                                    _mesh_layer_pass_index_data* set_index_data = NULL;

                                                    if (!system_hash64map_get(pass_ptr->index_data_maps[actual_stream_type],
                                                                              n_set,
                                                                             &set_index_data) )
                                                    {
                                                        pass_index_data = n_element;
                                                    }
                                                    else
                                                    {
                                                        pass_index_data = _mesh_get_source_index_from_index_key(key_ptr,
                                                                                                                pass_ptr,
                                                                                                                (mesh_layer_data_stream_type) actual_stream_type,
                                                                                                                n_set);
                                                    }
                                                }

                                                /* We can finally do a memcpy()! */
                                                const uint32_t n_bytes_for_element_data = n_different_layer_elements *
                                                                                          stream_n_components;
                                                const uint32_t n_bytes_to_copy          = sizeof(float) * data_stream_ptr->n_components;
                                                const uint32_t dst_offset               = mesh_ptr->gl_processed_data_stream_start_offset[n_data_stream_type] + /* move to location where the unique data starts for given stream type */
                                                                                          mesh_ptr->gl_processed_data_stride * n_set * pass_ptr->n_elements   + /* move to current set */
                                                                                          mesh_ptr->gl_processed_data_stride * final_index;                     /* identify processed index */
                                                const uint32_t src_offset_div_4         = data_stream_ptr->n_components                                       *
                                                                                          pass_index_data;

                                                ASSERT_DEBUG_SYNC(pass_index_data < data_stream_ptr->n_items,
                                                                  "Invalid index about to be used");

                                                memcpy((char*) mesh_ptr->gl_processed_data + dst_offset,
                                                       (float*)data_stream_ptr->data       + src_offset_div_4,
                                                       n_bytes_to_copy);

                                                ASSERT_DEBUG_SYNC(dst_offset + n_bytes_to_copy < mesh_ptr->gl_processed_data_size,
                                                                  "Data buffer overflow!");
                                            } /* for (all sets) */
                                        } /* if (data_stream_ptr != NULL) */
                                    } /* for (all data streams) */
                                } /* if (final index is recognized) */
                                else
                                {
                                    ASSERT_ALWAYS_SYNC(false, "Could not retrieve final GL buffer index key");
                                }
                            } /* for (uint32_t n_element = 0; n_element < layer_ptr->n_elements; ++n_element) */

                            /* Store max/min values for the pass */
                            pass_ptr->gl_elements_max_index = max_index;
                            pass_ptr->gl_elements_min_index = min_index;

                            /* Store GPU-side elements representation so that user apps can access it (needed for KDtree intersection) */
                            if (pass_ptr->gl_elements != NULL)
                            {
                                delete [] pass_ptr->gl_elements;

                                pass_ptr->gl_elements = NULL;
                            }

                            pass_ptr->gl_elements = new (std::nothrow) uint32_t[pass_ptr->n_elements * index_size / sizeof(unsigned int)];

                            ASSERT_ALWAYS_SYNC(pass_ptr->gl_elements != NULL, "Out of memory");
                            if (pass_ptr->gl_elements != NULL)
                            {
                                memcpy(pass_ptr->gl_elements,
                                       (char*)mesh_ptr->gl_processed_data + pass_ptr->gl_bo_elements_offset,
                                       index_size * pass_ptr->n_elements);
                            }
                        } /* if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) && pass_ptr != NULL) */
                        else
                        {
                            ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer pass [%d] descriptor", n_pass);
                        }
                    } /* for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass) */
                } /* if (layer_ptr != NULL) */
            } /* for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)*/
        } /* if (mesh_ptr->gl_processed_data != NULL) */

        /* Store GPU vertex data representation so that user apps can access it (needed for KDtree intersection) */
        mesh_ptr->n_gl_unique_vertices = n_different_layer_elements;
    } /* if (vertex_normal_bst != NULL) */

    /* We're done ! Release the helper BST instance */
    system_bst_release(datastreams_surfaceid_bst);

    /* Release key storage */
    if (key_ptr != NULL)
    {
        delete [] key_ptr;
        key_ptr = NULL;
    }

    /* If saving support is not required, we can deallocate all the data buffers at this point! */
    if (!((mesh_ptr->creation_flags & MESH_SAVE_SUPPORT) ))
    {
        for (uint32_t n_layer = 0;
                      n_layer < n_layers;
                    ++n_layer)
        {
            _mesh_layer* layer_ptr = NULL;

            system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                   n_layer,
                                                  &layer_ptr);

            ASSERT_ALWAYS_SYNC(layer_ptr != NULL,
                               "Could not retrieve mesh layer descriptor");
            if (layer_ptr != NULL)
            {
                _mesh_deinit_mesh_layer(mesh_ptr,
                                        layer_ptr,
                                        false); /* do_full_deinit */
            }
        }
    }

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(__in __notnull mesh        instance,
                                             __in __notnull ogl_context context)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    /* Before we block renderer thread, we need to convert the loaded data into single-indexed representation.
     * We do not store it, since it's very likely to be bigger than n-indexed one we use for serialization. */
    if (mesh_ptr->gl_processed_data_size == 0)
    {
        mesh_create_single_indexed_representation(instance);
    }

    /* Request renderer thread call-back */
    if (mesh_ptr->gl_context == NULL)
    {
        mesh_ptr->gl_context = context;

        ogl_context_retain(context);
    }
    else
    {
        ASSERT_DEBUG_SYNC(mesh_ptr->gl_context == context,
                          "Inter-context sharing of meshes is not supported");
    }

    if (!ogl_context_request_callback_from_context_thread(context,
                                                          _mesh_fill_gl_buffers_renderer_callback,
                                                          mesh_ptr,
                                                          false) ) /* block_until_available */
    {
        /* Rendering thread unavailable - postpone the upload process until the draw call time. */
        mesh_ptr->gl_thread_fill_gl_buffers_call_needed = true;
    }
    else
    {
        result = true;

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_free_single_indexed_representation(mesh instance)
{
    _mesh*   mesh_ptr = (_mesh*) instance;
    uint32_t n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

    if (mesh_ptr->gl_processed_data != NULL)
    {
        delete [] mesh_ptr->gl_processed_data;

        mesh_ptr->gl_processed_data      = NULL;
        mesh_ptr->gl_processed_data_size = 0;
    }

    for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass)
            {
                _mesh_layer_pass* pass_ptr = NULL;

                if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                           n_pass,
                                                          &pass_ptr) &&
                    pass_ptr != NULL)
                {
                    pass_ptr->gl_bo_elements_offset = 0;

                    if (pass_ptr->gl_elements != NULL)
                    {
                        delete [] pass_ptr->gl_elements;

                        pass_ptr->gl_elements = NULL;
                    }
                } /* if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) && pass_ptr != NULL) */
                else
                {
                    ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer pass [%d] descriptor", n_pass);
                }
            } /* for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass) */
        } /* if (layer_ptr != NULL) */
    } /* for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)*/

    mesh_ptr->n_gl_unique_vertices = 0;

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_generate_normal_data(__in __notnull mesh mesh)
{
    /* TODO: Should we move this to mesh_normal_data_generation.cc or sth? */
    system_resizable_vector allocated_polygon_vectors  = system_resizable_vector_create                (4, /* capacity */
                                                                                                        sizeof(system_resizable_vector) );
    system_resource_pool    mesh_polygon_resource_pool = system_resource_pool_create                   (sizeof(_mesh_polygon),
                                                                                                        4,     /* n_elements_to_preallocate */
                                                                                                        NULL,  /* init_fn */
                                                                                                        NULL); /* deinit_fn */
    _mesh*                  mesh_ptr                   = (_mesh*) mesh;
    const unsigned int      n_layers                   = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);
    system_resource_pool    vec3_resource_pool         = system_resource_pool_create                   (sizeof(float) * 3,
                                                                                                        4,     /* n_elements_to_preallocate */
                                                                                                        NULL,  /* init_fn */
                                                                                                        NULL); /* deinit_fn */

    /* Determine which vertices are used by which polygons and store that
     * information in a BST,
     *
     * Key is a float[3], which corresponds to X, Y, Z location of the vertex.
     * Epsilon of 1e-5 is assumed.
     *
     * Value is a system_resizable_vector, storing _mesh_polygon* entries.
     **/
    system_bst vertex_to_polygon_vector_bst = NULL;

    for (unsigned int n_iteration = 0;
                      n_iteration < 2; /* 1. vertex-to-polygon BST construction.
                                          2. per-polygon normal calculation */
                    ++n_iteration)
    {
        LOG_INFO("Generating normal data for mesh [%s] (iteration %d/3) ...",
                 system_hashed_ansi_string_get_buffer(mesh_ptr->name),
                 n_iteration + 1);

        for (unsigned int n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
        {
            /* Retrieve raw vertex data */
            _mesh_layer*             layer_ptr              = NULL;
            unsigned int             n_layer_passes         = 0;
            _mesh_layer_data_stream* vertex_data_stream_ptr = NULL;
            const float*             vertex_data_ptr        = NULL;

            if (!system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                        n_layer,
                                                       &layer_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Layer descriptor unavailable");

                continue;
            }

            n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            if (system_hash64map_contains(layer_ptr->data_streams,
                                          MESH_LAYER_DATA_STREAM_TYPE_NORMALS) )
            {
                /* Normal data defined for this layer, move on */
                continue;
            }

            if (!system_hash64map_get(layer_ptr->data_streams,
                                      MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                     &vertex_data_stream_ptr) )
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve vertex data stream for mesh layer [%d]", n_layer);

                continue;
            }

            vertex_data_ptr = (const float*) vertex_data_stream_ptr->data;

            /* Iterate over all layer passes */
            for (unsigned int n_layer_pass = 0;
                              n_layer_pass < n_layer_passes;
                            ++n_layer_pass)
            {
                _mesh_layer_pass* layer_pass_ptr = NULL;
                unsigned int      n_indices      = 0;

                if (!system_resizable_vector_get_element_at(layer_ptr->passes,
                                                            n_layer_pass,
                                                           &layer_pass_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve layer pass descriptor for mesh layer [%d]",
                                      n_layer);

                    continue;
                }

                n_indices = layer_pass_ptr->n_elements;

                /* Run iteration-specific code */
                switch (n_iteration)
                {
                    case 0:
                    {
                        /* Vertex-to-polygon BST construction.
                         *
                         * NOTE: If there is no index data available, assume the vertices are defined in order.
                         **/
                        bool                         index_data_available = (layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES] != NULL);
                        _mesh_layer_pass_index_data* index_data_ptr       = NULL;

                        if (index_data_available)
                        {
                            /* n_vertex_data_sets == 0: no index data available.
                             *                    == 1:    index data available.
                             */
                            const uint32_t n_vertex_data_sets = system_hash64map_get_amount_of_elements(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES]);

                            if (n_vertex_data_sets > 1)
                            {
                                ASSERT_DEBUG_SYNC(n_vertex_data_sets <= 1,
                                                  "Unsupported number of vertex data sets");

                                continue;
                            }

                            if (n_vertex_data_sets == 1)
                            {
                                system_hash64map_get_element_at(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                                                0,     /* set id */
                                                                &index_data_ptr,
                                                                NULL); /* pOutHash */
                            }
                            else
                            {
                                index_data_available = false;
                            }
                        }

                        for (unsigned int n_index = 0;
                                          n_index < n_indices;
                                          n_index += 3)
                        {
                            /* For each triangle polygon, create a _mesh_polygon instance */
                            const uint32_t vertex_indices[] =
                            {
                                index_data_ptr != NULL ? (index_data_ptr->data[n_index + 0]) : (n_index),
                                index_data_ptr != NULL ? (index_data_ptr->data[n_index + 1]) : (n_index + 1),
                                index_data_ptr != NULL ? (index_data_ptr->data[n_index + 2]) : (n_index + 2)
                            };
                            const float* vertex_data[] =
                            {
                                vertex_data_ptr + 3 /* components */ * vertex_indices[0],
                                vertex_data_ptr + 3 /* components */ * vertex_indices[1],
                                vertex_data_ptr + 3 /* components */ * vertex_indices[2]
                            };

                            _mesh_polygon* new_polygon_ptr = (_mesh_polygon*) system_resource_pool_get_from_pool(mesh_polygon_resource_pool);

                            new_polygon_ptr->layer_ptr         = layer_ptr;
                            new_polygon_ptr->layer_pass_ptr    = layer_pass_ptr;
                            new_polygon_ptr->n_mesh_layer      = n_layer;
                            new_polygon_ptr->n_mesh_layer_pass = n_layer_pass;
                            new_polygon_ptr->triangle_index    = n_index / 3;

                            memcpy(new_polygon_ptr->vertex_data,
                                   vertex_data,
                                   sizeof(vertex_data) );

                            /* Assign the descriptor to each vertex */
                            unsigned int start_vertex_index = 0;

                            if (vertex_to_polygon_vector_bst == NULL)
                            {
                                system_resizable_vector new_vector = system_resizable_vector_create(4, /* capacity */
                                                                                                    sizeof(_mesh_polygon*) );

                                system_resizable_vector_push(allocated_polygon_vectors,
                                                             new_vector);
                                system_resizable_vector_push(new_vector,
                                                             new_polygon_ptr);

                                vertex_to_polygon_vector_bst = system_bst_create(sizeof(float)    * 3,            /* key size */
                                                                                 sizeof(system_resizable_vector), /* value size */
                                                                                 _mesh_is_key_float_lower,
                                                                                 _mesh_is_key_float_equal,
                                                                                 (system_bst_key)   vertex_data[0],
                                                                                 (system_bst_value) &new_vector);

                                start_vertex_index = 1;
                            }

                            for (unsigned int n_vertex_index = start_vertex_index;
                                              n_vertex_index < 3;
                                            ++n_vertex_index)
                            {
                                system_resizable_vector polygon_vector = NULL;

                                if (!system_bst_get(vertex_to_polygon_vector_bst,
                                                    (system_bst_key)     vertex_data[n_vertex_index],
                                                    (system_bst_value*) &polygon_vector) )
                                {
                                    polygon_vector = system_resizable_vector_create(4, /* capacity */
                                                                                    sizeof(_mesh_polygon*) );

                                    system_resizable_vector_push(allocated_polygon_vectors,
                                                                 polygon_vector);

                                    system_bst_insert(vertex_to_polygon_vector_bst,
                                                      (system_bst_key)   vertex_data[n_vertex_index],
                                                      (system_bst_value) &polygon_vector);
                                }

                                system_resizable_vector_push(polygon_vector,
                                                             new_polygon_ptr);
                            } /* for (all triangle vertices) */
                        } /* for (all polygon indices defined for the layer) */

                        break;
                    } /* case (0th iteration) */

                    case 1:
                    {
                        /* Per-polygon normal calculation */
                        const uint32_t n_polygon_vectors = system_resizable_vector_get_amount_of_elements(allocated_polygon_vectors);

                        ASSERT_DEBUG_SYNC(n_polygon_vectors != 0,
                                          "No polygon vectors found for mesh");

                        for (uint32_t n_polygon_vector = 0;
                                      n_polygon_vector < n_polygon_vectors;
                                    ++n_polygon_vector)
                        {
                            uint32_t                n_polygon_vector_items = 0;
                            system_resizable_vector polygon_vector         = NULL;

                            if (!system_resizable_vector_get_element_at(allocated_polygon_vectors,
                                                                        n_polygon_vector,
                                                                       &polygon_vector) )
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Could not retrieve polygon vector descriptor");

                                continue;
                            }

                            n_polygon_vector_items = system_resizable_vector_get_amount_of_elements(polygon_vector);

                            for (uint32_t n_polygon_vector_item = 0;
                                          n_polygon_vector_item < n_polygon_vector_items;
                                        ++n_polygon_vector_item)
                            {
                                _mesh_polygon* polygon_ptr = NULL;

                                if (!system_resizable_vector_get_element_at(polygon_vector,
                                                                            n_polygon_vector_item,
                                                                           &polygon_ptr) )
                                {
                                    ASSERT_DEBUG_SYNC(false,
                                                      "Could not retrieve polygon vector descriptor");

                                    continue;
                                }

                                /* Calculate the normal vector */
                                float b_minus_a[3];
                                float c_minus_a[3];

                                system_math_vector_minus3    (polygon_ptr->vertex_data[1],
                                                              polygon_ptr->vertex_data[0],
                                                              b_minus_a);
                                system_math_vector_minus3    (polygon_ptr->vertex_data[2],
                                                              polygon_ptr->vertex_data[0],
                                                              c_minus_a);
                                system_math_vector_cross3    (b_minus_a,
                                                              c_minus_a,
                                                              polygon_ptr->polygon_normal);
                                system_math_vector_normalize3(polygon_ptr->polygon_normal,
                                                              polygon_ptr->polygon_normal);
                            }
                        } /* for (all polygons) */

                        break;
                    } /* case (1st iteration) */

                    default: ASSERT_DEBUG_SYNC(false,
                                               "Unrecognized iteration index");
                } /* switch (n_iteration) */
            } /* for (all layer passes) */
        } /* for (all layers) */
    } /* for( all iterations) */

    /* Compute per-vertex normals. */
    ASSERT_ALWAYS_SYNC(n_layers != 0,
                       "Zero layers defined for the mesh");

    LOG_INFO("Generating normal data for mesh [%s] (iteration 3/3) ...",
             system_hashed_ansi_string_get_buffer(mesh_ptr->name) );

    for (unsigned int n_layer = 0;
                      n_layer < n_layers;
                    ++n_layer)
    {
        /* Retrieve layer vertex data */
        _mesh_layer*             layer_ptr              = NULL;
        _mesh_layer_data_stream* vertex_data_stream_ptr = NULL;

        if (!system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                    n_layer,
                                                   &layer_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Layer descriptor unavailable");

            continue;
        }

        if (!system_hash64map_get(layer_ptr->data_streams,
                                  MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                 &vertex_data_stream_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve vertex data stream for mesh layer [%d]",
                              n_layer);

            continue;
        }

        /* We will need to iterate over all polygons. */
        const uint32_t n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

        for (uint32_t n_layer_pass = 0;
                      n_layer_pass < n_layer_passes;
                    ++n_layer_pass)
        {
            unsigned int*                layer_pass_index_data            = NULL;
            float*                       layer_pass_normal_data           = NULL;
            _mesh_layer_pass*            layer_pass_ptr                   = NULL;
            _mesh_layer_pass_index_data* layer_pass_vertex_index_data_ptr = NULL;

            if (!system_resizable_vector_get_element_at(layer_ptr->passes,
                                                        n_layer_pass,
                                                       &layer_pass_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve layer pass descriptor");

                continue;
            }

            /* Retrieve vertex smoothing information */
            mesh_material_get_property(layer_pass_ptr->material,
                                       MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                                      &layer_pass_ptr->smoothing_angle);

            /* Allocate buffer to hold the normal data.
             *
             * NOTE: Per-vertex normals will be held individually for each defined vertex,
             *       in the order given by the indices. This is to ensure that the vertices,
             *       which are used many times in a single layer pass, are assigned unique
             *       normals. The normal vectors may be different, given that they will be
             *       calculated for a different set of owning polygons.
             *       This may appear to be space-ineffective, but bear in mind that all the
             *       layer data will eventually be combined when forming the GL blob.
             */
            layer_pass_index_data  = new (std::nothrow) unsigned int[layer_pass_ptr->n_elements];
            layer_pass_normal_data = new (std::nothrow) float       [layer_pass_ptr->n_elements * 3 /* components */];

            ASSERT_ALWAYS_SYNC(layer_pass_normal_data != NULL && layer_pass_index_data != NULL,
                               "Out of memory");

            memset(layer_pass_normal_data,
                   0,
                   sizeof(float) * layer_pass_ptr->n_elements * 3 /* components */);

            if (system_hash64map_get_amount_of_elements(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES]) > 0)
            {
                system_hash64map_get_element_at(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                                0,     /* set id */
                                               &layer_pass_vertex_index_data_ptr,
                                                NULL); /* pOutHash */
            }

            for (unsigned int n_current_triangle = 0;
                              n_current_triangle < layer_pass_ptr->n_elements / 3;
                            ++n_current_triangle)
            {
                const uint32_t vertices_indices[] =
                {
                    (layer_pass_vertex_index_data_ptr != NULL) ? (layer_pass_vertex_index_data_ptr->data[n_current_triangle * 3 + 0]) : (n_current_triangle * 3 + 0),
                    (layer_pass_vertex_index_data_ptr != NULL) ? (layer_pass_vertex_index_data_ptr->data[n_current_triangle * 3 + 1]) : (n_current_triangle * 3 + 1),
                    (layer_pass_vertex_index_data_ptr != NULL) ? (layer_pass_vertex_index_data_ptr->data[n_current_triangle * 3 + 2]) : (n_current_triangle * 3 + 2)
                };
                const float* vertices_data[] =
                {
                    (float*) vertex_data_stream_ptr->data + 3 /* components */ * vertices_indices[0],
                    (float*) vertex_data_stream_ptr->data + 3 /* components */ * vertices_indices[1],
                    (float*) vertex_data_stream_ptr->data + 3 /* components */ * vertices_indices[2]
                };
                const unsigned int n_vertices = sizeof(vertices_data) / sizeof(vertices_data[0]);

                /* Fill the index array */
                layer_pass_index_data[n_current_triangle * 3 + 0] = n_current_triangle * 3;
                layer_pass_index_data[n_current_triangle * 3 + 1] = n_current_triangle * 3 + 1;
                layer_pass_index_data[n_current_triangle * 3 + 2] = n_current_triangle * 3 + 2;

                /* Find the polygon-specific normal vector data. It's the same for all polygon vertices,
                 * so we need not run it in the triangle vertex loop. */
                float*                  current_triangle_polygon_normal      = NULL;
                system_resizable_vector current_triangle_polygon_vector      = NULL;
                uint32_t                current_triangle_polygon_vector_size = 0;

                if (!system_bst_get(vertex_to_polygon_vector_bst,
                                    (system_bst_key)     vertices_data[0],
                                    (system_bst_value*) &current_triangle_polygon_vector) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Polygon vertex was not recognized");

                    continue;
                }

                current_triangle_polygon_vector_size = system_resizable_vector_get_amount_of_elements(current_triangle_polygon_vector);

                for (unsigned int n_triangle_polygon_vector_item = 0;
                                  n_triangle_polygon_vector_item < current_triangle_polygon_vector_size;
                                ++n_triangle_polygon_vector_item)
                {
                    _mesh_polygon* current_triangle_polygon_ptr = NULL;

                    if (!system_resizable_vector_get_element_at(current_triangle_polygon_vector,
                                                                n_triangle_polygon_vector_item,
                                                               &current_triangle_polygon_ptr) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not find triangle polygon descriptor");

                        continue;
                    }

                    if (current_triangle_polygon_ptr->layer_pass_ptr == layer_pass_ptr     &&
                        current_triangle_polygon_ptr->layer_ptr      == layer_ptr          &&
                        current_triangle_polygon_ptr->triangle_index == n_current_triangle)
                    {
                        current_triangle_polygon_normal = current_triangle_polygon_ptr->polygon_normal;

                        break;
                    }
                } /* for (all triangle polygon vector items) */

                ASSERT_DEBUG_SYNC(current_triangle_polygon_normal != NULL,
                                  "Current triangle polygon normal is NULL");

                /* Iterate over all triangle vertices */
                for (unsigned int n_vertex = 0;
                                  n_vertex < n_vertices;
                                ++n_vertex)
                {
                    const unsigned int      current_vertex_index      = vertices_indices[n_vertex];
                    const float*            triangle_vertex_data      = vertices_data   [n_vertex];
                    float                   triangle_vertex_normal[3] =
                    {
                        current_triangle_polygon_normal[0],
                        current_triangle_polygon_normal[1],
                        current_triangle_polygon_normal[2]
                    };
                    system_resizable_vector triangle_vertex_polygon_vector = NULL;

                    if (layer_pass_ptr->smoothing_angle > 0.0f)
                    {
                        /* Find all polygons that share the vertex */
                        unsigned int n_triangle_vertex_polygon_vector_items = 0;

                        if (!system_bst_get(vertex_to_polygon_vector_bst,
                                            (system_bst_key)    (vertices_data[n_vertex]),
                                            (system_bst_value*) &triangle_vertex_polygon_vector) )
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Could not find a polygon vector for input vertex");

                            continue;
                        }

                        n_triangle_vertex_polygon_vector_items = system_resizable_vector_get_amount_of_elements(triangle_vertex_polygon_vector);

                        for (unsigned int n_triangle_vertex_polygon_vector_item = 0;
                                          n_triangle_vertex_polygon_vector_item < n_triangle_vertex_polygon_vector_items;
                                        ++n_triangle_vertex_polygon_vector_item)
                        {
                            _mesh_polygon* triangle_vertex_polygon_ptr = NULL;

                            if (!system_resizable_vector_get_element_at(triangle_vertex_polygon_vector,
                                                                        n_triangle_vertex_polygon_vector_item,
                                                                       &triangle_vertex_polygon_ptr) )
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Could not retrieve triangle vertex polygon item");

                                continue;
                            }

                            /* For the purpose of per-vertex normal calculation, only consider normals
                             * coming from polygons other than the currently processed one */
                            if (triangle_vertex_polygon_ptr->layer_pass_ptr == layer_pass_ptr     &&
                                triangle_vertex_polygon_ptr->layer_ptr      == layer_ptr          &&
                                triangle_vertex_polygon_ptr->triangle_index != n_current_triangle)
                            {
                                float angle = acos(triangle_vertex_polygon_ptr->polygon_normal[0] * current_triangle_polygon_normal[0] +
                                                   triangle_vertex_polygon_ptr->polygon_normal[1] * current_triangle_polygon_normal[1] +
                                                   triangle_vertex_polygon_ptr->polygon_normal[2] * current_triangle_polygon_normal[2]);

                                if (angle <= layer_pass_ptr->smoothing_angle)
                                {
                                    system_math_vector_add3(triangle_vertex_normal,                      /* a */
                                                            triangle_vertex_polygon_ptr->polygon_normal, /* b */
                                                            triangle_vertex_normal);                     /* result */
                                }
                            }
                        } /* for (all triangle vertex polygon vector items) */
                    } /* if (vertex_smoothing_angle >= 0.0f) */

                    system_math_vector_normalize3(triangle_vertex_normal,
                                                  triangle_vertex_normal);

                    /* Store the result per-vertex normal */
                    memcpy(layer_pass_normal_data + 3 /* components */ * layer_pass_index_data[n_current_triangle * 3 + n_vertex],
                           triangle_vertex_normal,
                           sizeof(float) * 3 /* components */);
                } /* for (all triangle vertices) */
            } /* for (all pass-specific triangles) */

            /* Add the data stream as well as the index data
             *
             * We need not worry about adding duplicate normal streams, since mesh_add_layer_*() will throw
             * an assertion failure if the data is already defined.
             *
             * TODO: The mesh_add_layer_pass_index_data() call assumes only one set is ever defined.
             *       FIX IF NEEDED.
             */
            ASSERT_DEBUG_SYNC(system_hash64map_get_amount_of_elements(layer_pass_ptr->set_id_to_unique_set_id[MESH_LAYER_DATA_STREAM_TYPE_VERTICES]) <= 1,
                              "Layer pass uses >= 1 sets which is not supported.");

            mesh_add_layer_data_stream(mesh,
                                       n_layer,
                                       MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                       layer_pass_ptr->n_elements,
                                       layer_pass_normal_data);

            mesh_add_layer_pass_index_data(mesh,
                                           n_layer,
                                           n_layer_pass,
                                           MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                           0, /* set id */
                                           layer_pass_index_data,
                                           0, /* min_index */
                                           layer_pass_ptr->n_elements - 1);

            /* OK, we can release the buffers */
            if (layer_pass_index_data != NULL)
            {
                delete [] layer_pass_index_data;

                layer_pass_index_data = NULL;
            }

            if (layer_pass_normal_data != NULL)
            {
                delete [] layer_pass_normal_data;

                layer_pass_normal_data = NULL;
            }
        } /* for (all layer passes) */
    } /* for (all layers) */

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();

    if (allocated_polygon_vectors != NULL)
    {
        system_resizable_vector vector_item = NULL;

        while (system_resizable_vector_pop(allocated_polygon_vectors,
                                          &vector_item) )
        {
            system_resizable_vector_release(vector_item);

            vector_item = NULL;
        }

        system_resizable_vector_release(allocated_polygon_vectors);
        allocated_polygon_vectors = NULL;
    }

    if (mesh_polygon_resource_pool != NULL)
    {
        system_resource_pool_release(mesh_polygon_resource_pool);
    }

    if (vec3_resource_pool != NULL)
    {
        system_resource_pool_release(vec3_resource_pool);
    }
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t mesh_get_amount_of_layer_passes(__in __notnull mesh          instance,
                                                            __in           mesh_layer_id layer_id)
{
    _mesh*       mesh_ptr       = (_mesh*) instance;
    _mesh_layer* mesh_layer_ptr = NULL;
    uint32_t     result         = 0;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &mesh_layer_ptr) )
    {
        result = system_resizable_vector_get_amount_of_elements(mesh_layer_ptr->passes);
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve layer with id [%d]", layer_id);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_data(__in      __notnull mesh                        mesh,
                                                        __in      __notnull mesh_layer_id               layer_id,
                                                        __in      __notnull mesh_layer_data_stream_type type,
                                                        __out_opt __notnull unsigned int*               out_n_items_ptr,
                                                        __out_opt __notnull const void**                out_data_ptr)
{
    _mesh_layer* layer_ptr = NULL;
    _mesh*       mesh_ptr  = (_mesh*) mesh;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &layer_ptr) )
    {
        _mesh_layer_data_stream* stream_ptr = NULL;

        if (system_hash64map_get(layer_ptr->data_streams, type, &stream_ptr) )
        {
            if (out_n_items_ptr != NULL)
            {
                *out_n_items_ptr = stream_ptr->n_items;
            }

            if (out_data_ptr != NULL)
            {
                *out_data_ptr = stream_ptr->data;
            }
        } /* if (system_hash64map_get(layer_ptr->data_streams, type, &stream_ptr) ) */
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve data stream [%d] for mesh [%s]",
                              type,
                              system_hashed_ansi_string_get_buffer(mesh_ptr->name) );
        }
    } /* if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &layer_ptr) ) */
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve layer [%d]", layer_id);
    }

}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_get_layer_data_stream_property(__in  __notnull mesh                            mesh,
                                                            __in            mesh_layer_data_stream_type     type,
                                                            __in            mesh_layer_data_stream_property property,
                                                            __out __notnull void*                           out_result)
{
    _mesh* mesh_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC(type < MESH_LAYER_DATA_STREAM_TYPE_COUNT,
                      "Unrecognized stream type");

    switch (property)
    {
        case MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET:
        {
            *((uint32_t*) out_result) = mesh_ptr->gl_processed_data_stream_start_offset[type];

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized mesh layer data stream property");
        }
    } /* switch (property) */
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_get_property(__in  __notnull mesh          instance,
                                          __in  __notnull mesh_property property,
                                          __out __notnull void*         result)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   b_result = true;

    switch(property)
    {
        case MESH_PROPERTY_MODEL_AABB_MAX:
        {
            *((float**) result) = mesh_ptr->aabb_max;

            break;
        }

        case MESH_PROPERTY_MODEL_AABB_MIN:
        {
            *((float**) result) = mesh_ptr->aabb_min;

            break;
        }

        case MESH_PROPERTY_CREATION_FLAGS:
        {
            *((mesh_creation_flags*) result) = mesh_ptr->creation_flags;
            break;
        }

        case MESH_PROPERTY_GL_BO_ID:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_storage_initialized, "Cannot query GL BO id - GL storage not initialized");

            *((GLuint*)result) = mesh_ptr->gl_bo_id;
            break;
        }

        case MESH_PROPERTY_GL_INDEX_TYPE:
        {
            *((_mesh_index_type*) result) = mesh_ptr->gl_index_type;

            break;
        }

        case MESH_PROPERTY_GL_PROCESSED_DATA:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data != NULL, "Requested GL processed data is NULL");

            *((void**) result) = mesh_ptr->gl_processed_data;
            break;
        }

        case MESH_PROPERTY_GL_PROCESSED_DATA_SIZE:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data != NULL, "Requested GL processed data size, but the data buffer is NULL");

            *((uint32_t*) result) = mesh_ptr->gl_processed_data_size;
            break;
        }

        case MESH_PROPERTY_GL_STRIDE:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_stride != 0,
                              "Requested stride is 0");

            *((uint32_t*) result) = mesh_ptr->gl_processed_data_stride;

            break;
        }

        case MESH_PROPERTY_GL_TBO:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_storage_initialized, "Cannot query GL TBO id - GL storage not initialized");

            *((ogl_texture*)result) = mesh_ptr->gl_tbo;
            break;
        }

        case MESH_PROPERTY_GL_THREAD_FILL_BUFFERS_CALL_NEEDED:
        {
            *(bool*) result = mesh_ptr->gl_thread_fill_gl_buffers_call_needed;

            break;
        }

        case MESH_PROPERTY_GL_TOTAL_ELEMENTS:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_total_elements != 0,
                              "About to return 0 for MESH_PROPERTY_GL_TOTAL_ELEMENTS query");

            *((uint32_t*) result) = mesh_ptr->gl_processed_data_total_elements;

            break;
        }

        case MESH_PROPERTY_INSTANTIATION_PARENT:
        {
            *(mesh*) result = mesh_ptr->instantiation_parent;

            break;
        }

        case MESH_PROPERTY_MATERIALS:
        {
            *((system_resizable_vector*) result) = mesh_ptr->materials;

            break;
        }

        case MESH_PROPERTY_N_SH_BANDS:
        {
            *((uint32_t*)result) = mesh_ptr->n_sh_bands;

            break;
        }

        case MESH_PROPERTY_N_LAYERS:
        {
            *((uint32_t*) result) = system_resizable_vector_get_amount_of_elements(((_mesh*)instance)->layers);

            break;
        }

        case MESH_PROPERTY_N_GL_UNIQUE_VERTICES:
        {
            *((uint32_t*)result) = mesh_ptr->n_gl_unique_vertices;

            break;
        }

        case MESH_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) result) = mesh_ptr->name;

            break;
        }

        case MESH_PROPERTY_TIMESTAMP_MODIFICATION:
        {
            *((system_timeline_time*)result) = mesh_ptr->timestamp_last_modified;

            break;
        }

        default:
        {
            ASSERT_ALWAYS_SYNC(false, "Unrecognized mesh property [%d]", property);

            b_result = false;
            break;
        }
    }

    return b_result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_get_layer_pass_property(__in  __notnull mesh                instance,
                                                     __in            uint32_t            n_layer,
                                                     __in            uint32_t            n_pass,
                                                     __in  __notnull mesh_layer_property property,
                                                     __out __notnull void*               out_result)
{
    _mesh*       mesh_ptr       = (_mesh*) instance;
    _mesh_layer* mesh_layer_ptr = NULL;
    bool         result         = true;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &mesh_layer_ptr) )
    {
        _mesh_layer_pass* mesh_layer_pass_ptr = NULL;

        if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes,
                                                   n_pass,
                                                  &mesh_layer_pass_ptr) )
        {
            switch(property)
            {
                case MESH_LAYER_PROPERTY_MODEL_AABB_MAX:
                {
                    *((float**) out_result) = mesh_layer_ptr->aabb_max;

                    break;
                }

                case MESH_LAYER_PROPERTY_MODEL_AABB_MIN:
                {
                    *((float**) out_result) = mesh_layer_ptr->aabb_min;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET:
                {
                    *((uint32_t*)out_result) = mesh_layer_pass_ptr->gl_bo_elements_offset;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA:
                {
                    *((uint32_t**)out_result) = mesh_layer_pass_ptr->gl_elements;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX:
                {
                    *((uint32_t*)out_result) = mesh_layer_pass_ptr->gl_elements_max_index;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX:
                {
                    *((uint32_t*)out_result) = mesh_layer_pass_ptr->gl_elements_min_index;

                    break;
                }

                case MESH_LAYER_PROPERTY_MATERIAL:
                {
                    *((mesh_material*)out_result) = mesh_layer_pass_ptr->material;

                    break;
                }

                case MESH_LAYER_PROPERTY_N_ELEMENTS:
                {
                    *((uint32_t*)out_result) = mesh_layer_pass_ptr->n_elements;

                    break;
                }

                case MESH_LAYER_PROPERTY_N_TRIANGLES:
                {
                    *((uint32_t*)out_result) = mesh_layer_pass_ptr->n_elements / 3;

                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false, "Unrecognized mesh layer property");

                    result = false;
                    break;
                }
            } /* switch(property) */
        } /* if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes, n_pass, &mesh_layer_pass_ptr) )*/
        else
        {
            ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer pass descriptor");

            result = false;
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer descriptor");

        result = false;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_load(__in __notnull ogl_context               context,
                                  __in           mesh_creation_flags       flags,
                                  __in __notnull system_hashed_ansi_string full_file_path,
                                  __in __notnull system_hash64map          material_id_to_mesh_material_map,
                                  __in __notnull system_hash64map          mesh_name_to_mesh_map)
{
    /* Create file serializer instance */
    mesh                   result     = NULL;
    system_file_serializer serializer = system_file_serializer_create_for_reading(full_file_path);

    ASSERT_DEBUG_SYNC(serializer != NULL, "Out of memory");
    if (serializer != NULL)
    {
        result = mesh_load_with_serializer(context,
                                           flags,
                                           serializer,
                                           material_id_to_mesh_material_map,
                                           mesh_name_to_mesh_map);

        /* That's it. Release the serializer */
        system_file_serializer_release(serializer);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_load_with_serializer(__in __notnull ogl_context            context,
                                                  __in           mesh_creation_flags    flags,
                                                  __in __notnull system_file_serializer serializer,
                                                  __in __notnull system_hash64map       material_id_to_mesh_material_map,
                                                  __in __notnull system_hash64map       mesh_name_to_mesh_map)
{
    /* Read header */
    char header[16] = {0};
    mesh result     = NULL;

    system_file_serializer_read(serializer,
                               strlen(header_magic),
                               header);

    ASSERT_ALWAYS_SYNC(strcmp(header, header_magic) == 0,
                       "Mesh [%s] is corrupt.",
                       system_hashed_ansi_string_get_buffer(system_file_serializer_get_file_name(serializer) ) );

    if (strcmp(header, header_magic) != 0)
    {
        goto end;
    }

    /* Is this an instantiated mesh? */
    bool                      is_instantiated = false;
    system_hashed_ansi_string mesh_name       = NULL;

    system_file_serializer_read_hashed_ansi_string(serializer,
                                                  &mesh_name);
    system_file_serializer_read                   (serializer,
                                                   sizeof(is_instantiated),
                                                  &is_instantiated);

    result = mesh_create(flags,
                     mesh_name);

    ASSERT_DEBUG_SYNC(result != NULL, "Could not create mesh.");
    if (result == NULL)
    {
        goto end;
    }

    /* Set GL context */
    _mesh* mesh_ptr = (_mesh*) result;

    mesh_ptr->gl_context = context;
    ogl_context_retain(context);

    /* Fork, depending on whether we're dealing with an instantiated mesh,
     * or a parent instance */
    if (!is_instantiated)
    {
        /* Read mesh name */
        float aabb_max[4];
        float aabb_min[4];

        system_file_serializer_read(serializer,
                                    sizeof(aabb_max),
                                    aabb_max);
        system_file_serializer_read(serializer,
                                    sizeof(aabb_min),
                                    aabb_min);

        /* Set AABB box props */
        mesh_set_property(result,
                          MESH_PROPERTY_MODEL_AABB_MAX,
                          aabb_max);
        mesh_set_property(result,
                          MESH_PROPERTY_MODEL_AABB_MIN,
                          aabb_min);

        /* Read serialized GL data */
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->gl_processed_data_size),
                                    &mesh_ptr->gl_processed_data_size);

        ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_size != 0,
                          "Invalid GL processed data size");

        mesh_ptr->gl_processed_data = (float*) new (std::nothrow) unsigned char[mesh_ptr->gl_processed_data_size];
        ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL, "Out of memory");

        system_file_serializer_read(serializer,
                                    mesh_ptr->gl_processed_data_size,
                                    mesh_ptr->gl_processed_data);

        for (mesh_layer_data_stream_type stream_type = (mesh_layer_data_stream_type) 0;
                                         stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                       ++(int&)stream_type)
        {
            system_file_serializer_read(serializer,
                                        sizeof(mesh_ptr->gl_processed_data_stream_start_offset[0]),
                                       &mesh_ptr->gl_processed_data_stream_start_offset[stream_type]);
        }

        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->gl_processed_data_stride),
                                   &mesh_ptr->gl_processed_data_stride);
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->gl_processed_data_total_elements),
                                   &mesh_ptr->gl_processed_data_total_elements);
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->n_gl_unique_vertices),
                                   &mesh_ptr->n_gl_unique_vertices);
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->gl_index_type),
                                   &mesh_ptr->gl_index_type);

        /* Read SH properties */
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->n_sh_bands),
                                   &mesh_ptr->n_sh_bands);
        system_file_serializer_read(serializer,
                                    sizeof(mesh_ptr->n_sh_components),
                                   &mesh_ptr->n_sh_components);

        /* Read layers */
        uint32_t n_layers = 0;

        system_file_serializer_read(serializer,
                                    sizeof(n_layers),
                                   &n_layers);

        /* Read them */
        for (size_t n_layer = 0;
                    n_layer < n_layers;
                  ++n_layer)
        {
            mesh_layer_id current_layer = mesh_add_layer(result);

            /* Read all details of the layer */
            float    layer_aabb_max[4]       = {0};
            float    layer_aabb_min[4]       = {0};
            uint32_t n_layer_passes          = 0;
            uint32_t n_layer_unique_elements = 0;

            system_file_serializer_read(serializer,
                                        sizeof(layer_aabb_max),
                                        layer_aabb_max);
            system_file_serializer_read(serializer,
                                        sizeof(layer_aabb_min),
                                        layer_aabb_min);
            system_file_serializer_read(serializer,
                                        sizeof(n_layer_unique_elements),
                                       &n_layer_unique_elements);
            system_file_serializer_read(serializer,
                                        sizeof(n_layer_passes),
                                       &n_layer_passes);

            mesh_set_layer_property(result,
                                    current_layer,
                                    0, /* n_pass - doesn't matter for this property */
                                    MESH_LAYER_PROPERTY_MODEL_AABB_MAX,
                                    layer_aabb_max);
            mesh_set_layer_property(result,
                                    current_layer,
                                    0, /* n_pass - doesn't matter for this property */
                                    MESH_LAYER_PROPERTY_MODEL_AABB_MIN,
                                    layer_aabb_min);
            mesh_set_layer_property(result,
                                    current_layer,
                                    0, /* n_pass - doesn't matter for this property */
                                    MESH_LAYER_PROPERTY_GL_N_UNIQUE_ELEMENTS,
                                    &n_layer_unique_elements);

            for (uint32_t n_layer_pass = 0;
                          n_layer_pass < n_layer_passes;
                        ++n_layer_pass)
            {
                uint32_t layer_pass_gl_bo_elements_offset = 0;
                uint32_t layer_pass_gl_elements_max_index = 0;
                uint32_t layer_pass_gl_elements_min_index = 0;
                uint32_t layer_pass_material_id           = 0;
                uint32_t layer_pass_n_elements            = 0;
                float    layer_pass_smoothing_angle       = 0.0f;

                /* Read the layer pass properties */
                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_gl_bo_elements_offset),
                                           &layer_pass_gl_bo_elements_offset);
                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_gl_elements_max_index),
                                           &layer_pass_gl_elements_max_index);
                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_gl_elements_min_index),
                                           &layer_pass_gl_elements_min_index);
                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_n_elements),
                                           &layer_pass_n_elements);
                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_smoothing_angle),
                                           &layer_pass_smoothing_angle);

                /* Map the material ID to actual material instance. */
                mesh_material layer_pass_material = NULL;

                system_file_serializer_read(serializer,
                                            sizeof(layer_pass_material_id),
                                           &layer_pass_material_id);

                if (!system_hash64map_get(material_id_to_mesh_material_map,
                                          (system_hash64) layer_pass_material_id,
                                         &layer_pass_material) )
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unrecognized material ID [%d]",
                                       layer_pass_material_id);
                }

                /* Create the pass */
                mesh_layer_pass_id current_pass_id = mesh_add_layer_pass(result,
                                                                         current_layer,
                                                                         layer_pass_material,
                                                                         layer_pass_n_elements);

                /* Update pass properties */
                mesh_set_layer_property(result,
                                        current_layer,
                                        current_pass_id,
                                        MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET,
                                       &layer_pass_gl_bo_elements_offset);
                mesh_set_layer_property(result,
                                        current_layer,
                                        current_pass_id,
                                        MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX,
                                       &layer_pass_gl_elements_max_index);
                mesh_set_layer_property(result,
                                        current_layer,
                                        current_pass_id,
                                        MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX,
                                       &layer_pass_gl_elements_min_index);
                mesh_set_layer_property(result,
                                        current_layer,
                                        current_pass_id,
                                        MESH_LAYER_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                                       &layer_pass_smoothing_angle);

            } /* for (all layer passes) */
        } /* for (size_t n_layer = 0; n_layer < n_layers; ++n_layer) */

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();

        /* Generate GL buffers off the data we loaded */
        mesh_fill_gl_buffers(result,
                             mesh_ptr->gl_context);
    } /* if (!is_mesh_instantiated) */
    else
    {
        system_hashed_ansi_string instantiation_parent_name = NULL;

        /* Read the name of the instantiation parent */
        system_file_serializer_read_hashed_ansi_string(serializer,
                                                      &instantiation_parent_name);

        /* Retrieve the parent mesh */
        mesh instantiation_parent_mesh_gpu = NULL;

        if (!system_hash64map_get(mesh_name_to_mesh_map,
                                  system_hashed_ansi_string_get_hash(instantiation_parent_name),
                                 &instantiation_parent_mesh_gpu) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Instantiation parent [%s] was not recognized",
                              system_hashed_ansi_string_get_buffer(instantiation_parent_name) );
        }
        else
        {
            /* Mark the result mesh as an instanced object */
            mesh_set_as_instantiated(result,
                                     instantiation_parent_mesh_gpu);
        }

    }

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_release_layer_datum(__in __notnull mesh in_mesh)
{
    _mesh*         mesh_ptr = (_mesh*) in_mesh;
    const uint32_t n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        if (!system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                    n_layer,
                                                   &layer_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve mesh layer descriptor");

            continue;
        }

        /* Release data of all defined data streams */
        const uint32_t n_data_streams = system_hash64map_get_amount_of_elements(layer_ptr->data_streams);

        for (uint32_t n_data_stream = 0;
                      n_data_stream < n_data_streams;
                    ++n_data_stream)
        {
            _mesh_layer_data_stream* stream_ptr = NULL;

            if (!system_hash64map_get_element_at(layer_ptr->data_streams,
                                                 n_data_stream,
                                                &stream_ptr,
                                                 NULL) ) /* outHash */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve data stream at index [%d]",
                                  n_data_stream);

                continue;
            }

            if (stream_ptr->data != NULL)
            {
                delete [] stream_ptr->data;

                stream_ptr->data = NULL;
            }
        } /* for (all data streams) */

        /* Release index data defined for all passes */
        const uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

        for (uint32_t n_pass = 0;
                      n_pass < n_passes;
                    ++n_pass)
        {
            _mesh_layer_pass* pass_ptr = NULL;

            if (!system_resizable_vector_get_element_at(layer_ptr->passes,
                                                        n_pass,
                                                       &pass_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve mesh layer pass descriptor at index [%d]",
                                  n_pass);

                continue;
            }

            if (pass_ptr->gl_elements != NULL)
            {
                delete [] pass_ptr->gl_elements;

                pass_ptr->gl_elements = NULL;
            }

            /* Iterate over all index data maps */
            for (mesh_layer_data_stream_type stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                             stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                     ++(int&)stream_type)
            {
                if (pass_ptr->index_data_maps[stream_type] != NULL)
                {
                    const uint32_t n_index_data_maps = system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[stream_type]);

                    for (uint32_t n_index_data_map = 0;
                                  n_index_data_map < n_index_data_maps;
                                ++n_index_data_map)
                    {
                        _mesh_layer_pass_index_data* index_data_map_entry_ptr = NULL;

                        if (!system_hash64map_get_element_at(pass_ptr->index_data_maps[stream_type],
                                                             n_index_data_map,
                                                            &index_data_map_entry_ptr,
                                                             NULL) ) /* outHash */
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Cannot retrieve index data map entry");

                            continue;
                        }

                        if (index_data_map_entry_ptr->data != NULL)
                        {
                            delete [] index_data_map_entry_ptr->data;

                            index_data_map_entry_ptr->data = NULL;
                        }
                    } /* for (all index data map entries) */
                } /* if (index data map is defined for current stream type) */
            } /* for (all stream types) */
        } /* for (all layer passes) */
    } /* for (all mesh layers) */
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_save(__in __notnull mesh                      instance,
                                  __in __notnull system_hashed_ansi_string full_file_path,
                                  __in __notnull system_hash64map          material_name_to_id_map)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    /* Make sure the mesh implementation has been asked to maintain the data buffers! */
    ASSERT_ALWAYS_SYNC(mesh_ptr->creation_flags & MESH_SAVE_SUPPORT, "Cannot save - mesh instance has not been created with save-enabled flag");
    if (!(mesh_ptr->creation_flags & MESH_SAVE_SUPPORT) )
    {
        goto end;
    }

    /* Create file serializer instance */
    system_file_serializer serializer = system_file_serializer_create_for_writing(full_file_path);

    ASSERT_DEBUG_SYNC(serializer != NULL, "Out of memory");
    if (serializer != NULL)
    {
        result = mesh_save_with_serializer(instance,
                                           serializer,
                                           material_name_to_id_map);

        /* That's it. Release the serializer to get the file written down */
        system_file_serializer_release(serializer);
    }

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_save_with_serializer(__in __notnull mesh                   instance,
                                                  __in __notnull system_file_serializer serializer,
                                                  __in __notnull system_hash64map       mesh_material_to_id_map)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    /* Write header */
    system_file_serializer_write(serializer,
                                 strlen(header_magic),
                                 header_magic);

    /* Write general stuff */
    bool is_instantiated = (mesh_ptr->instantiation_parent != NULL);

    system_file_serializer_write_hashed_ansi_string(serializer,
                                                    mesh_ptr->name);
    system_file_serializer_write                   (serializer,
                                                    sizeof(is_instantiated),
                                                   &is_instantiated);

    if (!is_instantiated)
    {
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->aabb_max),
                                     mesh_ptr->aabb_max);
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->aabb_min),
                                     mesh_ptr->aabb_min);

        /* Serialize processed GL data buffer */
        ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data      != NULL &&
                          mesh_ptr->gl_processed_data_size != 0,
                          "Processed GL data is unavailable");

        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->gl_processed_data_size),
                                    &mesh_ptr->gl_processed_data_size);
        system_file_serializer_write(serializer,
                                     mesh_ptr->gl_processed_data_size,
                                     mesh_ptr->gl_processed_data);

        for (mesh_layer_data_stream_type stream_type = (mesh_layer_data_stream_type) 0;
                                         stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                       ++(int&)stream_type)
        {
            system_file_serializer_write(serializer,
                                         sizeof(mesh_ptr->gl_processed_data_stream_start_offset[0]),
                                        &mesh_ptr->gl_processed_data_stream_start_offset[stream_type]);
        }

        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->gl_processed_data_stride),
                                    &mesh_ptr->gl_processed_data_stride);
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->gl_processed_data_total_elements),
                                    &mesh_ptr->gl_processed_data_total_elements);
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->n_gl_unique_vertices),
                                    &mesh_ptr->n_gl_unique_vertices);
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->gl_index_type),
                                    &mesh_ptr->gl_index_type);

        /* Store SH properties */
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->n_sh_bands),
                                    &mesh_ptr->n_sh_bands);
        system_file_serializer_write(serializer,
                                     sizeof(mesh_ptr->n_sh_components),
                                    &mesh_ptr->n_sh_components);

        /* Store layers */
        const uint32_t n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

        system_file_serializer_write(serializer,
                                     sizeof(n_layers),
                                    &n_layers);

        for (size_t n_layer = 0; n_layer < n_layers; ++n_layer)
        {
            _mesh_layer* layer = NULL;

            system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                   n_layer,
                                                  &layer);

            ASSERT_DEBUG_SYNC(layer != NULL, "Could not retrieve layer info");
            if (layer != NULL)
            {
                const uint32_t n_layer_passes = system_resizable_vector_get_amount_of_elements(layer->passes);

                /* Store all details of the layer. Skip any data that's needed to generate the final
                 * GL blob - we already have it!
                 */
                system_file_serializer_write(serializer,
                                             sizeof(layer->aabb_max),
                                             layer->aabb_max);
                system_file_serializer_write(serializer,
                                             sizeof(layer->aabb_min),
                                             layer->aabb_min);
                system_file_serializer_write(serializer,
                                             sizeof(layer->n_gl_unique_elements),
                                             &layer->n_gl_unique_elements);
                system_file_serializer_write(serializer,
                                             sizeof(n_layer_passes),
                                             &n_layer_passes);

                /* Iterate over all layer passes */
                for (uint32_t n_layer_pass = 0;
                              n_layer_pass < n_layer_passes;
                            ++n_layer_pass)
                {
                    _mesh_layer_pass* pass_ptr = NULL;

                    if (system_resizable_vector_get_element_at(layer->passes,
                                                               n_layer_pass,
                                                              &pass_ptr) )
                    {
                        /* General properties */
                        system_file_serializer_write(serializer,
                                                     sizeof(pass_ptr->gl_bo_elements_offset),
                                                    &pass_ptr->gl_bo_elements_offset);
                        system_file_serializer_write(serializer,
                                                     sizeof(pass_ptr->gl_elements_max_index),
                                                    &pass_ptr->gl_elements_max_index);
                        system_file_serializer_write(serializer,
                                                     sizeof(pass_ptr->gl_elements_min_index),
                                                    &pass_ptr->gl_elements_min_index);
                        system_file_serializer_write(serializer,
                                                     sizeof(pass_ptr->n_elements),
                                                    &pass_ptr->n_elements);
                        system_file_serializer_write(serializer,
                                                     sizeof(pass_ptr->smoothing_angle),
                                                    &pass_ptr->smoothing_angle);

                        /* Instead of the string, for improved loading performance, we'll just store an ID using
                         * a map provided by the caller */
                        unsigned int material_id = -1;

                        if (!system_hash64map_get(mesh_material_to_id_map,
                                                  (system_hash64) pass_ptr->material,
                                                  &material_id) )
                        {
                            ASSERT_ALWAYS_SYNC(false,
                                               "Could not map material to an ID!");
                        }
                        else
                        {
                            system_file_serializer_write(serializer,
                                                         sizeof(material_id),
                                                        &material_id);
                        }
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve mesh layer pass descriptor at index [%d]",
                                          n_layer_pass);
                    }
                } /* for (all layer passes) */
            } /* if (layer != NULL) */
        } /* for (size_t n_layer = 0; n_layer < n_layers; ++n_layer) */
    } /* if (is_instantiated) */
    else
    {
        /* Write the name of the instantiation parent */
        system_file_serializer_write_hashed_ansi_string(serializer,
                                                        ((_mesh*) mesh_ptr->instantiation_parent)->name);
    }

    result = true;
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_as_instantiated(__in __notnull mesh mesh_to_modify,
                                                 __in __notnull mesh source_mesh)
{
    _mesh* mesh_to_modify_ptr = (_mesh*) mesh_to_modify;
    _mesh* source_mesh_ptr    = (_mesh*) source_mesh;

    ASSERT_DEBUG_SYNC(mesh_to_modify != NULL,
                      "Target mest is NULL");
    ASSERT_DEBUG_SYNC(mesh_to_modify != source_mesh,
                      "Cannot use the same mesh for instantiation");
    ASSERT_DEBUG_SYNC(mesh_to_modify_ptr->instantiation_parent == NULL,
                      "Mesh to be marked as instantiated is already instantiated!");
    ASSERT_DEBUG_SYNC(source_mesh != NULL,
                      "Source mesh is NULL");
    ASSERT_DEBUG_SYNC(source_mesh_ptr->instantiation_parent == NULL,
                      "Source mesh is instantiated!");

    /* Release layer data */
    _mesh_layer* layer_ptr = NULL;

    while (system_resizable_vector_pop(mesh_to_modify_ptr->layers,
                                      &layer_ptr) )
    {
        _mesh_deinit_mesh_layer(mesh_to_modify_ptr,
                                layer_ptr,
                                true); /* do_full_deinit */

        delete layer_ptr;

        layer_ptr = NULL;
    } /* while (layers are defined) */

    /* Release GPU data that may have already been generated */
    if (mesh_to_modify_ptr->gl_processed_data != NULL)
    {
        delete [] mesh_to_modify_ptr->gl_processed_data;

        mesh_to_modify_ptr->gl_processed_data      = NULL;
        mesh_to_modify_ptr->gl_processed_data_size = 0;
    }

    if (mesh_to_modify_ptr->gl_bo_id != 0)
    {
        ogl_context_request_callback_from_context_thread(mesh_to_modify_ptr->gl_context,
                                                         _mesh_release_renderer_callback,
                                                         mesh_to_modify_ptr);
    }

    /* Mark the mesh as instantiated */
    mesh_to_modify_ptr->instantiation_parent    = source_mesh;
    mesh_to_modify_ptr->timestamp_last_modified = true;

    mesh_retain(source_mesh);
}

#if 0
/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_layer_diffuse_shadowed_transfer_data(__in __notnull mesh          instance,
                                                                      __in           uint32_t      n_sh_bands,
                                                                      __in           sh_components n_sh_components,
                                                                      __in           mesh_layer_id layer_id,
                                                                      __in __notnull float*        vertex_diffuse_shadowed_transfer_data)
{
    _mesh*       mesh_ptr       = (_mesh*) instance;
    _mesh_layer* mesh_layer_ptr = NULL;

    ASSERT_ALWAYS_SYNC( mesh_ptr->n_gl_unique_vertices == 0, "GL buffer's contents already initialized");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &mesh_layer_ptr) )
    {
        bool has_vertex_data_stream = system_hash64map_contains(mesh_layer_ptr->data_streams, (system_hash64) MESH_LAYER_DATA_STREAM_TYPE_VERTICES);

        ASSERT_ALWAYS_SYNC(has_vertex_data_stream,                                                         "Unique vertices data not set for mesh layer [%d]", layer_id);
        ASSERT_ALWAYS_SYNC(mesh_ptr->n_sh_bands      == 0 || mesh_ptr->n_sh_bands      == n_sh_bands,      "Invalid SH band number");
        ASSERT_ALWAYS_SYNC(mesh_ptr->n_sh_components == 0 || mesh_ptr->n_sh_components == n_sh_components, "Invalid SH components");

        if (has_vertex_data_stream && (mesh_ptr->n_sh_bands == 0 || mesh_ptr->n_sh_bands == n_sh_bands) )
        {
            _mesh_layer_data_stream* data_stream_ptr = NULL;
            uint32_t                 n_bytes         = 0;

            system_hash64map_get(mesh_layer_ptr->data_streams, MESH_LAYER_DATA_STREAM_TYPE_VERTICES, &data_stream_ptr);

            n_bytes                                                   = n_sh_bands * n_sh_bands * n_sh_components * data_stream_ptr->n_items * sizeof(float);
            mesh_ptr->n_sh_bands                                      = n_sh_bands;
            mesh_ptr->n_sh_components                                 = n_sh_components;
            mesh_layer_ptr->diffuse_shadowed_transfer_data            = new (std::nothrow) float [n_bytes / sizeof(float)];
            mesh_layer_ptr->diffuse_shadowed_transfer_data_gl_ordered = false;

            ASSERT_ALWAYS_SYNC(mesh_layer_ptr->diffuse_shadowed_transfer_data != NULL, "Out of memory");
            memcpy(mesh_layer_ptr->diffuse_shadowed_transfer_data, vertex_diffuse_shadowed_transfer_data, n_bytes);
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve layer descriptor");
    }
}
#endif

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_layer_property(__in __notnull mesh                instance,
                                                __in           uint32_t            n_layer,
                                                __in           uint32_t            n_pass,
                                                __in __notnull mesh_layer_property property,
                                                __in __notnull const void*         data)
{
    _mesh*       mesh_ptr       = (_mesh*) instance;
    _mesh_layer* mesh_layer_ptr = NULL;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &mesh_layer_ptr) )
    {
        _mesh_layer_pass* mesh_layer_pass_ptr = NULL;

        if (property == MESH_LAYER_PROPERTY_MODEL_AABB_MAX)
        {
            memcpy(mesh_layer_ptr->aabb_max,
                   data,
                   sizeof(mesh_layer_ptr->aabb_max) );

            _mesh_update_aabb(mesh_ptr);
        }
        else
        if (property == MESH_LAYER_PROPERTY_MODEL_AABB_MIN)
        {
            memcpy(mesh_layer_ptr->aabb_min,
                   data,
                   sizeof(mesh_layer_ptr->aabb_min) );

            _mesh_update_aabb(mesh_ptr);
        }
        else
        if (property == MESH_LAYER_PROPERTY_GL_N_UNIQUE_ELEMENTS)
        {
            mesh_layer_ptr->n_gl_unique_elements = *(uint32_t*) data;
        }
        else
        if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes,
                                                   n_pass,
                                                  &mesh_layer_pass_ptr) )
        {
            switch (property)
            {
                case MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET:
                {
                    mesh_layer_pass_ptr->gl_bo_elements_offset = *((uint32_t*) data);

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX:
                {
                    mesh_layer_pass_ptr->gl_elements_max_index = *((uint32_t*) data);

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX:
                {
                    mesh_layer_pass_ptr->gl_elements_min_index = *((uint32_t*) data);

                    break;
                }

                case MESH_LAYER_PROPERTY_VERTEX_SMOOTHING_ANGLE:
                {
                    mesh_layer_pass_ptr->smoothing_angle = *((float*) data);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Invalid layer property requested");
                }
            } /* switch (property) */
        } /* if (layer pass descriptor was retrieved) */
        else
        {
            ASSERT_ALWAYS_SYNC(false, "Could not retrieve layer pass descriptor");
        }

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();
    } /* if (layer descriptor was retrieved) */
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve layer descriptor");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_set_processed_data_stream_start_offset(__in __notnull mesh                        mesh,
                                                                    __in           mesh_layer_data_stream_type stream_type,
                                                                    __in           unsigned int                start_offset)
{
    _mesh* mesh_ptr = (_mesh*) mesh;

    mesh_ptr->gl_processed_data_stream_start_offset[stream_type] = start_offset;

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_property(__in __notnull mesh          instance,
                                          __in           mesh_property property,
                                          __in __notnull void*         value)
{
    ASSERT_DEBUG_SYNC(instance != NULL, "Cannot set propetry - instance is NULL");

    if (instance != NULL)
    {
        _mesh* mesh_ptr = (_mesh*) instance;

        switch(property)
        {
            case MESH_PROPERTY_MODEL_AABB_MAX:
            {
                memcpy(mesh_ptr->aabb_max,
                       value,
                       sizeof(mesh_ptr->aabb_max) );

                break;
            }

            case MESH_PROPERTY_MODEL_AABB_MIN:
            {
                memcpy(mesh_ptr->aabb_min,
                       value,
                       sizeof(mesh_ptr->aabb_min) );

                break;
            }

            case MESH_PROPERTY_GL_STRIDE:
            {
                mesh_ptr->gl_processed_data_stride = *((uint32_t*) value);

                break;
            }

            case MESH_PROPERTY_GL_TOTAL_ELEMENTS:
            {
                mesh_ptr->gl_processed_data_total_elements = *((uint32_t*) value);

                break;
            }

            case MESH_PROPERTY_N_SH_BANDS:
            {
                ASSERT_DEBUG_SYNC(mesh_ptr->n_sh_bands == 0 || mesh_ptr->n_sh_bands == *(uint32_t*) value, "Cannot update number of SH bands");
                if (mesh_ptr->n_sh_bands == 0 || mesh_ptr->n_sh_bands == *(uint32_t*) value)
                {
                    mesh_ptr->n_sh_bands = *(uint32_t*)value;
                }

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false, "Unrecognized mesh property");
            }
        } /* switch(property) */

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_set_single_indexed_representation(__in __notnull                 mesh             mesh,
                                                               __in                           _mesh_index_type index_type,
                                                               __in                           uint32_t         n_blob_data_bytes,
                                                               __in_bcount(n_blob_data_bytes) const void*      blob_data)
{
    _mesh* mesh_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data      == NULL && mesh_ptr->gl_processed_data_size == 0, "Processed GL data already set");
    ASSERT_DEBUG_SYNC(!mesh_ptr->gl_storage_initialized,                                                 "Processed GL data already uploaded");

    mesh_ptr->gl_processed_data_size = n_blob_data_bytes;
    mesh_ptr->gl_processed_data      = (float*) new (std::nothrow) unsigned char[n_blob_data_bytes];
    mesh_ptr->gl_index_type          = index_type;

    ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL, "Out of memory");
    if (mesh_ptr->gl_processed_data != NULL)
    {
        memcpy(mesh_ptr->gl_processed_data, blob_data, n_blob_data_bytes);
    }

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}


