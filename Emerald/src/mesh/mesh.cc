
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
#include "system/system_file_serializer.h"
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
    GLuint   gl_bo_id;

    ogl_texture    gl_tbo; /* refers to TBO created for a BO containing gl_processed data. Uses GL_RGBA32F type for 4 SH bands, GL_RGB32F for 3 SH bands*/

    float*           gl_processed_data;
    uint32_t         gl_processed_data_size; /* tells size of gl_processed_data */
    uint32_t         gl_processed_data_stream_start_offset[MESH_LAYER_DATA_STREAM_TYPE_COUNT];
    uint32_t         gl_processed_data_stride;
    uint32_t         gl_processed_data_total_elements;
    ogl_context      gl_context;
    _mesh_index_type gl_index_type;
    bool             gl_storage_initialized;

    /* Properties */
    float                aabb_max   [4]; /* model-space */
    float                aabb_min   [4]; /* model-space */
    mesh_creation_flags  creation_flags;
    uint32_t             n_sh_bands;      /* can be 0 ! */
    sh_components        n_sh_components; /* can be 0 ! */
    system_timeline_time timestamp_last_modified;

    /* Properties specific to meshes created by merging more than 1 mesh */
    uint32_t* merged_mesh_unique_data_stream_start_offsets[MESH_LAYER_DATA_STREAM_TYPE_COUNT];
    uint32_t  n_merged_meshes;

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

typedef struct
{
    void*                            data;
    mesh_layer_data_stream_data_type data_type;
    bool                             gl_ordered; /* Tells if the data needs not be re-ordered during GL data generation. Only used for SH data seiralization */
    unsigned int                     n_components;
    unsigned int                     n_items;
    unsigned int                     required_bit_alignment;
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
    uint32_t     n_elements;

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

/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(mesh, mesh, _mesh);

/** Forward declarations */
PRIVATE void _mesh_deinit_mesh_layer     (__in __notnull const _mesh* mesh_ptr, __in __notnull _mesh_layer*      layer_ptr, bool do_full_deinit);
PRIVATE void _mesh_deinit_mesh_layer_pass(__in __notnull const _mesh* mesh_ptr, __in __notnull _mesh_layer_pass* pass_ptr,  bool do_full_deinit);
PRIVATE bool _mesh_is_key_uint32_equal   (void* arg1, void* arg2);
PRIVATE bool _mesh_is_key_uint32_lower   (void* arg1, void* arg2);

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

            for (uint32_t n_set = 0; n_set < n_sets; ++n_set)
            {
                uint32_t unique_set_id = 0;

                ASSERT_DEBUG_SYNC(n_element < layer_pass_ptr->n_elements,
                                  "Invalid element index requested");

                if (system_hash64map_get(layer_pass_ptr->index_data_maps        [n_data_stream_type], n_set, &index_data_ptr) &&
                    system_hash64map_get(layer_pass_ptr->set_id_to_unique_set_id[n_data_stream_type], n_set, &unique_set_id) )
                {
                    out_result[n_current_word] = unique_set_id;
                    n_current_word ++;

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

    ASSERT_DEBUG_SYNC(sizeof(void*) == sizeof(uint32_t), "32-bit only logic");

    memcpy(out_result + n_current_word, layer_pass_ptr->material, sizeof(layer_pass_ptr->material) );
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

    for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass)
    {
        const _mesh_layer_pass* pass_ptr = NULL;

        if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) )
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
PRIVATE bool _mesh_is_key_uint32_lower(size_t key_size, void* arg1, void* arg2)
{
    uint32_t* arg1_uint = (uint32_t*) arg1;
    uint32_t* arg2_uint = (uint32_t*) arg2;
    bool      result    = true;

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
PRIVATE void _mesh_deinit_mesh_layer(const _mesh* mesh_ptr, _mesh_layer* layer_ptr, bool do_full_deinit)
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

            while (system_hash64map_get_element_at(layer_ptr->data_streams, 0, &data_stream_ptr, &temp) )
            {
                delete data_stream_ptr;
                data_stream_ptr = NULL;

                system_hash64map_remove(layer_ptr->data_streams, temp);
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

                if (system_resizable_vector_pop(layer_ptr->passes, &pass_ptr) )
                {
                    _mesh_deinit_mesh_layer_pass(mesh_ptr, pass_ptr, do_full_deinit);

                    delete pass_ptr;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not pop mesh layer pass");
                }
            }
        }
    }    
}

/** TODO */
PRIVATE void _mesh_deinit_mesh_layer_pass(__in __notnull const _mesh* mesh_ptr, __in __notnull _mesh_layer_pass* pass_ptr, bool do_full_deinit)
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
PRIVATE void _mesh_fill_gl_buffers_renderer_callback(ogl_context context, void* arg)
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
    entry_points->pGLGenBuffers(1,
                               &mesh_ptr->gl_bo_id);

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "glGenBuffers() failed");

    /* Copy the data to VRAM.
     *
     * Use immutable storage if supported. Mesh data will not be changed
     * throughout the life-time of the object.
     */
    bool is_buffer_storage_supported = false;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_SUPPORT_GL_ARB_BUFFER_STORAGE,
                            &is_buffer_storage_supported);

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

    ASSERT_DEBUG_SYNC(entry_points->pGLGetError() == GL_NO_ERROR, "Could not copy mesh data to VRAM");

    /* Create TBO for the BO */
    if (mesh_ptr->n_sh_bands != 0)
    {
        ASSERT_DEBUG_SYNC(mesh_ptr->n_sh_bands == 3 ||
                          mesh_ptr->n_sh_bands == 4,
                          "Only 3 or 4 SH bands are supported");

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
    mesh_ptr->gl_storage_initialized = true;
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

        if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &layer_ptr) )
        {
            _mesh_layer_pass* pass_ptr = NULL;

            if (system_resizable_vector_get_element_at(layer_ptr->passes, layer_pass_id, &pass_ptr) )
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
                ASSERT_DEBUG_SYNC(false, "Cannot retrieve pass descriptor at index [%d]", layer_pass_id);
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Cannot retrieve layer descriptor at index [%d]", layer_id);
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

    new_mesh->aabb_max[3] = 1;
    new_mesh->aabb_min[3] = 1;
    new_mesh->creation_flags          = flags;
    new_mesh->gl_bo_id                = 0;
    new_mesh->gl_context              = NULL;
    new_mesh->gl_index_type           = MESH_INDEX_TYPE_UNKNOWN;
    new_mesh->gl_processed_data       = NULL;
    new_mesh->gl_processed_data_size  = 0;
    new_mesh->gl_storage_initialized  = false;
    new_mesh->gl_tbo                  = 0;
    new_mesh->layers                  = system_resizable_vector_create(START_LAYERS, sizeof(void*) );
    new_mesh->materials               = system_resizable_vector_create(4 /* capacity */, sizeof(mesh_material) );
    new_mesh->n_gl_unique_vertices    = 0;
    new_mesh->n_merged_meshes         = 0;
    new_mesh->n_sh_bands              = 0;
    new_mesh->n_sh_components         = SH_COMPONENTS_UNDEFINED;
    new_mesh->name                    = name;
    new_mesh->set_id_counter          = 0;
    new_mesh->timestamp_last_modified = system_time_now();

    memset(new_mesh->merged_mesh_unique_data_stream_start_offsets, NULL, sizeof(new_mesh->merged_mesh_unique_data_stream_start_offsets) );

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
    new_mesh_layer_data_stream->gl_ordered             = false;
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

    for (mesh_layer_data_stream_type stream_type  = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                     stream_type != MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                             ++(int&)stream_type)
    {
        new_mesh_layer_pass->index_data_maps        [stream_type] = system_hash64map_create(sizeof(_mesh_layer_pass_index_data*) );
        new_mesh_layer_pass->set_id_to_unique_set_id[stream_type] = system_hash64map_create(sizeof(uint32_t) );
    }
}

