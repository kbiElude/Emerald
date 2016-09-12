/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"


typedef struct _ral_present_task
{
    ral_command_buffer        command_buffer;
    ral_context               context;
    ral_present_task_io*      inputs;
    uint32_t                  n_inputs;
    uint32_t                  n_outputs;
    system_hashed_ansi_string name;
    ral_present_task_io*      outputs;
    ral_present_task_type     type;

    ral_present_task_ingroup_connection* group_task_connections;
    ral_present_task_group_mapping*      group_task_input_mappings;
    ral_present_task_group_mapping*      group_task_output_mappings;
    ral_present_task*                    group_task_subtasks;
    uint32_t                             n_group_task_connections;
    uint32_t                             n_group_task_input_mappings;
    uint32_t                             n_group_task_output_mappings;
    uint32_t                             n_group_task_subtasks;

    volatile uint32_t ref_counter;


    void set_io(ral_present_task_io_type   type,
                const ral_present_task_io* src_io_ptr,
                ral_present_task_io*       dst_io_ptr);

    /** TODO. CPU tasks only */
    explicit _ral_present_task(system_hashed_ansi_string               in_name,
                               const ral_present_task_cpu_create_info* in_create_info_ptr)
    {
        command_buffer               = nullptr;
        context                      = nullptr;
        group_task_connections       = nullptr;
        group_task_input_mappings    = nullptr;
        group_task_output_mappings   = nullptr;
        group_task_subtasks          = nullptr;
        inputs                       = nullptr;
        n_group_task_connections     = 0;
        n_group_task_input_mappings  = 0;
        n_group_task_output_mappings = 0;
        n_group_task_subtasks        = 0;
        n_inputs                     = 0;
        n_outputs                    = 0;
        outputs                      = nullptr;
        ref_counter                  = 1;
        type                         = RAL_PRESENT_TASK_TYPE_CPU_TASK;

        init_ios(in_create_info_ptr->n_unique_inputs,
                 in_create_info_ptr->unique_inputs,
                 in_create_info_ptr->n_unique_outputs,
                 in_create_info_ptr->unique_outputs);
    }

    /** TODO. GPU tasks only */
    explicit _ral_present_task(system_hashed_ansi_string               in_name,
                               const ral_present_task_gpu_create_info* in_create_info_ptr)
    {
        ral_command_buffer_get_property(in_create_info_ptr->command_buffer,
                                        RAL_COMMAND_BUFFER_PROPERTY_CONTEXT,
                                       &context);

        command_buffer               = in_create_info_ptr->command_buffer;
        group_task_connections       = nullptr;
        group_task_input_mappings    = nullptr;
        group_task_output_mappings   = nullptr;
        group_task_subtasks          = nullptr;
        inputs                       = nullptr;
        n_group_task_connections     = 0;
        n_group_task_input_mappings  = 0;
        n_group_task_output_mappings = 0;
        n_group_task_subtasks        = 0;
        n_inputs                     = 0;
        n_outputs                    = 0;
        name                         = in_name;
        outputs                      = nullptr;
        ref_counter                  = 1;
        type                         = RAL_PRESENT_TASK_TYPE_GPU_TASK;

        init_ios(in_create_info_ptr->n_unique_inputs,
                 in_create_info_ptr->unique_inputs,
                 in_create_info_ptr->n_unique_outputs,
                 in_create_info_ptr->unique_outputs);

        ral_context_retain_object(context,
                                  RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                  command_buffer);
    }

    /** TODO. Group tasks only */
    explicit _ral_present_task(system_hashed_ansi_string                 in_name,
                               const ral_present_task_group_create_info* in_create_info_ptr)
    {
        command_buffer               = nullptr;
        group_task_connections       = nullptr;
        group_task_input_mappings    = nullptr;
        group_task_output_mappings   = nullptr;
        group_task_subtasks          = nullptr;
        inputs                       = nullptr;
        n_group_task_connections     = 0;
        n_group_task_input_mappings  = 0;
        n_group_task_output_mappings = 0;
        n_group_task_subtasks        = 0;
        n_inputs                     = 0;
        n_outputs                    = 0;
        name                         = in_name;
        outputs                      = nullptr;
        ref_counter                  = 1;
        type                         = RAL_PRESENT_TASK_TYPE_GROUP;

        init_group_task(in_create_info_ptr);
    }

    /** TODO */
    ~_ral_present_task()
    {
        struct
        {
            ral_present_task_io* array;
            uint32_t             n_items;
        } io_arrays_to_release[] =
        {
            {inputs,  n_inputs},
            {outputs, n_outputs}
        };
        const uint32_t n_io_arrays_to_release = sizeof(io_arrays_to_release) / sizeof(io_arrays_to_release[0]);

        ASSERT_DEBUG_SYNC(ref_counter == 0,
                          "Destructor called for a ral_present_task instance whose ref counter == %d",
                          ref_counter);

        for (uint32_t n_io_array = 0;
                      n_io_array < n_io_arrays_to_release;
                    ++n_io_array)
        {
            const ral_present_task_io* current_io_array_ptr = io_arrays_to_release[n_io_array].array;
            const uint32_t             n_items              = io_arrays_to_release[n_io_array].n_items;

            if (current_io_array_ptr == nullptr)
            {
                continue;
            }

            for (uint32_t n_item = 0;
                          n_item < n_items;
                        ++n_item)
            {
                ral_context_delete_objects(context,
                                           current_io_array_ptr[n_item].object_type,
                                           1, /* n_objects */
                                           reinterpret_cast<void* const*>(&current_io_array_ptr[n_item].object) );
            }

            delete [] current_io_array_ptr;
        }

        if (command_buffer != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                       1, /* n_objects */
                                       reinterpret_cast<void* const*>(&command_buffer) );

            command_buffer = nullptr;
        }

        if (group_task_connections != nullptr)
        {
            delete [] group_task_connections;

            group_task_connections = nullptr;
        }

        if (group_task_input_mappings != nullptr)
        {
            delete [] group_task_input_mappings;

            group_task_input_mappings = nullptr;
        }

        if (group_task_output_mappings != nullptr)
        {
            delete [] group_task_output_mappings;

            group_task_output_mappings = nullptr;
        }

        if (group_task_subtasks != nullptr)
        {
            for (uint32_t n_subtask = 0;
                          n_subtask < n_group_task_subtasks;
                        ++n_subtask)
            {
                ral_present_task_release(group_task_subtasks[n_subtask]);
            }

            delete [] group_task_subtasks;
            group_task_subtasks = nullptr;
        }
    }

