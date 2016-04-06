/**
 *
 * Emerald (kbi/elude @2016)
 *
 */
#include "shared.h"
#include "ral/ral_buffer.h"
#include "ral/ral_command_buffer.h"
#include "ral/ral_context.h"
#include "ral/ral_sampler.h"
#include "ral/ral_texture.h"
#include "system/system_resizable_vector.h"
#include "system/system_resource_pool.h"

#define N_MAX_PREALLOCED_COMMANDS (32)


PRIVATE system_resource_pool command_buffer_pool = nullptr; /* holds _ral_command_buffer instances */
PRIVATE system_resource_pool command_pool        = nullptr; /* holds _ral_command instances        */


typedef enum
{
    RAL_COMMAND_TYPE_COPY_TEXTURE_TO_TEXTURE,
    RAL_COMMAND_TYPE_DRAW_CALL_INDEXED,
    RAL_COMMAND_TYPE_DRAW_CALL_INDIRECT,
    RAL_COMMAND_TYPE_DRAW_CALL_REGULAR,
    RAL_COMMAND_TYPE_SET_BINDING,
    RAL_COMMAND_TYPE_SET_RENDERTARGET_STATE,
    RAL_COMMAND_TYPE_SET_SCISSOR_BOX,
    RAL_COMMAND_TYPE_SET_VERTEX_ATTRIBUTE,
    RAL_COMMAND_TYPE_SET_VIEWPORT,

    RAL_COMMAND_TYPE_UNKNOWN
} _ral_command_type;

typedef struct _ral_command
{
    _ral_command_type type;

    union
    {
        ral_command_buffer_copy_texture_to_texture_command_info copy_texture_to_texture_command;
        ral_command_buffer_draw_call_indexed_command_info       draw_call_indexed_command;
        ral_command_buffer_draw_call_indirect_command_info      draw_call_indirect_command;
        ral_command_buffer_draw_call_regular_command_info       draw_call_regular_command;
        ral_command_buffer_set_binding_command_info             set_binding_command;
        ral_command_buffer_set_rendertarget_state_command_info  set_rendertarget_state_command;
        ral_command_buffer_set_scissor_box_command_info         set_scissor_box_command;
        ral_command_buffer_set_vertex_attribute_command_info    set_vertex_attribute_command;
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

                ral_buffer_get_property(draw_call_indirect_command.buffer,
                                        RAL_BUFFER_PROPERTY_CONTEXT,
                                       &buffer_context);

                ral_context_delete_objects(buffer_context,
                                           RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &draw_call_indirect_command.buffer);

                break;
            }

            case RAL_COMMAND_TYPE_SET_BINDING:
            {
                switch (set_binding_command.type)
                {
                    case RAL_BINDING_TYPE_SAMPLED_IMAGE:
                    case RAL_BINDING_TYPE_STORAGE_IMAGE:
                    {
                        ral_context texture_context = nullptr;

                        ral_texture_get_property  (set_binding_command.texture,
                                                   RAL_TEXTURE_PROPERTY_CONTEXT,
                                                  &texture_context);
                        ral_context_delete_objects(texture_context,
                                                   RAL_CONTEXT_OBJECT_TYPE_TEXTURE,
                                                   1, /* n_objects */
                                                   (const void**) &set_binding_command.texture);

                        if (set_binding_command.type == RAL_BINDING_TYPE_SAMPLED_IMAGE)
                        {
                            ral_context sampler_context = nullptr;

                            ral_sampler_get_property  (set_binding_command.sampler,
                                                       RAL_SAMPLER_PROPERTY_CONTEXT,
                                                      &sampler_context);
                            ral_context_delete_objects(sampler_context,
                                                       RAL_CONTEXT_OBJECT_TYPE_SAMPLER,
                                                       1, /* n_objects */
                                                       (const void**) &set_binding_command.sampler);
                        }

                        break;
                    }

                    case RAL_BINDING_TYPE_STORAGE_BUFFER:
                    case RAL_BINDING_TYPE_UNIFORM_BUFFER:
                    {
                        ral_context buffer_context = nullptr;

                        ral_buffer_get_property   (set_binding_command.buffer,
                                                   RAL_BUFFER_PROPERTY_CONTEXT,
                                                  &buffer_context);
                        ral_context_delete_objects(buffer_context,
                                                   RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                                   1, /* n_objects */
                                                   (const void**) &set_binding_command.buffer);

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

            case RAL_COMMAND_TYPE_SET_VERTEX_ATTRIBUTE:
            {
                ral_context buffer_context = nullptr;

                ral_buffer_get_property   (set_vertex_attribute_command.buffer,
                                           RAL_BUFFER_PROPERTY_CONTEXT,
                                          &buffer_context);
                ral_context_delete_objects(buffer_context,
                                           RAL_CONTEXT_OBJECT_TYPE_BUFFER,
                                           1, /* n_objects */
                                           (const void**) &set_vertex_attribute_command.buffer);

                break;
            }

            /* Dummy */
            case RAL_COMMAND_TYPE_DRAW_CALL_REGULAR:
            case RAL_COMMAND_TYPE_SET_RENDERTARGET_STATE:
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

typedef struct _ral_command_buffer
{
    system_resizable_vector commands;
    ral_context             context;

    void clear_commands()
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
} _ral_command_buffer;


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

    system_resizable_vector_release(cmd_buffer_ptr->commands);
}

/** TODO */
PRIVATE void _ral_command_buffer_init_command_buffer(system_resource_pool_block block)
{
    _ral_command_buffer* cmd_buffer_ptr = (_ral_command_buffer*) block; 

    cmd_buffer_ptr->commands = system_resizable_vector_create(N_MAX_PREALLOCED_COMMANDS);
    cmd_buffer_ptr->context  = nullptr;
}


/** Please see header for specification */
PUBLIC ral_command_buffer ral_command_buffer_create(ral_context context)
{
    _ral_command_buffer* new_command_buffer_ptr = (_ral_command_buffer*) system_resource_pool_get_from_pool(command_buffer_pool);

    ASSERT_ALWAYS_SYNC(new_command_buffer_ptr != NULL,
                       "Could not retrieve a command buffer from the command buffer pool");

    if (new_command_buffer_ptr != NULL)
    {
        new_command_buffer_ptr->clear_commands();

        new_command_buffer_ptr->context = context;
    }

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
}

/** Please see header for specification */
PUBLIC void ral_command_buffer_release(ral_command_buffer command_buffer)
{
    ASSERT_DEBUG_SYNC(command_buffer != NULL,
                      "Input ral_command_buffer instance is NULL");

    system_resource_pool_return_to_pool(command_buffer_pool,
                                        (system_resource_pool_block) command_buffer);
}