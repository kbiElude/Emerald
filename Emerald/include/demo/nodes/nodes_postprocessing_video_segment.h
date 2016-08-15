/**
 *
 *  Emerald (kbi/elude @ 2015-2016)
 *
 **/
#ifndef NODES_POSTPROCESSING_VIDEO_SEGMENT_H
#define NODES_POSTPROCESSING_VIDEO_SEGMENT_H

#include "demo/demo_timeline_segment_node.h"
#include "demo/demo_types.h"


enum
{
    /* settable ONCE; demo_timeline_segment (must be of video segment type) */
    NODES_POSTPROCESSING_VIDEO_SEGMENT_PROPERTY_VIDEO_SEGMENT
};

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_deinit(demo_timeline_segment_node_private node);

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_get_input_property(demo_timeline_segment_node_private     node,
                                                                  demo_timeline_segment_node_input_id    input_id,
                                                                  demo_timeline_segment_node_io_property property,
                                                                  void*                                  out_result_ptr);

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_get_output_property(demo_timeline_segment_node_private     node,
                                                                   demo_timeline_segment_node_output_id   output_id,
                                                                   demo_timeline_segment_node_io_property property,
                                                                   void*                                  out_result_ptr);

/** TODO */
PUBLIC bool nodes_postprocessing_video_segment_get_property(demo_timeline_segment_node_private  node,
                                                            int                                 property,
                                                            void*                               out_result_ptr);

/** TODO */
PUBLIC demo_timeline_segment_node_private nodes_postprocessing_video_segment_init(demo_timeline_segment      segment,
                                                                                  demo_timeline_segment_node node,
                                                                                  ral_context                context);

/** TODO */
PUBLIC bool nodes_postprocessing_video_segment_render(demo_timeline_segment_node_private node,
                                                      uint32_t                           frame_index,
                                                      system_time                        frame_time,
                                                      const int32_t*                     rendering_area_px_topdown,
                                                      ral_present_task*                  out_present_task_ptr);

/** TODO */
PUBLIC bool nodes_postprocessing_video_segment_set_property(demo_timeline_segment_node_private node,
                                                            int                                property,
                                                            const void*                        data);

#endif /* NODES_POSTPROCESSING_VIDEO_SEGMENT_H */