#if 0

/** TODO. Rework if needed! This implementation does not support stream data sets. */
PRIVATE void _mesh_merge_meshes(__in __notnull _mesh* new_mesh_ptr, __in uint32_t n_meshes, __in_ecount(n_meshes) __notnull _mesh** meshes)
{
    /* The approach we take here is to merge all layers into one. */
    ASSERT_ALWAYS_SYNC(!new_mesh_ptr->gl_storage_initialized, "Merging not allowed for meshes that have already had GL buffers generated.");
    if (!new_mesh_ptr->gl_storage_initialized)
    {
        /* Iterate through all submitted meshes and count how much space we need to allocate for unique stream data. */
        uint32_t n_total_layers                                                = 0;
        uint32_t n_total_unique_stream_data[MESH_LAYER_DATA_STREAM_TYPE_COUNT] = {0};

        memset(n_total_unique_stream_data, 0, sizeof(n_total_unique_stream_data) );

        for (uint32_t n_mesh = 0; n_mesh < n_meshes; ++n_mesh)
        {
            _mesh*   src_mesh_ptr      = (_mesh*) meshes[n_mesh];
            uint32_t n_src_mesh_layers = system_resizable_vector_get_amount_of_elements(src_mesh_ptr->layers);

            n_total_layers += n_src_mesh_layers;

            for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_src_mesh_layers; ++n_mesh_layer)
            {
                _mesh_layer* src_mesh_layer_ptr = NULL;

                if (system_resizable_vector_get_element_at(src_mesh_ptr->layers, n_mesh_layer, &src_mesh_layer_ptr) )
                {
                    for (unsigned int n_data_stream = 0;
                                      n_data_stream < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                    ++n_data_stream)
                    {
                        _mesh_layer_data_stream* stream_ptr = NULL;

                        if (system_hash64map_get(src_mesh_layer_ptr->data_streams, n_data_stream, &stream_ptr) &&
                            stream_ptr != NULL)
                        {
                            n_total_unique_stream_data[n_data_stream] += stream_ptr->n_items;
                        }
                    }
                } /* if (system_resizable_vector_get_element_at(src_mesh_ptr->layers, &src_mesh_layer_ptr) ) */
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve mesh layer descriptor");
                }
            } /* for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_src_mesh_layers; ++n_mesh_layer) */
        } /* for (uint32_t n_mesh = 0; n_mesh < n_meshes; ++n_mesh) */

        /* Allocate space for the buffers. We want the data merged into one layer */
        typedef struct
        {
            mesh_material    material;
            uint32_t*        updated_stream_data_indices[MESH_LAYER_DATA_STREAM_TYPE_COUNT];
            uint32_t         n_indices;
        } _mesh_layer_pass_merge_data;

        typedef struct
        {
            uint32_t                     n_passes;
            _mesh_layer_pass_merge_data* passes;
        } _mesh_layer_merge_data;

        float*                   new_unique_data[MESH_LAYER_DATA_STREAM_TYPE_COUNT] = {NULL};
        _mesh_layer_merge_data** new_layers_merge_data                              = new (std::nothrow) _mesh_layer_merge_data*[n_total_layers];

        memset(new_unique_data, 0, sizeof(new_unique_data) );

        for (unsigned int n_stream_type = 0;
                          n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                        ++n_stream_type)
        {
            if (n_total_unique_stream_data[n_stream_type] != 0)
            {
                /* TODO: We always assume 3 components are used here. not necessarily true for texcoords, etc. */
                ASSERT_ALWAYS_SYNC(n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_NORMALS ||
                                   n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                   "TODO");

                new_unique_data[n_stream_type] = new (std::nothrow) float [n_total_unique_stream_data[n_stream_type] * 3];
                ASSERT_ALWAYS_SYNC(new_unique_data[n_stream_type] != NULL, "Out of memory");
            }
        }

        /* Allocate buffers to hold merged data streams. While we're at it, verify that all meshes offer the
         * required streams. It's not necessarily a problem if they don't but we should log a warning if
         * this happens.
         */
        for (unsigned int n_stream_type = 0;
                          n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                        ++n_stream_type)
        {
#if _DEBUG
            if (new_unique_data[n_stream_type] != NULL)
            {
                for (uint32_t n_mesh = 0; n_mesh < n_meshes; ++n_mesh)
                {
                    _mesh*   src_mesh_ptr      = (_mesh*) meshes[n_mesh];
                    uint32_t n_src_mesh_layers = system_resizable_vector_get_amount_of_elements(src_mesh_ptr->layers);

                    for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_src_mesh_layers; ++n_mesh_layer)
                    {
                        _mesh_layer* src_mesh_layer_ptr = NULL;

                        if (system_resizable_vector_get_element_at(src_mesh_ptr->layers, n_mesh_layer, &src_mesh_layer_ptr) )
                        {
                            if (!system_hash64map_contains(src_mesh_layer_ptr->data_streams, n_stream_type) )
                            {
                                /* TODO: This code-path was not verified hence the assertion failure */
                                LOG_ERROR("A data stream of type [%d] is needed for merging but mesh [%d] layer [%d] does not offer it!",
                                          n_stream_type,
                                          n_mesh,
                                          n_mesh_layer);

                                ASSERT_DEBUG_SYNC(false, "");
                            }
                        }
                    }
                }
            } /* if (new_unique_data[n_stream_type] != NULL) */
