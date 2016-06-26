/**
 *
 * Emerald (kbi/elude @2015-2016)
 *
 */
#include "shared.h"
#include "ral/ral_context.h"
#include "ral/ral_program.h"
#include "ral/ral_shader.h"
#include "ral/ral_utils.h"
#include "system/system_callback_manager.h"
#include "system/system_event.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include "system/system_read_write_mutex.h"

typedef struct _ral_program_shader_stage
{
    bool       active;
    ral_shader shader;

    _ral_program_shader_stage()
    {
        active = false;
        shader = nullptr;
    }

    _ral_program_shader_stage(bool in_active)
    {
        active = in_active;
        shader = nullptr;
    }

} _ral_program_shader_stage;

typedef struct _ral_program_metadata_block
{
    system_hashed_ansi_string name;
    uint32_t                  size;
    ral_program_block_type    type;
    system_resizable_vector   variables;                         /* holds ral_program_variable instances */
    system_hash64map          variable_vector_by_class_hashmap;  /* holds system_resizable_vector instances; each vector does NOT own ral_program_variable instance values */
    system_hash64map          variables_by_name_hashmap;         /* does NOT own ral_program_variable instance values */
    system_hash64map          variables_by_offset_hashmap;       /* does NOT own ral_program_variable instance values; only not NULL for blocks with name != "" */

    explicit _ral_program_metadata_block(system_hashed_ansi_string in_name,
                                         uint32_t                  in_size,
                                         ral_program_block_type    in_type)
    {
        name                             = in_name;
        size                             = in_size;
        type                             = in_type;
        variables                        = system_resizable_vector_create(4 /* capacity */);
        variables_by_name_hashmap        = system_hash64map_create       (sizeof(ral_program_variable*) );
        variable_vector_by_class_hashmap = system_hash64map_create       (sizeof(system_resizable_vector) );
        variables_by_name_hashmap        = nullptr;
        variables_by_offset_hashmap      = nullptr;

        if (system_hashed_ansi_string_get_length(in_name) > 0)
        {
            variables_by_offset_hashmap = system_hash64map_create(sizeof(ral_program_variable*));
        }
        else
        {
            variables_by_offset_hashmap = nullptr;
        }
    }

    ~_ral_program_metadata_block()
    {
        if (variables != nullptr)
        {
            ral_program_variable* current_variable_ptr = nullptr;

            while (system_resizable_vector_pop(variables,
                                              &current_variable_ptr) )
            {
                delete current_variable_ptr;

                current_variable_ptr = nullptr;
            }

            system_resizable_vector_release(variables);
            variables = nullptr;
        }

        if (variable_vector_by_class_hashmap != nullptr)
        {
            uint32_t n_vectors = 0;

            system_hash64map_get_property(variable_vector_by_class_hashmap,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                         &n_vectors);

            for (uint32_t n_vector = 0;
                          n_vector < n_vectors;
                        ++n_vector)
            {
                system_resizable_vector current_vector = nullptr;

                system_hash64map_get_element_at(variable_vector_by_class_hashmap,
                                                n_vector,
                                               &current_vector,
                                                nullptr); /* result_hash_ptr */

                system_resizable_vector_release(current_vector);
            }

            system_hash64map_release(variable_vector_by_class_hashmap);
        }

        if (variables_by_name_hashmap != nullptr)
        {
            system_hash64map_release(variables_by_name_hashmap);

            variables_by_name_hashmap = nullptr;
        }

        if (variables_by_offset_hashmap != nullptr)
        {
            system_hash64map_release(variables_by_offset_hashmap);

            variables_by_offset_hashmap = nullptr;
        }
    }
} _ral_program_metadata_block;

