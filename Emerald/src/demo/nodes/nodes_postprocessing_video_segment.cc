/**
 *
 *  Emerald (kbi/elude @ 2015)
 *
 *  This node encapsulates a demo_timeline_video_segment instance. It serves a few purposes:
 *
 *  - If the video segment adjusts its input/output configuration, these will be applied to
 *    the node, too.
 *  - If any object is attached to an input, this information is passed to the video segment.
 *  - If any connection is made from the node's output to an external object's input, this
 *    information will also be broadcasted to the video segment.
 *
 *
 **/
#include "shared.h"
#include "demo/nodes/nodes_postprocessing_video_segment.h"
#include "demo/demo_timeline_postprocessing_segment.h"
#include "demo/demo_timeline_video_segment.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"

typedef struct _node_data
{
    system_callback_manager              callback_manager;
    demo_timeline_segment_node_id        id;
    demo_timeline_postprocessing_segment postprocessing_segment;
    demo_timeline                        timeline;
    demo_timeline_video_segment          video_segment;

    explicit _node_data(demo_timeline_segment_node_id        in_id,
                        demo_timeline_postprocessing_segment in_postprocessing_segment,
                        demo_timeline_video_segment          in_video_segment,
                        demo_timeline                        in_timeline)
    {
        callback_manager       = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT);
        id                     = in_id;
        postprocessing_segment = in_postprocessing_segment;
        timeline               = in_timeline;
        video_segment          = in_video_segment;
    }

    ~_node_data()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */
    }
} _node_data;


