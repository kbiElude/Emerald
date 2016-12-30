/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_present_job.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
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
    bool             has_been_flattened;
    uint32_t         id;
    ral_present_task task; 

    explicit _ral_present_job_task(system_dag_node  in_dag_node,
                                   ral_present_task in_task,
                                   uint32_t         in_id)
    {
        has_been_flattened = false;
        id                 = in_id;
        task               = in_task;

        ral_present_task_retain(task);
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
    uint32_t         n_total_connections_added;
    uint32_t         n_total_tasks_added;
    system_hash64map tasks;       /* holds & owns _ral_present_job_task instances */

    bool                     presentable_output_defined;
    uint32_t                 presentable_output_io_index;
    ral_present_task_io_type presentable_output_io_type;
    ral_present_task_id      presentable_output_task_id;

    _ral_present_job()
    {
        connections                 = system_hash64map_create(sizeof(_ral_present_job_connection*) );
        n_total_connections_added   = 0;
        n_total_tasks_added         = 0;
        tasks                       = system_hash64map_create(sizeof(_ral_present_job_task*) );
        presentable_output_defined  = false;
        presentable_output_io_index = ~0;
        presentable_output_io_type  = RAL_PRESENT_TASK_IO_TYPE_UNKNOWN;
        presentable_output_task_id  = ~0;
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
PUBLIC EMERALD_API bool ral_present_job_add_task(ral_present_job      job,
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

    /* Make sure the task has not already been added */
    #ifdef _DEBUG
    {
        for (uint32_t n_task = 0;
                      n_task < job_ptr->n_total_tasks_added;
                    ++n_task)
        {
            _ral_present_job_task* current_task_ptr     = nullptr;
            void*                  current_task_raw_ptr = nullptr;

            system_hash64map_get_element_at(job_ptr->tasks,
                                            n_task,
                                           &current_task_raw_ptr,
                                            nullptr); /* result_hash_ptr */

            current_task_ptr = reinterpret_cast<_ral_present_job_task*>(current_task_raw_ptr);

            ASSERT_DEBUG_SYNC(current_task_ptr->task != task,
                              "The specified ral_present_task instance has already been added to the ral_present_job instance.");
        }
    }
    #endif

    /* Store the new task, simultaneously assigning it a new ID */
    new_task_id = job_ptr->n_total_tasks_added++;

    new_task_ptr = new _ral_present_job_task(new_task_dag_node,
                                             task,
                                             new_task_id);

    ASSERT_ALWAYS_SYNC(new_task_ptr != nullptr,
                       "Out of memory");

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(job_ptr->tasks,
                                                 new_task_id),
                      "Task ID [%d] is already occupied",
                      new_task_id);

    system_hash64map_insert(job_ptr->tasks,
                            static_cast<system_hash64>(new_task_id),
                            new_task_ptr,
                            nullptr,  /* on_removal_callback               */
                            nullptr); /* on_removal_callback_user_argument */

    /* All done */
    *out_task_id_ptr = new_task_id;
    result           = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_connect_tasks(ral_present_job                job,
                                                      ral_present_task_id            src_task_id,
                                                      uint32_t                       n_src_task_output,
                                                      ral_present_task_id            dst_task_id,
                                                      uint32_t                       n_dst_task_input,
                                                      ral_present_job_connection_id* out_opt_connection_id_ptr)
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
                                     reinterpret_cast<void**>(&src_task_output_type) );
    ral_present_task_get_io_property(dst_task_ptr->task,
                                     RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                     n_dst_task_input,
                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                     reinterpret_cast<void**>(&dst_task_input_type) );

    if (dst_task_input_type != src_task_output_type)
    {
        ASSERT_DEBUG_SYNC(!(dst_task_input_type != src_task_output_type),
                          "Destination RAL present task output's type does not match RAL present task input's type");

        goto end;
    }

    #ifdef _DEBUG
    {
        /* Make sure the specified connection has not already been defined. Doing so will break
         * the flattening operation.
         */
        uint32_t n_connections_defined;

        system_hash64map_get_property(job_ptr->connections,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_connections_defined);

        for (uint32_t n_connection = 0;
                      n_connection < n_connections_defined;
                    ++n_connection)
        {
            _ral_present_job_connection* connection_ptr = nullptr;

            system_hash64map_get_element_at(job_ptr->connections,
                                            n_connection,
                                           &connection_ptr,
                                            nullptr); /* result_hash_ptr */

            if (connection_ptr->dst_task_id       == dst_task_id       &&
                connection_ptr->n_dst_task_input  == n_dst_task_input  &&
                connection_ptr->n_src_task_output == n_src_task_output &&
                connection_ptr->src_task_id       == src_task_id)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Application is about to create a duplicate inter-node connection. This will cause flattening to fail.");

                break;
            }
        }
    }
    #endif

    /* Create & store the new connection */
    new_connection_id = job_ptr->n_total_connections_added++;

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

    /* All done */
    if (out_opt_connection_id_ptr != nullptr)
    {
        *out_opt_connection_id_ptr = new_connection_id;
    }

    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API ral_present_job ral_present_job_create()
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
PUBLIC void ral_present_job_dump(ral_present_job job)
{
    _ral_present_job* job_ptr       = reinterpret_cast<_ral_present_job*>(job);
    uint32_t          n_connections = 0;
    uint32_t          n_tasks       = 0;

    system_hash64map_get_property(job_ptr->connections,
                                   SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                  &n_connections);
    system_hash64map_get_property(job_ptr->tasks,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_tasks);

    LOG_RAW("Dump of ral_present_job [%p]:\n"
            "\n"
            "Number of tasks:       [%u]\n"
            "Number of connections: [%u]\n"
            "\n"
            "* Tasks:",
            job,
            n_tasks,
            n_connections);

    for (uint32_t n_task = 0;
                  n_task < n_tasks;
                ++n_task)
    {
        system_hashed_ansi_string task_name = nullptr;
        _ral_present_job_task*    task_ptr  = nullptr;
        ral_present_task_type     task_type;

        /* General properties */
        system_hash64map_get_element_at(job_ptr->tasks,
                                        n_task,
                                       &task_ptr,
                                        nullptr); /* result_hash_ptr */

        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_NAME,
                                     &task_name);
        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &task_type);
        LOG_RAW("** Task [ID: %u]:\n"
                "----------------\n"
                "Name: [%s]",
                task_ptr->id,
                system_hashed_ansi_string_get_buffer(task_name) );

        switch (task_type)
        {
            case RAL_PRESENT_TASK_TYPE_CPU_TASK: LOG_RAW("Type: CPU task\n");   break;
            case RAL_PRESENT_TASK_TYPE_GPU_TASK: LOG_RAW("Type: GPU task\n");   break;
            case RAL_PRESENT_TASK_TYPE_GROUP:    LOG_RAW("Type: Group task\n"); break;

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unknown RAL present task type");
            }
        }

        /* IOs */
        if (task_type == RAL_PRESENT_TASK_TYPE_GROUP)
        {
            uint32_t n_input_mappings  = 0;
            uint32_t n_output_mappings = 0;

            ral_present_task_get_property(task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_N_INPUT_MAPPINGS,
                                         &n_input_mappings);
            ral_present_task_get_property(task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_N_OUTPUT_MAPPINGS,
                                         &n_output_mappings);

            LOG_RAW("Number of input mappings:  [%u]\n"
                    "Number of output mappings: [%u]\n",
                    n_input_mappings,
                    n_output_mappings);

            for (uint32_t n_mapping_type = 0;
                          n_mapping_type < 2;
                        ++n_mapping_type)
            {
                const bool     is_input_mapping = (n_mapping_type == 0);
                const uint32_t n_mappings       = (is_input_mapping)    ? n_input_mappings : n_output_mappings;

                for (uint32_t n_mapping = 0;
                              n_mapping < n_mappings;
                            ++n_mapping)
                {
                    uint32_t group_task_io_index = -1;
                    uint32_t io_index            = -1;
                    uint32_t subtask_index       = -1;

                    ral_present_task_get_io_mapping_property(task_ptr->task,
                                                             is_input_mapping ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                              : RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                                             n_mapping,
                                                             RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_GROUP_TASK_IO_INDEX,
                                                             reinterpret_cast<void**>(&group_task_io_index) );
                    ral_present_task_get_io_mapping_property(task_ptr->task,
                                                             is_input_mapping ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                              : RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                                             n_mapping,
                                                             RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_IO_INDEX,
                                                             reinterpret_cast<void**>(&io_index) );
                    ral_present_task_get_io_mapping_property(task_ptr->task,
                                                             is_input_mapping ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                              : RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                                             n_mapping,
                                                             RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_SUBTASK_INDEX,
                                                             reinterpret_cast<void**>(&subtask_index) );

                    LOG_RAW("%s [%u]\n"
                            "===\n"
                            "Group task IO index: [%u]\n"
                            "IO index:            [%u]\n"
                            "Subtask index:       [%u]\n",
                            (is_input_mapping) ? "[Input mapping]" : "[Output mapping]",
                            n_mapping,
                            group_task_io_index,
                            io_index,
                            subtask_index);
                }
            }
        }
        else
        {
            uint32_t n_inputs  = 0;
            uint32_t n_outputs = 0;

            ral_present_task_get_property(task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_N_INPUTS,
                                         &n_inputs);
            ral_present_task_get_property(task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS,
                                         &n_outputs);

            LOG_RAW("Number of inputs:  [%u]\n"
                    "Number of outputs: [%u]\n",
                    n_inputs,
                    n_outputs);

            for (uint32_t n_io_type = 0;
                          n_io_type < 2;
                        ++n_io_type)
            {
                const ral_present_task_io_type io_type = (n_io_type == 0) ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                          : RAL_PRESENT_TASK_IO_TYPE_OUTPUT;
                const uint32_t                 n_ios   = (n_io_type == 0) ? n_inputs : n_outputs;

                for (uint32_t n_io = 0;
                              n_io < n_ios;
                            ++n_io)
                {
                    ral_context_object_type object_type;

                    ral_present_task_get_io_property(task_ptr->task,
                                                     io_type,
                                                     n_io,
                                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                                     reinterpret_cast<void**>(&object_type) );

                    LOG_RAW("%s [%u]:\n"
                            "===\n"
                            "Accepted object type: [%s]\n",
                            (n_io_type == 0) ? "Input" : "Output",
                            n_io,
                            system_hashed_ansi_string_get_buffer(ral_utils_get_ral_context_object_type_has(object_type) ));
                }
            }
        }
    }

    LOG_RAW("\n"
            "* Connections:\n");

    for (uint32_t n_connection = 0;
                  n_connection < n_connections;
                ++n_connection)
    {
        _ral_present_job_connection* connection_ptr = nullptr;

        system_hash64map_get_element_at(job_ptr->connections,
                                        n_connection,
                                       &connection_ptr,
                                        nullptr); /* result_hash_ptr */

        LOG_RAW("** Connection [%u]:\n"
                "==================\n"
                "Destination task ID:          [%u]\n"
                "Destination task input index: [%u]\n"
                "Source task ID:               [%u]\n"
                "Source task output index:     [%u]\n",
                n_connection,
                connection_ptr->dst_task_id,
                connection_ptr->n_dst_task_input,
                connection_ptr->src_task_id,
                connection_ptr->n_src_task_output);
    }
}

