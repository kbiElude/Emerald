/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef SYSTEM_DAG_H
#define SYSTEM_DAG_H

#include "system_types.h"


/** TODO */
PUBLIC EMERALD_API system_dag_connection system_dag_add_connection(system_dag      dag,
                                                                   system_dag_node src,
                                                                   system_dag_node dst);
/** TODO */
PUBLIC EMERALD_API system_dag_node system_dag_add_node(system_dag            dag,
                                                       system_dag_node_value value);

/** TODO */
PUBLIC EMERALD_API system_dag system_dag_create();

/** TODO */
PUBLIC EMERALD_API void system_dag_delete_connection(system_dag            dag,
                                                     system_dag_connection connection);

/** Deletes:
 *
 *  1) Outgoing connections whose source node is      @param src, if @param dst is NULL.
 *  2) Incoming connections whose destination node is @param dst, if @param src is NULL.
 *
 *  It is an error to specify both @param src and @param dst as non-NULL values or both
 *  as NULL values.
 *
 *  @param dag DAG instance.
 *  @param src DAG node, used as a source for the seeked connection(s). Must be NULL if @param
 *             dst is not NULL. Must not be NULL if @param dst is also NULL.
 *  @param dst DAG node, used as a destination for the seeked connection(s). Must be NULL if @param
 *             src is not NULL. Must not be NULL if @param src is also NULL.
 *
 *  @return true if at least one connection was found, whose source or destiation matched the
 *          specified node handles; false if input arguments were invalid or no such connection
 *          was identified.
 **/
PUBLIC EMERALD_API bool system_dag_delete_connections(system_dag      dag,
                                                      system_dag_node src,
                                                      system_dag_node dst);

/** Returns an array of defined connections, matching user-specified requirements.
 *
 *  @param dag DAG instance.
 *  @param src DAG node, used as a source for the seeked connection(s). Can be NULL (in which case
 *             any node will be accepted as a valid source), if @param dst is not NULL.
 *  @param dst DAG node, used as a destination for the seeked connection(s). Can be NULL (in
 *             which case any node will be accepted as a valid destination), if @param src is
 *             not NULL.
 *
 *  @return true if @param out_opt_n_connections_ptr and @param out_opt_connections_ptr were updated
 *          (whenever not NULL) successfully, false otherwise.
 */
PUBLIC EMERALD_API bool system_dag_get_connections(system_dag             dag,
                                                   system_dag_node        src,
                                                   system_dag_node        dst,
                                                   uint32_t*              out_opt_n_connections_ptr,
                                                   system_dag_connection* out_opt_connections_ptr);

/** TODO */
PUBLIC EMERALD_API bool system_dag_get_topologically_sorted_node_values(system_dag              dag,
                                                                        system_resizable_vector result);

/** Tells if a specified connection is defined for the DAG.
 *
 *  @param dag DAG instance.
 *  @param src DAG node, used as a source for the seeked connection. Can be NULL (in which case
 *             any node will be accepted as a valid source), if @param dst is not NULL.
 *  @param dst DAG node, used as a destination for the seeked connection. Can be NULL (in
 *             which case any node will be accepted as a valid destination), if @param src is
 *             not NULL.
 *
 *  @return true if a connection matching the specified requirements was found, false
 *          otherwise.
 */
PUBLIC EMERALD_API bool system_dag_is_connection_defined(system_dag      dag,
                                                         system_dag_node src,
                                                         system_dag_node dst);

/** TODO */
PUBLIC EMERALD_API bool system_dag_is_dirty(system_dag dag);

/** TODO */
PUBLIC EMERALD_API void system_dag_release(system_dag dag);

/** TODO */
PUBLIC EMERALD_API void system_dag_reset_connections(system_dag dag);

/** TODO */
PUBLIC EMERALD_API bool system_dag_solve(system_dag dag);

#endif /* SYSTEM_DAG_H */
