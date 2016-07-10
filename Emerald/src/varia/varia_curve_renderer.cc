#if 0

TODO

/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "ogl/ogl_context.h"
#include "object_manager/object_manager_general.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_math_vector.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"
#include "varia/varia_curve_renderer.h"
#include "varia/varia_primitive_renderer.h"

/** Internal type declarations */
typedef struct _varia_curve_renderer_item
{
    varia_primitive_renderer_dataset_id path_dataset_id;
    varia_primitive_renderer_dataset_id view_vector_dataset_id;

    _varia_curve_renderer_item()
    {
        path_dataset_id        = -1;
        view_vector_dataset_id = -1;
    }

    ~_varia_curve_renderer_item()
    {
        ASSERT_DEBUG_SYNC(path_dataset_id == -1,
                          "Path dataset has not been released");
        ASSERT_DEBUG_SYNC(view_vector_dataset_id == -1,
                          "View vector dataset has not been released");
    }
} _varia_curve_renderer_item;

typedef struct
{
    varia_primitive_renderer_dataset_id* conversion_array_items;
    uint32_t                             n_conversion_array_items;

    system_resizable_vector   items;
    varia_primitive_renderer  primitive_renderer;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _varia_curve_renderer;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(varia_curve_renderer,
                               varia_curve_renderer,
                              _varia_curve_renderer)

/* Forward declarations */
PRIVATE bool _varia_curve_renderer_get_scene_graph_node_vertex_data(scene_graph                 graph,
                                                                    scene_graph_node            node,
                                                                    system_time                 duration,
                                                                    unsigned int                n_samples_per_second,
                                                                    float                       view_vector_length,
                                                                    unsigned int*               out_n_vertices,
                                                                    float**                     out_vertex_line_strip_data,
                                                                    float**                     out_view_vector_lines_data);
PRIVATE void _varia_curve_renderer_release                         (void*                       renderer);
PRIVATE void _varia_curve_renderer_release_item                    (_varia_curve_renderer*      renderer_ptr,
                                                                    _varia_curve_renderer_item* item_ptr);


/** TODO */
PRIVATE bool _varia_curve_renderer_get_scene_graph_node_vertex_data(scene_graph      graph,
                                                                    scene_graph_node node,
                                                                    system_time      duration,
                                                                    unsigned int     n_samples_per_second,
                                                                    float            view_vector_length,
                                                                    unsigned int*    out_n_vertices,
                                                                    float**          out_vertex_line_strip_data,
                                                                    float**          out_view_vector_lines_data)
{

    uint32_t     duration_ms             = 0;
    uint32_t     n_samples               = 0;
    float*       ref_traveller_ptr       = nullptr;
    bool         result                  = true;
    unsigned int result_n_vertices       = 0;
    float*       result_vertex_data      = nullptr;
    float*       result_view_vector_data = nullptr;
    uint32_t     sample_delta_time       = 0;
    float*       vertex_traveller_ptr    = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(graph != nullptr,
                      "Graph is NULL");
    ASSERT_DEBUG_SYNC(node  != nullptr,
                      "Graph node is NULL");
    ASSERT_DEBUG_SYNC(duration != 0,
                      "Graph duration is 0");
    ASSERT_DEBUG_SYNC(n_samples_per_second != 0,
                      "Samples/second is 0");
    ASSERT_DEBUG_SYNC(out_n_vertices != nullptr,
                      "out_n_vertices is NULL");
    ASSERT_DEBUG_SYNC(out_vertex_line_strip_data != nullptr,
                      "out_vertex_data is NULL");

    /* How many samples will we need to take? */
    system_time_get_msec_for_time(duration,
                                 &duration_ms);

    n_samples = duration_ms * n_samples_per_second / 1000 /* ms in 1 s */;

    /* Allocate vertex data. This will hold the vertices making up the node-defined path */
    ASSERT_DEBUG_SYNC(n_samples != 0,
                      "n_samples is 0");

    result_vertex_data = new float[3 /* x, y, z */ * n_samples];

    ASSERT_ALWAYS_SYNC(result_vertex_data != nullptr,
                       "Out of memory");

    if (result_vertex_data == nullptr)
    {
        result = false;

        goto end;
    }

    /* If requested, also allocate memory for the lines that will depict the view vectors for
     * each sample.
     */
    if (out_view_vector_lines_data != nullptr)
    {
        result_view_vector_data = new float[2 /* points */ * 3 /* x, y, z */ * n_samples];

        ASSERT_ALWAYS_SYNC(result_view_vector_data != nullptr,
                           "Out of memory");

        if (result_view_vector_data == nullptr)
        {
            result = false;

            goto end;
        }
    }

    /* Iterate over all samples and generate vertex data */
    ref_traveller_ptr    = result_view_vector_data;
    sample_delta_time    = 1000 / n_samples_per_second;
    vertex_traveller_ptr = result_vertex_data;

    scene_graph_lock(graph);

    for (uint32_t n_sample = 0;
                  n_sample < n_samples;
                ++n_sample)
    {
        /* Update the graph's projection matrices */
        system_time sample_time = system_time_get_time_for_msec(sample_delta_time * n_sample);

        scene_graph_compute_node(graph,
                                 node,
                                 sample_time);

        /* Retrieve the node's transformation matrix */
        system_matrix4x4 transformation_matrix = nullptr;

        scene_graph_node_get_property(node,
                                      SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                     &transformation_matrix);

        /* Compute the sample-specific position */
        const float base_position  [4] = {0.0f, 0.0f,  0.0f, 1.0f};
        float       sample_position[4];

        system_matrix4x4_multiply_by_vector4(transformation_matrix,
                                             base_position,
                                             sample_position);

        ASSERT_DEBUG_SYNC(fabs(sample_position[3]) > 1e-5f,
                          "NaN ahead");

        sample_position[0] /= sample_position[3];
        sample_position[1] /= sample_position[3];
        sample_position[2] /= sample_position[3];

        /* Store the vertex data */
        memcpy(vertex_traveller_ptr,
               sample_position,
               sizeof(float) * 3);

        vertex_traveller_ptr += 3;

        if (ref_traveller_ptr != nullptr)
        {
            /* Compute the position of the other vertex we need to use for the view vector. */
            const float ref_position       [4] = {0.0f, 0.0f, -1.0f, 1.0f}; /* NOTE: OpenGL-specific! */
            float       sample_ref_position[4];

            system_matrix4x4_multiply_by_vector4(transformation_matrix,
                                                 ref_position,
                                                 sample_ref_position);

            ASSERT_DEBUG_SYNC(fabs(sample_ref_position[3]) > 1e-5f,
                              "NaN ahead");

            sample_ref_position[0] /= sample_ref_position[3];
            sample_ref_position[1] /= sample_ref_position[3];
            sample_ref_position[2] /= sample_ref_position[3];

            /* Compute the view vector */
            float view_vector[3] =
            {
                sample_ref_position[0] - sample_position[0],
                sample_ref_position[1] - sample_position[1],
                sample_ref_position[2] - sample_position[2],
            };

            system_math_vector_normalize3(view_vector, view_vector);
            system_math_vector_mul3_float(view_vector, view_vector_length, view_vector);

            /* Use the view vector information and the caller-specified length to come up
             * with a final reference position */
            sample_ref_position[0] = sample_position[0] + view_vector[0];
            sample_ref_position[1] = sample_position[1] + view_vector[1];
            sample_ref_position[2] = sample_position[2] + view_vector[2];

            /* Store it */
            memcpy(ref_traveller_ptr,
                   sample_position,
                   sizeof(float) * 3);

            ref_traveller_ptr += 3;

            memcpy(ref_traveller_ptr,
                   sample_ref_position,
                   sizeof(float) * 3);

            ref_traveller_ptr += 3;
        }
    }

    /* Looks like everything's fine! */
    result                      = true;
    *out_n_vertices             = n_samples;
    *out_vertex_line_strip_data = result_vertex_data;

    if (result_view_vector_data != nullptr)
    {
        *out_view_vector_lines_data = result_view_vector_data;
    }

    scene_graph_unlock(graph);

end:
    if (!result)
    {
        if (result_vertex_data != nullptr)
        {
            delete [] result_vertex_data;

            result_vertex_data = nullptr;
        }

        if (result_view_vector_data != nullptr)
        {
            delete [] result_view_vector_data;

            result_view_vector_data = nullptr;
        }
    }

    return result;
}

/** TODO */
PRIVATE void _varia_curve_renderer_release(void* renderer)
{
    _varia_curve_renderer* renderer_ptr = (_varia_curve_renderer*) renderer;

    if (renderer_ptr->items != nullptr)
    {
        _varia_curve_renderer_item* item_ptr = nullptr;

        while (system_resizable_vector_pop(renderer_ptr->items,
                                          &item_ptr) )
        {
            if (item_ptr != nullptr)
            {
                _varia_curve_renderer_release_item(renderer_ptr,
                                                   item_ptr);

                delete item_ptr;
                item_ptr = nullptr;
            }
        }

        system_resizable_vector_release(renderer_ptr->items);
        renderer_ptr->items = nullptr;
    }

    if (renderer_ptr->primitive_renderer != nullptr)
    {
        varia_primitive_renderer_release(renderer_ptr->primitive_renderer);

        renderer_ptr->primitive_renderer = nullptr;
    }
}

/** TODO */
PRIVATE void _varia_curve_renderer_release_item(_varia_curve_renderer*      renderer_ptr,
                                                _varia_curve_renderer_item* item_ptr)
{
    varia_primitive_renderer_delete_dataset(renderer_ptr->primitive_renderer,
                                            item_ptr->path_dataset_id);
    varia_primitive_renderer_delete_dataset(renderer_ptr->primitive_renderer,
                                            item_ptr->view_vector_dataset_id);

    item_ptr->path_dataset_id        = -1;
    item_ptr->view_vector_dataset_id = -1;
}

/** Please see header for spec */
PUBLIC EMERALD_API varia_curve_item_id varia_curve_renderer_add_scene_graph_node_curve(varia_curve_renderer renderer,
                                                                                       scene_graph          graph,
                                                                                       scene_graph_node     node,
                                                                                       const float*         curve_color,
                                                                                       system_time          duration,
                                                                                       unsigned int         n_samples_per_second,
                                                                                       float                view_vector_length)
{
    varia_curve_item_id    item_id                     = -1;
    _varia_curve_renderer* renderer_ptr                = (_varia_curve_renderer*) renderer;
    bool                   should_include_view_vectors = (view_vector_length > 1e-5f);

    /* Prepare vertex data */
    _varia_curve_renderer_item* item_ptr               = nullptr;
    unsigned int                n_vertices             = 0;
    bool                        result                 = false;
    float*                      vertex_data            = nullptr;
    float*                      view_vector_lines_data = nullptr;

    result = _varia_curve_renderer_get_scene_graph_node_vertex_data(graph,
                                                                    node,
                                                                    duration,
                                                                    n_samples_per_second,
                                                                    view_vector_length,
                                                                   &n_vertices,
                                                                   &vertex_data,
                                                                   should_include_view_vectors ? &view_vector_lines_data : nullptr);

    ASSERT_ALWAYS_SYNC(result              &&
                       n_vertices  != 0    &&
                       vertex_data != nullptr,
                       "Could not generate vertex data");

    if (vertex_data == nullptr)
    {
        goto end;
    }

    /* Spawn new item descriptor */
    item_ptr = new (std::nothrow) _varia_curve_renderer_item;

    ASSERT_ALWAYS_SYNC(item_ptr != nullptr,
                       "Out of memory");

    if (item_ptr == nullptr)
    {
        goto end;
    }

    /* Fill it */
    item_ptr->path_dataset_id = varia_primitive_renderer_add_dataset(renderer_ptr->primitive_renderer,
                                                                     RAL_PRIMITIVE_TYPE_LINE_STRIP,
                                                                     n_vertices,
                                                                     vertex_data,
                                                                     curve_color);

    if (should_include_view_vectors)
    {
        const float view_vector_color[] = {1.0f, 0.0f, 0.0f, 1.0f};

        item_ptr->view_vector_dataset_id = varia_primitive_renderer_add_dataset(renderer_ptr->primitive_renderer,
                                                                                RAL_PRIMITIVE_TYPE_LINES,
                                                                                n_vertices * 2,
                                                                                view_vector_lines_data,
                                                                                view_vector_color);
    }

    /* Store it */
    system_resizable_vector_get_property(renderer_ptr->items,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &item_id);

    system_resizable_vector_push(renderer_ptr->items,
                                 item_ptr);

    /* Good to release the vertex data */
    delete [] vertex_data;

    vertex_data = nullptr;

end:
    return item_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API varia_curve_renderer varia_curve_renderer_create(ral_context               context,
                                                                    system_hashed_ansi_string name)
{
    _varia_curve_renderer* new_instance = new (std::nothrow) _varia_curve_renderer;

    ASSERT_DEBUG_SYNC(new_instance != nullptr,
                      "Out of memory");

    if (new_instance != nullptr)
    {
        new_instance->conversion_array_items   = nullptr;
        new_instance->items                    = system_resizable_vector_create(4 /* capacity */);
        new_instance->n_conversion_array_items = 0;
        new_instance->primitive_renderer       = varia_primitive_renderer_create(context,
                                                                                 system_hashed_ansi_string_create_by_merging_two_strings("Prim renderer for ",
                                                                                                                                         system_hashed_ansi_string_get_buffer(name) ));

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                       _varia_curve_renderer_release,
                                                       OBJECT_TYPE_VARIA_CURVE_RENDERER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Varia Curve Renderers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (varia_curve_renderer) new_instance;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_curve_renderer_delete_curve(varia_curve_renderer renderer,
                                                          varia_curve_item_id  item_id)
{
    _varia_curve_renderer_item* item_ptr     = nullptr;
    _varia_curve_renderer*      renderer_ptr = (_varia_curve_renderer*) renderer;

    if (!system_resizable_vector_get_element_at(renderer_ptr->items,
                                                item_id,
                                               &item_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find requested curve descriptor");

        goto end;
    }

    /* Release the item */
    _varia_curve_renderer_release_item(renderer_ptr,
                                       item_ptr);

    delete item_ptr;
    item_ptr = nullptr;

    /* Mark the item as released */
    system_resizable_vector_set_element_at(renderer_ptr->items,
                                           item_id,
                                           nullptr);

    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void varia_curve_renderer_draw(varia_curve_renderer       renderer,
                                                  unsigned int               n_item_ids,
                                                  const varia_curve_item_id* item_ids,
                                                  system_matrix4x4           mvp)
{
    unsigned int           n_items_used = 0;
    _varia_curve_renderer* renderer_ptr = (_varia_curve_renderer*) renderer;

    /* Convert curve item IDs to line strip item IDs. Use a preallocated array.
     * If necessary, scale it up.
     *
     * A single curve renderer item may hold both path data, as well as view vector data,
     * so make sure we have enough space for twice the number of items the caller has requested.
     */
    if (renderer_ptr->n_conversion_array_items < 2 * n_item_ids)
    {
        LOG_INFO("Performance warning: scaling up conversion array");

        if (renderer_ptr->conversion_array_items != nullptr)
        {
            delete [] renderer_ptr->conversion_array_items;

            renderer_ptr->conversion_array_items = nullptr;
        }

        renderer_ptr->conversion_array_items = new varia_primitive_renderer_dataset_id[2 * (2 * n_item_ids)];

        ASSERT_ALWAYS_SYNC(renderer_ptr->conversion_array_items != nullptr,
                           "Out of memory");

        if (renderer_ptr->conversion_array_items == nullptr)
        {
            goto end;
        }

        renderer_ptr->n_conversion_array_items = 2 * (2 * n_item_ids);
    }

    /* Fill the array with line strip IDs */
    for (unsigned int n_item_id = 0;
                      n_item_id < n_item_ids;
                    ++n_item_id)
    {
        _varia_curve_renderer_item* item_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(renderer_ptr->items,
                                                    item_ids[n_item_id],
                                                   &item_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve item descriptor");

            goto end;
        }

        renderer_ptr->conversion_array_items[n_items_used] = item_ptr->path_dataset_id;
        n_items_used++;

        if (item_ptr->view_vector_dataset_id != -1)
        {
            renderer_ptr->conversion_array_items[n_items_used] = item_ptr->view_vector_dataset_id;
            n_items_used++;
        }
    }

    varia_primitive_renderer_draw(renderer_ptr->primitive_renderer,
                                  mvp,
                                  n_items_used,
                                  renderer_ptr->conversion_array_items);

end:
    ;
}

#endif