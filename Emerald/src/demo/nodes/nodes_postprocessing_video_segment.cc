/**
 *
 *  Emerald (kbi/elude @ 2015-2016)
 *
 *  This node encapsulates a demo_timeline_video_segment instance. It serves a few purposes:
 *
 *  - If the video segment adjusts its input/output configuration, these will be applied to
 *    the node, too.
 *  - If any object is attached to an input, this information is passed to the video segment.
 *  - If any connection is created from the node's output to an external object's input, this
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
#include "system/system_resizable_vector.h"

typedef struct _exposed_vs_node_io
{
    uint32_t                      ps_vs_io_id;
    demo_timeline_segment_node_id ps_vs_node_id;
    bool                          vs_io_is_input;

    union
    {
        demo_timeline_segment_node_input_id  vs_io_input_id;
        demo_timeline_segment_node_output_id vs_io_output_id;
    };

    demo_timeline_segment_node_id vs_node_id;


    explicit _exposed_vs_node_io(demo_timeline_segment_node_id in_ps_vs_node_id,
                                 uint32_t                      in_ps_vs_io_id,
                                 demo_timeline_segment_node_id in_vs_node_id,
                                 bool                          in_vs_io_is_input,
                                 uint32_t                      in_vs_io_id)
    {
        ps_vs_node_id  = in_ps_vs_node_id;
        ps_vs_io_id    = in_ps_vs_io_id;
        vs_io_input_id = in_vs_io_id;
        vs_io_is_input = in_vs_io_is_input;
        vs_node_id     = in_vs_node_id;
    }

    bool operator==(const _exposed_vs_node_io& in)
    {
        return (ps_vs_io_id    == in.ps_vs_io_id    &&
                ps_vs_node_id  == in.ps_vs_node_id  &&
                vs_io_is_input == in.vs_io_is_input &&
                vs_io_input_id == in.vs_io_input_id &&
                vs_node_id     == in.vs_node_id);
    }
} _exposed_vs_node_io;

typedef struct _nodes_postprocessing_video_segment_node_data
{
    system_callback_manager    callback_manager;
    ral_context                context;
    demo_timeline_segment_node node;
    demo_timeline_segment      postprocessing_segment;
    demo_timeline_segment      video_segment;

    system_resizable_vector    exposed_vs_inputs;  /* owns _exposed_vs_node_io instances */
    system_resizable_vector    exposed_vs_outputs; /* owns _exposed_vs_node_io instances */

    explicit _nodes_postprocessing_video_segment_node_data(demo_timeline_segment_node in_node,
                                                           demo_timeline_segment      in_postprocessing_segment,
                                                           ral_context                in_context)
    {
        callback_manager       = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_COUNT);
        context                = nullptr;
        exposed_vs_inputs      = system_resizable_vector_create(4 /* capacity */);
        exposed_vs_outputs     = system_resizable_vector_create(4 /* capacity */);
        node                   = in_node;
        postprocessing_segment = in_postprocessing_segment;
        video_segment          = nullptr;
    }

    ~_nodes_postprocessing_video_segment_node_data()
    {
        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }

        if (exposed_vs_inputs != nullptr)
        {
            system_resizable_vector_release(exposed_vs_inputs);

            exposed_vs_inputs = nullptr;
        }

        if (exposed_vs_outputs != nullptr)
        {
            system_resizable_vector_release(exposed_vs_outputs);

            exposed_vs_outputs = nullptr;
        }
    }
} _nodes_postprocessing_video_segment_node_data;


