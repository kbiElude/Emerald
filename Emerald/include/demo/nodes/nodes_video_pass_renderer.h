#if 0

TODO: Remove. Video passes are deprecated.

/**
 *
 *  Emerald (kbi/elude @ 2015-2016)
 *
 **/
#ifndef NODES_VIDEO_PASS_RENDERER_H
#define NODES_VIDEO_PASS_RENDERER_H

#include "demo/demo_timeline_segment_node.h"
#include "demo/demo_types.h"
#include "ral/ral_types.h"


typedef enum
{
    /* TODO */
} nodes_video_pass_renderer_property;


/** TODO */
PUBLIC void nodes_video_pass_renderer_deinit(demo_timeline_segment_node_private node);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_get_texture_memory_allocation_details(demo_timeline_segment_node_private node,
                                                                            uint32_t                           n_allocation,
                                                                            demo_texture_memory_allocation*    out_memory_allocation_data_ptr);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_get_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   void*                              out_result_ptr);

/** TODO */
PUBLIC demo_timeline_segment_node_private nodes_video_pass_renderer_init(demo_timeline_segment      segment,
                                                                         demo_timeline_segment_node node,
                                                                         ral_context                context);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_render(demo_timeline_segment_node_private node,
                                             uint32_t                           frame_index,
                                             system_time                        frame_time,
                                             const int32_t*                     rendering_area_px_topdown);

/** TODO */
PUBLIC bool nodes_video_pass_renderer_set_property(demo_timeline_segment_node_private node,
                                                   int                                property,
                                                   const void*                        data);

/** TODO */
PUBLIC void nodes_video_pass_renderer_set_texture_memory_allocation(demo_timeline_segment_node_private node,
                                                                    uint32_t                           n_allocation,
                                                                    ral_texture                        texture);

#endif /* NODES_VIDEO_PASS_RENDERER_H */

#endif