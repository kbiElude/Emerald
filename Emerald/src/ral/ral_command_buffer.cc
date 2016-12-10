/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_gfx_state.h"
#include "ral/ral_program.h"
#include "ral/ral_sampler.h"
#include "ral/ral_shader.h"
#include "ral/ral_texture.h"
#include "ral/ral_texture_view.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

#define N_MAX_PREALLOCED_COMMANDS (32)


PRIVATE system_resource_pool command_buffer_pool = nullptr; /* holds _ral_command_buffer instances */
PRIVATE system_resource_pool command_pool        = nullptr; /* holds _ral_command instances        */


typedef struct _ral_command_buffer
{
    system_callback_manager   callback_manager;
    system_resizable_vector   commands;
    ral_queue_bits            compatible_queues;
    ral_context               context;
    bool                      is_invokable_from_other_command_buffers;
    bool                      is_resettable;
    bool                      is_transient;
    ral_command_buffer_status status;

    void clear_commands();
} _ral_command_buffer;

typedef struct _ral_command
{
    ral_command_type type;

    union
    {
        ral_command_buffer_clear_rt_binding_command_info        clear_rt_binding_command;
        ral_command_buffer_copy_buffer_to_buffer_command_info   copy_buffer_to_buffer_command;
        ral_command_buffer_copy_texture_to_texture_command_info copy_texture_to_texture_command;
        ral_command_buffer_dispatch_command_info                dispatch_command;
        ral_command_buffer_draw_call_indexed_command_info       draw_call_indexed_command;
        ral_command_buffer_draw_call_indirect_command_info      draw_call_indirect_command;
        ral_command_buffer_draw_call_regular_command_info       draw_call_regular_command;
        ral_command_buffer_execute_command_buffer_command_info  execute_command_buffer_command;
        ral_command_buffer_fill_buffer_command_info             fill_buffer_command;
        ral_command_buffer_invalidate_texture_command_info      invalidate_texture_command;
        ral_command_buffer_set_binding_command_info             set_binding_command;
        ral_command_buffer_set_gfx_state_command_info           set_gfx_state_command;
        ral_command_buffer_set_program_command_info             set_program_command;
        ral_command_buffer_set_color_rendertarget_command_info  set_color_rendertarget_command;
        ral_command_buffer_set_depth_rendertarget_command_info  set_depth_rendertarget_command;
        ral_command_buffer_set_scissor_box_command_info         set_scissor_box_command;
        ral_command_buffer_set_vertex_buffer_command_info       set_vertex_buffer_command;
        ral_command_buffer_set_viewport_command_info            set_viewport_command;
        ral_command_buffer_update_buffer_command_info           update_buffer_command;
    };

    void deinit()
    {
        if (type != RAL_COMMAND_TYPE_UNKNOWN)
        {
            update_reference_counters(false);

            type = RAL_COMMAND_TYPE_UNKNOWN;
        }
    }

    void init()
    {
        update_reference_counters(true);
    }

private:
    void update_reference_counters(bool is_init_op)
    {
        bool (*pfn_update_ref_proc)(ral_context, ral_context_object_type, uint32_t, void* const* objects);

        pfn_update_ref_proc = (is_init_op) ? ral_context_retain_objects
                                           : ral_context_delete_objects;

        switch (type)
        {
            case RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE:
            {
                ral_context dst_texture_view_context = nullptr;
                ral_context src_texture_view_context = nullptr;

                ral_texture_view_get_property(copy_texture_to_texture_command.dst_texture_view,
                                              RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                             &dst_texture_view_context);
                ral_texture_view_get_property(copy_texture_to_texture_command.src_texture_view,
                                              RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                             &src_texture_view_context);

                ral_texture_view texture_views[] =
                {
                    copy_texture_to_texture_command.dst_texture_view,
                    copy_texture_to_texture_command.src_texture_view
                };
                const uint32_t n_texture_views = sizeof(texture_views) / sizeof(texture_views[0]);

                pfn_update_ref_proc(dst_texture_view_context,
                                    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                    n_texture_views,
                                    reinterpret_cast<void* const*>(texture_views) );

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDEXED:
            {
                ral_context index_buffer_context = nullptr;

                ral_buffer_get_property(draw_call_indexed_command.index_buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &index_buffer_context);

                pfn_update_ref_proc(index_buffer_context,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&draw_call_indexed_command.index_buffer) );

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property(draw_call_indirect_command.indirect_buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                pfn_update_ref_proc(buffer_context,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&draw_call_indirect_command.indirect_buffer) );

                break;
            }

            case RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                pfn_update_ref_proc((reinterpret_cast<_ral_command_buffer*>(execute_command_buffer_command.command_buffer))->context,
                                    RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&execute_command_buffer_command.command_buffer) );

                break;
            }

            case RAL_COMMAND_TYPE_FILL_BUFFER:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property(fill_buffer_command.buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                pfn_update_ref_proc(buffer_context,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&fill_buffer_command.buffer) );

                break;
            }

            case RAL_COMMAND_TYPE_INVALIDATE_TEXTURE:
            {
                ral_context texture_context = nullptr;

                ral_texture_get_property(invalidate_texture_command.texture,
                                         RAL_TEXTURE_PROPERTY_CONTEXT,
                                        &texture_context);

                pfn_update_ref_proc(texture_context,
                                    RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&invalidate_texture_command.texture) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_BINDING:
            {
                switch (set_binding_command.binding_type)
                {
                    case RAL_BINDING_TYPE_RENDERTARGET:
                    {
                        /* Nop */
                        break;
                    }

                    case RAL_BINDING_TYPE_SAMPLED_IMAGE:
                    case RAL_BINDING_TYPE_STORAGE_IMAGE:
                    {
                        const ral_texture_view texture_view         = (set_binding_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE) ? set_binding_command.sampled_image_binding.texture_view
                                                                                                                                           : set_binding_command.storage_image_binding.texture_view;
                        ral_context            texture_view_context = nullptr;

                        ral_texture_view_get_property(texture_view,
                                                      RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                                     &texture_view_context);

                        pfn_update_ref_proc(texture_view_context,
                                            RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                            1, /* n_objects */
                                            reinterpret_cast<void* const*>(&texture_view) );

                        if (set_binding_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE)
                        {
                            ral_context sampler_context = nullptr;

                            ral_sampler_get_property(set_binding_command.sampled_image_binding.sampler,
                                                     RAL_SAMPLER_PROPERTY_CONTEXT,
                                                    &sampler_context);

                            pfn_update_ref_proc(sampler_context,
                                                RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                                1, /* n_objects */
                                                reinterpret_cast<void* const*>(&set_binding_command.sampled_image_binding.sampler) );
                        }

                        break;
                    }

                    case RAL_BINDING_TYPE_STORAGE_BUFFER:
                    case RAL_BINDING_TYPE_UNIFORM_BUFFER:
                    {
                        const ral_buffer buffer         = (set_binding_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? set_binding_command.storage_buffer_binding.buffer
                                                                                                                                : set_binding_command.uniform_buffer_binding.buffer;
                        ral_context      buffer_context = nullptr;

                        ral_buffer_get_property(buffer,
                                                RAL_BUFFER_PROPERTY_CONTEXT,
                                               &buffer_context);

                        pfn_update_ref_proc(buffer_context,
                                            RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                            1, /* n_objects */
                                            reinterpret_cast<void* const*>(&buffer) );

                        break;
                    }

                    default:
                    {
                        ASSERT_DEBUG_SYNC(false,
                                          "Unrecognized binding type");
                    }

                    break;
                }

                break;
            }

            case RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET:
            {
                ral_context texture_view_context = nullptr;

                ral_texture_view_get_property(set_color_rendertarget_command.texture_view,
                                              RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                             &texture_view_context);

                pfn_update_ref_proc(texture_view_context,
                                    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&set_color_rendertarget_command.texture_view) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_DEPTH_RENDERTARGET:
            {
                ral_context rt_context = nullptr;

                ral_texture_view_get_property(set_depth_rendertarget_command.depth_rt,
                                              RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                             &rt_context);

                pfn_update_ref_proc(rt_context,
                                    RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&set_depth_rendertarget_command.depth_rt) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_GFX_STATE:
            {
                ral_context gfx_state_context = nullptr;

                ral_gfx_state_get_property(set_gfx_state_command.new_state,
                                           RAL_GFX_STATE_PROPERTY_CONTEXT,
                                          &gfx_state_context);

                pfn_update_ref_proc(gfx_state_context,
                                    RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&set_gfx_state_command.new_state) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_PROGRAM:
            {
                ral_context program_context = nullptr;

                ral_program_get_property(set_program_command.new_program,
                                         RAL_PROGRAM_PROPERTY_CONTEXT,
                                        &program_context);

                pfn_update_ref_proc(program_context,
                                    RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&set_program_command.new_program) );

                break;
            }

            case RAL_COMMAND_TYPE_SET_VERTEX_BUFFER:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property(set_vertex_buffer_command.buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                pfn_update_ref_proc(buffer_context,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&set_vertex_buffer_command.buffer) );

                break;
            }

            case RAL_COMMAND_TYPE_UPDATE_BUFFER:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property(update_buffer_command.buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                pfn_update_ref_proc(buffer_context,
                                    RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                    1, /* n_objects */
                                    reinterpret_cast<void* const*>(&update_buffer_command.buffer) );

                break;
            }

            /* Dummy */
            case RAL_COMMAND_TYPE_CLEAR_RT_BINDING:
            case RAL_COMMAND_TYPE_DISPATCH:
            case RAL_COMMAND_TYPE_DRAW_CALL_REGULAR:
            case RAL_COMMAND_TYPE_SET_SCISSOR_BOX:
            case RAL_COMMAND_TYPE_SET_VIEWPORT:
            {
                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized ral_command type.");
            }
        }
    }
} _ral_command;


