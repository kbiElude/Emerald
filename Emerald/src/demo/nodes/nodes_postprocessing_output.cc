/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_postprocessing_output.h"
#include "demo/demo_timeline_segment.h"
#include "ral/ral_context.h"
#include "ral/ral_present_job.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"


typedef struct _nodes_postprocessing_output
{
    demo_timeline_segment_node_input_id color_data_node_input_id;
    ral_context                         context;
    ral_texture_view                    input_data_texture_view;
    demo_timeline_segment_node          node;
    demo_timeline_segment               segment;

    _nodes_postprocessing_output(ral_context                         in_context,
                                 demo_timeline_segment               in_segment,
                                 demo_timeline_segment_node          in_node,
                                 demo_timeline_segment_node_input_id in_color_data_node_input_id)
    {
        color_data_node_input_id = in_color_data_node_input_id;
        context                  = in_context;
        input_data_texture_view  = nullptr;
        node                     = in_node;
        segment                  = in_segment;
    }
} _nodes_postprocessing_output;


/* Forward declarations */
PRIVATE void _nodes_postprocessing_on_texture_view_attached_callback(const void*                   callback_data,
                                                                     void*                         user_arg);
PRIVATE void _nodes_postprocessing_on_texture_view_detached_callback(const void*                   callback_data,
                                                                     void*                         user_arg);
PRIVATE bool _nodes_postprocessing_output_update_subscriptions      (_nodes_postprocessing_output* node_ptr,
                                                                     bool                          should_subscribe);


/** TODO */
PRIVATE void _nodes_postprocessing_on_texture_view_attached_callback(const void* callback_data,
                                                                     void*       user_arg)
{
    const demo_timeline_segment_node_callback_texture_view_attached_callback_argument* callback_data_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_view_attached_callback_argument*>(callback_data);
    _nodes_postprocessing_output*                                                      node_data_ptr     = reinterpret_cast<_nodes_postprocessing_output*>                                                     (user_arg);

    if (callback_data_ptr->is_input_id                                            &&
        callback_data_ptr->id          == node_data_ptr->color_data_node_input_id &&
        callback_data_ptr->node        == node_data_ptr->node)
    {
        /* Cache the texture */
        ASSERT_DEBUG_SYNC(callback_data_ptr->texture_view != nullptr,
                          "NULL texture view attached to a post-processing output node");
        ASSERT_DEBUG_SYNC(node_data_ptr->input_data_texture_view == nullptr,
                          "Texture view attached to a node input with another texture attachment already defined");

        node_data_ptr->input_data_texture_view = callback_data_ptr->texture_view;

        ral_context_retain_object(node_data_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                  node_data_ptr->input_data_texture_view);
    }
}

/** TODO */
PRIVATE void _nodes_postprocessing_on_texture_view_detached_callback(const void* callback_data,
                                                                     void*       user_arg)
{
    const demo_timeline_segment_node_callback_texture_view_detached_callback_argument* callback_data_ptr = reinterpret_cast<const demo_timeline_segment_node_callback_texture_view_detached_callback_argument*>(callback_data);
    _nodes_postprocessing_output*                                                      node_data_ptr     = reinterpret_cast<_nodes_postprocessing_output*>                                                     (user_arg);

    if (callback_data_ptr->is_input_id                                     &&
        callback_data_ptr->id   == node_data_ptr->color_data_node_input_id &&
        callback_data_ptr->node == node_data_ptr->node)
    {
        /* Drop the attachment */
        ASSERT_DEBUG_SYNC(node_data_ptr->input_data_texture_view != nullptr,
                          "No texture view attached to a node input, for which a \"texture detached\" callback was received");

        ral_context_delete_objects(node_data_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&node_data_ptr->input_data_texture_view) );

        node_data_ptr->input_data_texture_view = nullptr;
    }
}

