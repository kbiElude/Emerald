/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 */
#ifndef CURVE_CONTAINER_H
#define CURVE_CONTAINER_H

#include "curve/curve_types.h"
#include "system/system_types.h"


REFCOUNT_INSERT_DECLARATIONS(curve_container, curve_container)

/* Adds a new non-tcb node to a curve segment. This function will work with static/lerp segments only.
 *
 * @param curve_container        Curve container to modify. Cannot be NULL.
 * @param curve_segment_id       Id of curve segment to modify the node in.
 * @param system_time            Node time to use.
 * @param system_variant         Value to use for the node.
 * @param curve_segment_node_id* Deref will store id of the new node, if function finishes successfully. Cannot be NULL.
 *
 * @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_add_general_node(curve_container        curve,
                                                         curve_segment_id       segment_id,
                                                         system_time            node_time,
                                                         system_variant         node_value,
                                                         curve_segment_node_id* out_node_id_ptr);

/* Adds a new tcb node to a curve segment. This function will work with tcb segments only.
 *
 * @param curve_container        Curve container to modify. Cannot be NULL.
 * @param curve_segment_id       Id of curve segment to modify the node in.
 * @param system_time            Node time to use.
 * @param system_variant         Value to use for the node.
 * @param float                  Tension to use for the node.
 * @param float                  Continuity to use for the node.
 * @param float                  Bias to use for the node.
 * @param curve_segment_node_id* Deref will store id of the new node, if function finishes successfully. Cannot be NULL.
 *
 * @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_add_tcb_node(curve_container        curve,
                                                     curve_segment_id       segment_id,
                                                     system_time            node_time,
                                                     system_variant         node_value,
                                                     float                  node_tension,
                                                     float                  node_continuity,
                                                     float                  node_bias,
                                                     curve_segment_node_id* out_node_id_ptr);

/* Adds a new LERP value segment to a given curve container's dimension. The segment will linearly interpolate
 * between start value for start time and end value for given end time.
 * Note that the segment is not allowed to overlap with existing segments for a given container's dimension.
 *
 * @param curve_container  Curve container to use. Cannot be NULL.
 * @param system_time      Start time of the new segment.
 * @param system_time      End time of the new segment.
 * @param system_variant   Value to be used for start time of the segment. Cannot be NULL.
 * @param system_variant   Value to be sued for end time of the segment. Cannot be NULL.
 * @param curve_segment_id Deref will be used to store id of the new segment, if the function succeeds.
 *                         Cannot be NULL.
 *
 * @return True if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_add_lerp_segment(curve_container   curve,
                                                         system_time       start_time,
                                                         system_time       end_time,
                                                         system_variant    value_start,
                                                         system_variant    value_end,
                                                         curve_segment_id* out_segment_id_ptr);

/* Adds a new static value segment to a given curve container's dimension. Note that the segment is not allowed
 * to overlap with existing segments for a given container's dimension.
 *
 * @param curve_container  Curve container to use. Cannot be NULL.
 * @param system_time      Start time of the new segment.
 * @param system_time      End time of the new segment.
 * @param system_variant   Value to be used for the segment. Cannot be NULL.
 * @param curve_segment_id Deref will be used to store id of the new segment, if the function succeeds.
 *                         Cannot be NULL.
 *
 * @return True if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_add_static_value_segment(curve_container   curve,
                                                                 system_time       start_time,
                                                                 system_time       end_time,
                                                                 system_variant    value,
                                                                 curve_segment_id* out_segment_id_ptr);

/* TODO */
PUBLIC EMERALD_API bool curve_container_add_tcb_segment(curve_container   curve,
                                                        system_time       start_time,
                                                        system_time       end_time,
                                                        system_variant    start_value,
                                                        float             start_tension,
                                                        float             start_continuity,
                                                        float             start_bias,
                                                        system_variant    end_value,
                                                        float             end_tension,
                                                        float             end_continuity,
                                                        float             end_bias,
                                                        curve_segment_id* out_segment_id_ptr);

