#include "shared.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture_view.h"
#include "system/system_resizable_vector.h"


typedef struct _ral_present_task
{
    ral_command_buffer        command_buffer;
    ral_context               context;
    system_resizable_vector   inputs;  /* holds ral_present_task_io instances  */
    system_hashed_ansi_string name;
    system_resizable_vector   outputs; /* holds ral_present_task_io instances  */
    ral_present_task_type     type;

    void set_io       (ral_present_task_io_type   type,
                       const ral_present_task_io* src_io_ptr,
                       ral_present_task_io*       dst_io_ptr);
    void update_ios   ();

    /** TODO */
    explicit _ral_present_task(system_hashed_ansi_string               in_name,
                               const ral_present_task_cpu_create_info* in_create_info_ptr)
    {
        command_buffer = nullptr;
        context        = nullptr;
        inputs         = (in_create_info_ptr->n_unique_inputs  != 0) ? system_resizable_vector_create(in_create_info_ptr->n_unique_inputs)
                                                                     : nullptr;
        outputs        = (in_create_info_ptr->n_unique_outputs != 0) ? system_resizable_vector_create(in_create_info_ptr->n_unique_outputs)
                                                                     : nullptr;
        type           = RAL_PRESENT_TASK_TYPE_CPU_TASK;

        for (ral_present_task_io_type io_type = RAL_PRESENT_TASK_IO_TYPE_FIRST;
                                      io_type < RAL_PRESENT_TASK_IO_TYPE_COUNT;
                             ++((int&)io_type) )
        {
            const uint32_t             n_ios   = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? in_create_info_ptr->n_unique_inputs
                                                                                             : in_create_info_ptr->n_unique_outputs;
            const ral_present_task_io* src_ios = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? in_create_info_ptr->unique_inputs
                                                                                             : in_create_info_ptr->unique_outputs;
            system_resizable_vector    dst_ios = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? inputs
                                                                                             : outputs;

            for (uint32_t n_io = 0;
                          n_io < n_ios;
                        ++n_io)
            {
                ral_present_task_io* new_io_ptr = new ral_present_task_io;

                ASSERT_ALWAYS_SYNC(new_io_ptr != nullptr,
                                   "Out of memory");

                set_io(RAL_PRESENT_TASK_IO_TYPE_INPUT,
                       src_ios + n_io,
                       new_io_ptr);

                system_resizable_vector_push(dst_ios,
                                             new_io_ptr);
            }
        }
    }

    /** TODO. GPU tasks only */
    explicit _ral_present_task(system_hashed_ansi_string in_name,
                               ral_command_buffer        in_command_buffer)
    {
        ral_command_buffer_get_property(in_command_buffer,
                                        RAL_COMMAND_BUFFER_PROPERTY_CONTEXT,
                                       &context);

        command_buffer = in_command_buffer;
        inputs         = system_resizable_vector_create(4 /* capacity */);
        name           = in_name;
        outputs        = system_resizable_vector_create(4 /* capacity */);
        type           = RAL_PRESENT_TASK_TYPE_GPU_TASK;

        ral_context_retain_object(context,
                                  RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                  command_buffer);

        update_ios();
    }

    /** TODO */
    ~_ral_present_task()
    {
        system_resizable_vector* vectors_to_release[] =
        {
            &inputs,
            &outputs
        };
        const uint32_t n_vectors_to_release = sizeof(vectors_to_release) / sizeof(vectors_to_release[0]);

        for (uint32_t n_vector = 0;
                      n_vector < n_vectors_to_release;
                    ++n_vector)
        {
            system_resizable_vector* current_vector_ptr = vectors_to_release[n_vector];

            if (*current_vector_ptr != nullptr)
            {
                system_resizable_vector_release(*current_vector_ptr);

                *current_vector_ptr = nullptr;
            }
        }

        if (command_buffer != nullptr)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                       1, /* n_objects */
                                       (const void**) command_buffer);

            command_buffer = nullptr;
        }
    }

    void add_io(ral_present_task_io_type io_type,
                ral_buffer               buffer)
    {
        add_io<RAL_CONTEXT_OBJECT_TYPE_BUFFER>(io_type,
                                               buffer); 
    }

    void add_io(ral_present_task_io_type io_type,
                ral_texture              buffer)
    {
        add_io<RAL_CONTEXT_OBJECT_TYPE_TEXTURE>(io_type,
                                                buffer); 
    }

    bool is_io_defined(ral_present_task_io_type io_type,
                       ral_buffer               object)
    {
        return is_io_defined<RAL_CONTEXT_OBJECT_TYPE_BUFFER>(io_type,
                                                             object);
    }

    bool is_io_defined(ral_present_task_io_type io_type,
                       ral_texture              object)
    {
        return is_io_defined<RAL_CONTEXT_OBJECT_TYPE_TEXTURE>(io_type,
                                                              object);
    }