#endif

            if (new_unique_data[n_stream_type] != NULL)
            {
                new_mesh_ptr->merged_mesh_unique_data_stream_start_offsets[n_stream_type] = new (std::nothrow) uint32_t[n_meshes];
            }
        }

        if (new_layers_merge_data != NULL)
        {
            /* Good. Now we can start copying stuff around */
            uint32_t current_data_stream_index[MESH_LAYER_DATA_STREAM_TYPE_COUNT] = {0};
            uint32_t current_layer_index                                          = 0;

            memset(current_data_stream_index, 0, sizeof(current_data_stream_index) );

            for (uint32_t n_mesh = 0; n_mesh < n_meshes; ++n_mesh)
            {
                _mesh*   src_mesh_ptr      = (_mesh*) meshes[n_mesh];
                uint32_t n_src_mesh_layers = system_resizable_vector_get_amount_of_elements(src_mesh_ptr->layers);

                ASSERT_DEBUG_SYNC(n_src_mesh_layers == 1, "TODO");
                for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_src_mesh_layers; ++n_mesh_layer, ++current_layer_index)
                {
                    _mesh_layer* src_mesh_layer_ptr = NULL;

                    if (system_resizable_vector_get_element_at(src_mesh_ptr->layers, n_mesh_layer, &src_mesh_layer_ptr) )
                    {
                        for (unsigned int n_stream_data_type = 0;
                                          n_stream_data_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                        ++n_stream_data_type)
                        {
                            _mesh_layer_data_stream* data_stream_ptr = NULL;

                            system_hash64map_get(src_mesh_layer_ptr->data_streams,
                                                 n_stream_data_type,
                                                 &data_stream_ptr);

                            if (data_stream_ptr != NULL)
                            {
                                memcpy(new_unique_data[n_stream_data_type] + current_data_stream_index[n_stream_data_type] * 3,
                                       data_stream_ptr->data,
                                       sizeof(float) * 3 * data_stream_ptr->n_items);
                            }
                        } /* for (all stream data types) */

                        /* Allocate space for updated layer indices */
                        uint32_t n_layer_passes = system_resizable_vector_get_amount_of_elements(src_mesh_layer_ptr->passes);

                        new_layers_merge_data[current_layer_index] = new (std::nothrow) _mesh_layer_merge_data[n_layer_passes];

                        ASSERT_ALWAYS_SYNC(new_layers_merge_data[current_layer_index] != NULL, "Out of memory");
                        if (new_layers_merge_data[current_layer_index] != NULL)
                        {
                            new_layers_merge_data[current_layer_index]->n_passes = n_layer_passes;
                            new_layers_merge_data[current_layer_index]->passes   = new (std::nothrow) _mesh_layer_pass_merge_data[n_layer_passes];

                            ASSERT_ALWAYS_SYNC(new_layers_merge_data[current_layer_index]->passes != NULL, "Out of memory");
                            if (new_layers_merge_data[current_layer_index]->passes != NULL)
                            {
                                for (uint32_t n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass)
                                {
                                    _mesh_layer_pass* src_mesh_layer_pass_ptr = NULL;

                                    if (system_resizable_vector_get_element_at(src_mesh_layer_ptr->passes, n_layer_pass, &src_mesh_layer_pass_ptr))
                                    {
                                        new_layers_merge_data[current_layer_index]->passes[n_layer_pass].n_indices = src_mesh_layer_pass_ptr->n_elements;

                                        /* Store updated indices */
                                        new_layers_merge_data[current_layer_index]->passes[n_layer_pass].material = src_mesh_layer_pass_ptr->material;

                                        if (src_mesh_layer_pass_ptr->n_elements != 0)
                                        {
                                            for (unsigned int n_data_stream_type  = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                                              n_data_stream_type != MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                                            ++n_data_stream_type)
                                            {
                                                /* Only copy stuff around IF source offers the stream */
                                                if (src_mesh_layer_pass_ptr->index_data[n_data_stream_type] != NULL)
                                                {
                                                    new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type] = new (std::nothrow) uint32_t[src_mesh_layer_pass_ptr->n_elements];

                                                    ASSERT_ALWAYS_SYNC(new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type] != NULL,
                                                                       "Out of memory");

                                                    if (new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type] != NULL)
                                                    {
                                                        uint32_t* dst_ptr = new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type];
                                                        uint32_t* src_ptr = src_mesh_layer_pass_ptr->index_data[n_data_stream_type];

                                                        for (uint32_t index = 0; index < src_mesh_layer_pass_ptr->n_elements; ++index)
                                                        {
                                                            *dst_ptr = *src_ptr + current_data_stream_index[n_data_stream_type];

                                                            dst_ptr++;
                                                            src_ptr++;
                                                        }
                                                    }

                                                }
                                            } /* if (updated normal indices and updated vertex indices buffers ain't NULL) */
                                        } /* if (src_mesh_layer_pass_ptr->n_elements != 0) */
                                        else
                                        {
                                            memset(new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices,
                                                   0,
                                                   sizeof(new_layers_merge_data[current_layer_index]->passes[n_layer_pass].updated_stream_data_indices) );
                                        }
                                    } /* if (system_resizable_vector_get_element_at(src_mesh_layer_ptr->passes, n_layer_pass, &src_mesh_layer_pass_ptr)) */
                                    else
                                    {
                                        ASSERT_DEBUG_SYNC(false, "Could not retrieve mesh layer pass descriptor");
                                    }
                                } /* for (uint32_t n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass) */
                            } /* if (new_layers_merge_data[current_layer_index]->passes != NULL) */
                        } /* if (new_layers_normal_indices[current_layer_index] != NULL) */

                        /* Update normal/vertex indices */
                        for (unsigned int n_data_stream_type = 0;
                                          n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                        ++n_data_stream_type)
                        {
                            if (new_mesh_ptr->merged_mesh_unique_data_stream_start_offsets[n_data_stream_type] != NULL)
                            {
                                new_mesh_ptr->merged_mesh_unique_data_stream_start_offsets[n_data_stream_type][n_mesh] = current_data_stream_index[n_data_stream_type];
                            }
                        }

                        for (unsigned int n_stream_data_type = 0;
                                          n_stream_data_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                        ++n_stream_data_type)
                        {
                            _mesh_layer_data_stream* data_stream_ptr = NULL;

                            system_hash64map_get(src_mesh_layer_ptr->data_streams,
                                                 n_stream_data_type,
                                                 &data_stream_ptr);

                            if (data_stream_ptr != NULL)
                            {
                                current_data_stream_index[n_stream_data_type] += data_stream_ptr->n_items;
                            }
                        }
                    } /* if (system_resizable_vector_get_element_at(src_mesh_ptr->layers, n_mesh_layer, &src_mesh_layer_ptr) ) */
                } /* for (uint32_t n_mesh_layer = 0; n_mesh_layer < n_src_mesh_layers; ++n_mesh_layer) */
            } /* for (uint32_t n_mesh = 0; n_mesh < n_meshes; ++n_mesh) */

            /* Excellent! At this point we've gathered all data needed to create a single-layered mesh representation of all the meshes we analysed. */
            mesh_layer_id new_layer_id = mesh_add_layer((mesh) new_mesh_ptr);

            for (unsigned int n_stream_data_type = 0;
                              n_stream_data_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                            ++n_stream_data_type)
            {
                /* Only add a data stream if at least one item is defined */
                if (n_total_unique_stream_data[n_stream_data_type] != 0)
                {
                    ASSERT_DEBUG_SYNC(n_stream_data_type == MESH_LAYER_DATA_STREAM_TYPE_NORMALS ||
                                      n_stream_data_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES,
                                      "TODO");
                    ASSERT_DEBUG_SYNC(new_unique_data[n_stream_data_type] != NULL,
                                      "Unique data buffer is NULL");

                    mesh_add_layer_data_stream((mesh) new_mesh_ptr,
                                                      new_layer_id,
                                                      (mesh_layer_data_stream_type) n_stream_data_type,
                                                      n_total_unique_stream_data[n_stream_data_type],
                                                      new_unique_data[n_stream_data_type]);
                }
            }

            for (uint32_t n_layer = 0; n_layer < n_total_layers; ++n_layer)
            {
                for (uint32_t n_layer_pass = 0; n_layer_pass < new_layers_merge_data[n_layer]->n_passes; ++n_layer_pass)
                {
                    if (new_layers_merge_data[n_layer]->passes[n_layer_pass].n_indices > 0)
                    {
                        mesh_layer_pass_id new_pass_id = 0;

                        new_pass_id = mesh_add_layer_pass((mesh) new_mesh_ptr,
                                                          0, /* the only valid layer id */
                                                          new_layers_merge_data[n_layer]->passes[n_layer_pass].material,
                                                          new_layers_merge_data[n_layer]->passes[n_layer_pass].n_indices);

                        for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                          n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                        ++n_data_stream_type)
                        {
                            if (new_layers_merge_data[n_layer]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type] != NULL)
                            {
                                mesh_add_layer_pass_index_data((mesh) new_mesh_ptr,
                                                               0, /* the only valid layer id */
                                                               new_pass_id,
                                                               (mesh_layer_data_stream_type) n_data_stream_type,
                                                               new_layers_merge_data[n_layer]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type]);
                            }
                        }
                    }
                } /* for (uint32_t n_layer_pass = 0; n_layer_pass < new_layers_merge_data[n_layer]->n_passes; ++n_layer_pass) */
            } /* for (uint32_t n_layer = 0; n_layer < n_total_layers; ++n_layer) */

            /* Clean up - our job here is done! */
            for (uint32_t n_layer = 0; n_layer < n_total_layers; ++n_layer)
            {
                for (uint32_t n_layer_pass = 0; n_layer_pass < new_layers_merge_data[n_layer]->n_passes; ++n_layer_pass)
                {
                    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                    ++n_data_stream_type)
                    {
                        if (new_layers_merge_data[n_layer]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type] != NULL)
                        {
                            delete [] new_layers_merge_data[n_layer]->passes[n_layer_pass].updated_stream_data_indices[n_data_stream_type];
                        }
                    }
                } /* for (uint32_t_n_layer_pass = 0; n_layer_pass < new_layers_merge_data[n_layer]->n_passes; ++n_layer_pass) */

                delete [] new_layers_merge_data[n_layer]->passes;
            } /* for (uint32_t n_layer = 0; n_layer < n_total_layers; ++n_layer) */

            delete [] new_layers_merge_data;
            new_layers_merge_data = NULL;
        } /* if (new_unique_normals != NULL && new_unique_vertices != NULL) */

        /* Update merge-specific properties of the final mesh */
        new_mesh_ptr->n_merged_meshes = n_meshes;
    } /* if (!new_mesh_ptr->gl_storage_initialized) */
}
#endif

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
        entry_points->pGLDeleteBuffers (1, &mesh->gl_bo_id);
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

    if (mesh->layers != NULL)
    {
        /* Release layers */
        while (system_resizable_vector_get_amount_of_elements(mesh->layers) > 0)
        {
            _mesh_layer* layer = NULL;

            system_resizable_vector_pop(mesh->layers, &layer);

            ASSERT_DEBUG_SYNC(layer != NULL, "Cannot release layer info - layer is NULL");
            if (layer != NULL)
            {
                _mesh_deinit_mesh_layer(mesh, layer, true);

                delete layer;
            }
        }

        system_resizable_vector_release(mesh->layers);
    } /* if (mesh->layers != NULL) */

    /* Release GL stuff */
    if (mesh->gl_storage_initialized)
    {
        /* Release non-VRAM GL stuff */
        if (mesh->gl_processed_data != NULL)
        {
            delete [] mesh->gl_processed_data;

            mesh->gl_processed_data = NULL;
        }

        /* Request rendering thread call-back */
        ogl_context_request_callback_from_context_thread(mesh->gl_context, _mesh_release_renderer_callback, mesh);

        if (mesh->gl_context != NULL)
        {
            ogl_context_release(mesh->gl_context);

            mesh->gl_context = NULL;
        }
    }

    /* Release other helper structures */
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

    ASSERT_ALWAYS_SYNC(!mesh_instance->gl_storage_initialized,    "Cannot add mesh layers after GL storage has been initialized");
    ASSERT_ALWAYS_SYNC( mesh_instance->n_gl_unique_vertices == 0, "GL buffer's contents already initialized");

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

    if (system_resizable_vector_get_element_at(mesh_instance->layers, layer_id, &layer_ptr) )
    {
        /* Make sure the stream has not already been added */
        if (!system_hash64map_contains(layer_ptr->data_streams, type) )
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

                ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT, "TODO");
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
                    memcpy(data_stream_ptr->data, data, data_stream_ptr->n_components * component_size * n_items);

                    /* Store the stream */
                    system_hash64 temp = type;

                    system_hash64map_insert(layer_ptr->data_streams, temp, data_stream_ptr, NULL, NULL);

                    /* Update modification timestamp */
                    mesh_instance->timestamp_last_modified = system_time_now();
                }
            }
            else
            {
                ASSERT_ALWAYS_SYNC(false, "Out of memory");
            }
        }
        else
        {
            /** TODO: This will be hit for multiple texcoord streams and needs to be expanded! */
            ASSERT_ALWAYS_SYNC(false, "Data stream [%x] has already been added to mesh", type);
        }
    } /* if (system_resizable_vector_get_element_at(mesh_instance->layers, layer_id, &layer_ptr) ) */
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve layer at index [%d]", layer_id);
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

    /* Sanity checks */
    ASSERT_ALWAYS_SYNC(!mesh_ptr->gl_storage_initialized,    "Cannot add mesh layer passes after GL storage has been initialized");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &mesh_layer_ptr) &&
        mesh_layer_ptr                 != NULL)
    {
        _mesh_layer_pass* new_layer_pass_ptr = new (std::nothrow) _mesh_layer_pass;

        if (new_layer_pass_ptr != NULL)
        {
            _mesh_init_mesh_layer_pass(new_layer_pass_ptr);

            new_layer_pass_ptr->material   = material;
            new_layer_pass_ptr->n_elements = n_elements;
            result_id                      = system_resizable_vector_get_amount_of_elements(mesh_layer_ptr->passes);

            if (material != NULL)
            {
                /* Cache the material in a helper structure */
                bool               is_material_cached = false;
                const unsigned int n_materials_cached = system_resizable_vector_get_amount_of_elements(mesh_ptr->materials);

                for (unsigned int n_material = 0; n_material < n_materials_cached; ++n_material)
                {
                    mesh_material cached_material = NULL;

                    if (system_resizable_vector_get_element_at(mesh_ptr->materials, n_material, &cached_material) )
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
                    system_resizable_vector_push(mesh_ptr->materials, material);
                }
            } /* if (material != NULL) */

            /* Add new pass */
            system_resizable_vector_push(mesh_layer_ptr->passes, new_layer_pass_ptr);

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

    ASSERT_ALWAYS_SYNC(!mesh_ptr->gl_storage_initialized,    "Cannot add mesh layer passes after GL storage has been initialized");
    ASSERT_ALWAYS_SYNC( mesh_ptr->n_gl_unique_vertices == 0, "GL buffer's contents already initialized");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &mesh_layer_ptr) &&
        mesh_layer_ptr                 != NULL                                              &&
        mesh_ptr->n_gl_unique_vertices == 0)
    {
        _mesh_layer_pass* pass_ptr = NULL;

        if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes, layer_pass_id, &pass_ptr))
        {
            if (!system_hash64map_contains(pass_ptr->index_data_maps[stream_type], set_id) )
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
            ASSERT_ALWAYS_SYNC(false, "Layer pass [%x] was not recognized", layer_pass_id);
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Could not retrieve mesh layer with id [%d]", layer_id);
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

#if 0

/* TODO: Commented out as dependent _mesh_merge_meshes() needs a rework.

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_create_from_meshes(__in                            mesh_creation_flags       flags,
                                                __in                  __notnull system_hashed_ansi_string name,
                                                __in                            uint32_t                  n_meshes,
                                                __in_ecount(n_meshes) __notnull mesh*                     meshes)
{
    _mesh* new_mesh = NULL;
    
    ASSERT_ALWAYS_SYNC(n_meshes >= 1, "Invalid mesh_create_from_meshes() call");
    if (n_meshes >= 1)
    {
        new_mesh = new (std::nothrow) _mesh;

        ASSERT_DEBUG_SYNC(new_mesh != NULL, "Out of memory");
        if (new_mesh != NULL)
        {
            _mesh_init_mesh   (new_mesh, name, flags);
            _mesh_merge_meshes(new_mesh, n_meshes, (_mesh**) meshes);

            new_mesh->n_sh_bands      = ((_mesh**)meshes)[0]->n_sh_bands;
            new_mesh->n_sh_components = ((_mesh**)meshes)[0]->n_sh_components;

            REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_mesh, 
                                                           _mesh_release, 
                                                           OBJECT_TYPE_MESH, 
                                                           system_hashed_ansi_string_create_by_merging_two_strings("\\Meshes\\", system_hashed_ansi_string_get_buffer(name)) );
        }
    }

    return (mesh) new_mesh;
}
#endif

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance)
{
          _mesh*   mesh_ptr              = (_mesh*) instance;
          uint32_t n_layers              = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);
          uint32_t n_indices_used_so_far = 0;
    const uint32_t sh_alignment          = mesh_ptr->n_sh_bands * 4;

    ASSERT_DEBUG_SYNC(mesh_ptr->n_gl_unique_vertices == 0, "Number of unique vertices must be 0");

    /* Reset amount of unique vertices and total number of elements */
    mesh_ptr->gl_processed_data_total_elements = 0;
    mesh_ptr->n_gl_unique_vertices             = 0;

    /* Iterate through layers */
    system_bst datastreams_surfaceid_bst                      = NULL;
    uint32_t   n_datastreams_surfaceid_key_words              = 0;
    uint32_t   n_different_layer_elements                     = 0;
    bool       stream_usage[MESH_LAYER_DATA_STREAM_TYPE_COUNT];

    memset(stream_usage, 0, sizeof(stream_usage) );

    /* Iterate through vertex data streams and calculate axis-aligned bounding boxes for each layer. */
    for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
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
                    const float* vertex_data = (const float*) stream_data_ptr->data + stream_data_ptr->n_components * n_item;

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
    for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            /* 1. Determine how many words BST key needs to consist of */
            uint32_t n_pass_key_words = _mesh_get_total_number_of_sets(layer_ptr) +
                                        sizeof(mesh_material) / sizeof(uint32_t); /* Count in material representation */

            if (n_pass_key_words > n_datastreams_surfaceid_key_words)
            {
                n_datastreams_surfaceid_key_words = n_pass_key_words;
            }
        } /* if (layer_ptr != NULL) */
    } /* for (all layers) */

    /* 2. Allocate space for a BST key */
    uint32_t* key_ptr = new (std::nothrow) uint32_t[2 /* unique set id + index */ * n_datastreams_surfaceid_key_words];

    ASSERT_ALWAYS_SYNC(key_ptr != NULL, "Out of memory");
    memset(key_ptr, 0, 2 /* unique set id + index */ * n_datastreams_surfaceid_key_words);

    /* 3. Build BST. Also count total number of indices the mesh uses */
    for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        _mesh_layer* layer_ptr = NULL;

        system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            const uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass)
            {
                const _mesh_layer_pass* pass_ptr = NULL;

                if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) )
                {
                    for (uint32_t n_element = 0; n_element < pass_ptr->n_elements; ++n_element)
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
                         */
                        ASSERT_DEBUG_SYNC(_mesh_get_total_number_of_sets(layer_ptr) + sizeof(mesh_material) / sizeof(uint32_t) == n_datastreams_surfaceid_key_words,
                                          "Key size mismatch");

                        _mesh_get_index_key(key_ptr,
                                            pass_ptr,
                                            n_element);

                        /* Insert the entry into the binary search tree */
                        if (datastreams_surfaceid_bst == NULL)
                        {
                            datastreams_surfaceid_bst = system_bst_create(sizeof(uint32_t) * n_datastreams_surfaceid_key_words * 2 /* set id + index */,
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

                            if (!system_bst_get(datastreams_surfaceid_bst, (system_bst_key) key_ptr, (system_bst_value*) &temp) )
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

                if (n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS ||
                    n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS)
                {
                    ASSERT_ALWAYS_SYNC(mesh_ptr->n_sh_bands      != 0, "Number of SH bands is 0");
                    ASSERT_ALWAYS_SYNC(mesh_ptr->n_sh_components != 0, "Number of SH components is 0");
                }

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
                                      "No index data set defined for vertex data.");

                    /* No data sets for given data stream type */
                    n_sets = 1;
                }

                ASSERT_DEBUG_SYNC(stream_data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT, "TODO: Unsupported stream data type");
#if 0
                mesh_ptr->gl_bo_unique_data_stream_data[n_data_stream_type]  = new (std::nothrow) float[n_different_layer_elements * stream_n_components];
#endif

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

        mesh_ptr->gl_processed_data_size = mesh_ptr->gl_processed_data_size + mesh_ptr->gl_processed_data_total_elements * index_size;

        /* Allocate space for GL data */
        mesh_ptr->gl_processed_data = new (std::nothrow) float[mesh_ptr->gl_processed_data_size];

        ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL, "Out of memory");
        if (mesh_ptr->gl_processed_data != NULL)
        {
            /* Insert indices data */
            void* elements_traveller_ptr = (uint32_t*) ((char*) mesh_ptr->gl_processed_data + (mesh_ptr->gl_processed_data_size - mesh_ptr->gl_processed_data_total_elements * index_size));

            for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
            {
                _mesh_layer* layer_ptr = NULL;

                system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

                ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
                if (layer_ptr != NULL)
                {
                    uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

                    for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass)
                    {
                        uint32_t          max_index = -1;
                        uint32_t          min_index = -1;
                        _mesh_layer_pass* pass_ptr  = NULL;

                        if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) && pass_ptr != NULL)
                        {
                            pass_ptr->gl_bo_elements_offset = (char*) elements_traveller_ptr - (char*) mesh_ptr->gl_processed_data;

                            for (uint32_t n_element = 0; n_element < pass_ptr->n_elements; ++n_element)
                            {
                                uint32_t final_index = 0;

                                /* Form the key */
                                _mesh_get_index_key(key_ptr, pass_ptr, n_element);

                                /* Find the final index */
                                if (system_bst_get(datastreams_surfaceid_bst, (system_bst_key) key_ptr, (system_bst_value*) &final_index) )
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

                                        system_hash64map_get(layer_ptr->data_streams, n_data_stream_type, &data_stream_ptr);

                                        if (data_stream_ptr != NULL)
                                        {
                                            ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT, "TODO");

                                            uint32_t stream_n_components = 0;
                                            uint32_t stream_n_sets       = 0;

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
                                                /* Re-use vertex index data set */
                                                stream_n_sets = 1;
                                            }

                                            for (unsigned int n_set = 0; n_set < stream_n_sets; ++n_set)
                                            {
                                                unsigned int pass_index_data = 0;

                                                if (!data_stream_ptr->gl_ordered)
                                                {
                                                    mesh_layer_data_stream_type actual_stream_type = (mesh_layer_data_stream_type) n_data_stream_type;

                                                    if (system_hash64map_get_amount_of_elements(pass_ptr->index_data_maps[actual_stream_type]) == 0)
                                                    {
                                                        actual_stream_type = MESH_LAYER_DATA_STREAM_TYPE_VERTICES;
                                                    }

                                                    if (pass_ptr->index_data_maps[actual_stream_type] != NULL)
                                                    {
                                                        _mesh_layer_pass_index_data* set_index_data = NULL;

                                                        if (!system_hash64map_get(pass_ptr->index_data_maps[actual_stream_type], n_set, &set_index_data) )
                                                        {
                                                            ASSERT_DEBUG_SYNC(false, "Could not retrieve set's index data");
                                                        }
                                                        else
                                                        {
                                                            unsigned int src_index = _mesh_get_source_index_from_index_key(key_ptr,
                                                                                                                           pass_ptr,
                                                                                                                           (mesh_layer_data_stream_type) actual_stream_type,
                                                                                                                           n_set);

                                                            pass_index_data = src_index;
                                                        }
                                                    }
                                                } /* if (!data_stream_ptr->gl_ordered) */

                                                /* We can finally do a memcpy()! */
                                                const uint32_t index                    = (data_stream_ptr->gl_ordered ? final_index : pass_index_data);
                                                const uint32_t n_bytes_for_element_data = sizeof(float) * n_different_layer_elements * stream_n_components;

                                                memcpy((char*) mesh_ptr->gl_processed_data                                         + /* update processed data buffer */
                                                               mesh_ptr->gl_processed_data_stream_start_offset[n_data_stream_type] + /* move to location where the unique data starts for given stream type */
                                                               mesh_ptr->gl_processed_data_stride * n_set * pass_ptr->n_elements   + /* move to current set */
                                                               mesh_ptr->gl_processed_data_stride * final_index,                     /* identify processed index */
                                                       (float*)data_stream_ptr->data                                               +
                                                               n_set * n_bytes_for_element_data                                    + /* move to current set */
                                                               data_stream_ptr->n_components                                       *
                                                               index,
                                                       sizeof(float) * data_stream_ptr->n_components);
                                            }
                                        } /* for (all sets) */
                                    }
                                }
                                else
                                {
                                    ASSERT_ALWAYS_SYNC(false, "Could not retrieve final GL buffer index key");
                                }
                            } /* for (uint32_t n_element = 0; n_element < layer_ptr->n_elements; ++n_element) */

                            /* Store max/min values for the pass */
                            pass_ptr->gl_elements_max_index = max_index;
                            pass_ptr->gl_elements_min_index = min_index;

                            /* Store GPU-side elements representation so that user apps can access it (needed for KDtree intersection) */
                            pass_ptr->gl_elements = new (std::nothrow) uint32_t[pass_ptr->n_elements];

                            ASSERT_ALWAYS_SYNC(pass_ptr->gl_elements != NULL, "Out of memory");
                            if (pass_ptr->gl_elements != NULL)
                            {
                                memcpy(pass_ptr->gl_elements,
                                       (char*)mesh_ptr->gl_processed_data + pass_ptr->gl_bo_elements_offset,
                                       sizeof(uint32_t) * pass_ptr->n_elements);
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
    if (!((mesh_ptr->creation_flags & MESH_SAVE_SUPPORT) || (mesh_ptr->creation_flags & MESH_MERGE_SUPPORT)))
    {
        for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)
        {
            _mesh_layer* layer_ptr = NULL;

            system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

            ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
            if (layer_ptr != NULL)
            {
                _mesh_deinit_mesh_layer(mesh_ptr, layer_ptr, false);
            }
        }
    }

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(__in __notnull mesh instance, __in __notnull ogl_context context)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    ASSERT_DEBUG_SYNC(!mesh_ptr->gl_storage_initialized, "OpenGL buffers already initialized!");
    if (!mesh_ptr->gl_storage_initialized)
    {
        /* Before we block renderer thread, we need to convert the loaded data into single-indexed representation.
         * We do not store it, since it's very likely to be bigger than n-indexed one we use for serialization.
         *
         * User may have already called the function, so make sure we do not reallocate the buffers in such case. */
        if (mesh_ptr->gl_processed_data_size == 0)
        {
            mesh_create_single_indexed_representation(instance);
        }

        /* Request renderer thread call-back */
        mesh_ptr->gl_context = context;
        result               = true;

        ogl_context_retain                              (context);
        ogl_context_request_callback_from_context_thread(context, _mesh_fill_gl_buffers_renderer_callback, mesh_ptr);

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

        system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr);

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL, "Could not retrieve mesh layer descriptor");
        if (layer_ptr != NULL)
        {
            uint32_t n_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass)
            {
                _mesh_layer_pass* pass_ptr = NULL;

                if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) && pass_ptr != NULL)
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
    _mesh*             mesh_ptr = (_mesh*) mesh;
    const unsigned int n_layers = system_resizable_vector_get_amount_of_elements(mesh_ptr->layers);

    ASSERT_DEBUG_SYNC(!mesh_ptr->gl_storage_initialized, "mesh_generate_normal_data() called after GL storage has been initialized.");

    LOG_INFO("Generating normal data for mesh [%s] ...", system_hashed_ansi_string_get_buffer(mesh_ptr->name) );

    /* Iterate over all layers */
    system_resource_pool vec3_resource_pool = system_resource_pool_create(sizeof(float) * 3,
                                                                          4,     /* n_elements_to_preallocate */
                                                                          NULL,  /* init_fn */
                                                                          NULL); /* deinit_fn */

    for (unsigned int n_layer = 0; n_layer < n_layers; ++n_layer)
    {
        system_hash64map index_to_vec3_map = system_hash64map_create(sizeof(void*) );
        _mesh_layer*     layer_ptr         = NULL;

        if (system_resizable_vector_get_element_at(mesh_ptr->layers, n_layer, &layer_ptr) )
        {
            _mesh_layer_data_stream* vertex_data_stream_ptr = NULL;
            const float*             vertex_data_ptr        = NULL;

            if (system_hash64map_contains(layer_ptr->data_streams, MESH_LAYER_DATA_STREAM_TYPE_NORMALS) )
            {
                /* Normal data defined for this layer, move on */
                continue;
            }

            if (!system_hash64map_get(layer_ptr->data_streams, MESH_LAYER_DATA_STREAM_TYPE_VERTICES, &vertex_data_stream_ptr) )
            {
                ASSERT_DEBUG_SYNC(false, "Could not retrieve vertex data stream for mesh layer [%d]", n_layer);

                continue;
            }

            vertex_data_ptr = (const float*) vertex_data_stream_ptr->data;

            /* Retrieve per-vertex normals first.. */
            unsigned int       max_index      = 0;
            const unsigned int n_layer_passes = system_resizable_vector_get_amount_of_elements(layer_ptr->passes);

            for (unsigned int n_layer_pass = 0; n_layer_pass < n_layer_passes; ++n_layer_pass)
            {
                _mesh_layer_pass* layer_pass_ptr = NULL;

                if (system_resizable_vector_get_element_at(layer_ptr->passes, n_layer_pass, &layer_pass_ptr) )
                {
                    const unsigned int n_indices = layer_pass_ptr->n_elements;

                    /* Iterate over triangles.
                     *
                     * TODO: The implementation below is memory-inefficient because it allocates the maximum-size
                     *       buffer for every single layer pass we  process. Large scenes may eat notable amount of
                     *       memory. Optimize if necessary.
                     */
                    _mesh_layer_pass_index_data* index_data_ptr     = NULL;
                    uint32_t                     n_vertex_data_sets = system_hash64map_get_amount_of_elements(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES]);

                    if (n_vertex_data_sets != 1)
                    {
                        ASSERT_DEBUG_SYNC(n_vertex_data_sets == 1,
                                          "Unsupported number of vertex data sets");

                        goto end;
                    }
                    else
                    {
                        system_hash64map_get_element_at(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                                        0,     /* set id */
                                                        &index_data_ptr,
                                                        NULL); /* pOutHash */
                    }

                    for (unsigned int n_index = 0; n_index < n_indices; n_index += 3)
                    {
                        const uint32_t vertex1_index = index_data_ptr->data[n_index + 0];
                        const uint32_t vertex2_index = index_data_ptr->data[n_index + 1];
                        const uint32_t vertex3_index = index_data_ptr->data[n_index + 2];
                        const float*   vertex1_data =  vertex_data_ptr + 3 /* components */ * vertex1_index;
                        const float*   vertex2_data =  vertex_data_ptr + 3 /* components */ * vertex2_index;
                        const float*   vertex3_data =  vertex_data_ptr + 3 /* components */ * vertex3_index;

                        /* Calculate the normal vector */
                        float b_minus_a           [3];
                        float c_minus_a           [3];
                        float result_normal_vector[3];

                        system_math_vector_minus3(vertex2_data, vertex1_data, b_minus_a);
                        system_math_vector_minus3(vertex3_data, vertex1_data, c_minus_a);
                        system_math_vector_cross3(b_minus_a,    c_minus_a,    result_normal_vector);

                        /* Add the vector to vectors assigned to each point */
                        struct _item
                        {
                            uint32_t     index;
                            const float* data;
                        } items[] =
                        {
                            {vertex1_index, vertex1_data},
                            {vertex2_index, vertex2_data},
                            {vertex3_index, vertex3_data}
                        };
                        const unsigned int n_items = sizeof(items) / sizeof(items[0]);

                        for (unsigned int n_item = 0; n_item < n_items; ++n_item)
                        {
                            const _item& current_item         = items[n_item];
                            float*       assigned_normal_data = NULL;

                            if (!system_hash64map_get(index_to_vec3_map, current_item.index, &assigned_normal_data) )
                            {
                                /* No normal assigned to the point yet */
                                assigned_normal_data = (float*) system_resource_pool_get_from_pool(vec3_resource_pool);

                                memcpy(assigned_normal_data, result_normal_vector, sizeof(float) * 3);

                                system_hash64map_insert(index_to_vec3_map,
                                                        current_item.index,
                                                        assigned_normal_data,
                                                        NULL,  /* on_remove_callback_fn */
                                                        NULL); /* on_remove_callback arg */
                            }
                            else
                            {
                                system_math_vector_add3(assigned_normal_data, result_normal_vector, assigned_normal_data);
                            }

                            if (current_item.index > max_index)
                            {
                                max_index = current_item.index;
                            }
                        } /* for (all three triangle vertices) */
                    } /* for (all triangle indices) */
                } /* if (layer pass descriptor exists) */
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve layer pass descriptor for mesh layer [%d]", n_layer);
                }
            } /* for (all layer passes) */

            /* Normalize normal vectors we have computed and assign them to relevant vertices */
            const unsigned int n_index_to_vec3_map_indices = system_hash64map_get_amount_of_elements(index_to_vec3_map);

            ASSERT_DEBUG_SYNC(n_index_to_vec3_map_indices != 0,
                              "Index->vec3 map is empty");

            float* normal_data_stream = new (std::nothrow) float[(max_index + 1) * 3 /* components */];
            ASSERT_ALWAYS_SYNC(normal_data_stream != NULL, "Out of memory");

            for (unsigned int n_current_index = 0;
                              n_current_index < n_index_to_vec3_map_indices;
                            ++n_current_index)
            {
                system_hash64 current_index = 0;
                float*        index_normal  = NULL;

                if (system_hash64map_get_element_at(index_to_vec3_map,
                                                    n_current_index,
                                                    &index_normal,
                                                    &current_index) )
                {
                    /* Normalize the vector */
                    system_math_vector_normalize3(index_normal, index_normal);

                    /* Store it in the stream */
                    memcpy(normal_data_stream + 3 * current_index,
                           index_normal,
                           sizeof(float) * 3);
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false, "Could not retrieve summed normal for index [%d]", current_index);
                }
            }

            /* Add the data stream */
            mesh_add_layer_data_stream(mesh,
                                       n_layer,
                                       MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                       max_index + 1,
                                       normal_data_stream);

            /* Release the buffer */
            delete [] normal_data_stream;
            normal_data_stream = NULL;
        } /* if (layer descriptor exists) */
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve descriptor of layer [%d]", n_layer);
        }

        if (index_to_vec3_map != NULL)
        {
            system_hash64map_release(index_to_vec3_map);
        }
    } /* for (all layers) */

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();

