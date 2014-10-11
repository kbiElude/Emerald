/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "curve/curve_container.h"
#include "ogl/ogl_context.h"
#include "ogl/ogl_curve_renderer.h"
#include "ogl/ogl_line_strip_renderer.h"
#include "object_manager/object_manager_general.h"
#include "scene/scene_graph.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_matrix4x4.h"
#include "system/system_resizable_vector.h"

/** Internal type declarations */
typedef struct _ogl_curve_renderer_item
{
    ogl_line_strip_renderer_dataset_id dataset_id;

    _ogl_curve_renderer_item()
    {
        dataset_id = -1;
    }

    ~_ogl_curve_renderer_item()
    {
        ASSERT_DEBUG_SYNC(dataset_id == -1,
                          "Dataset has not been released");
    }
} _ogl_curve_renderer_item;

typedef struct
{
    ogl_line_strip_renderer_dataset_id* conversion_array_items;
    uint32_t                            n_conversion_array_items;

    system_resizable_vector   items;
    ogl_line_strip_renderer   line_strip_renderer;
    system_hashed_ansi_string name;

    REFCOUNT_INSERT_VARIABLES
} _ogl_curve_renderer;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_curve_renderer, ogl_curve_renderer, _ogl_curve_renderer)

/* Forward declarations */
PRIVATE bool _ogl_curve_renderer_get_scene_graph_node_vertex_data(__in  __notnull scene_graph               graph,
                                                                  __in  __notnull scene_graph_node          node,
                                                                  __in            system_timeline_time      duration,
                                                                  __in            unsigned int              n_samples_per_second,
                                                                  __out __notnull unsigned int*             out_n_vertices,
                                                                  __out __notnull float**                   out_vertex_data);
PRIVATE void _ogl_curve_renderer_release                         (__in  __notnull void*                     renderer);
PRIVATE void _ogl_curve_renderer_release_item                    (__in  __notnull _ogl_curve_renderer*      renderer_ptr,
                                                                  __in  __notnull _ogl_curve_renderer_item* item_ptr);


/** TODO */
PRIVATE bool _ogl_curve_renderer_get_scene_graph_node_vertex_data(__in  __notnull scene_graph          graph,
                                                                  __in  __notnull scene_graph_node     node,
                                                                  __in            system_timeline_time duration,
                                                                  __in            unsigned int         n_samples_per_second,
                                                                  __out __notnull unsigned int*        out_n_vertices,
                                                                  __out __notnull float**              out_vertex_data)
{
    bool         result             = true;
    unsigned int result_n_vertices  = 0;
    float*       result_vertex_data = NULL;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(graph                != NULL, "Graph is NULL");
    ASSERT_DEBUG_SYNC(node                 != NULL, "Graph node is NULL");
    ASSERT_DEBUG_SYNC(duration             != 0,    "Graph duration is 0");
    ASSERT_DEBUG_SYNC(n_samples_per_second != 0,    "Samples/second is 0");
    ASSERT_DEBUG_SYNC(out_n_vertices       != NULL, "out_n_vertices is NULL");
    ASSERT_DEBUG_SYNC(out_vertex_data      != NULL, "out_vertex_data is NULL");

    /* How many samplese will we need to take? */
    uint32_t duration_ms = 0;
    uint32_t n_samples   = 0;

    system_time_get_msec_for_timeline_time(duration, &duration_ms);

    n_samples = duration_ms * n_samples_per_second / 1000 /* ms in 1 s */;

    /* Allocate vertex data */
    ASSERT_DEBUG_SYNC(n_samples != 0, "n_samples is 0");

    result_vertex_data = new float[3 /* x, y, z */ * n_samples];

    ASSERT_ALWAYS_SYNC(result_vertex_data != NULL, "Out of memory");
    if (result_vertex_data == NULL)
    {
        result = false;

        goto end;
    }

    /* Iterate over all samples and generate vertex data */
    uint32_t sample_delta_time = 1000 / n_samples_per_second;
    float*   traveller_ptr     = result_vertex_data;

    for (uint32_t n_sample = 0;
                  n_sample < n_samples;
                ++n_sample)
    {
        /* Update the graph's projection matrices */
        system_timeline_time sample_time = system_time_get_timeline_time_for_msec(sample_delta_time * n_sample);

        scene_graph_compute(graph, sample_time);

        /* Retrieve the node's transformation matrix */
        system_matrix4x4 transformation_matrix = NULL;

        scene_graph_node_get_property(node,
                                      SCENE_GRAPH_NODE_PROPERTY_TRANSFORMATION_MATRIX,
                                     &transformation_matrix);

        /* Compute the sample-specific position */
        const float base_position  [4] = {0.0f, 0.0f, 0.0f, 1.0f};
        float       sample_position[4];

        system_matrix4x4_multiply_by_vector4(transformation_matrix,
                                             base_position,
                                             sample_position);

        ASSERT_DEBUG_SYNC(fabs(sample_position[3]) > 1e-5f,
                          "NaN ahead");

        sample_position[0] /= sample_position[3];
        sample_position[1] /= sample_position[3];
        sample_position[2] /= sample_position[3];

        /* Store it */
        memcpy(traveller_ptr,
               sample_position,
               sizeof(float) * 3);

        traveller_ptr += 3;
    } /* for (all samples) */

    /* Looks like everything's fine! */
    result           = true;
    *out_n_vertices  = n_samples;
    *out_vertex_data = result_vertex_data;

end:
    if (!result)
    {
        if (result_vertex_data != NULL)
        {
            delete [] result_vertex_data;

            result_vertex_data = NULL;
        }
    } /* if (!result) */

    return result;
}

