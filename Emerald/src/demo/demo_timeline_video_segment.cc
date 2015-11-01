/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "demo/nodes/nodes_video_pass_renderer.h"
#include "demo/demo_timeline_segment_node.h"
#include "demo/demo_timeline_video_segment.h"
#include "ogl/ogl_context.h"                 /* TODO: Remove OpenGL dependency ! */
#include "ogl/ogl_context_textures.h"        /* TODO: Remove OpenGL dependency ! */
#include "ogl/ogl_pipeline.h"                /* TODO: Remove OpenGL dependency ! (issue #121) */
#include "ogl/ogl_texture.h"                 /* TODO: Remove OpenGL dependency ! */
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


typedef struct _demo_timeline_video_segment_io
{
    union
    {
        demo_timeline_segment_node_input_id  input_id;
        demo_timeline_segment_node_output_id output_id;
    };

    demo_timeline_segment_node node; /* DO NOT release */


    explicit _demo_timeline_video_segment_io(demo_timeline_segment_node          in_node,
                                             demo_timeline_segment_node_input_id in_input_id)
    {
        input_id = in_input_id;
        node     = in_node;
    }
} _demo_timeline_video_segment_io;

/** Defines a single src node output ID -> dst node input ID connection */
typedef struct _demo_timeline_video_segment_io_connection
{
    system_dag_connection                     dag_connection;

    struct _demo_timeline_video_segment_node* dst_node;
    demo_timeline_video_segment_input_id      dst_node_input_id;

    struct _demo_timeline_video_segment_node* src_node;
    demo_timeline_segment_node_output_id      src_node_output_id;

    _demo_timeline_video_segment_io_connection(_demo_timeline_video_segment_node*    in_dst_node,
                                               demo_timeline_video_segment_input_id  in_dst_node_input_id,
                                               _demo_timeline_video_segment_node*    in_src_node,
                                               demo_timeline_video_segment_output_id in_src_node_output_id,
                                               system_dag_connection                 in_dag_connection)
    {
        dag_connection     = in_dag_connection;
        dst_node           = in_dst_node;
        dst_node_input_id  = in_dst_node_input_id;
        src_node           = in_src_node;
        src_node_output_id = in_src_node_output_id;
    }
} _demo_timeline_video_segment_io_connection;

typedef struct _demo_timeline_video_segment_node
{
    system_dag_node                       dag_node;
    uint32_t                              id;
    demo_timeline_segment_node            node;               /* owned */
    demo_timeline_segment_node_private    node_internal;
    system_resizable_vector               outgoing_connections; /* owns _demo_timeline_video_segment_io_connection* instances */
    struct _demo_timeline_video_segment*  parent_segment_ptr; /* DO NOT release */
    demo_timeline_video_segment_node_type type;

    _demo_timeline_video_segment_node(uint32_t                              in_id,
                                      demo_timeline_segment_node            in_node,
                                      _demo_timeline_video_segment*         in_parent_segment_ptr,
                                      demo_timeline_video_segment_node_type in_type)
    {
        dag_node             = NULL;
        id                   = in_id;
        node                 = in_node;
        node_internal        = NULL;
        outgoing_connections = system_resizable_vector_create(sizeof(_demo_timeline_video_segment_io_connection*));
        parent_segment_ptr   = in_parent_segment_ptr;
        type                 = in_type;
    }

    ~_demo_timeline_video_segment_node()
    {
        if (node != NULL)
        {
            demo_timeline_segment_node_release(node);

            node = NULL;
        } /* if (node != NULL) */

        if (outgoing_connections != NULL)
        {
            _demo_timeline_video_segment_io_connection* current_connection_ptr = NULL;

            while (system_resizable_vector_pop(outgoing_connections,
                                              &current_connection_ptr) )
            {
                delete current_connection_ptr;

                current_connection_ptr = NULL;
            }

            system_resizable_vector_release(outgoing_connections);
            outgoing_connections = NULL;
        } /* if (outgoing_connections != NULL) */
    }
} _demo_timeline_video_segment_node;


typedef struct _demo_timeline_video_segment
{
    float                              aspect_ratio;       /* AR to use for the video segment */
    system_callback_manager            callback_manager;
    ogl_context                        context;
    system_hash64map                   dag_node_to_node_descriptor_map; /* does NOT own the stored _demo_timeline_video_segment_node instances */
    system_hashed_ansi_string          name;
    uint32_t                           node_id_counter;
    system_hash64map                   node_id_to_node_descriptor_map; /* does NOT own the stored _demo_timeline_video_segment_node instances */
    system_resizable_vector            nodes;                          /* owns the stored _demo_timeline_video_segment_node instances;
                                                                        * item order DOES NOT need to correspond to node ID.
                                                                        * Use node_id_to_node_descriptor_map to find descriptor of nodes with specific IDs */
    _demo_timeline_video_segment_node* output_node_ptr;                /* DO NOT release */
    ral_texture_format                 output_texture_format;
    ogl_pipeline                       pipeline;
    uint32_t                           pipeline_stage_id;
    demo_timeline                      timeline;

    /* Handles DAG-related computations. The DAG operates on segment nodes, not individual inputs or outputs.
     * Input->output dependencies need to be defined layer above, in demo_timeline_video_segment */
    system_dag                         dag;
    system_resizable_vector            dag_sorted_nodes;

    /* These form the exposed input & output namespaces */
    uint32_t                  exposed_input_id_counter;
    system_hash64map          exposed_input_id_to_io_descriptor_map;  /* owns _demo_timeline_video_segment_io instances */
    uint32_t                  exposed_output_id_counter;
    system_hash64map          exposed_output_id_to_io_descriptor_map; /* owns _demo_timeline_video_segment_io instances */

    REFCOUNT_INSERT_VARIABLES


    explicit _demo_timeline_video_segment(ogl_context               in_context,
                                          system_hashed_ansi_string in_name,
                                          ogl_pipeline              in_pipeline,
                                          demo_timeline             in_timeline,
                                          ral_texture_format        in_output_texture_format)
    {
        aspect_ratio          = 1.0f;
        callback_manager      = system_callback_manager_create( (_callback_id) DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_COUNT);
        context               = in_context;
        dag                   = system_dag_create();
        dag_sorted_nodes      = system_resizable_vector_create(16 /* capacity */);
        name                  = in_name;
        node_id_counter       = 0;
        nodes                 = system_resizable_vector_create(sizeof(_demo_timeline_video_segment_node*) );
        output_node_ptr       = NULL;
        output_texture_format = in_output_texture_format;
        pipeline              = in_pipeline;
        pipeline_stage_id     = -1;
        timeline              = in_timeline;

        dag_node_to_node_descriptor_map = system_hash64map_create(sizeof(_demo_timeline_video_segment_node*) );
        node_id_to_node_descriptor_map  = system_hash64map_create(sizeof(_demo_timeline_video_segment_node*) );

        exposed_input_id_counter               = 0;
        exposed_input_id_to_io_descriptor_map  = system_hash64map_create(sizeof(_demo_timeline_video_segment_io*) );
        exposed_output_id_counter              = 0;
        exposed_output_id_to_io_descriptor_map = system_hash64map_create(sizeof(_demo_timeline_video_segment_io*) );

        ASSERT_ALWAYS_SYNC(callback_manager != NULL,
                           "Could not spawn the callback manager");
        ASSERT_ALWAYS_SYNC(dag_sorted_nodes != NULL,
                           "Could not create a vector holding sorted DAG nodes");
        ASSERT_ALWAYS_SYNC(exposed_input_id_to_io_descriptor_map != NULL,
                           "Could not create exposed input id->input descriptor map.");
        ASSERT_ALWAYS_SYNC(exposed_output_id_to_io_descriptor_map != NULL,
                           "Could not create exposed output id->output descriptor map.");
        ASSERT_ALWAYS_SYNC(node_id_to_node_descriptor_map != NULL,
                           "Could not create node ID->node descriptor map");
        ASSERT_ALWAYS_SYNC(nodes != NULL,
                           "Could not create video segment nodes vector");
    }

    ~_demo_timeline_video_segment()
    {
        /* Release the owned objects */
        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        } /* if (callback_manager != NULL) */

        if (dag != NULL)
        {
            system_dag_release(dag);

            dag = NULL;
        } /* if (dag != NULL) */

        if (dag_node_to_node_descriptor_map != NULL)
        {
            system_hash64map_release(dag_node_to_node_descriptor_map);

            dag_node_to_node_descriptor_map = NULL;
        } /* if (dag_node_to_node_descriptor_map != NULL) */

        if (exposed_input_id_to_io_descriptor_map != NULL)
        {
            system_hash64map_release(exposed_input_id_to_io_descriptor_map);

            exposed_input_id_to_io_descriptor_map = NULL;
        } /* if (exposed_input_id_to_io_descriptor_map != NULL) */

        if (exposed_output_id_to_io_descriptor_map != NULL)
        {
            system_hash64map_release(exposed_output_id_to_io_descriptor_map);

            exposed_output_id_to_io_descriptor_map = NULL;
        } /* if (exposed_output_id_to_io_descriptor_map != NULL) */

        if (node_id_to_node_descriptor_map != NULL)
        {
            system_hash64map_release(node_id_to_node_descriptor_map);

            node_id_to_node_descriptor_map = NULL;
        } /* if (node_id_to_node_descriptor_map != NULL) */

        if (nodes != NULL)
        {
            _demo_timeline_video_segment_node* current_node_ptr = NULL;

            while (system_resizable_vector_pop(nodes,
                                              &current_node_ptr) )
            {
                delete current_node_ptr;

                current_node_ptr = NULL;
            }

            system_resizable_vector_release(nodes);
            nodes = NULL;
        } /* if (nodes != NULL) */
    }
} _demo_timeline_video_segment;

