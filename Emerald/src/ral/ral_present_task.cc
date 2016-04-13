#include "shared.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_present_task.h"
#include "ral/ral_texture_view.h"
#include "system/system_resizable_vector.h"


typedef struct _ral_present_task
{
    ral_command_buffer        command_buffer;
    ral_context               context;
    system_resizable_vector   modified_buffers;  /* holds ral_buffer instances  */
    system_resizable_vector   modified_textures; /* holds ral_texture instances */
    system_hashed_ansi_string name;
    system_resizable_vector   read_buffers;      /* holds ral_buffer instances  */
    system_resizable_vector   read_textures;     /* holds ral_buffer instances  */
    ral_present_task_type     type;

    void update_object_vectors();

    /** TODO */
    explicit _ral_present_task(system_hashed_ansi_string               in_name,
                               const ral_present_task_cpu_create_info* in_create_info_ptr)
    {
        command_buffer    = nullptr;
        context           = nullptr;
        modified_buffers  = (in_create_info_ptr->n_modified_buffers  != 0) ? system_resizable_vector_create(in_create_info_ptr->n_modified_buffers)
                                                                           : nullptr;
        modified_textures = (in_create_info_ptr->n_modified_textures != 0) ? system_resizable_vector_create(in_create_info_ptr->n_modified_textures)
                                                                           : nullptr;
        read_buffers      = (in_create_info_ptr->n_read_buffers      != 0) ? system_resizable_vector_create(in_create_info_ptr->n_read_buffers)
                                                                           : nullptr;
        read_textures     = (in_create_info_ptr->n_read_textures     != 0) ? system_resizable_vector_create(in_create_info_ptr->n_read_textures)
                                                                           : nullptr;
        type              = RAL_PRESENT_TASK_TYPE_CPU_TASK;

        for (uint32_t n_modified_buffer = 0;
                      n_modified_buffer < in_create_info_ptr->n_modified_buffers;
                    ++n_modified_buffer)
        {
            system_resizable_vector_push(modified_buffers,
                                         in_create_info_ptr->modified_buffers[n_modified_buffer]);
        }

        for (uint32_t n_modified_texture = 0;
                      n_modified_texture < in_create_info_ptr->n_modified_textures;
                    ++n_modified_texture)
        {
            system_resizable_vector_push(modified_textures,
                                         in_create_info_ptr->modified_textures[n_modified_texture]);
        }

        for (uint32_t n_read_buffer = 0;
                      n_read_buffer < in_create_info_ptr->n_read_buffers;
                    ++n_read_buffer)
        {
            system_resizable_vector_push(read_buffers,
                                         in_create_info_ptr->read_buffers[n_read_buffer]);
        }

        for (uint32_t n_read_texture = 0;
                      n_read_texture < in_create_info_ptr->n_read_textures;
                    ++n_read_texture)
        {
            system_resizable_vector_push(read_textures,
                                         in_create_info_ptr->read_textures[n_read_texture]);
        }
    }

    /** TODO. GPU tasks only */
    explicit _ral_present_task(system_hashed_ansi_string in_name,
                               ral_command_buffer        in_command_buffer)
    {
        ral_command_buffer_get_property(in_command_buffer,
                                        RAL_COMMAND_BUFFER_PROPERTY_CONTEXT,
                                       &context);

        command_buffer    = in_command_buffer;
        modified_buffers  = system_resizable_vector_create(4 /* capacity */);
        modified_textures = system_resizable_vector_create(4 /* capacity */);
        name              = in_name;
        read_buffers      = system_resizable_vector_create(4 /* capacity */);
        read_textures     = system_resizable_vector_create(4 /* capacity */);
        type              = RAL_PRESENT_TASK_TYPE_GPU_TASK;

        ral_context_retain_object(context,
                                  RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                  command_buffer);

        update_object_vectors();
    }

    /** TODO */
    ~_ral_present_task()
    {
        system_resizable_vector* vectors_to_release[] =
        {
            &modified_buffers,
            &modified_textures,
            &read_buffers,
            &read_textures
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

private:
    void update_object_vectors_internal(ral_command_buffer in_command_buffer);
} _ral_present_task;


/** TODO */
void _ral_present_task::update_object_vectors()
{
    /* Sanity checks */
    {
        uint32_t n_recorded_buffer_read_accesses   = 0;
        uint32_t n_recorded_buffer_write_accesses  = 0;
        uint32_t n_recorded_texture_read_accesses  = 0;
        uint32_t n_recorded_texture_write_accesses = 0;

        ASSERT_DEBUG_SYNC(type == RAL_PRESENT_TASK_TYPE_GPU_TASK,
                          "update_object_vectors() should only be invoked for a GPU task.");

        system_resizable_vector_get_property(modified_buffers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_buffer_write_accesses);
        system_resizable_vector_get_property(modified_textures,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_texture_write_accesses);
        system_resizable_vector_get_property(read_buffers,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_buffer_read_accesses);
        system_resizable_vector_get_property(read_textures,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_recorded_texture_read_accesses);

        ASSERT_DEBUG_SYNC(n_recorded_buffer_read_accesses   == 0 &&
                          n_recorded_buffer_write_accesses  == 0 &&
                          n_recorded_texture_read_accesses  == 0 &&
                          n_recorded_texture_write_accesses == 0,
                          "Invalid _ral_present_task::update_object_vectors() request.");
    }

    update_object_vectors_internal(command_buffer);
}

void _ral_present_task::update_object_vectors_internal(ral_command_buffer in_command_buffer)
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

                if (system_resizable_vector_find(read_textures,
                                                 command_ptr->src_texture) == ITEM_NOT_FOUND)
                {
                    system_resizable_vector_push(read_textures,
                                                 command_ptr->src_texture);
                }

                if (system_resizable_vector_find(modified_textures,
                                                 command_ptr->dst_texture) == ITEM_NOT_FOUND)
                {
                    system_resizable_vector_push(modified_textures,
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

                        if (system_resizable_vector_find(modified_buffers,
                                                         command_ptr->index_buffer) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(modified_buffers,
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

                        if (system_resizable_vector_find(modified_buffers,
                                                         command_ptr->buffer) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(modified_buffers,
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

                for (uint32_t n_read_dep = 0;
                              n_read_dep < n_read_accesses;
                            ++n_read_dep)
                {
                    const ral_object_access& current_access = read_access_ptr[n_read_dep];

                    if (current_access.buffer != nullptr)
                    {
                        if (system_resizable_vector_find(read_buffers,
                                                         current_access.buffer) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(read_buffers,
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

                        if (system_resizable_vector_find(read_textures,
                                                         parent_texture) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(read_textures,
                                                         parent_texture);
                        }
                    }
                }

                for (uint32_t n_write_dep = 0;
                              n_write_dep < n_write_accesses;
                            ++n_write_dep)
                {
                    const ral_object_access& current_access = write_access_ptr[n_write_dep];

                    if (current_access.buffer != nullptr)
                    {
                        if (system_resizable_vector_find(modified_buffers,
                                                         current_access.buffer) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(modified_buffers,
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

                        if (system_resizable_vector_find(modified_textures,
                                                         parent_texture) == ITEM_NOT_FOUND)
                        {
                            system_resizable_vector_push(modified_textures,
                                                         parent_texture);
                        }
                    }
                }

                break;
            }

            case RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                ral_command_buffer_execute_command_buffer_command_info* command_ptr = (ral_command_buffer_execute_command_buffer_command_info*) command_raw_ptr;

                update_object_vectors_internal(command_ptr->command_buffer);

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

                if (system_resizable_vector_find(modified_textures,
                                                 parent_texture) == ITEM_NOT_FOUND)
                {
                    system_resizable_vector_push(modified_textures,
                                                 parent_texture);
                }

                break;
            }

            case RAL_COMMAND_TYPE_SET_VERTEX_ATTRIBUTE:
            {
                ral_command_buffer_set_vertex_attribute_command_info* command_ptr = (ral_command_buffer_set_vertex_attribute_command_info*) command_raw_ptr;

                if (system_resizable_vector_find(modified_buffers,
                                                 command_ptr->buffer) == ITEM_NOT_FOUND)
                {
                    system_resizable_vector_push(modified_buffers,
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
ral_present_task ral_present_task_create_cpu(system_hashed_ansi_string               name,
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

    if (create_info_ptr->n_modified_buffers != 0      &&
        create_info_ptr->modified_buffers   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_modified_buffers != 0      &&
                            create_info_ptr->modified_buffers   == nullptr),
                          "Null modified buffer array specified for n_modified_buffers != 0");

        goto end;
    }

    if (create_info_ptr->n_modified_textures != 0      &&
        create_info_ptr->modified_textures   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_modified_textures != 0      &&
                            create_info_ptr->modified_textures   == nullptr),
                          "Null modified texture array specified for n_modified_textures != 0");

        goto end;
    }

    if (create_info_ptr->n_read_buffers != 0      &&
        create_info_ptr->read_buffers   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_read_buffers != 0      &&
                            create_info_ptr->read_buffers   == nullptr),
                          "Null read buffer array specified for n_read_buffers != 0");

        goto end;
    }

    if (create_info_ptr->n_read_textures != 0      &&
        create_info_ptr->read_textures   == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(create_info_ptr->n_read_textures != 0      &&
                            create_info_ptr->read_textures   == nullptr),
                          "Null read texture array specified for n_read_textures != 0");

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
ral_present_task ral_present_task_create_gpu(system_hashed_ansi_string name,
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
bool ral_present_task_get_modified_object(ral_present_task             task,
                                          ral_context_object_type      object_type,
                                          ral_present_task_access_type access_type,
                                          uint32_t                     n_object,
                                          void**                       out_object_ptr)
{
    system_resizable_vector object_vector = nullptr;
    bool                    result        = false;
    _ral_present_task*      task_ptr      = (_ral_present_task*) task;

    /* Sanity checks */
    if (task == nullptr)
    {
        ASSERT_DEBUG_SYNC(task != nullptr,
                          "Input ral_present_task instance is null");

        goto end;
    }

    switch (object_type)
    {
        case RAL_CONTEXT_OBJECT_TYPE_BUFFER:
        {
            object_vector = (access_type == RAL_PRESENT_TASK_ACCESS_TYPE_READ)  ? task_ptr->read_buffers
                          : (access_type == RAL_PRESENT_TASK_ACCESS_TYPE_WRITE) ? task_ptr->modified_buffers
                                                                                : nullptr;

            break;
        }

        case RAL_CONTEXT_OBJECT_TYPE_TEXTURE:
        {
            object_vector = (access_type == RAL_PRESENT_TASK_ACCESS_TYPE_READ)  ? task_ptr->read_textures
                          : (access_type == RAL_PRESENT_TASK_ACCESS_TYPE_WRITE) ? task_ptr->modified_textures
                                                                                : nullptr;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid object type requested.");

            goto end;
        }
    }


    if (object_vector == nullptr)
    {
        ASSERT_DEBUG_SYNC(object_vector != nullptr,
                          "Retrieved object vector is null.");

        goto end;
    }

    if (!system_resizable_vector_get_element_at(object_vector,
                                                n_object,
                                                out_object_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve object at index [%d]",
                          n_object);

        goto end;
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
void ral_present_task_get_property(ral_present_task          task,
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

        case RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_BUFFERS:
        {
            system_resizable_vector_get_property(task_ptr->modified_buffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_MODIFIED_TEXTURES:
        {
            system_resizable_vector_get_property(task_ptr->modified_textures,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_READ_BUFFERS:
        {
            system_resizable_vector_get_property(task_ptr->read_buffers,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PRESENT_TASK_PROPERTY_N_READ_TEXTURES:
        {
            system_resizable_vector_get_property(task_ptr->read_textures,
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
void ral_present_task_release(ral_present_task task)
{
    delete (_ral_present_task*) task;
}