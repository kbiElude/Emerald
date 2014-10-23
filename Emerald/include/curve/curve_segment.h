/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef CURVE_SEGMENT_H
#define CURVE_SEGMENT_H

#include "curve/curve_types.h"
#include "system/system_types.h"

/** Adds a new node to a given segment. Note that only TCB segments support user nodes.
 *
 *  @param curve_segment          Curve segment to use. Cannot be NULL.
 *  @param system_timeline_time   New node time.
 *  @param system_variant         New node value.
 *  @param curve_segment_node_id* If successful, deref will hold new node's id.
 *
 *  @return true if successful, false otherwise 
 **/
PUBLIC EMERALD_API bool curve_segment_add_node(__in __notnull  curve_segment,
                                               __in            system_timeline_time,
                                               __in __notnull  system_variant,
                                               __out __notnull curve_segment_node_id*);

/** Deletes an existing node from given segment. Note that only TCB segments can have their nodes
 *  deleted.
 *
 *  @param curve_segment         Curve segment to modify. Cannot be NULL.
 *  @param curve_segment_node_id Id of the node to remove.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_segment_delete_node(__in __notnull curve_segment,
                                                  __in           curve_segment_node_id);

/** Creates a LERP curve segment descriptor which uses the user-provided start/end values for
 *  given start/end times.
 *
 *  @param system_timeline_time Start time
 *  @param system_variant       Start value. Cannot be NULL.
 *  @param system_timeline_time End time.
 *  @param system_variant       End value. Cannot be NULL.
 *
 *  @return If successful, creates and returns the initialized segment. You can free it
 *          by using curve_segment_release() function.
 */
PUBLIC curve_segment curve_segment_create_linear(               system_timeline_time,
                                                 __in __notnull system_variant,
                                                                system_timeline_time,
                                                 __in __notnull system_variant);

/** Creates a static curve segment descriptor which uses the user-provided value.
 *
 *  @param system_variant Value to use for the segment. Cannot be NULL.
 *
 *  @return If successful, creates and returns the initialized segment. You can free it
 *          by using curve_segment_release() function.
 */
PUBLIC curve_segment curve_segment_create_static(__in __notnull system_variant);

/** Creates a TCB curve segment descriptor which uses the user-provided start/end values for 
 *  given start/end times and Tension/Continuity/Bias factors for both of the nodes.
 *
 *  @param system_timeline_time Start time
 *  @param float*               [0] = tension, [1] = continuity, [2] = bias for start node. Cannot be NULL.
 *  @param system_variant       Start value. Cannot be NULL.
 *  @param system_timeline_time End time.
 *  @param float*               [0] = tension, [1] = continuity, [2] = bias for end node. Cannot be NULL.
 *  @param system_variant       End value. Cannot be NULL.
 *  @param curve_container      TODO
 *  @param uint8_t              TODO
 *  @param curve_segment_id     TODO
 *
 *  @return If successful, creates and returns the initialized segment. You can free it
 *          by using curve_segment_release() function.
 */
PUBLIC curve_segment curve_segment_create_tcb(__in             system_timeline_time,
                                              __in __ecount(3) float*,
                                              __in __notnull   system_variant,
                                              __in             system_timeline_time,
                                              __in __ecount(3) float*,
                                              __in __notnull   system_variant,
                                              __in __notnull   curve_container,
                                              __in             curve_segment_id);

/** Retrieves amount of nodes held by given curve segment.
 *
 *  @param curve_segment Curve segment to query. Cannot be NULL.
 *  @param uint32_t*     Deref will hold the query result. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_segment_get_amount_of_nodes(__in  __notnull curve_segment,
                                                          __out __notnull uint32_t*);

/** TODO */
PUBLIC EMERALD_API system_timeline_time curve_segment_get_modification_time(__in __notnull curve_segment);