/* Global declarations */
static struct
{
    const char*                           name;
    demo_timeline_video_segment_node_type type;

    PFNSEGMENTNODEDEINITCALLBACKPROC      pfn_deinit_proc;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_property_proc;
    PFNSEGMENTNODEINITCALLBACKPROC        pfn_init_proc;
    PFNSEGMENTNODERENDERCALLBACKPROC      pfn_render_proc;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC pfn_set_property_proc;
} _node_data[] =
{
    /* DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT */
    {
        "Output node",
        DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT,

        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },

    /* DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER */
    {
        "Pass renderer node",
        DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_PASS_RENDERER,

        nodes_video_pass_renderer_deinit,
        nodes_video_pass_renderer_get_property,
        nodes_video_pass_renderer_init,
        nodes_video_pass_renderer_render,
        nodes_video_pass_renderer_set_property
    }
};

/** Forward declarations */
PRIVATE void _demo_timeline_video_segment_deinit_node_rendering_callback        (ogl_context                        context,
                                                                                 void*                              node);
PRIVATE void _demo_timeline_video_segment_init_node_rendering_callback          (ogl_context                        context,
                                                                                 void*                              node);
PRIVATE void _demo_timeline_video_segment_on_texture_attached_to_node           (const void*                        texture_void_ptr,
                                                                                       void*                        segment);
PRIVATE void _demo_timeline_video_segment_on_texture_detached_from_node         (const void*                        texture_void_ptr,
                                                                                       void*                        segment);
PRIVATE void _demo_timeline_video_segment_release                               (void*                              segment);
PRIVATE void _demo_timeline_video_segment_update_node_io_exposure               (_demo_timeline_video_segment_node* node_ptr,
                                                                                 bool                               is_input,
                                                                                 uint32_t                           io_id,
                                                                                 bool                               should_expose);
PRIVATE void _demo_timeline_video_segment_update_node_input_texture_attachments (_demo_timeline_video_segment_node* node_ptr);
PRIVATE void _demo_timeline_video_segment_update_node_output_texture_attachments(_demo_timeline_video_segment_node* node_ptr);


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(demo_timeline_video_segment,
                               demo_timeline_video_segment,
                              _demo_timeline_video_segment);


/** TODO */
PRIVATE void _demo_timeline_video_segment_deinit_node_rendering_callback(ogl_context context,
                                                                         void*       node)
{
    PFNSEGMENTNODEDEINITCALLBACKPROC   pfn_segment_deinit_proc = NULL;
    _demo_timeline_video_segment_node* segment_node_ptr        = (_demo_timeline_video_segment_node*) node;

    demo_timeline_segment_node_get_property(segment_node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_DEINIT_FUNC_PTR,
                                           &pfn_segment_deinit_proc);

    pfn_segment_deinit_proc(segment_node_ptr->node_internal);
}

/** TODO */
PRIVATE void _demo_timeline_video_segment_init_node_rendering_callback(ogl_context context,
                                                                       void*       node)
{
    PFNSEGMENTNODEINITCALLBACKPROC     pfn_segment_init_proc = NULL;
    _demo_timeline_video_segment_node* segment_node_ptr      = (_demo_timeline_video_segment_node*) node;

    demo_timeline_segment_node_get_property(segment_node_ptr->node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_INIT_FUNC_PTR,
                                           &pfn_segment_init_proc);

    pfn_segment_init_proc( (demo_timeline_segment) segment_node_ptr->parent_segment_ptr,
                          segment_node_ptr->node,
                          NULL,  /* user_arg1 */
                          NULL,  /* user_arg2 */
                          NULL); /* user_arg3 */
}

