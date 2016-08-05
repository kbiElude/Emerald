/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_postprocessing_output.h"
#include "demo/nodes/nodes_postprocessing_video_segment.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "demo/demo_timeline_segment_node.h"
#include "ral/ral_context.h"
#include "ral/ral_texture.h"
#include "ral/ral_types.h"
#include "ral/ral_utils.h"
#include "system/system_assertions.h"
#include "system/system_atomics.h"
#include "system/system_callback_manager.h"
#include "system/system_dag.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_math_other.h"
#include "system/system_resizable_vector.h"


typedef struct _demo_timeline_segment_io
{
    uint32_t                   io_id; /* Can be either demo_timeline_segment_node_input_id or demo_timeline_segment_node_output_id. */
    bool                       is_input_io;
    demo_timeline_segment_node node;  /* DO NOT release */


    explicit _demo_timeline_segment_io(demo_timeline_segment_node in_node,
                                       bool                       in_is_input_io,
                                       uint32_t                   in_io_id)
    {
        io_id       = in_io_id;
        is_input_io = in_is_input_io;
        node        = in_node;
    }
} _demo_timeline_segment_io;

/** Defines a single src node output ID -> dst node input ID connection */
typedef struct _demo_timeline_segment_io_connection
{
    system_dag_connection                    dag_connection;

    struct _demo_timeline_segment_node_item* dst_node;
    demo_timeline_segment_input_id           dst_node_input_id;

    struct _demo_timeline_segment_node_item*  src_node;
    demo_timeline_segment_node_output_id      src_node_output_id;

    _demo_timeline_segment_io_connection(_demo_timeline_segment_node_item* in_dst_node,
                                         demo_timeline_segment_input_id    in_dst_node_input_id,
                                         _demo_timeline_segment_node_item* in_src_node,
                                         demo_timeline_segment_output_id   in_src_node_output_id,
                                         system_dag_connection             in_dag_connection)
    {
        dag_connection     = in_dag_connection;
        dst_node           = in_dst_node;
        dst_node_input_id  = in_dst_node_input_id;
        src_node           = in_src_node;
        src_node_output_id = in_src_node_output_id;
    }
} _demo_timeline_segment_io_connection;

typedef struct _demo_timeline_segment_node_item
{
    system_resizable_vector            cached_texture_memory_allocations;
    system_dag_node                    dag_node;
    uint32_t                           id;
    demo_timeline_segment_node         node;               /* owned */
    demo_timeline_segment_node_private node_internal;
    system_resizable_vector            outgoing_connections;    /* owns _demo_timeline_segment_io_connection* instances */
    struct _demo_timeline_segment*     parent_segment_ptr;      /* DO NOT release */
    demo_timeline_segment_node_type    type;

    _demo_timeline_segment_node_item(uint32_t                        in_id,
                                     demo_timeline_segment_node      in_node,
                                     _demo_timeline_segment*         in_parent_segment_ptr,
                                     demo_timeline_segment_node_type in_type)
    {
        cached_texture_memory_allocations = system_resizable_vector_create(sizeof(ral_texture) );
        dag_node                          = nullptr;
        id                                = in_id;
        node                              = in_node;
        node_internal                     = nullptr;
        outgoing_connections              = system_resizable_vector_create(sizeof(_demo_timeline_segment_io_connection*));
        parent_segment_ptr                = in_parent_segment_ptr;
        type                              = in_type;
    }

    ~_demo_timeline_segment_node_item()
    {
        if (cached_texture_memory_allocations != nullptr)
        {
            uint32_t n_cached_items = 0;

            system_resizable_vector_get_property(cached_texture_memory_allocations,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_cached_items);

            ASSERT_DEBUG_SYNC(n_cached_items == 0,
                              "RAL textures not cached prior to destruction");

            system_resizable_vector_release(cached_texture_memory_allocations);
            cached_texture_memory_allocations = nullptr;
        }

        if (node != nullptr)
        {
            demo_timeline_segment_node_release(node);

            node = nullptr;
        }

        if (outgoing_connections != nullptr)
        {
            _demo_timeline_segment_io_connection* current_connection_ptr = nullptr;

            while (system_resizable_vector_pop(outgoing_connections,
                                              &current_connection_ptr) )
            {
                delete current_connection_ptr;

                current_connection_ptr = nullptr;
            }

            system_resizable_vector_release(outgoing_connections);
            outgoing_connections = nullptr;
        }
    }
} _demo_timeline_segment_node_item;


typedef struct _demo_timeline_segment
{
    float                             aspect_ratio;       /* AR to use for a video segment */
    system_callback_manager           callback_manager;
    ral_context                       context;
    system_hash64map                  dag_node_to_node_descriptor_map; /* does NOT own the stored _demo_timeline_segment_node instances */
    demo_timeline_segment_id          id;
    bool                              is_teardown_in_process;
    system_hashed_ansi_string         name;
    uint32_t                          node_id_counter;
    system_hash64map                  node_id_to_node_descriptor_map; /* does NOT own the stored _demo_timeline_segment_node instances */
    system_resizable_vector           nodes;                          /* owns the stored _demo_timeline_segment_node_item instances;
                                                                       * item order DOES NOT need to correspond to node ID.
                                                                       * Use node_id_to_node_descriptor_map to find descriptor of nodes with specific IDs */
    _demo_timeline_segment_node_item* output_node_ptr;                /* DO NOT release */
    ral_format                        output_texture_format;
    demo_timeline_segment_time_mode   time_mode;
    demo_timeline                     timeline;
    demo_timeline_segment_type        type;

    system_time             end_time;
    system_time             start_time;

    /* Handles DAG-related computations. The DAG operates on segment nodes, not individual inputs or outputs.
     * Input->output dependencies need to be defined layer above, in demo_timeline_segment */
    system_dag              dag;
    system_resizable_vector dag_sorted_nodes;

    system_resizable_vector exposed_input_ios;  /* owns _demo_timeline_segment_io instances */
    system_resizable_vector exposed_output_ios; /* owns _demo_timeline_segment_io instances */

    REFCOUNT_INSERT_VARIABLES


    explicit _demo_timeline_segment(ral_context                in_context,
                                    system_hashed_ansi_string  in_name,
                                    demo_timeline              in_timeline,
                                    ral_format                 in_output_texture_format,
                                    system_time                in_start_time,
                                    system_time                in_end_time,
                                    demo_timeline_segment_type in_type,
                                    demo_timeline_segment_id   in_id)
    {
        aspect_ratio           = 1.0f;
        callback_manager       = system_callback_manager_create( static_cast<_callback_id>(DEMO_TIMELINE_SEGMENT_CALLBACK_ID_COUNT) );
        context                = in_context;
        dag                    = system_dag_create();
        dag_sorted_nodes       = system_resizable_vector_create(16 /* capacity */);
        end_time               = in_end_time;
        id                     = in_id;
        is_teardown_in_process = false;
        name                   = in_name;
        node_id_counter        = 0;
        nodes                  = system_resizable_vector_create(sizeof(_demo_timeline_segment_node_item*) );
        output_node_ptr        = nullptr;
        output_texture_format  = in_output_texture_format;
        start_time             = in_start_time;
        time_mode              = DEMO_TIMELINE_SEGMENT_TIME_MODE_RELATIVE_TO_TIMELINE;
        timeline               = in_timeline;
        type                   = in_type;

        dag_node_to_node_descriptor_map = system_hash64map_create(sizeof(_demo_timeline_segment_node_item*) );
        node_id_to_node_descriptor_map  = system_hash64map_create(sizeof(_demo_timeline_segment_node_item*) );

        exposed_input_ios  = system_resizable_vector_create(sizeof(_demo_timeline_segment_io*) );
        exposed_output_ios = system_resizable_vector_create(sizeof(_demo_timeline_segment_io*) );
    }

    ~_demo_timeline_segment()
    {
        is_teardown_in_process = true;

        if (nodes != nullptr)
        {
            /* Release all nodes. Since each node may have signed up for callbacks, use the public
             * delete function. */
            _demo_timeline_segment_node_item*  current_node_ptr    = nullptr;
            uint32_t                           n_node_items        = 0;
            demo_timeline_segment_node_id*     node_ids_cached_ptr = nullptr;
            _demo_timeline_segment_node_item** node_items_ptr      = nullptr;

            system_resizable_vector_get_property(nodes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_node_items);

            system_resizable_vector_get_property(nodes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                                &node_items_ptr);

            if (n_node_items > 0)
            {
                node_ids_cached_ptr = new demo_timeline_segment_node_id[n_node_items];

                ASSERT_DEBUG_SYNC(node_ids_cached_ptr != nullptr,
                                  "Out of memory");

                if (node_ids_cached_ptr != nullptr)
                {
                    for (uint32_t n_node_item = 0;
                                  n_node_item < n_node_items;
                                ++n_node_item)
                    {
                        demo_timeline_segment_node_get_property(node_items_ptr[n_node_item]->node,
                                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                                                node_ids_cached_ptr + n_node_item);
                    }

                    demo_timeline_segment_delete_nodes(reinterpret_cast<demo_timeline_segment>(this),
                                                        n_node_items,
                                                        node_ids_cached_ptr);

                    delete [] node_ids_cached_ptr;
                    node_ids_cached_ptr = nullptr;
                }
            }

            system_resizable_vector_release(nodes);
            nodes = nullptr;
        }

        /* Release the owned objects */
        if (dag != nullptr)
        {
            system_dag_release(dag);

            dag = nullptr;
        }

        if (dag_node_to_node_descriptor_map != nullptr)
        {
            system_hash64map_release(dag_node_to_node_descriptor_map);

            dag_node_to_node_descriptor_map = nullptr;
        }

        if (exposed_input_ios != nullptr)
        {
            _demo_timeline_segment_io* current_io_ptr = nullptr;

            while (system_resizable_vector_pop(exposed_input_ios,
                                              &current_io_ptr) )
            {
                delete current_io_ptr;

                current_io_ptr = nullptr;

            }

            system_resizable_vector_release(exposed_input_ios);
            exposed_input_ios = nullptr;
        }

        if (exposed_output_ios != nullptr)
        {
            _demo_timeline_segment_io* current_io_ptr = nullptr;

            while (system_resizable_vector_pop(exposed_output_ios,
                                              &current_io_ptr) )
            {
                delete current_io_ptr;

                current_io_ptr = nullptr;

            }

            system_resizable_vector_release(exposed_output_ios);
            exposed_output_ios = nullptr;
        }

        if (node_id_to_node_descriptor_map != nullptr)
        {
            system_hash64map_release(node_id_to_node_descriptor_map);

            node_id_to_node_descriptor_map = nullptr;
        }

        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }
    }
} _demo_timeline_segment;

/* Global declarations */
typedef struct
{
    const char*                     name;
    demo_timeline_segment_node_type type;

    PFNSEGMENTNODEDEINITCALLBACKPROC                    pfn_deinit_proc;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC               pfn_get_property_proc;
    PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC pfn_get_texture_memory_allocation_details_proc;
    PFNSEGMENTNODEINITCALLBACKPROC                      pfn_init_proc;
    PFNSEGMENTNODERENDERCALLBACKPROC                    pfn_render_proc;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC               pfn_set_property_proc;
    PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        pfn_set_texture_memory_allocation_proc;
} _node_data;

static const _node_data node_data[] =
{
    /* DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER */
    {
        "Pass renderer node",
        DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,

        nodes_video_pass_renderer_deinit,
        nodes_video_pass_renderer_get_property,
        nodes_video_pass_renderer_get_texture_memory_allocation_details,
        nodes_video_pass_renderer_init,
        nodes_video_pass_renderer_render,
        nodes_video_pass_renderer_set_property,
        nodes_video_pass_renderer_set_texture_memory_allocation,
    },

    {
        "Output node",
        DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,

        nodes_postprocessing_output_deinit,
        nullptr, /* pfn_get_property_proc                          */
        nullptr, /* pfn_get_texture_memory_allocation_details_proc */
        nodes_postprocessing_output_init,
        nodes_postprocessing_output_render,
        nullptr, /* pfn_set_property_proc                  */
        nullptr, /* pfn_set_texture_memory_allocation_proc */
    },

    /* DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT */
    {
        "Video segment node",
        DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,

        nodes_postprocessing_video_segment_deinit,
        nodes_postprocessing_video_segment_get_property,
        nullptr, /* pfn_get_texture_memory_allocation_details_proc */
        nodes_postprocessing_video_segment_init,
        nodes_postprocessing_video_segment_render,
        nodes_postprocessing_video_segment_set_property,
        nullptr, /* pfn_set_texture_memory_allocation_proc */
    }
};

