/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 * The timeline is one of the core objects for demo applications. It allows the apps to define
 * what contents should be generated/rendered/transferred between objects for each frame, as well as
 * how the visuals should be combined together, and how everything should be animated.
 *
 * The timeline itself is a function of time. It can hold two types of segments:
 *
 * a) Video segments          - these take optional number of inputs, generate stuff, and push it to outputs.
 * b) Postprocessing segments - these combine the visual data generated by video segments and eventually stream
 *                              the result to the back buffer.
 *
 * For a given timespan on the timeline, more than one video segment can be defined. Postprocessing segments,
 * on the other hand, are not allowed to overlap.
 *
 * A postprocessing segment can use data coming from any video segment that is defined for the region partially or
 * completely overlapping with the time region the postprocessing segment has been defined for.
 *
 * Both types of segments are defined by DAGs, whose nodes can take any number of buffer/image inputs and can output
 * a node-specific number of buffer or image outputs. One node output can be connected to as many node inputs as
 * needed, but a single input can only be connected to one output.
 * Each video segment output is represented by a special "result" node in the DAG. The DAG is considered invalid if
 * any of these special nodes is not connected to the graph, as that would imply no data would've been exposed at the
 * output during rendering time.
 *
 * Certain parts of node implementations (eg. a render() call-back) need to be written separately for each graphics
 * back-end.
 *
 * Node parameters (eg. "multiplier" for a "bump up brightness" node) are defined by curve containers. This lets you
 * assign values to parameters that may either be static or which can change over time, whichever suits your needs.
 *
 * The general idea is that you first define a number of video segments which draw "stuff" to textures, and then you
 * throw a single postprocessing segment on top of those video segments to combine their outputs and inputs together.
 * Of course, video segments may also output depth, stencil data, as well as write data to buffers, which can be
 * used by segments that consume corresponding inputs.
 */
#ifndef DEMO_TIMELINE_H
#define DEMO_TIMELINE_H

#include "demo/demo_timeline_segment.h"
#include "system/system_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(demo_timeline,
                             demo_timeline)

typedef enum
{
    /* New postprocessing segment was successfully added to the timeline.
     *
     * @param arg New postprocessing segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_ADDED,
    DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_DELETED,

    /* An existing postprocessing segment was moved on the timeline.
     *
     * @param arg Moved postprocessing segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_MOVED,

    /* An existing postprocessing segment was resized on the timeline.
     *
     * @param arg Resized postprocessing segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_POSTPROCESSING_SEGMENT_WAS_RESIZED,


    /* New video segment was successfully added to the timeline.
     *
     * @param arg New video segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_ADDED,
    DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_DELETED,

    /* An existing video segment was moved on the timeline.
     *
     * @param arg Moved video segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_MOVED,

    /* An existing video segment was resized on the timeline.
     *
     * @param arg Resized video segment's ID
     */
    DEMO_TIMELINE_CALLBACK_ID_VIDEO_SEGMENT_WAS_RESIZED,


    /* Always last */
    DEMO_TIMELINE_CALLBACK_ID_COUNT
} demo_timeline_callback_id;

/* Timeline properties */
typedef enum
{
    /* float; settable.
     *
     * Determines proportions of the viewport for frame times, for which no video segments
     * are defined.
     *
     * Use special demo_timeline_get_aspect_ratio() getter to query the AR value for a specific
     * frame time.
     */
    DEMO_TIMELINE_PROPERTY_ASPECT_RATIO,

    /* system_callback_manager; not settable. */
    DEMO_TIMELINE_PROPERTY_CALLBACK_MANAGER,

    /* system_time; not settable.
     *
     * End time of the segment located the furthest from the beginning of the timeline */
    DEMO_TIMELINE_PROPERTY_DURATION,

    /* unsigned int; not settable. */
    DEMO_TIMELINE_PROPERTY_N_VIDEO_SEGMENTS,

} demo_timeline_property;

/** Adds a new postprocessing segment to the timeline. Only one postprocessing segment is allowed
 *  for the specified time interval.
 *
 *  @param timeline                           Timeline to add the new postprocessing segment in.
 *  @param name                               Name of the new postprocessing segment.
 *  @param start_time                         Postprocessing segment's start time.
 *  @param end_time                           Postprocessing segment's end time.
 *  @param out_opt_segment_id_ptr             TODO
 *  @param out_opt_postprocessing_segment_ptr TODO
 */
PUBLIC EMERALD_API bool demo_timeline_add_postprocessing_segment(demo_timeline             timeline,
                                                                 system_hashed_ansi_string name,
                                                                 system_time               start_time,
                                                                 system_time               end_time,
                                                                 demo_timeline_segment_id* out_opt_segment_id_ptr,
                                                                 demo_timeline_segment*    out_opt_postprocessing_segment_ptr);

