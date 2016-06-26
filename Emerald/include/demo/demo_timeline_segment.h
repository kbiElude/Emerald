/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#ifndef DEMO_TIMELINE_SEGMENT_H
#define DEMO_TIMELINE_SEGMENT_H

#include "demo/demo_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

typedef unsigned int demo_timeline_segment_input_id;
typedef unsigned int demo_timeline_segment_output_id;


/* IDs of call-backs used by demo_timeline_segment */
typedef enum
{
    /* Segment about to be released.
     *
     * @param arg Source demo_timeline_video_segment instance
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_ABOUT_TO_RELEASE,

    /* Node could not render last frame - one or more inputs, which require an attached object, lack it.
     *
     * Asynchronous call-back highly recommended. This call-back will be made from the rendering thread,
     * so synchronous call-back handlers should not stall for too long.
     *
     * @param arg ID of the segment node input, for which the problem was found when rendering the frame.
     *            This call-back will be fired once per each faulty input per frame.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_LACKS_ATTACHMENT,

    /* Node input is no longer exposed.
     *
     * @param arg Affected segment's ID.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_INPUT_NO_LONGER_EXPOSED,

    /* New node input has just been added.
     *
     * @param arg Affected segment's ID.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_INPUT_EXPOSED,

    /* New video segment node was created.
     *
     * @param arg New segment node ID
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_NODE_CREATED,

    /* A new node A output->node B input connection was created.
     *
     * @param arg Source node, whose one of the outputs was connected to another node's input.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NODE_INPUT_IS_NOW_CONNECTED,

    /* An existing node A output->node B input connection was deleted.
     *
     * @param arg Source node, whose one of the outputs was disconnected from the other node's input.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NODE_INPUT_NO_LONGER_CONNECTED,

    /* Node output is no longer exposed.
     *
     * @param arg Affected segment's ID.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_OUTPUT_NO_LONGER_EXPOSED,

    /* New node output has just been added.
     *
     * @param arg Affected segment's ID.
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_NEW_OUTPUT_EXPOSED,

    /* A texture attachment has been added to one of the owned nodes.
     *
     * Synchronous call-backs only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_attached_callback_argument instance
     */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_ATTACHED_TO_NODE,

    /* A texture attachment has been removed from one of the owned nodes.
     *
     * Synchronous call-backs only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_attached_callback_argument instance
     */
     DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_DETACHED_FROM_NODE,

    /* A new texture output has been added to one of the segment nodes
     *
     * Synchronous call-back only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_output_added_callback_argument instance.
     **/
     DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_ADDED_TO_NODE,

     /* A texture output has been removed from one of the segment nodes
     *
     * Synchronous call-back only.
     *
     * @param arg demo_timeline_segment_node_callback_texture_output_deleted_callback_argument instance.
     **/
     DEMO_TIMELINE_SEGMENT_CALLBACK_ID_TEXTURE_OUTPUT_DELETED_FROM_NODE,

    /* Always last */
    DEMO_TIMELINE_SEGMENT_CALLBACK_ID_COUNT
} demo_timeline_video_segment_callback_id;

typedef enum
{
    /* float; settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_ASPECT_RATIO,

    /* system_callback_manager; not settable.
     *
     * Callback manager instance which should be used to sign up for notifications.
     */
    DEMO_TIMELINE_SEGMENT_PROPERTY_CALLBACK_MANAGER,

    /* system_time; settable. **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_END_TIME,

    /* demo_timeline_segment_id; not settable. **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_ID,

    /* system_hashed_ansi_string; not settable.
     *
     * Name assigned to the video segment at creation time.
     */
    DEMO_TIMELINE_SEGMENT_PROPERTY_NAME,

    /* demo_timeline_segment_node; not settable. **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE,

    /* demo_timeline_segment_node_id; not settable.
     *
     * Query only valid for post-processing segments. **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_OUTPUT_NODE_ID,

    /* system_time; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_START_TIME,

    /* demo_timeline_segment_time_mode; settable.
     *
     * See demo_timeline_segment_time_mode documentation for more details.
     **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_TIME_MODE,

     /* demo_timeline; not settable.
     *
     * Parent timeline instance.
     **/
    DEMO_TIMELINE_SEGMENT_PROPERTY_TIMELINE,

    /* demo_timeline_segment_type; not settable. */
    DEMO_TIMELINE_SEGMENT_PROPERTY_TYPE,

} demo_timeline_segment_property;