/** Forward declarations */
PRIVATE system_hashed_ansi_string _demo_timeline_segment_get_video_segment_name(_demo_timeline_segment* postprocessing_segment_ptr,
                                                                                _demo_timeline_segment* video_segment_ptr);


PRIVATE bool _demo_timeline_segment_add_postprocessing_segment_video_segment_node(_demo_timeline_segment*         postprocessing_segment_ptr,
                                                                                  _demo_timeline_segment*         video_segment_ptr);
PRIVATE bool _demo_timeline_segment_enumerate_io                                 (_demo_timeline_segment*         segment_ptr,
                                                                                  bool                            should_enumerate_inputs,
                                                                                  uint32_t*                       out_n_ios_ptr,
                                                                                  demo_timeline_segment_node_io** out_segment_node_ios_ptr);
PRIVATE void _demo_timeline_segment_on_postprocessing_segment_added              (const void*                     postprocessing_segment_id_void_ptr,
                                                                                        void*                     timeline_void_ptr);
PRIVATE void _demo_timeline_segment_on_postprocessing_segment_deleted            (const void*                     postprocessing_segment,
                                                                                        void*                     timeline);
PRIVATE void _demo_timeline_segment_on_texture_attached_to_node                  (const void*                     callback_arg_ptr,
                                                                                        void*                     segment);
PRIVATE void _demo_timeline_segment_on_texture_deleted                           (const void*                     callback_arg_ptr,
                                                                                        void*                     segment);
PRIVATE void _demo_timeline_segment_on_texture_detached_from_node                (const void*                     callback_arg_ptr,
                                                                                        void*                     segment);
PRIVATE void _demo_timeline_segment_on_texture_output_added_to_node              (const void*                     segment_void_ptr,
                                                                                        void*                     new_output_id);
PRIVATE void _demo_timeline_segment_on_texture_output_deleted_from_node          (const void*                     segment_void_ptr,
                                                                                        void*                     former_output_id);
PRIVATE void _demo_timeline_segment_on_video_segment_added                       (const void*                     video_segment_id_void_ptr,
                                                                                        void*                     timeline_void_ptr);
PRIVATE void _demo_timeline_segment_on_video_segment_deleted                     (const void*                     video_segment,
                                                                                        void*                     timeline);
PRIVATE void _demo_timeline_segment_on_video_segment_moved                       (const void*                     video_segment,
                                                                                        void*                     timeline);
PRIVATE void _demo_timeline_segment_on_video_segment_resized                     (const void*                     video_segment,
                                                                                        void*                     timeline);
PRIVATE void _demo_timeline_segment_release                                      (void*                           segment);
PRIVATE void _demo_timeline_segment_update_global_subscriptions                  (_demo_timeline_segment*         segment_ptr,
                                                                                  bool                            should_subscribe);
PRIVATE void _demo_timeline_segment_update_node_subscriptions                    (_demo_timeline_segment*         segment_ptr,
                                                                                  demo_timeline_segment_node      node,
                                                                                  bool                            should_subscribe);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline_segment,
                               demo_timeline_segment,
                              _demo_timeline_segment);


/** TODO */
PRIVATE bool _demo_timeline_segment_add_postprocessing_segment_video_segment_node(_demo_timeline_segment* postprocessing_segment_ptr,
                                                                                  _demo_timeline_segment* video_segment_ptr)
{
    demo_timeline_segment_node     new_video_segment_node    = nullptr;
    demo_timeline_segment_node_id  new_video_segment_node_id = -1;
    bool                           result                    = false;

    if (!demo_timeline_segment_add_node(reinterpret_cast<demo_timeline_segment>(postprocessing_segment_ptr),
                                        DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,
                                        _demo_timeline_segment_get_video_segment_name(postprocessing_segment_ptr,
                                                                                      video_segment_ptr),
                                        nullptr, /* out_opt_node_id_ptr */
                                       &new_video_segment_node) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a new video segment node.");

        goto end;
    }

    demo_timeline_segment_node_get_property(new_video_segment_node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                           &new_video_segment_node_id);

    demo_timeline_segment_set_node_property(reinterpret_cast<demo_timeline_segment>(postprocessing_segment_ptr),
                                            new_video_segment_node_id,
                                            NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT,
                                           &video_segment_ptr);

    /* All done */
    result = true;

end:
    return result;
}

/** TODO */
PRIVATE bool _demo_timeline_segment_enumerate_io(_demo_timeline_segment*         segment_ptr,
                                                 bool                            should_enumerate_inputs,
                                                 uint32_t*                       out_n_ios_ptr,
                                                 demo_timeline_segment_node_io** out_segment_node_ios_ptr)
{
    system_resizable_vector        exposed_io_vector           = nullptr;
    uint32_t                       n_ios                       = 0;
    bool                           result                      = false;
    demo_timeline_segment_node_io* result_segment_node_ios_ptr = nullptr;

    /* Sanity checks */
    if (segment_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    if (out_n_ios_ptr            == nullptr ||
        out_segment_node_ios_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "At least one output variable is NULL");

        goto end;
    }

    /* Retrieve the requested data */
    exposed_io_vector = (should_enumerate_inputs) ? segment_ptr->exposed_input_ios
                                                  : segment_ptr->exposed_output_ios;

    system_resizable_vector_get_property(exposed_io_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         &n_ios);

    if (n_ios > 0)
    {
        result_segment_node_ios_ptr = new demo_timeline_segment_node_io[n_ios];

        ASSERT_ALWAYS_SYNC(result_segment_node_ios_ptr != nullptr,
                           "Out of memory");

        for (uint32_t n_io = 0;
                      n_io < n_ios;
                    ++n_io)
        {
            _demo_timeline_segment_io* current_io_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(exposed_io_vector,
                                                        n_io,
                                                       &current_io_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve IO descriptor at index [%d]",
                                  n_io);

                continue;
            }

            demo_timeline_segment_node_get_property(current_io_ptr->node,
                                                    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                                   &result_segment_node_ios_ptr[n_io].node_id);

            result_segment_node_ios_ptr[n_io].is_input   = current_io_ptr->is_input_io;
            result_segment_node_ios_ptr[n_io].node_io_id = current_io_ptr->io_id;
        }
    }
    else
    {
        *out_segment_node_ios_ptr = nullptr;
    }

    /* All done */
    *out_n_ios_ptr            = n_ios;
    *out_segment_node_ios_ptr = result_segment_node_ios_ptr;
     result                   = true;

 end:
    if (!result)
    {
        delete [] result_segment_node_ios_ptr;
    }

    return result;
}

/** TODO */
PRIVATE system_hashed_ansi_string _demo_timeline_segment_get_video_segment_name(_demo_timeline_segment* postprocessing_segment_ptr,
                                                                                _demo_timeline_segment* video_segment_ptr)
{
    char temp[128];

    snprintf(temp,
             sizeof(temp),
             "Video segment node for [%s]::[%s]",
             system_hashed_ansi_string_get_buffer(postprocessing_segment_ptr->name),
             system_hashed_ansi_string_get_buffer(video_segment_ptr->name) );


    return system_hashed_ansi_string_create(temp);
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_postprocessing_segment_added(const void* postprocessing_segment_id_void_ptr,
                                                                          void* timeline_void_ptr)
{
    uint32_t                  n_video_segments           = 0;
    demo_timeline_segment_id  postprocessing_segment_id  = reinterpret_cast<demo_timeline_segment_id>(postprocessing_segment_id_void_ptr);
    _demo_timeline_segment*   postprocessing_segment_ptr = nullptr;
    demo_timeline             timeline                   = reinterpret_cast<demo_timeline>(timeline_void_ptr);
    demo_timeline_segment_id* video_segment_ids          = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    /* Retrieve video segment descriptor */
    if (!demo_timeline_get_segment_by_id(timeline,
                                         DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                         postprocessing_segment_id,
                                         reinterpret_cast<demo_timeline_segment*>(&postprocessing_segment_ptr)) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve handle of a postprocessing segment with ID [%d]",
                          postprocessing_segment_id);

        goto end;
    }

    /* Run over all video segments and add a video segment node to the new postprocessing segment
     * for each video segment. */
    if (!demo_timeline_get_segment_ids(timeline,
                                       DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                       &n_video_segments,
                                       &video_segment_ids) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve video segment IDs");

        goto end;
    }

    for (uint32_t n_video_segment = 0;
                  n_video_segment < n_video_segments;
                ++n_video_segment)
    {
        demo_timeline_segment current_video_segment = nullptr;

        if (!demo_timeline_get_segment_by_id(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                             video_segment_ids[n_video_segment],
                                            &current_video_segment) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve video segment with ID [%d]",
                              video_segment_ids[n_video_segment]);

            continue;
        }

        if (!_demo_timeline_segment_add_postprocessing_segment_video_segment_node(postprocessing_segment_ptr,
                                                                                  reinterpret_cast<_demo_timeline_segment*>(current_video_segment) ))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create a new video segment node.");
        }
    }

    /* All done */