/* Forward declarations */
PRIVATE void _nodes_postprocessing_video_segment_on_input_exposed                  (const void*                                    unused,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_input_no_longer_exposed        (const void*                                    unused,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_output_exposed                 (const void*                                    unused,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_output_no_longer_exposed       (const void*                                    unused,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_texture_view_attached_to_node  (const void*                                    callback_arg,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_texture_view_detached_from_node(const void*                                    callback_arg,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_on_video_segment_releasing        (const void*                                    segment,
                                                                                    void*                                          node_data);
PRIVATE void _nodes_postprocessing_video_segment_sync_io                           (_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                                    bool                                           should_process_inputs);
PRIVATE void _nodes_postprocessing_video_segment_sync_output_attachments           (_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                                    demo_timeline_segment_node_id                  node_id,
                                                                                    demo_timeline_segment_output_id                node_output_id,
                                                                                    ral_texture_view                               new_texture_attachment);
PRIVATE void _nodes_postprocessing_video_segment_update_callbacks                  (_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                                    bool                                           should_subscribe);


/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_input_exposed(const void* unused,
                                                                        void* node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node_data);

    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                true /* should_process_inputs */);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_input_no_longer_exposed(const void* unused,
                                                                            void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node_data);

    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                true /* should_process_inputs */);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_output_exposed(const void* unused,
                                                                         void* node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node_data);

    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                false /* should_process_inputs */);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_output_no_longer_exposed(const void* unused,
                                                                             void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node_data);

    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
                      "Input node_data instance is NULL");

    /* Sync exposed texture inputs with the segment's */
    _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                false /* should_process_inputs */);
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_view_attached_to_node(const void* callback_arg,
                                                                                  void*       node_data)
{
    const demo_timeline_segment_node_callback_texture_view_attached_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_view_attached_callback_argument*>(callback_arg);
    _nodes_postprocessing_video_segment_node_data*                                     node_data_ptr    = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>                                    (node_data);

    if (callback_arg_ptr->is_input_id == false)
    {
        /* Bind the new texture attached to the video segment to the output of the video segment node */
        demo_timeline_segment_node_id node_id = -1;

        demo_timeline_segment_node_get_property(callback_arg_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                               &node_id);

        _nodes_postprocessing_video_segment_sync_output_attachments(node_data_ptr,
                                                                    node_id,
                                                                    callback_arg_ptr->id,
                                                                    callback_arg_ptr->texture_view);
    }
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_texture_view_detached_from_node(const void* callback_arg,
                                                                                    void*       node_data)
{
    const demo_timeline_segment_node_callback_texture_view_attached_callback_argument* callback_arg_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_view_attached_callback_argument*>(callback_arg);
    _nodes_postprocessing_video_segment_node_data*                                     node_data_ptr    = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>                                    (node_data);

    if (callback_arg_ptr->is_input_id == false)
    {
        /* Unbind the former texture from the output of the video segment node */
        demo_timeline_segment_node_id node_id = -1;

        demo_timeline_segment_node_get_property(callback_arg_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                               &node_id);

        _nodes_postprocessing_video_segment_sync_output_attachments(node_data_ptr,
                                                                    node_id,
                                                                    callback_arg_ptr->id,
                                                                    static_cast<ral_texture_view>(nullptr) );  /* new_texture_view_attachment */
    }
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_on_video_segment_releasing(const void* segment,
                                                                            void*       node_data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node_data);

    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
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
PRIVATE void _nodes_postprocessing_video_segment_sync_io(_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                         bool                                           should_process_inputs)
{
    uint32_t                       n_precall_node_ios              = 0;
    uint32_t                       n_video_segment_result_node_ios = 0;
    demo_timeline_segment_node_id  node_id                         = -1;
    demo_timeline_segment_node_io* video_segment_node_ios_ptr      = nullptr;

    /* NOTE: Buffers, textures, etc. connected to the inputs of a video segment's output node deliver data,
     *       so we need to expose these as video segment's *outputs*. */

    /* Sanity checks */
    if (node_data_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node data pointer is NULL");

        goto end;
    }

    /* TODO: Removal of existing IOs which are no longer exposed by the video segment */
    if (should_process_inputs)
    {
        demo_timeline_segment_node_get_inputs (node_data_ptr->node,
                                               0, /* n_input_ids */
                                              &n_precall_node_ios,
                                               nullptr); /* out_opt_input_ids_ptr */
    }
    else
    {
        demo_timeline_segment_node_get_outputs(node_data_ptr->node,
                                               0, /* n_output_ids_ptr */
                                              &n_precall_node_ios,
                                               nullptr); /* out_opt_output_ids_ptr */
    }

    if (n_precall_node_ios != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "TODO");

        goto end;
    }

    /* Iterate over all segment IOs and add them to the node */
    if ( should_process_inputs && !demo_timeline_segment_get_exposed_inputs (node_data_ptr->video_segment,
                                                                            &n_video_segment_result_node_ios,
                                                                            &video_segment_node_ios_ptr) ||
        !should_process_inputs && !demo_timeline_segment_get_exposed_outputs(node_data_ptr->video_segment,
                                                                            &n_video_segment_result_node_ios,
                                                                            &video_segment_node_ios_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve exposed IO data from the owned video segment.");

        goto end;
    }

    for (uint32_t n_video_segment_io = 0;
                  n_video_segment_io < n_video_segment_result_node_ios;
                ++n_video_segment_io)
    {
        ral_texture_view                          io_bound_texture_view        = nullptr;
        demo_timeline_segment_node_interface_type io_interface_type            = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN;
        bool                                      io_is_required               = false;
        system_hashed_ansi_string                 io_name                      = nullptr;
        ral_format                                io_texture_view_format       = RAL_FORMAT_UNKNOWN;
        uint32_t                                  io_texture_view_n_components = 0;
        uint32_t                                  io_texture_view_n_layers     = 0;
        uint32_t                                  io_texture_view_n_samples    = 0;
        ral_texture_type                          io_texture_view_type         = RAL_TEXTURE_TYPE_UNKNOWN;

        demo_timeline_segment_get_node_io_property_by_node_id(node_data_ptr->video_segment,
                                                              video_segment_node_ios_ptr[n_video_segment_io].node_id,
                                                              video_segment_node_ios_ptr[n_video_segment_io].is_input,
                                                              video_segment_node_ios_ptr[n_video_segment_io].node_io_id,
                                                              DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_INTERFACE_TYPE,
                                                             &io_interface_type);

        if (io_interface_type != DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE_VIEW)
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
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_BOUND_TEXTURE_VIEW_RAL,    &io_bound_texture_view},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_IS_REQUIRED,               &io_is_required},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_VIEW_FORMAT,       &io_texture_view_format},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_VIEW_N_COMPONENTS, &io_texture_view_n_components},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_VIEW_N_LAYERS,     &io_texture_view_n_layers},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_VIEW_N_SAMPLES,    &io_texture_view_n_samples},
            {DEMO_TIMELINE_SEGMENT_NODE_IO_PROPERTY_TEXTURE_VIEW_TYPE,         &io_texture_view_type}
        };
        const uint32_t n_properties = sizeof(properties) / sizeof(properties[0]);

        for (uint32_t n_property = 0;
                      n_property < n_properties;
                    ++n_property)
        {
            demo_timeline_segment_get_node_io_property_by_node_id(node_data_ptr->video_segment,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].node_id,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].is_input,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].node_io_id,
                                                                  properties[n_property].property,
                                                                  properties[n_property].result_ptr);
        }

        /* Create a new PS VS node IO */
        uint32_t                         new_texture_view_io_id = -1;
        demo_texture_view_io_declaration new_texture_view_io;
        bool                             result            = false;

        new_texture_view_io.is_attachment_required = io_is_required;
        new_texture_view_io.format                 = io_texture_view_format;
        new_texture_view_io.n_components           = io_texture_view_n_components;
        new_texture_view_io.n_layers               = io_texture_view_n_layers;
        new_texture_view_io.n_samples              = io_texture_view_n_samples;
        new_texture_view_io.name                   = io_name;
        new_texture_view_io.type                   = io_texture_view_type;

        if (should_process_inputs)
        {
            result = demo_timeline_segment_node_add_texture_view_input(node_data_ptr->node,
                                                                      &new_texture_view_io,
                                                                      &new_texture_view_io_id);

            ASSERT_DEBUG_SYNC(result,
                              "Could not create a new node texture view input");
        }
        else
        {
            result = demo_timeline_segment_node_add_texture_view_output(node_data_ptr->node,
                                                                       &new_texture_view_io,
                                                                       &new_texture_view_io_id);

            ASSERT_DEBUG_SYNC(result,
                              "Could not create a new node texture view output");
        }

        /* Attach the currently bound texture view to the new IO */
        if (io_bound_texture_view != nullptr)
        {
            demo_texture_view_attachment_declaration bound_texture_view_attachment_declaration;

            bound_texture_view_attachment_declaration.texture_view_ral = io_bound_texture_view;

            if (!demo_timeline_segment_node_attach_texture_view_to_texture_view_io(node_data_ptr->node,
                                                                                   should_process_inputs,
                                                                                   new_texture_view_io_id,
                                                                                  &bound_texture_view_attachment_declaration) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not attach the exposed node IO texture view to a video segment node IO.");
            }
        }

        /* Store node IO data. */
        demo_timeline_segment_node_get_property(node_data_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                               &node_id);

        _exposed_vs_node_io* new_io_ptr = new _exposed_vs_node_io(node_id,
                                                                  new_texture_view_io_id,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].node_id,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].is_input,
                                                                  video_segment_node_ios_ptr[n_video_segment_io].node_io_id);

        ASSERT_ALWAYS_SYNC(new_io_ptr != nullptr,
                           "Out of memory");

        system_resizable_vector_push( (should_process_inputs) ? node_data_ptr->exposed_vs_inputs
                                                              : node_data_ptr->exposed_vs_outputs,
                                     new_io_ptr);
    }

    /* All done */