private:
    void init_group_task(const ral_present_task_group_create_info* in_create_info_ptr)
    {
        group_task_connections = new ral_present_task_ingroup_connection[in_create_info_ptr->n_ingroup_connections];
        group_task_subtasks    = new ral_present_task                   [in_create_info_ptr->n_present_tasks];

        if (in_create_info_ptr->n_unique_input_to_ingroup_task_mappings > 0)
        {
            group_task_input_mappings = new ral_present_task_group_mapping[in_create_info_ptr->n_unique_input_to_ingroup_task_mappings];

            memcpy(group_task_input_mappings,
                   in_create_info_ptr->unique_input_to_ingroup_task_mapping,
                   sizeof(ral_present_task_group_mapping) * in_create_info_ptr->n_unique_input_to_ingroup_task_mappings);

            n_group_task_input_mappings = in_create_info_ptr->n_unique_input_to_ingroup_task_mappings;
        }

        if (in_create_info_ptr->n_unique_output_to_ingroup_task_mappings > 0)
        {
            group_task_output_mappings = new ral_present_task_group_mapping[in_create_info_ptr->n_unique_output_to_ingroup_task_mappings];

            memcpy(group_task_output_mappings,
                   in_create_info_ptr->unique_output_to_ingroup_task_mapping,
                   sizeof(ral_present_task_group_mapping) * in_create_info_ptr->n_unique_output_to_ingroup_task_mappings);

            n_group_task_output_mappings = in_create_info_ptr->n_unique_output_to_ingroup_task_mappings;
        }

        n_group_task_connections     = in_create_info_ptr->n_ingroup_connections;
        n_group_task_output_mappings = in_create_info_ptr->n_unique_output_to_ingroup_task_mappings;
        n_group_task_subtasks        = in_create_info_ptr->n_present_tasks;

        memcpy(group_task_connections,
               in_create_info_ptr->ingroup_connections,
               sizeof(ral_present_task_ingroup_connection) * n_group_task_connections);

        for (uint32_t n_subtask = 0;
                      n_subtask < n_group_task_subtasks;
                    ++n_subtask)
        {
            group_task_subtasks[n_subtask] = in_create_info_ptr->present_tasks[n_subtask];

            ral_present_task_retain(group_task_subtasks[n_subtask]);
        }

        #ifdef _DEBUG
        {
            /* Perform more advanced validation */
            uint32_t*      inputs_last_mapping_id_ptr  = nullptr;
            const uint32_t unused_mapping_id           = 0xFFFFFFFF;
            uint32_t*      outputs_last_mapping_id_ptr = nullptr;

            if (in_create_info_ptr->n_total_unique_inputs > 0)
            {
                inputs_last_mapping_id_ptr = reinterpret_cast<uint32_t*>(_malloca(sizeof(uint32_t) * in_create_info_ptr->n_total_unique_inputs));

                memset(inputs_last_mapping_id_ptr,
                       unused_mapping_id,
                       sizeof(uint32_t) * in_create_info_ptr->n_total_unique_inputs);
            }
            else
            {
                ASSERT_DEBUG_SYNC(in_create_info_ptr->n_unique_input_to_ingroup_task_mappings == 0,
                                  "No input->ingroup task input mapping can be defined if zero unique inputs are requested");
            }

            if (in_create_info_ptr->n_total_unique_outputs > 0)
            {
                outputs_last_mapping_id_ptr = reinterpret_cast<uint32_t*>(_malloca(sizeof(uint32_t) * in_create_info_ptr->n_total_unique_outputs));

                memset(outputs_last_mapping_id_ptr,
                       unused_mapping_id,
                       sizeof(uint32_t) * in_create_info_ptr->n_total_unique_outputs);
            }
            else
            {
                ASSERT_DEBUG_SYNC(in_create_info_ptr->n_unique_output_to_ingroup_task_mappings == 0,
                                  "No ingroup task output->output mapping can be defined if zero unique outputs are requested");
            }

            for (ral_present_task_io_type io_type = RAL_PRESENT_TASK_IO_TYPE_FIRST;
                                          io_type < RAL_PRESENT_TASK_IO_TYPE_COUNT;
                                 ++((int&)io_type) )
            {
                uint32_t*                             io_last_mapping_id_ptr = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? inputs_last_mapping_id_ptr
                                                                                                                           : outputs_last_mapping_id_ptr;
                const uint32_t                        n_max_ios              = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? in_create_info_ptr->n_total_unique_inputs
                                                                                                                           : in_create_info_ptr->n_total_unique_outputs;
                const ral_present_task_group_mapping* mappings               = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? in_create_info_ptr->unique_input_to_ingroup_task_mapping :
                                                                                                                             in_create_info_ptr->unique_output_to_ingroup_task_mapping;
                const uint32_t                        n_mappings             = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? in_create_info_ptr->n_unique_input_to_ingroup_task_mappings
                                                                                                                           : in_create_info_ptr->n_unique_output_to_ingroup_task_mappings;

                for (uint32_t n_mapping = 0;
                              n_mapping < n_mappings;
                            ++n_mapping)
                {
                    const ral_present_task_group_mapping& mapping                = mappings[n_mapping];
                    ral_present_task                      current_io_user        = nullptr;
                    void*                                 current_io_object      = nullptr;
                    ral_context_object_type               current_io_object_type = RAL_CONTEXT_OBJECT_TYPE_UNKNOWN;
         
                    ral_present_task                      last_io_user           = nullptr;
                    void*                                 last_io_object         = nullptr;
                    ral_context_object_type               last_io_object_type    = RAL_CONTEXT_OBJECT_TYPE_UNKNOWN;

                    ASSERT_DEBUG_SYNC(mapping.group_task_io_index < n_max_ios,
                                      "Invalid group task IO index specified");
                    ASSERT_DEBUG_SYNC(mapping.n_present_task < in_create_info_ptr->n_present_tasks,
                                      "Invalid present task index specified for a group present task mapping");

                    if (io_last_mapping_id_ptr[mapping.group_task_io_index] == unused_mapping_id)
                    {
                        io_last_mapping_id_ptr[mapping.group_task_io_index] = n_mapping;

                        continue;
                    }

                    /* Group IOs may be assigned data coming from multiple present task IOs. For validation, we need
                     * to ensure that all such IOs use the same underlying ral_buffer or ral_texture instance, and are
                     * assigned the same type of object. */
                    current_io_user = in_create_info_ptr->present_tasks[mapping.n_present_task];
                    last_io_user    = in_create_info_ptr->present_tasks[io_last_mapping_id_ptr[mapping.group_task_io_index] ];

                    ASSERT_DEBUG_SYNC(current_io_user != nullptr,
                                      "Null present task specified");

                    ral_present_task_get_io_property(current_io_user,
                                                     io_type,
                                                     mapping.present_task_io_index,
                                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                                     (void**) &current_io_object_type);
                    ral_present_task_get_io_property(last_io_user,
                                                     io_type,
                                                     mapping.present_task_io_index,
                                                     RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE,
                                                     (void**) &last_io_object_type);

                    ASSERT_DEBUG_SYNC(current_io_object_type != last_io_object_type,
                                      "A group IO maps to present task IOs of different object types");

                    switch (current_io_object_type)
                    {
                        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
                        {
                            ASSERT_DEBUG_SYNC(current_io_object == last_io_object,
                                              "A group IO maps to present task IOs which make use of different RAL buffer objects");

                            break;
                        }

                        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW:
                        {
                            ral_texture      current_io_user_parent_texture = nullptr;
                            ral_texture_view current_io_user_texture_view   = reinterpret_cast<ral_texture_view>(current_io_object);
                            ral_texture      last_io_user_parent_texture    = nullptr;
                            ral_texture_view last_io_user_texture_view      = reinterpret_cast<ral_texture_view>(last_io_object);

                            ral_texture_view_get_property(current_io_user_texture_view,
                                                          RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                                         &current_io_user_parent_texture);
                            ral_texture_view_get_property(last_io_user_texture_view,
                                                          RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                                         &last_io_user_parent_texture);

                            ASSERT_DEBUG_SYNC(current_io_user_parent_texture == last_io_user_parent_texture,
                                              "A group IO maps to present task IOs which make use of different RAL texture objects");

                            break;
                        }

                        default:
                        {
                            ASSERT_DEBUG_SYNC(false,
                                              "TODO: Unsupported IO object type");
                        }
                    }
                }

                /* Make sure all IOs of this type have present subtask IOs assigned */
                for (uint32_t n_io = 0;
                              n_io < n_max_ios;
                           ++n_io)
                {
                    ASSERT_DEBUG_SYNC(io_last_mapping_id_ptr[n_io] != unused_mapping_id,
                                      "Group present task IO [%d] has no sub-task IO assigned",
                                      n_io);
                }
            }
        }
        #endif
    }

    void init_ios(uint32_t                   n_input_ios,
                  const ral_present_task_io* input_ios,
                  uint32_t                   n_output_ios,
                  const ral_present_task_io* output_ios)
    {
        inputs  = (n_input_ios != 0)  ? new ral_present_task_io[n_input_ios]
                                      : nullptr;
        outputs = (n_output_ios != 0) ? new ral_present_task_io[n_output_ios]
                                      : nullptr;

        for (ral_present_task_io_type io_type = RAL_PRESENT_TASK_IO_TYPE_FIRST;
                                      io_type < RAL_PRESENT_TASK_IO_TYPE_COUNT;
                             ++((int&)io_type) )
        {
            const uint32_t             n_ios   = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? n_input_ios
                                                                                             : n_output_ios;
            const ral_present_task_io* src_ios = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? input_ios
                                                                                             : output_ios;
            ral_present_task_io*       dst_ios = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? inputs
                                                                                             : outputs;

            for (uint32_t n_io = 0;
                          n_io < n_ios;
                        ++n_io)
            {
                dst_ios[n_io].object      = src_ios[n_io].object;
                dst_ios[n_io].object_type = src_ios[n_io].object_type;

#if 0
                todo: restore

                ral_context_retain_object(context,
                                          dst_ios[n_io].object_type,
                                          dst_ios[n_io].object);
#endif
            }
        }
    }
} _ral_present_task;