end:
    if (video_segment_ids != nullptr)
    {
        demo_timeline_free_segment_ids(video_segment_ids);

        video_segment_ids = nullptr;
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_postprocessing_segment_deleted(const void* postprocessing_segment,
                                                                            void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_texture_attached_to_node(const void* callback_arg,
                                                                      void* segment)
{
    const demo_timeline_segment_node_callback_texture_attached_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_attached_callback_argument*>(callback_arg);
    _demo_timeline_segment*                                                       segment_ptr      = reinterpret_cast<_demo_timeline_segment*>                                                      (segment);

    /* TODO: We now have all info in the callback arg to perform this action much more efficiently */

    ASSERT_DEBUG_SYNC(segment_ptr != nullptr,
                      "Input segment instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_ptr->texture != nullptr,
                      "Input texture instance is NULL");

    LOG_INFO("Performance warning: new texture attached to the node.");

    /* If the texture has been attached to a node output, we need to traverse the list of connections defined
     * for that node and bind that texture to inputs of nodes which are connected to that node output.
     *
     * Currently we do not receive info about which node the event is coming from so run through all nodes
     * and see if input texture attachments are defined. Shouldn't be much of perf concern, since this event
     * should only be fired at init time, or when explicitly requested by the app.
     *
     * TODO: This is no longer true - callback_arg holds this info. This needs to be taken into account.
     */
    uint32_t n_nodes = 0;

    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    for (uint32_t n_destination_node = 0;
                  n_destination_node < n_nodes;
                ++n_destination_node)
    {
        demo_timeline_segment_input_id    current_destination_node_input_ids[32]; /* TODO: we may need something more fancy in the future.. */
        _demo_timeline_segment_node_item* current_destination_node_ptr           = nullptr;
        uint32_t                          n_current_destination_node_input_ids   = 0;

        if (!system_resizable_vector_get_element_at(segment_ptr->nodes,
                                                    n_destination_node,
                                                   &current_destination_node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve descriptor for node at index [%d]",
                              n_destination_node);

            continue;
        }

        demo_timeline_segment_node_get_inputs(current_destination_node_ptr->node,
                                              sizeof(current_destination_node_input_ids) / sizeof(current_destination_node_input_ids[0]),
                                             &n_current_destination_node_input_ids,
                                              current_destination_node_input_ids);

        ASSERT_DEBUG_SYNC(n_current_destination_node_input_ids < sizeof(current_destination_node_input_ids) / sizeof(current_destination_node_input_ids[0]),
                          "TODO");

        for (uint32_t n_input_id = 0;
                      n_input_id < n_current_destination_node_input_ids;
                    ++n_input_id)
        {
            ral_texture                         bound_texture                = nullptr;
            demo_timeline_segment_node_input_id current_destination_input_id = current_destination_node_input_ids[n_input_id];
            bool                                has_bound_texture_to_input   = false;

            demo_timeline_segment_node_get_io_property(current_destination_node_ptr->node,
                                                       true, /* is_input_io */
                                                       current_destination_input_id,
                                                       DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                      &bound_texture);

            if (bound_texture != nullptr)
            {
                /* Nothing to do for this input */
                continue;
            }

            /* Look through defined connections and see if a connection binding this input with any other segment node output is defined.
             * If so, check if a texture is attached to that output and - if that's the case - attach it to the input. */
            for (uint32_t n_source_node = 0;
                          n_source_node < n_nodes && !has_bound_texture_to_input;
                        ++n_source_node)
            {
                uint32_t                          n_connections_defined = 0;
                _demo_timeline_segment_node_item* source_node_ptr       = nullptr;

                /* Feedback loops are forbidden */
                if (n_source_node == n_destination_node)
                {
                    continue;
                }

                if (!system_resizable_vector_get_element_at(segment_ptr->nodes,
                                                            n_source_node,
                                                           &source_node_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve node descriptor for node with index [%d]",
                                      n_source_node);

                    continue;
                }

                system_resizable_vector_get_property(source_node_ptr->outgoing_connections,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                    &n_connections_defined);

                for (uint32_t n_connection = 0;
                              n_connection < n_connections_defined && !has_bound_texture_to_input;
                            ++n_connection)
                {
                    _demo_timeline_segment_io_connection* connection_ptr = nullptr;

                    if (!system_resizable_vector_get_element_at(source_node_ptr->outgoing_connections,
                                                                n_connection,
                                                               &connection_ptr) )
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Could not retrieve connection descriptor at index [%d]",
                                          n_connection);

                        continue;
                    }

                    if (connection_ptr->dst_node == current_destination_node_ptr)
                    {
                        /* OK, we have a matching connection. Does the source node expose a texture? */
                        ral_texture source_node_bound_texture = nullptr;

                        demo_timeline_segment_node_get_io_property(connection_ptr->src_node->node,
                                                                   false, /* is_input_io */
                                                                   connection_ptr->src_node_output_id,
                                                                   DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                                  &source_node_bound_texture);

                        if (source_node_bound_texture != nullptr)
                        {
                            /* Bind that texture to the currently processed destination node and then move on to the next destination node. */
                            demo_texture_attachment_declaration new_texture_attachment;

                            new_texture_attachment.texture_ral = source_node_bound_texture;

                            if (!demo_timeline_segment_node_attach_texture_to_texture_io(current_destination_node_ptr->node,
                                                                                         false, /* is_input_id */
                                                                                         current_destination_input_id,
                                                                                        &new_texture_attachment) )
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Could not attach a texture to the destination node's input ID");
                            }

                            has_bound_texture_to_input = true;
                        }
                    }
                }
            }
        }
    }

    /* Notify any subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_ATTACHED_TO_NODE,
                                      callback_arg_ptr);
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_texture_deleted(const void* callback_arg,
                                                             void* segment)
{
    const ral_context_callback_objects_deleted_callback_arg* callback_arg_ptr = reinterpret_cast<const ral_context_callback_objects_deleted_callback_arg*>(callback_arg);
    uint32_t                                                 n_node_items     = 0;
    _demo_timeline_segment*                                  segment_ptr      = reinterpret_cast<_demo_timeline_segment*>(segment);

    ASSERT_DEBUG_SYNC(callback_arg_ptr->object_type == RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                      "Invalid callback object type");

    /* Iterate over all node descriptors and wipe the specified texture instance out of maintained
     * node texture caches. */
    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_node_items);

    for (uint32_t n_node_item = 0;
                  n_node_item < n_node_items;
                ++n_node_item)
    {
        size_t                            deleted_texture_index = ITEM_NOT_FOUND;
        uint32_t                          n_cached_textures     = 0;
        _demo_timeline_segment_node_item* current_node_item_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(segment_ptr->nodes,
                                                    n_node_item,
                                                   &current_node_item_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node item descriptor at index [%d]",
                              n_node_item);

            continue;
        }

        for (uint32_t n_deleted_texture = 0;
                      n_deleted_texture < callback_arg_ptr->n_objects;
                    ++n_deleted_texture)
        {
            deleted_texture_index = system_resizable_vector_find(current_node_item_ptr->cached_texture_memory_allocations,
                                                                 callback_arg_ptr->deleted_objects[n_deleted_texture]);

            if (deleted_texture_index != ITEM_NOT_FOUND)
            {
                system_hashed_ansi_string segment_name = segment_ptr->name;
                system_hashed_ansi_string texture_name = nullptr;

                ral_texture_get_property((ral_texture) callback_arg_ptr->deleted_objects[n_deleted_texture],
                                         RAL_TEXTURE_PROPERTY_NAME,
                                        &texture_name);

                LOG_INFO("Removed texture [%s] from cached texture memory allocations for segment [%s]",
                         system_hashed_ansi_string_get_buffer(texture_name) ,
                         system_hashed_ansi_string_get_buffer(segment_name) );

                system_resizable_vector_delete_element_at(current_node_item_ptr->cached_texture_memory_allocations,
                                                          deleted_texture_index);
            }
        }
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_texture_detached_from_node(const void* callback_arg,
                                                                        void* segment)
{
    const demo_timeline_segment_node_callback_texture_detached_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_detached_callback_argument*>(callback_arg);
    uint32_t                                                                      n_nodes          = 0;
    _demo_timeline_segment_node_item*                                             node_item_ptr    = nullptr;
    _demo_timeline_segment*                                                       segment_ptr      = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* TODO: We now have all info in the callback arg to perform this action much more efficiently */

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment_ptr != nullptr,
                      "Input segment instance is NULL");
    ASSERT_DEBUG_SYNC(callback_arg_ptr->texture != nullptr,
                      "Input texture instance is NULL");

    /* This call-back func will be invoked in two cases:
     *
     * 1) A texture formerly attached to a segment node output was detached. In this case, we want to
     *    return that texture to the pool and ensure any inputs, to which that texture used to be bound to,
     *    have that texture detached, too.
     *
     * 2) A texture formerly attached to a segment node input was detached. We need to specifically
     *    *ignore* this scenario, because otherwise we would have entered an endless loop.
     *
     * Iterate over all node inputs, identify the one, to which the texture used to be attached, and detach
     * that texture from any node inputs that that output was connected to.
     */
    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        _demo_timeline_segment_node_item*    current_node_ptr = nullptr;
        uint32_t                             n_node_outputs   = 0;
        demo_timeline_segment_node_output_id node_output_ids[32]; /* TODO: We may need something more fancy in the future.. */

        if (!system_resizable_vector_get_element_at(segment_ptr->nodes,
                                                    n_node,
                                                   &current_node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node descriptor at index [%d]",
                              n_node);

            continue;
        }

        demo_timeline_segment_node_get_outputs(current_node_ptr->node,
                                               sizeof(node_output_ids) / sizeof(node_output_ids[0]),
                                              &n_node_outputs,
                                               node_output_ids);

        ASSERT_DEBUG_SYNC(n_node_outputs < sizeof(node_output_ids) / sizeof(node_output_ids[0]),
                          "TODO");

        for (uint32_t n_node_output_id = 0;
                      n_node_output_id < n_node_outputs;
                    ++n_node_output_id)
        {
            ral_texture                          current_node_bound_texture = nullptr;
            uint32_t                             current_node_n_connections = 0;
            demo_timeline_segment_node_output_id current_node_output_id     = node_output_ids[n_node_output_id];

            demo_timeline_segment_node_get_io_property(current_node_ptr->node,
                                                       false, /* is_input_io */
                                                       current_node_output_id,
                                                       DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                      &current_node_bound_texture);

            if (current_node_bound_texture != callback_arg_ptr->texture)
            {
                /* The about-to-be-detached texture is not exposed by this node output. Move on */
                continue;
            }

            /* OK, iterate over all outgoing connections of this node and identify those connections which start from
             * the output we're currently looking at. */
            system_resizable_vector_get_property(current_node_ptr->outgoing_connections,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &current_node_n_connections);

            for (uint32_t n_connection = 0;
                          n_connection < current_node_n_connections;
                        ++n_connection)
            {
                _demo_timeline_segment_io_connection* current_connection_ptr = nullptr;

                if (!system_resizable_vector_get_element_at(current_node_ptr->outgoing_connections,
                                                            n_connection,
                                                            &current_connection_ptr) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not retrieve connection descriptor at index [%d]",
                                      n_connection);

                    continue;
                }

                if (current_connection_ptr->src_node           == current_node_ptr       &&
                    current_connection_ptr->src_node_output_id == current_node_output_id)
                {
                    ral_texture dst_node_bound_texture = nullptr;

                    /* Sanity check.. */
                    demo_timeline_segment_node_get_io_property(current_connection_ptr->dst_node->node,
                                                               true, /* is_input_io */
                                                               current_connection_ptr->dst_node_input_id,
                                                               DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                              &dst_node_bound_texture);

                    ASSERT_DEBUG_SYNC(dst_node_bound_texture == callback_arg_ptr->texture,
                                      "Invalid bound texture detected at connection destination");

                    /* Detach the texture */
                    demo_timeline_segment_node_attach_texture_to_texture_io(current_connection_ptr->dst_node->node,
                                                                            true, /* is_input_id */
                                                                            current_connection_ptr->dst_node_input_id,
                                                                            nullptr); /* texture_attachment_ptr */
                }
            }
        }
    }

    /* If the detached texture was part of the texture cache associated with the node, remove it from there. */
    uint32_t n_current_node_item = -1;
    uint32_t n_total_nodes       = 0;

    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_total_nodes);

    for (  n_current_node_item = 0;
           n_current_node_item < n_total_nodes;
         ++n_current_node_item)
    {
        _demo_timeline_segment_node_item* current_node_item_ptr = nullptr;

        system_resizable_vector_get_element_at(segment_ptr->nodes,
                                               n_current_node_item,
                                              &current_node_item_ptr);

        if (current_node_item_ptr->node == callback_arg_ptr->node)
        {
            size_t find_result;

            find_result = system_resizable_vector_find(current_node_item_ptr->cached_texture_memory_allocations,
                                                       callback_arg_ptr->texture);

            if (find_result != ITEM_NOT_FOUND)
            {
                system_resizable_vector_delete_element_at(current_node_item_ptr->cached_texture_memory_allocations,
                                                          find_result);
            }

            break;
        }
    }

    /* The texture attachment was provided by the segment. We use the rendering context-wide texture pool to
     * ensure textures are reused whenever possible.
     *
     * Since the texture is no longer needed, return it back to the pool.
     *
     * TODO: We need a refcounted solution here to properly support cases where the same texture is
     *       consumed by multiple inputs !
     */
    ral_context_delete_objects(segment_ptr->context,
                               RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                               1, /* n_objects */
                               reinterpret_cast<void* const*>(&callback_arg_ptr->texture) );

    /* Notify any subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_DETACHED_FROM_NODE,
                                      callback_arg);
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_texture_output_added_to_node(const void* callback_data_ptr,
                                                                          void* segment_void_ptr)
{
    const demo_timeline_segment_node_callback_texture_output_added_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_output_added_callback_argument*>(callback_data_ptr);
    _demo_timeline_segment*                                                           segment_ptr      = reinterpret_cast<_demo_timeline_segment*>                                                          (segment_void_ptr);

    /* Forward the request to other nodes which signed up for the notification */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_ADDED_TO_NODE,
                                      callback_data_ptr);
}

