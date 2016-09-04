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
    ral_present_task task; 

    explicit _ral_present_job_task(system_dag_node  in_dag_node,
                                   ral_present_task in_task)
    {
        task = in_task;
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

    bool                     presentable_output_defined;
    uint32_t                 presentable_output_io_index;
    ral_present_task_io_type presentable_output_io_type;
    ral_present_task_id      presentable_output_task_id;

    _ral_present_job()
    {
        connections                 = system_hash64map_create(sizeof(_ral_present_job_connection*) );
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

    /* Store the new task, simultaneously assigning it a new ID */
    new_task_ptr = new _ral_present_job_task(new_task_dag_node,
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
PUBLIC EMERALD_API bool ral_present_job_get_connection_id_at_index(ral_present_job                job,
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

            *reinterpret_cast<uint32_t*>(out_result_ptr) = job_ptr->presentable_output_io_type;

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
