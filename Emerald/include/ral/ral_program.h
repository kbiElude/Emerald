#ifndef RAL_PROGRAM_H
#define RAL_PROGRAM_H

#include "ral/ral_types.h"

enum
{
    /* Notification fired when a shader is attached to one of the shader stages
     *
     * Arg: _ral_program_callback_shader_attach_callback_argument */
    RAL_PROGRAM_CALLBACK_ID_SHADER_ATTACHED,

    RAL_PROGRAM_CALLBACK_ID_COUNT
};

typedef struct _ral_program_callback_link_callback_argument
{
    ral_program program;

    explicit _ral_program_callback_link_callback_argument(ral_program in_program)
    {
        program = in_program;
    }
} _ral_program_callback_link_callback_argument;

typedef struct _ral_program_callback_shader_attach_callback_argument
{
    bool        all_shader_stages_have_shaders_attached;
    ral_program program;
    ral_shader  shader;

    _ral_program_callback_shader_attach_callback_argument()
    {
        all_shader_stages_have_shaders_attached = false;
        program                                 = NULL;
        shader                                  = NULL;
    }

    _ral_program_callback_shader_attach_callback_argument(ral_program in_program,
                                                          ral_shader  in_shader,
                                                          bool        in_all_shader_stages_have_shaders_attached)

    {
        all_shader_stages_have_shaders_attached = in_all_shader_stages_have_shaders_attached;
        program                                 = in_program;
        shader                                  = in_shader;
    }
} _ral_program_callback_shader_attach_callback_argument;

typedef enum
{
    /* not settable; system_callback_manager */
    RAL_PROGRAM_PROPERTY_CALLBACK_MANAGER,

    /* not settable; uint32_t */
    RAL_PROGRAM_PROPERTY_N_ATTACHED_SHADERS,

    /* not settable; system_hashed_ansi_string */
    RAL_PROGRAM_PROPERTY_NAME

} ral_program_property;


/** TODO */
PUBLIC EMERALD_API bool ral_program_attach_shader(ral_program program,
                                                  ral_shader  shader);

/** TODO
 *
 *  NOTE: This function should only be used by ral_context. Use ral_context_create_programs() to create
 *        a program instance instead.
 **/
PUBLIC ral_program ral_program_create(ral_context                    context,
                                      const ral_program_create_info* program_create_info_ptr);

/** TODO */
PUBLIC EMERALD_API bool ral_program_get_attached_shader_at_index(ral_program program,
                                                                 uint32_t    n_shader,
                                                                 ral_shader* out_shader_ptr);

/** TODO */
PUBLIC EMERALD_API void ral_program_get_property(ral_program          program,
                                                 ral_program_property property,
                                                 void*                out_result_ptr);

/** TODO */
PUBLIC void ral_program_release(ral_program& program);

#endif /* RAL_PROGRAM_H */