typedef enum
{
    /* Rendering function will be called back with frame time relative to the beginning
     * of the segment.
     */
    DEMO_TIMELINE_SEGMENT_TIME_MODE_RELATIVE_TO_SEGMENT,

    /* Rendering function will be called back with frame time relative to the beginning
     * of the timeline.
     */
    DEMO_TIMELINE_SEGMENT_TIME_MODE_RELATIVE_TO_TIMELINE,
} demo_timeline_segment_time_mode;


REFCOUNT_INSERT_DECLARATIONS(demo_timeline_segment,
                             demo_timeline_segment)


/** TODO
 *
 *  @param segment             TODO
 *  @param node_type           Type of the segment node to create.
 *  @param name                TODO
 *  @param out_opt_node_id_ptr TODO
 *  @param out_opt_node_ptr    TODO
 *
 *  @return TODO
 **/
PUBLIC EMERALD_API bool demo_timeline_segment_add_node(demo_timeline_segment           segment,
                                                       demo_timeline_segment_node_type node_type,
                                                       system_hashed_ansi_string       name,
                                                       demo_timeline_segment_node_id*  out_opt_node_id_ptr,
                                                       demo_timeline_segment_node*     out_opt_node_ptr);

/** Connects one node's output with another node's input.
 *
 *  @param segment            Timeline segment both source and destination nodes are in.
 *  @param src_node_id        Source node's ID
 *  @param src_node_output_id Source node's output ID
 *  @param dst_node_id        Destination node's ID
 *  @param dst_node_input_id  Destination node's input ID
 *
 *  @return true if the new connection was created successfully, false otherwise. Look at the console log
 *          for more details in case of any failures.
 **/
PUBLIC EMERALD_API bool demo_timeline_segment_connect_nodes(demo_timeline_segment                segment,
                                                            demo_timeline_segment_node_id        src_node_id,
                                                            demo_timeline_segment_node_output_id src_node_output_id,
                                                            demo_timeline_segment_node_id        dst_node_id,
                                                            demo_timeline_segment_node_input_id  dst_node_input_id);

/** TODO */
PUBLIC demo_timeline_segment demo_timeline_segment_create_postprocessing(ral_context               context,
                                                                         demo_timeline             owner_timeline,
                                                                         system_hashed_ansi_string name,
                                                                         system_time               start_time,
                                                                         system_time               end_time,
                                                                         demo_timeline_segment_id  id);

/** Creates a new DAG segment.
 *
 *  At creation time, the segment:
 *  - does not define any dependencies or nodes.
 *  - does not define any set of passes or inputs.
 *  - defines a single output of texture type specified at creation time.
 *
 * The first two need to be added manually after the object is instantiated.
 *
 *  NOTE: This function should only be used by demo_timeline. Use the demo_timeline_add_video_segment()
 *        func to instantiate a video segment.
 *
 *  @param context               Rendering context
 *  @param owner_timeline        Timeline instance which is going to control lifetime of the video segment.
 *  @param output_texture_format Texture format to use for the output node, to which the DAG will store texture data.
 *  @param name                  Name of the new video segment.
 *  @param start_time            Start time of the video segment.
 *  @param end_time              End time of the video segment.
 *
 *  @return Requested instance.
 */
PUBLIC demo_timeline_segment demo_timeline_segment_create_video(ral_context               context,
                                                                demo_timeline             owner_timeline,
                                                                ral_format                output_texture_format,
                                                                system_hashed_ansi_string name,
                                                                system_time               start_time,
                                                                system_time               end_time,
                                                                demo_timeline_segment_id  id);

/** TODO */
PUBLIC bool demo_timeline_segment_delete_nodes(demo_timeline_segment                segment,
                                               uint32_t                             n_nodes,
                                               const demo_timeline_segment_node_id* node_ids);

/** TODO
 *
 *  NOTE: The function can only be used for video segment node input or outputs.
 *  NOTE: The function will fail if the request is to expose an IO which has already been exposed,
 *        or to hide an IO which has not been exposed.
 *
 *
 *  @param segment                   TODO
 *  @param is_input_io               Tells if @param node_io_id refers to segment node input ID (true) or
 *                                   a segment node output ID (false).
 *  @param node_id                   TODO
 *  @param node_io_id                ID of the segment node IO which should either be exposed (true) on the segment-level
 *                                   or hidden (false).
 *  @param should_expose             TODO
 *
 *  @return true if the function succeeded, false otherwise.
 *
 **/