/** TODO */
PRIVATE void _ogl_curve_renderer_release(__in __notnull void* renderer)
{
    _ogl_curve_renderer* renderer_ptr = (_ogl_curve_renderer*) renderer;

    if (renderer_ptr->items != NULL)
    {
        _ogl_curve_renderer_item* item_ptr = NULL;

        while (system_resizable_vector_pop(renderer_ptr->items,
                                          &item_ptr) )
        {
            if (item_ptr != NULL)
            {
                _ogl_curve_renderer_release_item(renderer_ptr,
                                                 item_ptr);

                delete item_ptr;
                item_ptr = NULL;
            }
        } /* while (there are items left) */

        system_resizable_vector_release(renderer_ptr->items);
        renderer_ptr->items = NULL;
    }

    if (renderer_ptr->line_strip_renderer != NULL)
    {
        ogl_line_strip_renderer_release(renderer_ptr->line_strip_renderer);

        renderer_ptr->line_strip_renderer = NULL;
    }
}

/** TODO */
PRIVATE void _ogl_curve_renderer_release_item(__in __notnull _ogl_curve_renderer*      renderer_ptr,
                                              __in __notnull _ogl_curve_renderer_item* item_ptr)
{
    ogl_line_strip_renderer_delete_dataset(renderer_ptr->line_strip_renderer,
                                           item_ptr->dataset_id);

    item_ptr->dataset_id = -1;
}