private:
    template<ral_context_object_type object_type,
             typename                ral_object_type>
    void add_io(ral_present_task_io_type io_type,
                ral_object_type          object)
    {
        system_resizable_vector ios         = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? inputs
                                                                                          : outputs;
        ral_present_task_io*    new_io_ptr;

        #ifdef _DEBUG
        {
            /* Sanity checks */
            ASSERT_DEBUG_SYNC(!is_io_defined(io_type, object),
                              "add_io() called for an IO which is already defined.");
        }
        #endif

        new_io_ptr = new ral_present_task_io;

        ASSERT_ALWAYS_SYNC(new_io_ptr != nullptr,
                           "Out of memory");

        new_io_ptr->object      = object;
        new_io_ptr->object_type = object_type;
    }

    template<ral_context_object_type object_type,
             typename                ral_object_type>

    bool is_io_defined(ral_present_task_io_type io_type,
                       ral_object_type          object)
    {
        bool                    result = false;
        system_resizable_vector ios    = (io_type == RAL_PRESENT_TASK_IO_TYPE_INPUT) ? inputs
                                                                                     : outputs;
        uint32_t                n_ios  = 0;

        system_resizable_vector_get_property(ios,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_ios);

        for (uint32_t n_io = 0;
                      n_io < n_ios;
                    ++n_io)
        {
            ral_present_task_io* current_io_ptr = nullptr;

            system_resizable_vector_get_element_at(ios,
                                                   n_io,
                                                  &current_io_ptr);

            if (current_io_ptr->object == object)
            {
                result = true;

                break;
            }
        }

        return result;
    }

    void update_ios_internal(ral_command_buffer in_command_buffer);
} _ral_present_task;


/** TODO */
void _ral_present_task::set_io(ral_present_task_io_type   type,
                               const ral_present_task_io* src_io_ptr,
                               ral_present_task_io*       dst_io_ptr)
{
    dst_io_ptr->object      = src_io_ptr->object;
    dst_io_ptr->object_type = src_io_ptr->object_type;

    /* TODO: Retain/release mechanism? */
}

/** TODO */
void _ral_present_task::update_ios()
{
    /* Sanity checks */
    {
        uint32_t n_recorded_read_accesses   = 0;
        uint32_t n_recorded_write_accesses  = 0;

        ASSERT_DEBUG_SYNC(type == RAL_PRESENT_TASK_TYPE_GPU_TASK,
                          "update_ios() should only be invoked for a GPU task.");

        system_resizable_vector_get_property(inputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_read_accesses);
        system_resizable_vector_get_property(outputs,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_write_accesses);

        ASSERT_DEBUG_SYNC(n_recorded_read_accesses  == 0 &&
                          n_recorded_write_accesses == 0,
                          "Invalid _ral_present_task::update_ios() request.");
    }

    update_ios_internal(command_buffer);
}

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

/** Please see header for specification */
PUBLIC ral_present_task ral_present_task_create_cpu(system_hashed_ansi_string               name,
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
PUBLIC ral_present_task ral_present_task_create_gpu(system_hashed_ansi_string name,
                                                    ral_command_buffer        command_buffer)
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

    if (command_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                          "RAL command buffer, specified for RAL present task, is NULL");

        goto end;
    }

    ral_command_buffer_get_property(command_buffer,
                                    RAL_COMMAND_BUFFER_PROPERTY_IS_RESETTABLE,
                                   &is_command_buffer_resettable);
    ral_command_buffer_get_property(command_buffer,
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

    /* Should be fine to create a present task from this command buffer */
    result_ptr = new _ral_present_task(name,
                                       command_buffer);

end:
    return (ral_present_task) result_ptr;
}

/** Please see header for specification */
PUBLIC bool ral_present_task_get_io_property(ral_present_task             task,
                                             ral_present_task_io_type     io_type,
                                             uint32_t                     n_io,
                                             ral_present_task_io_property property,
                                             void**                       out_result_ptr)
{
    ral_present_task_io*    io_ptr   = nullptr;
    system_resizable_vector ios      = nullptr;
    bool                    result   = false;
    _ral_present_task*      task_ptr = (_ral_present_task*) task;

    /* Sanity checks */
    if (task == nullptr)
    {
        ASSERT_DEBUG_SYNC(task != nullptr,
                          "Input ral_present_task instance is null");

        goto end;
    }

    switch (io_type)
    {
        case RAL_PRESENT_TASK_IO_TYPE_INPUT:
        {
            ios = task_ptr->inputs;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            ios = task_ptr->outputs;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid IO type requested.");

            goto end;
        }
    }


    if (ios == nullptr)
    {
        ASSERT_DEBUG_SYNC(ios != nullptr,
                          "Retrieved IO vector is null.");

        goto end;
    }

    if (!system_resizable_vector_get_element_at(ios,
                                                n_io,
                                                io_ptr) ||
        io_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve IO at index [%d]",
                          n_io);

        goto end;
    }

    switch (property)
    {
        case RAL_PRESENT_TASK_IO_PROPERTY_OBJECT:
        {
            *((void**) out_result_ptr) = io_ptr->object;

            break;
        }

        case RAL_PRESENT_TASK_IO_PROPERTY_OBJECT_TYPE:
        {
            *((ral_context_object_type*) out_result_ptr) = io_ptr->object_type;

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
PUBLIC void ral_present_task_get_property(ral_present_task          task,
                                          ral_present_task_property property,
                                          void*                     out_result_ptr)
{
    _ral_present_task* task_ptr = (_ral_present_task*) task;

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

            *(ral_command_buffer*) out_result_ptr = task_ptr->command_buffer;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_INPUTS:
        {
            system_resizable_vector_get_property(task_ptr->inputs,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_OUTPUTS:
        {
            system_resizable_vector_get_property(task_ptr->outputs,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = task_ptr->name;

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_TYPE:
        {
            *(ral_present_task_type*) out_result_ptr = task_ptr->type;

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
PUBLIC void ral_present_task_release(ral_present_task task)
{
    delete (_ral_present_task*) task;
}