/** TODO */
void _ral_present_task::set_io(ral_present_task_io_type   type,
                               const ral_present_task_io* src_io_ptr,
                               ral_present_task_io*       dst_io_ptr)
{
    
}

#if 0

IOs need to be specified explicitly at the moment. We may want to change that in the future.

/** TODO */
void _ral_present_task::update_ios_internal(ral_command_buffer in_command_buffer)
{
    /* Iterate over all commands and cache all objects which are read or modified by the
     * encapsulated command buffer. Make sure to only create unique entries. */
    uint32_t n_recorded_commands = 0;

    ral_command_buffer_get_property(in_command_buffer,
                                    RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS,
                                   &n_recorded_commands);

    for (uint32_t n_recorded_command = 0;
                  n_recorded_command < n_recorded_commands;
                ++n_recorded_command)
    {
        const void*      command_raw_ptr = nullptr;
        ral_command_type command_type    = RAL_COMMAND_TYPE_UNKNOWN;

        if (!ral_command_buffer_get_recorded_command(in_command_buffer,
                                                     n_recorded_command,
                                                    &command_type,
                                                    &command_raw_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unable to retrieve a recorded command at index [%d]",
                              n_recorded_command);

            continue;
        }

        switch (command_type)
        {
            case RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE:
            {
                ral_command_buffer_copy_texture_to_texture_command_info* command_ptr = (ral_command_buffer_copy_texture_to_texture_command_info*) command_raw_ptr;

                if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                   command_ptr->src_texture) )
                {
                    #ifdef _DEBUG
                    {
                        ral_texture_usage_bits dst_texture_usage = 0;
                        ral_texture_usage_bits src_texture_usage = 0;

                        ral_texture_get_property(command_ptr->dst_texture,
                                                 RAL_TEXTURE_PROPERTY_USAGE,
                                                &dst_texture_usage);
                        ral_texture_get_property(command_ptr->src_texture,
                                                 RAL_TEXTURE_PROPERTY_USAGE,
                                                &src_texture_usage);

                        ASSERT_DEBUG_SYNC( (dst_texture_usage & RAL_TEXTURE_USAGE_BLIT_DST_BIT) != 0,
                                          "Destination texture for a copy op does not have the blit_dst usage enabled.");
                        ASSERT_DEBUG_SYNC( (src_texture_usage & RAL_TEXTURE_USAGE_BLIT_SRC_BIT) != 0,
                                          "Source texture for a copy op does not have the blit_src usage enabled.");
                    }
                    #endif

                    add_io(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                           command_ptr->src_texture);
                }

                if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                   command_ptr->dst_texture) )
                {
                    add_io(RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                           command_ptr->dst_texture);
                }

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDEXED:
            case RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT:
            case RAL_COMMAND_TYPE_DRAW_CALL_REGULAR:
            {
                uint32_t           n_read_accesses  = 0;
                uint32_t           n_write_accesses = 0;
                ral_object_access* read_access_ptr  = nullptr;
                ral_object_access* write_access_ptr = nullptr;

                switch (command_type)
                {
                    case RAL_COMMAND_TYPE_DRAW_CALL_INDEXED:
                    {
                        ral_command_buffer_draw_call_indexed_command_info* command_ptr = (ral_command_buffer_draw_call_indexed_command_info*) command_raw_ptr;

                        n_read_accesses  = command_ptr->n_read_deps;
                        n_write_accesses = command_ptr->n_write_deps;
                        read_access_ptr  = command_ptr->read_deps;
                        write_access_ptr = command_ptr->write_deps;

                        #ifdef _DEBUG
                        {
                            /* Sanity checks */
                            ral_buffer_usage_bits index_buffer_usage = 0;

                            ral_buffer_get_property(command_ptr->index_buffer,
                                                    RAL_BUFFER_PROPERTY_USAGE_BITS,
                                                   &index_buffer_usage);

                            ASSERT_DEBUG_SYNC( (index_buffer_usage & RAL_BUFFER_USAGE_INDEX_BUFFER_BIT) != 0,
                                              "Index buffer has not been initialized for index_buffer usage");
                        }
                        #endif

                        if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                           command_ptr->index_buffer) )
                        {
                            add_io(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                   command_ptr->index_buffer);
                        }

                        break;
                    }

                    case RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT:
                    {
                        ral_command_buffer_draw_call_indirect_command_info* command_ptr = (ral_command_buffer_draw_call_indirect_command_info*) command_raw_ptr;

                        n_read_accesses  = command_ptr->n_read_deps;
                        n_write_accesses = command_ptr->n_write_deps;
                        read_access_ptr  = command_ptr->read_deps;
                        write_access_ptr = command_ptr->write_deps;

                        #ifdef _DEBUG
                        {
                            /* Sanity checks */
                            ral_buffer_usage_bits indirect_arg_buffer_usage = 0;

                            ral_buffer_get_property(command_ptr->buffer,
                                                    RAL_BUFFER_PROPERTY_USAGE_BITS,
                                                   &indirect_arg_buffer_usage);

                            ASSERT_DEBUG_SYNC( (indirect_arg_buffer_usage & RAL_BUFFER_USAGE_INDIRECT_DRAW_BUFFER_BIT) != 0,
                                              "Indirect draw arg buffer has not been initialized for indirect_draw_buffer usage");
                        }
                        #endif

                        if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                           command_ptr->buffer))
                        {
                            add_io(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                   command_ptr->buffer);
                        }

                        break;
                    }

                    case RAL_COMMAND_TYPE_DRAW_CALL_REGULAR:
                    {
                        ral_command_buffer_draw_call_regular_command_info* command_ptr = (ral_command_buffer_draw_call_regular_command_info*) command_raw_ptr;

                        n_read_accesses  = command_ptr->n_read_deps;
                        n_write_accesses = command_ptr->n_write_deps;
                        read_access_ptr  = command_ptr->read_deps;
                        write_access_ptr = command_ptr->write_deps;

                        break;
                    }
                }

                /* TODO: Sanity checks here for the code below should check if the object, described in the user args', refer
                 *       to objects used for bindings or for vertex attributes */
                for (ral_present_task_io_type io_type = RAL_PRESENT_TASK_IO_TYPE_FIRST;
                                              io_type < RAL_PRESENT_TASK_IO_TYPE_COUNT;
                                    ++((int&) io_type))
                {
                    const ral_object_access* accesses   = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? read_access_ptr
                                                                                                      : write_access_ptr;
                    const uint32_t           n_accesses = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? n_read_accesses
                                                                                                      : n_write_accesses;

                    for (uint32_t n_access = 0;
                                  n_access < n_accesses;
                                ++n_access)
                    {
                        const ral_object_access& current_access = accesses[n_access];

                        if (current_access.buffer != nullptr)
                        {
                            if (!is_io_defined(io_type,
                                               current_access.buffer) )
                            {
                                add_io(io_type,
                                       current_access.buffer);
                            }
                        }
                        else
                        {
                            ral_texture parent_texture = nullptr;

                            ASSERT_DEBUG_SYNC(current_access.texture_view != nullptr,
                                              "Invalid RAL texture view instance.");

                            ral_texture_view_get_property(current_access.texture_view,
                                                          RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                                         &parent_texture);

                            ASSERT_DEBUG_SYNC(parent_texture != nullptr,
                                              "Invalid RAL texture instance.");

                            if (!is_io_defined(io_type,
                                               parent_texture) )
                            {
                                add_io(io_type,
                                       parent_texture);
                            }
                        }
                    }
                }

                break;
            }

            case RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                ral_command_buffer_execute_command_buffer_command_info* command_ptr = (ral_command_buffer_execute_command_buffer_command_info*) command_raw_ptr;

                update_ios_internal(command_ptr->command_buffer);

                break;
            }

            case RAL_COMMAND_TYPE_SET_RENDERTARGET_STATE:
            {
                ral_command_buffer_set_rendertarget_state_command_info* command_ptr    = (ral_command_buffer_set_rendertarget_state_command_info*) command_raw_ptr;
                ral_texture                                             parent_texture = nullptr;

                ral_texture_view_get_property(command_ptr->texture_view,
                                              RAL_TEXTURE_VIEW_PROPERTY_PARENT_TEXTURE,
                                             &parent_texture);

                ASSERT_DEBUG_SYNC(parent_texture != nullptr,
                                  "Invalid RAL texture instance.");

                if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                                   parent_texture) )
                {
                    #ifdef _DEBUG
                    {
                        ral_texture_usage_bits parent_texture_usage = 0;

                        ral_texture_get_property(parent_texture,
                                                 RAL_TEXTURE_PROPERTY_USAGE,
                                                &parent_texture_usage);

                        ASSERT_DEBUG_SYNC( (parent_texture_usage & RAL_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)         != 0 ||
                                           (parent_texture_usage & RAL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0,
                                           "Texture to be used as a render-target has not been created for C or DS usage.");
                    }
                    #endif

                    add_io(RAL_PRESENT_TASK_IO_TYPE_OUTPUT,
                           parent_texture);
                }

                break;
            }

            case RAL_COMMAND_TYPE_SET_VERTEX_ATTRIBUTE:
            {
                ral_command_buffer_set_vertex_attribute_command_info* command_ptr = (ral_command_buffer_set_vertex_attribute_command_info*) command_raw_ptr;

                if (!is_io_defined(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                                   command_ptr->buffer) )
                {
                    #ifdef _DEBUG
                    {
                        ral_buffer_usage_bits buffer_usage = 0;

                        ral_buffer_get_property(command_ptr->buffer,
                                                RAL_BUFFER_PROPERTY_USAGE_BITS,
                                               &buffer_usage);

                        ASSERT_DEBUG_SYNC( (buffer_usage & RAL_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0,
                                           "Specified buffer has not been created for vertex attribute usage.");
                    }
                    #endif

                    add_io(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                           command_ptr->buffer);
                }

                break;
            }

            /* Dummy commands we don't care about */
            case RAL_COMMAND_TYPE_SET_GFX_STATE:
            case RAL_COMMAND_TYPE_SET_PROGRAM:
            case RAL_COMMAND_TYPE_SET_SCISSOR_BOX:
            case RAL_COMMAND_TYPE_SET_VIEWPORT:
            {
                continue;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized RAL command type - please implement.");
            }
        }
    }
}
#endif

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task ral_present_task_create_cpu(system_hashed_ansi_string               name,
                                                                const ral_present_task_cpu_create_info* create_info_ptr)