typedef struct _ral_program_metadata
{
    system_resizable_vector attributes;                  /* holds ral_program_attribute instances */
    system_hash64map        attributes_by_name_hashmap;  /* does NOT own ral_program_attribute instance values */
    system_resizable_vector blocks;                      /* owns _ral_program_metadata_block instances */
    system_hash64map        blocks_by_name_hashmap;      /* does NOT own _ral_program_metadata_block instance values */
    system_hash64map        blocks_ssb_by_name_hashmap;  /* does NOT own _ral_program_metadata_block instance values */
    system_hash64map        blocks_ub_by_name_hashmap;   /* does NOT own _ral_program_metadata_block instance values */
    system_event            metadata_ready_event;
    ral_program             owner_program;

    explicit _ral_program_metadata(ral_program in_owner_program)
    {
        attributes                  = system_resizable_vector_create(4 /* capacity */);
        attributes_by_name_hashmap  = system_hash64map_create       (sizeof(ral_program_attribute*) );
        blocks                      = system_resizable_vector_create(4 /* capacity */);
        blocks_by_name_hashmap      = system_hash64map_create       (sizeof(_ral_program_metadata_block*) );
        blocks_ssb_by_name_hashmap  = system_hash64map_create       (sizeof(_ral_program_metadata_block*) );
        blocks_ub_by_name_hashmap   = system_hash64map_create       (sizeof(_ral_program_metadata_block*) );
        metadata_ready_event        = system_event_create           (true); /* manual_reset */
        owner_program               = in_owner_program;
    }

    ~_ral_program_metadata()
    {
        ral_program_clear_metadata(owner_program);

        if (attributes != nullptr)
        {
            ral_program_attribute* current_attribute_ptr = nullptr;

            while (system_resizable_vector_pop(attributes,
                                              &current_attribute_ptr) )
            {
                delete current_attribute_ptr;
            }

            system_resizable_vector_release(attributes);
            attributes = nullptr;
        }

        if (attributes_by_name_hashmap != nullptr)
        {
            system_hash64map_release(attributes_by_name_hashmap);

            attributes_by_name_hashmap = nullptr;
        }

        if (blocks != nullptr)
        {
            system_resizable_vector_release(blocks);
            blocks = nullptr;
        }

        if (blocks_by_name_hashmap != nullptr)
        {
            system_hash64map_release(blocks_by_name_hashmap);

            blocks_by_name_hashmap = nullptr;
        }

        if (blocks_ssb_by_name_hashmap != nullptr)
        {
            system_hash64map_release(blocks_ssb_by_name_hashmap);

            blocks_ssb_by_name_hashmap = nullptr;
        }

        if (blocks_ub_by_name_hashmap != nullptr)
        {
            system_hash64map_release(blocks_ub_by_name_hashmap);

            blocks_ub_by_name_hashmap = nullptr;
        }

        if (metadata_ready_event != nullptr)
        {
            system_event_release(metadata_ready_event);

            metadata_ready_event = nullptr;
        }
    }
} _ral_program_metadata;

typedef struct _ral_program
{
    _ral_program_shader_stage attached_shaders[RAL_SHADER_TYPE_COUNT];
    system_callback_manager   callback_manager;
    ral_context               context;
    system_read_write_mutex   lock;
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
        lock             = system_read_write_mutex_create();
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
            if (attached_shaders[n_shader_type].shader != nullptr)
            {
                shaders_to_delete[n_shaders_to_delete++] = attached_shaders[n_shader_type].shader;
            }
        }

        if (lock != nullptr)
        {
            system_read_write_mutex_release(lock);

            lock = nullptr;
        }

        if (n_shaders_to_delete > 0)
        {
            ral_context_delete_objects(context,
                                       RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                       n_shaders_to_delete,
                                       (const void**) shaders_to_delete);
        }

        if (callback_manager != nullptr)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = nullptr;
        }
    }
} _ral_program;