/** Creates a curve container. Curve type must be defined at creation time and
 *  cannot be changed later.
 *
 *  @param system_hashed_ansi_string Name of the curve.
 *  @param object_manager_path       TODO
 *  @param data_type                 TODO
 *
 *  @return Returns a new curve container.
 */
PUBLIC EMERALD_API curve_container curve_container_create(system_hashed_ansi_string name,
                                                          system_hashed_ansi_string object_manager_path,
                                                          system_variant_type       data_type);

/** Deletes an existing node from a curve segment.
 *
 *  @param curve_container       Curve container to modify. Cannot be NULL.
 *  @param curve_segment_id      Curve segment id.
 *  @param curve_segment_node_id Node id.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_delete_node(curve_container       curve,
                                                    curve_segment_id      segment_id,
                                                    curve_segment_node_id node_id);

/** Deletes an existing curve segment from a curve container's dimension.
 *
 *  @param curve_container  Curve container to modify. Cannot be NULL.
 *  @param curve_segment_id Id of curve segment to delete
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_delete_segment(curve_container  curve,
                                                       curve_segment_id segment_id);

/** Retrieves default value used for a given curve container's dimension.
 *
 *  @param curve_container Curve container to query. Cannot be NULL.
 *  @param bool            True if the reported value should be converted to user-provided variant type (slower!).
 *                         False to report an assertion failure in debug builds.
 *  @param system_variant  Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_default_value(curve_container curve,
                                                          bool            should_force,
                                                          system_variant  result_variant);

/** Retrieves general curve segment node data. Works for nodes created for static/lerp segments only.
 *
 *  @param curve_container       Curve container to query. Cannot be NULL.
 *  @param curve_segment_id      Curve segment id.
 *  @param curve_segment_node_id Curve node id.
 *  @param system_time*          Deref will be used to store node time. Cannot be NULL.
 *  @param system_variant        Variant will be set to node value. Variant must use float values. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_general_node_data(curve_container       curve,
                                                              curve_segment_id      segment_id,
                                                              curve_segment_node_id node_id,
                                                              system_time*          node_time,
                                                              system_variant        node_value);

/** Retrieves id of a node with given index, looking from the beginning of the curve segment.
 *
 *  @param curve_container        Curve container to query. Cannot be NULL.
 *  @param curve_segment_id       Curve segment id.
 *  @param uint32_t               Node index.
 *  @param curve_segment_node_id* Deref will be used to store the result. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_node_id_for_node_at(curve_container        curve,
                                                                curve_segment_id       segment_id,
                                                                uint32_t               node_index,
                                                                curve_segment_node_id* out_node_id_ptr);

/** TODO */
PUBLIC EMERALD_API void curve_container_get_property(curve_container          curve,
                                                     curve_container_property curve_property,
                                                     void*                    out_result);

/** TODO */
PUBLIC EMERALD_API curve_segment curve_container_get_segment(curve_container  curve,
                                                             curve_segment_id segment_id);

/** Retrieves id of a segment with user-provided index, starting from 0:00:00 time.
 *
 *  @param curve_container   Curve container to query.Cannot be NULL.
 *  @param uint32_t          Index of segment to retrieve id for.
 *  @param curve_segment_id* Deref will be used to store the result. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_segment_id_for_nth_segment(curve_container   curve,
                                                                       uint32_t          segment_index,
                                                                       curve_segment_id* out_segment_id_ptr);

/** Retrieves id of a segment that is closest to a given time-point. Seek direction is governed by the caller.
 *
 *  @param bool              True if the caller wants the seeking to kick in.
 *  @param bool              True if the caller wants the seeking to work from future to past, false to use reversed direction.
 *  @param curve_container   Curve container to query. Cannot be NULL.
 *  @param system_time       Time to use.
 *  @param curve_segment_id  Id of a segment to ignore, if found.
 *  @param curve_segment_id* Deref will be used to store the result, if function finishes successfully. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_segment_id_relative_to_time(bool              allow_seeking,
                                                                        bool              seek_forward,
                                                                        curve_container   curve,
                                                                        system_time       time,
                                                                        curve_segment_id  segment_to_exclude_id,
                                                                        curve_segment_id* out_segment_id_ptr);

/** Retrieves general curve segment node data. Works for TCB nodes only.
 *
 *  @param curve_container             Curve container to query. Cannot be NULL.
 *  @param curve_segment_id            Curve segment id.
 *  @param curve_segment_node_id       Curve node id.
 *  @param curve_segment_node_property TODO
 *  @param system_variant              TODO.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_node_property(curve_container             curve,
                                                          curve_segment_id            segment_id,
                                                          curve_segment_node_id       node_id,
                                                          curve_segment_node_property node_property,
                                                          system_variant              result);

/** TODO */
PUBLIC EMERALD_API bool curve_container_get_segment_property(curve_container                  curve,
                                                             curve_segment_id                 segment_id,
                                                             curve_container_segment_property segment_property,
                                                             void*                            out_result);