/** TODO */
void _ral_command_buffer::clear_commands()
{
    _ral_command* current_command_ptr = nullptr;

    while (system_resizable_vector_pop(commands,
                                      &current_command_ptr) )
    {
        current_command_ptr->deinit();

        system_resource_pool_return_to_pool(command_pool,
                                            (system_resource_pool_block) current_command_ptr);
    }
}


/** TODO */
PRIVATE void _ral_command_buffer_deinit_command(system_resource_pool_block block)
{
    _ral_command* command_ptr = reinterpret_cast<_ral_command*>(block);

    command_ptr->deinit();
}

/** TODO */
PRIVATE void _ral_command_buffer_deinit_command_buffer(system_resource_pool_block block)
{
    _ral_command_buffer* cmd_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(block); 

    if (cmd_buffer_ptr->callback_manager != nullptr)
    {
        system_callback_manager_release(cmd_buffer_ptr->callback_manager);

        cmd_buffer_ptr->callback_manager = nullptr;
    }

    if (cmd_buffer_ptr->commands != nullptr)
    {
        cmd_buffer_ptr->clear_commands();

        system_resizable_vector_release(cmd_buffer_ptr->commands);

        cmd_buffer_ptr->commands = nullptr;
    }
}

/** TODO */
PRIVATE void _ral_command_buffer_init_command_buffer(system_resource_pool_block block)
{
    _ral_command_buffer* cmd_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(block); 

    cmd_buffer_ptr->callback_manager = system_callback_manager_create((_callback_id) RAL_COMMAND_BUFFER_CALLBACK_ID_COUNT);
    cmd_buffer_ptr->commands         = system_resizable_vector_create(N_MAX_PREALLOCED_COMMANDS);
    cmd_buffer_ptr->context          = nullptr;
}


