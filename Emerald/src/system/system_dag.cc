/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "system/system_dag.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Private type definitions */
typedef struct
{
    system_dag_node dst;
    system_dag_node src;
} _system_dag_connection;

typedef enum
{
    STATUS_UNVISITED,
    STATUS_VISITED,
    STATUS_PROCESSED
} _system_dag_node_status;

typedef struct _system_dag_node
{
    system_dag_node_value value;

    /* Following properties are used only by solve() and are unavailable
     * to users of the module: */
    system_resizable_vector adjacent_nodes;
    _system_dag_node_status status;
    unsigned int            time_processed;

    ~_system_dag_node()
    {
        if (adjacent_nodes != NULL)
        {
            system_resizable_vector_release(adjacent_nodes);

            adjacent_nodes = NULL;
        }
    }
} _system_dag_node;

typedef struct
{
    system_resizable_vector connections;
    system_resizable_vector nodes;

    bool dirty;

    system_resizable_vector sorted_nodes;

    /* Following properties are used only by solve() and are unavailable
     * to users of the module */
    system_resizable_vector nodes_to_process;
} _system_dag;


/** TODO */
PRIVATE bool _system_dag_process_node(_system_dag_node*       node_ptr,
                                      unsigned int*           time_ptr,
                                      system_resizable_vector processed_nodes_vector)
    {
        _system_dag_node* adjacent_node_ptr = NULL;
        unsigned int      n_adjacent_nodes  = 0;
        bool              result            = true;

        system_resizable_vector_get_property(node_ptr->adjacent_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_adjacent_nodes);

        node_ptr->status = STATUS_VISITED;
        (*time_ptr)      ++;

        for (unsigned int n_adjacent_node = 0;
                          n_adjacent_node < n_adjacent_nodes;
                        ++n_adjacent_node)
        {
            if (system_resizable_vector_get_element_at(node_ptr->adjacent_nodes,
                                                       n_adjacent_node,
                                                      &adjacent_node_ptr) )
            {
                if (adjacent_node_ptr->status == STATUS_UNVISITED)
                {
                    result = _system_dag_process_node(adjacent_node_ptr,
                                                      time_ptr,
                                                      processed_nodes_vector);

                    if (!result)
                    {
                        goto end;
                    }
                }
                else
                if (adjacent_node_ptr->status == STATUS_VISITED)
                {
                    /* This is a returning edge indicating that the graph we're operating on
                     * ain't a DAG!
                     */
                    ASSERT_DEBUG_SYNC(false,
                                      "system_dag operating on a graph that's not a DAG");

                    result = false;
                    goto end;
                }
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve adjacent node at index [%d]",
                                  n_adjacent_node);
            }
        } /* for (all adjacent nodes) */

        (*time_ptr)++;

        node_ptr->status         = STATUS_PROCESSED;
        node_ptr->time_processed = *time_ptr;

        system_resizable_vector_insert_element_at(processed_nodes_vector,
                                                  0,
                                                  node_ptr);

end:
        return result;
    }

/** Please see header for specification */
PUBLIC EMERALD_API system_dag_connection system_dag_add_connection(system_dag      dag,
                                                                   system_dag_node src,
                                                                   system_dag_node dst)
{
    _system_dag* dag_ptr = (_system_dag*) dag;

    /* Create new descriptor */
    _system_dag_connection* new_connection = new (std::nothrow) _system_dag_connection;

    if (new_connection == NULL)
    {
        ASSERT_ALWAYS_SYNC(0,
                           "Out of memory");

        goto end;
    }

    new_connection->dst = dst;
    new_connection->src = src;

    /* Associate the connection with DAG */
    system_resizable_vector_push(dag_ptr->connections,
                                 new_connection);

    dag_ptr->dirty = true;
end:
    return new_connection;
}

/** TODO */
PUBLIC EMERALD_API system_dag_node system_dag_add_node(system_dag            dag,
                                                       system_dag_node_value value)
{
    _system_dag* dag_ptr = (_system_dag*) dag;

    /* Create new descriptor */
    _system_dag_node* new_node = new (std::nothrow) _system_dag_node;

    if (new_node == NULL)
    {
        ASSERT_ALWAYS_SYNC(0, 
                           "Out of memory");

        goto end;
    }

    new_node->adjacent_nodes   = system_resizable_vector_create(4 /* capacity */);
    new_node->status           = STATUS_UNVISITED;
    new_node->time_processed   = 0;
    new_node->value            = value;

    /* Associate the node with DAG */
    system_resizable_vector_push(dag_ptr->nodes,
                                 new_node);

    dag_ptr->dirty = true;

    /* Done */
end:
    return new_node;
}

/** TODO */
PUBLIC EMERALD_API system_dag system_dag_create()
{
    _system_dag* result = new (std::nothrow) _system_dag;

    if (result == NULL)
    {
        ASSERT_ALWAYS_SYNC(0,
                           "Out of memory");

        goto end;
    }

    result->connections      = system_resizable_vector_create(4 /* capacity */);
    result->dirty            = false;
    result->nodes            = system_resizable_vector_create(4 /* capacity */);
    result->nodes_to_process = system_resizable_vector_create(4 /* capacity */);
    result->sorted_nodes     = system_resizable_vector_create(4 /* capacity */);

end:
    return (system_dag) result;
}

