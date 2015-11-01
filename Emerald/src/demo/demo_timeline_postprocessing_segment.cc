/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/demo_timeline.h"
#include "demo/demo_timeline_postprocessing_segment.h"
#include "demo/demo_timeline_segment_node.h"
#include "demo/nodes/nodes_postprocessing_video_segment.h"
#include "ogl/ogl_context.h" /* TODO: Remove OGL dependency */
#include "ral/ral_types.h"
#include "system/system_callback_manager.h"
#include "system/system_critical_section.h"
#include "system/system_dag.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

typedef enum
{
    SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_POSTPROCESSING_SEGMENT,
    SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_TIMELINE,
    SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT, /* should only be used for video segment node */
    SEGMENT_NODE_USER_ARG_TYPE_NULL,

} segment_node_user_arg_type;

/** Predefined node -> func ptrs array.
 *
 *  Add new entries as new nodes are introduced.
 */
static struct 
{
    const char*                                     name;
    demo_timeline_postprocessing_segment_node_type  type;
    PFNSEGMENTNODEDEINITCALLBACKPROC                pfn_deinit_proc;

    PFNSEGMENTNODEINITCALLBACKPROC                  pfn_init_proc;
    segment_node_user_arg_type                      init_user_arg1_type;
    segment_node_user_arg_type                      init_user_arg2_type;
    segment_node_user_arg_type                      init_user_arg3_type;

    PFNSEGMENTNODERENDERCALLBACKPROC                pfn_render_proc;
} _node_data[] =
{
    /* DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT - dummy entry */
    {
        "Output node",
        DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,
        NULL,

        NULL,
        SEGMENT_NODE_USER_ARG_TYPE_NULL,
        SEGMENT_NODE_USER_ARG_TYPE_NULL,
        SEGMENT_NODE_USER_ARG_TYPE_NULL,

        NULL
    },

    /* DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT */
    {
        "Video segment node",
        DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,
        nodes_postprocessing_video_segment_deinit,

        nodes_postprocessing_video_segment_init,
        SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_POSTPROCESSING_SEGMENT,
        SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT,
        SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_TIMELINE,

        nodes_postprocessing_video_segment_render
    }
};

typedef struct _demo_timeline_postprocessing_segment
{
    ogl_context                   context;
    system_hashed_ansi_string     name;
    demo_timeline_segment_node_id node_id_counter;
    system_hash64map              node_id_to_descriptor_map;
    system_resizable_vector       nodes;    /* owns the demo_timeline_segment_node instances */
    demo_timeline                 timeline;

    demo_timeline_segment_node    output_node;


    REFCOUNT_INSERT_VARIABLES

    explicit _demo_timeline_postprocessing_segment(ogl_context               in_context,
                                                   system_hashed_ansi_string in_name,
                                                   demo_timeline             in_owner_timeline)
    {
        context                   = in_context;
        name                      = in_name;
        node_id_counter           = 0;
        node_id_to_descriptor_map = system_hash64map_create       (sizeof(demo_timeline_segment_node) );
        nodes                     = system_resizable_vector_create(sizeof(demo_timeline_segment_node) );
        output_node               = NULL;
        timeline                  = in_owner_timeline;

        ASSERT_DEBUG_SYNC(node_id_to_descriptor_map != NULL &&
                          nodes                     != NULL,
                          "Error creating internal containers");
    }

    ~_demo_timeline_postprocessing_segment()
    {
        if (node_id_to_descriptor_map != NULL)
        {
            system_hash64map_release(node_id_to_descriptor_map);

            node_id_to_descriptor_map = NULL;
        } /* if (node_id_to_descriptor_map != NULL) */

        if (nodes != NULL)
        {
            uint32_t                       n_nodes  = 0;
            demo_timeline_segment_node     node     = NULL;
            demo_timeline_segment_node_id* node_ids = NULL;

            system_resizable_vector_get_property(nodes,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                &n_nodes);

            if (n_nodes > 0)
            {
                node_ids = new (std::nothrow) demo_timeline_segment_node_id[n_nodes];

                ASSERT_ALWAYS_SYNC(node_ids != NULL,
                                   "Out of memory");

                if (node_ids != NULL)
                {
                    for (uint32_t n_node = 0;
                                  n_node < n_nodes;
                                ++n_node)
                    {
                        system_resizable_vector_get_element_at(nodes,
                                                               n_node,
                                                              &node);

                        demo_timeline_segment_node_get_property(node,
                                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                                                node_ids + n_node);
                    } /* for (all nodes) */

                    delete [] node_ids;
                    node_ids = NULL;
                } /* if (node_ids != NULL) */

                demo_timeline_postprocessing_segment_delete_nodes( (demo_timeline_postprocessing_segment) this,
                                                                  n_nodes,
                                                                  node_ids);

                while (system_resizable_vector_pop(nodes,
                                                  &node) )
                {
                    demo_timeline_segment_node_release(node);

                    node = NULL;
                } /* while (node descriptors can be popped from the vector) */
            } /* if (n_nodes > 0) */

            system_resizable_vector_release(nodes);
            nodes = NULL;
        } /* if (nodes != NULL) */
    }
} _demo_timeline_postprocessing_support;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline_postprocessing_segment,
                               demo_timeline_postprocessing_segment,
                              _demo_timeline_postprocessing_segment);

