/**
 *
 * Emerald (kbi/elude @2015)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "system/system_callback_manager.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

typedef struct _ral_program_shader_stage
{
    bool       active;
    ral_shader shader;

    _ral_program_shader_stage()
    {
        active = false;
        shader = NULL;
    }

    _ral_program_shader_stage(bool in_active)
    {
        active = in_active;
        shader = NULL;
    }

} _ral_program_shader_stage;

typedef struct _ral_program_metadata_block
{
    system_hashed_ansi_string name;
    ral_program_block_type    type;
    system_resizable_vector   variables;                   /* holds ral_program_variable instances */
    system_hash64map          variables_by_name_hashmap;   /* does NOT own ral_program_variable instance values */
    system_hash64map          variables_by_offset_hashmap; /* does NOT own ral_program_variable instance values; only not NULL for blocks with name != "" */

    explicit _ral_program_metadata_block(system_hashed_ansi_string in_name,
                                         ral_program_block_type    in_type)
    {
        name                        = in_name;
        type                        = in_type;
        variables                   = system_resizable_vector_create(4 /* capacity */);
        variables_by_name_hashmap   = system_hash64map_create       (sizeof(ral_program_variable*) );

        if (system_hashed_ansi_string_get_length(in_name) > 0)
        {
            variables_by_offset_hashmap = system_hash64map_create(sizeof(ral_program_variable*));
        }
        else
        {
            variables_by_offset_hashmap = NULL;
        }
    }

    ~_ral_program_metadata_block()
    {
        if (variables != NULL)
        {
            ral_program_variable* current_variable_ptr = NULL;

            while (system_resizable_vector_pop(variables,
                                              &current_variable_ptr) )
            {
                delete current_variable_ptr;

                current_variable_ptr = NULL;
            }

            system_resizable_vector_release(variables);
            variables = NULL;
        } /* if (variables != NULL) */

        if (variables_by_name_hashmap != NULL)
        {
            system_hash64map_release(variables_by_name_hashmap);

            variables_by_name_hashmap = NULL;
        } /* if (variables_by_name_hashmap != NULL) */

        if (variables_by_offset_hashmap != NULL)
        {
            system_hash64map_release(variables_by_offset_hashmap);

            variables_by_offset_hashmap = NULL;
        } /* if (variables_by_offset_hashmap != NULL) */
    }
} _ral_program_metadata_block;

typedef struct _ral_program_metadata
{
    system_resizable_vector attributes;                  /* holds ral_program_attribute instances */
    system_hash64map        attributes_by_name_hashmap;  /* does NOT own ral_program_attribute instance values */
    system_resizable_vector blocks;                      /* owns _ral_program_metadata_block instances */
    system_hash64map        blocks_by_name_hashmap;      /* does NOT own _ral_program_metadata_block instance values */
    ral_program             owner_program;

    explicit _ral_program_metadata(ral_program in_owner_program)
    {
        attributes                  = system_resizable_vector_create(4 /* capacity */);
        attributes_by_name_hashmap  = system_hash64map_create       (sizeof(ral_program_attribute*) );
        blocks                      = system_resizable_vector_create(4 /* capacity */);
        blocks_by_name_hashmap      = system_hash64map_create       (sizeof(_ral_program_metadata_block*) );
        owner_program               = in_owner_program;
    }

    ~_ral_program_metadata()
    {
        ral_program_clear_metadata(owner_program);

        if (attributes != NULL)
        {
            ral_program_attribute* current_attribute_ptr = NULL;

            while (system_resizable_vector_pop(attributes,
                                              &current_attribute_ptr) )
            {
                delete current_attribute_ptr;
            }

            system_resizable_vector_release(attributes);
            attributes = NULL;
        } /* if (attributes != NULL) */

        if (attributes_by_name_hashmap != NULL)
        {
            system_hash64map_release(attributes_by_name_hashmap);

            attributes_by_name_hashmap = NULL;
        } /* if (attributes_by_name_hashmap != NULL) */

        if (blocks != NULL)
        {
            system_resizable_vector_release(blocks);
            blocks = NULL;
        } /* if (blocks != NULL) */

        if (blocks_by_name_hashmap != NULL)
        {
            system_hash64map_release(blocks_by_name_hashmap);

            blocks_by_name_hashmap = NULL;
        } /* if (blocks_by_name_hashmap != NULL) */
    }
} _ral_program_metadata;