/** TODO */
PRIVATE void _demo_timeline_video_segment_on_texture_attached_to_node(const void* texture_void_ptr,
                                                                            void* segment)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;
    ogl_texture                   texture     = (ogl_texture)                   texture_void_ptr;

    ASSERT_DEBUG_SYNC(segment_ptr != NULL,
                      "Input segment instance is NULL");
    ASSERT_DEBUG_SYNC(texture != NULL,
                      "Input texture instance is NULL");

    LOG_INFO("Performance warning: new texture attached to the node.");

    /* If the texture has been attached to a node output, we need to traverse the list of connections defined
     * for that node and bind that texture to inputs of nodes which are connected to that node output.
     *
     * Currently we do not receive info about which node the event is coming from so run through all nodes
     * and see if input texture attachments are defined. Shouldn't be much of perf concern, since this event
     * should only be fired at init time, or when explicitly requested by the app.
     */
    uint32_t n_nodes = 0;

    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    for (uint32_t n_destination_node = 0;
                  n_destination_node < n_nodes;
                ++n_destination_node)
    {
        demo_timeline_video_segment_input_id current_destination_node_input_ids[32]; /* TODO: we may need something more fancy in the future.. */
        _demo_timeline_video_segment_node*   current_destination_node_ptr           = NULL;
        uint32_t                             n_current_destination_node_input_ids   = 0;

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
            ogl_texture                         bound_texture                = NULL;
            demo_timeline_segment_node_input_id current_destination_input_id = current_destination_node_input_ids[n_input_id];
            bool                                has_bound_texture_to_input   = false;

            demo_timeline_segment_node_get_input_property(current_destination_node_ptr->node,
                                                          current_destination_input_id,
                                                          DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_BOUND_TEXTURE,
                                                         &bound_texture);

            if (bound_texture != NULL)
            {
                /* Nothing to do for this input */
                continue;
            } /* if (bound_texture != NULL) */

            /* Look through defined connections and see if a connection binding this input with any other segment node output is defined.
             * If so, check if a texture is attached to that output and - if that's the case - attach it to the input. */
            for (uint32_t n_source_node = 0;
                          n_source_node < n_nodes && !has_bound_texture_to_input;
                        ++n_source_node)
            {
                uint32_t                           n_connections_defined = 0;
                _demo_timeline_video_segment_node* source_node_ptr       = NULL;

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
                    _demo_timeline_video_segment_io_connection* connection_ptr = NULL;

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
                        ogl_texture source_node_bound_texture = NULL;

                        demo_timeline_segment_node_get_output_property(connection_ptr->src_node->node,
                                                                       connection_ptr->src_node_output_id,
                                                                       DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_BOUND_TEXTURE,
                                                                      &source_node_bound_texture);

                        if (source_node_bound_texture != NULL)
                        {
                            /* Bind that texture to the currently processed destination node and then move on to the next destination node. */
                            demo_texture_attachment_declaration new_texture_attachment;

                            new_texture_attachment.texture = source_node_bound_texture;

                            if (!demo_timeline_segment_node_attach_texture_to_texture_io(current_destination_node_ptr->node,
                                                                                         false, /* is_input_id */
                                                                                         current_destination_input_id,
                                                                                        &new_texture_attachment) )
                            {
                                ASSERT_DEBUG_SYNC(false,
                                                  "Could not attach a texture to the destination node's input ID");
                            }

                            has_bound_texture_to_input = true;
                        } /* if (source_node_bound_texture != NULL) */
                    } /* if (connection_ptr->dst_node == current_node_ptr) */
                } /* for (all outgoing connections defined for the currently processed connection node, until a texture is bound) */
            } /* for (all source nodes, until a texture is bound) */
        } /* for (all input IDs for currently processed node) */
    } /* for (all destination nodes) */
}

/** TODO */
PRIVATE void _demo_timeline_video_segment_on_texture_detached_from_node(const void* texture_void_ptr,
                                                                              void* segment)
{
    uint32_t                      n_nodes     = 0;
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;
    ogl_texture                   texture     = (ogl_texture)                   texture_void_ptr;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment_ptr != NULL,
                      "Input segment instance is NULL");
    ASSERT_DEBUG_SYNC(texture != NULL,
                      "Input texture instance is NULL");

    /* This call-back func will be invoked in two cases:
     *
     * 1) A texture formerly attached to a video segment node output was detached. In this case, we want to
     *    return that texture to the pool and ensure any inputs, to which that texture used to be bound to,
     *    have that texture detached, too.
     *
     * 2) A texture formerly attached to a video segment node input was detached. We need to specifically
     *    *ignore* this scenario, because otherwise we would have entered an endless loop.
     *
     * Iterate over all node outputs, identify the one, to which the texture used to be attached, and detach
     * that texture from any node inputs that that output was connected to.
     */
    system_resizable_vector_get_property(segment_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        _demo_timeline_video_segment_node*   current_node_ptr = NULL;
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
            ogl_texture                          current_node_bound_texture = NULL;
            uint32_t                             current_node_n_connections = 0;
            demo_timeline_segment_node_output_id current_node_output_id     = node_output_ids[n_node_output_id];

            demo_timeline_segment_node_get_output_property(current_node_ptr->node,
                                                           current_node_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_BOUND_TEXTURE,
                                                          &current_node_bound_texture);

            if (current_node_bound_texture != texture)
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
                _demo_timeline_video_segment_io_connection* current_connection_ptr = NULL;

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
                    ogl_texture dst_node_bound_texture = NULL;

                    /* Sanity check.. */
                    demo_timeline_segment_node_get_input_property(current_connection_ptr->dst_node->node,
                                                                  current_connection_ptr->dst_node_input_id,
                                                                  DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_BOUND_TEXTURE,
                                                                  &dst_node_bound_texture);

                    ASSERT_DEBUG_SYNC(dst_node_bound_texture == texture,
                                      "Invalid bound texture detected at connection destination");

                    /* Detach the texture */
                    demo_timeline_segment_node_attach_texture_to_texture_io(current_connection_ptr->dst_node->node,
                                                                            true, /* is_input_id */
                                                                            current_connection_ptr->dst_node_input_id,
                                                                            NULL); /* texture_attachment_ptr */
                } /* if (found a connection which starts at the output, whose texture is about to be detached */
            } /* for (all outgoing connections) */
        } /* for (all node output IDs) */
    } /* for (all defined nodes) */

    /* The texture attachment was provided by the video segment. We use the rendering-wide texture pool to
     * ensure textures are reused whenever possible.
     *
     * Since the texture is no longer needed, return it back to the pool.
     *
     * TODO: We need a refcounted solution here to properly support cases where the same texture is
     *       consumed by multiple inputs !
     */
    ogl_context_textures_return_reusable(segment_ptr->context,
                                         texture);
}

/* TODO */
PRIVATE void _demo_timeline_video_segment_release(void* segment)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Ping any interested parties about the fact we're winding up */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,
                                      (void*) segment_ptr);

    /* Actual tear-down will be handled by the destructor. */
}