/** TODO */
PUBLIC EMERALD_API bool system_dag_get_topologically_sorted_node_values(system_dag              dag,
                                                                        system_resizable_vector result)
{
    _system_dag* dag_ptr        = (_system_dag*) dag;
    unsigned int n_result_nodes = 0;
    bool         result_bool    = false;

    system_resizable_vector_get_property(dag_ptr->sorted_nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_result_nodes);

    if (dag_ptr->dirty)
    {
        if (!system_dag_solve(dag) )
        {
            LOG_FATAL("Cannot retrieve topologically sorted node values - graph is not a DAG");

            goto end;
        }
    }

    /* Copy the sorted nodes to the result vector */
    system_resizable_vector_empty(result);

    for (unsigned int n_result_node = 0;
                      n_result_node < n_result_nodes;
                    ++n_result_node)
    {
        system_dag_node node = NULL;

        if (system_resizable_vector_get_element_at(dag_ptr->sorted_nodes,
                                                   n_result_node,
                                                  &node) )
        {
            _system_dag_node* node_ptr = (_system_dag_node*) node;

            system_resizable_vector_push(result,
                                         node_ptr->value);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not get sorted node at index [%d]",
                              n_result_node);

            goto end;
        }
    } /* for (all result nodes) */

    /* Done */
    result_bool = true;

end:
    return result_bool;
}

/** TODO */
PUBLIC EMERALD_API void system_dag_release(system_dag dag)
{
    _system_dag* dag_ptr = (_system_dag*) dag;

    if (dag_ptr->connections != NULL)
    {
        _system_dag_connection* connection_ptr = NULL;

        while (system_resizable_vector_pop(dag_ptr->connections,
                                          &connection_ptr) )
        {
            delete connection_ptr;

            connection_ptr = NULL;
        }

        system_resizable_vector_release(dag_ptr->connections);
        dag_ptr->connections = NULL;
    }

    if (dag_ptr->nodes != NULL)
    {
        _system_dag_node* node_ptr = NULL;

        while (!system_resizable_vector_pop(dag_ptr->nodes,
                                           &node_ptr) )
        {
            delete node_ptr;

            node_ptr = NULL;
        }

        system_resizable_vector_release(dag_ptr->nodes);
        dag_ptr->nodes = NULL;
    }

    if (dag_ptr->nodes_to_process != NULL)
    {
        system_resizable_vector_release(dag_ptr->nodes_to_process);

        dag_ptr->nodes_to_process = NULL;
    }

    if (dag_ptr->sorted_nodes != NULL)
    {
        system_resizable_vector_release(dag_ptr->sorted_nodes);

        dag_ptr->sorted_nodes = NULL;
    }
}

/** TODO */
PUBLIC EMERALD_API void system_dag_reset_connections(system_dag dag)
{
    _system_dag*            dag_ptr        = (_system_dag*) dag;
    _system_dag_connection* connection_ptr = NULL;

    while (!system_resizable_vector_pop(dag_ptr->connections,
                                       &connection_ptr) )
    {
        delete connection_ptr;

        connection_ptr = NULL;
    }
}

/** TODO */
PUBLIC EMERALD_API bool system_dag_solve(system_dag dag)
{
    _system_dag_node* current_node_ptr = NULL;
    _system_dag*      dag_ptr          = (_system_dag*) dag;
    unsigned int      n_connections    = 0;
    unsigned int      n_nodes          = 0;
    bool              result           = false;
    unsigned int      time             = 0;

    system_resizable_vector_get_property(dag_ptr->connections,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_connections);
    system_resizable_vector_get_property(dag_ptr->nodes,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_nodes);

    /* This is the man-on-the-street implementation of topological sorting of DAG nodes.
     *
     * 1. Reset adjacent node storage for all nodes. Also, mark all nodes as not-visited.
     */
    for (unsigned int n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
    {
        _system_dag_node* node_ptr = NULL;

        if (system_resizable_vector_get_element_at(dag_ptr->nodes,
                                                   n_node,
                                                  &node_ptr) )
        {
            system_resizable_vector_empty(node_ptr->adjacent_nodes);

            node_ptr->status = STATUS_UNVISITED;
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node at index [%d]",
                              n_node);

            goto end;
        }
    } /* for (all nodes) */

    /* 2. Fill adjacent node storage */
    for (unsigned int n_connection = 0;
                      n_connection < n_connections;
                    ++n_connection)
    {
        _system_dag_connection* connection_ptr = NULL;

        if (system_resizable_vector_get_element_at(dag_ptr->connections,
                                                   n_connection,
                                                  &connection_ptr) )
        {
            _system_dag_node* dst_node_ptr = (_system_dag_node*) connection_ptr->dst;
            _system_dag_node* src_node_ptr = (_system_dag_node*) connection_ptr->src;

            system_resizable_vector_push(src_node_ptr->adjacent_nodes,
                                         dst_node_ptr);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve connection at index [%d]",
                              n_connection);

            goto end;
        }
    } /* for (all connections) */

    /* 3. Store all nodes in a vector that we'll be modifying as we go */
    system_resizable_vector_empty(dag_ptr->nodes_to_process);

    for (unsigned int n_node = 0;
                      n_node < n_nodes;
                    ++n_node)
    {
        system_dag_node node = NULL;

        if (system_resizable_vector_get_element_at(dag_ptr->nodes,
                                                   n_node,
                                                  &node) )
        {
            system_resizable_vector_push(dag_ptr->nodes_to_process,
                                         node);
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve node at index [%d]",
                              n_node);

            goto end;
        }
    }

    /* 4. Execute the actual algorithm */
    result = true;

    while (system_resizable_vector_pop(dag_ptr->nodes_to_process,
                                      &current_node_ptr) )
    {
        if (current_node_ptr->status == STATUS_UNVISITED)
        {
            result = _system_dag_process_node(current_node_ptr,
                                             &time,
                                              dag_ptr->sorted_nodes);
        }

        if (!result)
        {
            goto end;
        }
    } /* while (there are nodes to process) */

    /* Done - mark the DAG as clean */
    dag_ptr->dirty = false;
    result         = true;

end:
    return result;
}
