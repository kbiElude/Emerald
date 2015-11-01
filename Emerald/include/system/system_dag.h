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
PUBLIC EMERALD_API void system_dag_release(system_dag dag);

/** TODO */
PUBLIC EMERALD_API void system_dag_reset_connections(system_dag dag);

/** TODO */
PUBLIC EMERALD_API bool system_dag_solve(system_dag dag);

#endif /* SYSTEM_DAG_H */