end:
    if (video_segment_node_ios_ptr != nullptr)
    {
        demo_timeline_segment_free_exposed_io_result(video_segment_node_ios_ptr);

        video_segment_node_ios_ptr = nullptr;
    }
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_sync_output_attachments(_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                         demo_timeline_segment_node_id                  node_id,
                                                                         demo_timeline_segment_output_id                node_output_id,
                                                                         ral_texture_view                               new_texture_view_attachment)
{
    uint32_t             n_exposed_vs_outputs = 0;
    _exposed_vs_node_io* ps_vs_node_io_ptr    = nullptr;

    /* Find the _exposed_vs_node_io instance for the specified VS node output */
    system_resizable_vector_get_property(node_data_ptr->exposed_vs_outputs,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_exposed_vs_outputs);

    for (uint32_t n_output = 0;
                  n_output < n_exposed_vs_outputs;
                ++n_output)
    {
        if (!system_resizable_vector_get_element_at(node_data_ptr->exposed_vs_outputs,
                                                    n_output,
                                                   &ps_vs_node_io_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not get the exposed VS output descriptor at index [%d]",
                              n_output);

            continue;
        }

        if (ps_vs_node_io_ptr->ps_vs_node_id == node_id        &&
            ps_vs_node_io_ptr->ps_vs_io_id   == node_output_id)
        {
            break;
        }
        else
        {
            ps_vs_node_io_ptr = nullptr;
        }
    }

    if (ps_vs_node_io_ptr == nullptr)
    {
        /* The notification is coming from an exposed IO this video segment post-processing node
         * does not watch. Ignore.
         */
        goto end;
    }

    /* Update the PS VS texture view attachment */
    demo_texture_view_attachment_declaration new_attachment;

    new_attachment.texture_view_ral = new_texture_view_attachment;

    demo_timeline_segment_node_attach_texture_view_to_texture_view_io(node_data_ptr->node,
                                                                      false, /* is_input_id */
                                                                      ps_vs_node_io_ptr->ps_vs_io_id,
                                                                     &new_attachment);

end:
    ;
}

