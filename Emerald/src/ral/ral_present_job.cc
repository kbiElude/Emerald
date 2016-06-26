/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "system/system_dag.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

typedef struct _ral_present_job_connection
{
    ral_present_task_id dst_task_id;
    uint32_t            n_dst_task_input;
    uint32_t            n_src_task_output;
    ral_present_task_id src_task_id;

    explicit _ral_present_job_connection(ral_present_task_id in_dst_task_id,
                                         uint32_t            in_n_dst_task_input,
                                         uint32_t            in_n_src_task_output,
                                         ral_present_task_id in_src_task_id)
    {
        dst_task_id       = in_dst_task_id;
        n_dst_task_input  = in_n_dst_task_input;
        n_src_task_output = in_n_src_task_output;
        src_task_id       = in_src_task_id;
    }

} _ral_present_job_connection;

typedef struct _ral_present_job_task
{
    system_dag_node  dag_node;
    ral_present_task task; 

    explicit _ral_present_job_task(system_dag_node  in_dag_node,
                                   ral_present_task in_task)
    {
        dag_node = in_dag_node;
        task     = in_task;
    }

    ~_ral_present_job_task()
    {
        if (task != nullptr)
        {
            ral_present_task_release(task);

            task = nullptr;
        }
    }
} _ral_present_job_task;

typedef struct _ral_present_job
{
    system_hash64map connections; /* holds & owns _ral_present_job_connection instances */
    system_hash64map tasks;       /* holds & owns _ral_present_job_task instances */

    system_dag              dag;
    system_resizable_vector dag_sorted_nodes;

    bool                presentable_output_defined;
    uint32_t            presentable_output_id;
    ral_present_task_id presentable_output_task_id;

    _ral_present_job()
    {
        connections                = system_hash64map_create       (sizeof(_ral_present_job_connection*) );
        dag                        = system_dag_create             ();
        dag_sorted_nodes           = system_resizable_vector_create(16);
        tasks                      = system_hash64map_create       (sizeof(_ral_present_job_task*) );
        presentable_output_defined = false;
        presentable_output_id      = ~0;
        presentable_output_task_id = ~0;
    }

    ~_ral_present_job()
    {
        if (connections != nullptr)
        {
            uint32_t n_connections = 0;

            system_hash64map_get_property(connections,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_connections);

            for (uint32_t n_connection = 0;
                          n_connection < n_connections;
                        ++n_connection)
            {
                _ral_present_job_connection* connection_ptr = nullptr;

                system_hash64map_get_element_at(connections,
                                                n_connection,
                                               &connection_ptr,
                                                nullptr); /* result_hash_ptr */

                if (connection_ptr != nullptr)
                {
                    delete connection_ptr;
                }
            }

            system_hash64map_release(connections);
            connections = nullptr;
        }

        if (dag != nullptr)
        {
            system_dag_release(dag);

            dag = nullptr;
        }

        if (dag_sorted_nodes != nullptr)
        {
            system_resizable_vector_release(dag_sorted_nodes);

            dag_sorted_nodes = nullptr;
        }

        if (tasks != nullptr)
        {
            uint32_t n_tasks = 0;

            system_hash64map_get_property(tasks,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_tasks);

            for (uint32_t n_task = 0;
                          n_task < n_tasks;
                        ++n_task)
            {
                _ral_present_job_task* task_ptr = nullptr;

                system_hash64map_get_element_at(tasks,
                                                n_task,
                                               &task_ptr,
                                                nullptr); /* result_hash_ptr */

                if (task_ptr != nullptr)
                {
                    delete task_ptr;

                    task_ptr = nullptr;
                }
            }

            system_hash64map_release(tasks);
            tasks = nullptr;
        }
    }

} _ral_present_job;


