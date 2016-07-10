/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef CURVE_SEGMENT_STATIC_H
#define CURVE_SEGMENT_STATIC_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC bool curve_segment_static_add_node(curve_segment_data     segment_data,
                                          system_time            node_time,
                                          system_variant         node_value,
                                          curve_segment_node_id* out_node_id);

/** TODO */
PUBLIC bool curve_segment_static_delete_node(curve_segment_data    segment_data,
                                             curve_segment_node_id node_id);

/** TODO */
PUBLIC bool curve_segment_static_deinit(curve_segment_data);

/** TODO */
PUBLIC bool curve_segment_static_get_amount_of_nodes(curve_segment_data segment_data,
                                                     uint32_t*          out_result);

/** TODO */
PUBLIC bool curve_segment_static_get_node(curve_segment_data    segment_data,
                                          curve_segment_node_id node_id,
                                          system_time*          out_node_time,
                                          system_variant        out_node_value);

/** TODO */
PUBLIC bool curve_segment_static_get_node_in_order(curve_segment_data     segment_data,
                                                   uint32_t               node_index,
                                                   curve_segment_node_id* out_node_id_ptr);

/** TODO */
PUBLIC bool curve_segment_static_get_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value);

/** TODO */
PUBLIC bool curve_segment_static_get_value(curve_segment_data segment_data,
                                           system_time        time,
                                           system_variant     out_result,
                                           bool               should_force);

/** TODO */
PUBLIC bool curve_segment_static_init(curve_segment_data* segment_data,
                                      system_variant      value);

/** TODO */
PUBLIC bool curve_segment_static_modify_node_time(curve_segment_data    segment_data,
                                                  curve_segment_node_id node_id,
                                                  system_time           new_node_time);

/** TODO */
PUBLIC bool curve_segment_static_modify_node_time_value(curve_segment_data    segment_data,
                                                        curve_segment_node_id node_id,
                                                        system_time           new_node_time,
                                                        system_variant        new_node_value,
                                                        bool                  unused);

/** TODO */
PUBLIC bool curve_segment_static_set_property(curve_segment_data     segment_data,
                                              curve_segment_property segment_property,
                                              system_variant         value);

#endif /* CURVE_SEGMENT_STATIC_H */