/* Forward declarations */
PRIVATE bool _demo_timeline_postprocessing_segment_add_node                  (      demo_timeline_postprocessing_segment           segment,
                                                                                    demo_timeline_postprocessing_segment_node_type node_type,
                                                                                    system_hashed_ansi_string                      name,
                                                                                    const demo_timeline_video_segment*             opt_video_segment_ptr,
                                                                                    demo_timeline_segment_node_id*                 out_opt_node_id_ptr,
                                                                                    demo_timeline_segment_node*                    out_opt_node_ptr);
PRIVATE void _demo_timeline_postprocessing_segment_init_rendering_callback   (      ogl_context                                    context,
                                                                                    void*                                          node);
PRIVATE void _demo_timeline_postprocessing_segment_on_new_video_segment_added(const void*                                          new_video_segment,
                                                                                    void*                                          timeline);
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_deleted  (const void*                                          video_segment,
                                                                                    void*                                          timeline);
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_moved    (const void*                                          video_segment,
                                                                                    void*                                          timeline);
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_resized  (const void*                                          video_segment,
                                                                                    void*                                          timeline);
PRIVATE void _demo_timeline_postprocessing_segment_release                   (      void*                                          segment);


/** Adds a new node to the postprocessing segment.
 *
 *  @param segment             Postprocessing segment to add a new node to.
 *  @param node_type           Type of the node which is to be added.
 *  @param name                Name of the new node.
 *  @param opt_video_segment   If @param node_type is DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT, this argument
 *                             should point to the demo_timeline_video_segment instance the node should act as a wrapper for.
 *                             Otherwise, this argument is ignored.
 *  @param out_opt_node_id_ptr If not NULL, deref will be set to ID of the newly created node, assuming the function returns true.
 *                             Otherwise, the pointed memory will not be touched.
 *  @param out_opt_node_ptr    If not NULL, deref will be set to the handle of the newly created node, assuming the function returns
 *                             true. Otherwise, the pointed memory will not be touched.
 *
 *  @return true if the node is added successfuly, false otherwise.
 */