/** TODO */
PRIVATE void _demo_timeline_segment_on_texture_output_deleted_from_node(const void* callback_data_ptr,
                                                                              void* segment_void_ptr)
{
    const demo_timeline_segment_node_callback_texture_output_deleted_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_output_deleted_callback_argument*>(callback_data_ptr);
    _demo_timeline_segment*                                                             segment_ptr      = reinterpret_cast<_demo_timeline_segment*>                                                            (segment_void_ptr);

    /* Forward the request to other nodes which signed up for the notification */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_DELETED_FROM_NODE,
                                      callback_data_ptr);
}

/** Callback entry-point. Called whenever a new video segment is added to the timeline.
 *
 *  TODO
 *
 *  @param new_video_segment TODO
 *  @param timeline          TODO
 **/
PRIVATE void _demo_timeline_segment_on_video_segment_added(const void* video_segment_id_void_ptr,
                                                                 void* timeline_void_ptr)
{
    uint32_t                  n_postprocessing_segment_ids = 0;
    demo_timeline_segment_id* postprocessing_segment_ids   = nullptr;
    demo_timeline             timeline                     = reinterpret_cast<demo_timeline>           (timeline_void_ptr);
    demo_timeline_segment_id  video_segment_id             = reinterpret_cast<demo_timeline_segment_id>(video_segment_id_void_ptr);
    _demo_timeline_segment*   video_segment_ptr            = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(timeline != nullptr,
                      "Input timeline instance is NULL");

    /* Retrieve video segment descriptor */
    if (!demo_timeline_get_segment_by_id(timeline,
                                         DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                         video_segment_id,
                                         reinterpret_cast<demo_timeline_segment*>(&video_segment_ptr) ))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve handle of a video segment with ID [%d]",
                          video_segment_id);

        goto end;
    }

    /* Create video segment nodes in all postprocessing nodes we know of */
    if (!demo_timeline_get_segment_ids(timeline,
                                       DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                      &n_postprocessing_segment_ids,
                                      &postprocessing_segment_ids) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve the array of post-processing segment IDs");

        goto end;
    }

    for (uint32_t n_postprocessing_segment_id = 0;
                  n_postprocessing_segment_id < n_postprocessing_segment_ids;
                ++n_postprocessing_segment_id)
    {
        demo_timeline_segment          current_postprocessing_segment     = nullptr;
        const demo_timeline_segment_id current_postprocessing_segment_id  = postprocessing_segment_ids[n_postprocessing_segment_id];

        if (!demo_timeline_get_segment_by_id(timeline,
                                             DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                             current_postprocessing_segment_id,
                                            &current_postprocessing_segment) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve handle of the post-processing segment with ID [%d]",
                              current_postprocessing_segment_id);

            continue;
        }

        /* Create a new video segment node for the post-processing segment */
        if (!_demo_timeline_segment_add_postprocessing_segment_video_segment_node( reinterpret_cast<_demo_timeline_segment*>(current_postprocessing_segment),
                                                                                   video_segment_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create a new video segment node.");
        }
    }

    /* All done */
end:
    if (postprocessing_segment_ids != nullptr)
    {
        demo_timeline_free_segment_ids(postprocessing_segment_ids);
    }

}

/** Callback entry-point. Called whenever a video segment is deleted from the timeline.
 *
 *  TODO
 *
 *  @param video_segment TODO
 *  @param timeline      TODO
 **/
PRIVATE void _demo_timeline_segment_on_video_segment_deleted(const void* video_segment,
                                                                   void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** Callback entry-point. Called whenever a video segment is moved on the timeline.
 *
 *  TODO
 *
 *  @param video_segment TODO
 *  @param timeline      TODO
 **/
PRIVATE void _demo_timeline_segment_on_video_segment_moved(const void* video_segment,
                                                                 void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** Callback entry-point. Called whenever a video segment is resized on the timeline.
 *
 *  TODO
 *
 *  @param video_segment TODO
 *  @param timeline      TODO
 **/
PRIVATE void _demo_timeline_segment_on_video_segment_resized(const void* video_segment,
                                                                    void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/* TODO */
PRIVATE void _demo_timeline_segment_release(void* segment)
{
    _demo_timeline_segment* segment_ptr = reinterpret_cast<_demo_timeline_segment*>(segment);

    _demo_timeline_segment_update_global_subscriptions(segment_ptr,
                                                       false /* should_subscribe */);

    /* Ping any interested parties about the fact we're winding up */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                      reinterpret_cast<void*>(segment_ptr) );

    /* Actual tear-down will be handled by the destructor. */
}

/** TODO */
PRIVATE void _demo_timeline_segment_update_global_subscriptions(_demo_timeline_segment* segment_ptr,
                                                                bool                    should_subscribe)
{
    /* NOTE: This function is called every time a segment is created or deleted, even though we're adjusting global
     *       subscriptions. In order to support this, subscriptions are now ref-counted to avoid duplicate registrations
     *       or problems with signing out of notifications in cases like this.
     */
    system_callback_manager context_callback_manager  = nullptr;
    system_callback_manager timeline_callback_manager = nullptr;

    ral_context_get_property  (segment_ptr->context,
                               RAL_CONTEXT_PROPERTY_CALLBACK_MANAGER,
                              &context_callback_manager);
    demo_timeline_get_property(segment_ptr->timeline,
                               DEMO_TIMELINE_PROPERTY_CALLBACK_MANAGER,
                              &timeline_callback_manager);

    if (should_subscribe)
    {
        /* Sign up for timeline notifications. We need to react whenever new video segments, that overlap with the time region
         * covered by this postprocessing segment, show up or are removed from the timeline, or are moved or resized.
         */
        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_video_segment_added,
                                                        segment_ptr->timeline);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_video_segment_deleted,
                                                        segment_ptr->timeline);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_video_segment_moved,
                                                        segment_ptr->timeline);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_video_segment_resized,
                                                        segment_ptr->timeline);

        /* Don't forget about notifications realted to postprocessing segments */
        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_postprocessing_segment_added,
                                                        segment_ptr->timeline);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_postprocessing_segment_deleted,
                                                        segment_ptr->timeline);

        /* Hook up to context notifications, so that we know when context objects go out of scope. */
        system_callback_manager_subscribe_for_callbacks(context_callback_manager,
                                                        RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_texture_deleted,
                                                        segment_ptr);
    }
    else
    {
        /* Sign out of notifications */
        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
                                                           _demo_timeline_segment_on_video_segment_added,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_DELETED,
                                                           _demo_timeline_segment_on_video_segment_deleted,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,
                                                           _demo_timeline_segment_on_video_segment_moved,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,
                                                           _demo_timeline_segment_on_video_segment_resized,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_ADDED,
                                                           _demo_timeline_segment_on_postprocessing_segment_added,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                           DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_DELETED,
                                                           _demo_timeline_segment_on_postprocessing_segment_deleted,
                                                           segment_ptr->timeline);

        system_callback_manager_unsubscribe_from_callbacks(context_callback_manager,
                                                          RAL_CONTEXT_CALLBACK_ID_TEXTURES_DELETED,
                                                          _demo_timeline_segment_on_texture_deleted,
                                                          segment_ptr);
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_update_node_subscriptions(_demo_timeline_segment*    segment_ptr,
                                                              demo_timeline_segment_node node,
                                                              bool                       should_subscribe)
{
    system_callback_manager new_segment_node_callback_manager = nullptr;

    demo_timeline_segment_node_get_property(node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,
                                           &new_segment_node_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_texture_output_added_to_node,
                                                        segment_ptr);
        system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_texture_output_deleted_from_node,
                                                        segment_ptr);
        system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_texture_detached_from_node,
                                                        segment_ptr);
        system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_segment_on_texture_attached_to_node,
                                                        segment_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(new_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                           _demo_timeline_segment_on_texture_output_added_to_node,
                                                           segment_ptr);
        system_callback_manager_unsubscribe_from_callbacks(new_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_DELETED,
                                                           _demo_timeline_segment_on_texture_output_deleted_from_node,
                                                           segment_ptr);
        system_callback_manager_unsubscribe_from_callbacks(new_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                                           _demo_timeline_segment_on_texture_detached_from_node,
                                                           segment_ptr);
        system_callback_manager_unsubscribe_from_callbacks(new_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,
                                                           _demo_timeline_segment_on_texture_attached_to_node,
                                                           segment_ptr);
    }
}

/** TODO */
PRIVATE void _demo_timeline_segment_update_node_texture_memory_allocations(_demo_timeline_segment_node_item* node_ptr)
{
    demo_texture_memory_allocation                      current_alloc_details;
    uint32_t                                            n_current_alloc                          = 0;
    PFNSEGMENTNODEGETTEXTUREMEMORYALLOCATIONDETAILSPROC pfn_get_texture_memory_alloc_details_ptr = nullptr;
    PFNSEGMENTNODESETTEXTUREMEMORYALLOCATIONPROC        pfn_set_texture_memory_allocation_ptr    = nullptr;

    demo_timeline_segment_node_get_property(node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_TEXTURE_MEMORY_ALLOCATION_DETAILS_FUNC_PTR,
                                           &pfn_get_texture_memory_alloc_details_ptr);
    demo_timeline_segment_node_get_property(node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_TEXTURE_MEMORY_ALLOCATION_FUNC_PTR,
                                           &pfn_set_texture_memory_allocation_ptr);

    if (pfn_get_texture_memory_alloc_details_ptr == nullptr)
    {
        /* No texture memory allocations needed for the node. */
        goto end;
    }

    if (pfn_set_texture_memory_allocation_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No set_texture_memory_allocation callback set for a node which can request for texture memory allocations.");

        goto end;
    }

    while (pfn_get_texture_memory_alloc_details_ptr(node_ptr->node_internal,
                                                    n_current_alloc,
                                                   &current_alloc_details) )
    {
        ral_texture new_texture              = nullptr;
        uint32_t    requested_texture_depth  = 0;
        uint32_t    requested_texture_height = 0;
        uint32_t    requested_texture_width  = 0;

        ++n_current_alloc;

        if (current_alloc_details.bound_texture != nullptr)
        {
            /* Nothing to do.
             *
             * TODO: This will not hold if the allocation requirements change. We might need to do some checking here. */
            continue;
        }

        /* What is the absolute texture size requested for the texture? */
        switch (current_alloc_details.size.mode)
        {
            case DEMO_TEXTURE_SIZE_MODE_ABSOLUTE:
            {
                switch (current_alloc_details.type)
                {
                    case RAL_TEXTURE_TYPE_1D:
                    case RAL_TEXTURE_TYPE_1D_ARRAY:
                    {
                        requested_texture_width  = current_alloc_details.size.absolute_size[0];
                        requested_texture_height = 0;
                        requested_texture_depth  = 0;

                        break;
                    }

                    case RAL_TEXTURE_TYPE_2D:
                    case RAL_TEXTURE_TYPE_2D_ARRAY:
                    case RAL_TEXTURE_TYPE_CUBE_MAP:
                    case RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY:
                    case RAL_TEXTURE_TYPE_MULTISAMPLE_2D:
                    case RAL_TEXTURE_TYPE_MULTISAMPLE_2D_ARRAY:
                    {
                        requested_texture_width  = current_alloc_details.size.absolute_size[0];
                        requested_texture_height = current_alloc_details.size.absolute_size[1];
                        requested_texture_depth  = 0;

                        if (current_alloc_details.size.mode == RAL_TEXTURE_TYPE_CUBE_MAP        ||
                            current_alloc_details.size.mode == RAL_TEXTURE_TYPE_CUBE_MAP_ARRAY)
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "For CM or CM Array textures, texture width must match texture height.");

                            continue;
                        }

                        break;
                    }

                    case RAL_TEXTURE_TYPE_3D:
                    {
                        requested_texture_width  = current_alloc_details.size.absolute_size[0];
                        requested_texture_height = current_alloc_details.size.absolute_size[1];
                        requested_texture_depth  = current_alloc_details.size.absolute_size[2];

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unsupported texture type");
                    }
                }

                break;
            }

            case DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL:
            {
                uint32_t context_fb_size[2] = {0};

                ral_context_get_property(node_ptr->parent_segment_ptr->context,
                                         RAL_CONTEXT_PROPERTY_SYSTEM_FB_SIZE,
                                         context_fb_size);

                requested_texture_depth  = 1;
                requested_texture_width  = int(float(context_fb_size[0]) * current_alloc_details.size.proportional_size[0]);
                requested_texture_height = int(float(context_fb_size[1]) * current_alloc_details.size.proportional_size[1]);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized texture size mode requested.");
            }
        }

        /* Alloc */
        ral_texture_create_info new_texture_create_info;

        new_texture_create_info.base_mipmap_depth      = requested_texture_depth;
        new_texture_create_info.base_mipmap_height     = requested_texture_height;
        new_texture_create_info.base_mipmap_width      = requested_texture_width;
        new_texture_create_info.fixed_sample_locations = false;
        new_texture_create_info.format                 = current_alloc_details.format;
        new_texture_create_info.name                   = current_alloc_details.name;
        new_texture_create_info.n_layers               = current_alloc_details.n_layers;
        new_texture_create_info.n_samples              = current_alloc_details.n_samples;
        new_texture_create_info.type                   = current_alloc_details.type;
        new_texture_create_info.usage                  = RAL_TEXTURE_USAGE_SAMPLED_BIT; /* TODO? */
        new_texture_create_info.use_full_mipmap_chain  = current_alloc_details.needs_full_mipmap_chain;

        ral_context_create_textures(node_ptr->parent_segment_ptr->context,
                                    1, /* n_textures */
                                   &new_texture_create_info,
                                   &new_texture);

        if (new_texture == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve a new texture from the texture pool.");

            continue;
        }

        /* Ask the node to make a good use of the texture we've just allocated. This will usually
         * (but does not have to) include things like attaching the texture to one of the node's
         * outputs, or setting up a FBO to render to that texture at rendering time.
         */
        pfn_set_texture_memory_allocation_ptr(node_ptr->node_internal,
                                              n_current_alloc - 1,
                                              new_texture);

        /* Cache the texture allocated to the node. We need this to automatically release all textures
         * the moment the node goes out of scope and needs to be destroyed. */
        system_resizable_vector_push(node_ptr->cached_texture_memory_allocations,
                                     new_texture);
    }

