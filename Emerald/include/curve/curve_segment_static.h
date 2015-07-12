/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_SEGMENT_STATIC_H
#define CURVE_SEGMENT_STATIC_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC bool curve_segment_static_add_node(curve_segment_data,
                                          system_time,
                                          system_variant,
                                          curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_static_delete_node(curve_segment_data,
                                             curve_segment_node_id);

/** TODO */
PUBLIC bool curve_segment_static_deinit(curve_segment_data);

/** TODO */
PUBLIC bool curve_segment_static_get_amount_of_nodes(curve_segment_data,
                                                     uint32_t*);

/** TODO */
PUBLIC bool curve_segment_static_get_node(curve_segment_data,
                                          curve_segment_node_id,
                                          system_time*,
                                          system_variant);

/** TODO */
PUBLIC bool curve_segment_static_get_node_in_order(curve_segment_data,
                                                   uint32_t,
                                                   curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_static_get_value(curve_segment_data,
                                           system_time,
                                           system_variant,
                                           bool);

/** TODO */
PUBLIC bool curve_segment_static_init(curve_segment_data*,
                                      system_variant);

/** TODO */
PUBLIC bool curve_segment_static_modify_node_time(curve_segment_data,
                                                  curve_segment_node_id,
                                                  system_time);

/** TODO */
PUBLIC bool curve_segment_static_modify_node_time_value(curve_segment_data,
                                                        curve_segment_node_id,
                                                        system_time,
                                                        system_variant,
                                                        bool);

/** TODO */
PUBLIC bool curve_segment_static_set_property(curve_segment_data,
                                              curve_segment_property,
                                              system_variant);

/** TODO */
PUBLIC bool curve_segment_static_get_property(curve_segment_data,
                                              curve_segment_property,
                                              system_variant);

#endif /* CURVE_SEGMENT_STATIC_H */