PRIVATE bool _demo_timeline_postprocessing_segment_add_node(demo_timeline_postprocessing_segment           segment,
                                                            demo_timeline_postprocessing_segment_node_type node_type,
                                                            system_hashed_ansi_string                      name,
                                                            const demo_timeline_video_segment*             opt_video_segment_ptr,
                                                            demo_timeline_segment_node_id*                 out_opt_node_id_ptr,
                                                            demo_timeline_segment_node*                    out_opt_node_ptr)
{
    demo_timeline_segment_node             new_node     = NULL;
    demo_timeline_segment_node_id          new_node_id  = -1;
    bool                                   result       = false;
    _demo_timeline_postprocessing_segment* segment_ptr  = (_demo_timeline_postprocessing_segment*) segment;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input postprocessing segment is NULL");

        goto end;
    }

    if (name == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL name specified for the input postprocessing segment node.");

        goto end;
    }

    if (node_type                == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT &&
        segment_ptr->output_node != NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot add an output node to the postprocessing segment: the node is already present.");

        goto end;
    }

    if (node_type >= DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_COUNT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid postprocessing segment node type requested");

        goto end;
    }

    /* If we're asked to add an output node, handle it with a separate code-path.. */
    if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT)
    {
        /* Determine the texture type that the output node should use. It needs to be directly related to the
         * texture format expected by the rendering context. */
        ral_texture_format fbo_color_texture_format         = RAL_TEXTURE_FORMAT_UNKNOWN;
        ral_texture_format fbo_depth_stencil_texture_format = RAL_TEXTURE_FORMAT_UNKNOWN;

        ogl_context_get_property(segment_ptr->context,
                                 OGL_CONTEXT_PROPERTY_DEFAULT_FBO_COLOR_TEXTURE_FORMAT,
                                &fbo_color_texture_format);
        ogl_context_get_property(segment_ptr->context,
                                 OGL_CONTEXT_PROPERTY_DEFAULT_FBO_DEPTH_STENCIL_TEXTURE_FORMAT,
                                &fbo_depth_stencil_texture_format);

        if (fbo_color_texture_format == RAL_TEXTURE_FORMAT_UNKNOWN)
        {
            ASSERT_DEBUG_SYNC(false,
                              "No color texture format defined for the context's default FBO");

            goto end;
        } /* if (fbo_color_texture_format == RAL_TEXTURE_FORMAT_UNKNOWN) */

        if (fbo_depth_stencil_texture_format != RAL_TEXTURE_FORMAT_UNKNOWN)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Context's default FBO expects depth+stencil info which is not supported.");

            goto end;
        } /* if (fbo_depth_stencil_texture_format != RAL_TEXTURE_FORMAT_UNKNOWN) */

        /* Create the output node */
        new_node_id = segment_ptr->node_id_counter++;
        new_node    = demo_timeline_segment_node_create(DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                                        node_type,
                                                        new_node_id,
                                                        NULL, /* pfn_deinit_proc      */
                                                        NULL, /* pfn_init_proc        */
                                                        NULL, /* init_user_args_void3 */
                                                        NULL, /* pfn_render_proc      */
                                                        system_hashed_ansi_string_create(_node_data[node_type].name) );

        if (new_node == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            goto end;
        }
    } /* if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT) */
    else
    {
        ASSERT_DEBUG_SYNC(_node_data[node_type].type == node_type,
                          "Predefined node data corruption detected");

        void* init_user_args[3] = {NULL};

        for (uint32_t n_user_arg = 0;
                      n_user_arg < sizeof(init_user_args) / sizeof(init_user_args[0]);
                    ++n_user_arg)
        {
            segment_node_user_arg_type current_user_arg_type = (n_user_arg == 0) ? _node_data[node_type].init_user_arg1_type
                                                             : (n_user_arg == 1) ? _node_data[node_type].init_user_arg2_type
                                                                                 : _node_data[node_type].init_user_arg3_type;

            switch (current_user_arg_type)
            {
                case SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_POSTPROCESSING_SEGMENT:
                {
                    init_user_args[n_user_arg] = segment;

                    break;
                }

                case SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_TIMELINE:
                {
                    init_user_args[n_user_arg] = segment_ptr->timeline;

                    break;
                }

                case SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT:
                {
                    ASSERT_DEBUG_SYNC(node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT,
                                      "SEGMENT_NODE_USER_ARG_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT user arg can only be used for video segment node!");
                    ASSERT_DEBUG_SYNC(opt_video_segment_ptr != NULL,
                                      "One of the user args requires video segment optional parameter to be != NULL");

                    init_user_args[n_user_arg] = *opt_video_segment_ptr;

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized user arg type");
                }
            } /* switch (current_user_arg_type) */
        } /* for (all user args) */

        /* Create the new node container */
        new_node_id = segment_ptr->node_id_counter++;
        new_node    = demo_timeline_segment_node_create(DEMO_TIMELINE_SEGMENT_TYPE_POSTPROCESSING,
                                                        node_type,
                                                        new_node_id,
                                                        _node_data[node_type].pfn_deinit_proc,
                                                        _node_data[node_type].pfn_init_proc,
                                                        init_user_args,
                                                        _node_data[node_type].pfn_render_proc,
                                                        system_hashed_ansi_string_create(_node_data[node_type].name) );

        if (new_node == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Could not spawn a new postprocessing segment node.");

            goto end;
        } /* if (new_node_ptr == NULL) */

        /* Issue a rendering call-back to initialize the node. */
        ogl_context_request_callback_from_context_thread(segment_ptr->context,
                                                         _demo_timeline_postprocessing_segment_init_rendering_callback,
                                                         new_node);
    }

    /* Store the node */
    if (system_hash64map_contains(segment_ptr->node_id_to_descriptor_map,
                                  (system_hash64) new_node_id) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Node ID->descriptor map already contains an element under the assigned node ID!");

        goto end;
    }

    system_resizable_vector_push(segment_ptr->nodes,
                                 new_node);
    system_hash64map_insert     (segment_ptr->node_id_to_descriptor_map,
                                 (system_hash64) new_node_id,
                                 new_node,
                                 NULL,  /* on_remove_callback_proc */
                                 NULL); /* callback_argument       */

    /* All done */
    result = true;

