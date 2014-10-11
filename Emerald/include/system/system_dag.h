/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#ifndef SYSTEM_DAG_H
#define SYSTEM_DAG_H

#include "dll_exports.h"
#include "system_types.h"


/** TODO */
PUBLIC EMERALD_API system_dag_connection system_dag_add_connection(__in __notnull system_dag      dag,
                                                                   __in __notnull system_dag_node src,
                                                                   __in __notnull system_dag_node dst);
/** TODO */
PUBLIC EMERALD_API system_dag_node system_dag_add_node(__in __notnull system_dag            dag,
                                                       __in           system_dag_node_value value);

/** TODO */
PUBLIC EMERALD_API system_dag system_dag_create();

/** TODO */
PUBLIC EMERALD_API bool system_dag_get_topologically_sorted_node_values(__in    __notnull system_dag              dag,
                                                                        __inout __notnull system_resizable_vector result);

/** TODO */
PUBLIC EMERALD_API void system_dag_release(__in __notnull system_dag dag);

/** TODO */
PUBLIC EMERALD_API void system_dag_reset_connections(__in __notnull system_dag dag);

/** TODO */
PUBLIC EMERALD_API bool system_dag_solve(__in __notnull system_dag dag);

#endif /* SYSTEM_DAG_H */