/** TODO */
PRIVATE void _demo_timeline_video_segment_update_node_io_exposure(_demo_timeline_video_segment_node* node_ptr,
                                                                  bool                               is_input,
                                                                  uint32_t                           io_id,
                                                                  bool                               should_expose)
{
    system_hash64map io_map  = (is_input) ? node_ptr->parent_segment_ptr->exposed_input_id_to_io_descriptor_map
                                          : node_ptr->parent_segment_ptr->exposed_output_id_to_io_descriptor_map;
    uint32_t         n_items = 0;

    system_hash64map_get_property(io_map,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_items);

    if (!should_expose)
    {
       bool found_a_match = false;

       for (uint32_t n_item = 0;
                     n_item < n_items;
                   ++n_item)
       {
           system_hash64                    current_hash   = 0;
           _demo_timeline_video_segment_io* current_io_ptr = NULL;

           if (!system_hash64map_get_element_at(io_map,
                                                n_item,
                                                &current_io_ptr,
                                                &current_hash) )
           {
               ASSERT_DEBUG_SYNC(false,
                                 "Could not retrieve IO descriptor at index [%d]",
                                 n_item);

               continue;
           }

           if (current_io_ptr->node == node_ptr->node)
           {
               if (is_input)
               {
                   found_a_match = (current_io_ptr->input_id == io_id);
               }
               else
               {
                   found_a_match = (current_io_ptr->output_id == io_id);
               }
           } /* if (current_io_ptr->node == node_ptr->node) */

           if (found_a_match)
           {
               /* 1. Release the descriptor */
               delete current_io_ptr;

               /* 2. Remove it from the map. */
               if (!system_hash64map_remove(io_map,
                                            current_hash) )
               {
                   ASSERT_DEBUG_SYNC(false,
                                     "Could not remove the IO descriptor")
               }

               /* 3. Fire the notification. */
               system_callback_manager_call_back(node_ptr->parent_segment_ptr->callback_manager,
                                                 is_input ? DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_INPUT_NO_LONGER_EXPOSED
                                                          : DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_OUTPUT_NO_LONGER_EXPOSED,
                                                 (void*) current_hash);

               break;
           }
       } /* for (all map items) */

       ASSERT_DEBUG_SYNC(found_a_match,
                         "Could not update video segment node IO exposure status - IO descriptor not found.");
    } /* if (!should_expose) */
    else
    {
        uint32_t                         new_id     = -1;
        _demo_timeline_video_segment_io* new_io_ptr = NULL;

        /* Make sure the node is not already exposed */
        for (uint32_t n_item = 0;
                      n_item < n_items;
                    ++n_item)
       {
           system_hash64                    current_hash   = 0;
           _demo_timeline_video_segment_io* current_io_ptr = NULL;

           if (!system_hash64map_get_element_at(io_map,
                                                n_item,
                                                &current_io_ptr,
                                                &current_hash) )
           {
               ASSERT_DEBUG_SYNC(false,
                                 "Could not retrieve IO descriptor at index [%d]",
                                 n_item);

               continue;
           }

           if (current_io_ptr->node == node_ptr->node)
           {
               if ( is_input && current_io_ptr->input_id  == current_hash ||
                   !is_input && current_io_ptr->output_id == current_hash)
               {
                   LOG_ERROR("Requested node input/output is already exposed!");

                   goto end;
               }
           }
       } /* for (all existing exposed items) */

        /* Spawn a new IO descriptor */
        if (is_input)
        {
            new_id     = system_atomics_increment(&node_ptr->parent_segment_ptr->exposed_input_id_counter);
            new_io_ptr = new (std::nothrow) _demo_timeline_video_segment_io(node_ptr->node,
                                                                            (demo_timeline_segment_node_input_id) io_id);
        } /* if (is_input) */
        else
        {
            new_id     = system_atomics_increment(&node_ptr->parent_segment_ptr->exposed_output_id_counter);
            new_io_ptr = new (std::nothrow) _demo_timeline_video_segment_io(node_ptr->node,
                                                                            (demo_timeline_segment_node_output_id) io_id);
        }

        if (new_io_ptr == NULL)
        {
            ASSERT_ALWAYS_SYNC(false,
                               "Out of memory");

            goto end;
        } /* if (new_io_ptr == NULL) */

        /* Store it */
        ASSERT_DEBUG_SYNC(!system_hash64map_contains(io_map,
                                                     new_id),
                          "New ID for the exposed IO map is already assigned an object.");

        if (!system_hash64map_insert(io_map,
                                     new_id,
                                     new_io_ptr,
                                     NULL,   /* callback */
                                     NULL) ) /* callback_argument */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not store new IO descriptor");

            goto end;
        }

        /* Let the event subscribers know something happened */
        system_callback_manager_call_back(node_ptr->parent_segment_ptr->callback_manager,
                                          is_input ? DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_NEW_INPUT_EXPOSED
                                                   : DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_NEW_OUTPUT_EXPOSED,
                                          (void*) new_id);
    }

end:
    ;
}

