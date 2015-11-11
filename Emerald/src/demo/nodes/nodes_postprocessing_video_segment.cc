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
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_segment.h"
#include "system/system_assertions.h"
#include "system/system_callback_manager.h"

typedef struct _nodes_postprocessing_video_segment_node_data
{
    system_callback_manager    callback_manager;
    ral_context                context;
    demo_timeline_segment_node node;
    demo_timeline_segment      postprocessing_segment;
    demo_timeline_segment      video_segment;


    explicit _nodes_postprocessing_video_segment_node_data(demo_timeline_segment_node in_node,
                                                           demo_timeline_segment      in_postprocessing_segment,
                                                           ral_context                in_context)
    {
        callback_manager       = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT);
        context                = NULL;
        node                   = in_node;
        postprocessing_segment = in_postprocessing_segment;
        video_segment          = NULL;
    }

    ~_nodes_postprocessing_video_segment_node_data()
    {
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */
    }
} _nodes_postprocessing_video_segment_node_data;


/** Forward declarations */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_input_added    (const void*                                    segment,
                                                                            void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_texture_output_added   (const void*                                    segment,
                                                                            void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_video_segment_releasing(const void*                                    segment,
                                                                            void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_sync_io                   (_nodes_postprocessing_video_segment_node_data* node_data_ptr);
PRIVATE void _nodes_postprocessing_video_segment_update_callbacks          (_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                            bool                                           should_subscribe);


/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_input_added(const void* new_input_id,
                                                                        void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr);

    /* Let the subscribers know about the event */
    system_callback_manager_call_back(node_data_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                      (void*) new_input_id);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_output_added(const void* new_output_id,
                                                                         void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr);

    /* Let the subscribers know about the event */
    system_callback_manager_call_back(node_data_ptr->callback_manager,
                                      DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                      (void*) new_output_id);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_video_segment_releasing(const void* segment,
                                                                            void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node_data;

    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node_data instance is NULL");

    /* Since this node is an adapter for the underlying video segment, we need to destroy this node the moment
     * the video segment is released */
    demo_timeline_segment_node_id node_id = -1;

    demo_timeline_segment_node_get_property(node_data_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                           &node_id);

    demo_timeline_segment_delete_nodes(node_data_ptr->postprocessing_segment,
                                       1, /* n_nodes */
                                      &node_id);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_sync_io(_nodes_postprocessing_video_segment_node_data* node_data_ptr)
{
    demo_timeline_segment_node_id*       input_video_segment_node_ids_ptr         = NULL;
    demo_timeline_segment_node_input_id* input_video_segment_node_input_ids_ptr   = NULL;
    uint32_t                             n_precall_node_inputs                    = 0;
    uint32_t                             n_precall_node_outputs                   = 0;
    uint32_t                             n_video_segment_result_node_inputs       = 0;

    /* NOTE: Buffers, textures, etc. connected to the inputs of a video segment's output node deliver data,
     *       so we need to expose these as video segment's *outputs*. */

    /* Sanity checks */
    if (node_data_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node data pointer is NULL");

        goto end;
    }

    /* TODO: Removal of existing inputs & outputs which are no longer exposed by the video segment */

    demo_timeline_segment_node_get_inputs (node_data_ptr->node,
                                           0, /* n_input_ids */
                                          &n_precall_node_inputs,
                                           NULL); /* out_opt_input_ids_ptr */
    demo_timeline_segment_node_get_outputs(node_data_ptr->node,
                                           0, /* n_output_ids_ptr */
                                          &n_precall_node_outputs,
                                           NULL); /* out_opt_output_ids_ptr */

    if (n_precall_node_inputs != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "TODO");

        goto end;
    }

    if (n_precall_node_outputs != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "TODO");

        goto end;
    }

    /* Iterate over all segment inputs and add them to the node */
    if (!demo_timeline_segment_get_exposed_inputs (node_data_ptr->video_segment,
                                                  &n_video_segment_result_node_inputs,
                                                  &input_video_segment_node_ids_ptr,
                                                  &input_video_segment_node_input_ids_ptr) /*||
        !demo_timeline_segment_get_exposed_outputs(node_data_ptr->video_segment,
                                                  &n_video_segment_outputs,
                                                  &output_video_segment_node_ids_ptr,
                                                  &output_video_segment_node_output_ids_ptr) */)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve exposed IO data from the owned video segment.");

        goto end;
    }

    for (uint32_t n_io_iteration = 0;
                  n_io_iteration < 1; /* inputs to be exposed as outputs; outputs - TODO */
                ++n_io_iteration)
    {
        const bool                           is_output_node_input_iteration   = (n_io_iteration == 0);
        const uint32_t                       n_video_segment_ios              = n_video_segment_result_node_inputs;
        const demo_timeline_segment_node_id* video_segment_node_ios           = input_video_segment_node_ids_ptr;
        const uint32_t*                      video_segment_node_io_ids        = input_video_segment_node_input_ids_ptr;

        for (uint32_t n_video_segment_io = 0;
                      n_video_segment_io < n_video_segment_ios;
                    ++n_video_segment_io)
        {
            demo_timeline_segment_node_interface_type io_interface_type       = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN;
            bool                                      io_is_required          = false;
            system_hashed_ansi_string                 io_name                 = NULL;
            ral_texture_format                        io_texture_format       = RAL_TEXTURE_FORMAT_UNKNOWN;
            uint32_t                                  io_texture_n_components = 0;
            uint32_t                                  io_texture_n_layers     = 0;
            uint32_t                                  io_texture_n_samples    = 0;
            ral_texture_type                          io_texture_type         = RAL_TEXTURE_TYPE_UNKNOWN;

            demo_timeline_segment_get_node_io_property_by_node_id(node_data_ptr->video_segment,
                                                                  video_segment_node_ios[n_video_segment_io],
                                                                  true, /* is_input_iteration */
                                                                  video_segment_node_io_ids[n_video_segment_io],
                                                                  DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_INTERFACE_TYPE,
                                                                  &io_interface_type);

            if (io_interface_type != DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized input/output interface type.");

                continue;
            }

            /* Retrieve texture interface properties.. */
            struct _property
            {
                demo_timeline_segment_node_io_property property;
                void*                                  result_ptr;
            } properties[] =
            {
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_IS_REQUIRED,          &io_is_required},
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_FORMAT,       &io_texture_format},
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_COMPONENTS, &io_texture_n_components},
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_LAYERS,     &io_texture_n_layers},
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_N_SAMPLES,    &io_texture_n_samples},
                {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_TYPE,         &io_texture_type}
            };
            const uint32_t n_properties = sizeof(properties) / sizeof(properties[0]);

            for (uint32_t n_property = 0;
                          n_property < n_properties;
                        ++n_property)
            {
                demo_timeline_segment_get_node_io_property_by_node_id(node_data_ptr->video_segment,
                                                                      video_segment_node_ios[n_video_segment_io],
                                                                      true, /* is_input_io */
                                                                      video_segment_node_io_ids[n_video_segment_io],
                                                                      properties[n_property].property,
                                                                      properties[n_property].result_ptr);
            }

            /* Create a new node IO */
            demo_texture_io_declaration new_texture_io;
            bool                        result             = false;

            new_texture_io.is_attachment_required = io_is_required;
            new_texture_io.name                   = io_name;
            new_texture_io.texture_format         = io_texture_format;
            new_texture_io.texture_n_components   = io_texture_n_components;
            new_texture_io.texture_n_layers       = io_texture_n_layers;
            new_texture_io.texture_n_samples      = io_texture_n_samples;
            new_texture_io.texture_type           = io_texture_type;

            result = demo_timeline_segment_node_add_texture_output(node_data_ptr->node,
                                                                  &new_texture_io,
                                                                   NULL); /* out_opt_input_id_ptr */

            ASSERT_DEBUG_SYNC(result,
                              "Could not create a new node texture input");
        } /* for (all exposed video segment IOs) */
    } /* for (segment inputs; segment outputs) */

    /* All done */