/** TODO */
PRIVATE void _nodes_postprocessing_video_segment_update_callbacks(_nodes_postprocessing_video_segment_node_data* node_data_ptr,
                                                                  bool                                           should_subscribe)
{
    system_callback_manager video_segment_callback_manager = nullptr;

    demo_timeline_segment_get_property     ( node_data_ptr->video_segment,
                                            DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                           &video_segment_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_video_segment_releasing,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_NO_LONGER_EXPOSED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_input_no_longer_exposed,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_INPUT_EXPOSED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_input_exposed,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_OUTPUT_EXPOSED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_output_exposed,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_OUTPUT_NO_LONGER_EXPOSED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_output_no_longer_exposed,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_ATTACHED_TO_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_texture_view_attached_to_node,
                                                        node_data_ptr);
        system_callback_manager_subscribe_for_callbacks(video_segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_DETACHED_FROM_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_video_segment_on_texture_view_detached_from_node,
                                                        node_data_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                                           _nodes_postprocessing_video_segment_on_video_segment_releasing,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_NO_LONGER_EXPOSED,
                                                           _nodes_postprocessing_video_segment_on_input_no_longer_exposed,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_INPUT_EXPOSED,
                                                           _nodes_postprocessing_video_segment_on_input_exposed,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_OUTPUT_EXPOSED,
                                                           _nodes_postprocessing_video_segment_on_output_exposed,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_OUTPUT_NO_LONGER_EXPOSED,
                                                           _nodes_postprocessing_video_segment_on_output_no_longer_exposed,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_ATTACHED_TO_NODE,
                                                           _nodes_postprocessing_video_segment_on_texture_view_attached_to_node,
                                                           node_data_ptr);
        system_callback_manager_unsubscribe_from_callbacks(video_segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_DETACHED_FROM_NODE,
                                                           _nodes_postprocessing_video_segment_on_texture_view_detached_from_node,
                                                           node_data_ptr);
    }
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_video_segment_deinit(demo_timeline_segment_node_private node)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);

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
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
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
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(node_data_ptr != nullptr,
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
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);
    bool                                           result        = true;

    if (node == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node instance is NULL");

        result = false;
        goto end;
    }

    switch (property)
    {
        case NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT:
        {
            *reinterpret_cast<demo_timeline_segment*>(out_result_ptr) = node_data_ptr->video_segment;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_segment_node_property value.");

            result = false;
            goto end;
        }
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC demo_timeline_segment_node_private nodes_postprocessing_video_segment_init(demo_timeline_segment      segment,
                                                                                  demo_timeline_segment_node node,
                                                                                  ral_context                context)
{
    demo_timeline_segment_node_private result                   = nullptr;
    system_callback_manager            segment_callback_manager = nullptr;

    /* Sanity checks */
    if (segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input post-processing segment is NULL");

        goto end;
    }

    /* Spawn a new descriptor */
    _nodes_postprocessing_video_segment_node_data* new_node_data_ptr = new (std::nothrow) _nodes_postprocessing_video_segment_node_data(node,
                                                                                                                                        segment,
                                                                                                                                        context);

    if (new_node_data_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        goto end;
    }

    /* All done */
    result = (demo_timeline_segment_node_private) new_node_data_ptr;
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool nodes_postprocessing_video_segment_render(demo_timeline_segment_node_private node,
                                                      uint32_t                           frame_index,
                                                      system_time                        frame_time,
                                                      const int32_t*                     rendering_area_px_topdown,
                                                      ral_present_task*                  out_present_task_ptr)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);
    bool                                           result        = false;

    if (node_data_ptr->video_segment == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No video segment assigned to the video segment node.");
    }
    else
    {
        result = demo_timeline_segment_render_video_segment(node_data_ptr->video_segment,
                                                            frame_index,
                                                            frame_time,
                                                            rendering_area_px_topdown,
                                                            out_present_task_ptr);
    }

    return result;
}

