/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_SEGMENT_LINEAR_H
#define CURVE_SEGMENT_LINEAR_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/** TODO */
PUBLIC bool curve_segment_linear_add_node(__in  __notnull curve_segment_data,
                                          __in            system_time,
                                          __in  __notnull system_variant,
                                          __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_linear_delete_node(__in __notnull curve_segment_data,
                                             __in           curve_segment_node_id);

/** TODO */
PUBLIC bool curve_segment_linear_deinit(__in __notnull curve_segment_data);

/** TODO */
PUBLIC bool curve_segment_linear_get_amount_of_nodes(__in  __notnull curve_segment_data,
                                                     __out __notnull uint32_t*);

/** TODO */
PUBLIC bool curve_segment_linear_get_node(__in __notnull    curve_segment_data,
                                          __in              curve_segment_node_id,
                                          __out __maybenull system_time*,
                                          __out __maybenull system_variant);

/** TODO */
PUBLIC bool curve_segment_linear_get_node_in_order(__in __notnull  curve_segment_data,
                                                   __in            uint32_t,
                                                   __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_linear_get_value(__in    __notnull curve_segment_data,
                                                             system_time,
                                           __inout __notnull system_variant,
                                           __in              bool);

/** TODO */
PUBLIC bool curve_segment_linear_init(__deref_out __notnull curve_segment_data*  segment_data,
                                                            system_variant_type  data_type,
                                                            system_time start_time,
                                                            system_variant       start_value,
                                                            system_time end_time,
                                                            system_variant       end_value);

/** TODO */
PUBLIC bool curve_segment_linear_modify_node_time(__in __notnull curve_segment_data,
                                                  __in           curve_segment_node_id,
                                                  __in           system_time);

/** TODO */
PUBLIC bool curve_segment_linear_modify_node_time_value(__in __notnull curve_segment_data,
                                                        __in           curve_segment_node_id,
                                                        __in           system_time,
                                                        __in __notnull system_variant,
                                                        __in           bool force);

/** TODO */
PUBLIC bool curve_segment_linear_set_property(__in __notnull curve_segment_data,
                                              __in           curve_segment_property,
                                              __in __notnull system_variant);

/** TODO */
PUBLIC bool curve_segment_linear_get_property(__in __notnull curve_segment_data,
                                              __in           curve_segment_property,
                                              __in __notnull system_variant);

#endif /* CURVE_SEGMENT_LINEAR_H */