/** Retrieves node details for a given curve segment.
 *
 *  @param curve_segment         Curve segment to query. Cannot be NULL.
 *  @param curve_segment_node_id Id of curve segment node to retrieve properties for. 
 *  @param system_timeline_time* Deref will hold node time, if function finishes successfully.
 *  @param system_variant        Variant will be stored the node value, if function finishes successfully.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_segment_get_node(__in  __notnull   curve_segment,
                                               __in              curve_segment_node_id,
                                               __out __maybenull system_timeline_time*,
                                                     __maybenull system_variant);

/** TODO */
PUBLIC EMERALD_API bool curve_segment_get_node_at_time(__in  __notnull curve_segment,
                                                       __in            system_timeline_time,
                                                       __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC bool curve_segment_get_node_by_index(__in  __notnull curve_segment,
                                            __in            uint32_t,
                                            __out __notnull curve_segment_node_id*);

/** Retrieves id of a curve segment's node of user-requested index. The indexes will be sorted from the one
 *  located the closest to zero, and then moving on forward.
 *
 *  @param curve_segment          Curve segment to query. Cannot be NULL.
 *  @param uint32_t               Node index.
 *  @param curve_segment_node_id* Deref will hold the result, if successful.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC bool curve_segment_get_node_in_order(__in  __notnull  curve_segment,
                                            __in             uint32_t,
                                            __out __notnull curve_segment_node_id*);

/** TODO */
PUBLIC EMERALD_API bool curve_segment_get_node_property(__in    __notnull curve_segment,
                                                        __in    __notnull curve_segment_node_id,
                                                        __in    __notnull curve_segment_node_property,
                                                        __inout __notnull system_variant);

/** Retrieves variant type that is used for storing node values.
 *
 *  @param curve_segment Curve segment to query. Cannot be NULL.
 *  @param system_variant_type Deref will be used to hold the result, if function finishes successfully.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC bool curve_segment_get_node_value_variant_type(__in            __notnull curve_segment,
                                                      __out_ecount(1) __notnull system_variant_type*);

/** Retrieves curve segment's type.
 *
 *  @param curve_segment      Curve segment to query. Cannot be NULL.
 *  @param curve_segment_type Deref will hold the result, if successful.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC bool curve_segment_get_type(__in  __notnull curve_segment,
                                   __out __notnull curve_segment_type*);

/** Retrieves value from segment handler for a given curve segment.
 *
 *  @param curve_segment        Curve segment to use. Cannot be NULL.
 *  @param system_timeline_time Time to use for the query.
 *  @param bool                 True if the value should be forced for result @param system_variant.
 *  @param system_variant       Variant to use for storing the result.
 *
 *  @return true if successful and result avlue has been saved in @param system_variant, false otherwise.
 **/ 
PUBLIC bool curve_segment_get_value(__in __notnull  curve_segment,
                                    __in            system_timeline_time,
                                    __in            bool,
                                    __out __notnull system_variant);

/** TODO */
PUBLIC EMERALD_API bool curve_segment_modify_node_property(__in __notnull curve_segment,
                                                           __in __notnull curve_segment_node_id,
                                                           __in __notnull curve_segment_node_property,
                                                           __in __notnull system_variant);

/** Modifies curve segment node's time.
 *
 *  @param curve_segment          Curve segment to modify the node for. Cannot be NULL.
 *  @param curve_segement_node_id Id of node to move.
 *  @param system_timeline_time   New node time.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_segment_modify_node_time(__in __notnull curve_segment,
                                                       __in           curve_segment_node_id,
                                                       __in           system_timeline_time);

/** Modifies curve segment node's time and value.
 *
 *  @param curve_segment         Curve segment to modify the node for. Cannot be NULL.
 *  @param curve_segment_node_id Id of node to modify.
 *  @param system_timeline_time  New node time.
 *  @param system_variant        New node value. Cannot be NULL.
 *  @param bool                  True if variant set operation for curve node's value should be forced.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_segment_modify_node_time_value(__in __notnull curve_segment,
                                                             __in           curve_segment_node_id,
                                                             __in           system_timeline_time,
                                                             __in __notnull system_variant,
                                                             __in           bool);

/** Releases an initialized curve segment. 
 *
 *  @param curve_segment Curve segment to deinitialize. Cannot be null. Do not use after calling this function.
 **/
PUBLIC void curve_segment_release(__notnull __deallocate(mem) curve_segment);

/** TODO */
PUBLIC void curve_segment_set_on_segment_changed_callback(__in __notnull   curve_segment                 segment,
                                                          __in __maybenull PFNCURVESEGMENTONCURVECHANGED callback_proc,
                                                          __in __maybenull void*                         user_arg);

#endif /* CURVE_SEGMENT_H */