end:
    ;
}


/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_add_node(demo_timeline_segment           segment,
                                                       demo_timeline_segment_node_type node_type,
                                                       system_hashed_ansi_string       name,
                                                       demo_timeline_segment_node_id*  out_opt_node_id_ptr,
                                                       demo_timeline_segment_node*     out_opt_node_ptr)
{
    bool                              is_output_node   = false;
    uint32_t                          new_node_id      = -1;
    _demo_timeline_segment_node_item* new_node_ptr     = nullptr;
    demo_timeline_segment_node        new_segment_node = nullptr;
    bool                              result           = false;
    _demo_timeline_segment*           segment_ptr      = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment is NULL");

        goto end;
    }

    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid video segment node name");

        goto end;
    }

    is_output_node = (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING && node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT);

    if (is_output_node                        &&
         segment_ptr->output_node_ptr != nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output node already created for the specified segment");

        goto end;
    }

    /* Try to create the requested node type. */
    new_node_id = system_atomics_increment(&segment_ptr->node_id_counter);

    ASSERT_DEBUG_SYNC(node_data[node_type].type == node_type,
                      "Node data array corruption");

    new_segment_node = demo_timeline_segment_node_create(segment_ptr->context,
                                                         segment_ptr->type,
                                                         node_type,
                                                         new_node_id,
                                                         node_data[node_type].pfn_deinit_proc,
                                                         node_data[node_type].pfn_get_property_proc,
                                                         node_data[node_type].pfn_get_texture_memory_allocation_details_proc,
                                                         node_data[node_type].pfn_init_proc,
                                                         node_data[node_type].pfn_render_proc,
                                                         node_data[node_type].pfn_set_property_proc,
                                                         node_data[node_type].pfn_set_texture_memory_allocation_proc,
                                                         system_hashed_ansi_string_create(node_data[node_type].name) );

    if (new_segment_node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create new video segment node.");

        goto end;
    }

    /* Create the node descriptor */
    new_node_ptr = new (std::nothrow) _demo_timeline_segment_node_item(new_node_id,
                                                                       new_segment_node,
                                                                       segment_ptr,
                                                                       node_type);

    if (new_node_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        goto end;
    }

    /* Cache if this is an output node */
    switch (segment_ptr->type)
    {
        case DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING:
        {
            if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT)
            {
                segment_ptr->output_node_ptr = new_node_ptr;
            }

            break;
        }

        case DEMO_TIMELINE_SEGMENT_TYPE_VIDEO:
        {
            /* Video segments do not use output nodes */
            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Segment type was not recognized.");
        }
    }

    /* Sign up for node call-backs */
    _demo_timeline_segment_update_node_subscriptions(segment_ptr,
                                                     new_segment_node,
                                                     true /* should_subscribe */);

    /* Create a DAG node for the new video segment node and store it */
    new_node_ptr->dag_node = system_dag_add_node(segment_ptr->dag,
                                                 static_cast<system_dag_node_value>(new_node_ptr) );

    if (new_node_ptr->dag_node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a DAG node");

        goto end;
    }

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(segment_ptr->dag_node_to_node_descriptor_map,
                                                 reinterpret_cast<system_hash64>(new_node_ptr->dag_node) ),
                      "DAG node is already recognized by the DAG node->node descriptor map.");

    system_hash64map_insert(segment_ptr->dag_node_to_node_descriptor_map,
                            reinterpret_cast<system_hash64>(new_node_ptr->dag_node),
                            new_node_ptr,
                            nullptr,  /* on_removal_callback */
                            nullptr); /* callback_argument */

    /* Store the node descriptor */
    ASSERT_DEBUG_SYNC(!system_hash64map_contains(segment_ptr->node_id_to_node_descriptor_map,
                                                 new_node_id),
                      "Node id->node descriptor map already holds an item for the new node's ID");

    system_hash64map_insert     (segment_ptr->node_id_to_node_descriptor_map,
                                 new_node_id,
                                 new_node_ptr,
                                 nullptr,  /* on_remove_callback */
                                 nullptr); /* callback_argument  */
    system_resizable_vector_push(segment_ptr->nodes,
                                 new_node_ptr);

    /* If the node needs to be initialized, do it now */
    if (node_data[node_type].pfn_init_proc != nullptr)
    {
        new_node_ptr->node_internal = node_data[node_type].pfn_init_proc(reinterpret_cast<demo_timeline_segment>(new_node_ptr->parent_segment_ptr),
                                                                         new_node_ptr->node,
                                                                         new_node_ptr->parent_segment_ptr->context);
    }

    /* Add the only input to the output node.
     *
     * For video segments, we additionally want to expose that output as an input, so that post-processing segment can use
     * the data generated by the video segment for further composition.
     */
    if (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_VIDEO && is_output_node)
    {
        demo_timeline_segment_node_input_id output_segment_node_input_id = -1;
        demo_texture_io_declaration         output_segment_node_input_info;

        output_segment_node_input_info.is_attachment_required = true;
        output_segment_node_input_info.name                   = system_hashed_ansi_string_create("Result");
        output_segment_node_input_info.texture_format         = segment_ptr->output_texture_format;
        output_segment_node_input_info.texture_n_components   = 4;
        output_segment_node_input_info.texture_n_layers       = 1;
        output_segment_node_input_info.texture_n_samples      = 1;
        output_segment_node_input_info.texture_type           = RAL_TEXTURE_TYPE_2D;

        ral_utils_get_format_property(segment_ptr->output_texture_format,
                                      RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                     &output_segment_node_input_info.texture_n_components);

        if (!demo_timeline_segment_node_add_texture_input(new_segment_node,
                                                         &output_segment_node_input_info,
                                                         &output_segment_node_input_id) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not add texture input to the output node.");

            goto end;
        }

        if (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
        {
            if (!demo_timeline_segment_expose_node_io(segment,
                                                      true, /* is_input_io */
                                                      new_node_id,
                                                      output_segment_node_input_id,
                                                      true,    /* should_expose */
                                                      false) ) /* should_expose_as_vs_input */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not expose texture input of the output node.");
            }
        }
    }

    /* For each texture memory allocation, make sure a texture object is provided.
     *
     * TODO: This is TEMPORARY and will be delegated to the rendering back-end. For OpenGL, we could use sparse textures.
     *       For other APIs, well.. :)
     */
    _demo_timeline_segment_update_node_texture_memory_allocations(new_node_ptr);

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_NODE_CREATED,
                                      reinterpret_cast<void*>(new_node_id) );

    /* All done */
    if (out_opt_node_id_ptr != nullptr)
    {
        *out_opt_node_id_ptr = new_node_id;
    }

    if (out_opt_node_ptr != nullptr)
    {
        *out_opt_node_ptr = new_segment_node;
    }

    result = true;