/** Please see header for spec */
PUBLIC bool ral_present_job_add_task(ral_present_job      job,
                                     ral_present_task     task,
                                     ral_present_task_id* out_task_id_ptr)
{
    _ral_present_job*      job_ptr           = reinterpret_cast<_ral_present_job*>(job);
    system_dag_node        new_task_dag_node = nullptr;
    ral_present_task_id    new_task_id       = 0;
    _ral_present_job_task* new_task_ptr      = nullptr;
    bool                   result            = false;

    /* Sanity checks */
    if (job == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job == nullptr),
                          "Null ral_present_job instance specified");

        goto end;
    }

    if (task == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(task == nullptr),
                          "Null ral_present_task instance specified.");

        goto end;
    }

    /* Store the new task, simultaneously assigning it a new ID */
    new_task_dag_node = system_dag_add_node      (job_ptr->dag,
                                                  job_ptr);
    new_task_ptr      = new _ral_present_job_task(new_task_dag_node,
                                                  task);

    ASSERT_ALWAYS_SYNC(new_task_ptr != nullptr,
                       "Out of memory");

    system_hash64map_get_property(job_ptr->tasks,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &new_task_id);

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(job_ptr->tasks,
                                                 new_task_id),
                      "Task ID [%d] is already occupied",
                      new_task_id);

    system_hash64map_insert(job_ptr->tasks,
                            static_cast<system_hash64>(new_task_id),
                            new_task_ptr,
                            nullptr,  /* on_removal_callback               */
                            nullptr); /* on_removal_callback_user_argument */

    ral_present_task_retain(task);

    /* All done */
    *out_task_id_ptr = new_task_id;
    result           = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC bool ral_present_job_connect_tasks(ral_present_job                job,
                                          ral_present_task_id            src_task_id,
                                          uint32_t                       n_src_task_output,
                                          ral_present_task_id            dst_task_id,
                                          uint32_t                       n_dst_task_input,
                                          ral_present_job_connection_id* out_connection_id_ptr)
{
    _ral_present_job_task*        dst_task_ptr         = nullptr;
    ral_context_object_type       dst_task_input_type;
    _ral_present_job*             job_ptr              = reinterpret_cast<_ral_present_job*>(job);
    ral_present_job_connection_id new_connection_id;
    _ral_present_job_connection*  new_connection_ptr   = nullptr;
    bool                          result               = false;
    _ral_present_job_task*        src_task_ptr         = nullptr;
    ral_context_object_type       src_task_output_type;

    /* Sanity checks */
    if (job_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job_ptr == nullptr),
                          "Input ral_present_job instance is null");

        goto end;
    }

    if (!system_hash64map_get(job_ptr->tasks,
                              src_task_id,
                             &src_task_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "The specified job does not contain a task with ID [%d] (to be used as a source for a new connection)",
                          src_task_id);

        goto end;
    }

    if (!system_hash64map_get(job_ptr->tasks,
                              dst_task_id,
                             &dst_task_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "The specified job does not contain a task with ID [%d] (to be used as a destination for a new connection)",
                          dst_task_id);

        goto end;
    }

    ral_present_task_get_io_property(src_task_ptr->task,
                                     RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                     n_src_task_output,
                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                     (void**) &src_task_output_type);
    ral_present_task_get_io_property(dst_task_ptr->task,
                                     RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                     n_dst_task_input,
                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                     (void**) &dst_task_input_type);

    if (dst_task_input_type != src_task_output_type)
    {
        ASSERT_DEBUG_SYNC(!(dst_task_input_type != src_task_output_type),
                          "Destination RAL present task output's type does not match RAL present task input's type");

        goto end;
    }

    /* Create & store the new connection */
    system_hash64map_get_property(job_ptr->connections,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &new_connection_id);

    new_connection_ptr = new _ral_present_job_connection(dst_task_id,
                                                         n_dst_task_input,
                                                         n_src_task_output,
                                                         src_task_id);

    ASSERT_ALWAYS_SYNC(new_connection_ptr != nullptr,
                       "Out of memory");

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(job_ptr->connections,
                                                 new_connection_id),
                      "Connection ID [%d] is already occupied",
                      new_connection_id);

    system_hash64map_insert(job_ptr->connections,
                            new_connection_id,
                            new_connection_ptr,
                            nullptr,  /* on_remove_callback          */
                            nullptr); /* on_remove_callback_user_arg */

    /* Update the DAG */
    system_dag_add_connection(job_ptr->dag,
                              src_task_ptr->dag_node,
                              dst_task_ptr->dag_node);

    /* All done */
    *out_connection_id_ptr = new_connection_id;
    result                 = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC ral_present_job ral_present_job_create()
{
    _ral_present_job* new_job_ptr = nullptr;

    /* Sanity checks */

    /* Spawn the new descriptor */
    new_job_ptr = new _ral_present_job;

    ASSERT_ALWAYS_SYNC(new_job_ptr != nullptr,
                       "Out of memory");

    return (ral_present_job) new_job_ptr;
}