PUBLIC EMERALD_API bool demo_timeline_segment_expose_node_io(demo_timeline_segment         segment,
                                                             bool                          is_input_io,
                                                             demo_timeline_segment_node_id node_id,
                                                             uint32_t                      node_io_id,
                                                             bool                          should_expose,
                                                             bool                          should_expose_as_vs_input);

/** TODO */
PUBLIC void demo_timeline_segment_free_exposed_io_result(demo_timeline_segment_node_io* segment_node_ios_ptr);

/** TODO
 *
 *  Retrieved arrays must be freed with a demo_timeline_segment_free_exposed_io_result() call, when no longer
 *  needed.
 *
 *  @param segment                  TODO
 *  @param out_n_inputs_ptr         TODO
 *  @param out_segment_node_ios_ptr TODO
 **/
PUBLIC bool demo_timeline_segment_get_exposed_inputs(demo_timeline_segment           segment,
                                                     uint32_t*                       out_n_inputs_ptr,
                                                     demo_timeline_segment_node_io** out_segment_node_ios_ptr);

/** TODO
 *
 *  Retrieved arrays must be freed with a demo_timeline_segment_free_exposed_io_result() call, when no longer
 *  needed.
 *
 *  @param segment                  TODO
 *  @param out_n_outputs_ptr        TODO
 *  @param out_segment_node_ios_ptr TODO
 **/
PUBLIC bool demo_timeline_segment_get_exposed_outputs(demo_timeline_segment           segment,
                                                      uint32_t*                       out_n_outputs_ptr,
                                                      demo_timeline_segment_node_io** out_segment_node_ios_ptr);

/** TODO
 *
 *  @param segment        TODO
 *  @param node_id        TODO
 *  @param is_input_io    Tells if @param io_id is a segment node input ID (true), or a segment node output ID (false).
 *  @param io_id          TODO
 *  @param property       A demo_timeline_segment_node_input_property value if @param is_input_io is true,
 *                        or a demo_timeline_segment_node_output_property value otherwise.
 *  @param out_result_ptr TODO
 *
 *  @return TODO
 **/
PUBLIC bool demo_timeline_segment_get_node_io_property_by_node_id(demo_timeline_segment                  segment,
                                                                  demo_timeline_segment_node_id          node_id,
                                                                  bool                                   is_input_io,
                                                                  uint32_t                               io_id,
                                                                  demo_timeline_segment_node_io_property property,
                                                                  void*                                  out_result_ptr);

/** TODO */
PUBLIC bool demo_timeline_segment_get_node_property(demo_timeline_segment         segment,
                                                    demo_timeline_segment_node_id node_id,
                                                    int                           property,
                                                    void*                         out_result_ptr);

/** TODO */
PUBLIC EMERALD_API void demo_timeline_segment_get_property(demo_timeline_segment          segment,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_segment_get_video_segment_node_id_for_ps_and_vs_segments(demo_timeline_segment          postprocessing_segment,
                                                                                               demo_timeline_segment          video_segment,
                                                                                               demo_timeline_segment_node_id* out_node_id_ptr);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_segment_render(demo_timeline_segment segment,
                                                                            uint32_t              frame_index,
                                                                            system_time           rendering_pipeline_time,
                                                                            const int*            rendering_area_px_topdown);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_segment_set_node_io_property(demo_timeline_segment_node             node,
                                                                   bool                                   is_input_io,
                                                                   uint32_t                               io_id,
                                                                   demo_timeline_segment_node_io_property property,
                                                                   const void*                            data);


/** TODO */
PUBLIC EMERALD_API bool demo_timeline_segment_set_node_property(demo_timeline_segment         segment,
                                                                demo_timeline_segment_node_id node_id,
                                                                int                           property,
                                                                const void*                   data);

/** TODO */
PUBLIC void demo_timeline_segment_set_property(demo_timeline_segment          segment,
                                               demo_timeline_segment_property property,
                                               const void*                    data);

#endif /* DEMO_TIMELINE_SEGMENT_H */
