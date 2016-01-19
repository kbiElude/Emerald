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
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

typedef struct _ral_program
{
    ral_shader                attached_shaders[RAL_SHADER_TYPE_COUNT];
    system_callback_manager   callback_manager;
    ral_context               context;
    system_hashed_ansi_string name;

    _ral_program(ral_context                    in_context,
                 const ral_program_create_info* program_create_info_ptr)
    {
        memset(attached_shaders,
               0,
               sizeof(attached_shaders) );

        callback_manager = system_callback_manager_create( (_callback_id) RAL_PROGRAM_CALLBACK_ID_COUNT);
        context          = in_context;
        name             = program_create_info_ptr->name;
    }

    ~_ral_program()
    {
        for (uint32_t n_shader_type = 0;
                      n_shader_type < RAL_SHADER_TYPE_COUNT;
                    ++n_shader_type)
        {
            if (attached_shaders[n_shader_type] != NULL)
            {
                ral_context_delete_objects(context,
                                           RAL_CONTEXT_OBJECT_TYPE_SHADER,
                                           1, /* n_objects */
                                           attached_shaders + n_shader_type);
            }
        } /* for (all shader types) */

        if (callback_manager != NULL)
        {
            system_callback_manager_release(callback_manager);

            callback_manager = NULL;
        }
    }
} _ral_program;



/** Please see header for specification */
PUBLIC bool ral_program_attach_shader(ral_program program,
                                      ral_shader  shader,
                                      bool        relink_needed)
{
    _ral_program_callback_shader_attach_callback_argument callback_arg = _ral_program_callback_shader_attach_callback_argument(program,
                                                                                                                               shader,
                                                                                                                               relink_needed);
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

    if (program_ptr->attached_shaders[shader_type] != NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Another shader is already attached for the pipeline stage, used by the input shader.");

        goto end;
    }

    /* Attach the shader and fire a notification */
    program_ptr->attached_shaders[shader_type] = shader;

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

    if (program_create_info_ptr == NULL)
    {
        ASSERT_DEBUG_SYNC(!(program_create_info_ptr == NULL),
                          "Input program create info is NULL");

        goto end;
    }

    program_ptr = new (std::nothrow) _ral_program(context,
                                                  program_create_info_ptr);

    ASSERT_ALWAYS_SYNC(program_ptr != NULL,
                       "Out of memory");

end:
    return (ral_program) program_ptr;
}

/** Please see header for specification */
PUBLIC bool ral_program_get_attached_shader_at_index(ral_program program,
                                                     uint32_t    n_shader,
                                                     ral_shader* out_shader_ptr)
{
}

/** Please see header for specification */
PUBLIC void ral_program_get_property(ral_program          program,
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
                if (program_ptr->attached_shaders[n_shader_type] != NULL)
                {
                    ++result;
                }
            } /* for (all shader types) */

            *(uint32_t*) out_result_ptr = result;

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