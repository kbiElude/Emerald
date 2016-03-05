/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 */
#ifndef GLSL_SHADER_CONSTRUCTOR_H
#define GLSL_SHADER_CONSTRUCTOR_H

#include "glsl/glsl_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(glsl_shader_constructor,
                             glsl_shader_constructor)

/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_add_function(glsl_shader_constructor              constructor,
                                                             system_hashed_ansi_string            name,
                                                             ral_program_variable_type            returned_value_type,
                                                             glsl_shader_constructor_function_id* out_new_function_id_ptr);

/** TODO */
PUBLIC EMERALD_API void glsl_shader_constructor_add_function_argument(glsl_shader_constructor                           constructor,
                                                                      glsl_shader_constructor_function_id               id,
                                                                      glsl_shader_constructor_shader_argument_qualifier qualifier,
                                                                      ral_program_variable_type                         argument_type,
                                                                      system_hashed_ansi_string                         argument_name);

/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_add_general_variable_to_structure(glsl_shader_constructor              constructor,
                                                                                  ral_program_variable_type            type,
                                                                                  uint32_t                             array_size,
                                                                                  glsl_shader_constructor_structure_id structure,
                                                                                  system_hashed_ansi_string            name);

/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_add_general_variable_to_ub(glsl_shader_constructor                  constructor,
                                                                           glsl_shader_constructor_variable_type    variable_type,
                                                                           glsl_shader_constructor_layout_qualifier layout_qualifiers,
                                                                           ral_program_variable_type                type,
                                                                           uint32_t                                 array_size,
                                                                           glsl_shader_constructor_uniform_block_id uniform_block,
                                                                           system_hashed_ansi_string                name,
                                                                           glsl_shader_constructor_variable_id*     out_variable_id);

/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_add_structure_variable_to_ub(glsl_shader_constructor                  constructor,
                                                                             glsl_shader_constructor_structure_id     structure,
                                                                             glsl_shader_constructor_uniform_block_id uniform_block,
                                                                             system_hashed_ansi_string                name);

/** TODO */
PUBLIC EMERALD_API glsl_shader_constructor_uniform_block_id glsl_shader_constructor_add_uniform_block(glsl_shader_constructor                  constructor,
                                                                                                      glsl_shader_constructor_layout_qualifier layout_qualifiers,
                                                                                                      system_hashed_ansi_string                name);
/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_add_structure(glsl_shader_constructor               constructor,
                                                              system_hashed_ansi_string             name,
                                                              glsl_shader_constructor_structure_id* out_result_id_ptr);

/** TODO */
PUBLIC EMERALD_API void glsl_shader_constructor_append_to_function_body(glsl_shader_constructor             constructor,
                                                                        glsl_shader_constructor_function_id id,
                                                                        system_hashed_ansi_string           body_to_append);

/** TODO */
PUBLIC EMERALD_API glsl_shader_constructor glsl_shader_constructor_create(ral_shader_type           shader_type,
                                                                          system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API unsigned int glsl_shader_constructor_get_amount_of_variables_in_ub(glsl_shader_constructor                  constructor,
                                                                                      glsl_shader_constructor_uniform_block_id ub_id);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string glsl_shader_constructor_get_shader_body(glsl_shader_constructor constructor);

/** TODO */
PUBLIC EMERALD_API bool glsl_shader_constructor_is_general_variable_in_ub(glsl_shader_constructor                  constructor,
                                                                          glsl_shader_constructor_uniform_block_id uniform_block,
                                                                          system_hashed_ansi_string                var_name);

/** TODO */
PUBLIC EMERALD_API void glsl_shader_constructor_set_function_body(glsl_shader_constructor             constructor,
                                                                  glsl_shader_constructor_function_id id,
                                                                  system_hashed_ansi_string           body);

/** TODO.
 *
 *  @param uniform_block Must be 0 (that is: refer to the default UB).
 */
PUBLIC EMERALD_API bool glsl_shader_constructor_set_general_variable_default_value(glsl_shader_constructor                  constructor,
                                                                                   glsl_shader_constructor_uniform_block_id uniform_block,
                                                                                   glsl_shader_constructor_variable_id      id,
                                                                                   const void*                              data,
                                                                                   uint32_t*                                out_n_bytes_to_read);

#endif /* GLSL_SHADER_CONSTRUCTOR_H */