end:
    if (input_video_segment_node_ids_ptr       != NULL ||
        input_video_segment_node_input_ids_ptr != NULL)
    {
        demo_timeline_segment_free_exposed_io_result(input_video_segment_node_ids_ptr,
                                                     input_video_segment_node_input_ids_ptr);

        input_video_segment_node_ids_ptr       = NULL;
        input_video_segment_node_input_ids_ptr = NULL;
    }

#if 0
    if (output_video_segment_node_ids_ptr        != NULL ||
        output_video_segment_node_output_ids_ptr != NULL)
    {
        demo_timeline_segment_free_exposed_io_result(output_video_segment_node_ids_ptr,
                                                     output_video_segment_node_output_ids_ptr);

        output_video_segment_node_ids_ptr        = NULL;
        output_video_segment_node_output_ids_ptr = NULL;
    }
#endif
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_update_callbacks(_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                  bool                                           should_subscribe)
{
    system_callback_manager video_segment_node_callback_manager = NULL;
    system_callback_manager video_segment_callback_manager      = NULL;

    demo_timeline_segment_get_property     ( node_data_ptr->video_segment,
                                            DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                           &video_segment_callback_manager);
    demo_timeline_segment_node_get_property(node_data_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,
                                           &video_segment_node_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_video_segment_releasing,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_texture_input_added,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_node_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_texture_output_added,
                                                        node_data_ptr);
    } /* if (should_subscribe) */
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                           _nodes_postprocessing_video_segment_on_video_segment_releasing,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_INPUT_ADDED,
                                                           _nodes_postprocessing_video_segment_on_texture_input_added,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_OUTPUT_ADDED,
                                                           _nodes_postprocessing_video_segment_on_texture_output_added,
                                                           node_data_ptr);
    }
}


