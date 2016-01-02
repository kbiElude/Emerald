
/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "mesh/mesh.h"
#include "mesh/mesh_material.h"
#include "ogl/ogl_context.h"
#include "raGL/raGL_buffer.h"
#include "ral/ral_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_sampler.h"
#include "ral/ral_texture.h"
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
    /* Common properties */
    system_hashed_ansi_string name;
    uint32_t                  n_gl_unique_vertices;
    mesh_type                 type;

    /* Custom mesh-specifics */
    void*                   get_custom_mesh_aabb_proc_user_arg;
    PFNGETMESHAABBPROC      pfn_get_custom_mesh_aabb_proc;
    PFNRENDERCUSTOMMESHPROC pfn_render_custom_mesh_proc;
    void*                   render_custom_mesh_proc_user_arg;

    /* GPU stream mesh-specifics */
    void*                   get_gpu_stream_mesh_aabb_proc_user_arg;
    PFNGETMESHAABBPROC      pfn_get_gpu_stream_mesh_aabb_proc;

    /* GL storage. This BO is maintained by ogl_buffers, so DO NOT release it with glDeleteBuffers().
     *
     * NOTE for offsets: these refer to gl_processed_data, not used if -1.
     *                   Each offset stands for the first entry. Subsequent entries are
     *                   separated with a mesh-specific stride.
     *                   SH MUST be aligned to 32 (as it's used by means of a RGBA32F texture buffer.
     */
    ral_buffer  gl_bo;

    float*           gl_processed_data;
    uint32_t         gl_processed_data_size; /* tells size of gl_processed_data */
    uint32_t         gl_processed_data_stream_start_offset[MESH_LAYER_DATA_STREAM_TYPE_COUNT];
    uint32_t         gl_processed_data_stride;
    uint32_t         gl_processed_data_total_elements;
    ogl_context      gl_context;
    ral_context      ral_context;
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
    system_time          timestamp_last_modified;
    mesh_vertex_ordering vertex_ordering;

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
    mesh_layer_data_stream_data_type data_type;
    mesh_layer_data_stream_source    source;

    /* GPU stream meshes: */
    ral_buffer   bo;
    unsigned int bo_stride;

    /* Regular meshes: */
    unsigned char* data;
    unsigned int   n_components;          /* number of components stored in data per each entry */
    unsigned int   required_bit_alignment;

    /* number of (potentially multi-component!) entries in data. The value can be stored in:
     *
     * - client memory (use *n_items_ptr).
     * - buffer memory (use n_items_bo_id and n_items_bo_start_offset. size is always equal to sizeof(int)
     */
    ral_buffer                    n_items_bo;
    bool                          n_items_bo_requires_memory_barrier;
    unsigned int*                 n_items_ptr;
    mesh_layer_data_stream_source n_items_source;

    ~_mesh_layer_data_stream()
    {
        if (data != NULL)
        {
            delete [] data;

            data = NULL;
        }

        if (n_items_ptr != NULL)
        {
            delete n_items_ptr;

            n_items_ptr = NULL;
        }
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

    /* GPU stream meshes only: */
    mesh_draw_call_arguments draw_call_arguments;
    mesh_draw_call_type      draw_call_type;

    /* Regular meshes only: */
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
REFCOUNT_INSERT_IMPLEMENTATION(mesh,
                               mesh,
                              _mesh);

/** Forward declarations */
PRIVATE mesh_layer_pass_id _mesh_add_layer_pass                          (      mesh                              mesh_instance,
                                                                                mesh_layer_id                     layer_id,
                                                                                mesh_material                     material,
                                                                                uint32_t                          n_elements,
                                                                                mesh_draw_call_type               draw_call_type,
                                                                                const mesh_draw_call_arguments*   draw_call_argument_values_ptr);
PRIVATE void               _mesh_deinit_mesh_layer                       (const _mesh*                            mesh_ptr,
                                                                                _mesh_layer*                      layer_ptr,
                                                                                bool                              do_full_deinit);
PRIVATE void               _mesh_deinit_mesh_layer_pass                  (const _mesh*                            mesh_ptr,
                                                                                _mesh_layer_pass*                 pass_ptr,
                                                                                bool                              do_full_deinit);
PRIVATE void               _mesh_fill_gl_buffers_renderer_callback       (      ogl_context                       context,
                                                                                void*                             arg);
PRIVATE void               _mesh_get_amount_of_stream_data_sets          (      _mesh*                            mesh_ptr,
                                                                                mesh_layer_data_stream_type       stream_type,
                                                                                mesh_layer_id                     layer_id,
                                                                                mesh_layer_pass_id                layer_pass_id,
                                                                                uint32_t*                         out_n_stream_sets_ptr);
PRIVATE void               _mesh_get_index_key                           (      uint32_t*                         out_result_ptr,
                                                                          const _mesh_layer_pass*                 layer_pass_ptr,
                                                                                unsigned int                      n_element);
PRIVATE uint32_t           _mesh_get_source_index_from_index_key         (const uint32_t*                         key_ptr,
                                                                          const _mesh_layer_pass*                 layer_pass_ptr,
                                                                                mesh_layer_data_stream_type       data_stream_type,
                                                                                unsigned int                      set_id);
PRIVATE void               _mesh_get_stream_data_properties              (      _mesh*                            mesh_ptr,
                                                                                mesh_layer_data_stream_type       stream_type,
                                                                                mesh_layer_data_stream_data_type* out_data_type_ptr,
                                                                                uint32_t*                         out_required_bit_alignment_ptr);
PRIVATE uint32_t           _mesh_get_total_number_of_sets                (      _mesh_layer*                      layer_ptr);
PRIVATE void               _mesh_get_total_number_of_stream_sets_for_mesh(      _mesh*                            mesh_ptr,
                                                                                mesh_layer_data_stream_type       stream_type,
                                                                                uint32_t*                         out_n_stream_sets_ptr);
PRIVATE void               _mesh_init_mesh                               (      _mesh*                            new_mesh_ptr,
                                                                                system_hashed_ansi_string         name,
                                                                                mesh_creation_flags               flags);
PRIVATE void               _mesh_init_mesh_layer                         (      _mesh_layer*                      new_mesh_layer_ptr);
PRIVATE void               _mesh_init_mesh_layer_data_stream             (      _mesh_layer_data_stream*          new_mesh_layer_data_stream_ptr,
                                                                                mesh_layer_data_stream_source     source);
PRIVATE void               _mesh_init_mesh_layer_pass                    (      _mesh_layer_pass*                 new_mesh_layer_pass_ptr,
                                                                                mesh_type                         mesh_type);
PRIVATE bool               _mesh_is_key_float_lower                      (      size_t                            key_size,
                                                                                void*                             arg1,
                                                                                void*                             arg2);
PRIVATE bool               _mesh_is_key_float_equal                      (      size_t                            key_size,
                                                                                void*                             arg1,
                                                                                void*                             arg2);
PRIVATE bool               _mesh_is_key_uint32_equal                     (      void*                             arg1,
                                                                                void*                             arg2);
PRIVATE bool               _mesh_is_key_uint32_lower                     (      void*                             arg1,
                                                                                void*                             arg2);
PRIVATE void              _mesh_material_setting_changed                 (const void*                             callback_data,
                                                                                void*                             user_arg);
PRIVATE void              _mesh_release                                  (      void*                             arg);
PRIVATE void              _mesh_release_normals_data                     (      _mesh*                            mesh_ptr);
PRIVATE void              _mesh_release_renderer_callback                (      ogl_context                       context,
                                                                                void*                             arg);
PRIVATE void              _mesh_update_aabb                              (      _mesh*                            mesh_ptr);


/** TODO */
PRIVATE mesh_layer_pass_id _mesh_add_layer_pass(mesh                            mesh_instance,
                                                mesh_layer_id                   layer_id,
                                                mesh_material                   material,
                                                uint32_t                        n_elements,
                                                mesh_draw_call_type             draw_call_type,
                                                const mesh_draw_call_arguments* draw_call_argument_values_ptr)
{
    _mesh_layer*       mesh_layer_ptr = NULL;
    _mesh*             mesh_ptr       = (_mesh*) mesh_instance;
    mesh_layer_pass_id result_id      = -1;

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               layer_id,
                                              &mesh_layer_ptr) &&
        mesh_layer_ptr != NULL)
    {
        _mesh_layer_pass* new_layer_pass_ptr = new (std::nothrow) _mesh_layer_pass;

        ASSERT_ALWAYS_SYNC(new_layer_pass_ptr != NULL,
                           "Out of memory");

        if (new_layer_pass_ptr != NULL)
        {
            _mesh_init_mesh_layer_pass(new_layer_pass_ptr,
                                       mesh_ptr->type);

            if (draw_call_argument_values_ptr != NULL)
            {
                new_layer_pass_ptr->draw_call_arguments = *draw_call_argument_values_ptr;
            }

            new_layer_pass_ptr->draw_call_type = draw_call_type;
            new_layer_pass_ptr->material       = material;
            new_layer_pass_ptr->n_elements     = n_elements;
            result_id                          = 0;

            system_resizable_vector_get_property(mesh_layer_ptr->passes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &result_id);

            mesh_material_retain(material);

            if (material != NULL)
            {
                /* Cache the material in a helper structure */
                bool         is_material_cached = false;
                unsigned int n_materials_cached = 0;

                system_resizable_vector_get_property(mesh_ptr->materials,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_materials_cached);

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
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve cached material at index [%d]",
                                          n_material);
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
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve mesh layer with id [%d]",
                           layer_id);
    }

    return result_id;
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
        if (!do_full_deinit && !(mesh_ptr->creation_flags & MESH_CREATION_FLAGS_KDTREE_GENERATION_SUPPORT) ||
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
            while (true)
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
                    break;
                }
            }
        }
    }
}