end:
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
PUBLIC EMERALD_API void mesh_get_merged_meshes_unique_data_stream_offset(__in  __notnull mesh                        mesh_instance,
                                                                         __in            mesh_layer_data_stream_type stream_type,
                                                                         __out __notnull uint32_t**                  out_result)
{
    _mesh* mesh_ptr = (_mesh*) mesh_instance;

    *out_result = mesh_ptr->merged_mesh_unique_data_stream_start_offsets[stream_type];
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
        case MESH_PROPERTY_AABB_MAX:
        {
            *((float**) result) = mesh_ptr->aabb_max;

            break;
        }

        case MESH_PROPERTY_AABB_MIN:
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

        case MESH_PROPERTY_GL_TOTAL_ELEMENTS:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_total_elements != 0,
                              "About to return 0 for MESH_PROPERTY_GL_TOTAL_ELEMENTS query");

            *((uint32_t*) result) = mesh_ptr->gl_processed_data_total_elements;

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

        case MESH_PROPERTY_N_MESHES_MERGED:
        {
            *((uint32_t*)result) = mesh_ptr->n_merged_meshes;

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
    bool         result         = false;

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
                case MESH_LAYER_PROPERTY_AABB_MAX:
                {
                    *((float**) out_result) = mesh_layer_ptr->aabb_max;

                    break;
                }

                case MESH_LAYER_PROPERTY_AABB_MIN:
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

                    break;
                }
            } /* switch(property) */
        } /* if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes, n_pass, &mesh_layer_pass_ptr) )*/
        else
        {
            ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer pass descriptor");
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false, "Cannot retrieve mesh layer descriptor");
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_load(__in __notnull ogl_context               context,
                                  __in           mesh_creation_flags       flags,
                                  __in __notnull system_hashed_ansi_string full_file_path,
                                  __in __notnull system_hash64map          material_id_to_mesh_material_map)
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
                                           material_id_to_mesh_material_map);

        /* That's it. Release the serializer */
        system_file_serializer_release(serializer);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_load_with_serializer(__in __notnull ogl_context            context,
                                                  __in           mesh_creation_flags    flags,
                                                  __in __notnull system_file_serializer serializer,
                                                  __in __notnull system_hash64map       material_id_to_mesh_material_map)
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

    /* Read mesh name */
    float                     aabb_max[4];
    float                     aabb_min[4];
    system_hashed_ansi_string mesh_name = NULL;

    system_file_serializer_read                   (serializer,
                                                   sizeof(aabb_max),
                                                   aabb_max);
    system_file_serializer_read                   (serializer,
                                                   sizeof(aabb_min),
                                                   aabb_min);
    system_file_serializer_read_hashed_ansi_string(serializer,
                                                  &mesh_name);

    /* Create result instance before we continue */
    result = mesh_create(flags,
                         mesh_name);

    ASSERT_DEBUG_SYNC(result != NULL, "Could not create mesh.");
    if (result != NULL)
    {
        _mesh* mesh_ptr = (_mesh*) result;

        /* Set GL context */
        mesh_ptr->gl_context = context;

        /* Set AABB box props */
        mesh_set_property(result,
                          MESH_PROPERTY_AABB_MAX,
                          aabb_max);
        mesh_set_property(result,
                          MESH_PROPERTY_AABB_MIN,
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
                                    MESH_LAYER_PROPERTY_AABB_MAX,
                                    layer_aabb_max);
            mesh_set_layer_property(result,
                                    current_layer,
                                    0, /* n_pass - doesn't matter for this property */
                                    MESH_LAYER_PROPERTY_AABB_MIN,
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
            } /* for (all layer passes) */
        } /* for (size_t n_layer = 0; n_layer < n_layers; ++n_layer) */

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();

        /* Generate GL buffers off the data we loaded */
        mesh_fill_gl_buffers(result,
                             mesh_ptr->gl_context);
    }