end:
    if (!result && new_node != NULL)
    {
        demo_timeline_segment_node_release(new_node);

        new_node = NULL;
    }
    else
    if (result)
    {
        if (out_opt_node_id_ptr != NULL)
        {
            *out_opt_node_id_ptr = new_node_id;
        } /* if (out_opt_node_id_ptr != NULL) */

        if (out_opt_node_ptr != NULL)
        {
            *out_opt_node_ptr = new_node;
        } /* if (out_opt_node_ptr != NULL) */
    } /* if (result) */

    return result;
}

/** TODO */
PRIVATE void _demo_timeline_postprocessing_segment_init_rendering_callback(ogl_context context,
                                                                           void*       in_node)
{
    demo_timeline_segment_node node = (demo_timeline_segment_node) in_node;

    ASSERT_DEBUG_SYNC(node != NULL,
                      "NULL node instance");

    /* Call back the node-specific init func ptr */
    demo_timeline_segment_node_id  node_id       = 0;
    PFNSEGMENTNODEINITCALLBACKPROC pfn_init_proc = NULL;
    void*                          user_args[3] = {NULL};

    demo_timeline_segment_node_get_property(node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_ID,
                                           &node_id);
    demo_timeline_segment_node_get_property(node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_FUNC_PTR,
                                           &pfn_init_proc);
    demo_timeline_segment_node_get_property(node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_USER_ARGS,
                                           &user_args);

    pfn_init_proc(node_id,
                  user_args[0],
                  user_args[1],
                  user_args[2]);
}

/** Callback entry-point. Called whenever a new video segment is added to the timeline.
 *
 *  TODO
 *
 *  @param new_video_segment TODO
 *  @param timeline          TODO
 **/