/** TODO */
PRIVATE void _demo_timeline_video_segment_update_node_output_texture_attachments(_demo_timeline_video_segment_node* node_ptr)
{
    bool                                 has_updated_output_texture_attachment = false;
    uint32_t                             n_output_ids                          = 0;
    demo_timeline_segment_node_output_id output_ids[16]; /* NOTE: we may need something more fancy in the future.. */
    ogl_context_textures                 texture_pool = NULL;

    ogl_context_get_property(node_ptr->parent_segment_ptr->context,
                             OGL_CONTEXT_PROPERTY_TEXTURES,
                            &texture_pool);

    demo_timeline_segment_node_get_outputs(node_ptr->node,
                                           sizeof(output_ids) / sizeof(output_ids[0]),
                                          &n_output_ids,
                                           output_ids);

    ASSERT_DEBUG_SYNC(n_output_ids <= sizeof(output_ids) / sizeof(output_ids[0]),
                      "TODO");

    for (uint32_t n_output_id = 0;
                  n_output_id < n_output_ids;
                ++n_output_id)
    {
        ogl_texture                               current_output_bound_texture = NULL;
        demo_timeline_segment_node_output_id      current_output_id            = output_ids[n_output_id];
        demo_timeline_segment_node_interface_type current_output_interface     = DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_UNKNOWN;

        /* Only worry about texture attachments .. */
        demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                       current_output_id,
                                                       DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_INTERFACE_TYPE,
                                                      &current_output_interface);

        if (current_output_interface != DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE)
        {
            ASSERT_DEBUG_SYNC(false,
                              "TODO?");

            continue;
        } /* if (current_output_interface != DEMO_TIMELINE_SEGMENT_NODE_INTERFACE_TYPE_TEXTURE) */

        demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                       current_output_id,
                                                       DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_BOUND_TEXTURE,
                                                      &current_output_bound_texture);

        if (current_output_bound_texture == NULL)
        {
            /* Grab a new texture from the pool and bind it to the node output.
             *
             * TODO: If the rendering context's framebuffer is resized, we need all textures with proportional size to be
             *       automagically updated!
             */
            ogl_texture                         new_texture                              = NULL;
            demo_texture_attachment_declaration new_texture_declaration;
            uint32_t                            requested_texture_depth                  = 0;
            ral_texture_format                  requested_texture_format                 = RAL_TEXTURE_FORMAT_UNKNOWN;
            uint32_t                            requested_texture_height                 = 0;
            uint32_t                            requested_texture_n_mipmaps              = 0;
            uint32_t                            requested_texture_n_samples              = 0;
            demo_texture_size_mode              requested_texture_size_mode              = DEMO_TEXTURE_SIZE_MODE_UNKNOWN;
            ral_texture_type                    requested_texture_type                   = RAL_TEXTURE_TYPE_UNKNOWN;
            bool                                requested_texture_uses_full_mipmap_chain = false;
            uint32_t                            requested_texture_width                  = 0;

            demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                           current_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_FORMAT,
                                                          &requested_texture_format);
            demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                           current_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_USES_FULL_MIPMAP_CHAIN,
                                                          &requested_texture_uses_full_mipmap_chain);
            demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                           current_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_MODE,
                                                          &requested_texture_size_mode);
            demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                           current_output_id,
                                                           DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_TYPE,
                                                          &requested_texture_type);

            switch (requested_texture_size_mode)
            {
                case DEMO_TEXTURE_SIZE_MODE_ABSOLUTE:
                {
                    uint32_t texture_size_absolute[3] = {0};

                    demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                                   current_output_id,
                                                                   DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_ABSOLUTE,
                                                                   texture_size_absolute);

                    requested_texture_width  = texture_size_absolute[0];
                    requested_texture_height = texture_size_absolute[1];
                    requested_texture_depth  = texture_size_absolute[2];

                    break;
                }

                case DEMO_TEXTURE_SIZE_MODE_PROPORTIONAL:
                {
                    uint32_t default_fb_size[2]           = {0};
                    float    texture_size_proportional[2] = {0.0f};

                    demo_timeline_segment_node_get_output_property(node_ptr->node,
                                                                   current_output_id,
                                                                   DEMO_TIMELINE_SEGMENT_NODE_OUTPUT_PROPERTY_TEXTURE_SIZE_PROPORTIONAL,
                                                                   texture_size_proportional);

                    ogl_context_get_property(node_ptr->parent_segment_ptr->context,
                                             OGL_CONTEXT_PROPERTY_DEFAULT_FBO_SIZE,
                                             default_fb_size);

                    requested_texture_width  = uint32_t(float(default_fb_size[0]) * texture_size_proportional[0]);
                    requested_texture_height = uint32_t(float(default_fb_size[1]) * texture_size_proportional[1]);

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Texture size mode not recognized.");
                }
            } /* switch (requested_texture_size_mode) */

            if (requested_texture_uses_full_mipmap_chain)
            {
                uint32_t max_size = 0;

                max_size = (requested_texture_width > requested_texture_height) ? requested_texture_width : requested_texture_height;
                max_size = (requested_texture_depth > max_size)                 ? requested_texture_depth : max_size;

                requested_texture_n_mipmaps = 1 + system_math_other_log2_uint32(max_size);
            } /* if (requested_texture_uses_full_mipmap_chain) */
            else
            {
                requested_texture_n_mipmaps = 1;
            }

            new_texture = ogl_context_textures_get_texture_from_pool(node_ptr->parent_segment_ptr->context,
                                                                     requested_texture_type,
                                                                     requested_texture_n_mipmaps,
                                                                     requested_texture_format,
                                                                     requested_texture_width,
                                                                     requested_texture_height,
                                                                     requested_texture_depth,
                                                                     requested_texture_n_samples,
                                                                     false); /* fixed_sample_locations */

            if (new_texture == NULL)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve a new texture from the texture pool.");

                continue;
            } /* if (new_texture == NULL) */

            /* Attach the texture to the segment node output. */
            new_texture_declaration.texture = new_texture;

            if (!demo_timeline_segment_node_attach_texture_to_texture_io(node_ptr->node,
                                                                         false, /* is_input_id */
                                                                         current_output_id,
                                                                         &new_texture_declaration) )
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not attach a texture to the segment node output");

                ogl_context_textures_return_reusable(node_ptr->parent_segment_ptr->context,
                                                     new_texture);

                continue;
            }

            has_updated_output_texture_attachment = true;
        } /* if (current_output_bound_texture == NULL) */
    } /* for (all node output IDs) */
}


/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_add_node(demo_timeline_video_segment           segment,
                                                 demo_timeline_video_segment_node_type node_type,
                                                 system_hashed_ansi_string             name,
                                                 demo_timeline_segment_node_id*        out_opt_node_id_ptr,
                                                 demo_timeline_segment_node*           out_opt_node_ptr)
{
    uint32_t                           new_node_id                       = -1;
    _demo_timeline_video_segment_node* new_node_ptr                      = NULL;
    demo_timeline_segment_node         new_segment_node                  = NULL;
    system_callback_manager            new_segment_node_callback_manager = NULL;
    bool                               result                            = false;
    _demo_timeline_video_segment*      segment_ptr                       = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment is NULL");

        goto end;
    } /* if (segment == NULL) */

    if (name == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid video segment node name");

        goto end;
    }

    if (node_type                    == DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT &&
        segment_ptr->output_node_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Output node already created for the specified video segment");

        goto end;
    }

    if (node_type >= DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_COUNT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid video segment node type requested");

        goto end;
    } /* if (node_type >= DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_COUNT) */

    /* Try to create the requested node type. */
    new_node_id = system_atomics_increment(&segment_ptr->node_id_counter);

    ASSERT_DEBUG_SYNC(_node_data[node_type].type == node_type,
                      "Node data array corruption");

    new_segment_node = demo_timeline_segment_node_create(DEMO_TIMELINE_SEGMENT_TYPE_VIDEO,
                                                         DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT,
                                                         new_node_id,
                                                         _node_data[node_type].pfn_deinit_proc,
                                                         _node_data[node_type].pfn_get_property_proc,
                                                         _node_data[node_type].pfn_init_proc,
                                                         NULL, /* init_user_args_void3 */
                                                         _node_data[node_type].pfn_render_proc,
                                                         _node_data[node_type].pfn_set_property_proc,
                                                         system_hashed_ansi_string_create(_node_data[node_type].name) );

    if (new_segment_node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create new video segment node.");

        goto end;
    } /* if (new_segment_node == NULL) */

    if (node_type == DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT)
    {
        /* Add the only input to the output node */
        demo_texture_input_declaration output_segment_node_input_info;

        output_segment_node_input_info.input_name             = system_hashed_ansi_string_create("Result");
        output_segment_node_input_info.is_attachment_required = true;
        output_segment_node_input_info.texture_format         = segment_ptr->output_texture_format;
        output_segment_node_input_info.texture_n_layers       = 1;
        output_segment_node_input_info.texture_n_samples      = 1;
        output_segment_node_input_info.texture_type           = RAL_TEXTURE_TYPE_2D;

        ral_utils_get_texture_format_property(segment_ptr->output_texture_format,
                                              RAL_TEXTURE_FORMAT_PROPERTY_N_COMPONENTS,
                                             &output_segment_node_input_info.texture_n_components);

        if (!demo_timeline_segment_node_add_texture_input(new_segment_node,
                                                         &output_segment_node_input_info,
                                                          NULL) ) /* out_opt_input_id_ptr */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not add texture input to the video segment's output node.");

            goto end;
        }
    } /* if (node_type == DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT) */

    /* Create the node descriptor */
    new_node_ptr = new (std::nothrow) _demo_timeline_video_segment_node(new_node_id,
                                                                        new_segment_node,
                                                                        segment_ptr,
                                                                        DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT);

    if (new_node_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Out of memory");

        goto end;
    }

    /* Sign up for node call-backs */
    demo_timeline_segment_node_get_property(new_segment_node,
                                            DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,
                                           &new_segment_node_callback_manager);

    system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_ATTACHED,
                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                    _demo_timeline_video_segment_on_texture_attached_to_node,
                                                    segment_ptr);
    system_callback_manager_subscribe_for_callbacks(new_segment_node_callback_manager,
                                                    DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                                    CALLBACK_SYNCHRONICITY_SYNCHRONOUS,
                                                    _demo_timeline_video_segment_on_texture_detached_from_node,
                                                    segment_ptr);

    /* Create a DAG node for the new video segment node and store it */
    new_node_ptr->dag_node = system_dag_add_node(segment_ptr->dag,
                                                 (system_dag_node_value) new_node_ptr);

    if (new_node_ptr->dag_node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not create a DAG node");

        goto end;
    }

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(segment_ptr->dag_node_to_node_descriptor_map,
                                                 (system_hash64) new_node_ptr->dag_node),
                      "DAG node is already recognized by the DAG node->node descriptor map.");

    system_hash64map_insert(segment_ptr->dag_node_to_node_descriptor_map,
                            (system_hash64) new_node_ptr->dag_node,
                            new_node_ptr,
                            NULL,  /* on_removal_callback */
                            NULL); /* callback_argument */

    /* If the node needs to be initialized, request a rendering callback and call back node impl */
    if (_node_data[node_type].pfn_init_proc != NULL)
    {
        ogl_context_request_callback_from_context_thread(segment_ptr->context,
                                                         _demo_timeline_video_segment_init_node_rendering_callback,
                                                         new_node_ptr);
    }

    /* For each texture output defined for the node at this point, make sure a texture object is attached */
    _demo_timeline_video_segment_update_node_output_texture_attachments(new_node_ptr);

    /* Store the node descriptor */
    ASSERT_DEBUG_SYNC(!system_hash64map_contains(segment_ptr->node_id_to_node_descriptor_map,
                                                 new_node_id),
                      "Node id->node descriptor map already holds an item for the new node's ID");

    system_hash64map_insert     (segment_ptr->node_id_to_node_descriptor_map,
                                 new_node_id,
                                 new_node_ptr,
                                 NULL,  /* on_remove_callback */
                                 NULL); /* callback_argument  */
    system_resizable_vector_push(segment_ptr->nodes,
                                 new_node_ptr);

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_NEW_NODE_CREATED,
                                      (void*) new_node_id);

    /* All done */
    result = true;