{
    _ral_present_task* result_ptr = nullptr;

    /* Sanity checks */
    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(name != nullptr,
                          "RAL present task's name is NULL");

        goto end;
    }

    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(create_info_ptr != nullptr,
                          "Create info structure for RAL present task is NULL");

        goto end;
    }

    if (create_info_ptr->pfn_cpu_task_callback_proc == nullptr)
    {
        ASSERT_DEBUG_SYNC(create_info_ptr->pfn_cpu_task_callback_proc != nullptr,
                          "RAL present CPU task call-back func ptr is NULL");

        goto end;
    }

    if (create_info_ptr->n_unique_inputs != 0      &&
        create_info_ptr->unique_inputs   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_unique_inputs != 0 &&
                            create_info_ptr->unique_inputs == nullptr),
                          "Null unique inputs array specified for n_unique_inputs != 0");

        goto end;
    }

    if (create_info_ptr->n_unique_outputs != 0 &&
        create_info_ptr->unique_outputs == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_unique_outputs != 0 &&
                            create_info_ptr->unique_outputs == nullptr),
                            "Null unique outputs array specified for n_unique_outputs != 0");

        goto end;
    }

    /* Spawn the new descriptor */
    result_ptr = new _ral_present_task(name,
                                       create_info_ptr);

    ASSERT_ALWAYS_SYNC(result_ptr != nullptr,
                       "Out of memory");