PRIVATE void _demo_timeline_postprocessing_segment_on_new_video_segment_added(const void* new_video_segment,
                                                                                    void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/** Callback entry-point. Called whenever a video segment is deleted from the timeline.
 *
 *  TODO
 *
 *  @param video_segment TODO
 *  @param timeline      TODO
 **/
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_deleted(const void* video_segment,
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
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_moved(const void* video_segment,
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
PRIVATE void _demo_timeline_postprocessing_segment_on_video_segment_resized(const void* video_segment,
                                                                                  void* timeline)
{
    ASSERT_DEBUG_SYNC(false,
                      "TODO");
}

/* TODO */
PRIVATE void _demo_timeline_postprocessing_segment_release(void* segment)
{
    _demo_timeline_postprocessing_segment* segment_ptr               = (_demo_timeline_postprocessing_segment*) segment;
    system_callback_manager                timeline_callback_manager = NULL;

    demo_timeline_get_property(segment_ptr->timeline,
                               DEMO_TIMELINE_PROPERTY_CALLBACK_MANAGER,
                              &timeline_callback_manager);

    /* Unsubscribe from the notifications */
    system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                       DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
                                                       _demo_timeline_postprocessing_segment_on_new_video_segment_added,
                                                       segment);

    system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                       DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_DELETED,
                                                       _demo_timeline_postprocessing_segment_on_video_segment_deleted,
                                                       segment);

    system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                       DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,
                                                       _demo_timeline_postprocessing_segment_on_video_segment_moved,
                                                       segment);

    system_callback_manager_unsubscribe_from_callbacks(timeline_callback_manager,
                                                       DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,
                                                       _demo_timeline_postprocessing_segment_on_video_segment_resized,
                                                       segment);

    /* Actual tear-down is handled by the destructor. */
}


/** Please see header for specification */
PUBLIC bool demo_timeline_postprocessing_segment_add_node(demo_timeline_postprocessing_segment           segment,
                                                          demo_timeline_postprocessing_segment_node_type node_type,
                                                          system_hashed_ansi_string                      name,
                                                          demo_timeline_segment_node_id*                 out_opt_node_id_ptr,
                                                          demo_timeline_segment_node*                    out_opt_node_ptr)
{
    bool result = false;

    if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_VIDEO_SEGMENT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "demo_timeline_postprocessing_segment_add_node() cannot be used to add a new video segment node.");

        goto end;
    }

    result = _demo_timeline_postprocessing_segment_add_node(segment,
                                                            node_type,
                                                            name,
                                                            NULL, /* opt_video_segment */
                                                            out_opt_node_id_ptr,
                                                            out_opt_node_ptr);
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_postprocessing_segment_connect_nodes(demo_timeline_postprocessing_segment segment,
                                                               demo_timeline_segment_node_id        src_node_id,
                                                               demo_timeline_segment_node_output_id src_node_output_id,
                                                               demo_timeline_segment_node_id        dst_node_id,
                                                               demo_timeline_segment_node_output_id dst_node_input_id)
{
    demo_timeline_segment_node             dst_node    = NULL;
    bool                                   result      = false;
    _demo_timeline_postprocessing_segment* segment_ptr = NULL;
    demo_timeline_segment_node             src_node    = NULL;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input segment is NULL");

        goto end;
    } /* if (segment == NULL) */

    if (dst_node_id == src_node_id)
    {
        LOG_FATAL("Node loop-backs are forbidden (src_node_id == %d == dst_node_id.",
                  src_node_id);

        goto end;
    }

    /* Retrieve node descriptors */
    if (!system_hash64map_get(segment_ptr->node_id_to_descriptor_map,
                              (system_hash64) src_node_id,
                              &src_node) ||
        !system_hash64map_get(segment_ptr->node_id_to_descriptor_map,
                              (system_hash64) dst_node_id,
                              &dst_node) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve node descriptor(s)");

        goto end;
    }

    /* Check if the input & output are compatible.. */
    if (demo_timeline_segment_node_is_node_output_compatible_with_node_input(src_node,
                                                                             src_node_output_id,
                                                                             dst_node,
                                                                             dst_node_input_id) )
    {
        LOG_FATAL("Source node's output is incompatible with destination node's input.");

        goto end;
    }

    /* Check if the DAG is still acyclic if we include the new connection */
    sedes;

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC demo_timeline_postprocessing_segment demo_timeline_postprocessing_segment_create(ogl_context               context,
                                                                                        demo_timeline             owner_timeline,
                                                                                        system_hashed_ansi_string name)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != NULL,
                      "Rendering context handle is NULL");
    ASSERT_DEBUG_SYNC(owner_timeline != NULL,
                      "Owner timeline is NULL");
    ASSERT_DEBUG_SYNC(name != NULL                                  &&
                      system_hashed_ansi_string_get_length(name) > 0,
                      "Invalid postprocessing segment name specified");

    /* Create a new instance */
    _demo_timeline_postprocessing_segment* new_segment_ptr = new (std::nothrow) _demo_timeline_postprocessing_segment(context,
                                                                                                                      name,
                                                                                                                      owner_timeline);

    ASSERT_ALWAYS_SYNC(new_segment_ptr != NULL,
                       "Out of memory");

    if (new_segment_ptr != NULL)
    {
        /* Add the output node */
        bool result = demo_timeline_postprocessing_segment_add_node( (demo_timeline_postprocessing_segment) new_segment_ptr,
                                                                    DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT,
                                                                    system_hashed_ansi_string_create("Output node"),
                                                                    NULL,  /* out_opt_node_id_ptr */
                                                                    NULL); /* out_opt_node_ptr */

        ASSERT_DEBUG_SYNC(result,
                          "Could not add the output node to the post-processing segment.");

        /* Sign up for timeline notifications. We need to react whenever new video segments, that overlap with the time region
         * covered by this postprocessing segment, show up or are removed from the timeline, or are moved or resized. */
        system_callback_manager timeline_callback_manager = NULL;

        demo_timeline_get_property(owner_timeline,
                                   DEMO_TIMELINE_PROPERTY_CALLBACK_MANAGER,
                                  &timeline_callback_manager);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_postprocessing_segment_on_new_video_segment_added,
                                                        new_segment_ptr);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_DELETED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_postprocessing_segment_on_video_segment_deleted,
                                                        new_segment_ptr);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_postprocessing_segment_on_video_segment_moved,
                                                        new_segment_ptr);

        system_callback_manager_subscribe_for_callbacks(timeline_callback_manager,
                                                        DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,
                                                        CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                        _demo_timeline_postprocessing_segment_on_video_segment_resized,
                                                        new_segment_ptr);

        /* Finally, register the object */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_segment_ptr,
                                                       _demo_timeline_postprocessing_segment_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE_POSTPROCESSING_SEGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timeline Segment (Post-processing)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_segment_ptr != NULL) */

    return (demo_timeline_postprocessing_segment) new_segment_ptr;
}