/** Please see header for spec */
PUBLIC bool ral_present_job_flatten(ral_present_job job)
{
    _ral_present_job* job_ptr = reinterpret_cast<_ral_present_job*>(job);
    uint32_t          n_tasks = 0;
    bool              result  = false;

    system_hash64map_get_property(job_ptr->tasks,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_tasks);

    for (int32_t n_task = 0;
                 n_task < static_cast<int32_t>(n_tasks);
               ++n_task)
    {
        /* First, identify a group task */
        struct subtask_data
        {
            ral_present_task    ingroup_subtask;
            ral_present_task_id new_job_task_id;
        } *subtasks = nullptr;

        uint32_t               n_connections         = 0;
        uint32_t               n_ingroup_connections = 0;
        uint32_t               n_subtasks            = 0;
        system_hash64          task_id               = -1;
        _ral_present_job_task* task_ptr              = nullptr;
        ral_present_task_type  task_type             = RAL_PRESENT_TASK_TYPE_UNKNOWN;

        if (!system_hash64map_get_element_at(job_ptr->tasks,
                                             n_task,
                                            &task_ptr,
                                            &task_id) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve task at index [%d]",
                              n_task);

            goto end;
        }

        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &task_type);

        if (task_type != RAL_PRESENT_TASK_TYPE_GROUP)
        {
            continue;
        }

        if (task_ptr->has_been_flattened)
        {
            continue;
        }

        /* A group task needs to be broken down into a group of subtasks with corresponding connections.
         * Furthermore, it may consist of group subtasks, so these will have to be taken care of as well.
         */
        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_N_GROUP_TASK_SUBTASKS,
                                     &n_subtasks);

        ASSERT_DEBUG_SYNC(n_subtasks != 0,
                          "A group task of 0 subtasks was encountered.");

        if (n_subtasks > 0)
        {
            subtasks = reinterpret_cast<subtask_data*>(_malloca(sizeof(*subtasks) * n_subtasks));

            for (uint32_t n_subtask = 0;
                          n_subtask < n_subtasks;
                        ++n_subtask)
            {
                ral_present_task_get_group_subtask(task_ptr->task,
                                                   n_subtask,
                                                  &subtasks[n_subtask].ingroup_subtask);

                ral_present_job_add_task(job,
                                         subtasks[n_subtask].ingroup_subtask,
                                        &subtasks[n_subtask].new_job_task_id);
            }
        }

        /* If presentable output is bound to one of the IOs of this task, reassign it accordingly */
        if (job_ptr->presentable_output_defined                                         &&
            task_id                             == job_ptr->presentable_output_task_id)
        {
            uint32_t presentable_subtask_index = -1;

            ral_present_task_get_io_mapping_property(task_ptr->task,
                                                     job_ptr->presentable_output_io_type,
                                                     job_ptr->presentable_output_io_index,
                                                     RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_SUBTASK_INDEX,
                                                     reinterpret_cast<void**>(&presentable_subtask_index) );

            ASSERT_DEBUG_SYNC(presentable_subtask_index < n_subtasks,
                              "Invalid presentable subtask index specified");

            job_ptr->presentable_output_task_id = subtasks[presentable_subtask_index].new_job_task_id;
        }

        /* Iterate over in-group connections and create corresponding entries on the job levle. */
        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_N_INGROUP_CONNECTIONS,
                                     &n_ingroup_connections);

        for (uint32_t n_connection = 0;
                      n_connection < n_ingroup_connections;
                    ++n_connection)
        {
            uint32_t input_present_task_index;
            uint32_t input_present_task_io_index;
            uint32_t output_present_task_index;
            uint32_t output_present_task_io_index;
            bool     result_getter;

            result_getter = ral_present_task_get_ingroup_connection(task_ptr->task,
                                                                    n_connection,
                                                                   &input_present_task_index,
                                                                   &input_present_task_io_index,
                                                                   &output_present_task_index,
                                                                   &output_present_task_io_index);

            ASSERT_DEBUG_SYNC(result_getter,
                              "ral_present_task_get_ingroup_connection() failed");
            ASSERT_DEBUG_SYNC(input_present_task_index  < n_subtasks &&
                              output_present_task_index < n_subtasks,
                              "Invalid IO referred to from an ingroup connection");

            ral_present_job_connect_tasks(job,
                                          subtasks[output_present_task_index].new_job_task_id,
                                          output_present_task_io_index,
                                          subtasks[input_present_task_index].new_job_task_id,
                                          input_present_task_io_index,
                                          nullptr); /* out_opt_connection_id_ptr */
        }

        /* Iterate over connections defined for the job. If any of these refers to the group node we're
         * currently working, either as a source or a destination, or both, the connections
         * need to be re-mapped to the newly created node's IOs. */
        system_hash64map_get_property(job_ptr->connections,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_connections);

        for (int32_t n_connection = 0;
                     n_connection < static_cast<int32_t>(n_connections);
                   ++n_connection)
        {
            system_hash64                connection_id  = -1;
            _ral_present_job_connection* connection_ptr = nullptr;

            if (!system_hash64map_get_element_at(job_ptr->connections,
                                                 n_connection,
                                                &connection_ptr,
                                                &connection_id)) /* result_hash_ptr */
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve data of connection at index [%d]",
                                  n_connection);

                goto end;
            }

            ASSERT_DEBUG_SYNC(connection_ptr->dst_task_id != connection_ptr->src_task_id,
                              "Loopback connections may not work correctly - needs to be verified");

            if (connection_ptr->dst_task_id == task_id ||
                connection_ptr->src_task_id == task_id)
            {
                for (uint32_t n_connection_type = 0;
                              n_connection_type < 2; /* src->this node ; this node->dst */
                            ++n_connection_type)
                {
                    const ral_present_task_io_type io_type                          = (n_connection_type == 0) ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                                                               : RAL_PRESENT_TASK_IO_TYPE_OUTPUT;
                    uint32_t                       n_group_task_io_mappings         = 0;
                    const uint32_t                 n_io                             = (n_connection_type == 0) ? connection_ptr->n_dst_task_input
                                                                                                               : connection_ptr->n_src_task_output;
                    ral_present_task_id            new_connection_dst_task_id       = -1;
                    uint32_t                       new_connection_dst_task_n_input  = -1;
                    ral_present_task_id            new_connection_src_task_id       = -1;
                    uint32_t                       new_connection_src_task_n_output = -1;
                    uint32_t                       subtask_index                    = -1;
                    uint32_t                       subtask_io_index                 = -1;

                    /* Do not process if current endpoint does not refer to current group node */
                    if (n_connection_type == 0 && connection_ptr->dst_task_id != task_id ||
                        n_connection_type == 1 && connection_ptr->src_task_id != task_id)
                    {
                        continue;
                    }

                    if (n_connection_type == 0)
                    {
                        new_connection_src_task_id       = connection_ptr->src_task_id;
                        new_connection_src_task_n_output = connection_ptr->n_src_task_output;
                    }
                    else
                    {
                        new_connection_dst_task_id      = connection_ptr->dst_task_id;
                        new_connection_dst_task_n_input = connection_ptr->n_dst_task_input;
                    }

                    /* Identify the mapping we need to use */
                    bool has_found_mapping = false;

                    ral_present_task_get_property(task_ptr->task,
                                                  (n_connection_type == 0) ? RAL_PRESENT_TASK_PROPERTY_N_INPUT_MAPPINGS
                                                                           : RAL_PRESENT_TASK_PROPERTY_N_OUTPUT_MAPPINGS,
                                                 &n_group_task_io_mappings);

                    for (uint32_t n_mapping = 0;
                                  n_mapping < n_group_task_io_mappings;
                                ++n_mapping)
                    {
                        uint32_t group_task_io_index = -1;

                        ral_present_task_get_io_mapping_property(task_ptr->task,
                                                                 io_type,
                                                                 n_mapping,
                                                                 RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_GROUP_TASK_IO_INDEX,
                                                                 reinterpret_cast<void**>(&group_task_io_index) );

                        if (group_task_io_index != n_io)
                        {
                            continue;
                        }

                        ral_present_task_get_io_mapping_property(task_ptr->task,
                                                                 io_type,
                                                                 n_mapping,
                                                                 RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_SUBTASK_INDEX,
                                                                 reinterpret_cast<void**>(&subtask_index) );
                        ral_present_task_get_io_mapping_property(task_ptr->task,
                                                                 io_type,
                                                                 n_mapping,
                                                                 RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_IO_INDEX,
                                                                 reinterpret_cast<void**>(&subtask_io_index) );

                        if (n_connection_type == 0)
                        {
                            new_connection_dst_task_id      = subtasks[subtask_index].new_job_task_id;
                            new_connection_dst_task_n_input = subtask_io_index;
                        }
                        else
                        {
                            new_connection_src_task_id       = subtasks[subtask_index].new_job_task_id;
                            new_connection_src_task_n_output = subtask_io_index;
                        }

                        #ifdef _DEBUG
                        {
                            ASSERT_DEBUG_SYNC(!ral_present_job_is_connection_defined(job,
                                                                                     new_connection_src_task_id,
                                                                                     new_connection_src_task_n_output,
                                                                                     new_connection_dst_task_id,
                                                                                     new_connection_dst_task_n_input),
                                              "A new connection, about to be created as a result of the flattening op, is already there?!");
                        }
                        #endif

                        /* Create a new connection between root-level nodes */
                        ral_present_job_connect_tasks(job,
                                                      new_connection_src_task_id,
                                                      new_connection_src_task_n_output,
                                                      new_connection_dst_task_id,
                                                      new_connection_dst_task_n_input,
                                                      nullptr); /* out_opt_connection_id_ptr */

                        has_found_mapping = true;
                    }

                    if (!has_found_mapping)
                    {
                        ASSERT_DEBUG_SYNC(has_found_mapping,
                                          "Could not find relevant mappings!");

                        goto end;
                    }
                }

                /* Safe to drop the processed connection at this point */
                delete connection_ptr;

                system_hash64map_remove(job_ptr->connections,
                                        connection_id);

                n_connection = -1;
            }

            system_hash64map_get_property(job_ptr->connections,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_connections);
        }

        /* Clean up other stuff */
        if (subtasks != nullptr)
        {
            _freea(subtasks);
            subtasks = nullptr;
        }

        task_ptr->has_been_flattened = true;

        /* If we reached this place, it means we've broken a group task into one or more subtasks.
         * Since we use a hashmap for task storage, we need to walk through all tasks all over again
         * to ensure we do not miss any of the defined tasks */
        n_task = -1;

        system_hash64map_get_property(job_ptr->tasks,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_tasks);
    }

    /* At this point, none of the existing group tasks should be referred to from any of the connections.
     * Therefore, we should be safe to drop them all into an endless pit of forgetfulness */
    #ifdef _DEBUG
    {
        /* Verify our assertion is actually true */
        uint32_t n_connections = 0;

        system_hash64map_get_property(job_ptr->connections,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                     &n_connections);

        for (uint32_t n_connection = 0;
                      n_connection < n_connections;
                    ++n_connection)
        {
            _ral_present_job_connection* connection_ptr = nullptr;
            _ral_present_job_task*       dst_task_ptr   = nullptr;
            ral_present_task_type        dst_task_type;
            _ral_present_job_task*       src_task_ptr   = nullptr;
            ral_present_task_type        src_task_type;

            system_hash64map_get_element_at(job_ptr->connections,
                                            n_connection,
                                           &connection_ptr,
                                            nullptr); /* result_hash_ptr */

            system_hash64map_get(job_ptr->tasks,
                                 connection_ptr->dst_task_id,
                                &dst_task_ptr);
            system_hash64map_get(job_ptr->tasks,
                                 connection_ptr->src_task_id,
                                &src_task_ptr);

            ral_present_task_get_property(dst_task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_TYPE,
                                         &dst_task_type);
            ral_present_task_get_property(src_task_ptr->task,
                                          RAL_PRESENT_TASK_PROPERTY_TYPE,
                                         &src_task_type);

            ASSERT_DEBUG_SYNC(dst_task_type != RAL_PRESENT_TASK_TYPE_GROUP &&
                              src_task_type != RAL_PRESENT_TASK_TYPE_GROUP,
                              "Group tasks are still present despite completed flattening !");
        }
    }
    #endif

    system_hash64map_get_property(job_ptr->tasks,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_tasks);

    for (int32_t n_task = 0;
                 n_task < static_cast<int32_t>(n_tasks);
               ++n_task)
    {
        system_hash64          task_id;
        _ral_present_job_task* task_ptr = nullptr;
        ral_present_task_type  task_type;

        system_hash64map_get_element_at(job_ptr->tasks,
                                        n_task,
                                       &task_ptr,
                                       &task_id);

        ral_present_task_get_property(task_ptr->task,
                                      RAL_PRESENT_TASK_PROPERTY_TYPE,
                                     &task_type);

        if (task_type == RAL_PRESENT_TASK_TYPE_GROUP)
        {
            delete task_ptr;

            system_hash64map_remove(job_ptr->tasks,
                                    task_id);

            n_task = -1;

            --n_tasks;
        }
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_get_connection_id_at_index(ral_present_job                job,
                                                                   uint32_t                       n_connection,
                                                                   ral_present_job_connection_id* out_result_ptr)
{
    _ral_present_job* job_ptr               = reinterpret_cast<_ral_present_job*>(job);
    uint32_t          n_connections_defined = 0;
    bool              result                = false;
    system_hash64     result64;

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

    system_hash64map_get_element_at(job_ptr->connections,
                                    n_connection,
                                    nullptr, /* result_element_ptr */
                                   &result64);

    *out_result_ptr = static_cast<ral_present_job_connection_id>(result64);

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_get_connection_property(ral_present_job                     job,
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
            *reinterpret_cast<ral_present_task_id*>(out_result_ptr) = connection_ptr->dst_task_id;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_DST_TASK_INPUT_INDEX:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = connection_ptr->n_dst_task_input;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_ID:
        {
            *reinterpret_cast<ral_present_task_id*>(out_result_ptr) = connection_ptr->src_task_id;

            break;
        }

        case RAL_PRESENT_JOB_CONNECTION_PROPERTY_SRC_TASK_OUTPUT_INDEX:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = connection_ptr->n_src_task_output;

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
PUBLIC EMERALD_API void ral_present_job_get_property(ral_present_job          job,
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
        case RAL_PRESENT_JOB_PROPERTY_N_CONNECTIONS:
        {
            system_hash64map_get_property(job_ptr->connections,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                          out_result_ptr);

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_N_PRESENT_TASKS:
        {
            system_hash64map_get_property(job_ptr->tasks,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                          out_result_ptr);

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_DEFINED:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = job_ptr->presentable_output_defined;

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID:
        {
            ASSERT_DEBUG_SYNC(job_ptr->presentable_output_defined,
                             "RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_ID property value queried, even though presentable output was not defined.");

            *reinterpret_cast<ral_present_task_id*>(out_result_ptr) = job_ptr->presentable_output_task_id;

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_INDEX:
        {
            ASSERT_DEBUG_SYNC(job_ptr->presentable_output_defined,
                             "RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_INDEX property value queried, even though presentable output was not defined.");

            *reinterpret_cast<uint32_t*>(out_result_ptr) = job_ptr->presentable_output_io_index;

            break;
        }

        case RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_TYPE:
        {
            ASSERT_DEBUG_SYNC(job_ptr->presentable_output_defined,
                             "RAL_PRESENT_JOB_PROPERTY_PRESENTABLE_OUTPUT_TASK_IO_TYPE property value queried, even though presentable output was not defined.");

            *reinterpret_cast<ral_present_task_io_type*>(out_result_ptr) = job_ptr->presentable_output_io_type;

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
PUBLIC EMERALD_API bool ral_present_job_get_task_at_index(ral_present_job   job,
                                                          uint32_t          index,
                                                          ral_present_task* out_task_ptr)
{
    _ral_present_job*      job_ptr      = reinterpret_cast<_ral_present_job*>(job);
    _ral_present_job_task* job_task_ptr = nullptr;
    bool                   result;

    result = system_hash64map_get_element_at(job_ptr->tasks,
                                             index,
                                            &job_task_ptr,
                                             nullptr); /* result_hash_ptr */

    if (result)
    {
        *out_task_ptr = job_task_ptr->task;
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_get_task_with_id(ral_present_job     present_job,
                                                         ral_present_task_id task_id,
                                                         ral_present_task*   out_result_task_ptr)
{
    _ral_present_job*      job_ptr      = reinterpret_cast<_ral_present_job*>(present_job);
    _ral_present_job_task* job_task_ptr = nullptr;
    bool                   result;

    result = system_hash64map_get(job_ptr->tasks,
                                  static_cast<system_hash64>(task_id),
                                 &job_task_ptr);

    if (result)
    {
        *out_result_task_ptr = job_task_ptr->task;
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_is_connection_defined(ral_present_job     job,
                                                              ral_present_task_id src_task_id,
                                                              uint32_t            n_src_task_output,
                                                              ral_present_task_id dst_task_id,
                                                              uint32_t            n_dst_task_n_input)
{
    _ral_present_job* job_ptr       = reinterpret_cast<_ral_present_job*>(job);
    uint32_t          n_connections = 0;
    bool              result        = false;

    system_hash64map_get_property(job_ptr->connections,
                                  SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                 &n_connections);

    for (uint32_t n_connection = 0;
                  n_connection < n_connections && !result;
                ++n_connection)
    {
        _ral_present_job_connection* connection_ptr = nullptr;

        system_hash64map_get_element_at(job_ptr->connections,
                                        n_connection,
                                       &connection_ptr,
                                        nullptr); /* result_hash_ptr */

        if (connection_ptr->src_task_id       == src_task_id       &&
            connection_ptr->n_src_task_output == n_src_task_output &&
            connection_ptr->dst_task_id       == dst_task_id       &&
            connection_ptr->n_dst_task_input  == n_dst_task_n_input)
        {
            result = true;

            break;
        }
    }

    return result;
}

/** Please see header for spec */
PUBLIC EMERALD_API void ral_present_job_release(ral_present_job job)
{
    delete reinterpret_cast<_ral_present_job*>(job);
}

/** Please see header for spec */
PUBLIC EMERALD_API bool ral_present_job_set_presentable_output(ral_present_job     job,
                                                               ral_present_task_id task_id,
                                                               bool                is_input_io,
                                                               uint32_t            n_io)
{
    _ral_present_job*              job_ptr          = reinterpret_cast<_ral_present_job*>(job);
    bool                           result           = false;
    const ral_present_task_io_type task_io_type     = (is_input_io) ? RAL_PRESENT_TASK_IO_TYPE_INPUT
                                                                    : RAL_PRESENT_TASK_IO_TYPE_OUTPUT;
    ral_texture_view               task_output      = nullptr;
    ral_format                     task_output_format;
    ral_context_object_type        task_output_type;
    _ral_present_job_task*         task_ptr         = nullptr;

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
                                          task_io_type,
                                          n_io,
                                          RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                          reinterpret_cast<void**>(&task_output_type)) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve object type of a present task IO");

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

    /* Presentable output needs to hold color data */
    bool has_color_data = false;

    ral_present_task_get_io_property(task_ptr->task,
                                     task_io_type,
                                     n_io,
                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT,
                                     reinterpret_cast<void**>(&task_output));

    ral_texture_view_get_property(task_output,
                                  RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                 &task_output_format);

    ral_utils_get_format_property(task_output_format,
                                  RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                 &has_color_data);

    if (!has_color_data)
    {
        ASSERT_DEBUG_SYNC(has_color_data,
                          "Presentable output specified for a present job does not carry color data!");

        goto end;
    }

    /* Store the specified presentable output */
    job_ptr->presentable_output_defined  = true;
    job_ptr->presentable_output_io_index = n_io;
    job_ptr->presentable_output_io_type  = task_io_type;
    job_ptr->presentable_output_task_id  = task_id;

    /* All done */
    result = true;

end:
    return result;
}
