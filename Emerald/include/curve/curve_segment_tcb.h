/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_SEGMENT_TCB_H
#define CURVE_SEGMENT_TCB_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC bool curve_segment_tcb_add_node(curve_segment_data,
                                       system_time,
                                       system_variant,
                                       curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_delete_node(curve_segment_data,
                                          curve_segment_node_id);

/** TODO */
PUBLIC bool curve_segment_tcb_deinit(curve_segment_data);

/** TODO */
PUBLIC bool curve_segment_tcb_get_amount_of_nodes(curve_segment_data,
                                                  uint32_t*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node(curve_segment_data,
                                       curve_segment_node_id,
                                       system_time*,
                                       system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_index(curve_segment_data,
                                                         uint32_t,
                                                         curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_in_order(curve_segment_data,
                                                            uint32_t,
                                                            curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_property(curve_segment_data,
                                                curve_segment_node_id,
                                                curve_segment_node_property,
                                                system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_property(curve_segment_data,
                                           curve_segment_property,
                                           system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_value(curve_segment_data,
                                        system_time,
                                        system_variant,
                                        bool);

/** TODO */
PUBLIC bool curve_segment_tcb_init(curve_segment_data*,
                                   curve_container,
                                   system_time,
                                   float*,
                                   system_variant,
                                   system_time,
                                   float*,
                                   system_variant,
                                   curve_segment_id segment_id
                                   );

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_time(curve_segment_data,
                                               curve_segment_node_id,
                                               system_time);

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_time_value(curve_segment_data,
                                                     curve_segment_node_id,
                                                     system_time,
                                                     system_variant,
                                                     bool);

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_property(curve_segment_data,
                                                   curve_segment_node_id,
                                                   curve_segment_node_property,
                                                   system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_set_property(curve_segment_data,
                                           curve_segment_property,
                                           system_variant);

#endif /* CURVE_SEGMENT_TCB_H */
