/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#ifndef DEMO_TIMELINE_VIDEO_SEGMENT_H
#define DEMO_TIMELINE_VIDEO_SEGMENT_H

#include "demo/demo_types.h"
#include "ogl/ogl_types.h"
#include "system/system_types.h"

typedef PFNOGLPIPELINECALLBACKPROC PFNDEMOTIMELINEVIDEOSEGMENTPASSCALLBACKPROC;

/* Helper structure used to declare a single pass of a video segment.
 *
 * Only used by demo_timeline_add_video_segment()
 */
typedef struct
{
    /* Name of the pass. Used when presenting performance statistics */
    system_hashed_ansi_string name;

    /* Function to call back to render stage contents. */
    PFNDEMOTIMELINEVIDEOSEGMENTPASSCALLBACKPROC pfn_callback_proc;

    /* User data to pass with the call-back */
    void* user_arg;
} demo_timeline_video_segment_pass;

typedef enum
{
    /* float; settable.
     *
     * Aspect ratio which should be used for the video segment.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_ASPECT_RATIO,

    /* system_hashed_ansi_string; not settable.
     *
     * Name assigned to the video segment at creation time.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_PROPERTY_NAME
} demo_timeline_video_segment_property;

typedef enum
{
    /* Video segment which is described by a DAG. Each node is described by a relevant render call-back func ptr,
     * inputs and outputs taken by the node. This allows to order the execution on-the-fly and parallelize the work
     * whenever the dependencies are met.
     **/
    // TODO: DEMO_TIMELINE_VIDEO_SEGMENT_TYPE_GRAPH_BASED

    /* Video segment which is built of rendering passes. These are executed sequentially one after another.
     * No dependency support is present.
     */
    DEMO_TIMELINE_VIDEO_SEGMENT_TYPE_PASS_BASED,
} demo_timeline_video_segment_type;



REFCOUNT_INSERT_DECLARATIONS(demo_timeline_video_segment,
                             demo_timeline_video_segment)


/** Appends a set of passes to the specified video segment.
 *
 *  NOTE: This function may only be called for a pass-based video segment. Assertion failure will
 *        occur otherwise.
 *
 *  @param segment  Video segment to configure.
 *  @param n_passes Number of items available under @param passes. Must be > 0.
 *  @param passes   Array of video segment pass descriptors.
 *
 *  @return true if the operation was successful, false otherwise.
 */
PUBLIC EMERALD_API bool demo_timeline_video_segment_add_passes(demo_timeline_video_segment             segment,
                                                               unsigned int                            n_passes,
                                                               const demo_timeline_video_segment_pass* passes);

/** Creates a new video segment of type @param type.
 *
 *  At creation time, the video segment:
 *  - does not define any dependencies, nodes (graph-based video segment).
 *  - does not define any set of passes or outputs.
 *
 * These need to be added manually after the object is instantiated.
 *
 *  NOTE: This function should only be used by demo_timeline. Use relevant demo_timeline_add_video_segment*()
 *        funcs to instantiate a video segment.
 *
 *  @param context        Rendering context
 *  @param pipeline       Rendering pipeline
 *  @param owner_timeline Timeline instance which is going to control lifetime of the video segment.
 *  @param name           Name of the new video segment.
 *  @param type           Type of the video segment.
 *
 *  @return Requested instance.
 */
PUBLIC demo_timeline_video_segment demo_timeline_video_segment_create(ogl_context                      context,
                                                                      ogl_pipeline                     pipeline,
                                                                      demo_timeline                    owner_timeline,
                                                                      system_hashed_ansi_string        name,
                                                                      demo_timeline_video_segment_type type);

/** TODO */
PUBLIC void demo_timeline_video_segment_get_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     void*                                out_result_ptr);

/** TODO */
PUBLIC EMERALD_API RENDERING_CONTEXT_CALL bool demo_timeline_video_segment_render(demo_timeline_video_segment segment,
                                                                                  uint32_t                    frame_index,
                                                                                  system_time                 rendering_pipeline_time,
                                                                                  const int*                  rendering_area_px_topdown);

/** TODO */
PUBLIC void demo_timeline_video_segment_set_property(demo_timeline_video_segment          segment,
                                                     demo_timeline_video_segment_property property,
                                                     const void*                          data);

#endif /* DEMO_TIMELINE_VIDEO_SEGMENT_H */