/** TODO */
PRIVATE bool _nodes_postprocessing_output_update_subscriptions(_nodes_postprocessing_output* node_ptr,
                                                               bool                          should_subscribe)
{
    system_callback_manager segment_callback_manager = nullptr;

    demo_timeline_segment_get_property(node_ptr->segment,
                                       DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER,
                                      &segment_callback_manager);

    if (should_subscribe)
    {
        system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_ATTACHED_TO_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_on_texture_view_attached_callback,
                                                        node_ptr);
        system_callback_manager_subscribe_for_callbacks(segment_callback_manager,
                                                        DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_DETACHED_FROM_NODE,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _nodes_postprocessing_on_texture_view_detached_callback,
                                                        node_ptr);
    }
    else
    {
        system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_ATTACHED_TO_NODE,
                                                           _nodes_postprocessing_on_texture_view_attached_callback,
                                                           node_ptr);
        system_callback_manager_unsubscribe_from_callbacks(segment_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_VIEW_DETACHED_FROM_NODE,
                                                           _nodes_postprocessing_on_texture_view_detached_callback,
                                                           node_ptr);
    }

    return true;
}

/** Please see header for specification */
PUBLIC void nodes_postprocessing_output_deinit(demo_timeline_segment_node_private node)
{
    _nodes_postprocessing_output* node_ptr = reinterpret_cast<_nodes_postprocessing_output*>(node);

    _nodes_postprocessing_output_update_subscriptions(node_ptr,
                                                      false /* should_subscribe */);

    if (node_ptr->input_data_texture_view != nullptr)
    {
        ral_context_delete_objects(node_ptr->context,
                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                   1, /* n_objects */
                                   reinterpret_cast<void* const*>(&node_ptr->input_data_texture_view) );

        node_ptr->input_data_texture_view = nullptr;
    }

    delete node_ptr;
    node_ptr = nullptr;
}

/** Please see header for specification */
PUBLIC demo_timeline_segment_node_private nodes_postprocessing_output_init(demo_timeline_segment      segment,
                                                                           demo_timeline_segment_node node,
                                                                           ral_context                context)
{
    ral_format                       color_texture_format;
    _nodes_postprocessing_output*    new_node_data_ptr = nullptr;
    demo_texture_view_io_declaration new_node_input_declaration;
    bool                             result;
    demo_timeline_segment_node_id    texture_input_id;

    /* Determine texture format of the system framebuffer's color output. */
    ral_context_get_property(context,
                             RAL_CONTEXT_PROPERTY_SYSTEM_FB_BACK_BUFFER_COLOR_FORMAT,
                            &color_texture_format);

    /* Add a new input to the node we own */
    new_node_input_declaration.format                 = color_texture_format;
    new_node_input_declaration.is_attachment_required = true;
    new_node_input_declaration.name                   = system_hashed_ansi_string_create("Color data");
    new_node_input_declaration.n_layers               = 1;
    new_node_input_declaration.n_samples              = 1;
    new_node_input_declaration.type                   = RAL_TEXTURE_TYPE_2D;

    ral_utils_get_format_property(color_texture_format,
                                  RAL_FORMAT_PROPERTY_N_COMPONENTS,
                                 &new_node_input_declaration.n_components);

    result = demo_timeline_segment_node_add_texture_view_input(node,
                                                              &new_node_input_declaration,
                                                              &texture_input_id);

    ASSERT_DEBUG_SYNC(result,
                      "Could not add a new texture input to the post-processing output node.");

    /* Create, initialize & return the new descriptor. */
    new_node_data_ptr = new _nodes_postprocessing_output(context,
                                                         segment,
                                                         node,
                                                         texture_input_id);

    _nodes_postprocessing_output_update_subscriptions(new_node_data_ptr,
                                                      true /* should_subscribe */);

    /* All done */
    return (demo_timeline_segment_node_private) new_node_data_ptr;
}

/** Please see header for specification */
PUBLIC bool nodes_postprocessing_output_render(demo_timeline_segment_node_private node,
                                               uint32_t                           frame_index,
                                               system_time                        frame_time,
                                               const int32_t*                     rendering_area_px_topdown,
                                               ral_present_task*                  out_present_task_ptr)
{
    _nodes_postprocessing_output* node_ptr = reinterpret_cast<_nodes_postprocessing_output*>(node);

    /* Nothing to do. demo_timeline_segment will take this node's input and present it on screen. */

    return true;
}