/** Please see header for spec */
PUBLIC bool ral_present_job_get_connection_id_at_index(ral_present_job                job,
                                                       uint32_t                       n_connection,
                                                       ral_present_job_connection_id* out_result_ptr)
{
    _ral_present_job* job_ptr               = reinterpret_cast<_ral_present_job*>(job);
    uint32_t          n_connections_defined = 0;
    bool              result                = false;

    /* Sanity checks */
    if (job_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job_ptr == nullptr),
                          "Input ral_present_job instance is null");

        goto end;
    }

    /* NOTE: At the moment connections cannot be deleted, once created, so we can go with a simple
     *       solution here.
     */
    system_hash64map_get_property(job_ptr->connections,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_connections_defined);

    if (n_connection >= n_connections_defined)
    {
        /* Not enough connections defined */
        goto end;
    }

    *out_result_ptr = n_connection;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC bool ral_present_job_get_connection_property(ral_present_job                     job,
                                                    ral_present_job_connection_id       connection_id,
                                                    ral_present_job_connection_property property,
                                                    void*                               out_result_ptr)
{
    _ral_present_job_connection* connection_ptr = nullptr;
    _ral_present_job*            job_ptr        = reinterpret_cast<_ral_present_job*>(job);
    bool                         result         = false;

    /* Sanity checks */
    if (job_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job_ptr == nullptr),
            "Input ral_present_job instance is null");

        goto end;
    }

    if (!system_hash64map_get(job_ptr->connections,
                              static_cast<system_hash64>(connection_id),
                             &connection_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid connection ID specified");

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_ID:
        {
            *((ral_present_task_id*) out_result_ptr) = connection_ptr->dst_task_id;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_INPUT_INDEX:
        {
            *((uint32_t*) out_result_ptr) = connection_ptr->n_dst_task_input;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_ID:
        {
            *((ral_present_task_id*)out_result_ptr) = connection_ptr->src_task_id;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_OUTPUT_INDEX:
        {
            *((uint32_t*)out_result_ptr) = connection_ptr->n_src_task_output;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid ral_present_job_connection_property value specified.");

            goto end;
        }
    }

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC void ral_present_job_get_property(ral_present_job          job,
                                         ral_present_job_property property,
                                         void*                    out_result_ptr)
{
    _ral_present_job* job_ptr = reinterpret_cast<_ral_present_job*>(job);

    /* Sanity checks */
    if (job_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job_ptr == nullptr),
                          "Specified ral_present_job instance is null.");

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED:
        {
            *(bool*) out_result_ptr = job_ptr->presentable_output_defined;

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID:
        {
            ASSERT_DEBUG_SYNC(job_ptr->presentable_output_defined,
                             "RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID property value queried, even though presentable output was not defined.");

            *(ral_present_task_id*) out_result_ptr = job_ptr->presentable_output_task_id;

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_OUTPUT_INDEX:
        {
            ASSERT_DEBUG_SYNC(job_ptr->presentable_output_defined,
                             "RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID property value queried, even though presentable output was not defined.");

            *(uint32_t*) out_result_ptr = job_ptr->presentable_output_id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid ral_present_job_proprerty value specified.");
        }
    }
end:
    ;
}

/** Please see header for spec */
PUBLIC bool ral_present_job_get_sorted_tasks(ral_present_job    present_job,
                                             uint32_t*          out_n_tasks,
                                             ral_present_task** out_tasks)
{
    _ral_present_job* present_job_ptr = reinterpret_cast<_ral_present_job*>(present_job);
    bool              result          = false;

    if (system_dag_is_dirty(present_job_ptr->dag) )
    {
        result = system_dag_get_topologically_sorted_node_values(present_job_ptr->dag,
                                                                 present_job_ptr->dag_sorted_nodes);
    }
    else
    {
        result = true;
    }

    if (result)
    {
        system_resizable_vector_get_property(present_job_ptr->dag_sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             (void*) out_n_tasks);

        system_resizable_vector_get_property(present_job_ptr->dag_sorted_nodes,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_ARRAY,
                                             out_tasks);
    }

    return result;
}

/** Please see header for spec */
PUBLIC void ral_present_job_release(ral_present_job job)
{
    delete (_ral_present_job*) job;
}

/** Please see header for spec */
PUBLIC bool ral_present_job_set_presentable_output(ral_present_job     job,
                                                   ral_present_task_id task_id,
                                                   uint32_t            n_output)
{
    _ral_present_job*       job_ptr          = reinterpret_cast<_ral_present_job*>(job);
    bool                    result           = false;
    ral_context_object_type task_output_type;
    _ral_present_job_task*  task_ptr         = nullptr;

    /* Sanity checks */
    if (job_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(job_ptr == nullptr),
                          "Input ral_present_job instance is NULL");

        goto end;
    }

    if (!system_hash64map_get(job_ptr->tasks,
                              task_id,
                             &task_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No RAL present task with ID [%d] found",
                          task_id);

        goto end;
    }

    if (!ral_present_task_get_io_property(task_ptr->task,
                                          RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                          n_output,
                                          RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                          (void**) &task_output_type) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve property value of a present task output (id:[%d] output:[%d])",
                          task_id,
                          n_output);

        goto end;
    }

    if (task_output_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE       &&
        task_output_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW)
    {
        ASSERT_DEBUG_SYNC(!(task_output_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE       &&
                            task_output_type != RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW),
                          "Specified task output is neither a texture, nor a texture view.");

        goto end;
    }

    /* Store the specified presentable output */
    job_ptr->presentable_output_defined = true;
    job_ptr->presentable_output_id      = n_output;
    job_ptr->presentable_output_task_id = task_id;

    /* All done */
    result = true;

end:
    return result;
}