end:
    return (ral_present_task) result_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task ral_present_task_create_gpu(system_hashed_ansi_string               name,
                                                                const ral_present_task_gpu_create_info* create_info_ptr)
{
    ral_command_buffer_status command_buffer_status        = RAL_COMMAND_BUFFER_STATUS_UNDEFINED;
    bool                      is_command_buffer_resettable = false;
    _ral_present_task*        result_ptr                   = nullptr;

    /* Sanity checks */
    if (name == nullptr)
    {
        ASSERT_DEBUG_SYNC(name != nullptr,
                          "RAL present task's name is NULL");

        goto end;
    }

    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(create_info_ptr != nullptr,
                          "Create info structure for RAL present task is NULL");

        goto end;
    }

    if (create_info_ptr->command_buffer != nullptr)
    {
        ral_command_buffer_get_property(create_info_ptr->command_buffer,
                                        RAL_COMMAND_BUFFER_PROPERTY_IS_RESETTABLE,
                                       &is_command_buffer_resettable);
        ral_command_buffer_get_property(create_info_ptr->command_buffer,
                                        RAL_COMMAND_BUFFER_PROPERTY_STATUS,
                                       &command_buffer_status);

        if (command_buffer_status != RAL_COMMAND_BUFFER_STATUS_RECORDED)
        {
            ASSERT_DEBUG_SYNC(command_buffer_status == RAL_COMMAND_BUFFER_STATUS_RECORDED,
                              "RAL present task can only be created for a recorded RAL command buffer");

            goto end;
        }

        if (is_command_buffer_resettable)
        {
            ASSERT_DEBUG_SYNC(!is_command_buffer_resettable,
                              "RAL present task does not support resettable command buffers at the moment.");

            goto end;
        }
    }

    /* Should be fine to create a present task for this GPU task */
    result_ptr = new _ral_present_task(name,
                                       create_info_ptr);