end:
    return result;
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
                                                  __in __notnull system_hash64map       material_name_to_id_map)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    /* Write header */
    system_file_serializer_write(serializer,
                                 strlen(header_magic),
                                 header_magic);

    /* Write general stuff */
    system_file_serializer_write                   (serializer,
                                                    sizeof(mesh_ptr->aabb_max),
                                                    mesh_ptr->aabb_max);
    system_file_serializer_write                   (serializer,
                                                    sizeof(mesh_ptr->aabb_min),
                                                    mesh_ptr->aabb_min);
    system_file_serializer_write_hashed_ansi_string(serializer,
                                                    mesh_ptr->name);

    /* Serialize processed GL data buffer */
    ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data      != NULL &&
                      mesh_ptr->gl_processed_data_size != 0    &&
                      mesh_ptr->gl_storage_initialized,
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

                    /* Material name */
                    system_hashed_ansi_string material_name = mesh_material_get_name(pass_ptr->material);

                    ASSERT_DEBUG_SYNC(material_name != NULL,
                                      "NULL material for layer pass [%d]",
                                      n_layer_pass);

                    /* Instead of the string, for improved loading performance, we'll just store an ID using
                     * a map provided by the caller */
                    void* material_id_ptr = NULL;

                    if (!system_hash64map_get(material_name_to_id_map,
                                              (system_hash64) material_name,
                                              &material_id_ptr) )
                    {
                        ASSERT_ALWAYS_SYNC(false,
                                           "Could not map material name [%s] to an ID!",
                                           system_hashed_ansi_string_get_buffer(material_name) );
                    }
                    else
                    {
                        uint32_t material_id = (uint32_t) material_id_ptr;

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

    result = true;
    return result;
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

        if (property == MESH_LAYER_PROPERTY_AABB_MAX)
        {
            memcpy(mesh_layer_ptr->aabb_max,
                   data,
                   sizeof(mesh_layer_ptr->aabb_max) );

            _mesh_update_aabb(mesh_ptr);
        }
        else
        if (property == MESH_LAYER_PROPERTY_AABB_MIN)
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
            case MESH_PROPERTY_AABB_MAX:
            {
                memcpy(mesh_ptr->aabb_max,
                       value,
                       sizeof(mesh_ptr->aabb_max) );

                break;
            }

            case MESH_PROPERTY_AABB_MIN:
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