typedef struct _ral_program
{
    _ral_program_shader_stage attached_shaders[RAL_SHADER_TYPE_COUNT];
    system_callback_manager   callback_manager;
    ral_context               context;
    _ral_program_metadata     metadata;
    system_hashed_ansi_string name;

    _ral_program(ral_context                    in_context,
                 const ral_program_create_info* program_create_info_ptr)
        :metadata( (ral_program) this)
    {
        attached_shaders[RAL_SHADER_TYPE_COMPUTE]                 = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE)         != 0);
        attached_shaders[RAL_SHADER_TYPE_FRAGMENT]                = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_FRAGMENT)        != 0);
        attached_shaders[RAL_SHADER_TYPE_GEOMETRY]                = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_GEOMETRY)        != 0);
        attached_shaders[RAL_SHADER_TYPE_TESSELLATION_CONTROL]    = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_TESS_CONTROL)    != 0);
        attached_shaders[RAL_SHADER_TYPE_TESSELLATION_EVALUATION] = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_TESS_EVALUATION) != 0);
        attached_shaders[RAL_SHADER_TYPE_VERTEX]                  = _ral_program_shader_stage( (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX)          != 0);

        callback_manager = system_callback_manager_create( (_callback_id) RAL_PROGRAM_CALLBACK_ID_COUNT);
        context          = in_context;
        name             = program_create_info_ptr->name;
    }

    ~_ral_program()
    {
        uint32_t   n_shaders_to_delete = 0;
        ral_shader shaders_to_delete[RAL_SHADER_TYPE_COUNT];

        for (uint32_t n_shader_type = 0;
                      n_shader_type < RAL_SHADER_TYPE_COUNT;
                    ++n_shader_type)
        {
            if (attached_shaders[n_shader_type].shader != NULL)
            {
                shaders_to_delete[n_shaders_to_delete++] = attached_shaders[n_shader_type].shader;
            }
        } /* for (all shader types) */

        if (n_shaders_to_delete > 0)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                       n_shaders_to_delete,
                                       (const void**) shaders_to_delete);
        }

        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        }
    }
} _ral_program;