/** Please see header for specification */
PUBLIC bool nodes_postprocessing_video_segment_set_property(demo_timeline_segment_node_private  node,
                                                            int                                 property,
                                                            const void*                         data)
{
    _nodes_postprocessing_video_segment_node_data* node_data_ptr = reinterpret_cast<_nodes_postprocessing_video_segment_node_data*>(node);
    bool                                           result        = true;

    if (node == nullptr)
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
            demo_timeline_segment      new_segment = nullptr;
            demo_timeline_segment_type new_segment_type;

            if (node_data_ptr->video_segment != nullptr)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "A video segment has already been assigned to the video segment node!");

                break;
            }


            /* Assign the new video segment and perform some sanity checks along the way */
            new_segment                  = *reinterpret_cast<const demo_timeline_segment*>(data);
            node_data_ptr->video_segment = new_segment;

            demo_timeline_segment_get_property(node_data_ptr->video_segment,
                                               DEMO_TIMELINE_SEGMENT_PROPERTY_TYPE,
                                              &new_segment_type);

            if (new_segment_type != DEMO_TIMELINE_SEGMENT_TYPE_VIDEO)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Input segment is not a video segment");

                node_data_ptr->video_segment = nullptr;
                break;
            }

            /* Set up input / outputs */
            _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                        false /* should_process_inputs */);
            _nodes_postprocessing_video_segment_sync_io(node_data_ptr,
                                                        true  /* should_process_inputs */);

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
    }

end:
    return result;
}
