/**
 *
 *  Emerald (kbi/elude @ 2015)
 *
 **/
#ifndef NODES_VIDEO_PASS_RENDERER_H
#define NODES_VIDEO_PASS_RENDERER_H

#include "demo/demo_types.h"

typedef enum
{
    /** settable; uint32_t
     *
     *  Defines the rendering pipeline's stage ID to render. The node uses timeline's rendering pipeline namespace.
     **/
    NODES_VIDEO_PASS_RENDERER_PROPERTY_RENDERING_PIPELINE_STAGE_ID,

} nodes_video_pass_renderer_property;


/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void nodes_video_pass_renderer_deinit(demo_timeline_segment_node_private node);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_get_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   void*                              out_result_ptr);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_video_pass_renderer_init(demo_timeline_segment      segment,
                                                                                                demo_timeline_segment_node node,
                                                                                                void*                      unused1,
                                                                                                void*                      unused2,
                                                                                                void*                      unused3);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_video_pass_renderer_render(demo_timeline_segment_node_private node,
                                                                    uint32_t                           frame_index,
                                                                    system_time                        frame_time,
                                                                    const int32_t*                     rendering_area_px_topdown);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_set_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   const void*                        data);

#endif /* NODES_VIDEO_PASS_RENDERER_H */