end:
    return (ral_present_task) result_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API ral_present_task ral_present_task_create_group(system_hashed_ansi_string                 name,
                                                                  const ral_present_task_group_create_info* create_info_ptr)
{
    _ral_present_task* result_ptr = nullptr;

    /* Sanity checks */
    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(create_info_ptr != nullptr,
                          "Input ral_present_task_group_create_info instance is null");

        goto end;
    }

    if (create_info_ptr->n_ingroup_connections != 0        &&
        create_info_ptr->ingroup_connections   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_ingroup_connections != 0        &&
                            create_info_ptr->ingroup_connections   == nullptr),
                          "Ingroup connection array is null even though a non-zero number of ingroup connections was specified");

        goto end;
    }

    if (create_info_ptr->n_present_tasks != 0       &&
        create_info_ptr->present_tasks   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_present_tasks != 0       &&
                            create_info_ptr->present_tasks   == nullptr),
                          "Present task array is null even though a non-zero number of present tasks was specified");

        goto end;
    }

    if (create_info_ptr->n_total_unique_inputs                != 0       &&
        create_info_ptr->unique_input_to_ingroup_task_mapping == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_total_unique_inputs                != 0       &&
                            create_info_ptr->unique_input_to_ingroup_task_mapping == nullptr),
                          "Unique input->ingroup task mapping array is null even though a non-zero number of input mappings was specified");

        goto end;
    }

    if (create_info_ptr->n_total_unique_outputs                != 0 &&
        create_info_ptr->unique_output_to_ingroup_task_mapping == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_total_unique_outputs != 0 &&
                            create_info_ptr->unique_output_to_ingroup_task_mapping == nullptr),
                          "Unique output->ingroup task mapping array is null even though a non-zero number of output mappings was specified");

        goto end;
    }

    /* Should be fine to create a present task for this group task */
    result_ptr = new _ral_present_task(name,
                                       create_info_ptr);