/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_append_commands_from_command_buffer(ral_command_buffer recording_command_buffer,
                                                                               ral_command_buffer src_command_buffer,
                                                                               uint32_t           n_start_command,
                                                                               uint32_t           n_commands)
{
    uint32_t             n_src_commands               = 0;
    _ral_command_buffer* recording_command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command_buffer* src_command_buffer_ptr       = reinterpret_cast<_ral_command_buffer*>(src_command_buffer);

    /* Sanity checks */
    if (recording_command_buffer == nullptr ||
        src_command_buffer       == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(recording_command_buffer == nullptr ||
                            src_command_buffer       == nullptr),
                          "At least one of the input command buffers is null");

        goto end;
    }

    if (recording_command_buffer == src_command_buffer)
    {
        ASSERT_DEBUG_SYNC(!(recording_command_buffer == src_command_buffer),
                          "Source and target command buffers are exactly the same.");

        goto end;
    }

    if (recording_command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(!(recording_command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING),
                          "Target command buffer is not in the recording state");

        goto end;
    }

    if ((recording_command_buffer_ptr->compatible_queues & src_command_buffer_ptr->compatible_queues) != recording_command_buffer_ptr->compatible_queues)
    {
        ASSERT_DEBUG_SYNC(!((recording_command_buffer_ptr->compatible_queues & src_command_buffer_ptr->compatible_queues) != recording_command_buffer_ptr->compatible_queues),
                          "Target command buffer's queue set is not a sub-set of the source command buffer's queue set");
        
        goto end;
    }

    /* Copy all commands from the source command buffer */
    system_resizable_vector_get_property(src_command_buffer_ptr->commands,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_commands);

    for (uint32_t n_src_command = 0;
                  n_src_command < n_src_commands;
                ++n_src_command)
    {
        _ral_command* new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );
        _ral_command* src_command_ptr = nullptr;

        system_resizable_vector_get_element_at(src_command_buffer_ptr->commands,
                                               n_src_command,
                                              &src_command_ptr);

        /* TODO: The "update buffer" command may use heap-allocated memory if the data chunk's size exceeds the predefined
         *       size. If we append such command to another cmd buffer and later release the source command buffer, that chunk
         *       will have been freed when it's accessed while processing the command from the other command buffer. The
         *       existing solution needs to be redesigned to take such cases into account (move to shared pointers? copy constructors?)
         **/
        ASSERT_DEBUG_SYNC(src_command_ptr->type != RAL_COMMAND_TYPE_UPDATE_BUFFER,
                          "TODO");

        memcpy(new_command_ptr,
               src_command_ptr,
               sizeof(*src_command_ptr) );

        new_command_ptr->init();

        system_resizable_vector_push(recording_command_buffer_ptr->commands,
                                     new_command_ptr);
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context                           context,
                                                    const ral_command_buffer_create_info* create_info_ptr)
{
    _ral_command_buffer* new_command_buffer_ptr = nullptr;

    /* Sanity checks */
    if (context == nullptr)
    {
        ASSERT_DEBUG_SYNC(context != nullptr,
                          "Input context is NULL");

        goto end;
    }

    if (create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(create_info_ptr != nullptr,
                          "Input ral_command_buffer_create_info instance is NULL");

        goto end;
    }

    /* Carry on */
    new_command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(system_resource_pool_get_from_pool(command_buffer_pool) );

    if (new_command_buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(new_command_buffer_ptr != nullptr,
                          "Could not retrieve a command buffer from the command buffer pool");

        goto end;
    }

    new_command_buffer_ptr->clear_commands();

    new_command_buffer_ptr->compatible_queues                       = create_info_ptr->compatible_queues;
    new_command_buffer_ptr->context                                 = context;
    new_command_buffer_ptr->is_invokable_from_other_command_buffers = create_info_ptr->is_invokable_from_other_command_buffers;
    new_command_buffer_ptr->is_resettable                           = create_info_ptr->is_resettable;
    new_command_buffer_ptr->is_transient                            = create_info_ptr->is_transient;

end:
    return reinterpret_cast<ral_command_buffer>(new_command_buffer_ptr);
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_deinit()
{
    system_resource_pool_release(command_buffer_pool);
    system_resource_pool_release(command_pool);

    command_buffer_pool = nullptr;
    command_pool        = nullptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_get_property(ral_command_buffer          command_buffer,
                                                        ral_command_buffer_property property,
                                                        void*                       out_result_ptr)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(command_buffer);

    switch (property)
    {
        case RAL_COMMAND_BUFFER_PROPERTY_CALLBACK_MANAGER:
        {
            *reinterpret_cast<system_callback_manager*>(out_result_ptr) = command_buffer_ptr->callback_manager;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_COMPATIBLE_QUEUES:
        {
            *reinterpret_cast<ral_queue_bits*>(out_result_ptr) = command_buffer_ptr->compatible_queues;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_CONTEXT:
        {
            *reinterpret_cast<ral_context*>(out_result_ptr) = command_buffer_ptr->context;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_INVOKABLE_FROM_OTHER_COMMAND_BUFFERS:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = command_buffer_ptr->is_invokable_from_other_command_buffers;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_RESETTABLE:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = command_buffer_ptr->is_resettable;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_TRANSIENT:
        {
            *reinterpret_cast<bool*>(out_result_ptr) = command_buffer_ptr->is_transient;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_N_RECORDED_COMMANDS:
        {
            system_resizable_vector_get_property(command_buffer_ptr->commands,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_STATUS:
        {
            *reinterpret_cast<ral_command_buffer_status*>(out_result_ptr) = command_buffer_ptr->status;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_command_buffer_property value");
        }
    }
}

/** Please see header for specification */
PUBLIC bool ral_command_buffer_get_recorded_command(ral_command_buffer command_buffer,
                                                    uint32_t           n_command,
                                                    ral_command_type*  out_command_type_ptr,
                                                    const void**       out_command_ptr_ptr)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(command_buffer);
    _ral_command*        command_ptr        = nullptr;
    bool                 result             = false;

    /* Sanity checks */
    if (command_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                          "Input RAL command buffer instance is NULL");

        goto end;
    }

    if (!system_resizable_vector_get_element_at(command_buffer_ptr->commands,
                                                n_command,
                                               &command_ptr) )
    {
        ASSERT_DEBUG_SYNC(command_ptr != nullptr,
                          "No command descriptor found at command index [%d]",
                          n_command);

        goto end;
    }

    /* Go for it
     *
     * Note: command descriptors are all part of the same union, so it doesn't matter
     *       which command descriptor we return the pointer to.
     **/
    *out_command_type_ptr = command_ptr->type;
    *out_command_ptr_ptr  = &command_ptr->copy_texture_to_texture_command;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_init()
{
    command_buffer_pool = system_resource_pool_create(sizeof(_ral_command_buffer),
                                                      32 /* n_elements_to_preallocate */,
                                                      _ral_command_buffer_init_command_buffer,
                                                      _ral_command_buffer_deinit_command_buffer);
    command_pool        = system_resource_pool_create(sizeof(_ral_command),
                                                      32,                                  /* n_elements_to_preallocate */
                                                      nullptr,                             /* init_fn                   */
                                                      _ral_command_buffer_deinit_command); /* deinit_fn                 */

    ASSERT_DEBUG_SYNC(command_buffer_pool != nullptr,
                      "Could not create a command buffer pool");
    ASSERT_DEBUG_SYNC(command_pool != nullptr,
                      "Could not create a command pool");
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_insert_commands_from_command_buffer(ral_command_buffer       dst_command_buffer,
                                                                               uint32_t                 n_command_to_insert_before,
                                                                               const ral_command_buffer src_command_buffer,
                                                                               uint32_t                 n_start_command,
                                                                               uint32_t                 n_commands_to_insert)
{
    _ral_command_buffer*       dst_command_buffer_ptr        = reinterpret_cast<_ral_command_buffer*>      (dst_command_buffer);
    bool                       is_append_op                  = false;
    uint32_t                   n_dst_command_buffer_commands = 0;
    uint32_t                   n_src_command_buffer_commands = 0;
    const _ral_command_buffer* src_command_buffer_ptr        = reinterpret_cast<const _ral_command_buffer*>(src_command_buffer);

    /* Sanity checks */
    if (dst_command_buffer == nullptr ||
        src_command_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(dst_command_buffer == nullptr ||
                            src_command_buffer == nullptr),
                          "At least one of the input command buffers is null");

        goto end;
    }

    if (dst_command_buffer == src_command_buffer)
    {
        ASSERT_DEBUG_SYNC(!(dst_command_buffer == src_command_buffer),
                          "Source and target command buffers are exactly the same.");

        goto end;
    }

    if (dst_command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(!(dst_command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING),
                          "Target command buffer is not in the recording state");

        goto end;
    }

    if ((dst_command_buffer_ptr->compatible_queues & src_command_buffer_ptr->compatible_queues) != dst_command_buffer_ptr->compatible_queues)
    {
        ASSERT_DEBUG_SYNC(!((dst_command_buffer_ptr->compatible_queues & src_command_buffer_ptr->compatible_queues) != dst_command_buffer_ptr->compatible_queues),
                          "Target command buffer's queue set is not a sub-set of the source command buffer's queue set");

        goto end;
    }

    system_resizable_vector_get_property(dst_command_buffer_ptr->commands,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_dst_command_buffer_commands);
    system_resizable_vector_get_property(src_command_buffer_ptr->commands,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_src_command_buffer_commands);

    if (n_command_to_insert_before > n_dst_command_buffer_commands)
    {
        ASSERT_DEBUG_SYNC(!(n_command_to_insert_before > n_dst_command_buffer_commands),
                          "Invalid n_command_to_insert_before arg value");

        goto end;
    }

    if (n_commands_to_insert > n_start_command + n_src_command_buffer_commands)
    {
        ASSERT_DEBUG_SYNC(!(n_commands_to_insert > n_start_command + n_src_command_buffer_commands),
                          "Source command buffer does not hold enough commands to satisfy the requested insert op.");

        goto end;
    }

    is_append_op = (n_command_to_insert_before == n_dst_command_buffer_commands);

    for (uint32_t n_src_command = n_start_command;
                  n_src_command < n_start_command + n_src_command_buffer_commands;
                ++n_src_command)
    {
        _ral_command* new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );
        _ral_command* src_command_ptr = nullptr;

        system_resizable_vector_get_element_at(src_command_buffer_ptr->commands,
                                               n_src_command,
                                              &src_command_ptr);

        /* TODO: The "update buffer" command may use heap-allocated memory if the data chunk's size exceeds the predefined
         *       size. If we append such command to another cmd buffer and later release the source command buffer, that chunk
         *       will have been freed when it's accessed while processing the command from the other command buffer. The
         *       existing solution needs to be redesigned to take such cases into account (move to shared pointers? copy constructors?)
         **/
        ASSERT_DEBUG_SYNC(src_command_ptr->type != RAL_COMMAND_TYPE_UPDATE_BUFFER,
                          "TODO");

        memcpy(new_command_ptr,
               src_command_ptr,
               sizeof(*src_command_ptr) );

        new_command_ptr->init();

        if (is_append_op)
        {
            system_resizable_vector_push(dst_command_buffer_ptr->commands,
                                         new_command_ptr);
        }
        else
        {
            system_resizable_vector_insert_element_at(dst_command_buffer_ptr->commands,
                                                      n_command_to_insert_before + (n_src_command - n_start_command),
                                                      new_command_ptr);
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_clear_rendertarget_binding(ral_command_buffer                                      recording_command_buffer,
                                                                             uint32_t                                                n_clear_ops,
                                                                             const ral_command_buffer_clear_rt_binding_command_info* clear_op_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    if (command_buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr != nullptr,
                          "Input command buffer is null");

        goto end;
    }

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                          "Command buffer not in recording status");

        goto end;
    }

    for (uint32_t n_clear_op = 0;
                  n_clear_op < n_clear_ops;
                ++n_clear_op)
    {
        bool                                                    is_command_valid = true;
        const ral_command_buffer_clear_rt_binding_command_info& src_command      = clear_op_ptrs[n_clear_op];

        if (src_command.n_clear_regions >= N_MAX_CLEAR_REGIONS)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Invalid number of clear regions requested.");

            is_command_valid = false;
        }

        for (uint32_t n_rendertarget = 0;
                      n_rendertarget < src_command.n_rendertargets && is_command_valid;
                    ++n_rendertarget)
        {
            const ral_command_buffer_clear_rt_binding_rendertarget& current_rt = src_command.rendertargets[n_rendertarget];

            if (current_rt.aspect   == RAL_TEXTURE_ASPECT_COLOR_BIT &&
                current_rt.rt_index >= N_MAX_CLEAR_RENDERTARGETS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid rendertarget index specified");

                is_command_valid = false;

                break;
            }
        }

        if (is_command_valid)
        {
            new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

            memcpy(new_command_ptr->clear_rt_binding_command.clear_regions,
                   src_command.clear_regions,
                   src_command.n_clear_regions * sizeof(src_command.clear_regions[0]) );
            memcpy(new_command_ptr->clear_rt_binding_command.rendertargets,
                   src_command.rendertargets,
                   src_command.n_rendertargets * sizeof(src_command.rendertargets[0]) );

            new_command_ptr->clear_rt_binding_command.n_clear_regions = src_command.n_clear_regions;
            new_command_ptr->clear_rt_binding_command.n_rendertargets = src_command.n_rendertargets;
            new_command_ptr->type                                     = RAL_COMMAND_TYPE_CLEAR_RT_BINDING;

            new_command_ptr->init();

            system_resizable_vector_push(command_buffer_ptr->commands,
                                         new_command_ptr);
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_copy_buffer_to_buffer(ral_command_buffer                                           recording_command_buffer,
                                                                        uint32_t                                                     n_copy_ops,
                                                                        const ral_command_buffer_copy_buffer_to_buffer_command_info* copy_op_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    if (command_buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr != nullptr,
                          "Input command buffer is null");

        goto end;
    }

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                          "Command buffer not in recording status");

        goto end;
    }

    for (uint32_t n_copy_op = 0;
                  n_copy_op < n_copy_ops;
                ++n_copy_op)
    {
        const ral_command_buffer_copy_buffer_to_buffer_command_info& src_command = copy_op_ptrs[n_copy_op];

        #ifdef _DEBUG
        {
            uint32_t dst_buffer_size         = 0;
            uint32_t dst_buffer_start_offset = 0;
            uint32_t src_buffer_size         = 0;
            uint32_t src_buffer_start_offset = 0;

            if (src_command.dst_buffer == nullptr ||
                src_command.src_buffer == nullptr)
            {
                ASSERT_DEBUG_SYNC(!(src_command.dst_buffer == nullptr ||
                                    src_command.src_buffer == nullptr),
                                  "Destination/source RAL buffer, specified for a buffer->buffer copy op, is null");

                continue;
            }

            ral_buffer_get_property(src_command.dst_buffer,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &dst_buffer_size);
            ral_buffer_get_property(src_command.dst_buffer,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &dst_buffer_start_offset);
            ral_buffer_get_property(src_command.src_buffer,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &src_buffer_size);
            ral_buffer_get_property(src_command.src_buffer,
                                    RAL_BUFFER_PROPERTY_START_OFFSET,
                                   &src_buffer_start_offset);

            if (src_command.dst_buffer_start_offset + dst_buffer_start_offset + src_command.size > dst_buffer_size ||
                src_command.src_buffer_start_offset + src_buffer_start_offset + src_command.size > src_buffer_size)
            {
                ASSERT_DEBUG_SYNC(!(src_command.dst_buffer_start_offset + src_command.size > dst_buffer_size ||
                                    src_command.src_buffer_start_offset + src_command.size > src_buffer_size),
                                  "Data region exceeds destination / source buffer boundary");

                continue;
            }

            /* TODO: Region overlapping test if dst_buffer == src_buffer */
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_command_ptr->copy_buffer_to_buffer_command = copy_op_ptrs[n_copy_op];
        new_command_ptr->type                          = RAL_COMMAND_TYPE_COPY_BUFFER_TO_BUFFER;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_copy_texture_to_texture(ral_command_buffer                                             recording_command_buffer,
                                                                          uint32_t                                                       n_copy_ops,
                                                                          const ral_command_buffer_copy_texture_to_texture_command_info* copy_op_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    if (command_buffer_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr != nullptr,
                          "Input command buffer is null");

        goto end;
    }

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                          "Command buffer not in recording status");

        goto end;
    }

    for (uint32_t n_copy_op = 0;
                  n_copy_op < n_copy_ops;
                ++n_copy_op)
    {
        const ral_command_buffer_copy_texture_to_texture_command_info& src_command = copy_op_ptrs[n_copy_op];

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.dst_size)      == sizeof(src_command.dst_size),      "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.dst_start_xyz) == sizeof(src_command.dst_start_xyz), "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.src_size)      == sizeof(src_command.src_size),      "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.src_start_xyz) == sizeof(src_command.src_start_xyz), "");

        #ifdef _DEBUG
        {
            uint32_t   dst_mip_size[3]       = {0};
            ral_format dst_texture_view_format;
            uint32_t   dst_texture_view_n_layers  =  0;
            uint32_t   dst_texture_view_n_mipmaps =  0;
            uint32_t   src_mip_size[3]       = {0};
            ral_format src_texture_view_format;
            uint32_t   src_texture_view_n_layers  =  0;
            uint32_t   src_texture_view_n_mipmaps =  0;

            /* Sanity checks */
            if ( src_command.scaling_filter                                      == RAL_TEXTURE_FILTER_LINEAR &&
                (command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) == 0)
            {
                ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) != 0,
                                  "Scaling texture->texture copy requires a graphics queue command buffer.");

                continue;
            }

            if (src_command.dst_texture_view == nullptr)
            {
                ASSERT_DEBUG_SYNC(src_command.dst_texture_view != nullptr,
                                  "Destination texture view is null");

                continue;
            }

            if (src_command.src_texture_view == nullptr)
            {
                ASSERT_DEBUG_SYNC(src_command.src_texture_view != nullptr,
                                  "Source texture view is null");

                continue;
            }

            if (!(src_command.scaling_filter == RAL_TEXTURE_FILTER_LINEAR ||
                  src_command.scaling_filter == RAL_TEXTURE_FILTER_NEAREST) )
            {
                ASSERT_DEBUG_SYNC(src_command.scaling_filter == RAL_TEXTURE_FILTER_LINEAR ||
                                  src_command.scaling_filter == RAL_TEXTURE_FILTER_NEAREST,
                                  "Unrecognized scaling filter setting value");

                continue;
            }

            ral_texture_view_get_property(src_command.dst_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                         &dst_texture_view_format);
            ral_texture_view_get_property(src_command.dst_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &dst_texture_view_n_layers);
            ral_texture_view_get_property(src_command.src_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                         &src_texture_view_format);
            ral_texture_view_get_property(src_command.src_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_LAYERS,
                                         &src_texture_view_n_layers);
            ral_texture_view_get_property(src_command.dst_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                         &dst_texture_view_n_mipmaps);
            ral_texture_view_get_property(src_command.src_texture_view,
                                          RAL_TEXTURE_VIEW_PROPERTY_N_MIPMAPS,
                                         &src_texture_view_n_mipmaps);

            if (!(src_command.n_dst_mipmap < dst_texture_view_n_mipmaps) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_dst_mipmap < dst_texture_view_n_mipmaps,
                                  "Invalid texture mipmap requested for the destination texture view.");

                continue;
            }

            if (!(src_command.n_dst_layer < dst_texture_view_n_layers) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_dst_layer < dst_texture_view_n_layers,
                                  "Invalid texture layer requested for the destination texture view.");

                continue;
            }

            if (!(src_command.n_src_mipmap < src_texture_view_n_mipmaps) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_src_mipmap < src_texture_view_n_mipmaps,
                                  "Invalid texture mipmap requested for the source texture view.");

                continue;
            }

            if (!(src_command.n_src_layer < src_texture_view_n_layers) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_src_layer < src_texture_view_n_layers,
                                  "Invalid texture layer requested for the source texture view.");

                continue;
            }

            ral_texture_view_get_mipmap_property(src_command.dst_texture_view,
                                                 src_command.n_dst_layer,
                                                 src_command.n_dst_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                                 dst_mip_size + 0);
            ral_texture_view_get_mipmap_property(src_command.dst_texture_view,
                                                 src_command.n_dst_layer,
                                                 src_command.n_dst_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                                 dst_mip_size + 1);
            ral_texture_view_get_mipmap_property(src_command.dst_texture_view,
                                                 src_command.n_dst_layer,
                                                 src_command.n_dst_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                                 dst_mip_size + 2);

            ral_texture_view_get_mipmap_property(src_command.src_texture_view,
                                                 src_command.n_src_layer,
                                                 src_command.n_src_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                                 src_mip_size + 0);
            ral_texture_view_get_mipmap_property(src_command.src_texture_view,
                                                 src_command.n_src_layer,
                                                 src_command.n_src_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                                 src_mip_size + 1);
            ral_texture_view_get_mipmap_property(src_command.src_texture_view,
                                                 src_command.n_src_layer,
                                                 src_command.n_src_mipmap,
                                                 RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                                 src_mip_size + 2);

            if (!(src_command.src_start_xyz[0] + src_command.src_size[0] <= src_mip_size[0] &&
                  src_command.src_start_xyz[1] + src_command.src_size[1] <= src_mip_size[1] &&
                  src_command.src_start_xyz[2] + src_command.src_size[2] <= src_mip_size[2]) )
            {
                ASSERT_DEBUG_SYNC(src_command.src_start_xyz[0] + src_command.src_size[0] <= src_mip_size[0] &&
                                  src_command.src_start_xyz[1] + src_command.src_size[1] <= src_mip_size[1] &&
                                  src_command.src_start_xyz[2] + src_command.src_size[2] <= src_mip_size[2],
                                  "Source copy region exceeds source texture size");

                continue;
            }

            if (!(src_command.dst_start_xyz[0] + src_command.dst_size[0] <= dst_mip_size[0] &&
                  src_command.dst_start_xyz[1] + src_command.dst_size[1] <= dst_mip_size[1] &&
                  src_command.dst_start_xyz[2] + src_command.dst_size[2] <= dst_mip_size[2]) )
            {
                ASSERT_DEBUG_SYNC(src_command.dst_start_xyz[0] + src_command.dst_size[0] <= dst_mip_size[0] &&
                                  src_command.dst_start_xyz[1] + src_command.dst_size[1] <= dst_mip_size[1] &&
                                  src_command.dst_start_xyz[2] + src_command.dst_size[2] <= dst_mip_size[2],
                                  "Target copy region exceeds target texture size");

                continue;
            }

            if ((src_command.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) != 0)
            {
                bool dst_texture_has_color_data = false;
                bool src_texture_has_color_data = false;

                ral_utils_get_format_property(dst_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                             &dst_texture_has_color_data);
                ral_utils_get_format_property(src_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                             &src_texture_has_color_data);

                if (!(dst_texture_has_color_data && src_texture_has_color_data) )
                {
                    ASSERT_DEBUG_SYNC(dst_texture_has_color_data && src_texture_has_color_data,
                                      "Invalid texture->texture copy op requested");

                    continue;
                }
            }

            if ((src_command.aspect & RAL_TEXTURE_ASPECT_DEPTH_BIT) != 0)
            {
                bool dst_texture_has_depth_data = false;
                bool src_texture_has_depth_data = false;

                ral_utils_get_format_property(dst_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                             &dst_texture_has_depth_data);
                ral_utils_get_format_property(src_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                             &src_texture_has_depth_data);

                if (!(dst_texture_has_depth_data && src_texture_has_depth_data))
                {
                    ASSERT_DEBUG_SYNC(dst_texture_has_depth_data && src_texture_has_depth_data,
                                      "Invalid texture->texture copy op requested");

                    continue;
                }
            }

            if ((src_command.aspect & RAL_TEXTURE_ASPECT_STENCIL_BIT) != 0)
            {
                bool dst_texture_has_stencil_data = false;
                bool src_texture_has_stencil_data = false;

                ral_utils_get_format_property(dst_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                             &dst_texture_has_stencil_data);
                ral_utils_get_format_property(src_texture_view_format,
                                              RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                             &src_texture_has_stencil_data);

                if (!(dst_texture_has_stencil_data && src_texture_has_stencil_data) )
                {
                    ASSERT_DEBUG_SYNC(dst_texture_has_stencil_data && src_texture_has_stencil_data,
                                      "Invalid texture->texture copy op requested");

                    continue;
                }
            }
        }
        #endif

        memcpy(new_command_ptr->copy_texture_to_texture_command.dst_size,
               src_command.dst_size,
               sizeof(src_command.dst_size) );
        memcpy(new_command_ptr->copy_texture_to_texture_command.dst_start_xyz,
               src_command.dst_start_xyz,
               sizeof(src_command.dst_start_xyz) );
        memcpy(new_command_ptr->copy_texture_to_texture_command.src_size,
               src_command.src_size,
               sizeof(src_command.src_size) );
        memcpy(new_command_ptr->copy_texture_to_texture_command.src_start_xyz,
               src_command.src_start_xyz,
               sizeof(src_command.src_start_xyz) );

        new_command_ptr->copy_texture_to_texture_command.aspect           = src_command.aspect;
        new_command_ptr->copy_texture_to_texture_command.dst_texture_view = src_command.dst_texture_view;
        new_command_ptr->copy_texture_to_texture_command.n_dst_layer      = src_command.n_dst_layer;
        new_command_ptr->copy_texture_to_texture_command.n_dst_mipmap     = src_command.n_dst_mipmap;
        new_command_ptr->copy_texture_to_texture_command.n_src_layer      = src_command.n_src_layer;
        new_command_ptr->copy_texture_to_texture_command.n_src_mipmap     = src_command.n_src_mipmap;
        new_command_ptr->copy_texture_to_texture_command.scaling_filter   = src_command.scaling_filter;
        new_command_ptr->copy_texture_to_texture_command.src_texture_view = src_command.src_texture_view;

        new_command_ptr->type = RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_dispatch(ral_command_buffer recording_command_buffer,
                                                           const uint32_t*    xyz)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        /* Sanity checks */
        const uint32_t* max_xyz = nullptr;

        ral_context_get_property(command_buffer_ptr->context,
                                 RAL_CONTEXT_PROPERTY_MAX_COMPUTE_WORK_GROUP_SIZE,
                                &max_xyz);

        ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_COMPUTE_BIT) != 0,
                          "Dispatch calls require a compute queue command buffer.");

        ASSERT_DEBUG_SYNC(xyz[0] >= 1 && xyz[0] < max_xyz[0] &&
                          xyz[1] >= 1 && xyz[1] < max_xyz[1] &&
                          xyz[2] >= 1 && xyz[2] < max_xyz[2],
                          "Invalid compute work group size requested in the dispatch call");
    }
    #endif

    new_command_ptr                     = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool));
    new_command_ptr->dispatch_command.x = xyz[0];
    new_command_ptr->dispatch_command.y = xyz[1];
    new_command_ptr->dispatch_command.z = xyz[2];
    new_command_ptr->type               = RAL_COMMAND_TYPE_DISPATCH;

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_draw_call_indexed(ral_command_buffer                                       recording_command_buffer,
                                                                    uint32_t                                                 n_draw_calls,
                                                                    const ral_command_buffer_draw_call_indexed_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        /* Sanity checks */
        ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) != 0,
                          "Draw calls require a graphics queue command buffer.");
    }
    #endif

    for (uint32_t n_draw_call = 0;
                  n_draw_call < n_draw_calls;
                ++n_draw_call)
    {
        const ral_command_buffer_draw_call_indexed_command_info& src_command = draw_call_ptrs[n_draw_call];

        new_command_ptr                            = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool));
        new_command_ptr->draw_call_indexed_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_DRAW_CALL_INDEXED;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_draw_call_indirect_regular(ral_command_buffer                                        recording_command_buffer,
                                                                             uint32_t                                                  n_draw_calls,
                                                                             const ral_command_buffer_draw_call_indirect_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        /* Sanity checks */
        ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) != 0,
                          "Draw calls require a graphics queue command buffer.");
    }
    #endif

    for (uint32_t n_draw_call = 0;
                  n_draw_call < n_draw_calls;
                ++n_draw_call)
    {
        const ral_command_buffer_draw_call_indirect_command_info& src_command = draw_call_ptrs[n_draw_call];

        #ifdef _DEBUG
        {
            /* Sanity checks */
            ASSERT_DEBUG_SYNC(src_command.indirect_buffer != nullptr,
                              "Index buffer is NULL");
        }
        #endif

        new_command_ptr                             = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );
        new_command_ptr->draw_call_indirect_command = src_command;
        new_command_ptr->type                       = RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_draw_call_regular(ral_command_buffer                                       recording_command_buffer,
                                                                    uint32_t                                                 n_draw_calls,
                                                                    const ral_command_buffer_draw_call_regular_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        /* Sanity checks */
        ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) != 0,
                          "Draw calls require a graphics queue command buffer.");
    }
    #endif

    for (uint32_t n_draw_call = 0;
                  n_draw_call < n_draw_calls;
                ++n_draw_call)
    {
        const ral_command_buffer_draw_call_regular_command_info& src_command = draw_call_ptrs[n_draw_call];

        new_command_ptr                            = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool));
        new_command_ptr->draw_call_regular_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_DRAW_CALL_REGULAR;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_execute_command_buffer(ral_command_buffer                                            recording_command_buffer,
                                                                         uint32_t                                                      n_commands,
                                                                         const ral_command_buffer_execute_command_buffer_command_info* command_ptrs)
{
    ral_context          command_buffer_context = nullptr;
    _ral_command_buffer* command_buffer_ptr     = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr        = nullptr;

    ASSERT_DEBUG_SYNC(recording_command_buffer != nullptr,
                      "Specified RAL command buffer instance is NULL");
    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_command = 0;
                  n_command < n_commands;
                ++n_command)
    {
        const ral_command_buffer_execute_command_buffer_command_info& src_command = command_ptrs[n_command];

        #ifdef _DEBUG
        {
            /* Sanity checks */
            ASSERT_DEBUG_SYNC(src_command.command_buffer != nullptr,
                              "Command buffer to be executed is null");
            ASSERT_DEBUG_SYNC(src_command.command_buffer != recording_command_buffer,
                              "Recursive command buffer execution requested.");
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool));

        new_command_ptr->execute_command_buffer_command.command_buffer = src_command.command_buffer;
        new_command_ptr->type                                          = RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_fill_buffer(ral_command_buffer                                 recording_command_buffer,
                                                              uint32_t                                           n_fill_ops,
                                                              const ral_command_buffer_fill_buffer_command_info* fill_ops_ptr)
{
    ral_context          buffer_context     = nullptr;
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(recording_command_buffer != nullptr,
                      "Specified RAL command buffer instance is NULL");
    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_fill_op = 0;
                  n_fill_op < n_fill_ops;
                ++n_fill_op)
    {
        const ral_command_buffer_fill_buffer_command_info& src_command = fill_ops_ptr[n_fill_op];

        #ifdef _DEBUG
        {
            /* Sanity checks */
            uint32_t buffer_size = 0;

            ASSERT_DEBUG_SYNC(src_command.buffer != nullptr,
                              "Buffer to fill with DWORDs is null");
            ASSERT_DEBUG_SYNC(src_command.start_offset % sizeof(uint32_t) == 0,
                              "Start offset must be a mul of 4");

            ral_buffer_get_property(src_command.buffer,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &buffer_size);

            ASSERT_DEBUG_SYNC(src_command.start_offset < buffer_size,
                              "Invalid start offset specified.");
            ASSERT_DEBUG_SYNC(src_command.start_offset + src_command.n_dwords * sizeof(uint32_t) <= buffer_size,
                              "Buffer overflow condition detected.");
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool));

        new_command_ptr->fill_buffer_command = src_command;
        new_command_ptr->type                = RAL_COMMAND_TYPE_FILL_BUFFER;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_invalidate_texture(ral_command_buffer recording_command_buffer,
                                                                     ral_texture        texture,
                                                                     uint32_t           n_start_mip,
                                                                     uint32_t           n_mips)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;
    uint32_t             texture_n_mips     = 0;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    /* Sanity checks */
    if (texture == nullptr)
    {
        ASSERT_DEBUG_SYNC(texture != nullptr,
                          "Input RAL texture instance is NULL");

        goto end;
    }

    ral_texture_get_property(texture,
                             RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                            &texture_n_mips);

    if (n_start_mip + n_mips > texture_n_mips)
    {
        ASSERT_DEBUG_SYNC(!(n_start_mip + n_mips > texture_n_mips),
                          "Invalid mipmap range specified");

        goto end;
    }

    /* Record the command */
    new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

    new_command_ptr->invalidate_texture_command.n_mips      = n_mips;
    new_command_ptr->invalidate_texture_command.n_start_mip = n_start_mip;
    new_command_ptr->invalidate_texture_command.texture     = texture;
    new_command_ptr->type                                   = RAL_COMMAND_TYPE_INVALIDATE_TEXTURE;

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_bindings(ral_command_buffer                           recording_command_buffer,
                                                               uint32_t                                     n_bindings,
                                                               ral_command_buffer_set_binding_command_info* binding_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_binding = 0;
                  n_binding < n_bindings;
                ++n_binding)
    {
        const ral_command_buffer_set_binding_command_info& src_command = binding_ptrs[n_binding];

        #ifdef _DEBUG
        {
            /* Sanity checks */
            switch (src_command.binding_type)
            {
                case RAL_BINDING_TYPE_RENDERTARGET:
                {
                    /* No sanity checks to perform at RAL level. */
                    break;
                }

                case RAL_BINDING_TYPE_SAMPLED_IMAGE:
                {
                    ASSERT_DEBUG_SYNC(src_command.sampled_image_binding.sampler != nullptr,
                                      "Null sampler defined for a sampled image binding");

                    ASSERT_DEBUG_SYNC(src_command.sampled_image_binding.texture_view != nullptr,
                                      "Null texture view defined for a sampled image binding.");

                    break;
                }

                case RAL_BINDING_TYPE_STORAGE_IMAGE:
                {
                    ASSERT_DEBUG_SYNC(src_command.storage_image_binding.texture_view != nullptr,
                                      "Null texture view defined for a storage image binding.");

                    break;
                }

                case RAL_BINDING_TYPE_STORAGE_BUFFER:
                case RAL_BINDING_TYPE_UNIFORM_BUFFER:
                {
                    const ral_buffer buffer             = (src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? src_command.storage_buffer_binding.buffer
                                                                                                                        : src_command.uniform_buffer_binding.buffer;
                    const uint32_t   buffer_offset      = (src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? src_command.storage_buffer_binding.offset
                                                                                                                        : src_command.uniform_buffer_binding.offset;
                    const uint32_t   buffer_region_size = (src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? src_command.storage_buffer_binding.size
                                                                                                                        : src_command.uniform_buffer_binding.size;
                    uint32_t         buffer_size        = 0;
                    ral_buffer_usage buffer_usage;

                    ASSERT_DEBUG_SYNC(buffer != nullptr,
                                      "Null buffer defined for a storage/uniform buffer binding.");

                    ral_buffer_get_property(buffer,
                                            RAL_BUFFER_PROPERTY_SIZE,
                                           &buffer_size);
                    ral_buffer_get_property(buffer,
                                            RAL_BUFFER_PROPERTY_USAGE_BITS,
                                           &buffer_usage);

                    ASSERT_DEBUG_SYNC(buffer_offset < buffer_size,
                                      "Start offset for the storage/uniform buffer binding exceeds available buffer storage");

                    if (buffer_region_size != 0)
                    {
                        ASSERT_DEBUG_SYNC(buffer_offset + buffer_region_size <= buffer_size,
                                          "Region for the storage/uniform buffer binding exceeds available buffer storage");
                    }

                    ASSERT_DEBUG_SYNC(src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER && (buffer_usage & RAL_BUFFER_USAGE_SHADER_STORAGE_BUFFER_BIT) != 0 ||
                                      src_command.binding_type == RAL_BINDING_TYPE_UNIFORM_BUFFER && (buffer_usage & RAL_BUFFER_USAGE_UNIFORM_BUFFER_BIT)        != 0,
                                      "Usage pattern specified at RAL buffer creation time does not permit it to be used as the specified binding");

                    break;
                }

                default:
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized RAL binding type");
                }
            }
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_command_ptr->set_binding_command = src_command;
        new_command_ptr->type                = RAL_COMMAND_TYPE_SET_BINDING;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_gfx_state(ral_command_buffer recording_command_buffer,
                                                                ral_gfx_state      gfx_state)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        /* Sanity checks */
        ASSERT_DEBUG_SYNC(gfx_state != nullptr,
                          "Null gfx state specified");

        ASSERT_DEBUG_SYNC(command_buffer_ptr->compatible_queues == RAL_QUEUE_GRAPHICS_BIT,
                          "GFX state can only be applied to graphics queue command buffers.");
    }
    #endif

    new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

    new_command_ptr->set_gfx_state_command.new_state = gfx_state;
    new_command_ptr->type                            = RAL_COMMAND_TYPE_SET_GFX_STATE;

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_program(ral_command_buffer recording_command_buffer,
                                                              ral_program        program)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

    new_command_ptr->set_program_command.new_program = program;
    new_command_ptr->type                            = RAL_COMMAND_TYPE_SET_PROGRAM;

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_color_rendertargets(ral_command_buffer                                            recording_command_buffer,
                                                                          uint32_t                                                      n_rendertargets,
                                                                          const ral_command_buffer_set_color_rendertarget_command_info* rendertarget_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_rendertarget = 0;
                  n_rendertarget < n_rendertargets;
                ++n_rendertarget)
    {
        const ral_command_buffer_set_color_rendertarget_command_info& src_command = rendertarget_ptrs[n_rendertarget];

        #ifdef _DEBUG
        {
            ASSERT_DEBUG_SYNC(src_command.texture_view != nullptr,
                              "Null texture view specified");
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_command_ptr->set_color_rendertarget_command = src_command;
        new_command_ptr->type                           = RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_depth_rendertarget(ral_command_buffer recording_command_buffer,
                                                                        ral_texture_view   depth_rt)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        ral_format depth_rt_format;
        bool       depth_rt_format_has_depth_components = false;

        ASSERT_DEBUG_SYNC(depth_rt != nullptr,
                          "Null texture view specified");

        ral_texture_view_get_property(depth_rt,
                                      RAL_TEXTURE_VIEW_PROPERTY_FORMAT,
                                     &depth_rt_format);
        ral_utils_get_format_property(depth_rt_format,
                                      RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                     &depth_rt_format_has_depth_components);

        ASSERT_DEBUG_SYNC(depth_rt_format_has_depth_components,
                          "Depth texture view uses a format which does not contain depth data.");
    }
    #endif

    new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

    new_command_ptr->set_depth_rendertarget_command.depth_rt = depth_rt;
    new_command_ptr->type                                    = RAL_COMMAND_TYPE_SET_DEPTH_RENDERTARGET;

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                                     recording_command_buffer,
                                                                    uint32_t                                               n_scissor_boxes,
                                                                    const ral_command_buffer_set_scissor_box_command_info* scissor_box_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_scissor_box = 0;
                  n_scissor_box < n_scissor_boxes;
                ++n_scissor_box)
    {
        const ral_command_buffer_set_scissor_box_command_info& src_command = scissor_box_ptrs[n_scissor_box];

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        static_assert(sizeof(new_command_ptr->set_scissor_box_command.size) == sizeof(src_command.size), "");
        static_assert(sizeof(new_command_ptr->set_scissor_box_command.xy)   == sizeof(src_command.xy),   "");

        memcpy(new_command_ptr->set_scissor_box_command.size,
               src_command.size,
               sizeof(src_command.size) );
        memcpy(new_command_ptr->set_scissor_box_command.xy,
               src_command.xy,
               sizeof(src_command.xy) );

        new_command_ptr->set_scissor_box_command.index = src_command.index;
        new_command_ptr->type                          = RAL_COMMAND_TYPE_SET_SCISSOR_BOX;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_vertex_buffers(ral_command_buffer                                       recording_command_buffer,
                                                                     uint32_t                                                 n_vertex_buffers,
                                                                     const ral_command_buffer_set_vertex_buffer_command_info* vertex_buffer_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_vertex_buffer = 0;
                  n_vertex_buffer < n_vertex_buffers;
                ++n_vertex_buffer)
    {
        const ral_command_buffer_set_vertex_buffer_command_info& src_command = vertex_buffer_ptrs[n_vertex_buffer];

        #ifdef _DEBUG
        {
            /* Sanity checks */
            uint32_t buffer_size = 0;

            ASSERT_DEBUG_SYNC(src_command.buffer != nullptr,
                              "Buffer specified for a vertex attribute is NULL");

            ral_buffer_get_property(src_command.buffer,
                                    RAL_BUFFER_PROPERTY_SIZE,
                                   &buffer_size);

            ASSERT_DEBUG_SYNC(src_command.start_offset < buffer_size,
                              "Invalid start offset specified for a vertex attribute.");
        }
        #endif

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        new_command_ptr->set_vertex_buffer_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_SET_VERTEX_BUFFER;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_set_viewports(ral_command_buffer                                  recording_command_buffer,
                                                                uint32_t                                            n_viewports,
                                                                const ral_command_buffer_set_viewport_command_info* viewport_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    for (uint32_t n_viewport = 0;
                  n_viewport < n_viewports;
                ++n_viewport)
    {
        const ral_command_buffer_set_viewport_command_info& src_command = viewport_ptrs[n_viewport];

        new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

        memcpy(new_command_ptr->set_viewport_command.depth_range,
               src_command.depth_range,
               sizeof(src_command.depth_range) );
        memcpy(new_command_ptr->set_viewport_command.size,
               src_command.size,
               sizeof(src_command.size) );
        memcpy(new_command_ptr->set_viewport_command.xy,
               src_command.xy,
               sizeof(src_command.xy) );

        new_command_ptr->set_viewport_command.index = src_command.index;
        new_command_ptr->type                       = RAL_COMMAND_TYPE_SET_VIEWPORT;

        new_command_ptr->init();

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_command_buffer_record_update_buffer(ral_command_buffer recording_command_buffer,
                                                                ral_buffer         buffer,
                                                                uint32_t           start_offset,
                                                                uint32_t           n_data_bytes,
                                                                const void*        data)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(recording_command_buffer);
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    #ifdef _DEBUG
    {
        uint32_t buffer_size         = 0;
        uint32_t buffer_start_offset = -1;

        ral_buffer_get_property(buffer,
                                RAL_BUFFER_PROPERTY_SIZE,
                               &buffer_size);
        ral_buffer_get_property(buffer,
                                RAL_BUFFER_PROPERTY_START_OFFSET,
                               &buffer_start_offset);

        if (start_offset + n_data_bytes > buffer_size - buffer_start_offset)
        {
            ASSERT_DEBUG_SYNC(!(start_offset + n_data_bytes > buffer_size - buffer_start_offset),
                              "Data region specified for the \"update buffer\" command exceeds available buffer space");

            goto end;
        }
    }
    #endif

    new_command_ptr = reinterpret_cast<_ral_command*>(system_resource_pool_get_from_pool(command_pool) );

    new_command_ptr->update_buffer_command.buffer       = buffer;
    new_command_ptr->update_buffer_command.size         = n_data_bytes;
    new_command_ptr->update_buffer_command.start_offset = start_offset;
    new_command_ptr->type                               = RAL_COMMAND_TYPE_UPDATE_BUFFER;

    if (n_data_bytes > sizeof(new_command_ptr->update_buffer_command.preallocated_data))
    {
        LOG_FATAL("Performance warning: need to alloc a heap memory-backed region (%u bytes) for the \"update buffer\" RAL command",
                  n_data_bytes);

        new_command_ptr->update_buffer_command.alloced_data = new char[n_data_bytes];

        ASSERT_DEBUG_SYNC(new_command_ptr->update_buffer_command.alloced_data != nullptr,
                          "Out of memory");

        memcpy(new_command_ptr->update_buffer_command.alloced_data,
               data,
               n_data_bytes);
    }
    else
    {
        new_command_ptr->update_buffer_command.alloced_data = nullptr;

        memcpy(new_command_ptr->update_buffer_command.preallocated_data,
               data,
               n_data_bytes);
    }

    new_command_ptr->init();

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer)
{
    _ral_command_buffer* command_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(command_buffer);

    ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                      "Input ral_command_buffer instance is NULL");

    command_buffer_ptr->clear_commands();
    command_buffer_ptr->status = RAL_COMMAND_BUFFER_STATUS_UNDEFINED;

    system_resource_pool_return_to_pool(command_buffer_pool,
                                        reinterpret_cast<system_resource_pool_block>(command_buffer) );
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_command_buffer_start_recording(ral_command_buffer command_buffer)
{
    _ral_command_buffer* cmd_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(command_buffer);
    bool                 result         = false;

    /* Sanity checks */
    if (command_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                          "A null command buffer was specified");

        goto end;
    }

    if ( cmd_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDED &&
        !cmd_buffer_ptr->is_resettable)
    {
        ASSERT_DEBUG_SYNC(cmd_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDED && cmd_buffer_ptr->is_resettable,
                          "Cannot reset a non-resettable command buffer");

        goto end;
    }

    if (cmd_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(cmd_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING,
                          "ral_command_buffer_start_recording() called against a command buffer in a recording state");

        goto end;
    }

    /* Update the cmd buffer and fire a notification to listening backend. */
    cmd_buffer_ptr->status = RAL_COMMAND_BUFFER_STATUS_RECORDING;

    cmd_buffer_ptr->clear_commands();

    system_callback_manager_call_back(cmd_buffer_ptr->callback_manager,
                                      RAL_COMMAND_BUFFER_CALLBACK_ID_RECORDING_STARTED,
                                      command_buffer);

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_command_buffer_stop_recording(ral_command_buffer command_buffer)
{
    _ral_command_buffer* cmd_buffer_ptr = reinterpret_cast<_ral_command_buffer*>(command_buffer);
    bool                 result         = false;

    /* Sanity checks */
    if (command_buffer == nullptr)
    {
        ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                          "A null command buffer was specified");

        goto end;
    }

    if (cmd_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        ASSERT_DEBUG_SYNC(cmd_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                          "ral_command_buffer_stop_recording() called against a command buffer in a non-recording state");

        goto end;
    }

    /* Update the cmd buffer and fire a notification to listening backend. */
    cmd_buffer_ptr->status = RAL_COMMAND_BUFFER_STATUS_RECORDED;

    system_callback_manager_call_back(cmd_buffer_ptr->callback_manager,
                                      RAL_COMMAND_BUFFER_CALLBACK_ID_RECORDING_STOPPED,
                                      command_buffer);

    /* All done */
    result = true;
end:
    return result;
}