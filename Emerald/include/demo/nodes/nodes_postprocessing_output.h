/**
 *
 *  Emerald (kbi/elude @ 2015)
 *
 **/
#ifndef NODES_POSTPROCESSING_OUTPUT_H
#define NODES_POSTPROCESSING_OUTPUT_H

#include "demo/demo_timeline_segment_node.h"
#include "demo/demo_types.h"


/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void nodes_postprocessing_output_deinit(demo_timeline_segment_node_private node);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL demo_timeline_segment_node_private nodes_postprocessing_output_init(demo_timeline_segment      segment,
                                                                                                  demo_timeline_segment_node node,
                                                                                                  ral_context                context);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL bool nodes_postprocessing_output_render(demo_timeline_segment_node_private node,
                                                                      uint32_t                           frame_index,
                                                                      system_time                        frame_time,
                                                                      const int32_t*                     rendering_area_px_topdown);

#endif /* NODES_POSTPROCESSING_OUTPUT_H */