/** Adds a new video segment to the timeline. Video segments are allowed to overlap on the timeline.
 *
 *  @param timeline                     Timeline to add the new video segment in.
 *  @param name                         Name of the new video segment.
 *  @param start_time                   Video segment's start time
 *  @param end_time                     Video segment's end time. Must be > start_time.
 *  @param aspect_ratio                 Aspect ratio recommended for texture outputs of the video segment.
 *  @param output_texture_format        Texture format, in which the video segment is going to output data.
 *                                      Must not be RAL_TEXTURE_FORMAT_UNKNOWN.
 *  @param out_opt_video_segment_id_ptr If not NULL, deref will be set to the segment's ID.
 *  @param out_opt_video_segment_ptr    If not NULL, deref will be set to the segment's handle.
 *
 *  @return true if the video segment was successfully added to the timeline, false otherwise.
 */
PUBLIC EMERALD_API bool demo_timeline_add_video_segment(demo_timeline             timeline,
                                                        system_hashed_ansi_string name,
                                                        system_time               start_time,
                                                        system_time               end_time,
                                                        float                     aspect_ratio,
                                                        ral_format                output_texture_format,
                                                        demo_timeline_segment_id* out_opt_video_segment_id_ptr,
                                                        demo_timeline_segment*    out_opt_video_segment_ptr);

/** TODO. */
PUBLIC EMERALD_API demo_timeline demo_timeline_create(system_hashed_ansi_string name,
                                                      ral_context               context,
                                                      system_window             window);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_delete_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id);

/** Releases an array of segment IDs allocated by demo_timeline_get_segment_ids()
 *
 *  @param segment_ids The array to release. Must not be NULL.
 **/
PUBLIC EMERALD_API void demo_timeline_free_segment_ids(demo_timeline_segment_id* segment_ids);

/** TODO */
PUBLIC EMERALD_API float demo_timeline_get_aspect_ratio(demo_timeline timeline,
                                                        system_time   frame_time);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_property(const demo_timeline    timeline,
                                                   demo_timeline_property property,
                                                   void*                  out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_by_id(demo_timeline              timeline,
                                                        demo_timeline_segment_type segment_type,
                                                        demo_timeline_segment_id   segment_id,
                                                        demo_timeline_segment*     out_segment_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_id(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     unsigned int               n_segment,
                                                     demo_timeline_segment_id*  out_segment_id_ptr);

/** TODO
 *
 *  Internal use only.
 *
 *  @param timeline              TODO
 *  @param segment_type          TODO
 *  @param out_n_segment_ids_ptr TODO
 *  @param out_segment_ids_ptr   Deref will be set to a pointer to a buffer holding IDs of all
 *                               segments existing at the time of the call. The array holds
 *                               @param *out_n_segment_ids_ptr items. The array must be released
 *                               by the caller with demo_timeline_free_segment_ids(), when no longer
 *                               needed.
 **/
PUBLIC EMERALD_API bool demo_timeline_get_segment_ids(demo_timeline              timeline,
                                                      demo_timeline_segment_type segment_type,
                                                      unsigned int*              out_n_segment_ids_ptr,
                                                      demo_timeline_segment_id** out_segment_ids_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_get_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           void*                          out_result_ptr);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_move_segment(demo_timeline              timeline,
                                                   demo_timeline_segment_type segment_type,
                                                   demo_timeline_segment_id   segment_id,
                                                   system_time                new_segment_start_time);

/** TODO
 *
 *  Internal usage only.
 *
 *  @param timeline                       TODO
 *  @param frame_time                     TODO
 *  @param rendering_area_px_topdown [0]: x1 of the rendering area (in pixels)
 *                                   [1]: y1 of the rendering area (in pixels)
 *                                   [2]: x2 of the rendering area (in pixels)
 *                                   [3]: y2 of the rendering area (in pixels)
 *
 *  @return TODO 
 */
PUBLIC ral_present_job demo_timeline_render(demo_timeline timeline,
                                            uint32_t      frame_index,
                                            system_time   frame_time,
                                            const int*    rendering_area_px_topdown);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_resize_segment(demo_timeline              timeline,
                                                     demo_timeline_segment_type segment_type,
                                                     demo_timeline_segment_id   segment_id,
                                                     system_time                new_segment_duration);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_set_property(demo_timeline          timeline,
                                                   demo_timeline_property property,
                                                   const void*            data);

/** TODO */
PUBLIC EMERALD_API bool demo_timeline_set_segment_property(demo_timeline                  timeline,
                                                           demo_timeline_segment_type     segment_type,
                                                           demo_timeline_segment_id       segment_id,
                                                           demo_timeline_segment_property property,
                                                           const void*                    data);

#endif /* DEMO_TIMELINE_H */
