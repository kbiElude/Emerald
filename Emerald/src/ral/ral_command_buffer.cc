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
        ral_command_buffer_copy_texture_to_texture_command_info copy_texture_to_texture_command;
        ral_command_buffer_draw_call_indexed_command_info       draw_call_indexed_command;
        ral_command_buffer_draw_call_indirect_command_info      draw_call_indirect_command;
        ral_command_buffer_draw_call_regular_command_info       draw_call_regular_command;
        ral_command_buffer_execute_command_buffer_command_info  execute_command_buffer_command;
        ral_command_buffer_set_binding_command_info             set_binding_command;
        ral_command_buffer_set_gfx_state_command_info           set_gfx_state_command;
        ral_command_buffer_set_program_command_info             set_program_command;
        ral_command_buffer_set_color_rendertarget_command_info  set_color_rendertarget_command;
        ral_command_buffer_set_scissor_box_command_info         set_scissor_box_command;
        ral_command_buffer_set_vertex_buffer_command_info       set_vertex_buffer_command;
        ral_command_buffer_set_viewport_command_info            set_viewport_command;
    };

    void deinit()
    {
        switch (type)
        {
            case RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE:
            {
                ral_context dst_texture_context = nullptr;
                ral_context src_texture_context = nullptr;

                ral_texture_get_property(copy_texture_to_texture_command.dst_texture,
                                         RAL_TEXTURE_PROPERTY_CONTEXT,
                                        &dst_texture_context);
                ral_texture_get_property(copy_texture_to_texture_command.src_texture,
                                         RAL_TEXTURE_PROPERTY_CONTEXT,
                                        &src_texture_context);

                ral_context_delete_objects(dst_texture_context,
                                           RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                           1, /* n_objects */
                                           (const void**) &copy_texture_to_texture_command.dst_texture);
                ral_context_delete_objects(src_texture_context,
                                           RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                           1, /* n_objects */
                                           (const void**) &copy_texture_to_texture_command.src_texture);

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDEXED:
            {
                ral_context index_buffer_context = nullptr;

                ral_buffer_get_property(draw_call_indexed_command.index_buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &index_buffer_context);

                ral_context_delete_objects(index_buffer_context,
                                           RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &draw_call_indexed_command.index_buffer);

                break;
            }

            case RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property(draw_call_indirect_command.indirect_buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                ral_context_delete_objects(buffer_context,
                                           RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &draw_call_indirect_command.indirect_buffer);

                break;
            }

            case RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER:
            {
                ral_context_delete_objects( ((_ral_command_buffer*) execute_command_buffer_command.command_buffer)->context,
                                           RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &execute_command_buffer_command.command_buffer);

                break;
            }

            case RAL_COMMAND_TYPE_SET_BINDING:
            {
                switch (set_binding_command.binding_type)
                {
                    case RAL_BINDING_TYPE_SAMPLED_IMAGE:
                    case RAL_BINDING_TYPE_STORAGE_IMAGE:
                    {
                        const ral_texture_view texture_view         = (set_binding_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE) ? set_binding_command.sampled_image_binding.texture_view
                                                                                                                                           : set_binding_command.storage_image_binding.texture_view;
                        ral_context            texture_view_context = nullptr;

                        ral_texture_view_get_property(texture_view,
                                                      RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                                     &texture_view_context);
                        ral_context_delete_objects   (texture_view_context,
                                                      RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                                      1, /* n_objects */
                                                      (const void**) &texture_view);

                        if (set_binding_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE)
                        {
                            ral_context sampler_context = nullptr;

                            ral_sampler_get_property  (set_binding_command.sampled_image_binding.sampler,
                                                       RAL_SAMPLER_PROPERTY_CONTEXT,
                                                      &sampler_context);
                            ral_context_delete_objects(sampler_context,
                                                       RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                                       1, /* n_objects */
                                                       (const void**) &set_binding_command.sampled_image_binding.sampler);
                        }

                        break;
                    }

                    case RAL_BINDING_TYPE_STORAGE_BUFFER:
                    case RAL_BINDING_TYPE_UNIFORM_BUFFER:
                    {
                        const ral_buffer buffer         = (set_binding_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? set_binding_command.storage_buffer_binding.buffer
                                                                                                                                : set_binding_command.uniform_buffer_binding.buffer;
                        ral_context      buffer_context = nullptr;

                        ral_buffer_get_property   (buffer,
                                                   RAL_BUFFER_PROPERTY_CONTEXT,
                                                  &buffer_context);
                        ral_context_delete_objects(buffer_context,
                                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                                   1, /* n_objects */
                                                   (const void**) &buffer);

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

            case RAL_COMMAND_TYPE_SET_GFX_STATE:
            {
                ral_context gfx_state_context = nullptr;

                ral_gfx_state_get_property(set_gfx_state_command.new_state,
                                           RAL_GFX_STATE_PROPERTY_CONTEXT,
                                          &gfx_state_context);
                ral_context_delete_objects(gfx_state_context,
                                           RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                                           1, /* n_objects */
                                           (const void**) &set_gfx_state_command.new_state);

                break;
            }

            case RAL_COMMAND_TYPE_SET_PROGRAM:
            {
                ral_context program_context = nullptr;

                ral_program_get_property  (set_program_command.new_program,
                                           RAL_PROGRAM_PROPERTY_CONTEXT,
                                          &program_context);
                ral_context_delete_objects(program_context,
                                           RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                                           1, /* n_objects */
                                           (const void**) &set_program_command.new_program);

                break;
            }

            case RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET:
            {
                ral_context texture_view_context = nullptr;

                ral_texture_view_get_property(set_color_rendertarget_command.texture_view,
                                              RAL_TEXTURE_VIEW_PROPERTY_CONTEXT,
                                             &texture_view_context);
                ral_context_delete_objects   (texture_view_context,
                                              RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                              1, /* n_objects */
                                              (const void**) &set_color_rendertarget_command.texture_view);

                break;
            }

            case RAL_COMMAND_TYPE_SET_VERTEX_BUFFER:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property   (set_vertex_buffer_command.buffer,
                                           RAL_BUFFER_PROPERTY_CONTEXT,
                                          &buffer_context);
                ral_context_delete_objects(buffer_context,
                                           RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &set_vertex_buffer_command.buffer);

                break;
            }

            /* Dummy */
            case RAL_COMMAND_TYPE_CLEAR_RT_BINDING:
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
    _ral_command* command_ptr = (_ral_command*) block;

    command_ptr->deinit();
}

/** TODO */
PRIVATE void _ral_command_buffer_deinit_command_buffer(system_resource_pool_block block)
{
    _ral_command_buffer* cmd_buffer_ptr = (_ral_command_buffer*) block; 

    if (cmd_buffer_ptr->callback_manager != nullptr)
    {
        system_callback_manager_release(cmd_buffer_ptr->callback_manager);

        cmd_buffer_ptr->callback_manager = nullptr;
    }

    if (cmd_buffer_ptr->commands != nullptr)
    {
        system_resizable_vector_release(cmd_buffer_ptr->commands);

        cmd_buffer_ptr->commands = nullptr;
    }
}

/** TODO */
PRIVATE void _ral_command_buffer_init_command_buffer(system_resource_pool_block block)
{
    _ral_command_buffer* cmd_buffer_ptr = (_ral_command_buffer*) block; 

    cmd_buffer_ptr->callback_manager = system_callback_manager_create((_callback_id) RAL_COMMAND_BUFFER_CALLBACK_ID_COUNT);
    cmd_buffer_ptr->commands         = system_resizable_vector_create(N_MAX_PREALLOCED_COMMANDS);
    cmd_buffer_ptr->context          = nullptr;
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
    new_command_buffer_ptr = (_ral_command_buffer*) system_resource_pool_get_from_pool(command_buffer_pool);

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
    return (ral_command_buffer) new_command_buffer_ptr;
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
PUBLIC void ral_command_buffer_get_property(ral_command_buffer          command_buffer,
                                            ral_command_buffer_property property,
                                            void*                       out_result_ptr)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) command_buffer;

    switch (property)
    {
        case RAL_COMMAND_BUFFER_PROPERTY_COMPATIBLE_QUEUES:
        {
            *(ral_queue_bits*) out_result_ptr = command_buffer_ptr->compatible_queues;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_CONTEXT:
        {
            *(ral_context*) out_result_ptr = command_buffer_ptr->context;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_INVOKABLE_FROM_OTHER_COMMAND_BUFFERS:
        {
            *(bool*) out_result_ptr = command_buffer_ptr->is_invokable_from_other_command_buffers;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_RESETTABLE:
        {
            *(bool*) out_result_ptr = command_buffer_ptr->is_resettable;

            break;
        }

        case RAL_COMMAND_BUFFER_PROPERTY_IS_TRANSIENT:
        {
            *(bool*) out_result_ptr = command_buffer_ptr->is_transient;

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
            *(ral_command_buffer_status*) out_result_ptr = command_buffer_ptr->status;

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
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) command_buffer;
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
PUBLIC void ral_command_buffer_record_clear_rendertarget_binding(ral_command_buffer                                      recording_command_buffer,
                                                                 uint32_t                                                n_clear_ops,
                                                                 const ral_command_buffer_clear_rt_binding_command_info* clear_op_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

            if (current_rt.rt_index >= N_MAX_CLEAR_RENDERTARGETS)
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Invalid rendertarget index specified");

                is_command_valid = false;

                break;
            }
        }

        if (is_command_valid)
        {
            memcpy(new_command_ptr->clear_rt_binding_command.clear_regions,
                   src_command.clear_regions,
                   src_command.n_clear_regions * sizeof(src_command.clear_regions[0]) );
            memcpy(new_command_ptr->clear_rt_binding_command.rendertargets,
                   src_command.rendertargets,
                   src_command.n_rendertargets * sizeof(src_command.rendertargets[0]) );

            new_command_ptr->type = RAL_COMMAND_TYPE_CLEAR_RT_BINDING;

            system_resizable_vector_push(command_buffer_ptr->commands,
                                         new_command_ptr);
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_copy_texture_to_texture(ral_command_buffer                                             recording_command_buffer,
                                                              uint32_t                                                       n_copy_ops,
                                                              const ral_command_buffer_copy_texture_to_texture_command_info* copy_op_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.dst_size)      == sizeof(src_command.dst_size),      "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.dst_start_xyz) == sizeof(src_command.dst_start_xyz), "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.src_size)      == sizeof(src_command.src_size),      "");
        static_assert(sizeof(new_command_ptr->copy_texture_to_texture_command.src_start_xyz) == sizeof(src_command.src_start_xyz), "");

        #ifdef _DEBUG
        {
            uint32_t   dst_mip_size[3]       = {0};
            ral_format dst_texture_format;
            uint32_t   dst_texture_n_layers  =  0;
            uint32_t   dst_texture_n_mipmaps =  0;
            uint32_t   src_mip_size[3]       = {0};
            ral_format src_texture_format;
            uint32_t   src_texture_n_layers  =  0;
            uint32_t   src_texture_n_mipmaps =  0;

            /* Sanity checks */
            if (src_command.scaling_filter == RAL_TEXTURE_FILTER_LINEAR)
            {
                ASSERT_DEBUG_SYNC((command_buffer_ptr->compatible_queues & RAL_QUEUE_GRAPHICS_BIT) != 0,
                                  "Scaling texture->texture copy requires a graphics queue command buffer.");

                continue;
            }

            if (src_command.dst_texture == nullptr)
            {
                ASSERT_DEBUG_SYNC(src_command.dst_texture != nullptr,
                                  "Destination texture is null");

                continue;
            }

            if (src_command.src_texture == nullptr)
            {
                ASSERT_DEBUG_SYNC(src_command.src_texture != nullptr,
                                  "Source texture is null");

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

            ral_texture_get_property(src_command.dst_texture,
                                     RAL_TEXTURE_PROPERTY_FORMAT,
                                    &dst_texture_format);
            ral_texture_get_property(src_command.dst_texture,
                                     RAL_TEXTURE_PROPERTY_N_LAYERS,
                                    &dst_texture_n_layers);
            ral_texture_get_property(src_command.src_texture,
                                     RAL_TEXTURE_PROPERTY_FORMAT,
                                    &src_texture_format);
            ral_texture_get_property(src_command.src_texture,
                                     RAL_TEXTURE_PROPERTY_N_LAYERS,
                                    &src_texture_n_layers);
            ral_texture_get_property(src_command.dst_texture,
                                     RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                    &dst_texture_n_mipmaps);
            ral_texture_get_property(src_command.src_texture,
                                     RAL_TEXTURE_PROPERTY_N_MIPMAPS,
                                    &src_texture_n_mipmaps);

            if (!(src_command.n_dst_texture_mipmap < dst_texture_n_mipmaps) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_dst_texture_mipmap < dst_texture_n_mipmaps,
                                  "Invalid texture mipmap requested for the destination texture.");

                continue;
            }

            if (!(src_command.n_dst_texture_layer < dst_texture_n_layers) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_dst_texture_layer < dst_texture_n_layers,
                                  "Invalid texture layer requested for the destination texture.");

                continue;
            }

            if (!(src_command.n_src_texture_mipmap < src_texture_n_mipmaps) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_src_texture_mipmap < src_texture_n_mipmaps,
                                  "Invalid texture mipmap requested for the source texture.");

                continue;
            }

            if (!(src_command.n_src_texture_layer < src_texture_n_layers) )
            {
                ASSERT_DEBUG_SYNC(src_command.n_src_texture_layer < src_texture_n_layers,
                                  "Invalid texture layer requested for the source texture.");

                continue;
            }

            ral_texture_get_mipmap_property(src_command.dst_texture,
                                            src_command.n_dst_texture_layer,
                                            src_command.n_dst_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                            dst_mip_size + 0);
            ral_texture_get_mipmap_property(src_command.dst_texture,
                                            src_command.n_dst_texture_layer,
                                            src_command.n_dst_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                            dst_mip_size + 1);
            ral_texture_get_mipmap_property(src_command.dst_texture,
                                            src_command.n_dst_texture_layer,
                                            src_command.n_dst_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                            dst_mip_size + 2);

            ral_texture_get_mipmap_property(src_command.src_texture,
                                            src_command.n_src_texture_layer,
                                            src_command.n_src_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_WIDTH,
                                            src_mip_size + 0);
            ral_texture_get_mipmap_property(src_command.src_texture,
                                            src_command.n_src_texture_layer,
                                            src_command.n_src_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_HEIGHT,
                                            src_mip_size + 1);
            ral_texture_get_mipmap_property(src_command.src_texture,
                                            src_command.n_src_texture_layer,
                                            src_command.n_src_texture_mipmap,
                                            RAL_TEXTURE_MIPMAP_PROPERTY_DEPTH,
                                            src_mip_size + 2);

            if (!(src_command.src_start_xyz[0] + src_command.src_size[0] < src_mip_size[0] &&
                  src_command.src_start_xyz[1] + src_command.src_size[1] < src_mip_size[1] &&
                  src_command.src_start_xyz[2] + src_command.src_size[2] < src_mip_size[2]) )
            {
                ASSERT_DEBUG_SYNC(src_command.src_start_xyz[0] + src_command.src_size[0] < src_mip_size[0] &&
                                  src_command.src_start_xyz[1] + src_command.src_size[1] < src_mip_size[1] &&
                                  src_command.src_start_xyz[2] + src_command.src_size[2] < src_mip_size[2],
                                  "Source copy region exceeds source texture size");

                continue;
            }

            if (!(src_command.dst_start_xyz[0] + src_command.dst_size[0] < dst_mip_size[0] &&
                  src_command.dst_start_xyz[1] + src_command.dst_size[1] < dst_mip_size[1] &&
                  src_command.dst_start_xyz[2] + src_command.dst_size[2] < dst_mip_size[2]) )
            {
                ASSERT_DEBUG_SYNC(src_command.dst_start_xyz[0] + src_command.dst_size[0] < dst_mip_size[0] &&
                                  src_command.dst_start_xyz[1] + src_command.dst_size[1] < dst_mip_size[1] &&
                                  src_command.dst_start_xyz[2] + src_command.dst_size[2] < dst_mip_size[2],
                                  "Target copy region exceeds target texture size");

                continue;
            }

            if ((src_command.aspect & RAL_TEXTURE_ASPECT_COLOR_BIT) != 0)
            {
                bool dst_texture_has_color_data = false;
                bool src_texture_has_color_data = false;

                ral_utils_get_format_property(dst_texture_format,
                                              RAL_FORMAT_PROPERTY_HAS_COLOR_COMPONENTS,
                                             &dst_texture_has_color_data);
                ral_utils_get_format_property(src_texture_format,
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

                ral_utils_get_format_property(dst_texture_format,
                                              RAL_FORMAT_PROPERTY_HAS_DEPTH_COMPONENTS,
                                             &dst_texture_has_depth_data);
                ral_utils_get_format_property(src_texture_format,
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

                ral_utils_get_format_property(dst_texture_format,
                                              RAL_FORMAT_PROPERTY_HAS_STENCIL_COMPONENTS,
                                             &dst_texture_has_stencil_data);
                ral_utils_get_format_property(src_texture_format,
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

        new_command_ptr->copy_texture_to_texture_command.aspect               = src_command.aspect;
        new_command_ptr->copy_texture_to_texture_command.dst_texture          = src_command.dst_texture;
        new_command_ptr->copy_texture_to_texture_command.n_dst_texture_layer  = src_command.n_dst_texture_layer;
        new_command_ptr->copy_texture_to_texture_command.n_dst_texture_mipmap = src_command.n_dst_texture_mipmap;
        new_command_ptr->copy_texture_to_texture_command.n_src_texture_layer  = src_command.n_src_texture_layer;
        new_command_ptr->copy_texture_to_texture_command.n_src_texture_mipmap = src_command.n_src_texture_mipmap;
        new_command_ptr->copy_texture_to_texture_command.scaling_filter       = src_command.scaling_filter;

        new_command_ptr->type = RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE;

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                  src_command.dst_texture);
        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                  src_command.src_texture);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_draw_call_indexed(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_indexed_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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
        const ral_command_buffer_draw_call_indexed_command_info& src_command = draw_call_ptrs[n_draw_calls];

        new_command_ptr                            = (_ral_command*) system_resource_pool_get_from_pool(command_pool);
        new_command_ptr->draw_call_indexed_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_DRAW_CALL_INDEXED;

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                  src_command.index_buffer);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_draw_call_indirect_regular(ral_command_buffer                                        recording_command_buffer,
                                                                 uint32_t                                                  n_draw_calls,
                                                                 const ral_command_buffer_draw_call_indirect_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr                             = (_ral_command*) system_resource_pool_get_from_pool(command_pool);
        new_command_ptr->draw_call_indirect_command = src_command;
        new_command_ptr->type                       = RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT;

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                  src_command.indirect_buffer);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_draw_call_regular(ral_command_buffer                                       recording_command_buffer,
                                                        uint32_t                                                 n_draw_calls,
                                                        const ral_command_buffer_draw_call_regular_command_info* draw_call_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr                            = (_ral_command*) system_resource_pool_get_from_pool(command_pool);
        new_command_ptr->draw_call_regular_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_DRAW_CALL_REGULAR;

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_execute_command_buffer(ral_command_buffer                                            recording_command_buffer,
                                                             uint32_t                                                      n_commands,
                                                             const ral_command_buffer_execute_command_buffer_command_info* command_ptrs)
{
    ral_context          command_buffer_context = nullptr;
    _ral_command_buffer* command_buffer_ptr     = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        new_command_ptr->execute_command_buffer_command.command_buffer = src_command.command_buffer;
        new_command_ptr->type                                          = RAL_COMMAND_TYPE_EXECUTE_COMMAND_BUFFER;

        if (n_command == 0)
        {
            command_buffer_context = ((_ral_command_buffer*) src_command.command_buffer)->context;
        }
        else
        {
            ASSERT_DEBUG_SYNC(((_ral_command_buffer*) src_command.command_buffer)->context == command_buffer_context,
                              "Scheduled command buffers use different rendering contexts.");
        }

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_COMMAND_BUFFER,
                                  (void*) src_command.command_buffer);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_bindings(ral_command_buffer                           recording_command_buffer,
                                                   uint32_t                                     n_bindings,
                                                   ral_command_buffer_set_binding_command_info* binding_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

                    ASSERT_DEBUG_SYNC(buffer != nullptr,
                                      "Null buffer defined for a storage/uniform buffer binding.");

                    ral_buffer_get_property(buffer,
                                            RAL_BUFFER_PROPERTY_SIZE,
                                           &buffer_size);

                    ASSERT_DEBUG_SYNC(buffer_offset < buffer_size,
                                      "Start offset for the storage/uniform buffer binding exceeds available buffer storage");

                    if (buffer_region_size != 0)
                    {
                        ASSERT_DEBUG_SYNC(buffer_offset + buffer_region_size <= buffer_size,
                                          "Region for the storage/uniform buffer binding exceeds available buffer storage");
                    }

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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        new_command_ptr->set_binding_command = src_command;
        new_command_ptr->type                = RAL_COMMAND_TYPE_SET_BINDING;

        if (src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER ||
            src_command.binding_type == RAL_BINDING_TYPE_UNIFORM_BUFFER)
        {
            ral_context_retain_object(command_buffer_ptr->context,
                                      RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                      (src_command.binding_type == RAL_BINDING_TYPE_STORAGE_BUFFER) ? new_command_ptr->set_binding_command.storage_buffer_binding.buffer
                                                                                                    : new_command_ptr->set_binding_command.uniform_buffer_binding.buffer);
        }

        if (src_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE)
        {
            ral_context_retain_object(command_buffer_ptr->context,
                                      RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                      new_command_ptr->set_binding_command.sampled_image_binding.sampler);
        }

        if (src_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE ||
            src_command.binding_type == RAL_BINDING_TYPE_STORAGE_IMAGE)
        {
            ral_context_retain_object(command_buffer_ptr->context,
                                      RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                      (src_command.binding_type == RAL_BINDING_TYPE_SAMPLED_IMAGE) ? new_command_ptr->set_binding_command.sampled_image_binding.texture_view
                                                                                                   : new_command_ptr->set_binding_command.storage_image_binding.texture_view);
        }

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_gfx_state(ral_command_buffer recording_command_buffer,
                                                    ral_gfx_state      gfx_state)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

    new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

    new_command_ptr->set_gfx_state_command.new_state = gfx_state;
    new_command_ptr->type                            = RAL_COMMAND_TYPE_SET_GFX_STATE;

    ral_context_retain_object(command_buffer_ptr->context,
                              RAL_CONTEXT_OBJECT_TYPE_GFX_STATE,
                              gfx_state);

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_program(ral_command_buffer recording_command_buffer,
                                                  ral_program        program)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
    _ral_command*        new_command_ptr    = nullptr;

    ASSERT_DEBUG_SYNC(command_buffer_ptr->status == RAL_COMMAND_BUFFER_STATUS_RECORDING,
                      "Command buffer not in recording status");

    if (command_buffer_ptr->status != RAL_COMMAND_BUFFER_STATUS_RECORDING)
    {
        goto end;
    }

    new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

    new_command_ptr->set_program_command.new_program = program;
    new_command_ptr->type                            = RAL_COMMAND_TYPE_SET_PROGRAM;

    ral_context_retain_object(command_buffer_ptr->context,
                              RAL_CONTEXT_OBJECT_TYPE_PROGRAM,
                              program);

    system_resizable_vector_push(command_buffer_ptr->commands,
                                 new_command_ptr);

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_color_rendertargets(ral_command_buffer                                            recording_command_buffer,
                                                              uint32_t                                                      n_rendertargets,
                                                              const ral_command_buffer_set_color_rendertarget_command_info* rendertarget_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        new_command_ptr->set_color_rendertarget_command = src_command;
        new_command_ptr->type                           = RAL_COMMAND_TYPE_SET_COLOR_RENDERTARGET;

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_TEXTURE_VIEW,
                                  new_command_ptr->set_color_rendertarget_command.texture_view);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_scissor_boxes(ral_command_buffer                                     recording_command_buffer,
                                                        uint32_t                                               n_scissor_boxes,
                                                        const ral_command_buffer_set_scissor_box_command_info* scissor_box_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

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

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_vertex_buffers(ral_command_buffer                                       recording_command_buffer,
                                                         uint32_t                                                 n_vertex_buffers,
                                                         const ral_command_buffer_set_vertex_buffer_command_info* vertex_buffer_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        new_command_ptr->set_vertex_buffer_command = src_command;
        new_command_ptr->type                      = RAL_COMMAND_TYPE_SET_VERTEX_BUFFER;

        ral_context_retain_object(command_buffer_ptr->context,
                                  RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                  src_command.buffer);

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_record_set_viewports(ral_command_buffer                                  recording_command_buffer,
                                                    uint32_t                                            n_viewports,
                                                    const ral_command_buffer_set_viewport_command_info* viewport_ptrs)
{
    _ral_command_buffer* command_buffer_ptr = (_ral_command_buffer*) recording_command_buffer;
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

        new_command_ptr = (_ral_command*) system_resource_pool_get_from_pool(command_pool);

        memcpy(new_command_ptr->set_viewport_command.size,
               src_command.size,
               sizeof(src_command.size) );
        memcpy(new_command_ptr->set_viewport_command.xy,
               src_command.xy,
               sizeof(src_command.xy) );

        new_command_ptr->set_viewport_command.index = src_command.index;
        new_command_ptr->type                       = RAL_COMMAND_TYPE_SET_VIEWPORT;

        system_resizable_vector_push(command_buffer_ptr->commands,
                                     new_command_ptr);
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer)
{
    ASSERT_DEBUG_SYNC(command_buffer != nullptr,
                      "Input ral_command_buffer instance is NULL");

    system_resource_pool_return_to_pool(command_buffer_pool,
                                        (system_resource_pool_block) command_buffer);
}