/** Please see header for specification */
PUBLIC void ral_program_add_block(ral_program               program,
                                  uint32_t                  block_size,
                                  ral_program_block_type    block_type,
                                  system_hashed_ansi_string block_name)
{
    const system_hash64 block_name_hash             = system_hashed_ansi_string_get_hash(block_name);
    _ral_program*       program_ptr                 = (_ral_program*) program;
    system_hash64map    specialized_by_name_hashmap = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(program != nullptr,
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
                                                                                                block_size,
                                                                                                block_type);

    if (new_block_ptr == nullptr)
    {
        ASSERT_ALWAYS_SYNC(new_block_ptr != nullptr,
                           "Out of memory");

        goto end;
    }

    system_resizable_vector_push(program_ptr->metadata.blocks,
                                 new_block_ptr);
    system_hash64map_insert     (program_ptr->metadata.blocks_by_name_hashmap,
                                 block_name_hash,
                                 new_block_ptr,
                                 nullptr,  /* callback          */
                                 nullptr); /* callback_argument */

    switch (block_type)
    {
        case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER: specialized_by_name_hashmap = program_ptr->metadata.blocks_ssb_by_name_hashmap; break;
        case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER: specialized_by_name_hashmap = program_ptr->metadata.blocks_ub_by_name_hashmap;  break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL program block type.");
        }
    }

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(specialized_by_name_hashmap,
                                                 block_name_hash),
                      "Block [%s] is already registered.",
                      system_hashed_ansi_string_get_buffer(block_name) );

    system_hash64map_insert(specialized_by_name_hashmap,
                            block_name_hash,
                            new_block_ptr,
                            nullptr,  /* callback          */
                            nullptr); /* callback_argument */
end:
    ;
}