/** TODO */
PRIVATE void _mesh_deinit_mesh_layer_pass(const _mesh*            mesh_ptr,
                                                _mesh_layer_pass* pass_ptr,
                                                 bool             do_full_deinit)
{
    for (unsigned int n_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                      n_stream_type ++)
    {
        /* Vertex elements should not be deleted at this point if KD tree generation functionality is required. */
        if (n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES && !do_full_deinit && !(mesh_ptr->creation_flags & MESH_CREATION_FLAGS_KDTREE_GENERATION_SUPPORT) ||
            n_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES &&  do_full_deinit                                                                                ||
            n_stream_type != MESH_LAYER_DATA_STREAM_TYPE_VERTICES)
        {
            if (pass_ptr->index_data_maps[n_stream_type] != NULL)
            {
                _mesh_layer_pass_index_data* index_data = NULL;
                uint32_t                     n_sets     = 0;

                system_hash64map_get_property(pass_ptr->index_data_maps[n_stream_type],
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_sets);

                for (uint32_t n_set = 0;
                              n_set < n_sets;
                            ++n_set)
                {
                    if (system_hash64map_get(pass_ptr->index_data_maps[n_stream_type],
                                             n_set,
                                            &index_data) )
                    {
                        delete index_data;

                        index_data = NULL;
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve set data descriptor");
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

    if (do_full_deinit                &&
        pass_ptr->gl_elements != NULL)
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
    const ogl_context_gl_entrypoints_ext_direct_state_access* dsa_entry_points = NULL;
    const ogl_context_gl_entrypoints*                         entry_points     = NULL;
    const ogl_context_gl_limits*                              limits_ptr       = NULL;
    _mesh*                                                    mesh_ptr         = (_mesh*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);
    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL_EXT_DIRECT_STATE_ACCESS,
                            &dsa_entry_points);

    /* Retrieve BO region to hold the data */
    if (mesh_ptr->gl_bo != NULL)
    {
        ral_context_delete_buffers(mesh_ptr->ral_context,
                                   1, /* n_buffers */
                                  &mesh_ptr->gl_bo);

        mesh_ptr->gl_bo = NULL;
    }

    /* Copy the data to VRAM.
     *
     * NOTE: For normals preview renderer, we need to align
     *       mesh data on SSBO boundary.
     */
    ral_buffer_create_info                bo_create_info;
    ral_buffer_client_sourced_update_info bo_update_info;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_LIMITS,
                            &limits_ptr);

    bo_create_info.mappability_bits = RAL_BUFFER_MAPPABILITY_NONE;
    bo_create_info.property_bits    = RAL_BUFFER_PROPERTY_SPARSE_IF_AVAILABLE_BIT;
    bo_create_info.size             = mesh_ptr->gl_processed_data_size;
    bo_create_info.usage_bits       = RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT | RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT | RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bo_create_info.user_queue_bits  = 0xFFFFFFFF;

    ral_context_create_buffers(mesh_ptr->ral_context,
                               1, /* n_buffers */
                              &bo_create_info,
                              &mesh_ptr->gl_bo);

    bo_update_info.data         = mesh_ptr->gl_processed_data;
    bo_update_info.data_size    = mesh_ptr->gl_processed_data_size;
    bo_update_info.start_offset = 0;

    ral_buffer_set_data_from_client_memory(mesh_ptr->gl_bo,
                                           1, /* n_updates */
                                          &bo_update_info);

    /* Safe to release GL processed data buffer now! */
    if (!(mesh_ptr->creation_flags & MESH_CREATION_FLAGS_SAVE_SUPPORT) )
    {
        delete [] mesh_ptr->gl_processed_data;

        mesh_ptr->gl_processed_data = NULL;
    }

    /* Mark mesh as GL-initialized */
    mesh_ptr->gl_thread_fill_gl_buffers_call_needed = false;
    mesh_ptr->gl_storage_initialized                = true;
}

/** TODO */
PRIVATE void _mesh_get_amount_of_stream_data_sets(_mesh*                      mesh_ptr,
                                                  mesh_layer_data_stream_type stream_type,
                                                  mesh_layer_id               layer_id,
                                                  mesh_layer_pass_id          layer_pass_id,
                                                  uint32_t*                   out_n_stream_sets_ptr)
{
    if (out_n_stream_sets_ptr != NULL)
    {
        _mesh_layer* layer_ptr = NULL;

        *out_n_stream_sets_ptr = 0;

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
                    uint32_t n_sets = 0;

                    system_hash64map_get_property(pass_ptr->index_data_maps[stream_type],
                                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                 &n_sets);

                    if (n_sets > 0)
                    {
                        *out_n_stream_sets_ptr += n_sets;
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
PRIVATE void _mesh_get_index_key(uint32_t*               out_result_ptr,
                                 const _mesh_layer_pass* layer_pass_ptr,
                                 unsigned int            n_element)
{
    unsigned int n_current_word = 0;

    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                    ++n_data_stream_type)
    {
        if (layer_pass_ptr->index_data_maps[n_data_stream_type] != NULL)
        {
            _mesh_layer_pass_index_data* index_data_ptr = NULL;
            uint32_t                     n_sets         = 0;

            system_hash64map_get_property(layer_pass_ptr->index_data_maps[n_data_stream_type],
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_sets);

            for (uint32_t n_set = 0;
                          n_set < n_sets;
                        ++n_set)
            {
                uint32_t unique_set_id = 0;

                ASSERT_DEBUG_SYNC(n_element < layer_pass_ptr->n_elements,
                                  "Invalid element index requested");

                if (system_hash64map_get(layer_pass_ptr->index_data_maps[n_data_stream_type],
                                         n_set,
                                        &index_data_ptr)                                              &&
                    system_hash64map_get(layer_pass_ptr->set_id_to_unique_set_id[n_data_stream_type],
                                         n_set,
                                        &unique_set_id) )
                {
                    out_result_ptr[n_current_word] = unique_set_id;
                    n_current_word ++;

                    ASSERT_DEBUG_SYNC(index_data_ptr->data[n_element] != 0xcdcdcdcd,
                                      "Invalid index data");

                    out_result_ptr[n_current_word] = index_data_ptr->data[n_element];
                    n_current_word ++;
                }
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve index data.");

                    out_result_ptr[n_current_word] = 0;
                    n_current_word++;

                    out_result_ptr[n_current_word] = 0;
                    n_current_word++;
                }
            } /* for (all sets) */
        } /* if (index data available for given data stream type) */
    } /* for (all data stream types) */

    ASSERT_DEBUG_SYNC(sizeof(void*) == sizeof(uint32_t),
                      "32-bit only logic");

    memcpy(out_result_ptr + n_current_word,
           layer_pass_ptr->material,
           sizeof(layer_pass_ptr->material) );
}

/** TODO */
PRIVATE uint32_t _mesh_get_source_index_from_index_key(const uint32_t*             key_ptr,
                                                       const _mesh_layer_pass*     layer_pass_ptr,
                                                       mesh_layer_data_stream_type data_stream_type,
                                                       unsigned int                set_id)
{
    unsigned int n_current_word = 0;
    uint32_t     result         = -1;

    for (unsigned int n_data_stream_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                      n_data_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                    ++n_data_stream_type)
    {
        if (layer_pass_ptr->index_data_maps[n_data_stream_type] != NULL)
        {
            uint32_t n_sets = 0;

            system_hash64map_get_property(layer_pass_ptr->index_data_maps[n_data_stream_type],
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_sets);

            for (uint32_t n_set = 0;
                          n_set < n_sets;
                        ++n_set)
            {
                if (n_set              == set_id &&
                    n_data_stream_type == data_stream_type)
                {
                    result = key_ptr[2 * n_current_word + 1];

                    #ifdef _WIN32
                    {
                        ASSERT_DEBUG_SYNC(result != 0xcdcdcdcd,
                                          "Whoops?");
                    }
                    #endif

                    goto end;
                }

                n_current_word++;
            }
        }
    }

end:
    ASSERT_DEBUG_SYNC(result != -1,
                      "Invalid request");

    return result;
}

/** TODO */
PRIVATE void _mesh_get_stream_data_properties(_mesh*                            mesh_ptr,
                                              mesh_layer_data_stream_type       stream_type,
                                              mesh_layer_data_stream_data_type* out_data_type_ptr,
                                              uint32_t*                         out_required_bit_alignment_ptr)
{
    mesh_layer_data_stream_data_type result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN;
    uint32_t                         result_required_bit_alignment = 0;

    switch (stream_type)
    {
        case MESH_LAYER_DATA_STREAM_TYPE_NORMALS:
        case MESH_LAYER_DATA_STREAM_TYPE_VERTICES:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            result_required_bit_alignment = 0;

            break;
        }

        case MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            result_required_bit_alignment = 0;

            break;
        }

        case MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS:
        case MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS:
        {
            result_data_type              = MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT;
            // result_n_components           = mesh_ptr->n_sh_bands * mesh_ptr->n_sh_bands * mesh_ptr->n_sh_components;
            result_required_bit_alignment = (stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS) ? 96 : 128;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized stream type");
        }
    }

    if (out_data_type_ptr != NULL)
    {
        *out_data_type_ptr = result_data_type;
    }

    if (out_required_bit_alignment_ptr != NULL)
    {
        *out_required_bit_alignment_ptr = result_required_bit_alignment;
    }
}

/** TODO */
PRIVATE uint32_t _mesh_get_total_number_of_sets(_mesh_layer* layer_ptr)
{
    uint32_t n_passes = 0;
    uint32_t result   = 0;

    system_resizable_vector_get_property(layer_ptr->passes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_passes);

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
                    uint32_t n_sets = 0;

                    system_hash64map_get_property(pass_ptr->index_data_maps[n_data_stream_type],
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_sets);

                    result += n_sets;
                }
            } /* for (all data streams) */
        }
    } /* for (all layer passes) */

    return result;
}

/** TODO */
PRIVATE void _mesh_get_total_number_of_stream_sets_for_mesh(_mesh*                      mesh_ptr,
                                                            mesh_layer_data_stream_type stream_type,
                                                            uint32_t*                   out_n_stream_sets_ptr)
{
    if (out_n_stream_sets_ptr != NULL)
    {
        _mesh_layer* layer_ptr = NULL;
        unsigned int n_layers  = 0;

        system_resizable_vector_get_property(mesh_ptr->layers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layers);

        *out_n_stream_sets_ptr = 0;

        for (unsigned int n_layer = 0;
                          n_layer < n_layers;
                        ++n_layer)
        {
            if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                       n_layer,
                                                      &layer_ptr) )
            {
                unsigned int      n_layer_passes = 0;
                _mesh_layer_pass* pass_ptr       = NULL;

                system_resizable_vector_get_property(layer_ptr->passes,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_layer_passes);

                for (unsigned int n_layer_pass = 0;
                                  n_layer_pass < n_layer_passes;
                                ++n_layer_pass)
                {
                    if (system_resizable_vector_get_element_at(layer_ptr->passes,
                                                               n_layer_pass,
                                                              &pass_ptr) )
                    {
                        if (pass_ptr->index_data_maps[stream_type] != NULL)
                        {
                            uint32_t n_sets = 0;

                            system_hash64map_get_property(pass_ptr->index_data_maps[stream_type],
                                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                         &n_sets);

                            if (n_sets > 0)
                            {
                                *out_n_stream_sets_ptr += n_sets;
                            }
                        }
                    }
                    else
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Cannot retrieve pass descriptor at index [%d]",
                                          n_layer_pass);
                    }
                } /* for (all layer passes) */
            } /* if (layer found) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Cannot retrieve layer descriptor at index [%d]",
                                  n_layer);
            }
        } /* for (all layers) */
    } /* if (out_n_stream_sets != NULL) */
}