/** Forward declarations */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_input_added    (const void* segment,
                                                                            void*       node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_texture_output_added   (const void* segment,
                                                                            void*       node_data);
PRIVATE void _nodes_postprocessing_video_segment_unsubscribe_from_callbacks(_node_data* node_data_ptr);
PRIVATE void _nodes_postprocessing_video_segment_video_segment_releasing   (const void* segment,
                                                                            void*       node_data);


/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_input_added(const void* new_input_id,
                                                                        void*       node_data)
{
    _node_data* node_data_ptr = (_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Let the subscribers know about the event */
    system_callback_manager_call_back(node_data_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                      (void*) new_input_id);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_output_added(const void* new_output_id,
                                                                         void*       node_data)
{
    _node_data* node_data_ptr = (_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Let the subscribers know about the event */
    system_callback_manager_call_back(node_data_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                      (void*) new_output_id);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_unsubscribe_from_callbacks(_node_data* node_data_ptr)
{
    system_callback_manager segment_callback_manager = NULL;

    demo_timeline_video_segment_get_property(node_data_ptr->video_segment,
                                             DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                            &segment_callback_manager);

    system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                       DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                       _nodes_postprocessing_video_segment_video_segment_releasing,
                                                       node_data_ptr);
    system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                       DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                                       _nodes_postprocessing_video_segment_on_texture_input_added,
                                                       node_data_ptr);
    system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                       DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                       _nodes_postprocessing_video_segment_on_texture_output_added,
                                                       node_data_ptr);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_video_segment_releasing(const void* segment,
                                                                         void*       node_data)
{
    _node_data* node_data_ptr = (_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Since this node is an adapter for the underlying video segment, we need to destroy this node the moment
     * the video segment is released */
    demo_timeline_postprocessing_segment_delete_nodes(node_data_ptr->postprocessing_segment,
                                                      1, /* n_nodes */
                                                     &node_data_ptr->id);
}


/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void nodes_postprocessing_video_segment_deinit(demo_timeline_segment_node_private node)
{
    _node_data* node_data_ptr = (_node_data*) node;

    /* Unsubscribe from the segment's callbacks.. */
    _nodes_postprocessing_video_segment_unsubscribe_from_callbacks(node_data_ptr);

    /* Use the destructor to release any objects that were allocated to handle the duties */
    delete node_data_ptr;
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_get_input_property(demo_timeline_segment_node_private        node,
                                                                  demo_timeline_segment_node_input_id       input_id,
                                                                  demo_timeline_segment_node_input_property property,
                                                                  void*                                     out_result_ptr)
{
    _node_data* node_data_ptr = (_node_data*) node;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node instance is NULL");

    /* Forward the query to the video segment instance */
    bool                                       can_forward = true;
    demo_timeline_video_segment_input_property vs_property;

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_INTERFACE_TYPE:       vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_INTERFACE_TYPE;         break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_IS_REQUIRED:          vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_IS_ATTACHMENT_REQUIRED; break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_NAME:                 vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_NAME;                   break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_FORMAT:       vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_TEXTURE_FORMAT;         break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_COMPONENTS: vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_TEXTURE_N_COMPONENTS;   break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_LAYERS:     vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_TEXTURE_N_LAYERS;       break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_N_SAMPLES:    vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_TEXTURE_N_SAMPLES;      break;
        case DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_TYPE:         vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_INPUT_PROPERTY_TEXTURE_TYPE;           break;

        default:
        {
            can_forward = false;

            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_input_property value");
        }
    } /* switch(property) */

    if (can_forward)
    {
        demo_timeline_video_segment_get_input_property(node_data_ptr->video_segment,
                                                       input_id,
                                                       vs_property,
                                                       out_result_ptr);
    } /* if (can_forward) */
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_get_output_property(demo_timeline_segment_node_private         node,
                                                                   demo_timeline_segment_node_output_id       output_id,
                                                                   demo_timeline_segment_node_output_property property,
                                                                   void*                                      out_result_ptr)
{
    _node_data* node_data_ptr = (_node_data*) node;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node instance is NULL");

    /* Forward the query to the video segment instance */
    bool                                        can_forward = true;
    demo_timeline_video_segment_output_property vs_property;

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_INTERFACE_TYPE:              vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_INTERFACE_TYPE;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_COMPONENTS:          vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_TEXTURE_COMPONENTS;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_FORMAT:              vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_TEXTURE_FORMAT;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_COMPONENTS:        vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_N_COMPONENTS;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_LAYERS:            vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_N_LAYERS;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_SAMPLES:           vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_N_SAMPLES;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_N_START_LAYER_INDEX: vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_N_START_LAYER_INDEX;
        case DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_TYPE:                vs_property = DEMO_TIMELINE_VIDEO_SEGMENT_OUTPUT_PROPERTY_TEXTURE_TYPE;

        default:
        {
            can_forward = false;

            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_output_property value");
        }
    } /* switch(property) */

    if (can_forward)
    {
        demo_timeline_video_segment_get_output_property(node_data_ptr->video_segment,
                                                        output_id,
                                                        vs_property,
                                                        out_result_ptr);
    } /* if (can_forward) */
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_get_property(demo_timeline_segment_node_private  node,
                                                            demo_timeline_segment_node_property property,
                                                            void*                               out_result_ptr)
{
    _node_data* node_data_ptr = (_node_data*) node;

    ASSERT_DEBUG_SYNC(node != NULL,
                      "Input node instance is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = node_data_ptr->callback_manager;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_property value.");
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_postprocessing_video_segment_init(demo_timeline_segment_node_id node_id,
                                                                                                         void*                         postprocessing_segment,
                                                                                                         void*                         video_segment,
                                                                                                         void*                         timeline)
{
    demo_timeline_segment_node_private result                   = NULL;
    system_callback_manager            segment_callback_manager = NULL;

    /* Sanity checks */
    if (postprocessing_segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input post-processing segment is NULL");

        goto end;
    } /* if (segment == NULL) */

    if (timeline == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input timeline instance is NULL");

        goto end;
    } /* if (timeline == NULL) */

    if (video_segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                         "Input video segment is NULL");

        goto end;
    }

    /* Spawn a new descriptor */
    _node_data* new_node_data_ptr = new (std::nothrow) _node_data(node_id,
                                                                  (demo_timeline_postprocessing_segment) postprocessing_segment,
                                                                  (demo_timeline_video_segment)          video_segment,
                                                                  (demo_timeline)                        timeline);

    if (new_node_data_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        goto end;
    } /* if (new_node_data_ptr == NULL) */

    /* Sign up for the video segment's callbacks */
    demo_timeline_video_segment_get_property( (demo_timeline_video_segment) video_segment,
                                             DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                            &segment_callback_manager);

    system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                    DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                    _nodes_postprocessing_video_segment_video_segment_releasing,
                                                    new_node_data_ptr);
    system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                    DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                    _nodes_postprocessing_video_segment_on_texture_input_added,
                                                    new_node_data_ptr);
    system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                    DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                    _nodes_postprocessing_video_segment_on_texture_output_added,
                                                    new_node_data_ptr);

    /* All done */
    result = (demo_timeline_segment_node_private) new_node_data_ptr;
end:
    return result;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_postprocessing_video_segment_render(demo_timeline_segment_node_private node,
                                                                             uint32_t                           frame_index,
                                                                             system_time                        frame_time,
                                                                             const int32_t*                     rendering_area_px_topdown,
                                                                             void*                              user_arg)
{
    _node_data* node_data_ptr = (_node_data*) node;

    return demo_timeline_video_segment_render(node_data_ptr->video_segment,
                                              frame_index,
                                              frame_time,
                                              rendering_area_px_topdown);
}