/** Please see header for specificatio */
PUBLIC bool demo_timeline_postprocessing_segment_delete_nodes(demo_timeline_postprocessing_segment segment,
                                                              uint32_t                             n_nodes,
                                                              const demo_timeline_segment_node_id* node_ids)
{
    bool                                   result      = false;
    _demo_timeline_postprocessing_segment* segment_ptr = (_demo_timeline_postprocessing_segment*) segment;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input postprocessing segment is NULL");

        goto end;
    } /* if (segment == NULL) */

    if (n_nodes == 0)
    {
        /* Don't waste any cycles.. */
        goto end;
    } /* if (n_nodes == 0) */

    if (node_ids == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "NULL node IDs array but n_nodes=[%d]",
                          n_nodes);

        goto end;
    } /* if (node_ids == NULL) */

    /* Iterate over all nodes and ensure they can be safely deleted */
    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        demo_timeline_segment_node node      = NULL;
        int                        node_type = -1;

        if (!system_hash64map_get(segment_ptr->node_id_to_descriptor_map,
                                  (system_hash64) node_ids[n_node],
                                  &node) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find node descriptor for node with ID [%d]",
                              node_ids[n_node]);

            goto end;
        } /* if (node descriptor could not have been retrieved) */

        demo_timeline_segment_node_get_property(node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_TYPE,
                                               &node_type);

        if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Node ID [%d] refers to an output node which cannot be deleted",
                              node_ids[n_node]);

            goto end;
        } /* if (node_type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT) */
    } /* for (all node IDs) */

    sedes;

    /* All done */
    result = true;

end:
    return result;
}