end:
    if (!result)
    {
        if (new_node_ptr != nullptr)
        {
            delete new_node_ptr;

            new_node_ptr = nullptr;
        }

        if (new_segment_node != nullptr)
        {
            demo_timeline_segment_node_release(new_segment_node);

            new_segment_node = nullptr;
        }
    }

    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_connect_nodes(demo_timeline_segment                segment,
                                                            demo_timeline_segment_node_id        src_node_id,
                                                            demo_timeline_segment_node_output_id src_node_output_id,
                                                            demo_timeline_segment_node_id        dst_node_id,
                                                            demo_timeline_segment_node_input_id  dst_node_input_id)
{
    _demo_timeline_segment_node_item*     dst_node_ptr       = nullptr;
    bool                                  need_to_update     = true;
    _demo_timeline_segment_io_connection* new_connection_ptr = nullptr;
    system_dag_connection                 new_dag_connection = nullptr;
    bool                                  result             = false;
    _demo_timeline_segment*               segment_ptr        = reinterpret_cast<_demo_timeline_segment*>(segment);
    _demo_timeline_segment_node_item*     src_node_ptr       = nullptr;

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    if (dst_node_id == src_node_id)
    {
        LOG_FATAL("Node loop-backs are forbidden (src_node_id == %u == dst_node_id).",
                  src_node_id);

        goto end;
    }

    /* Retrieve node descriptors */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              static_cast<system_hash64>(src_node_id),
                              &src_node_ptr) ||
        !system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              static_cast<system_hash64>(dst_node_id),
                              &dst_node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve node descriptor(s)");

        goto end;
    }

    /* Check if the input & output are compatible.. */
    if (!demo_timeline_segment_node_is_node_output_compatible_with_node_input(src_node_ptr->node,
                                                                              src_node_output_id,
                                                                              dst_node_ptr->node,
                                                                              dst_node_input_id) )
    {
        LOG_FATAL("Source node's output is incompatible with destination node's input.");

        goto end;
    }

    /* Since the DAG we're using operates on nodes, a segment node A<->segment node B connection may already
     * be defined for different input ID & output ID pairs. If that's the case, make sure the user is not
     * trying to redefine the same connection (which is worrying, so we should definitely log a message if we
     * detect it and then bail out). Otherwise, don't add a new connection to the DAG. */
    if (system_dag_is_connection_defined(segment_ptr->dag,
                                         src_node_ptr->dag_node,
                                         dst_node_ptr->dag_node) )
    {
        bool     is_connection_already_defined = false;
        uint32_t n_outgoing_connections        = 0;

        system_resizable_vector_get_property(src_node_ptr->outgoing_connections,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_outgoing_connections);

        for (uint32_t n_outgoing_connection = 0;
                      n_outgoing_connection < n_outgoing_connections;
                    ++n_outgoing_connection)
        {
            _demo_timeline_segment_io_connection* current_connection_ptr = nullptr;

            if (!system_resizable_vector_get_element_at(src_node_ptr->outgoing_connections,
                                                        n_outgoing_connection,
                                                       &current_connection_ptr) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve connection descriptor at index [%d]",
                                  n_outgoing_connection);

                continue;
            }

            if (current_connection_ptr->dst_node           == dst_node_ptr      &&
                current_connection_ptr->dst_node_input_id  == dst_node_input_id &&
                current_connection_ptr->src_node           == src_node_ptr      &&
                current_connection_ptr->src_node_output_id == src_node_output_id)
            {
                new_dag_connection            = current_connection_ptr->dag_connection;
                is_connection_already_defined = true;

                break;
            }
        }

        if (is_connection_already_defined)
        {
            LOG_FATAL("Cannot add a new video segment node->video segment node connection: specified connection is already defined");

            goto end;
        }
        else
        {
            /* The segments are already connected - no need to update the DAG, as well as attach output's attachment
             * to the specified input. */
            need_to_update = false;
        }
    }

    if (need_to_update)
    {
        demo_timeline_segment_node_interface_type src_node_output_interface_type = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN;

        /* Check if the DAG is still acyclic after we include the new connection */
        #ifdef _DEBUG
        {
            bool dag_solves_ok = system_dag_solve(segment_ptr->dag);

            ASSERT_DEBUG_SYNC(dag_solves_ok,
                              "DAG does not solve OK even without the new connection!");
        }
        #endif

        new_dag_connection = system_dag_add_connection(segment_ptr->dag,
                                                       src_node_ptr->dag_node,
                                                       dst_node_ptr->dag_node);

        if (new_dag_connection == nullptr)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create a new DAG connection");

            goto end;
        }

        if (!system_dag_solve(segment_ptr->dag) )
        {
            /* Whoops, the DAG does not solve now. Remove the new connection, ensure it solves correct again,
             * report an error and quit */
            system_dag_delete_connection(segment_ptr->dag,
                                         new_dag_connection);

            ASSERT_ALWAYS_SYNC(system_dag_solve(segment_ptr->dag),
                               "DAG does not solve after a new connection was removed");

            goto end;
        }

        /* The output's attachment needs to be bound to the specified input
         *
         * TODO: This code needs ot be moved to a DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED handler.
         *       If any of the node output's texture attachments change after creation, we're going to crash if
         *       the former texture is released.
         */
        demo_timeline_segment_node_get_io_property(src_node_ptr->node,
                                                   false, /* is_input_io */
                                                   src_node_output_id,
                                                   DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_INTERFACE_TYPE,
                                                  &src_node_output_interface_type);

        switch (src_node_output_interface_type)
        {
            case DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE:
            {
                demo_texture_attachment_declaration new_attachment;
                ral_texture                         src_output_texture = nullptr;

                demo_timeline_segment_node_get_io_property(src_node_ptr->node,
                                                           false, /* is_input_io */
                                                           src_node_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                          &src_output_texture);

                if (src_output_texture == nullptr)
                {
                    ASSERT_DEBUG_SYNC(src_output_texture != nullptr,
                                      "No texture bound to a node output");

                    break;
                }

                /* Attach the texture attachment to the node input */
                new_attachment.texture_ral = src_output_texture;

                demo_timeline_segment_node_attach_texture_to_texture_io(dst_node_ptr->node,
                                                                        true, /* is_input_id */
                                                                        dst_node_input_id,
                                                                       &new_attachment);

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized node IO interface type.")
            }
        }
    }

    /* Create the new connection descriptor & store it */
    new_connection_ptr = new (std::nothrow) _demo_timeline_segment_io_connection(dst_node_ptr,
                                                                                 dst_node_input_id,
                                                                                 src_node_ptr,
                                                                                 src_node_output_id,
                                                                                 new_dag_connection);

    if (new_connection_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(new_connection_ptr != nullptr,
                          "Out of memory");

        goto end;
    }

    system_resizable_vector_push(dst_node_ptr->outgoing_connections,
                                 new_connection_ptr);

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NODE_INPUT_IS_NOW_CONNECTED,
                                      src_node_ptr);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC demo_timeline_segment demo_timeline_segment_create_postprocessing(ral_context               context,
                                                                         demo_timeline             owner_timeline,
                                                                         system_hashed_ansi_string name,
                                                                         system_time               start_time,
                                                                         system_time               end_time,
                                                                         demo_timeline_segment_id  id)
{
    ral_format              context_fb_color_format = RAL_FORMAT_UNKNOWN;
    _demo_timeline_segment* new_segment_ptr         = nullptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != nullptr,
                      "Rendering context handle is NULL");
    ASSERT_DEBUG_SYNC(owner_timeline != nullptr,
                      "Owner timeline is NULL");
    ASSERT_DEBUG_SYNC(name != nullptr                                &&
                      system_hashed_ansi_string_get_length(name) > 0,
                      "Invalid postprocessing segment name specified");

    /* Retrieve format of the system FB's color attachment.
     *
     * TODO: Some postprocessors will want to use HDR color format. We shouldn't be using FB format here. */
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT,
                             &context_fb_color_format);

    /* Create a new instance */
    new_segment_ptr = new (std::nothrow) _demo_timeline_segment(context,
                                                                name,
                                                                owner_timeline,
                                                                context_fb_color_format,
                                                                start_time,
                                                                end_time,
                                                                DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                                                id);

    ASSERT_ALWAYS_SYNC(new_segment_ptr != nullptr,
                       "Out of memory");

    if (new_segment_ptr != nullptr)
    {
        /* Add the output node */
        bool result = demo_timeline_segment_add_node(reinterpret_cast<demo_timeline_segment>(new_segment_ptr),
                                                     DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,
                                                     system_hashed_ansi_string_create("Output node"),
                                                     nullptr,  /* out_opt_node_id_ptr */
                                                     nullptr); /* out_opt_node_ptr */

        ASSERT_DEBUG_SYNC(result,
                          "Could not add the output node to the post-processing segment.");

        /* Subscribe for notifications */
        _demo_timeline_segment_update_global_subscriptions(new_segment_ptr,
                                                           true); /* should_subscribe */

        /* Finally, register the object */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_segment_ptr,
                                                       _demo_timeline_segment_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE_SEGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timeline Segment\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return reinterpret_cast<demo_timeline_segment>(new_segment_ptr);
}

