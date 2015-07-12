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
PUBLIC EMERALD_API bool system_dag_get_topologically_sorted_node_values(system_dag              dag,
                                                                        system_resizable_vector result);

/** TODO */
PUBLIC EMERALD_API void system_dag_release(system_dag dag);

/** TODO */
PUBLIC EMERALD_API void system_dag_reset_connections(system_dag dag);

/** TODO */
PUBLIC EMERALD_API bool system_dag_solve(system_dag dag);

#endif /* SYSTEM_DAG_H */