end:
    if (!result)
    {
        if (new_node_ptr != NULL)
        {
            delete new_node_ptr;

            new_node_ptr = NULL;
        } /* if (new_node_ptr != NULL) */

        if (new_segment_node != NULL)
        {
            demo_timeline_segment_node_release(new_segment_node);

            new_segment_node = NULL;
        }
    } /* if (!result) */

    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_connect_nodes(demo_timeline_video_segment          segment,
                                                      demo_timeline_segment_node_id        src_node_id,
                                                      demo_timeline_segment_node_output_id src_node_output_id,
                                                      demo_timeline_segment_node_id        dst_node_id,
                                                      demo_timeline_segment_node_output_id dst_node_input_id)
{
    _demo_timeline_video_segment_node*          dst_node_ptr       = NULL;
    bool                                        need_to_update_dag = true;
    _demo_timeline_video_segment_io_connection* new_connection_ptr = NULL;
    system_dag_connection                       new_dag_connection = NULL;
    bool                                        result             = false;
    _demo_timeline_video_segment*               segment_ptr        = NULL;
    _demo_timeline_video_segment_node*          src_node_ptr       = NULL;

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
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              (system_hash64) src_node_id,
                              &src_node_ptr) ||
        !system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              (system_hash64) dst_node_id,
                              &dst_node_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve node descriptor(s)");

        goto end;
    }

    /* Check if the input & output are compatible.. */
    if (demo_timeline_segment_node_is_node_output_compatible_with_node_input(src_node_ptr->node,
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
            _demo_timeline_video_segment_io_connection* current_connection_ptr = NULL;

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
        } /* for (all outgoing connections) */

        if (is_connection_already_defined)
        {
            LOG_FATAL("Cannot add a new video segment node->video segment node connection: specified connection is already defined");

            goto end;
        }
        else
        {
            /* The segments are already connected - no need to update the DAG */
            need_to_update_dag = false;
        }
    } /* if (the connection already exists) */

    if (need_to_update_dag)
    {
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

        if (new_dag_connection == NULL)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not create a new DAG connection");

            goto end;
        } /* if (new_dag_connection == NULL) */

        if (!system_dag_solve(segment_ptr->dag) )
        {
            /* Whoops, the DAG does not solve now. Remove the new connection, ensure it solves correct again,
             * report an error and quit */
            system_dag_delete_connection(segment_ptr->dag,
                                         new_dag_connection);

            ASSERT_ALWAYS_SYNC(system_dag_solve(segment_ptr->dag),
                               "DAG does not solve after a new connection was removed");

            goto end;
        } /* if (!system_dag_solve(segment_ptr->dag) ) */
    } /* if (need_to_update_dag) */

    /* Create the new connection descriptor & store it */
    new_connection_ptr = new (std::nothrow) _demo_timeline_video_segment_io_connection(dst_node_ptr,
                                                                                       dst_node_input_id,
                                                                                       src_node_ptr,
                                                                                       src_node_output_id,
                                                                                       new_dag_connection);

    if (new_connection_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(new_connection_ptr != NULL,
                          "Out of memory");

        goto end;
    } /* if (new_connection_ptr == NULL) */

    system_resizable_vector_push(dst_node_ptr->outgoing_connections,
                                 new_connection_ptr);

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_NODE_INPUT_IS_NOW_CONNECTED,
                                      src_node_ptr);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC demo_timeline_video_segment demo_timeline_video_segment_create(ogl_context               context,
                                                                      ogl_pipeline              pipeline,
                                                                      demo_timeline             owner_timeline,
                                                                      ral_texture_format        output_texture_format,
                                                                      system_hashed_ansi_string name)
{
    /* Sanity checks */
    ASSERT_DEBUG_SYNC(context != NULL,
                      "Rendering context handle is NULL");
    ASSERT_DEBUG_SYNC(owner_timeline != NULL,
                      "Owner timeline is NULL");
    ASSERT_DEBUG_SYNC(name != NULL                                  &&
                      system_hashed_ansi_string_get_length(name) > 0,
                      "Invalid video segment name specified");
    ASSERT_DEBUG_SYNC(output_texture_format < RAL_TEXTURE_FORMAT_COUNT,
                      "Invalid output texture format requested.");

    /* Create a new instance */
    _demo_timeline_video_segment* new_segment_ptr = new (std::nothrow) _demo_timeline_video_segment(context,
                                                                                                    name,
                                                                                                    pipeline,
                                                                                                    owner_timeline,
                                                                                                    output_texture_format);

    ASSERT_ALWAYS_SYNC(new_segment_ptr != NULL,
                       "Out of memory");

    if (new_segment_ptr != NULL)
    {
        /* Add the output node taking the user-specified texture format on input.
         *
         * TODO: The output is currently defined as a single-layered, single-mipmapped 2D texture but this restriction
         *       can be lifted in the future.
         */
        if (!demo_timeline_video_segment_add_node((demo_timeline_video_segment) new_segment_ptr,
                                                  DEMO_TIMELINE_VIDEO_SEGMENT_NODE_TYPE_OUTPUT,
                                                  system_hashed_ansi_string_create("Output node"),
                                                  NULL,  /* out_opt_node_id_ptr */
                                                  NULL)) /* out_opt_node_ptr */
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not add the output node to a video segment");
        }

        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(new_segment_ptr,
                                                       _demo_timeline_video_segment_release,
                                                       OBJECT_TYPE_DEMO_TIMELINE_VIDEO_SEGMENT,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\Demo Timeline Segment (Video)\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    } /* if (new_segment_ptr != NULL) */

    return (demo_timeline_video_segment) new_segment_ptr;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_delete_nodes(demo_timeline_video_segment          segment,
                                                     uint32_t                             n_nodes,
                                                     const demo_timeline_segment_node_id* node_ids)
{
    bool                          result      = false;
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment is NULL");

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
        _demo_timeline_video_segment_node* node_ptr  = NULL;

        if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                                  (system_hash64) node_ids[n_node],
                                  &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find node descriptor for node with ID [%d]",
                              node_ids[n_node]);

            goto end;
        } /* if (node descriptor could not have been retrieved) */

        /* The node must not be an output node */
        if (node_ptr->type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Node ID [%d] refers to an output node which cannot be deleted",
                              node_ids[n_node]);

            goto end;
        } /* if (node_ptr->type == DEMO_TIMELINE_POSTPROCESSING_SEGMENT_NODE_TYPE_OUTPUT) */

        /* The node must not be a part of any existing DAG connections */
        if (system_dag_is_connection_defined(segment_ptr->dag,
                                             node_ptr->dag_node,
                                             NULL) ||             /* dst */
            system_dag_is_connection_defined(segment_ptr->dag,
                                             NULL,                /* src */
                                             node_ptr->dag_node) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Node ID [%d] forms one or more DAG connections.",
                              node_ids[n_node]);

            goto end;
        }
    } /* for (all node IDs) */

    /* Good to delete the nodes */
    for (uint32_t n_node = 0;
                  n_node < n_nodes;
                ++n_node)
    {
        system_callback_manager            node_callback_manager = NULL;
        size_t                             node_find_index       = -1;
        PFNSEGMENTNODEDEINITCALLBACKPROC   node_pfn_deinit_proc  = NULL;
        _demo_timeline_video_segment_node* node_ptr              = NULL;

        if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                                  (system_hash64) node_ids[n_node],
                                  &node_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not find node descriptor for node with ID [%d]",
                              node_ids[n_node]);

            goto end;
        } /* if (node descriptor could not have been retrieved) */

        /* If there's a deinit callback func ptr defined, call it now */
        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_CALLBACK_MANAGER,
                                               &node_callback_manager);
        demo_timeline_segment_node_get_property(node_ptr->node,
                                                DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_DEINIT_FUNC_PTR,
                                               &node_pfn_deinit_proc);

        if (node_pfn_deinit_proc != NULL)
        {
            ogl_context_request_callback_from_context_thread(segment_ptr->context,
                                                             _demo_timeline_video_segment_deinit_node_rendering_callback,
                                                             node_ptr);
        } /* if (node_pfn_deinit_proc != NULL) */

        /* Sign out of notifications we signed up for at creation time */
        system_callback_manager_unsubscribe_from_callbacks(node_callback_manager,
                                                           DEMO_TIMELINE_SEGMENT_NODE_CALLBACK_ID_TEXTURE_DETACHED,
                                                           _demo_timeline_video_segment_on_texture_detached_from_node,
                                                           segment_ptr);

        /* Release the node */
        demo_timeline_segment_node_release(node_ptr->node);

        if (!system_hash64map_remove(segment_ptr->node_id_to_node_descriptor_map,
                                     (system_hash64) node_ids[n_node]) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not remove video segment node decriptor from the node ID->node descriptor map.");
        }

        if ( (node_find_index = system_resizable_vector_find(segment_ptr->nodes,
                                                             (void*) node_ptr) ) != ITEM_NOT_FOUND)
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
        node_ptr = NULL;
    } /* for (all node IDs) */

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_disconnect_nodes(demo_timeline_video_segment          segment,
                                                         demo_timeline_segment_node_id        src_node_id,
                                                         demo_timeline_segment_node_output_id src_node_output_id,
                                                         demo_timeline_segment_node_id        dst_node_id,
                                                         demo_timeline_segment_node_output_id dst_node_input_id)
{
    uint32_t                                    connection_index                = -1;
    _demo_timeline_video_segment_io_connection* connection_ptr                  = NULL;
    _demo_timeline_video_segment_node*          dst_node_ptr                    = NULL;
    uint32_t                                    n_src_dst_connections           = 0;
    uint32_t                                    n_src_node_outgoing_connections = 0;
    bool                                        result                          = false;
    _demo_timeline_video_segment*               segment_ptr                     = (_demo_timeline_video_segment*) segment;
    _demo_timeline_video_segment_node*          src_node_ptr                    = NULL;

    /* Sanity checks */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment instance is NULL");

        goto end;
    } /* if (segment == NULL) */

    /* Retrieve node descriptors */
    if (!system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              (system_hash64) src_node_id,
                              &src_node_ptr) ||
        !system_hash64map_get(segment_ptr->node_id_to_node_descriptor_map,
                              (system_hash64) dst_node_id,
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
        _demo_timeline_video_segment_io_connection* current_connection_ptr = NULL;

        if (!system_resizable_vector_get_element_at(src_node_ptr->outgoing_connections,
                                                    n_outgoing_connection,
                                                   &current_connection_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve descriptor of the video segment node->video segment node connection at index [%d]",
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
                ASSERT_DEBUG_SYNC(connection_ptr == NULL,
                                  "Duplicated video segment node connections detected");

                connection_index = n_outgoing_connection;
                connection_ptr   = current_connection_ptr;
            }
        } /* if (current connection binds the specified source and destination nodes) */
    } /* for (all outgoing connections) */

    if (connection_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Specified node connection was not found");

        goto end;
    } /* if (connection_ptr == NULL) */

    /* Two DAG nodes are only connected if there's one or more connections between video segment nodes represented by those DAG nodes.
     * Two video segment nodes are only connected if there's at least one outgoing connection from source node to the destination node.
     *
     * Therefore, we can only destroy the DAG connection if there's only one outgoing connections that go from the source node
     * to the destination node.
     */
    if (n_src_dst_connections == 1)
    {
        system_dag_delete_connection(segment_ptr->dag,
                                     connection_ptr->dag_connection);
    } /* if (n_src_dst_connections == 1) */

    /* Inform subscribers about the event */
    system_callback_manager_call_back(segment_ptr->callback_manager,
                                      DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_NODE_INPUT_NO_LONGER_CONNECTED,
                                      connection_ptr->src_node);

    /* Release the connection descriptor and clean up */
    delete connection_ptr;
    connection_ptr = NULL;

    system_resizable_vector_delete_element_at(src_node_ptr->outgoing_connections,
                                              connection_index);

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_get_node_property(demo_timeline_video_segment   segment,
                                                          demo_timeline_segment_node_id node_id,
                                                          int                           property,
                                                          void*                         out_result_ptr)
{
    _demo_timeline_video_segment_node*    node_ptr                       = NULL;
    PFNSEGMENTNODEGETPROPERTYCALLBACKPROC pfn_get_node_property_func_ptr = NULL;
    bool                                  result                         = false;
    _demo_timeline_video_segment*         segment_ptr                    = (_demo_timeline_video_segment*) segment;

    /* Sanity check */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment is NULL");

        goto end;
    } /* if (segment == NULL) */

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

    if (pfn_get_node_property_func_ptr != NULL)
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
PUBLIC void demo_timeline_video_segment_get_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     void*                                out_result_ptr)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            *(float*) out_result_ptr = segment_ptr->aspect_ratio;

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = segment_ptr->callback_manager;

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = segment_ptr->name;

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_OUTPUT_NODE:
        {
            *(demo_timeline_segment_node*) out_result_ptr = segment_ptr->output_node_ptr->node;

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_TIMELINE:
        {
            *(demo_timeline*) out_result_ptr = segment_ptr->timeline;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property value.")
        }
    } /* switch (property) */
}