end:
    return reinterpret_cast<ral_present_task>(result_ptr);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_present_task_get_io_mapping_property(ral_present_task                     task,
                                                                 ral_present_task_io_type             io_type,
                                                                 uint32_t                             n_io_mapping,
                                                                 ral_present_task_io_mapping_property property,
                                                                 void**                               out_result_ptr)
{
    const ral_present_task_group_mapping* mappings_ptr = nullptr;
    bool                                  result       = false;
    _ral_present_task*                    task_ptr     = reinterpret_cast<_ral_present_task*>(task);

    /* Sanity checks */
    if (task == nullptr)
    {
        ASSERT_DEBUG_SYNC(task != nullptr,
                          "Input ral_present_task instance is null");

        goto end;
    }

    if (task_ptr->type != RAL_PRESENT_TASK_TYPE_GROUP)
    {
        ASSERT_DEBUG_SYNC(task_ptr->type == RAL_PRESENT_TASK_TYPE_GROUP,
                          "IO mapping properties only available for group present tasks.");

        goto end;
    }

    switch (io_type)
    {
        case RAL_PRESENT_TASK_IO_TYPE_INPUT:
        {
            if (task_ptr->n_group_task_input_mappings < n_io_mapping)
            {
                ASSERT_DEBUG_SYNC(!(task_ptr->n_group_task_input_mappings < n_io_mapping),
                                  "Invalid input mapping index [%d] requested.",
                                 n_io_mapping);

                goto end;
            }

            mappings_ptr = task_ptr->group_task_input_mappings;

            break;
        }

        case RAL_PRESENT_TASK_IO_TYPE_OUTPUT:
        {
            if (task_ptr->n_group_task_output_mappings < n_io_mapping)
            {
                ASSERT_DEBUG_SYNC(!(task_ptr->n_group_task_output_mappings < n_io_mapping),
                                  "Invalid output mapping index [%d] requested.",
                                 n_io_mapping);

                goto end;
            }

            mappings_ptr = task_ptr->group_task_output_mappings;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid IO type requested.");

            goto end;
        }
    }


    if (mappings_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(mappings_ptr != nullptr,
                          "No mappings of specified type have been defined.");

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_GROUP_TASK_IO_INDEX:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = mappings_ptr[n_io_mapping].group_task_io_index;

            break;
        }

        case RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_IO_INDEX:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = mappings_ptr[n_io_mapping].present_task_io_index;

            break;
        }

        case RAL_PRESENT_TASK_IO_MAPPING_PROPERTY_SUBTASK_INDEX:
        {
            *reinterpret_cast<uint32_t*>(out_result_ptr) = mappings_ptr[n_io_mapping].n_present_task;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid ral_present_task_io_mapping_property value specified.");

            goto end;
        }
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_present_task_get_io_property(ral_present_task             task,
                                                         ral_present_task_io_type     io_type,
                                                         uint32_t                     n_io,
                                                         ral_present_task_io_property property,
                                                         void**                       out_result_ptr)
{
    const ral_present_task_io* ios_ptr  = nullptr;
    bool                       result   = false;
    _ral_present_task*         task_ptr = reinterpret_cast<_ral_present_task*>(task);

    /* Sanity checks */
    if (task == nullptr)
    {
        ASSERT_DEBUG_SYNC(task != nullptr,
                          "Input ral_present_task instance is null");

        goto end;
    }

    if (task_ptr->type == RAL_PRESENT_TASK_TYPE_GROUP)
    {
        ASSERT_DEBUG_SYNC(task_ptr->type != RAL_PRESENT_TASK_TYPE_GROUP,
                          "IO properties unavailable for a group present task.");

        goto end;
    }

    switch (io_type)
    {
        case RAL_PRESENT_TASK_IO_TYPE_INPUT:
        {
            if (task_ptr->n_inputs < n_io)
            {
                ASSERT_DEBUG_SYNC(!(task_ptr->n_inputs < n_io),
                                  "Invalid input index [%d] requested.",
                                  n_io);

                goto end;
            }

            ios_ptr = task_ptr->inputs;

            break;
        }

        case RAL_PRESENT_TASK_IO_TYPE_OUTPUT:
        {
            if (task_ptr->n_outputs < n_io)
            {
                ASSERT_DEBUG_SYNC(!(task_ptr->n_outputs < n_io),
                                  "Invalid output index [%d] requested.",
                                  n_io);

                goto end;
            }

            ios_ptr = task_ptr->outputs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid IO type requested.");

            goto end;
        }
    }


    if (ios_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(ios_ptr != nullptr,
                          "No IOs of specified type have been defined.");

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_TASK_IO_PROPERTY_OBJECT:
        {
            *reinterpret_cast<void**>(out_result_ptr) = ios_ptr[n_io].object;

            break;
        }

        case RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE:
        {
            *reinterpret_cast<ral_context_object_type*>(out_result_ptr) = ios_ptr[n_io].object_type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid ral_present_task_io_property value specified.");

            goto end;
        }
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_present_task_get_property(ral_present_task          task,
                                                      ral_present_task_property property,
                                                      void*                     out_result_ptr)
{
    _ral_present_task* task_ptr = reinterpret_cast<_ral_present_task*>(task);

    /* Sanity checks */
    if (task_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(task_ptr != nullptr,
                          "Input ral_present_task instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER:
        {
            ASSERT_DEBUG_SYNC(task_ptr->type == RAL_PRESENT_TASK_TYPE_GPU_TASK,
                              "RAL_PRESENT_TASK_PROPERTY_COMMAND_BUFFER property is only available for RAL present GPU tasks");

            *reinterpret_cast<ral_command_buffer*>(out_result_ptr) = task_ptr->command_buffer;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_INPUT_MAPPINGS:
        {
            ASSERT_DEBUG_SYNC(task_ptr->type == RAL_PRESENT_TASK_TYPE_GROUP,
                              "RAL_PRESENT_TASK_PROPERTY_N_INPUT_MAPPINGS query can only be used against group present tasks.");

            *reinterpret_cast<uint32_t*>(out_result_ptr) = task_ptr->n_group_task_input_mappings;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_INPUTS:
        {
            ASSERT_DEBUG_SYNC(task_ptr->type != RAL_PRESENT_TASK_TYPE_GROUP,
                              "RAL_PRESENT_TASK_PROPERTY_N_INPUTS query cannot be used against group present tasks.");

            *reinterpret_cast<uint32_t*>(out_result_ptr) = task_ptr->n_inputs;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_OUTPUT_MAPPINGS:
        {
            ASSERT_DEBUG_SYNC(task_ptr->type == RAL_PRESENT_TASK_TYPE_GROUP,
                              "RAL_PRESENT_TASK_PROPERTY_N_OUTPUT_MAPPINGS query can only be used against group present tasks.");

            *reinterpret_cast<uint32_t*>(out_result_ptr) = task_ptr->n_group_task_output_mappings;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS:
        {
            ASSERT_DEBUG_SYNC(task_ptr->type != RAL_PRESENT_TASK_TYPE_GROUP,
                              "RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS query cannot be used against group present tasks.");

            *reinterpret_cast<uint32_t*>(out_result_ptr) = task_ptr->n_outputs;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_NAME:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = task_ptr->name;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_TYPE:
        {
            *reinterpret_cast<ral_present_task_type*>(out_result_ptr) = task_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_present_task_property value specified.");
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_present_task_release(ral_present_task task)
{
    volatile uint32_t& ref_counter = reinterpret_cast<_ral_present_task*>(task)->ref_counter;

    if (--ref_counter == 0)
    {
        delete reinterpret_cast<_ral_present_task*>(task);
    }
}

/** Please see header for specification */
PUBLIC void ral_present_task_retain(ral_present_task task)
{
    ++(reinterpret_cast<_ral_present_task*>(task)->ref_counter);
}