/** Please see header for specification */
PUBLIC void ral_program_add_metadata_block(ral_program               program,
                                           ral_program_block_type    block_type,
                                           system_hashed_ansi_string block_name)
{
    const system_hash64 block_name_hash = system_hashed_ansi_string_get_hash(block_name);
    _ral_program*       program_ptr     = (_ral_program*) program;

    /* Sanity checks */
    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(program != NULL,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Make sure the block is not already recognized */
    if (system_hash64map_contains(program_ptr->metadata.blocks_by_name_hashmap,
                                  block_name_hash) )
    {
        const char* block_name_raw_ptr   = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] already has a metadata block [%s] added.",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Spawn a new descriptor */
    _ral_program_metadata_block* new_block_ptr = new (std::nothrow) _ral_program_metadata_block(block_name,
                                                                                                block_type);

    if (new_block_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(new_block_ptr != NULL,
                           "Out of memory");

        goto end;
    }

    system_resizable_vector_push(program_ptr->metadata.blocks,
                                 new_block_ptr);
    system_hash64map_insert     (program_ptr->metadata.blocks_by_name_hashmap,
                                 block_name_hash,
                                 new_block_ptr,
                                 NULL,  /* callback          */
                                 NULL); /* callback_argument */

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_program_attach_variable_to_metadata_block(ral_program               program,
                                                          system_hashed_ansi_string block_name,
                                                          ral_program_variable*     variable_ptr)
{
    const system_hash64          block_name_hash    = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr          = NULL;
    _ral_program*                program_ptr        = (_ral_program*) program;
    system_hash64                variable_name_hash = 0;

    /* Sanity checks */
    if (program_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(program_ptr != NULL,
                          "Input RAL program instance is NULL");

        goto end;
    }

    if (variable_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(variable_ptr != NULL,
                          "Input RAL program variable instance is NULL");

        goto end;
    }
    else
    {
        variable_name_hash = system_hashed_ansi_string_get_hash(variable_ptr->name);
    }

    /* Retrieve the block descriptor */
    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr   = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program does not have a metadata block [%s] added",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Make sure the variable has not already been added */
    if (                                                  system_hash64map_contains(block_ptr->variables_by_name_hashmap,
                                                                                    variable_name_hash)                     ||
        block_ptr->variables_by_offset_hashmap != NULL && system_hash64map_contains(block_ptr->variables_by_offset_hashmap,
                                                                                    variable_ptr->block_offset) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);
        const char* variable_name_raw_ptr = system_hashed_ansi_string_get_buffer(variable_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "Metadata block [%s] of a RAL program [%s] already has variable [%s] attached.",
                          block_name_raw_ptr,
                          program_name_raw_ptr,
                          variable_name_raw_ptr);

        goto end;
    }

    /* Append the specified variable descriptor */
    system_resizable_vector_push(block_ptr->variables,
                                 variable_ptr);
    system_hash64map_insert     (block_ptr->variables_by_name_hashmap,
                                 variable_name_hash,
                                 variable_ptr,
                                 NULL,  /* callback          */
                                 NULL); /* callback_argument */

    if (block_ptr->variables_by_offset_hashmap != NULL)
    {
        system_hash64map_insert(block_ptr->variables_by_offset_hashmap,
                                variable_ptr->block_offset,
                                variable_ptr,
                                NULL,  /* callback          */
                                NULL); /* callback_argument */
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_program_attach_vertex_attribute(ral_program            program,
                                                ral_program_attribute* attribute_ptr)
{
    system_hash64 attribute_name_hash = 0;
    _ral_program* program_ptr         = (_ral_program*) program;

    /* Sanity checks */
    if (program_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(program_ptr != NULL,
                          "Input RAL program instance is NULL");

        goto end;
    }

    if (attribute_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(attribute_ptr != NULL,
                          "Input RAL attribute instance is NULL");

        goto end;
    }
    else
    {
        attribute_name_hash = system_hashed_ansi_string_get_hash(attribute_ptr->name);
    }

    /* Make sure the variable has not already been added */
    if (system_hash64map_contains(program_ptr->metadata.attributes_by_name_hashmap,
                                  attribute_name_hash)  )
    {
        const char* attribute_name_raw_ptr = system_hashed_ansi_string_get_buffer(attribute_ptr->name);
        const char* program_name_raw_ptr   = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] already has attribute [%s] attached.",
                          program_name_raw_ptr,
                          attribute_name_raw_ptr);

        goto end;
    }

    /* Append the specified variable descriptor */
    system_resizable_vector_push(program_ptr->metadata.attributes,
                                 attribute_ptr);
    system_hash64map_insert     (program_ptr->metadata.attributes_by_name_hashmap,
                                 attribute_name_hash,
                                 attribute_ptr,
                                 NULL,  /* callback          */
                                 NULL); /* callback_argument */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_attach_shader(ral_program program,
                                                  ral_shader  shader)
{
    bool                                                  all_shaders_attached = true;
    _ral_program_callback_shader_attach_callback_argument callback_arg;
    _ral_program*                                         program_ptr  = (_ral_program*) program;
    bool                                                  result       = false;
    ral_shader_type                                       shader_type;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_program instance is NULL");

        goto end;
    }

    if (shader == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_shader instance is NULL");

        goto end;
    }

    /* Verify there's no shader already attached to the slot which the specified shader
     * is about to take over */
    ral_shader_get_property(shader,
                            RAL_SHADER_PROPERTY_TYPE,
                           &shader_type);

    if (!program_ptr->attached_shaders[shader_type].active)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Cannot attach RAL shader - the target RAL program does not expose the required shader stage slot.");

        goto end;
    }

    if (program_ptr->attached_shaders[shader_type].shader != NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Another shader is already attached to the pipeline stage the input shader defines.");

        goto end;
    }

    /* Attach the shader and fire a notification */
    program_ptr->attached_shaders[shader_type].shader = shader;

    for (ral_shader_type current_shader_type = RAL_SHADER_TYPE_FIRST;
                         current_shader_type < RAL_SHADER_TYPE_COUNT;
                 ++(int&)current_shader_type)
    {
        if (program_ptr->attached_shaders[current_shader_type].active         &&
            program_ptr->attached_shaders[current_shader_type].shader == NULL)
        {
            all_shaders_attached = false;

            break;
        }
    }

    callback_arg = _ral_program_callback_shader_attach_callback_argument(program,
                                                                         shader,
                                                                         all_shaders_attached);

    ral_context_retain_object(program_ptr->context,
                              RAL_CONTEXT_OBJECT_TYPE_SHADER,
                              shader);

    system_callback_manager_call_back(program_ptr->callback_manager,
                                      RAL_PROGRAM_CALLBACK_ID_SHADER_ATTACHED,
                                     &callback_arg);

    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC ral_program ral_program_create(ral_context                    context,
                                      const ral_program_create_info* program_create_info_ptr)
{
    _ral_program* program_ptr = NULL;

    /* Sanity checks */
    if (program_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(!(program_create_info_ptr == NULL),
                          "Input program create info is NULL");

        goto end;
    }

    if (program_create_info_ptr->active_shader_stages == 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No shader stages defined at RAL program creation time");

        goto end;
    }
    else
    if ((program_create_info_ptr->active_shader_stages &  RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE) != 0 &&
        (program_create_info_ptr->active_shader_stages & ~RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE) != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "No shader stages apart from the compute stage can be defined for a compute RAL program");

        goto end;
    }
    else
    if ((program_create_info_ptr->active_shader_stages &  RAL_PROGRAM_SHADER_STAGE_BIT_COMPUTE) == 0)
    {
        if ((program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_VERTEX) == 0)
        {
            ASSERT_DEBUG_SYNC(false,
                              "Vertex shader stage is required for a graphics RAL program");

            goto end;
        }

        if ((program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_TESS_CONTROL)   ^
            (program_create_info_ptr->active_shader_stages & RAL_PROGRAM_SHADER_STAGE_BIT_TESS_EVALUATION))
        {
            ASSERT_DEBUG_SYNC(false,
                              "Tessellation control shader stage must be paired with a tessellation evaluation "
                              "shader stage for a graphics RAL program");

            goto end;
        }
    }

    /* Create a new program instance */
    program_ptr = new (std::nothrow) _ral_program(context,
                                                  program_create_info_ptr);

    ASSERT_ALWAYS_SYNC(program_ptr != NULL,
                       "Out of memory");

end:
    return (ral_program) program_ptr;
}