/** Please see header for specification */
PUBLIC void ral_program_attach_variable_to_block(ral_program               program,
                                                 system_hashed_ansi_string block_name,
                                                 ral_program_variable*     variable_ptr)
{
    const system_hash64          block_name_hash    = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr          = nullptr;
    _ral_program*                program_ptr        = (_ral_program*) program;
    system_hash64                variable_name_hash = 0;

    /* Sanity checks */
    if (program_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(program_ptr != nullptr,
                          "Input RAL program instance is NULL");

        goto end;
    }

    if (variable_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(variable_ptr != nullptr,
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
    bool                            is_variable_added_to_type_class_vector = false;
    ral_program_variable_type_class variable_type_class;
    system_resizable_vector         variable_type_vector                   = nullptr;

    ral_utils_get_ral_program_variable_type_property(variable_ptr->type,
                                                     RAL_PROGRAM_VARIABLE_TYPE_PROPERTY_CLASS,
                                                     (void**) &variable_type_class);

    if (system_hash64map_get(block_ptr->variable_vector_by_class_hashmap,
                             variable_type_class,
                            &variable_type_vector) )
    {
        uint32_t n_variables_added = 0;

        system_resizable_vector_get_property(variable_type_vector,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_variables_added);

        for (uint32_t n_variable = 0;
                      n_variable < n_variables_added && !is_variable_added_to_type_class_vector;
                    ++n_variable)
        {
            ral_program_variable* current_variable_ptr = nullptr;

            system_resizable_vector_get_element_at(variable_type_vector,
                                                   n_variable,
                                                  &current_variable_ptr);

            if (system_hashed_ansi_string_is_equal_to_hash_string(current_variable_ptr->name,
                                                                  variable_ptr->name) )
            {
                is_variable_added_to_type_class_vector = true;
            }
        }
    }

    if (is_variable_added_to_type_class_vector                                                                                 ||
                                                             system_hash64map_contains(block_ptr->variables_by_name_hashmap,
                                                                                       variable_name_hash)                     ||
        block_ptr->variables_by_offset_hashmap != nullptr && system_hash64map_contains(block_ptr->variables_by_offset_hashmap,
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
                                 nullptr,  /* callback          */
                                 nullptr); /* callback_argument */

    if (block_ptr->variables_by_offset_hashmap != nullptr)
    {
        system_hash64map_insert(block_ptr->variables_by_offset_hashmap,
                                variable_ptr->block_offset,
                                variable_ptr,
                                nullptr,  /* callback          */
                                nullptr); /* callback_argument */
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
    if (program_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(program_ptr != nullptr,
                          "Input RAL program instance is NULL");

        goto end;
    }

    if (attribute_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(attribute_ptr != nullptr,
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
                                 nullptr,  /* callback          */
                                 nullptr); /* callback_argument */

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_attach_shader(ral_program program,
                                                  ral_shader  shader,
                                                  bool        async)
{
    bool                                                  all_shaders_attached = true;
    _ral_program_callback_shader_attach_callback_argument callback_arg;
    _ral_program*                                         program_ptr  = (_ral_program*) program;
    bool                                                  result       = false;
    ral_shader_type                                       shader_type;

    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_program instance is NULL");

        goto end;
    }

    if (shader == nullptr)
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

    if (program_ptr->attached_shaders[shader_type].shader != nullptr)
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
        if (program_ptr->attached_shaders[current_shader_type].active            &&
            program_ptr->attached_shaders[current_shader_type].shader == nullptr)
        {
            all_shaders_attached = false;

            break;
        }
    }

    callback_arg = _ral_program_callback_shader_attach_callback_argument(program,
                                                                         shader,
                                                                         all_shaders_attached,
                                                                         async);

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
PUBLIC void ral_program_clear_metadata(ral_program program)
{
    _ral_program* program_ptr = (_ral_program*) program;

    /* Block any get() calls until metadata is filled by the rendering backend. */
    system_event_reset(program_ptr->metadata.metadata_ready_event);

    if (program_ptr->metadata.attributes != nullptr)
    {
        ral_program_attribute* current_attribute_ptr = nullptr;

        while (system_resizable_vector_pop(program_ptr->metadata.attributes,
                                          &current_attribute_ptr) )
        {
            delete current_attribute_ptr;

            current_attribute_ptr = nullptr;
        }
    }

    if (program_ptr->metadata.blocks != nullptr)
    {
        _ral_program_metadata_block* current_block_ptr = nullptr;

        while (system_resizable_vector_pop(program_ptr->metadata.blocks,
                                          &current_block_ptr) )
        {
            delete current_block_ptr;

            current_block_ptr = nullptr;
        }
    }

    if (program_ptr->metadata.attributes_by_name_hashmap != nullptr)
    {
        system_hash64map_clear(program_ptr->metadata.attributes_by_name_hashmap);
    }

    if (program_ptr->metadata.blocks_by_name_hashmap != nullptr)
    {
        system_hash64map_clear(program_ptr->metadata.blocks_by_name_hashmap);
    }

    if (program_ptr->metadata.blocks_ssb_by_name_hashmap != nullptr)
    {
        system_hash64map_clear(program_ptr->metadata.blocks_ssb_by_name_hashmap);
    }

    if (program_ptr->metadata.blocks_ub_by_name_hashmap != nullptr)
    {
        system_hash64map_clear(program_ptr->metadata.blocks_ub_by_name_hashmap);
    }
}

/** Please see header for specification */
PUBLIC ral_program ral_program_create(ral_context                    context,
                                      const ral_program_create_info* program_create_info_ptr)
{
    _ral_program* program_ptr = nullptr;

    /* Sanity checks */
    if (program_create_info_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(!(program_create_info_ptr == nullptr),
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

    ASSERT_ALWAYS_SYNC(program_ptr != nullptr,
                       "Out of memory");

end:
    return (ral_program) program_ptr;
}

/** Please see header for specification */
PUBLIC void ral_program_expose_metadata(ral_program program)
{
    _ral_program* program_ptr = (_ral_program*) program;

    system_event_set(program_ptr->metadata.metadata_ready_event);
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_attached_shader_at_index(ral_program program,
                                                                 uint32_t    n_shader,
                                                                 ral_shader* out_shader_ptr)
{
    uint32_t      n_nonnull_shader = 0;
    _ral_program* program_ptr      = (_ral_program*) program;
    bool          result           = false;

    if (program == nullptr)
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
        if (program_ptr->attached_shaders[n_shader_type].shader != nullptr)
        {
            if (n_nonnull_shader == n_shader)
            {
                *out_shader_ptr = program_ptr->attached_shaders[n_shader_type].shader;

                result = true;
                goto end;
            }

            ++n_nonnull_shader;
        }
    }

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
    _ral_program_metadata_block* block_ptr       = nullptr;
    _ral_program*                program_ptr     = (_ral_program*) program;
    bool                         result          = false;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

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
        case RAL_PROGRAM_BLOCK_PROPERTY_NAME:
        {
            *(system_hashed_ansi_string*) out_result_ptr = block_ptr->name;

            break;
        }

        case RAL_PROGRAM_BLOCK_PROPERTY_N_SAMPLER_VARIABLES:
        case RAL_PROGRAM_BLOCK_PROPERTY_N_STORAGE_IMAGE_VARIABLES:
        {
            const ral_program_variable_type_class variable_type_class = (property == RAL_PROGRAM_BLOCK_PROPERTY_N_SAMPLER_VARIABLES) ? RAL_PROGRAM_VARIABLE_TYPE_CLASS_SAMPLER
                                                                                                                                     : RAL_PROGRAM_VARIABLE_TYPE_CLASS_IMAGE;
            system_resizable_vector               variable_vector;

            if (system_hash64map_get(block_ptr->variable_vector_by_class_hashmap,
                                     variable_type_class,
                                    &variable_vector) )
            {
                system_resizable_vector_get_property(variable_vector,
                                                     SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                     out_result_ptr);
            }
            else
            {
                *(uint32_t*) out_result_ptr = 0;
            }

            break;
        }

        case RAL_PROGRAM_BLOCK_PROPERTY_N_VARIABLES:
        {
            system_resizable_vector_get_property(block_ptr->variables,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PROGRAM_BLOCK_PROPERTY_SIZE:
        {
            *(uint32_t*) out_result_ptr = block_ptr->size;

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
    }

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_property_by_index(ral_program                program,
                                                                ral_program_block_type     block_type,
                                                                uint32_t                   block_index,
                                                                ral_program_block_property property,
                                                                void*                      out_result_ptr)
{
    _ral_program_metadata_block* block_ptr                   = nullptr;
    _ral_program*                program_ptr                 = (_ral_program*) program;
    system_hash64map             specialized_by_name_hashmap = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end_fail;
    }

    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

    switch (block_type)
    {
        case RAL_PROGRAM_BLOCK_TYPE_STORAGE_BUFFER: specialized_by_name_hashmap = program_ptr->metadata.blocks_ssb_by_name_hashmap; break;
        case RAL_PROGRAM_BLOCK_TYPE_UNIFORM_BUFFER: specialized_by_name_hashmap = program_ptr->metadata.blocks_ub_by_name_hashmap;  break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized RAL program block type specified.");

            goto end_fail;
        }
    }

    if (!system_hash64map_get_element_at(specialized_by_name_hashmap,
                                         block_index,
                                        &block_ptr,
                                         nullptr /* result_hash_ptr */) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "No block defined at index [%d]",
                          block_index);

        goto end_fail;
    }

    return ral_program_get_block_property(program,
                                          block_ptr->name,
                                          property,
                                          out_result_ptr);

end_fail:
    return false;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_get_block_variable_by_class(ral_program                     program,
                                                                system_hashed_ansi_string       block_name,
                                                                ral_program_variable_type_class variable_type_class,
                                                                uint32_t                        n_variable,
                                                                const ral_program_variable**    out_variable_ptr_ptr)
{
    const system_hash64          block_name_hash = system_hashed_ansi_string_get_hash(block_name);
    _ral_program_metadata_block* block_ptr       = nullptr;
    _ral_program*                program_ptr     = (_ral_program*) program;
    bool                         result          = false;
    system_resizable_vector      variable_vector = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

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
    if (!system_hash64map_get(block_ptr->variable_vector_by_class_hashmap,
                              variable_type_class,
                             &variable_vector) )
    {
        system_hashed_ansi_string variable_type_class_name = nullptr;

        ral_utils_get_ral_program_variable_type_class_property(variable_type_class,
                                                               RAL_PROGRAM_VARIABLE_TYPE_CLASS_PROPERTY_NAME,
                                                               (void**) &variable_type_class_name);

        ASSERT_DEBUG_SYNC(false,
                          "No variable of the requested type class [%s] is defined for program [%s].",
                          system_hashed_ansi_string_get_buffer(variable_type_class_name),
                          system_hashed_ansi_string_get_buffer(program_ptr->name) );

        goto end;
    }

    if (!system_resizable_vector_get_element_at(variable_vector,
                                                n_variable,
                                                out_variable_ptr_ptr) )
    {
        system_hashed_ansi_string variable_type_class_name = nullptr;

        ral_utils_get_ral_program_variable_type_class_property(variable_type_class,
                                                               RAL_PROGRAM_VARIABLE_TYPE_CLASS_PROPERTY_NAME,
                                                               (void**) &variable_type_class_name);

        ASSERT_DEBUG_SYNC(false,
                          "Could not retrieve descriptor of variable of the requested type class [%s] at index [%d] for program [%s].",
                          system_hashed_ansi_string_get_buffer(variable_type_class_name),
                          n_variable,
                          system_hashed_ansi_string_get_buffer(program_ptr->name) );

        goto end;
    }

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
    _ral_program_metadata_block* block_ptr       = nullptr;
    _ral_program*                program_ptr     = (_ral_program*) program;
    bool                         result          = false;
    ral_program_variable*        variable_ptr    = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

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
    _ral_program_metadata_block* block_ptr          = nullptr;
    _ral_program*                program_ptr        = (_ral_program*) program;
    bool                         result             = false;
    ral_program_variable*        variable_ptr       = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

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
    _ral_program_metadata_block* block_ptr          = nullptr;
    _ral_program*                program_ptr        = (_ral_program*) program;
    bool                         result             = false;
    const system_hash64          variable_name_hash = system_hashed_ansi_string_get_hash(variable_name);
    ral_program_variable*        variable_ptr       = nullptr;

    /* Sanity checks */
    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input RAL program instance is NULL");

        goto end;
    }

    /* Retrieve the block descriptor */
    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

    if (!system_hash64map_get(program_ptr->metadata.blocks_by_name_hashmap,
                              block_name_hash,
                             &block_ptr) )
    {
        const char* block_name_raw_ptr    = system_hashed_ansi_string_get_buffer(block_name);
        const char* program_name_raw_ptr  = system_hashed_ansi_string_get_buffer(program_ptr->name);

        LOG_ERROR("RAL program [%s] does not have a block [%s] assigned.",
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

    if (program == nullptr)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Input ral_program instance is NULL");

        goto end;
    }

    switch (property)
    {
        case RAL_PROGRAM_PROPERTY_ALL_SHADERS_ATTACHED:
        {
            bool result = true;

            for (uint32_t n_shader_stage = 0;
                          n_shader_stage < RAL_SHADER_TYPE_COUNT;
                        ++n_shader_stage)
            {
                if (program_ptr->attached_shaders[n_shader_stage].active)
                {
                    const ral_shader          shader      = program_ptr->attached_shaders[n_shader_stage].shader;
                    system_hashed_ansi_string shader_body = nullptr;

                    if (shader == nullptr)
                    {
                        result = false;

                        break;
                    }

                    ral_shader_get_property(shader,
                                            RAL_SHADER_PROPERTY_GLSL_BODY,
                                           &shader_body);

                    if (shader_body == nullptr)
                    {
                        result = false;

                        break;
                    }
                }
            }

            *(bool*) out_result_ptr = result;

            break;
        }

        case RAL_PROGRAM_PROPERTY_CALLBACK_MANAGER:
        {
            *(system_callback_manager*) out_result_ptr = program_ptr->callback_manager;

            break;
        }

        case RAL_PROGRAM_PROPERTY_CONTEXT:
        {
            *(ral_context*) out_result_ptr = program_ptr->context;

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

            {
                for (uint32_t n_shader_type = 0;
                              n_shader_type < RAL_SHADER_TYPE_COUNT;
                            ++n_shader_type)
                {
                    if (program_ptr->attached_shaders[n_shader_type].shader != nullptr)
                    {
                        ++result;
                    }
                }
            }

            *(uint32_t*) out_result_ptr = result;

            break;
        }

        case RAL_PROGRAM_PROPERTY_N_BLOCKS:
        {
            system_event_wait_single(program_ptr->metadata.metadata_ready_event);

            system_resizable_vector_get_property(program_ptr->metadata.blocks,
                                                 SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                                 out_result_ptr);

            break;
        }

        case RAL_PROGRAM_PROPERTY_N_STORAGE_BLOCKS:
        {
            system_event_wait_single(program_ptr->metadata.metadata_ready_event);

            system_hash64map_get_property(program_ptr->metadata.blocks_ssb_by_name_hashmap,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                          out_result_ptr);

            break;
        }

        case RAL_PROGRAM_PROPERTY_N_UNIFORM_BLOCKS:
        {
            system_event_wait_single(program_ptr->metadata.metadata_ready_event);

            system_hash64map_get_property(program_ptr->metadata.blocks_ub_by_name_hashmap,
                                          SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                          out_result_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized ral_program_property value.")
        }
    }

end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ral_program_is_block_defined(ral_program               program,
                                                     system_hashed_ansi_string block_name)
{
    _ral_program* program_ptr = (_ral_program*) program;
    bool          result      = false;

    if (program_ptr == nullptr)
    {
        ASSERT_DEBUG_SYNC(program_ptr != nullptr,
                          "Input ral_program instance is NULL");

        goto end;
    }

    if (block_name == nullptr)
    {
        ASSERT_DEBUG_SYNC(block_name != nullptr,
                          "Input block name is NULL");

        goto end;
    }

    system_event_wait_single(program_ptr->metadata.metadata_ready_event);

    result = system_hash64map_contains(program_ptr->metadata.blocks_by_name_hashmap,
                                       system_hashed_ansi_string_get_hash(block_name) );
end:
    return result;
}

/** Please see header for specification */
PUBLIC bool ral_program_is_shader_attached(ral_program program,
                                           ral_shader  shader)
{
    _ral_program* program_ptr = (_ral_program*) program;
    bool          result      = false;

    system_read_write_mutex_lock(program_ptr->lock,
                                 ACCESS_READ);
    {
        for (uint32_t n_shader_stage = 0;
                      n_shader_stage < RAL_SHADER_TYPE_COUNT;
                    ++n_shader_stage)
        {
            if (program_ptr->attached_shaders[n_shader_stage].shader == shader)
            {
                result = true;

                break;
            }
        }
    }
    system_read_write_mutex_unlock(program_ptr->lock,
                                   ACCESS_READ);
    return result;
}

/** Please see header for specification */
PUBLIC void ral_program_lock(ral_program program)
{
    system_read_write_mutex_lock( ((_ral_program*) program)->lock,
                                 ACCESS_WRITE);
}

/** Please see header for specification */
PUBLIC void ral_program_release(ral_program& program)
{
    _ral_program* program_ptr = (_ral_program*) program;

    delete program_ptr;
}

/** Please see header for specification */
PUBLIC void ral_program_unlock(ral_program program)
{
    system_read_write_mutex_unlock( ((_ral_program*) program)->lock,
                                   ACCESS_WRITE);
}