/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL void nodes_postprocessing_video_segment_deinit(demo_timeline_segment_node_private node)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;

    /* Unsubscribe from the segment & node callbacks.. */
    _nodes_postprocessing_video_segment_update_callbacks(node_data_ptr,
                                                         false); /* should_subscribe */

    /* Use the destructor to release any objects that were allocated to handle the duties */
    delete node_data_ptr;
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_get_input_property(demo_timeline_segment_node_private     node,
                                                                  demo_timeline_segment_node_input_id    input_id,
                                                                  demo_timeline_segment_node_io_property property,
                                                                  void*                                  out_result_ptr)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node instance is NULL");

    /* Forward the query to the video segment's node instance */
    demo_timeline_segment_node_get_io_property(node_data_ptr->node,
                                               true, /* is_input_io */
                                               input_id,
                                               property,
                                               out_result_ptr);
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_get_output_property(demo_timeline_segment_node_private     node,
                                                                   demo_timeline_segment_node_output_id   output_id,
                                                                   demo_timeline_segment_node_io_property property,
                                                                   void*                                  out_result_ptr)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != NULL,
                      "Input node instance is NULL");

    /* Forward the query to the video segment instance */
    demo_timeline_segment_node_get_io_property(node_data_ptr->node,
                                               false, /* is_input_io */
                                               output_id,
                                               property,
                                               out_result_ptr);
}

/** Please see header for specification */
PUBLIC bool nodes_postprocessing_video_segment_get_property(demo_timeline_segment_node_private  node,
                                                            int                                 property,
                                                            void*                               out_result_ptr)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;
    bool                                           result        = true;

    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node instance is NULL");

        result = false;
        goto end;
    } /* if (node == NULL) */

    switch (property)
    {
        case NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT:
        {
            *(demo_timeline_segment*) out_result_ptr = node_data_ptr->video_segment;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_property value.");

            result = false;
            goto end;
        }
    } /* switch (property) */

end:
    return result;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_postprocessing_video_segment_init(demo_timeline_segment      segment,
                                                                                                         demo_timeline_segment_node node,
                                                                                                         ral_context                context)
{
    demo_timeline_segment_node_private result                   = NULL;
    system_callback_manager            segment_callback_manager = NULL;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input post-processing segment is NULL");

        goto end;
    } /* if (segment == NULL) */

    /* Spawn a new descriptor */
    _nodes_postprocessing_video_segment_node_data* new_node_data_ptr = new (std::nothrow) _nodes_postprocessing_video_segment_node_data(node,
                                                                                                                                        segment,
                                                                                                                                        context);

    if (new_node_data_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        goto end;
    } /* if (new_node_data_ptr == NULL) */

    /* All done */
    result = (demo_timeline_segment_node_private) new_node_data_ptr;
end:
    return result;
}

/** Please see header for specification */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_postprocessing_video_segment_render(demo_timeline_segment_node_private node,
                                                                             uint32_t                           frame_index,
                                                                             system_time                        frame_time,
                                                                             const int32_t*                     rendering_area_px_topdown)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;
    bool                                           result        = false;

    if (node_data_ptr->video_segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No video segment assigned to the video segment node.");
    }
    else
    {
        result = demo_timeline_segment_render(node_data_ptr->video_segment,
                                              frame_index,
                                              frame_time,
                                              rendering_area_px_topdown);
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool nodes_postprocessing_video_segment_set_property(demo_timeline_segment_node_private  node,
                                                            int                                 property,
                                                            const void*                         data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = (_nodes_postprocessing_video_segment_node_data*) node;
    bool                                           result        = true;

    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        result = false;
        goto end;
    }

    switch (property)
    {
        case NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT:
        {
            demo_timeline_segment      new_segment = NULL;
            demo_timeline_segment_type new_segment_type;

            if (node_data_ptr->video_segment != NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "A video segment has already been assigned to the video segment node!");

                break;
            }


            /* Assign the new video segment and perform some sanity checks along the way */
            new_segment                  = *(demo_timeline_segment*) data;
            node_data_ptr->video_segment = new_segment;

            demo_timeline_segment_get_property(node_data_ptr->video_segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_TYPE,
                                              &new_segment_type);

            if (new_segment_type != DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Input segment is not a video segment");

                node_data_ptr->video_segment = NULL;
                break;
            }

            /* Set up input / outputs */
            _nodes_postprocessing_video_segment_sync_io(node_data_ptr);

            /* Sign up for callbacks */
            _nodes_postprocessing_video_segment_update_callbacks(node_data_ptr,
                                                                 true); /* should_subscribe */

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unsupported demo_timeline_segment_node_property value.");

            result = false;
        }
    } /* switch (property) */

end:
    return result;
}