/** Please see header for specification */
PUBLIC void ral_program_clear_metadata(ral_program program)
{
    _ral_program* program_ptr = (_ral_program*) program;

    if (program_ptr->metadata.attributes != NULL)
    {
        ral_program_attribute* current_attribute_ptr = NULL;

        while (system_resizable_vector_pop(program_ptr->metadata.attributes,
                                          &current_attribute_ptr) )
        {
            delete current_attribute_ptr;

            current_attribute_ptr = NULL;
        }
    }

    if (program_ptr->metadata.blocks != NULL)
    {
        _ral_program_metadata_block* current_block_ptr = NULL;

        while (system_resizable_vector_pop(program_ptr->metadata.blocks,
                                          &current_block_ptr) )
        {
            delete current_block_ptr;

            current_block_ptr = NULL;
        }
    } /* if (program_ptr->metadata.blocks != NULL) */

    if (program_ptr->metadata.attributes_by_name_hashmap != NULL)
    {
        system_hash64map_clear(program_ptr->metadata.attributes_by_name_hashmap);
    }

    if (program_ptr->metadata.blocks_by_name_hashmap != NULL)
    {
        system_hash64map_clear(program_ptr->metadata.blocks_by_name_hashmap);
    } /* if (program_ptr->metadata.blocks_by_name_hashmap != NULL) */
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_attached_shader_at_index(ral_program program,
                                                                 uint32_t    n_shader,
                                                                 ral_shader* out_shader_ptr)
{
    uint32_t      n_nonnull_shader = 0;
    _ral_program* program_ptr      = (_ral_program*) program;
    bool          result           = false;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_program instance is NULL");

        goto end;
    }

    if (n_shader >= RAL_SHADER_TYPE_COUNT)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid shader index requested");

        goto end;
    }

    for (uint32_t n_shader_type = 0;
                  n_shader_type < RAL_SHADER_TYPE_COUNT;
                ++n_shader_type)
    {
        if (program_ptr->attached_shaders[n_shader_type].shader != NULL)
        {
            if (n_nonnull_shader == n_shader)
            {
                *out_shader_ptr = program_ptr->attached_shaders[n_shader_type].shader;

                result = true;
                goto end;
            }

            ++n_nonnull_shader;
        }
    } /* for (all available shader types) */

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_property(ral_program                program,
                                                       system_hashed_ansi_string  block_name,
                                                       ral_program_block_property property,
                                                       void*                      out_result_ptr)
{
    const system_hash64          block_name_hash = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr       = NULL;
    _ral_program*                program_ptr     = (_ral_program*) program;
    bool                         result          = false;

    /* Sanity checks */
    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] does not have a block [%s] assigned.",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Retrieve the requested property value */
    switch (property)
    {
        case RAL_PROGRAM_BLOCK_PROPERTY_N_VARIABLES:
        {
            system_resizable_vector_get_property(block_ptr->variables,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PROGRAM_BLOCK_PROPERTY_TYPE:
        {
            *(ral_program_block_type*) out_result_ptr = block_ptr->type;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_block_property value.");

            goto end;
        }
    } /* switch (property) */

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_variable_by_index(ral_program                  program,
                                                                system_hashed_ansi_string    block_name,
                                                                uint32_t                     n_variable,
                                                                const ral_program_variable** out_variable_ptr_ptr)
{
    const system_hash64          block_name_hash = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr       = NULL;
    _ral_program*                program_ptr     = (_ral_program*) program;
    bool                         result          = false;
    ral_program_variable*        variable_ptr    = NULL;

    /* Sanity checks */
    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] does not have a block [%s] assigned.",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Retrieve the requested variable descriptor */
    if (!system_resizable_vector_get_element_at(block_ptr->variables,
                                                n_variable,
                                               &variable_ptr) )
    {
        const char* block_name_raw_ptr = system_hashed_ansi_string_get_buffer(block_name);

        ASSERT_DEBUG_SYNC(false,
                          "Block [%s] does not hold any variable at index [%d]",
                          block_name_raw_ptr,
                          n_variable);

        goto end;
    }

    *out_variable_ptr_ptr = variable_ptr;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_variable_by_offset(ral_program                 program,
                                                                system_hashed_ansi_string    block_name,
                                                                uint32_t                     variable_block_offset,
                                                                const ral_program_variable** out_variable_ptr_ptr)
{
    const system_hash64          block_name_hash    = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr          = NULL;
    _ral_program*                program_ptr        = (_ral_program*) program;
    bool                         result             = false;
    ral_program_variable*        variable_ptr       = NULL;

    /* Sanity checks */
    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] does not have a block [%s] assigned.",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Retrieve the requested variable descriptor */
    if (!system_hash64map_get(block_ptr->variables_by_offset_hashmap,
                              variable_block_offset,
                              &variable_ptr) )
    {
        const char* block_name_raw_ptr = system_hashed_ansi_string_get_buffer(block_name);

        ASSERT_DEBUG_SYNC(false,
                          "Block [%s] does not hold any variable at offset [%d]",
                          block_name_raw_ptr,
                          variable_block_offset);

        goto end;
    }

    *out_variable_ptr_ptr = variable_ptr;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_variable_by_name(ral_program                  program,
                                                               system_hashed_ansi_string    block_name,
                                                               system_hashed_ansi_string    variable_name,
                                                               const ral_program_variable** out_variable_ptr_ptr)
{
    const system_hash64          block_name_hash    = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr          = NULL;
    _ral_program*                program_ptr        = (_ral_program*) program;
    bool                         result             = false;
    const system_hash64          variable_name_hash = system_hashed_ansi_string_get_hash(variable_name);
    ral_program_variable*        variable_ptr       = NULL;

    /* Sanity checks */
    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);

        ASSERT_DEBUG_SYNC(false,
                          "RAL program [%s] does not have a block [%s] assigned.",
                          program_name_raw_ptr,
                          block_name_raw_ptr);

        goto end;
    }

    /* Retrieve the requested variable descriptor */
    if (!system_hash64map_get(block_ptr->variables_by_name_hashmap,
                              variable_name_hash,
                             &variable_ptr) )
    {
        goto end;
    }

    *out_variable_ptr_ptr = variable_ptr;

    /* All done */
    result = true;
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ral_program_get_property(ral_program          program,
                                                 ral_program_property property,
                                                 void*                out_result_ptr)
{
    _ral_program* program_ptr = (_ral_program*) program;

    if (program == NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_program instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_PROGRAM_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = program_ptr->callback_manager;

            break;
        }

        case RAL_PROGRAM_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = program_ptr->name;

            break;
        }

        case RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS:
        {
            uint32_t result = 0;

            for (uint32_t n_shader_type = 0;
                          n_shader_type < RAL_SHADER_TYPE_COUNT;
                        ++n_shader_type)
            {
                if (program_ptr->attached_shaders[n_shader_type].shader != NULL)
                {
                    ++result;
                }
            } /* for (all shader types) */

            *(uint32_t*) out_result_ptr = result;

            break;
        }

        case RAL_PROGRAM_PROPERTY_N_METADATA_BLOCKS:
        {
            system_resizable_vector_get_property(program_ptr->metadata.blocks,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_property value.")
        }
    } /* switch (property) */

end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_program_release(ral_program& program)
{
    _ral_program* program_ptr = (_ral_program*) program;

    delete program_ptr;
}