/** Please see header for spec */
PUBLIC EMERALD_API ogl_curve_item_id ogl_curve_renderer_add_scene_graph_node_curve(__in            __notnull ogl_curve_renderer   renderer,
                                                                                   __in            __notnull scene_graph          graph,
                                                                                   __in            __notnull scene_graph_node     node,
                                                                                   __in_ecount(4)            const float*         curve_color,
                                                                                   __in                      system_timeline_time duration,
                                                                                   __in                      unsigned int         n_samples_per_second)
{
    ogl_curve_item_id    item_id      = -1;
    _ogl_curve_renderer* renderer_ptr = (_ogl_curve_renderer*) renderer;

    /* Prepare vertex data */
    unsigned int n_vertices  = 0;
    bool         result      = false;
    float*       vertex_data = NULL;

    result = _ogl_curve_renderer_get_scene_graph_node_vertex_data(graph,
                                                                  node,
                                                                  duration,
                                                                  n_samples_per_second,
                                                                 &n_vertices,
                                                                 &vertex_data);

    ASSERT_ALWAYS_SYNC(result              &&
                       n_vertices  != 0    &&
                       vertex_data != NULL,
                       "Could not generate vertex data");
    if (vertex_data == NULL)
    {
        goto end;
    }

    /* Spawn new item descriptor */
    _ogl_curve_renderer_item* item_ptr = new (std::nothrow) _ogl_curve_renderer_item;

    ASSERT_ALWAYS_SYNC(item_ptr != NULL, "Out of memory");
    if (item_ptr == NULL)
    {
        goto end;
    }

    /* Fill it */
    item_ptr->dataset_id = ogl_line_strip_renderer_add_dataset(renderer_ptr->line_strip_renderer,
                                                               n_vertices,
                                                               vertex_data,
                                                               curve_color);

    /* Store it */
    item_id = system_resizable_vector_get_amount_of_elements(renderer_ptr->items);

    system_resizable_vector_push(renderer_ptr->items,
                                 item_ptr);

    /* Good to release the vertex data */
    delete [] vertex_data;

    vertex_data = NULL;

end:
    return item_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_curve_renderer ogl_curve_renderer_create(__in __notnull ogl_context               context,
                                                                __in __notnull system_hashed_ansi_string name)
{
    _ogl_curve_renderer* new_instance = new (std::nothrow) _ogl_curve_renderer;

    ASSERT_DEBUG_SYNC(new_instance != NULL, "Out of memory");
    if (new_instance != NULL)
    {
        ogl_context_get_property(context,
                                 OGL_CONTEXT_PROPERTY_LINE_STRIP_RENDERER,
                                &new_instance->line_strip_renderer);

        new_instance->conversion_array_items   = NULL;
        new_instance->items                    = system_resizable_vector_create(4, /* capacity */
                                                                                sizeof(_ogl_curve_renderer_item*) );
        new_instance->n_conversion_array_items = 0;

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_instance,
                                                       _ogl_curve_renderer_release,
                                                       OBJECT_TYPE_OGL_CURVE_RENDERER,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Curve Renderers\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_instance != NULL) */

    return (ogl_curve_renderer) new_instance;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_curve_renderer_delete_curve(__in __notnull ogl_curve_renderer renderer,
                                                        __in           ogl_curve_item_id  item_id)
{
    _ogl_curve_renderer_item* item_ptr     = NULL;
    _ogl_curve_renderer*      renderer_ptr = (_ogl_curve_renderer*) renderer;

    if (!system_resizable_vector_get_element_at(renderer_ptr->items,
                                                item_id,
                                               &item_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not find requested curve descriptor");

        goto end;
    }

    /* Release the item */
    _ogl_curve_renderer_release_item(renderer_ptr,
                                     item_ptr);

    delete item_ptr;
    item_ptr = NULL;

    /* Mark the item as released */
    system_resizable_vector_set_element_at(renderer_ptr->items,
                                           item_id,
                                           NULL);

    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_curve_renderer_draw(__in __notnull                    ogl_curve_renderer   renderer,
                                                __in                              unsigned int         n_item_ids,
                                                __in_ecount(n_item_ids) __notnull ogl_curve_item_id*   item_ids,
                                                __in                              system_matrix4x4     mvp)
{
    _ogl_curve_renderer* renderer_ptr = (_ogl_curve_renderer*) renderer;

    /* Convert curve item IDs to line strip item IDs. Use a preallocated array.
     * If necessary, scale it up */
    if (renderer_ptr->n_conversion_array_items < n_item_ids)
    {
        LOG_INFO("Performance warning: scaling up conversion array");

        if (renderer_ptr->conversion_array_items != NULL)
        {
            delete [] renderer_ptr->conversion_array_items;

            renderer_ptr->conversion_array_items = NULL;
        }

        renderer_ptr->conversion_array_items = new ogl_line_strip_renderer_dataset_id[2 * n_item_ids];

        ASSERT_ALWAYS_SYNC(renderer_ptr->conversion_array_items != NULL,
                           "Out of memory");
        if (renderer_ptr->conversion_array_items == NULL)
        {
            goto end;
        }

        renderer_ptr->n_conversion_array_items = 2 * n_item_ids;
    }

    /* Fill the array with line strip IDs */
    for (unsigned int n_item_id = 0;
                      n_item_id < n_item_ids;
                    ++n_item_id)
    {
        _ogl_curve_renderer_item* item_ptr = NULL;

        if (!system_resizable_vector_get_element_at(renderer_ptr->items,
                                                    item_ids[n_item_id],
                                                   &item_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve item descriptor");

            goto end;
        }

        renderer_ptr->conversion_array_items[n_item_id] = item_ptr->dataset_id;
    } /* for (all item IDs) */

    ogl_line_strip_renderer_draw(renderer_ptr->line_strip_renderer,
                                 mvp,
                                 n_item_ids,
                                 renderer_ptr->conversion_array_items);

end:
    ;
}