/** Please see header for specification */
PUBLIC demo_timeline_segment demo_timeline_segment_create_video(ral_context               context,
                                                                demo_timeline             owner_timeline,
                                                                ral_format                output_texture_format,
                                                                system_hashed_ansi_string name,
                                                                system_time               start_time,
                                                                system_time               end_time,
                                                                demo_timeline_segment_id  id)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != nullptr,
                      "Rendering context handle is NULL");
    ASSERT_DEBUG_SYNC(owner_timeline != nullptr,
                      "Owner timeline is NULL");
    ASSERT_DEBUG_SYNC(name != nullptr                                &&
                      system_hashed_ansi_string_get_length(name) > 0,
                      "Invalid video segment name specified");
    ASSERT_DEBUG_SYNC(output_texture_format < RAL_FORMAT_COUNT,
                      "Invalid output texture format requested.");
    ASSERT_DEBUG_SYNC(start_time < end_time,
                      "Segment's start time does not precede its end time");

    /* Create a new instance */
    _demo_timeline_segment* new_segment_ptr = new (std::nothrow) _demo_timeline_segment(context,
                                                                                        name,
                                                                                        owner_timeline,
                                                                                        output_texture_format,
                                                                                        start_time,
                                                                                        end_time,
                                                                                        DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                                                                        id);

    ASSERT_ALWAYS_SYNC(new_segment_ptr != nullptr,
                       "Out of memory");

    if (new_segment_ptr != nullptr)
    {
        /* Subscribe for notifications */
        _demo_timeline_segment_update_global_subscriptions(new_segment_ptr,
                                                           true); /* should_subscribe */

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_segment_ptr,
                                                       _demo_timeline_segment_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE_SEGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timeline Segment\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return reinterpret_cast<demo_timeline_segment>(new_segment_ptr);
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_delete_nodes(demo_timeline_segment                segment,
                                               uint32_t                             n_nodes,
                                               const demo_timeline_segment_node_id* node_ids)
{
    bool                    result      = false;
    _demo_timeline_segment* segment_ptr = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    if (n_nodes == 0)
    {
        /* Don't waste any cycles.. */
        goto end;
    }

    if (node_ids == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL node IDs array but n_nodes=[%d]",
                          n_nodes);

        goto end;
    }

    /* Iterate over all nodes and ensure they can be safely deleted */
    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        ral_texture*                      cached_texture_attachments   = nullptr;
        uint32_t                          n_cached_texture_attachments = 0;
        uint32_t                          n_node_output_ids            = 0;
        _demo_timeline_segment_node_item* node_ptr                     = nullptr;

        if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                                  static_cast<system_hash64>(node_ids[n_node]),
                                  &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find node descriptor for node with ID [%d]",
                              node_ids[n_node]);

            goto end;
        }

        /* The node must not be an output node, unless the segment is in the tear-down mode. */
        if (!segment_ptr->is_teardown_in_process && (segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING              &&
                                                     node_ptr->type    == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Node ID [%d] refers to an output node which cannot be deleted",
                              node_ids[n_node]);

            goto end;
        }

        /* If the node is a part of any existing DAG connections, we need to:
         *
         * 1. Detach any output attachments of the about-to-be-removed node from
         *    any node inputs this node is connected to. This is done by 
         * 2. Release these textures.
         * 3. Remove the connection.
         */
        demo_timeline_segment_node_get_outputs(node_ptr->node,
                                               0, /* n_output_ids */
                                               &n_node_output_ids,
                                               nullptr /* out_opt_output_ids_ptr */);

        if (n_node_output_ids > 0)
        {
            demo_timeline_segment_output_id* node_output_ids = new (std::nothrow) demo_timeline_segment_output_id[n_node_output_ids];

            ASSERT_ALWAYS_SYNC(node_output_ids != nullptr,
                               "Out of memory");

            demo_timeline_segment_node_get_outputs(node_ptr->node,
                                                   n_node_output_ids,
                                                   nullptr, /* out_opt_n_output_ids */
                                                   node_output_ids);

            for (uint32_t n_node_output_id = 0;
                          n_node_output_id < n_node_output_ids;
                        ++n_node_output_id)
            {
                if (!demo_timeline_segment_node_attach_texture_to_texture_io(node_ptr->node,
                                                                             false, /* is_input_id */
                                                                             node_output_ids[n_node_output_id],
                                                                             nullptr /* texture_attachment_ptr */) )
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Could not detach a texture attachment");
                }
            }

            delete [] node_output_ids;
            node_output_ids = nullptr;
        }

        system_dag_delete_connections(segment_ptr->dag,
                                      node_ptr->dag_node, /* src */
                                      nullptr);           /* dst */
        system_dag_delete_connections(segment_ptr->dag,
                                      nullptr,             /* dst */
                                      node_ptr->dag_node); /* src */

        /* Release texture attachments assigned to the node */
        system_resizable_vector_get_property(node_ptr->cached_texture_memory_allocations,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_cached_texture_attachments);
        system_resizable_vector_get_property(node_ptr->cached_texture_memory_allocations,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                            &cached_texture_attachments);

        if (n_cached_texture_attachments > 0 && !ral_context_delete_objects(node_ptr->parent_segment_ptr->context,
                                                                            RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                                                            n_cached_texture_attachments,
                                                                            reinterpret_cast<void* const*>(cached_texture_attachments) ))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Texture attachment release process failed.");
        }

        system_resizable_vector_clear(node_ptr->cached_texture_memory_allocations);

        #ifdef _DEBUG
        {
            /* At this point, the node must not be a part of any existing DAG connections */
            if (system_dag_is_connection_defined(segment_ptr->dag,
                                                 node_ptr->dag_node,
                                                 nullptr) ||          /* dst */
                system_dag_is_connection_defined(segment_ptr->dag,
                                                 nullptr,             /* src */
                                                 node_ptr->dag_node) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Node ID [%d] forms one or more DAG connections.",
                                  node_ids[n_node]);

                goto end;
            }
        }
        #endif /* _DEBUG */
    }

    /* Good to delete the nodes */
    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        system_callback_manager           node_callback_manager = nullptr;
        size_t                            node_find_index       = -1;
        PFNSEGMENTNODEDEINITCALLBACKPROC  node_pfn_deinit_proc  = nullptr;
        _demo_timeline_segment_node_item* node_ptr              = nullptr;

        if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                                  static_cast<system_hash64>(node_ids[n_node]),
                                  &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find node descriptor for node with ID [%d]",
                              node_ids[n_node]);

            goto end;
        }

        /* If there's a deinit callback func ptr defined, call it now */
        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,
                                               &node_callback_manager);
        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_DEINIT_FUNC_PTR,
                                               &node_pfn_deinit_proc);

        if (node_pfn_deinit_proc != nullptr)
        {
            node_pfn_deinit_proc(node_ptr->node_internal);
        }

        /* Sign out of notifications we signed up for at creation time */
        _demo_timeline_segment_update_node_subscriptions(segment_ptr,
                                                         node_ptr->node,
                                                         false /* should_subscribe */);

        /* Release the node */
        if (!system_hash64map_remove(segment_ptr->node_id_to_node_descriptor_map,
                                     static_cast<system_hash64>(node_ids[n_node])) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not remove video segment node decriptor from the node ID->node descriptor map.");
        }

        if ( (node_find_index = system_resizable_vector_find(segment_ptr->nodes,
                                                             reinterpret_cast<void*>(node_ptr) )) != ITEM_NOT_FOUND)
        {
            system_resizable_vector_delete_element_at(segment_ptr->nodes,
                                                      node_find_index);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not remove video segment node descriptor from the node vector.");
        }

        delete node_ptr;
        node_ptr = nullptr;
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_disconnect_nodes(demo_timeline_segment                segment,
                                                   demo_timeline_segment_node_id        src_node_id,
                                                   demo_timeline_segment_node_output_id src_node_output_id,
                                                   demo_timeline_segment_node_id        dst_node_id,
                                                   demo_timeline_segment_node_output_id dst_node_input_id)
{
    uint32_t                              connection_index                = -1;
    _demo_timeline_segment_io_connection* connection_ptr                  = nullptr;
    _demo_timeline_segment_node_item*     dst_node_ptr                    = nullptr;
    uint32_t                              n_src_dst_connections           = 0;
    uint32_t                              n_src_node_outgoing_connections = 0;
    bool                                  result                          = false;
    _demo_timeline_segment*               segment_ptr                     = reinterpret_cast<_demo_timeline_segment*>(segment);
    _demo_timeline_segment_node_item*     src_node_ptr                    = nullptr;

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment instance is NULL");

        goto end;
    }

    /* Retrieve node descriptors */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              static_cast<system_hash64>(src_node_id),
                              &src_node_ptr) ||
        !system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              static_cast<system_hash64>(dst_node_id),
                              &dst_node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not map source/destination node ID to a node descriptor");

        goto end;
    }

    /* Find the specified connection's handle. Since we're looping over all outgoing connections,
     * also count all connections from the source node to the destination node - we're going to need
     * this for the next step. */
    system_resizable_vector_get_property(src_node_ptr->outgoing_connections,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_node_outgoing_connections);

    for (uint32_t n_outgoing_connection = 0;
                  n_outgoing_connection < n_src_node_outgoing_connections;
                ++n_outgoing_connection)
    {
        _demo_timeline_segment_io_connection* current_connection_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(src_node_ptr->outgoing_connections,
                                                    n_outgoing_connection,
                                                   &current_connection_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve descriptor of the segment node->video segment node connection at index [%d]",
                              n_outgoing_connection);

            continue;
        }

        ASSERT_DEBUG_SYNC(current_connection_ptr->src_node == src_node_ptr,
                          "Outgoing connection vector corruption detected");

        if (current_connection_ptr->dst_node == dst_node_ptr &&
            current_connection_ptr->src_node == src_node_ptr)
        {
            ++n_src_dst_connections;

            if (current_connection_ptr->dst_node_input_id  == dst_node_input_id &&
                current_connection_ptr->src_node_output_id == src_node_output_id)
            {
                ASSERT_DEBUG_SYNC(connection_ptr == nullptr,
                                  "Duplicated segment node connections detected");

                connection_index = n_outgoing_connection;
                connection_ptr   = current_connection_ptr;
            }
        }
    }

    if (connection_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified node connection was not found");

        goto end;
    }

    /* Two DAG nodes are only connected if there's one or more connections between segment nodes represented by those DAG nodes.
     * Two video segment nodes are only connected if there's at least one outgoing connection from source node to the destination node.
     *
     * Therefore, we can only destroy the DAG connection if there's only one outgoing connections that go from the source node
     * to the destination node.
     */
    if (n_src_dst_connections == 1)
    {
        system_dag_delete_connection(segment_ptr->dag,
                                     connection_ptr->dag_connection);
    }

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NODE_INPUT_NO_LONGER_CONNECTED,
                                      connection_ptr->src_node);

    /* Release the connection descriptor and clean up */
    delete connection_ptr;
    connection_ptr = nullptr;

    system_resizable_vector_delete_element_at(src_node_ptr->outgoing_connections,
                                              connection_index);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_expose_node_io(demo_timeline_segment         segment,
                                                             bool                          is_input_io,
                                                             demo_timeline_segment_node_id node_id,
                                                             uint32_t                      node_io_id,
                                                             bool                          should_expose,
                                                             bool                          should_expose_as_vs_input)
{
    uint32_t                          exposed_io_index  = -1; /* only used if @param should_expose is false */
    _demo_timeline_segment_io*        exposed_io_ptr    = nullptr;
    system_resizable_vector           exposed_io_vector = nullptr;
    bool                              is_io_exposed     = false;
    uint32_t                          n_ios_exposed     = 0;
    _demo_timeline_segment_node_item* node_item_ptr     = nullptr;
    bool                              result            = false;
    _demo_timeline_segment*           segment_ptr       = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    if (segment_ptr->type != DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Only video segment node IOs can be exposed.");

        goto end;
    }

    exposed_io_vector = (should_expose_as_vs_input) ? segment_ptr->exposed_input_ios
                                                    : segment_ptr->exposed_output_ios;

    system_resizable_vector_get_property(exposed_io_vector,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_ios_exposed);

    for (uint32_t n_io = 0;
                  n_io < n_ios_exposed;
                ++n_io)
    {
        _demo_timeline_segment_io* current_io_ptr = nullptr;

        if (!system_resizable_vector_get_element_at(exposed_io_vector,
                                                    n_io,
                                                   &current_io_ptr))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve IO descriptor at index [%d]",
                              n_io);

            continue;
        }

        if (current_io_ptr->io_id       == node_io_id &&
            current_io_ptr->is_input_io == is_input_io)
        {
            exposed_io_index = n_io;
            exposed_io_ptr   = current_io_ptr;
            is_io_exposed    = true;

            break;
        }
    }

    if (!should_expose && !is_io_exposed)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot hide a node IO - IO not exposed");

        goto end;
    }

    if (should_expose && is_io_exposed)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot expose a node IO - IO already exposed");

        goto end;
    }

    /* Retrieve the IO descriptor */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              static_cast<system_hash64>(node_id),
                              &node_item_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve descriptor for node with ID [%d]",
                          node_id);

        goto end;
    }

    /* Update the exposed IOs vector accordingly */
    if (should_expose)
    {
        _demo_timeline_segment_io* new_io_ptr = nullptr;

        new_io_ptr = new (std::nothrow) _demo_timeline_segment_io(node_item_ptr->node,
                                                                  is_input_io,
                                                                  node_io_id);

        ASSERT_ALWAYS_SYNC(new_io_ptr != nullptr,
                           "Out of memory");

        system_resizable_vector_push(exposed_io_vector,
                                     new_io_ptr);
    }
    else
    {
        /* Release & remove the IO descriptor */
        if (!system_resizable_vector_delete_element_at(exposed_io_vector,
                                                       exposed_io_index) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not remove the IO descriptor at index [%d]",
                              exposed_io_index);

            goto end;
        }

        delete exposed_io_ptr;
        exposed_io_ptr = nullptr;
    }

    /* Notify subscribers */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      ( should_expose &&  should_expose_as_vs_input) ? DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_INPUT_EXPOSED
                                    : ( should_expose && !should_expose_as_vs_input) ? DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_OUTPUT_EXPOSED
                                    : (!should_expose &&  should_expose_as_vs_input) ? DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_NO_LONGER_EXPOSED
                                                                                     : DEMO_TIMELINE_SEGMENT_CALLBACK_ID_OUTPUT_NO_LONGER_EXPOSED,
                                      reinterpret_cast<void*>(segment_ptr->id) );

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_free_exposed_io_result(demo_timeline_segment_node_io* segment_node_ios_ptr)
{
    delete [] segment_node_ios_ptr;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_get_exposed_inputs(demo_timeline_segment           segment,
                                                     uint32_t*                       out_n_inputs_ptr,
                                                     demo_timeline_segment_node_io** out_segment_node_ios_ptr)
{
    return _demo_timeline_segment_enumerate_io(reinterpret_cast<_demo_timeline_segment*>(segment),
                                               true, /* should_enumerate_inputs */
                                               out_n_inputs_ptr,
                                               out_segment_node_ios_ptr);
}

/** TODO */
PUBLIC bool demo_timeline_segment_get_exposed_outputs(demo_timeline_segment           segment,
                                                      uint32_t*                       out_n_outputs_ptr,
                                                      demo_timeline_segment_node_io** out_segment_node_ios_ptr)
{
    return _demo_timeline_segment_enumerate_io(reinterpret_cast<_demo_timeline_segment*>(segment),
                                               false, /* should_enumerate_inputs */
                                               out_n_outputs_ptr,
                                               out_segment_node_ios_ptr);
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_get_node_io_property_by_node_id(demo_timeline_segment                  segment,
                                                                  demo_timeline_segment_node_id          node_id,
                                                                  bool                                   is_input_io,
                                                                  uint32_t                               io_id,
                                                                  demo_timeline_segment_node_io_property property,
                                                                  void*                                  out_result_ptr)
{
    _demo_timeline_segment_node_item* node_ptr    = nullptr;
    bool                              result      = false;
    _demo_timeline_segment*           segment_ptr = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity check */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    /* Retrieve the node descriptor */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              node_id,
                             &node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No node descriptor found for node ID [%d]",
                          node_id);

        goto end;
    }

    demo_timeline_segment_node_get_io_property(node_ptr->node,
                                               is_input_io,
                                               io_id,
                                               property,
                                               out_result_ptr);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_segment_get_node_property(demo_timeline_segment         segment,
                                                    demo_timeline_segment_node_id node_id,
                                                    int                           property,
                                                    void*                         out_result_ptr)
{
    _demo_timeline_segment_node_item*     node_ptr                       = nullptr;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_node_property_func_ptr = nullptr;
    bool                                  result                         = false;
    _demo_timeline_segment*               segment_ptr                    = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity check */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    /* Retrieve the node descriptor */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              node_id,
                             &node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No node descriptor found for node ID [%d]",
                          node_id);

        goto end;
    }

    /* Retrieve the getter func ptr and call it */
    demo_timeline_segment_node_get_property(node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_PROPERTY_FUNC_PTR,
                                           &pfn_get_node_property_func_ptr);

    if (pfn_get_node_property_func_ptr != nullptr)
    {
        result = pfn_get_node_property_func_ptr(node_ptr->node_internal,
                                                property,
                                                out_result_ptr);
    }
    else
    {
        result = false;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void demo_timeline_segment_get_property(demo_timeline_segment          segment,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr)
{
    _demo_timeline_segment* segment_ptr = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != nullptr,
                      "Input video segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            *reinterpret_cast<float*>(out_result_ptr) = segment_ptr->aspect_ratio;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = segment_ptr->callback_manager;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME:
        {
            *reinterpret_cast<system_time*>(out_result_ptr) = segment_ptr->end_time;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_ID:
        {
            *reinterpret_cast<demo_timeline_segment_id*>(out_result_ptr) = segment_ptr->id;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = segment_ptr->name;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE:
        {
            *reinterpret_cast<demo_timeline_segment_node*>(out_result_ptr) = segment_ptr->output_node_ptr->node;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID:
        {
            ASSERT_DEBUG_SYNC(segment_ptr->output_node_ptr != nullptr,
                              "DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID query issued against non-postprocessing segment.");

            *reinterpret_cast<demo_timeline_segment_node_id*>(out_result_ptr) = segment_ptr->output_node_ptr->id;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME:
        {
            *reinterpret_cast<system_time*>(out_result_ptr) = segment_ptr->start_time;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_TIME_MODE:
        {
            *reinterpret_cast<demo_timeline_segment_time_mode*>(out_result_ptr) = segment_ptr->time_mode;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_TIMELINE:
        {
            *reinterpret_cast<demo_timeline*>(out_result_ptr) = segment_ptr->timeline;

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_TYPE:
        {
            *reinterpret_cast<demo_timeline_segment_type*>(out_result_ptr) = segment_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property value.")
        }
    }
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_get_video_segment_node_id_for_ps_and_vs_segments(demo_timeline_segment          postprocessing_segment,
                                                                                               demo_timeline_segment          video_segment,
                                                                                               demo_timeline_segment_node_id* out_node_id_ptr)
{
    /* TODO: Optimize me! */
    uint32_t                n_ps_nodes = 0;
    _demo_timeline_segment* ps_ptr     = reinterpret_cast<_demo_timeline_segment*>(postprocessing_segment);
    bool                    result     = false;
    _demo_timeline_segment* vs_ptr     = reinterpret_cast<_demo_timeline_segment*>(video_segment);

    /* Sanity checks */
    if (out_node_id_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "out_node_id_ptr argument is NULL");

        goto end;
    }

    if (postprocessing_segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "postprocessing_segment argument is NULL");

        goto end;
    }

    if (video_segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "video_segment argument is NULL");

        goto end;
    }

    if (ps_ptr->type != DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Segment under postprocessing_segment is not a post-processing segment.");

        goto end;
    }

    if (vs_ptr->type != DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Segment under video_segment is not a video segment");

        goto end;
    }

    /* Iterate over all nodes of the postprocessing segment and identify the node created to encapsulate
     * the specified video segment
     *
     * TODO: This really could be optimized by caching video segment -1-> postprocessing segment -n-> video
     *       segment node id mappings */
    system_resizable_vector_get_property(ps_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_ps_nodes);

    for (uint32_t n_ps_node = 0;
                  n_ps_node < n_ps_nodes;
                ++n_ps_node)
    {
        demo_timeline_segment                 node_owned_video_segment   = nullptr;
        _demo_timeline_segment_node_item*     node_ptr                   = nullptr;
        demo_timeline_segment_node_type       node_type                  = (demo_timeline_segment_node_type) -1;
        PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_node_property_proc = nullptr;

        if (!system_resizable_vector_get_element_at(ps_ptr->nodes,
                                                    n_ps_node,
                                                    &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node descriptor at index [%d]",
                              n_ps_node);

            continue;
        }

        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_TYPE,
                                               &node_type);

        if (node_type != DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT)
        {
            continue;
        }

        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_GET_PROPERTY_FUNC_PTR,
                                               &pfn_get_node_property_proc);

        if (pfn_get_node_property_proc == nullptr)
        {
            continue;
        }

        pfn_get_node_property_proc(node_ptr->node_internal,
                                   NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT,
                                  &node_owned_video_segment);

        if (node_owned_video_segment == video_segment)
        {
            *out_node_id_ptr = node_ptr->id;
             result          = true;

             break;
        }
    }

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_render(demo_timeline_segment segment,
                                                     uint32_t              frame_index,
                                                     system_time           rendering_pipeline_time,
                                                     const int*            rendering_area_px_topdown)
{
    uint32_t                n_sorted_nodes = 0;
    bool                    result         = false;
    _demo_timeline_segment* segment_ptr    = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != nullptr,
                      "Input video segment is NULL");

    /* 1. Retrieve a list of nodes, sorted topogically, in which we should proceed with the rendering process */
    if (!system_dag_get_topologically_sorted_node_values(segment_ptr->dag,
                                                         segment_ptr->dag_sorted_nodes) )
    {
        LOG_ERROR("Segment DAG failed to solve. No output will be produced.");

        goto end;
    }

    /* 2. Perform the rendering process. For each node, before calling the render call-back, make sure that:
     *
     *    a) all inputs that require a bound texture actually have one attached. If not, the render func
     *       ptr should not be called and we should signal a problem.
     *    b) if the node offers one or more outputs, make sure at least one output is connected to another
     *       node. Otherwise, do not render the node if we're dealing with a post-processing segment; for video
     *       segments, this case is skipped since there's no output node there and all nodes are assumed to be
     *       important.
     *    c) if the node does not offer any outputs and it's not an output node, skip it.
     *
     *       This case is especially important for post-processing segment's video segment nodes.
     *
     *  TODO: The result of the checks below could be cached and only re-done whenever the graph changes.
     */
    system_resizable_vector_get_property(segment_ptr->dag_sorted_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         &n_sorted_nodes);

    for (uint32_t n_sorted_node = 0;
                  n_sorted_node < n_sorted_nodes;
                ++n_sorted_node)
    {
        bool                                 can_render          = true;
        uint32_t                             n_node_input_ids    = 0;
        uint32_t                             n_node_output_ids   = 0;
        demo_timeline_segment_node_input_id  node_input_ids [32]; /* TODO: we may need something more fancy.. */
        demo_timeline_segment_node_output_id node_output_ids[32]; /* TODO: we may need something more fancy.. */
        _demo_timeline_segment_node_item*    node_ptr            = nullptr;

        if (!system_resizable_vector_get_element_at(segment_ptr->dag_sorted_nodes,
                                                    n_sorted_node,
                                                    &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node descriptor from DAG-sorted nodes vector at index [%d]",
                              n_sorted_node);

            continue;
        }

        /* Do all inputs which require an attachment, actually have one? */
        demo_timeline_segment_node_get_inputs(node_ptr->node,
                                              sizeof(node_input_ids),
                                              &n_node_input_ids,
                                              node_input_ids);

        ASSERT_DEBUG_SYNC(n_node_input_ids < sizeof(node_input_ids) / sizeof(node_input_ids[0]),
                          "TODO");

        for (uint32_t n_node_input_id = 0;
                      n_node_input_id < n_node_input_ids;
                    ++n_node_input_id)
        {
            ral_texture bound_texture          = nullptr;
            bool        is_attachment_required = false;

            demo_timeline_segment_node_get_io_property(node_ptr->node,
                                                       true, /* is_input_io */
                                                       node_input_ids[n_node_input_id],
                                                       DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_IS_REQUIRED,
                                                      &is_attachment_required);

            if (!is_attachment_required)
            {
                /* Meh */
                continue;
            }

            demo_timeline_segment_node_get_io_property(node_ptr->node,
                                                       true, /* is_input_io */
                                                       node_input_ids[n_node_input_id],
                                                       DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL,
                                                      &bound_texture);

            if (bound_texture == nullptr)
            {
                /* Sound the bugle! */
                system_callback_manager_call_back(node_ptr->parent_segment_ptr->callback_manager,
                                                  DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_LACKS_ATTACHMENT,
                                                  reinterpret_cast<void*>(node_input_ids[n_node_input_id]));

                /* This node cannot be rendered. */
                can_render = false;

                break;
            }
        }

        /* If the node offers outputs, is any of them actually used? */
        demo_timeline_segment_node_get_outputs(node_ptr->node,
                                               sizeof(node_output_ids),
                                              &n_node_output_ids,
                                               node_output_ids);

        ASSERT_DEBUG_SYNC(n_node_output_ids < sizeof(node_output_ids) / sizeof(node_output_ids[0]),
                          "TODO");

        if (n_node_output_ids >  0                                         &&
            segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING)
        {
            uint32_t n_node_outgoing_connections = 0;

            system_dag_get_connections(segment_ptr->dag,
                                       node_ptr->dag_node, /* src */
                                       nullptr,            /* dst */
                                       &n_node_outgoing_connections,
                                       nullptr);           /* out_opt_connections_ptr */

            if (n_node_outgoing_connections == 0)
            {
                can_render = false;
            }
        }
        else
        if (n_node_output_ids == 0                                                     &&
            segment_ptr->type == DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING             &&
            node_ptr->type    != DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT)
        {
            can_render = false;
        }

        if (can_render)
        {
            PFNSEGMENTNODERENDERCALLBACKPROC render_func_ptr = nullptr;

            demo_timeline_segment_node_get_property(node_ptr->node,
                                                    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_RENDER_FUNC_PTR,
                                                   &render_func_ptr);

            if (render_func_ptr != nullptr)
            {
                render_func_ptr(node_ptr->node_internal,
                                frame_index,
                                rendering_pipeline_time,
                                rendering_area_px_topdown);
            }
        }
    }

    /* All done */
    result = true;
end:
    return result;

}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_set_node_io_property(demo_timeline_segment_node             node,
                                                                   bool                                   is_input_io,
                                                                   uint32_t                               io_id,
                                                                   demo_timeline_segment_node_io_property property,
                                                                   const void*                            data)
{
    _demo_timeline_segment_node_item* node_ptr = reinterpret_cast<_demo_timeline_segment_node_item*>(node);
    bool                              result   = false;

    /* Sanity checks */
    if (node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* If this is a segment-specific request, handle it now. Otherwise, pass it to the general node handler */
    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_RAL:
        {
            demo_timeline_segment_node_attach_texture_to_texture_io(node_ptr->node,
                                                                    is_input_io,
                                                                    io_id,
                                                                   *reinterpret_cast<const demo_texture_attachment_declaration* const*>(data) );

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_input_property value.");
        }
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_segment_set_node_property(demo_timeline_segment         segment,
                                                                demo_timeline_segment_node_id node_id,
                                                                int                           property,
                                                                const void*                   data)
{
    _demo_timeline_segment_node_item*     node_ptr                       = nullptr;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC pfn_set_node_property_func_ptr = nullptr;
    bool                                  result                         = false;
    _demo_timeline_segment*               segment_ptr                    = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity check */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    }

    /* Retrieve the node descriptor */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              node_id,
                             &node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No node descriptor found for node ID [%d]",
                          node_id);

        goto end;
    }

    /* Retrieve the setter func ptr and call it */
    demo_timeline_segment_node_get_property(node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_SET_PROPERTY_FUNC_PTR,
                                           &pfn_set_node_property_func_ptr);

    if (pfn_set_node_property_func_ptr != nullptr)
    {
        result = pfn_set_node_property_func_ptr(node_ptr->node_internal,
                                                property,
                                                data);
    }
    else
    {
        result = false;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC void demo_timeline_segment_set_property(demo_timeline_segment          segment,
                                               demo_timeline_segment_property property,
                                               const void*                    data)
{
    _demo_timeline_segment* segment_ptr = reinterpret_cast<_demo_timeline_segment*>(segment);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != nullptr,
                      "Input  segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            segment_ptr->aspect_ratio = *reinterpret_cast<const float*>(data);

            ASSERT_DEBUG_SYNC(segment_ptr->aspect_ratio > 0.0f,
                              "A segment AR of <= 0.0 value was requested!");

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME:
        {
            segment_ptr->end_time = *reinterpret_cast<const system_time*>(data);

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME:
        {
            segment_ptr->start_time = *reinterpret_cast<const system_time*>(data);

            break;
        }

        case DEMO_TIMELINE_SEGMENT_PROPERTY_TIME_MODE:
        {
            segment_ptr->time_mode = *reinterpret_cast<const demo_timeline_segment_time_mode*>(data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property requested.");
        }
    }
}