/** Please see header for specification */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_video_segment_render(demo_timeline_video_segment segment,
                                                                                  uint32_t                    frame_index,
                                                                                  system_time                 rendering_pipeline_time,
                                                                                  const int*                  rendering_area_px_topdown)
{
    uint32_t                      n_sorted_nodes = 0;
    bool                          result         = false;
    _demo_timeline_video_segment* segment_ptr    = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(ogl_context_get_current_context() == segment_ptr->context,
                      "Invalid rendering context thread");
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    /* 1. Retrieve a list of nodes, sorted topogically, in which we should proceed with the rendering process */
    if (!system_dag_get_topologically_sorted_node_values(segment_ptr->dag,
                                                         segment_ptr->dag_sorted_nodes) )
    {
        LOG_ERROR("Video segment DAG failed to solve. No output will be produced.");

        goto end;
    }

    /* 2. Perform the rendering process. For each node, before calling the render call-back, make sure all inputs that require a bound texture
     *    actually have one attached. If not, the render func ptr should not be called and we should signal a problem.
     */
    system_resizable_vector_get_property(segment_ptr->dag_sorted_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                         &n_sorted_nodes);

    for (uint32_t n_sorted_node = 0;
                  n_sorted_node < n_sorted_nodes;
                ++n_sorted_node)
    {
        bool                                can_render       = true;
        uint32_t                            n_node_input_ids = 0;
        demo_timeline_segment_node_input_id node_input_ids[32]; /* TODO: we may need something more fancy.. */
        _demo_timeline_video_segment_node*  node_ptr = NULL;

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
            ogl_texture bound_texture          = NULL;
            bool        is_attachment_required = false;

            demo_timeline_segment_node_get_input_property(node_ptr->node,
                                                          node_input_ids[n_node_input_id],
                                                          DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_IS_REQUIRED,
                                                         &is_attachment_required);

            if (!is_attachment_required)
            {
                /* Meh */
                continue;
            }

            demo_timeline_segment_node_get_input_property(node_ptr->node,
                                                          node_input_ids[n_node_input_id],
                                                          DEMO_TIMELINE_SEGMENT_NODE_INPUT_PROPERTY_BOUND_TEXTURE,
                                                         &bound_texture);

            if (bound_texture == NULL)
            {
                /* Sound the bugle! */
                system_callback_manager_call_back(node_ptr->parent_segment_ptr->callback_manager,
                                                  DEMO_TIMELINE_VIDEO_SEGMENT_CALLBACK_ID_INPUT_LACKS_ATTACHMENT,
                                                  (void*) node_input_ids[n_node_input_id]);

                /* This node cannot be rendered. */
                can_render = false;
            }
        } /* for (all node input IDs) */

        if (can_render)
        {
            PFNSEGMENTNODERENDERCALLBACKPROC render_func_ptr = NULL;

            demo_timeline_segment_node_get_property(node_ptr->node,
                                                    DEMO_TIMELINE_SEGMENT_NODE_PROPERTY_RENDER_FUNC_PTR,
                                                   &render_func_ptr);

            render_func_ptr(node_ptr->node_internal,
                            frame_index,
                            rendering_pipeline_time,
                            rendering_area_px_topdown);
        }
        else
        {
            LOG_ERROR("One or more video segment inputs lack an attachment - video segment won't produce output.")
        }
    }

    /* All done */
