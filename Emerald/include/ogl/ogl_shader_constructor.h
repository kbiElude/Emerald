/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_SHADER_CONSTRUCTOR_H
#define OGL_SHADER_CONSTRUCTOR_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "system/system_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_shader_constructor,
                             ogl_shader_constructor)

typedef enum
{
    /* TODO: Bindings or locations are not currently supported by this scheme. Well.. :) */
    LAYOUT_QUALIFIER_NONE         = 0,

    LAYOUT_QUALIFIER_COLUMN_MAJOR = 1 << 0,
    LAYOUT_QUALIFIER_PACKED       = 1 << 1,
    LAYOUT_QUALIFIER_ROW_MAJOR    = 1 << 2,
    LAYOUT_QUALIFIER_SHARED       = 1 << 3,
    LAYOUT_QUALIFIER_STD140       = 1 << 4,
    LAYOUT_QUALIFIER_STD430       = 1 << 5
} _layout_qualifier;

typedef enum
{
    SHADER_ARGUMENT_QUALIFIER_IN,
    SHADER_ARGUMENT_QUALIFIER_INOUT,
    SHADER_ARGUMENT_QUALIFIER_OUT,

    /* Always last */
    SHADER_ARGUMENT_QUALIFIER_UNKNOWN
} _shader_argument_qualifier;

typedef enum
{
    VARIABLE_TYPE_CONST,
    VARIABLE_TYPE_INPUT_ATTRIBUTE,
    VARIABLE_TYPE_NONE, /* only used internally for function arguments */
    VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
    VARIABLE_TYPE_UNIFORM,

    /* Always last */
    VARIABLE_TYPE_UNKNOWN
} _variable_type;

typedef unsigned int _function_id;
typedef unsigned int _structure_id;
typedef unsigned int _uniform_block_id;
typedef unsigned int _variable_id;

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_function(ogl_shader_constructor    constructor,
                                                            system_hashed_ansi_string name,
                                                            _shader_variable_type     returned_value_type,
                                                            _function_id*             out_new_function_id_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_add_function_argument(ogl_shader_constructor     constructor,
                                                                     _function_id               function_id,
                                                                     _shader_argument_qualifier qualifier,
                                                                     _shader_variable_type      argument_type,
                                                                     system_hashed_ansi_string  argument_name);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_structure(ogl_shader_constructor    constructor,
                                                                                 _shader_variable_type     type,
                                                                                 uint32_t                  array_size,
                                                                                 _structure_id             structure,
                                                                                 system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_ub(ogl_shader_constructor    constructor,
                                                                          _variable_type            variable_type,
                                                                          _layout_qualifier         layout_qualifiers,
                                                                          _shader_variable_type     type,
                                                                          uint32_t                  array_size,
                                                                          _uniform_block_id         uniform_block,
                                                                          system_hashed_ansi_string name,
                                                                          _variable_id*             out_variable_id);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure_variable_to_ub(ogl_shader_constructor    constructor,
                                                                            _structure_id             structure,
                                                                            _uniform_block_id         uniform_block,
                                                                            system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API _uniform_block_id ogl_shader_constructor_add_uniform_block(ogl_shader_constructor    constructor,
                                                                              _layout_qualifier         layout_qualifiers,
                                                                              system_hashed_ansi_string name);
/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure(ogl_shader_constructor    constructor,
                                                             system_hashed_ansi_string name,
                                                             _structure_id*            out_result_id_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_append_to_function_body(ogl_shader_constructor    constructor,
                                                                       _function_id              function_id,
                                                                       system_hashed_ansi_string body_to_append);

/** TODO */
PUBLIC EMERALD_API ogl_shader_constructor ogl_shader_constructor_create(ral_shader_type,
                                                                        system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API unsigned int ogl_shader_constructor_get_amount_of_variables_in_ub(ogl_shader_constructor constructor,
                                                                                     _uniform_block_id      ub_id);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_constructor_get_shader_body(ogl_shader_constructor constructor);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_is_general_variable_in_ub(ogl_shader_constructor    constructor,
                                                                         _uniform_block_id         uniform_block,
                                                                         system_hashed_ansi_string var_name);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_set_function_body(ogl_shader_constructor    constructor,
                                                                 _function_id              function_id,
                                                                 system_hashed_ansi_string body);

/** TODO.
 *
 *  @param uniform_block Must be 0 (that is: refer to the default UB).
 */
PUBLIC EMERALD_API bool ogl_shader_constructor_set_general_variable_default_value(ogl_shader_constructor constructor,
                                                                                  _uniform_block_id      uniform_block,
                                                                                  _variable_id           variable_id,
                                                                                  const void*            data,
                                                                                  uint32_t*              out_n_bytes_to_read);

#endif /* OGL_SHADER_CONSTRUCTOR_H */
