/**
 *
 *  Emerald (kbi/elude @ 2015)
 *
 **/
#ifndef NODES_POSTPROCESSING_VIDEO_SEGMENT_H
#define NODES_POSTPROCESSING_VIDEO_SEGMENT_H

#include "demo/demo_types.h"


/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void nodes_postprocessing_video_segment_deinit(demo_timeline_segment_node_private node);

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_get_input_property(demo_timeline_segment_node_private        node,
                                                                  demo_timeline_segment_node_input_id       input_id,
                                                                  demo_timeline_segment_node_input_property property,
                                                                  void*                                     out_result_ptr);

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_get_output_property(demo_timeline_segment_node_private         node,
                                                                   demo_timeline_segment_node_output_id       output_id,
                                                                   demo_timeline_segment_node_output_property property,
                                                                   void*                                      out_result_ptr);

/** TODO */
PUBLIC void nodes_postprocessing_video_segment_get_property(demo_timeline_segment_node_private  node,
                                                            demo_timeline_segment_node_property property,
                                                            void*                               out_result_ptr);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_postprocessing_video_segment_init(demo_timeline_segment_node node,
                                                                                                         void*                      postprocessing_segment,
                                                                                                         void*                      video_segment,
                                                                                                         void*                      timeline);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_postprocessing_video_segment_render(demo_timeline_segment_node_private node,
                                                                             uint32_t                           frame_index,
                                                                             system_time                        frame_time,
                                                                             const int32_t*                     rendering_area_px_topdown,
                                                                             void*                              user_arg);

#endif /* NODES_POSTPROCESSING_VIDEO_SEGMENT_H */