end:
    return result;

}

/** Please see header for specification */
PUBLIC EMERALD_API bool demo_timeline_video_segment_set_node_input_property(demo_timeline_video_segment_node                node,
                                                                            demo_timeline_segment_node_input_id             input_id,
                                                                            demo_timeline_video_segment_node_input_property property,
                                                                            const void*                                     data)
{
    _demo_timeline_video_segment_node* node_ptr = (_demo_timeline_video_segment_node*) node;
    bool                               result   = false;

    /* Sanity checks */
    if (node == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input node is NULL");

        goto end;
    }

    /* If this is a video segment-specific request, handle it now. Otherwise, pass it to the general node handler */
    switch (property)
    {
        case DEMO_TIMELINE_VIDEO_SEGMENT_NODE_INPUT_PROPERTY_EXPOSED:
        {
            bool should_expose = *(bool*) data;

            _demo_timeline_video_segment_update_node_io_exposure(node_ptr,
                                                                 true, /* is_input */
                                                                 input_id,
                                                                 should_expose);

            break;
        }

        case DEMO_TIMELINE_VIDEO_SEGMENT_NODE_INPUT_PROPERTY_TEXTURE_ATTACHMENT:
        {
            demo_timeline_segment_node_attach_texture_to_texture_io(node_ptr->node,
                                                                    true, /* is_input_id */
                                                                    input_id,
                                                                   *(demo_texture_attachment_declaration**) data);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized demo_timeline_video_segment_node_input_property value.");
        }
    } /* switch (property) */

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC bool demo_timeline_video_segment_set_node_property(demo_timeline_video_segment   segment,
                                                          demo_timeline_segment_node_id node_id,
                                                          int                           property,
                                                          const void*                   data)
{
    _demo_timeline_video_segment_node*    node_ptr                       = NULL;
    PFNSEGMENTNODESETPROPERTYCALLBACKPROC pfn_set_node_property_func_ptr = NULL;
    bool                                  result                         = false;
    _demo_timeline_video_segment*         segment_ptr                    = (_demo_timeline_video_segment*) segment;

    /* Sanity check */
    if (segment == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input video segment is NULL");

        goto end;
    } /* if (segment == NULL) */

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

    if (pfn_set_node_property_func_ptr != NULL)
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
PUBLIC void demo_timeline_video_segment_set_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     const void*                          data)
{
    _demo_timeline_video_segment* segment_ptr = (_demo_timeline_video_segment*) segment;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(segment != NULL,
                      "Input video segment is NULL");

    switch (property)
    {
        case DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO:
        {
            segment_ptr->aspect_ratio = *(float*) data;

            ASSERT_DEBUG_SYNC(segment_ptr->aspect_ratio > 0.0f,
                              "A segment AR of <= 0.0 value was requested!");

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized video segment property requested.");
        }
    } /* switch (property) */
}