/** TODO */
PRIVATE void _mesh_init_mesh(_mesh*                    new_mesh_ptr,
                             system_hashed_ansi_string name,
                             mesh_creation_flags       flags)
{
    memset(new_mesh_ptr->aabb_max,
           0,
           sizeof(new_mesh_ptr->aabb_max) );
    memset(new_mesh_ptr->aabb_min,
           0,
           sizeof(new_mesh_ptr->aabb_min) );

    new_mesh_ptr->aabb_max[3]                            = 1;
    new_mesh_ptr->aabb_min[3]                            = 1;
    new_mesh_ptr->creation_flags                         = flags;
    new_mesh_ptr->get_custom_mesh_aabb_proc_user_arg     = NULL;
    new_mesh_ptr->get_gpu_stream_mesh_aabb_proc_user_arg = NULL;
    new_mesh_ptr->gl_bo                                  = NULL;
    new_mesh_ptr->gl_context                             = NULL;
    new_mesh_ptr->gl_index_type                          = MESH_INDEX_TYPE_UNKNOWN;
    new_mesh_ptr->gl_processed_data                      = NULL;
    new_mesh_ptr->gl_processed_data_size                 = 0;
    new_mesh_ptr->gl_storage_initialized                 = false;
    new_mesh_ptr->gl_thread_fill_gl_buffers_call_needed  = false;
    new_mesh_ptr->instantiation_parent                   = NULL;
    new_mesh_ptr->layers                                 = NULL;
    new_mesh_ptr->materials                              = NULL;
    new_mesh_ptr->n_gl_unique_vertices                   = 0;
    new_mesh_ptr->n_sh_bands                             = 0;
    new_mesh_ptr->n_sh_components                        = SH_COMPONENTS_UNDEFINED;
    new_mesh_ptr->name                                   = name;
    new_mesh_ptr->pfn_get_custom_mesh_aabb_proc          = NULL;
    new_mesh_ptr->pfn_get_gpu_stream_mesh_aabb_proc      = NULL;
    new_mesh_ptr->pfn_render_custom_mesh_proc            = NULL;
    new_mesh_ptr->render_custom_mesh_proc_user_arg       = NULL;
    new_mesh_ptr->set_id_counter                         = 0;
    new_mesh_ptr->timestamp_last_modified                = system_time_now();
    new_mesh_ptr->vertex_ordering                        = MESH_VERTEX_ORDERING_CCW;

    for (unsigned int n_stream_type = 0;
                      n_stream_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                      n_stream_type ++)
    {
        new_mesh_ptr->gl_processed_data_stream_start_offset[n_stream_type] = -1;
    }
    new_mesh_ptr->gl_processed_data_stride = -1;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer(_mesh_layer* new_mesh_layer_ptr)
{
    new_mesh_layer_ptr->data_streams         = system_hash64map_create(4 /* capacity */);
    new_mesh_layer_ptr->n_gl_unique_elements = 0;
    new_mesh_layer_ptr->passes_counter       = 1;

    memset(new_mesh_layer_ptr->aabb_max,
           0,
           sizeof(new_mesh_layer_ptr->aabb_max) );
    memset(new_mesh_layer_ptr->aabb_min,
           0,
           sizeof(new_mesh_layer_ptr->aabb_min) );

    new_mesh_layer_ptr->aabb_max[3] = 1;
    new_mesh_layer_ptr->aabb_min[3] = 1;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer_data_stream(_mesh_layer_data_stream*      new_mesh_layer_data_stream_ptr,
                                               mesh_layer_data_stream_source source)
{
    new_mesh_layer_data_stream_ptr->bo                                 = NULL;
    new_mesh_layer_data_stream_ptr->bo_stride                          = 0;
    new_mesh_layer_data_stream_ptr->data                               = NULL;
    new_mesh_layer_data_stream_ptr->data_type                          = MESH_LAYER_DATA_STREAM_DATA_TYPE_UNKNOWN;
    new_mesh_layer_data_stream_ptr->n_components                       = 0;
    new_mesh_layer_data_stream_ptr->n_items_bo                         = NULL;
    new_mesh_layer_data_stream_ptr->n_items_bo_requires_memory_barrier = false;
    new_mesh_layer_data_stream_ptr->n_items_ptr                        = NULL;
    new_mesh_layer_data_stream_ptr->n_items_source                     = MESH_LAYER_DATA_STREAM_SOURCE_UNDEFINED;
    new_mesh_layer_data_stream_ptr->required_bit_alignment             = 0;
    new_mesh_layer_data_stream_ptr->source                             = source;
}

/** TODO */
PRIVATE void _mesh_init_mesh_layer_pass(_mesh_layer_pass* new_mesh_layer_pass_ptr,
                                        mesh_type         mesh_type)
{
    new_mesh_layer_pass_ptr->material = NULL;

    switch (mesh_type)
    {
        case MESH_TYPE_GPU_STREAM:
        {
            new_mesh_layer_pass_ptr->draw_call_type = MESH_DRAW_CALL_TYPE_UNKNOWN;

            memset(&new_mesh_layer_pass_ptr->draw_call_arguments,
                    0,
                    sizeof(new_mesh_layer_pass_ptr->draw_call_arguments) );

            break;
        }

        case MESH_TYPE_REGULAR:
        {
            new_mesh_layer_pass_ptr->gl_bo_elements_offset = 0;
            new_mesh_layer_pass_ptr->gl_elements           = NULL;
            new_mesh_layer_pass_ptr->n_elements            = 0;
            new_mesh_layer_pass_ptr->smoothing_angle       = 0.0f;
            
            for (mesh_layer_data_stream_type stream_type  = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                                             stream_type != MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                                     ++(int&)stream_type)
            {
                new_mesh_layer_pass_ptr->index_data_maps        [stream_type] = system_hash64map_create(sizeof(_mesh_layer_pass_index_data*) );
                new_mesh_layer_pass_ptr->set_id_to_unique_set_id[stream_type] = system_hash64map_create(sizeof(uint32_t) );
            }

            break;
        }
    } /* switch (mesh_type) */

    if (mesh_type == MESH_TYPE_REGULAR)
    {
        
    }
}

/** TODO */
PRIVATE bool _mesh_is_key_float_lower(size_t key_size,
                                      void*  arg1,
                                      void*  arg2)
{
    float*        arg1_float_ptr = (float*) arg1;
    system_hash64 arg1_hash      = system_hash64_calculate((const char*) arg1_float_ptr,
                                                           key_size);
    float*        arg2_float_ptr = (float*) arg2;
    system_hash64 arg2_hash      = system_hash64_calculate((const char*) arg2_float_ptr,
                                                           key_size);
    bool          result         = true;

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
            if (arg2_float_ptr[n_key_item] >= arg2_float_ptr[n_key_item])
            {
                result = false;

                break;
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_float_equal(size_t key_size,
                                      void*  arg1,
                                      void*  arg2)
{
    float* arg1_float_ptr = (float*) arg1;
    float* arg2_float_ptr = (float*) arg2;
    bool   result         = true;

    for (unsigned int n_key_item = 0;
                      n_key_item < key_size / sizeof(float);
                    ++n_key_item)
    {
        if (fabs(arg1_float_ptr[n_key_item] - arg2_float_ptr[n_key_item]) > 1e-5f)
        {
            result = false;

            break;
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_uint32_lower(size_t key_size,
                                       void*  arg1,
                                       void*  arg2)
{
    uint32_t*     arg1_uint_ptr = (uint32_t*) arg1;
    system_hash64 arg1_hash     = system_hash64_calculate( (const char*) arg1,
                                                           key_size);
    uint32_t*     arg2_uint_ptr = (uint32_t*) arg2;
    system_hash64 arg2_hash     = system_hash64_calculate( (const char*) arg2,
                                                           key_size);
    bool          result        = true;

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
            if (!(arg1_uint_ptr[n_key_item] < arg2_uint_ptr[n_key_item]))
            {
                result = false;

                break;
            }
        }
    }

    return result;
}

/** TODO */
PRIVATE bool _mesh_is_key_uint32_equal(size_t key_size,
                                       void*  arg1,
                                       void*  arg2)
{
    uint32_t* arg1_uint_ptr = (uint32_t*) arg1;
    uint32_t* arg2_uint_ptr = (uint32_t*) arg2;
    bool      result        = true;

    for (unsigned int n_key_item = 0;
                      n_key_item < key_size / sizeof(uint32_t);
                    ++n_key_item)
    {
        if (!(arg1_uint_ptr[n_key_item] == arg2_uint_ptr[n_key_item]))
        {
            result = false;

            break;
        }
    }

    return result;
}

/** TODO */
PRIVATE void _mesh_material_setting_changed(const void* callback_data,
                                                  void* user_arg)
{
    _mesh*        mesh_ptr     = (_mesh*)        user_arg;
    float         material_vsa = 0.0f;
    mesh_material src_material = (mesh_material) callback_data;

    mesh_material_get_property(src_material,
                               MESH_MATERIAL_PROPERTY_VERTEX_SMOOTHING_ANGLE,
                              &material_vsa);

    /* Is the Vertex Smoothing Angle setting different we used to generate normals data
     * different from the new value? */
    bool     found_dependant_layers   = false;
    uint32_t n_layers                 = 0;
    bool     needs_normals_data_regen = false;

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

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

        system_resizable_vector_get_property(layer_ptr->passes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layer_passes);

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
                             mesh_ptr->ral_context);
    } /* if (needs_normals_data_regen) */
}

/** TODO */
PRIVATE void _mesh_release(void* arg)
{
    _mesh* mesh_ptr = (_mesh*) arg;

    /* Sign out of material call-backs */
    if (mesh_ptr->materials != NULL)
    {
        uint32_t n_materials = 0;

        system_resizable_vector_get_property(mesh_ptr->materials,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_materials);

        for (uint32_t n_material = 0;
                      n_material < n_materials;
                    ++n_material)
        {
            system_callback_manager callback_manager = NULL;
            mesh_material           material         = NULL;

            if (!system_resizable_vector_get_element_at(mesh_ptr->materials,
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
                                                               mesh_ptr);
        } /* for (all mesh materials) */
    } /* if (mesh_ptr->materials != NULL) */

    /* Carry on with the usual release process */
    if (mesh_ptr->layers != NULL)
    {
        /* Release layers */
        while (true)
        {
            _mesh_layer* layer_ptr = NULL;

            if (!system_resizable_vector_pop(mesh_ptr->layers,
                                            &layer_ptr) )
            {
                break;
            }

            ASSERT_DEBUG_SYNC(layer_ptr != NULL,
                              "Cannot release layer info - layer is NULL");

            if (layer_ptr != NULL)
            {
                _mesh_deinit_mesh_layer(mesh_ptr,
                                        layer_ptr,
                                        true);

                delete layer_ptr;
            }
        }

        system_resizable_vector_release(mesh_ptr->layers);
        mesh_ptr->layers = NULL;
    } /* if (mesh->layers != NULL) */

    /* Release GL stuff */
    if (mesh_ptr->gl_processed_data != NULL)
    {
        delete [] mesh_ptr->gl_processed_data;

        mesh_ptr->gl_processed_data = NULL;
    }

    /* Request rendering thread call-back */
    if (mesh_ptr->gl_context != NULL)
    {
        ogl_context_request_callback_from_context_thread(mesh_ptr->gl_context,
                                                         _mesh_release_renderer_callback,
                                                         mesh_ptr);

        mesh_ptr->gl_context = NULL;
    }

    /* Release other helper structures */
    if (mesh_ptr->instantiation_parent != NULL)
    {
        mesh_release(mesh_ptr->instantiation_parent);

        mesh_ptr->instantiation_parent = NULL;
    }

    if (mesh_ptr->materials != NULL)
    {
        system_resizable_vector_release(mesh_ptr->materials);

        mesh_ptr->materials = NULL;
    }
}

/** TODO */
PRIVATE void _mesh_release_normals_data(_mesh* mesh_ptr)
{
    uint32_t n_layers = 0;

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

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
        system_resizable_vector_get_property(layer_ptr->passes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layer_passes);

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
                uint32_t n_index_data_entries = 0;

                system_hash64map_get_property(pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_NORMALS],
                                              SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                             &n_index_data_entries);

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
PRIVATE void _mesh_release_renderer_callback(ogl_context context,
                                             void*       arg)
{
    const ogl_context_gl_entrypoints* entry_points = NULL;
    _mesh*                            mesh_ptr     = (_mesh*) arg;

    ogl_context_get_property(context,
                             OGL_CONTEXT_PROPERTY_ENTRYPOINTS_GL,
                            &entry_points);

    if (mesh_ptr->gl_bo != NULL)
    {
        ral_context_delete_buffers(mesh_ptr->ral_context,
                                   1, /* n_buffers */
                                  &mesh_ptr->gl_bo);

        mesh_ptr->gl_bo = NULL;
    } /* if (mesh->gl_bo != NULL) */
}

/** TODO */
PRIVATE void _mesh_update_aabb(_mesh* mesh_ptr)
{
    uint32_t n_layers = 0;

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

    for (int n_dimension = 0;
             n_dimension < 3; /* x, y, z */
           ++n_dimension)
    {
        mesh_ptr->aabb_max[n_dimension] = 0;
        mesh_ptr->aabb_min[n_dimension] = 0;
    }

    for (uint32_t n_layer = 0;
                  n_layer < n_layers;
                ++n_layer)
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


/* Please see header for specification */
PUBLIC EMERALD_API mesh_layer_id mesh_add_layer(mesh instance)
{
    _mesh*        mesh_instance_ptr = (_mesh*) instance;
    mesh_layer_id result            = -1;

    ASSERT_DEBUG_SYNC (mesh_instance_ptr->type == MESH_TYPE_REGULAR ||
                       mesh_instance_ptr->type == MESH_TYPE_GPU_STREAM,
                       "Entry-point is only compatible with regular & GPU stream meshes only.");

    if (mesh_instance_ptr->type == MESH_TYPE_GPU_STREAM)
    {
        unsigned int n_layers_created = 0;

        system_resizable_vector_get_property(mesh_instance_ptr->layers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layers_created);

        ASSERT_DEBUG_SYNC(n_layers_created == 0,
                          "Only ONE layer can be created for GPU stream meshes.");
    }

    if (mesh_instance_ptr->type == MESH_TYPE_REGULAR)
    {
        ASSERT_ALWAYS_SYNC(!mesh_instance_ptr->gl_storage_initialized,
                           "Cannot add mesh layers after GL storage has been initialized");
    }

    /* Create new layer */
    _mesh_layer* new_layer_ptr = new (std::nothrow) _mesh_layer;

    ASSERT_DEBUG_SYNC(new_layer_ptr != NULL,
                      "Out of memory");

    if (new_layer_ptr != NULL)
    {
        _mesh_init_mesh_layer(new_layer_ptr);

        new_layer_ptr->passes = system_resizable_vector_create(4);

        ASSERT_DEBUG_SYNC(new_layer_ptr->passes != NULL,
                          "Out of memory");

        /* Add the new layer */
        system_resizable_vector_push(mesh_instance_ptr->layers,
                                     new_layer_ptr);

        system_resizable_vector_get_property(mesh_instance_ptr->layers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &result);
        result--;

        /* Update modification timestamp */
        mesh_instance_ptr->timestamp_last_modified = system_time_now();
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_buffer_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      unsigned int                n_components,
                                                                      ral_buffer                  bo,
                                                                      unsigned int                bo_stride)
{
    _mesh_layer* layer_ptr         = NULL;
    _mesh*       mesh_instance_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC(mesh_instance_ptr->type == MESH_TYPE_GPU_STREAM,
                      "Entry-point is only compatible with GPU stream meshes only.");
    ASSERT_DEBUG_SYNC(n_components >= 1 &&
                      n_components <= 4,
                      "Invalid number of components requested.");

    if (system_resizable_vector_get_element_at(mesh_instance_ptr->layers,
                                               layer_id,
                                              &layer_ptr) )
    {
        /* Make sure the stream has not already been added */
        if (!system_hash64map_contains(layer_ptr->data_streams,
                                       type) )
        {
            _mesh_layer_data_stream* data_stream_ptr = new (std::nothrow) _mesh_layer_data_stream;

            ASSERT_ALWAYS_SYNC(data_stream_ptr != NULL,
                               "Out of memory");

            if (data_stream_ptr != NULL)
            {
                _mesh_init_mesh_layer_data_stream(data_stream_ptr,
                                                  MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY);

                data_stream_ptr->bo           = bo;
                data_stream_ptr->bo_stride    = bo_stride;
                data_stream_ptr->n_components = n_components;

                /* Store the stream */
                ASSERT_DEBUG_SYNC(!system_hash64map_contains(layer_ptr->data_streams,
                                                             (system_hash64) type),
                                  "Stream already defined");

                system_hash64map_insert(layer_ptr->data_streams,
                                        (system_hash64) type,
                                        data_stream_ptr,
                                        NULL,
                                        NULL);

                /* Update modification timestamp */
                mesh_instance_ptr->timestamp_last_modified = system_time_now();
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
PUBLIC EMERALD_API void mesh_add_layer_data_stream_from_client_memory(mesh                        mesh,
                                                                      mesh_layer_id               layer_id,
                                                                      mesh_layer_data_stream_type type,
                                                                      unsigned int                n_components,
                                                                      unsigned int                n_items,
                                                                      const void*                 data)
{
    _mesh_layer* layer_ptr         = NULL;
    _mesh*       mesh_instance_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC (mesh_instance_ptr->type == MESH_TYPE_REGULAR,
                       "Entry-point is only compatible with regular meshes only.");
    ASSERT_DEBUG_SYNC(n_components >= 1 &&
                      n_components <= 4,
                      "Invalid number of components requested.");

    /* Store amount of SH bands if this is a SH data stream. */
    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS ||
        type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS)
    {
        /* We only support RGB spherical harmonics */
        mesh_instance_ptr->n_sh_components = SH_COMPONENTS_RGB;
    }

    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS)
    {
        if (mesh_instance_ptr->n_sh_bands == 0)
        {
            mesh_instance_ptr->n_sh_bands = 3;
        }
        else
        {
            ASSERT_ALWAYS_SYNC(mesh_instance_ptr->n_sh_bands == 3,
                               "Amount of SH bands must be shared between all mesh layers");
        }
    }

    if (type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_4BANDS)
    {
        if (mesh_instance_ptr->n_sh_bands == 0)
        {
            mesh_instance_ptr->n_sh_bands = 4;
        }
        else
        {
            ASSERT_ALWAYS_SYNC(mesh_instance_ptr->n_sh_bands == 4,
                               "Amount of SH bands must be shared between all mesh layers");
        }
    }

    if (system_resizable_vector_get_element_at(mesh_instance_ptr->layers,
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
                _mesh_init_mesh_layer_data_stream(data_stream_ptr,
                                                  MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY);
                _mesh_get_stream_data_properties (mesh_instance_ptr,
                                                  type,
                                                 &data_stream_ptr->data_type,
                                                 &data_stream_ptr->required_bit_alignment);

                ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT,
                                  "TODO");

                unsigned int component_size = (data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT) ? sizeof(float)
                                                                                                                     : 1;

                data_stream_ptr->data           = new (std::nothrow) unsigned char[data_stream_ptr->n_components * component_size * n_items];
                data_stream_ptr->n_components   = n_components;
                data_stream_ptr->n_items_ptr    = new unsigned int(n_items);
                data_stream_ptr->n_items_source = MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY;

                ASSERT_ALWAYS_SYNC(data_stream_ptr->n_items_ptr != NULL,
                                   "Out of memory");

                if (data_stream_ptr->data == NULL)
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Out of memory");

                    delete data_stream_ptr;
                    data_stream_ptr = NULL;
                }
                else
                {
                    memcpy(data_stream_ptr->data,
                           data,
                           data_stream_ptr->n_components * component_size * n_items);

                    /* Store the stream */
                    ASSERT_DEBUG_SYNC(!system_hash64map_contains(layer_ptr->data_streams,
                                                                 (system_hash64) type),
                                      "Streeam already defined");

                    system_hash64map_insert(layer_ptr->data_streams,
                                            (system_hash64) type,
                                            data_stream_ptr,
                                            NULL,
                                            NULL);

                    /* Update modification timestamp */
                    mesh_instance_ptr->timestamp_last_modified = system_time_now();
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
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass_for_gpu_stream_mesh(mesh                            mesh_instance,
                                                                              mesh_layer_id                   layer_id,
                                                                              mesh_material                   material,
                                                                              mesh_draw_call_type             draw_call_type,
                                                                              const mesh_draw_call_arguments* draw_call_argument_values_ptr)

{
    _mesh* mesh_ptr = (_mesh*) mesh_instance;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM,
                      "Entry-point is only compatible with GPU stream meshes only.");

    return _mesh_add_layer_pass(mesh_instance,
                                layer_id,
                                material,
                                0, /* n_elements */
                                draw_call_type,
                                draw_call_argument_values_ptr);
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh_layer_pass_id mesh_add_layer_pass_for_regular_mesh(mesh          mesh_instance,
                                                                           mesh_layer_id layer_id,
                                                                           mesh_material material,
                                                                           uint32_t      n_elements)
{
    _mesh* mesh_ptr = (_mesh*) mesh_instance;

    ASSERT_DEBUG_SYNC(!mesh_ptr->gl_storage_initialized,
                      "Cannot add mesh layer passes after GL storage has been initialized");
    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");
    ASSERT_DEBUG_SYNC(n_elements % 3 == 0,
                      "n_elements is not divisible by three which is invalid");

    return _mesh_add_layer_pass(mesh_instance,
                                layer_id,
                                material,
                                n_elements,
                                MESH_DRAW_CALL_TYPE_UNKNOWN,
                                NULL); /* draw_call_argument_values_ptr */
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_add_layer_pass_index_data_for_regular_mesh(mesh                        mesh_instance,
                                                                        mesh_layer_id               layer_id,
                                                                        mesh_layer_pass_id          layer_pass_id,
                                                                        mesh_layer_data_stream_type stream_type,
                                                                        unsigned int                set_id,
                                                                        const void*                 index_data,
                                                                        unsigned int                min_index,
                                                                        unsigned int                max_index)
{
    /* TODO: This implementation is not currently able to handle multiple texcoord streams. Please improve */
    _mesh*             mesh_ptr       = (_mesh*) mesh_instance;
    _mesh_layer*       mesh_layer_ptr = NULL;
    bool               result         = false;
    mesh_layer_pass_id result_id      = -1;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               layer_id,
                                              &mesh_layer_ptr) &&
        mesh_layer_ptr                 != NULL                 &&
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
                _mesh_layer_pass_index_data* new_data_set_ptr = new (std::nothrow) _mesh_layer_pass_index_data;

                ASSERT_ALWAYS_SYNC(new_data_set_ptr != NULL,
                                   "Out of memory");

                if (new_data_set_ptr != NULL)
                {
                    ASSERT_DEBUG_SYNC(new_data_set_ptr->max_index >= new_data_set_ptr->min_index,
                                      "Invalid min/max mesh indices");

                    new_data_set_ptr->data      = new (std::nothrow) uint32_t[pass_ptr->n_elements];
                    new_data_set_ptr->max_index = max_index;
                    new_data_set_ptr->min_index = min_index;

                    ASSERT_ALWAYS_SYNC(new_data_set_ptr->data != NULL,
                                       "Out of memory");

                    if (new_data_set_ptr->data != NULL)
                    {
                        memcpy(new_data_set_ptr->data,
                               index_data,
                               pass_ptr->n_elements * sizeof(unsigned int) );

                        system_hash64map_insert(pass_ptr->index_data_maps[stream_type],
                                                set_id,
                                                new_data_set_ptr,
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
                                                (void*) (intptr_t) mesh_ptr->set_id_counter,
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
PUBLIC EMERALD_API mesh mesh_create_custom_mesh(PFNRENDERCUSTOMMESHPROC   pfn_render_custom_mesh_proc,
                                                void*                     render_custom_mesh_proc_user_arg,
                                                PFNGETMESHAABBPROC        pfn_get_custom_mesh_aabb_proc,
                                                void*                     get_custom_mesh_aabb_proc_user_arg,
                                                system_hashed_ansi_string name)
{
    _mesh* new_mesh_ptr = new (std::nothrow) _mesh;

    ASSERT_DEBUG_SYNC(new_mesh_ptr != NULL,
                      "Out of memory");

    if (new_mesh_ptr != NULL)
    {
        _mesh_init_mesh(new_mesh_ptr,
                        name,
                        0); /* flags */

        /* Initialize fields specific to custom meshes */
        new_mesh_ptr->get_custom_mesh_aabb_proc_user_arg = get_custom_mesh_aabb_proc_user_arg;
        new_mesh_ptr->pfn_get_custom_mesh_aabb_proc      = pfn_get_custom_mesh_aabb_proc;
        new_mesh_ptr->pfn_render_custom_mesh_proc        = pfn_render_custom_mesh_proc;
        new_mesh_ptr->render_custom_mesh_proc_user_arg   = render_custom_mesh_proc_user_arg;
        new_mesh_ptr->type                               = MESH_TYPE_CUSTOM;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_mesh_ptr,
                                                       _mesh_release,
                                                       OBJECT_TYPE_MESH,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Meshes\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        LOG_INFO("Custom mesh [%s] created.",
                 system_hashed_ansi_string_get_buffer(name) );
    }

    return (mesh) new_mesh_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_create_gpu_stream_mesh(PFNGETMESHAABBPROC        pfn_get_gpu_stream_mesh_aabb_proc,
                                                    void*                     get_gpu_stream_mesh_aabb_user_arg,
                                                    system_hashed_ansi_string name)
{
    _mesh* new_mesh_ptr = (_mesh*) mesh_create_regular_mesh(0, /* flags */
                                                            name);

    ASSERT_ALWAYS_SYNC(new_mesh_ptr != NULL,
                       "Could not create a GPU stream mesh.");

    if (new_mesh_ptr != NULL)
    {
        /* Dirty tricks here! :) */
        new_mesh_ptr->get_gpu_stream_mesh_aabb_proc_user_arg = get_gpu_stream_mesh_aabb_user_arg;
        new_mesh_ptr->pfn_get_gpu_stream_mesh_aabb_proc      = pfn_get_gpu_stream_mesh_aabb_proc;
        new_mesh_ptr->type                                   = MESH_TYPE_GPU_STREAM;
    }

    return (mesh) new_mesh_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_create_regular_mesh(mesh_creation_flags       flags,
                                                 system_hashed_ansi_string name)
{
    _mesh* new_mesh_ptr = new (std::nothrow) _mesh;

    ASSERT_DEBUG_SYNC(new_mesh_ptr != NULL,
                      "Out of memory");

    if (new_mesh_ptr != NULL)
    {
        _mesh_init_mesh(new_mesh_ptr,
                        name,
                        flags);

        /* Initialize fields specific to regular meshes */
        new_mesh_ptr->layers    = system_resizable_vector_create(START_LAYERS);
        new_mesh_ptr->materials = system_resizable_vector_create(4 /* capacity */);
        new_mesh_ptr->type      = MESH_TYPE_REGULAR;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_mesh_ptr,
                                                       _mesh_release,
                                                       OBJECT_TYPE_MESH,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Meshes\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );

        LOG_INFO("Regular mesh [%s] created.",
                 system_hashed_ansi_string_get_buffer(name) );
    }

    return (mesh) new_mesh_ptr;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_create_single_indexed_representation(mesh instance)
{
          _mesh*   mesh_ptr              = (_mesh*) instance;
          uint32_t n_layers              = 0;
          uint32_t n_indices_used_so_far = 0;
    const uint32_t sh_alignment          = mesh_ptr->n_sh_bands * 4;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

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
                ASSERT_DEBUG_SYNC(stream_data_ptr->n_items_source == MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY &&
                                  stream_data_ptr->n_items_ptr   != NULL,
                                  "Invalid source used for storage of number of vertex data stream items.");

                const uint32_t n_items = *stream_data_ptr->n_items_ptr;

                for (unsigned int n_item = 0;
                                  n_item < n_items;
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

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL,
                           "Could not retrieve mesh layer descriptor");

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

        ASSERT_ALWAYS_SYNC(layer_ptr != NULL,
                           "Could not retrieve mesh layer descriptor");

        if (layer_ptr != NULL)
        {
            uint32_t n_passes = 0;

            system_resizable_vector_get_property(layer_ptr->passes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_passes);

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
                                                                          (system_bst_key)    key_ptr,
                                                                          (system_bst_value) &n_different_layer_elements);

                            n_different_layer_elements ++;
                        }
                        else
                        {
                            uint32_t temp = 0;

                            if (!system_bst_get(datastreams_surfaceid_bst,
                                                (system_bst_key)     key_ptr,
                                                (system_bst_value*) &temp) )
                            {
                                system_bst_insert(datastreams_surfaceid_bst,
                                                  (system_bst_key)    key_ptr,
                                                  (system_bst_value) &n_different_layer_elements);

                                n_different_layer_elements ++;
                            }
                        }
                    } /* for (all indices) */

                    mesh_ptr->gl_processed_data_total_elements += pass_ptr->n_elements;
                } /* if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) ) */
                else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve pass [%d] descriptor",
                                      n_pass);
                }
            } /* for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass) */

            layer_ptr->n_gl_unique_elements = n_different_layer_elements;

            /* Determine which data streams are used */
            for (unsigned int n_stream_data_type = MESH_LAYER_DATA_STREAM_TYPE_FIRST;
                              n_stream_data_type < MESH_LAYER_DATA_STREAM_TYPE_COUNT;
                            ++n_stream_data_type)
            {
                if (system_hash64map_contains(layer_ptr->data_streams,
                                              n_stream_data_type) )
                {
                    stream_usage[n_stream_data_type] = true;
                }
            }
        } /* if (layer_ptr != NULL) */
    } /* for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer) */

    ASSERT_DEBUG_SYNC(datastreams_surfaceid_bst != NULL,
                      "Data Stream + SurfaceID BST is not allowed to be NULL at this point.");

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
                unsigned int                     stream_n_components           = (n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS)                ? 2
                                                                               : (n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_NORMALS                   ||
                                                                                  n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS ||
                                                                                  n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES)                 ? 3
                                                                               :                                                                                4;
                unsigned int                     stream_required_bit_alignment = 0;

                _mesh_get_stream_data_properties(mesh_ptr,
                                                 (mesh_layer_data_stream_type) n_data_stream_type,
                                                &stream_data_type,
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
                    ASSERT_DEBUG_SYNC(stream_required_bit_alignment % 32 == 0,
                                      "32-bit multiple alignment only supported");

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
                    uint32_t n_passes = 0;

                    system_resizable_vector_get_property(layer_ptr->passes,
                                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                        &n_passes);

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
                            pass_ptr->gl_bo_elements_offset = (char*) elements_traveller_ptr      -
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
                                                   (system_bst_key)     key_ptr,
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
                                            ASSERT_DEBUG_SYNC(data_stream_ptr->data_type == MESH_LAYER_DATA_STREAM_DATA_TYPE_FLOAT,
                                                              "TODO");

                                            mesh_layer_data_stream_type actual_stream_type  = (mesh_layer_data_stream_type) n_data_stream_type;
                                            unsigned int                stream_n_components = (n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_TEXCOORDS)                ? 2
                                                                                            : (n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_NORMALS                   ||
                                                                                               n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_SPHERICAL_HARMONIC_3BANDS ||
                                                                                               n_data_stream_type == MESH_LAYER_DATA_STREAM_TYPE_VERTICES)                 ? 3
                                                                                            :                                                                                4;
                                            uint32_t                    stream_n_sets       = 0;

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
                                                    _mesh_layer_pass_index_data* set_index_data_ptr = NULL;

                                                    if (!system_hash64map_get(pass_ptr->index_data_maps[actual_stream_type],
                                                                              n_set,
                                                                             &set_index_data_ptr) )
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

                                                ASSERT_DEBUG_SYNC(data_stream_ptr->n_items_ptr != NULL,
                                                                  "Number of data stream items could not be determined.");
                                                ASSERT_DEBUG_SYNC(pass_index_data < *data_stream_ptr->n_items_ptr,
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
                                    ASSERT_ALWAYS_SYNC(false,
                                                       "Could not retrieve final GL buffer index key");
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

                            ASSERT_ALWAYS_SYNC(pass_ptr->gl_elements != NULL,
                                               "Out of memory");

                            if (pass_ptr->gl_elements != NULL)
                            {
                                memcpy(pass_ptr->gl_elements,
                                       (char*)mesh_ptr->gl_processed_data + pass_ptr->gl_bo_elements_offset,
                                       index_size * pass_ptr->n_elements);
                            }
                        } /* if (system_resizable_vector_get_element_at(layer_ptr->passes, n_pass, &pass_ptr) && pass_ptr != NULL) */
                        else
                        {
                            ASSERT_ALWAYS_SYNC(false,
                                               "Cannot retrieve mesh layer pass [%d] descriptor",
                                               n_pass);
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
    if (!((mesh_ptr->creation_flags & MESH_CREATION_FLAGS_SAVE_SUPPORT) ))
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
PUBLIC EMERALD_API bool mesh_fill_gl_buffers(mesh        instance,
                                             ral_context context)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    /* Before we block renderer thread, we need to convert the loaded data into single-indexed representation.
     * We do not store it, since it's very likely to be bigger than n-indexed one we use for serialization. */
    if (mesh_ptr->gl_processed_data_size == 0)
    {
        mesh_create_single_indexed_representation(instance);
    }

    /* Request renderer thread call-back */
    if (mesh_ptr->gl_context == NULL)
    {
        mesh_ptr->ral_context = context;
        mesh_ptr->gl_context  = ral_context_get_gl_context(context);
    }
    else
    {
        ASSERT_DEBUG_SYNC(mesh_ptr->ral_context == context,
                          "Inter-context sharing of meshes is not supported");
    }

    if (!ogl_context_request_callback_from_context_thread(mesh_ptr->gl_context,
                                                          _mesh_fill_gl_buffers_renderer_callback,
                                                          mesh_ptr,
                                                          false,   /* swap_buffers_afterward */
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
    uint32_t n_layers = 0;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

    if (mesh_ptr->gl_processed_data != NULL)
    {
        delete [] mesh_ptr->gl_processed_data;

        mesh_ptr->gl_processed_data = NULL;
    }

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
            uint32_t n_passes = 0;

            system_resizable_vector_get_property(layer_ptr->passes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_passes);

            for (uint32_t n_pass = 0;
                          n_pass < n_passes;
                        ++n_pass)
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
                    ASSERT_ALWAYS_SYNC(false,
                                       "Cannot retrieve mesh layer pass [%d] descriptor",
                                       n_pass);
                }
            } /* for (uint32_t n_pass = 0; n_pass < n_passes; ++n_pass) */
        } /* if (layer_ptr != NULL) */
    } /* for (uint32_t n_layer = 0; n_layer < n_layers; ++n_layer)*/

    mesh_ptr->n_gl_unique_vertices = 0;

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_generate_normal_data(mesh mesh)
{
    /* TODO: Should we move this to mesh_normal_data_generation.cc or sth? */
    system_resizable_vector allocated_polygon_vectors  = system_resizable_vector_create(4);
    system_resource_pool    mesh_polygon_resource_pool = system_resource_pool_create   (sizeof(_mesh_polygon),
                                                                                        4,     /* n_elements_to_preallocate */
                                                                                        NULL,  /* init_fn */
                                                                                        NULL); /* deinit_fn */
    _mesh*                  mesh_ptr                   = (_mesh*) mesh;
    unsigned int            n_layers                   = 0;
    system_resource_pool    vec3_resource_pool         = system_resource_pool_create   (sizeof(float) * 3,
                                                                                        4,     /* n_elements_to_preallocate */
                                                                                        NULL,  /* init_fn */
                                                                                        NULL); /* deinit_fn */

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

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
        LOG_INFO("Generating normal data for mesh [%s] (iteration %u/3) ...",
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

            system_resizable_vector_get_property(layer_ptr->passes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_layer_passes);

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
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve vertex data stream for mesh layer [%d]",
                                  n_layer);

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
                            uint32_t n_vertex_data_sets = 0;

                            system_hash64map_get_property(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                         &n_vertex_data_sets);

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
                                system_resizable_vector new_vector = system_resizable_vector_create(4 /* capacity */);

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
                                    polygon_vector = system_resizable_vector_create(4 /* capacity */);

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
                        uint32_t n_polygon_vectors = 0;

                        system_resizable_vector_get_property(allocated_polygon_vectors,
                                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                            &n_polygon_vectors);

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

                            system_resizable_vector_get_property(polygon_vector,
                                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                                &n_polygon_vector_items);

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
        uint32_t n_layer_passes = 0;

        system_resizable_vector_get_property(layer_ptr->passes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layer_passes);

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
            uint32_t n_vertex_items = 0;

            layer_pass_index_data  = new (std::nothrow) unsigned int[layer_pass_ptr->n_elements];
            layer_pass_normal_data = new (std::nothrow) float       [layer_pass_ptr->n_elements * 3 /* components */];

            ASSERT_ALWAYS_SYNC(layer_pass_normal_data != NULL &&
                               layer_pass_index_data  != NULL,
                               "Out of memory");

            memset(layer_pass_normal_data,
                   0,
                   sizeof(float) * layer_pass_ptr->n_elements * 3 /* components */);

            system_hash64map_get_property(layer_pass_ptr->index_data_maps[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_vertex_items);

            if (n_vertex_items > 0)
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

                system_resizable_vector_get_property(current_triangle_polygon_vector,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &current_triangle_polygon_vector_size);

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

                        system_resizable_vector_get_property(triangle_vertex_polygon_vector,
                                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                            &n_triangle_vertex_polygon_vector_items);

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
            uint32_t n_vertex_sets = 0;

            system_hash64map_get_property(layer_pass_ptr->set_id_to_unique_set_id[MESH_LAYER_DATA_STREAM_TYPE_VERTICES],
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_vertex_sets);

            ASSERT_DEBUG_SYNC(n_vertex_sets <= 1,
                              "Layer pass uses >= 1 sets which is not supported.");

            mesh_add_layer_data_stream_from_client_memory(mesh,
                                                          n_layer,
                                                          MESH_LAYER_DATA_STREAM_TYPE_NORMALS,
                                                          3, /* n_components */
                                                          layer_pass_ptr->n_elements,
                                                          layer_pass_normal_data);

            mesh_add_layer_pass_index_data_for_regular_mesh(mesh,
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
PUBLIC EMERALD_API bool mesh_get_layer_data_stream_data(mesh                        mesh,
                                                        mesh_layer_id               layer_id,
                                                        mesh_layer_data_stream_type type,
                                                        unsigned int*               out_n_items_ptr,
                                                        const void**                out_data_ptr)
{
    _mesh_layer* layer_ptr = NULL;
    _mesh*       mesh_ptr  = (_mesh*) mesh;
    bool         result    = false;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM ||
                      mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with GPU stream & regular meshes only.");


    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               layer_id,
                                              &layer_ptr) )
    {
        _mesh_layer_data_stream* stream_ptr = NULL;

        if (system_hash64map_get(layer_ptr->data_streams,
                                 type,
                                &stream_ptr) )
        {
            if (out_n_items_ptr         != NULL &&
                stream_ptr->n_items_ptr != NULL)
            {
                ASSERT_DEBUG_SYNC(stream_ptr->n_items_ptr != NULL,
                                  "Number of data stream items is unavailable.");

                *out_n_items_ptr = *stream_ptr->n_items_ptr;
            }

            if (out_data_ptr   != NULL              &&
                mesh_ptr->type == MESH_TYPE_REGULAR)
            {
                *out_data_ptr = stream_ptr->data;
            }

            result = true;
        } /* if (system_hash64map_get(layer_ptr->data_streams, type, &stream_ptr) ) */
    } /* if (system_resizable_vector_get_element_at(mesh_ptr->layers, layer_id, &layer_ptr) ) */

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_get_layer_data_stream_property(mesh                            mesh,
                                                            mesh_layer_id                   layer_id,
                                                            mesh_layer_data_stream_type     type,
                                                            mesh_layer_data_stream_property property,
                                                            void*                           out_result_ptr)
{
    _mesh* mesh_ptr = (_mesh*) mesh;
    bool   result   = false;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM ||
                      mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");
    ASSERT_DEBUG_SYNC(type < MESH_LAYER_DATA_STREAM_TYPE_COUNT,
                      "Unrecognized stream type");

    if (mesh_ptr->instantiation_parent != NULL)
    {
        result = mesh_get_layer_data_stream_property(mesh_ptr->instantiation_parent,
                                                     layer_id,
                                                     type,
                                                     property,
                                                     out_result_ptr);
    } /* if (mesh_ptr->instantiation_parent != NULL) */
    else
    {
        if  (mesh_ptr->type == MESH_TYPE_REGULAR                            &&
             property       == MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET)
        {
            raGL_buffer bo_raGL              = ral_context_get_buffer_gl(mesh_ptr->ral_context,
                                                                         mesh_ptr->gl_bo);
            uint32_t    bo_raGL_start_offset = 0;
            uint32_t    bo_ral_start_offset  = 0;

            raGL_buffer_get_property(bo_raGL,
                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                    &bo_raGL_start_offset);

            ral_buffer_get_property(mesh_ptr->gl_bo,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &bo_ral_start_offset);

            *((uint32_t*) out_result_ptr) = bo_raGL_start_offset                                   +
                                            bo_ral_start_offset                                    +
                                            mesh_ptr->gl_processed_data_stream_start_offset[type];
            result                        = true;
        }
        else
        {
            _mesh_layer* layer_ptr = NULL;

            if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                       layer_id,
                                                      &layer_ptr) )
            {
                _mesh_layer_data_stream* stream_ptr = NULL;

                if (system_hash64map_get(layer_ptr->data_streams,
                                         type,
                                        &stream_ptr) )
                {
                    switch (property)
                    {
                        case MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL:
                        {
                            *(ral_buffer*) out_result_ptr = stream_ptr->bo;
                            result                        = true;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_STRIDE:
                        {
                            *(unsigned int*) out_result_ptr = stream_ptr->bo_stride;
                            result                          = true;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_N_COMPONENTS:
                        {
                            *(unsigned int*) out_result_ptr = stream_ptr->n_components;
                            result                          = true;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS:
                        {
                            ASSERT_DEBUG_SYNC(stream_ptr->n_items_ptr != NULL,
                                              "Number of data stream items is unavailable");

                            if (stream_ptr->n_items_ptr != NULL)
                            {
                                *(unsigned int*) out_result_ptr = *stream_ptr->n_items_ptr;
                            }

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_RAL:
                        {
                            ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM,
                                              "Invalid mesh type for the MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO query.");

                            *(ral_buffer*) out_result_ptr = stream_ptr->n_items_bo;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER:
                        {
                            ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM,
                                              "Invalid mesh type for the MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_BO_READ_REQUIRES_MEMORY_BARRIER query.");

                            *(bool*) out_result_ptr = stream_ptr->n_items_bo_requires_memory_barrier;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS_SOURCE:
                        {
                            *(mesh_layer_data_stream_source*) out_result_ptr = stream_ptr->n_items_source;

                            break;
                        }

                        case MESH_LAYER_DATA_STREAM_PROPERTY_START_OFFSET:
                        {
#if 0
                            raGL_buffer_get_property(stream_ptr->bo,
                                                     RAGL_BUFFER_PROPERTY_START_OFFSET,
                                                     out_result_ptr);
#else
                            /* ral_buffer defines where the buffer memory region starts from. */
                            *(uint32_t*) out_result_ptr = 0;
#endif
                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "Unrecognized mesh layer data stream property");
                        }
                    } /* switch (property) */
                } /* if (layer data stream descriptor was retrieved) */
                else
                {
                    /* This location may be hit for meshes loaded from a serialized data source !
                     * Do NOT throw an assertion failure, but instead report a failure */
                }
            } /* if (layer descriptor was retrieved) */
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve layer descriptor");
            }
        }
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API uint32_t mesh_get_number_of_layer_passes(mesh          instance,
                                                            mesh_layer_id layer_id)
{
    _mesh_layer* mesh_layer_ptr = NULL;
    _mesh*       mesh_ptr       = (_mesh*) instance;
    uint32_t     result         = 0;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM ||
                      mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with GPU STREAM and regular meshes only.");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               layer_id,
                                              &mesh_layer_ptr) )
    {
        system_resizable_vector_get_property(mesh_layer_ptr->passes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &result);
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve layer with id [%d]",
                           layer_id);
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_get_property(mesh          instance,
                                          mesh_property property,
                                          void*         out_result_ptr)
{
    bool   b_result = true;
    _mesh* mesh_ptr = (_mesh*) instance;

    #ifdef _DEBUG
    {
        const bool is_custom_mesh_property = (property == MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_PTR      ||
                                              property == MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_USER_ARG);
        const bool is_shared_mesh_property = (property == MESH_PROPERTY_MODEL_AABB_MAX                   ||
                                              property == MESH_PROPERTY_MODEL_AABB_MIN                   ||
                                              property == MESH_PROPERTY_TYPE);

        ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_CUSTOM     && ( is_custom_mesh_property || is_shared_mesh_property) ||
                          mesh_ptr->type == MESH_TYPE_GPU_STREAM && (!is_custom_mesh_property || is_shared_mesh_property) ||
                          mesh_ptr->type == MESH_TYPE_REGULAR    && (!is_custom_mesh_property || is_shared_mesh_property),
                          "An invalid query was used for a mesh_get_property() call");
    }
    #endif /* _DEBUG */

    switch (property)
    {
        case MESH_PROPERTY_MODEL_AABB_MAX:
        {
            if (mesh_ptr->type == MESH_TYPE_CUSTOM)
            {
                float temp_aabb_min[4];

                mesh_ptr->pfn_get_custom_mesh_aabb_proc(mesh_ptr->get_custom_mesh_aabb_proc_user_arg,
                                                        temp_aabb_min,
                                                        mesh_ptr->aabb_max);
            }
            else
            if (mesh_ptr->type == MESH_TYPE_GPU_STREAM)
            {
                float temp_aabb_min[4];

                mesh_ptr->pfn_get_gpu_stream_mesh_aabb_proc(mesh_ptr->get_gpu_stream_mesh_aabb_proc_user_arg,
                                                            temp_aabb_min,
                                                            mesh_ptr->aabb_max);
            }

            *((float**) out_result_ptr) = mesh_ptr->aabb_max;

            break;
        }

        case MESH_PROPERTY_MODEL_AABB_MIN:
        {
            if (mesh_ptr->type == MESH_TYPE_CUSTOM)
            {
                float temp_aabb_max[4];

                mesh_ptr->pfn_get_custom_mesh_aabb_proc(mesh_ptr->get_custom_mesh_aabb_proc_user_arg,
                                                        mesh_ptr->aabb_min,
                                                        temp_aabb_max);
            }
            else
            if (mesh_ptr->type == MESH_TYPE_GPU_STREAM)
            {
                float temp_aabb_max[4];

                mesh_ptr->pfn_get_gpu_stream_mesh_aabb_proc(mesh_ptr->get_gpu_stream_mesh_aabb_proc_user_arg,
                                                            mesh_ptr->aabb_min,
                                                            temp_aabb_max);
            }

            *((float**) out_result_ptr) = mesh_ptr->aabb_min;

            break;
        }

        case MESH_PROPERTY_CREATION_FLAGS:
        {
            *((mesh_creation_flags*) out_result_ptr) = mesh_ptr->creation_flags;

            break;
        }

        case MESH_PROPERTY_GL_BO_RAL:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                if (!mesh_ptr->gl_storage_initialized)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "TODO");

#if 0
                    mesh_fill_gl_buffers(instance,
                                         ogl_context_get_current_context() );
#endif
                }

                ASSERT_DEBUG_SYNC(mesh_ptr->gl_storage_initialized,
                                  "Cannot query GL BO id - GL storage not initialized");

                *(ral_buffer*) out_result_ptr = mesh_ptr->gl_bo;
            } /* if (mesh_ptr->instantiation_parent == NULL) */
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  property,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_GL_INDEX_TYPE:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                *((_mesh_index_type*) out_result_ptr) = mesh_ptr->gl_index_type;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_GL_INDEX_TYPE,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_GL_PROCESSED_DATA:
        {
            ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data != NULL,
                              "Requested GL processed data is NULL");

            *((void**) out_result_ptr) = mesh_ptr->gl_processed_data;

            break;
        }

        case MESH_PROPERTY_GL_PROCESSED_DATA_SIZE:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                *((uint32_t*) out_result_ptr) = mesh_ptr->gl_processed_data_size;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_GL_PROCESSED_DATA_SIZE,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_GL_STRIDE:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_stride != 0,
                                  "Requested stride is 0");

                *((uint32_t*) out_result_ptr) = mesh_ptr->gl_processed_data_stride;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_GL_STRIDE,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_GL_THREAD_FILL_BUFFERS_CALL_NEEDED:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                *(bool*) out_result_ptr = mesh_ptr->gl_thread_fill_gl_buffers_call_needed;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_GL_THREAD_FILL_BUFFERS_CALL_NEEDED,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_GL_TOTAL_ELEMENTS:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data_total_elements != 0,
                                  "About to return 0 for MESH_PROPERTY_GL_TOTAL_ELEMENTS query");

                *((uint32_t*) out_result_ptr) = mesh_ptr->gl_processed_data_total_elements;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_GL_TOTAL_ELEMENTS,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_INSTANTIATION_PARENT:
        {
            *(mesh*) out_result_ptr = mesh_ptr->instantiation_parent;

            break;
        }

        case MESH_PROPERTY_MATERIALS:
        {
            *((system_resizable_vector*) out_result_ptr) = mesh_ptr->materials;

            break;
        }

        case MESH_PROPERTY_N_SH_BANDS:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                *((uint32_t*) out_result_ptr) = mesh_ptr->n_sh_bands;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_N_SH_BANDS,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_N_LAYERS:
        {
            system_resizable_vector_get_property(((_mesh*)instance)->layers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case MESH_PROPERTY_N_GL_UNIQUE_VERTICES:
        {
            if (mesh_ptr->instantiation_parent == NULL)
            {
                *((uint32_t*) out_result_ptr) = mesh_ptr->n_gl_unique_vertices;
            }
            else
            {
                mesh_get_property(mesh_ptr->instantiation_parent,
                                  MESH_PROPERTY_N_GL_UNIQUE_VERTICES,
                                  out_result_ptr);
            }

            break;
        }

        case MESH_PROPERTY_NAME:
        {
            *((system_hashed_ansi_string*) out_result_ptr) = mesh_ptr->name;

            break;
        }

        case MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_PTR:
        {
            *((PFNRENDERCUSTOMMESHPROC*) out_result_ptr) = mesh_ptr->pfn_render_custom_mesh_proc;

            break;
        }

        case MESH_PROPERTY_RENDER_CUSTOM_MESH_FUNC_USER_ARG:
        {
            *(void**) out_result_ptr = mesh_ptr->render_custom_mesh_proc_user_arg;

            break;
        }

        case MESH_PROPERTY_TIMESTAMP_MODIFICATION:
        {
            *((system_time*) out_result_ptr) = mesh_ptr->timestamp_last_modified;

            break;
        }

        case MESH_PROPERTY_TYPE:
        {
            *((mesh_type*) out_result_ptr) = mesh_ptr->type;

            break;
        }

        case MESH_PROPERTY_VERTEX_ORDERING:
        {
            *(mesh_vertex_ordering*) out_result_ptr = mesh_ptr->vertex_ordering;

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
PUBLIC EMERALD_API bool mesh_get_layer_pass_property(mesh                instance,
                                                     uint32_t            n_layer,
                                                     uint32_t            n_pass,
                                                     mesh_layer_property property,
                                                     void*               out_result_ptr)
{
    _mesh_layer* mesh_layer_ptr = NULL;
    _mesh*       mesh_ptr       = (_mesh*) instance;
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
                case MESH_LAYER_PROPERTY_DRAW_CALL_ARGUMENTS:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM,
                                      "MESH_LAYER_PROPERTY_DRAW_CALL_ARGUMENTS query is only valid for GPU stream meshes.");

                    *(mesh_draw_call_arguments*) out_result_ptr = mesh_layer_pass_ptr->draw_call_arguments;

                    break;
                }

                case MESH_LAYER_PROPERTY_DRAW_CALL_TYPE:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM,
                                      "MESH_LAYER_PROPERTY_DRAW_CALL_TYPE query is only valid for GPU stream meshes.");

                    *(mesh_draw_call_type*) out_result_ptr = mesh_layer_pass_ptr->draw_call_type;

                    break;
                }

                case MESH_LAYER_PROPERTY_MODEL_AABB_MAX:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_MODEL_AABB_MAX query is only valid for regular meshes.");

                    *((float**) out_result_ptr) = mesh_layer_ptr->aabb_max;

                    break;
                }

                case MESH_LAYER_PROPERTY_MODEL_AABB_MIN:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_MODEL_AABB_MIN query is only valid for regular meshes.");

                    *((float**) out_result_ptr) = mesh_layer_ptr->aabb_min;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET:
                {
                    raGL_buffer bo_raGL              = ral_context_get_buffer_gl(mesh_ptr->ral_context,
                                                                                 mesh_ptr->gl_bo);
                    uint32_t    bo_raGL_start_offset = 0;
                    uint32_t    bo_ral_start_offset  = 0;

                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_GL_BO_ELEMENTS_OFFSET query is only valid for regular meshes.");

                    raGL_buffer_get_property(bo_raGL,
                                             RAGL_BUFFER_PROPERTY_START_OFFSET,
                                            &bo_raGL_start_offset);
                    ral_buffer_get_property (mesh_ptr->gl_bo,
                                             RAL_BUFFER_PROPERTY_START_OFFSET,
                                            &bo_ral_start_offset);

                    *((uint32_t*) out_result_ptr) = bo_ral_start_offset                        +
                                                    bo_raGL_start_offset                       +
                                                    mesh_layer_pass_ptr->gl_bo_elements_offset;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA query is only valid for regular meshes.");

                    *((uint32_t**) out_result_ptr) = mesh_layer_pass_ptr->gl_elements;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MAX_INDEX query is only valid for regular meshes.");

                    *((uint32_t*) out_result_ptr) = mesh_layer_pass_ptr->gl_elements_max_index;

                    break;
                }

                case MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_GL_ELEMENTS_DATA_MIN_INDEX query is only valid for regular meshes.");

                    *((uint32_t*) out_result_ptr) = mesh_layer_pass_ptr->gl_elements_min_index;

                    break;
                }

                case MESH_LAYER_PROPERTY_MATERIAL:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_GPU_STREAM ||
                                      mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_MATERIAL query is only valid for GPU stream & regular meshes.");

                    *((mesh_material*) out_result_ptr) = mesh_layer_pass_ptr->material;

                    break;
                }

                case MESH_LAYER_PROPERTY_N_ELEMENTS:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_N_ELEMENTS query is only valid for regular meshes.");

                    *((uint32_t*) out_result_ptr) = mesh_layer_pass_ptr->n_elements;

                    break;
                }

                case MESH_LAYER_PROPERTY_N_TRIANGLES:
                {
                    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                                      "MESH_LAYER_PROPERTY_N_TRIANGLES query is only valid for regular meshes.");

                    *((uint32_t*) out_result_ptr) = mesh_layer_pass_ptr->n_elements / 3;

                    break;
                }

                default:
                {
                    ASSERT_ALWAYS_SYNC(false,
                                       "Unrecognized mesh layer property");

                    result = false;
                    break;
                }
            } /* switch(property) */
        } /* if (system_resizable_vector_get_element_at(mesh_layer_ptr->passes, n_pass, &mesh_layer_pass_ptr) )*/
        else
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Cannot retrieve mesh layer pass descriptor");

            result = false;
        }
    }
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Cannot retrieve mesh layer descriptor");

        result = false;
    }

    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API mesh mesh_load(ral_context               context,
                                  mesh_creation_flags       flags,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_id_to_mesh_material_map,
                                  system_hash64map          mesh_name_to_mesh_map)
{
    /* Create file serializer instance */
    mesh                   result     = NULL;
    system_file_serializer serializer = system_file_serializer_create_for_reading(full_file_path);

    ASSERT_DEBUG_SYNC(serializer != NULL,
                      "Out of memory");

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
PUBLIC EMERALD_API mesh mesh_load_with_serializer(ral_context            context_ral,
                                                  mesh_creation_flags    flags,
                                                  system_file_serializer serializer,
                                                  system_hash64map       material_id_to_mesh_material_map,
                                                  system_hash64map       mesh_name_to_mesh_map)
{
    /* Read header */
    char                      header[16]           = {0};
    bool                      is_instantiated      = false;
    system_hashed_ansi_string mesh_name            = NULL;
    _mesh*                    mesh_ptr             = NULL;
    mesh                      result               = NULL;
    system_hashed_ansi_string serializer_file_name = NULL;

    system_file_serializer_get_property(serializer,
                                        SYSTEM_FILE_SERIALIZER_PROPERTY_FILE_NAME,
                                       &serializer_file_name);
    system_file_serializer_read        (serializer,
                                        strlen(header_magic),
                                        header);

    ASSERT_ALWAYS_SYNC(strcmp(header, header_magic) == 0,
                       "Mesh [%s] is corrupt.",
                       system_hashed_ansi_string_get_buffer(serializer_file_name) );

    if (strcmp(header,
               header_magic) != 0)
    {
        goto end;
    }

    /* Is this an instantiated mesh? */
    system_file_serializer_read_hashed_ansi_string(serializer,
                                                  &mesh_name);
    system_file_serializer_read                   (serializer,
                                                   sizeof(is_instantiated),
                                                  &is_instantiated);

    result = mesh_create_regular_mesh(flags,
                                      mesh_name);

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Could not create mesh.");

    if (result == NULL)
    {
        goto end;
    }

    /* Set GL context */
    mesh_ptr = (_mesh*) result;

    mesh_ptr->gl_context  = ral_context_get_gl_context(context_ral);
    mesh_ptr->ral_context = context_ral;

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

        ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL,
                           "Out of memory");

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
                mesh_layer_pass_id current_pass_id = mesh_add_layer_pass_for_regular_mesh(result,
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
                             mesh_ptr->ral_context);
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
PUBLIC EMERALD_API void mesh_release_layer_datum(mesh in_mesh)
{
    _mesh*   mesh_ptr = (_mesh*) in_mesh;
    uint32_t n_layers = 0;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    system_resizable_vector_get_property(mesh_ptr->layers,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_layers);

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
        uint32_t n_data_streams = 0;

        system_hash64map_get_property(layer_ptr->data_streams,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_data_streams);

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
        uint32_t n_passes = 0;

        system_resizable_vector_get_property(layer_ptr->passes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_passes);

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
                    uint32_t n_index_data_maps = 0;

                    system_hash64map_get_property(pass_ptr->index_data_maps[stream_type],
                                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                                 &n_index_data_maps);

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
PUBLIC EMERALD_API bool mesh_save(mesh                      instance,
                                  system_hashed_ansi_string full_file_path,
                                  system_hash64map          material_name_to_id_map)
{
    _mesh*                 mesh_ptr   = (_mesh*) instance;
    bool                   result     = false;
    system_file_serializer serializer = NULL;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    /* Make sure the mesh implementation has been asked to maintain the data buffers! */
    ASSERT_ALWAYS_SYNC(mesh_ptr->creation_flags & MESH_CREATION_FLAGS_SAVE_SUPPORT,
                       "Cannot save - mesh instance has not been created with save-enabled flag");

    if (!(mesh_ptr->creation_flags & MESH_CREATION_FLAGS_SAVE_SUPPORT) )
    {
        goto end;
    }

    /* Create file serializer instance */
    serializer = system_file_serializer_create_for_writing(full_file_path);

    ASSERT_DEBUG_SYNC(serializer != NULL,
                      "Out of memory");

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
PUBLIC EMERALD_API bool mesh_save_with_serializer(mesh                   instance,
                                                  system_file_serializer serializer,
                                                  system_hash64map       mesh_material_to_id_map)
{
    _mesh* mesh_ptr = (_mesh*) instance;
    bool   result   = false;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

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
        uint32_t n_layers = 0;

        system_resizable_vector_get_property(mesh_ptr->layers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_layers);

        system_file_serializer_write(serializer,
                                     sizeof(n_layers),
                                    &n_layers);

        for (size_t n_layer = 0;
                    n_layer < n_layers;
                  ++n_layer)
        {
            _mesh_layer* layer = NULL;

            system_resizable_vector_get_element_at(mesh_ptr->layers,
                                                   n_layer,
                                                  &layer);

            ASSERT_DEBUG_SYNC(layer != NULL,
                              "Could not retrieve layer info");

            if (layer != NULL)
            {
                uint32_t n_layer_passes = 0;

                system_resizable_vector_get_property(layer->passes,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_layer_passes);

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
PUBLIC EMERALD_API void mesh_set_as_instantiated(mesh mesh_to_modify,
                                                 mesh source_mesh)
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

    ASSERT_DEBUG_SYNC(mesh_to_modify_ptr->type == MESH_TYPE_REGULAR &&
                      source_mesh_ptr->type    == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

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

        mesh_to_modify_ptr->gl_processed_data = NULL;
    }

    if (mesh_to_modify_ptr->gl_bo != NULL)
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

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_set_layer_data_stream_property(mesh                            instance,
                                                            uint32_t                        n_layer,
                                                            mesh_layer_data_stream_type     type,
                                                            mesh_layer_data_stream_property property,
                                                            const void*                     data)
{
    _mesh_layer_data_stream* data_stream_ptr = NULL;
    _mesh*                   instance_ptr    = (_mesh*) instance;
    _mesh_layer*             layer_ptr       = NULL;
    bool                     result          = true;

    ASSERT_DEBUG_SYNC(instance_ptr != NULL,
                      "Input mesh instance is NULL");

    /* Retrieve the data stream descriptor. */
    if (!system_resizable_vector_get_element_at(instance_ptr->layers,
                                                n_layer,
                                               &layer_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve layer descriptor");

        result = false;
        goto end;
    }

    if (!system_hash64map_get(layer_ptr->data_streams,
                              type,
                             &data_stream_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve data stream descriptor");

        result = false;
        goto end;
    }

    /* Update the property */
    switch (property)
    {
        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS:
        {
            if (data_stream_ptr->n_items_bo != NULL)
            {
                /* External entity owns the buffer memory region, so do not deallocate it. */
                data_stream_ptr->n_items_bo = NULL;
            } /* if (data_stream_ptr->n_items_bo != NULL) */

            if (data_stream_ptr->n_items_ptr == NULL)
            {
                data_stream_ptr->n_items_ptr = new (std::nothrow) unsigned int;

                ASSERT_ALWAYS_SYNC(data_stream_ptr->n_items_ptr != NULL,
                                   "Out of memory");
            } /* if (data_stream_ptr->n_items_ptr != NULL) */

            *data_stream_ptr->n_items_ptr   = *(unsigned int*) data;
            data_stream_ptr->n_items_source = MESH_LAYER_DATA_STREAM_SOURCE_CLIENT_MEMORY;

            result = true;
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported mesh layer data stream property");
        }
    } /* switch (property) */

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API bool mesh_set_layer_data_stream_property_with_buffer_memory(mesh                            instance,
                                                                               uint32_t                        n_layer,
                                                                               mesh_layer_data_stream_type     type,
                                                                               mesh_layer_data_stream_property property,
                                                                               ral_buffer                      bo,
                                                                               bool                            does_read_require_memory_barrier)
{
    _mesh_layer_data_stream* data_stream_ptr = NULL;
    _mesh*                   instance_ptr    = (_mesh*) instance;
    _mesh_layer*             layer_ptr       = NULL;
    bool                     result          = true;

    ASSERT_DEBUG_SYNC(instance_ptr != NULL,
                      "Input mesh instance is NULL");

    /* Retrieve the data stream descriptor. */
    if (!system_resizable_vector_get_element_at(instance_ptr->layers,
                                                n_layer,
                                               &layer_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve layer descriptor");

        result = false;
        goto end;
    }

    if (!system_hash64map_get(layer_ptr->data_streams,
                              type,
                             &data_stream_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve data stream descriptor");

        result = false;
        goto end;
    }

    /* Update the property */
    switch (property)
    {
        case MESH_LAYER_DATA_STREAM_PROPERTY_GL_BO_RAL:
        {
            data_stream_ptr->bo = bo;

            result = true;
            break;
        }

        case MESH_LAYER_DATA_STREAM_PROPERTY_N_ITEMS:
        {
            uint32_t bo_size = 0;

            if (data_stream_ptr->n_items_ptr != NULL)
            {
                delete data_stream_ptr->n_items_ptr;

                data_stream_ptr->n_items_ptr = NULL;
            } /* if (data_stream_ptr->n_items_ptr != NULL) */

            ral_buffer_get_property(bo,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &bo_size);

            ASSERT_DEBUG_SYNC(bo_size == sizeof(unsigned int),
                              "Invalid buffer memory region size requested.");

            data_stream_ptr->n_items_bo                         = bo;
            data_stream_ptr->n_items_bo_requires_memory_barrier = does_read_require_memory_barrier;
            data_stream_ptr->n_items_source                     = MESH_LAYER_DATA_STREAM_SOURCE_BUFFER_MEMORY;

            result = true;
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported mesh layer data stream property");
        }
    } /* switch (property) */

end:
    return result;
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_layer_property(mesh                instance,
                                                uint32_t            n_layer,
                                                uint32_t            n_pass,
                                                mesh_layer_property property,
                                                const void*         data)
{
    _mesh_layer* mesh_layer_ptr = NULL;
    _mesh*       mesh_ptr       = (_mesh*) instance;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    if (system_resizable_vector_get_element_at(mesh_ptr->layers,
                                               n_layer,
                                              &mesh_layer_ptr) )
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
            ASSERT_ALWAYS_SYNC(false,
                               "Could not retrieve layer pass descriptor");
        }

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();
    } /* if (layer descriptor was retrieved) */
    else
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Could not retrieve layer descriptor");
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_set_processed_data_stream_start_offset(mesh                        mesh,
                                                                    mesh_layer_data_stream_type stream_type,
                                                                    unsigned int                start_offset)
{
    _mesh* mesh_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");

    mesh_ptr->gl_processed_data_stream_start_offset[stream_type] = start_offset;

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}

/* Please see header for specification */
PUBLIC EMERALD_API void mesh_set_property(mesh          instance,
                                          mesh_property property,
                                          const void*   value)
{
    ASSERT_DEBUG_SYNC(instance != NULL,
                      "Cannot set propetry - instance is NULL");

    if (instance != NULL)
    {
        _mesh* mesh_ptr = (_mesh*) instance;

        ASSERT_DEBUG_SYNC(property       == MESH_PROPERTY_VERTEX_ORDERING ||
                          mesh_ptr->type == MESH_TYPE_REGULAR,
                          "Entry-point is incompatible with the requested mesh type.");

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
                ASSERT_DEBUG_SYNC(mesh_ptr->n_sh_bands == 0                  ||
                                  mesh_ptr->n_sh_bands == *(uint32_t*) value,
                                  "Cannot update number of SH bands");

                if (mesh_ptr->n_sh_bands == 0                  ||
                    mesh_ptr->n_sh_bands == *(uint32_t*) value)
                {
                    mesh_ptr->n_sh_bands = *(uint32_t*)value;
                }

                break;
            }

            case MESH_PROPERTY_VERTEX_ORDERING:
            {
                mesh_ptr->vertex_ordering = *(mesh_vertex_ordering*) value;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized mesh property");
            }
        } /* switch(property) */

        /* Update modification timestamp */
        mesh_ptr->timestamp_last_modified = system_time_now();
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API void mesh_set_single_indexed_representation(mesh             mesh,
                                                               _mesh_index_type index_type,
                                                               uint32_t         n_blob_data_bytes,
                                                               const void*      blob_data)
{
    _mesh* mesh_ptr = (_mesh*) mesh;

    ASSERT_DEBUG_SYNC(mesh_ptr->type == MESH_TYPE_REGULAR,
                      "Entry-point is only compatible with regular meshes only.");
    ASSERT_DEBUG_SYNC(mesh_ptr->gl_processed_data      == NULL &&
                      mesh_ptr->gl_processed_data_size == 0,
                      "Processed GL data already set");
    ASSERT_DEBUG_SYNC(!mesh_ptr->gl_storage_initialized,
                      "Processed GL data already uploaded");

    mesh_ptr->gl_processed_data_size = n_blob_data_bytes;
    mesh_ptr->gl_processed_data      = (float*) new (std::nothrow) unsigned char[n_blob_data_bytes];
    mesh_ptr->gl_index_type          = index_type;

    ASSERT_ALWAYS_SYNC(mesh_ptr->gl_processed_data != NULL,
                       "Out of memory");

    if (mesh_ptr->gl_processed_data != NULL)
    {
        memcpy(mesh_ptr->gl_processed_data,
               blob_data,
               n_blob_data_bytes);
    }

    /* Update modification timestamp */
    mesh_ptr->timestamp_last_modified = system_time_now();
}


