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
PUBLIC bool curve_segment_tcb_add_node(__in  __notnull curve_segment_data,
                                       __in            system_time,
                                       __in  __notnull system_variant,
                                       __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_delete_node(__in __notnull curve_segment_data,
                                          __in           curve_segment_node_id);

/** TODO */
PUBLIC bool curve_segment_tcb_deinit(__in __notnull __post_invalid curve_segment_data);

/** TODO */
PUBLIC bool curve_segment_tcb_get_amount_of_nodes(__in  __notnull curve_segment_data,
                                                  __out __notnull uint32_t*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node(__in __notnull    curve_segment_data,
                                       __in              curve_segment_node_id,
                                       __out __maybenull system_time*,
                                       __out __maybenull system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_index(__in __notnull  curve_segment_data,
                                                         __in            uint32_t,
                                                         __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_id_for_node_in_order(__in __notnull  curve_segment_data,
                                                            __in            uint32_t,
                                                            __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_tcb_get_node_property(__in    __notnull curve_segment_data,
                                                __in              curve_segment_node_id,
                                                __in              curve_segment_node_property,
                                                __inout __notnull system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_property(__in __notnull curve_segment_data,
                                           __in           curve_segment_property,
                                           __in __notnull system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_get_value(__in __notnull curve_segment_data,
                                                       system_time,
                                        __inout        system_variant,
                                        __in           bool);

/** TODO */
PUBLIC bool curve_segment_tcb_init(__inout __notnull     curve_segment_data*,
                                   __in __notnull        curve_container,
                                   __in                  system_time,
                                   __in __ecount(3)      float*,
                                   __in __notnull        system_variant,
                                   __in                  system_time,
                                   __in __ecount(3)      float*,
                                   __in __notnull        system_variant,
                                   __in curve_segment_id segment_id
                                   );

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_time(__in __notnull curve_segment_data,
                                               __in           curve_segment_node_id,
                                               __in           system_time);

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_time_value(__in __notnull curve_segment_data,
                                                     __in           curve_segment_node_id,
                                                     __in           system_time,
                                                     __in __notnull system_variant,
                                                     __in           bool);

/** TODO */
PUBLIC bool curve_segment_tcb_modify_node_property(__in __notnull curve_segment_data,
                                                   __in           curve_segment_node_id,
                                                   __in           curve_segment_node_property,
                                                   __in __notnull system_variant);

/** TODO */
PUBLIC bool curve_segment_tcb_set_property(__in __notnull curve_segment_data,
                                           __in           curve_segment_property,
                                           __in __notnull system_variant);

#endif /* CURVE_SEGMENT_TCB_H */