/** Retrieves value for a given time point and dimension of the curve container.
 *
 *  @param curve_container Curve container to query. Cannot be NULL.
 *  @param system_time     Time to query the value for.
 *  @param bool            True if the reported value should be converted to user-provided variant type (slower!).
 *                         False to report an assertion failure in debug builds.
 *  @param system_variant  Variant to store the result in. Cannot be NULL.
 *
 *  @return true if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_get_value(curve_container curve,
                                                  system_time     time,
                                                  bool            should_force,
                                                  system_variant  result);

/* Tells whether two curve containers are equal.
 *
 * @param curve_container Curve container to use for reference. Cannot be NULL.
 * @param curve_container Curve container to compare with. Cannot be NULL.
 * 
 * @return True if equal, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_is_equal(curve_container curve_a,
                                                 curve_container curve_b);

/** TODO */
PUBLIC bool curve_container_is_range_defined(curve_container  curve,
                                             system_time      start_time,
                                             system_time      end_time,
                                             curve_segment_id segment_id);

/* Modify properties of a segment node.
 *
 * @param curve_container       Curve container to modify. Cannot be NULL.
 * @param curve_segment_id      Id of curve segment to modify the node in.
 * @param curve_segment_node_id Id of the node to modify.
 * @param system_time           New time to use for the node.
 * @param system_variant        New value to use for the node. Cannot be NULL.
 *
 * @return true if successful, false otherwise.
 */
PUBLIC EMERALD_API bool curve_container_modify_node(curve_container       curve,
                                                    curve_segment_id      segment_id,
                                                    curve_segment_node_id node_id,
                                                    system_time           new_node_time,
                                                    system_variant        new_node_value);

/* Sets a new defautl value for a specific dimension of user-provided curve container.
 *
 * @param curve_container Curve container to modify. Cannot be NULL.
 * @param system_variant  Value to use. Cannot be NULL.
 */
PUBLIC EMERALD_API bool curve_container_set_default_value(curve_container curve,
                                                          system_variant  new_default_value);

/** TODO */
PUBLIC EMERALD_API void curve_container_set_property(curve_container          curve,
                                                     curve_container_property curve_property,
                                                     const void*              value_ptr);

/** TODO */
PUBLIC EMERALD_API void curve_container_set_segment_property(curve_container                  curve,
                                                             curve_segment_id                 segment_id,
                                                             curve_container_segment_property segment_property,
                                                             void*                            value_ptr);

/* Changes times of a given segment of user-proivided curve container's dimension
 *
 * @param curve_container  Curve container to modify.
 * @param curve_segment_id Id of segment to modify.
 * @param system_time      New start time to use for the segment.
 * @param system_time      New end time to use to use for the segment.
 *
 * @return True if successful, false otherwise.
 **/
PUBLIC EMERALD_API bool curve_container_set_segment_times(curve_container  curve,
                                                          curve_segment_id segment_id,
                                                          system_time      start_time,
                                                          system_time      end_time);

#endif /* CURVE_CONTAINER_H */