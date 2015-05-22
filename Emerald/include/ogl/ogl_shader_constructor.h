/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#ifndef OGL_SHADER_CONSTRUCTOR_H
#define OGL_SHADER_CONSTRUCTOR_H

#include "ogl/ogl_types.h"
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
PUBLIC EMERALD_API bool ogl_shader_constructor_add_function(__in  __notnull ogl_shader_constructor    constructor,
                                                            __in            system_hashed_ansi_string name,
                                                            __in            _shader_variable_type     returned_value_type,
                                                            __out __notnull _function_id*             out_new_function_id_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_add_function_argument(__in __notnull ogl_shader_constructor     constructor,
                                                                     __in           _function_id               function_id,
                                                                     __in           _shader_argument_qualifier qualifier,
                                                                     __in           _shader_variable_type      argument_type,
                                                                     __in __notnull system_hashed_ansi_string  argument_name);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_structure(__in __notnull ogl_shader_constructor    constructor,
                                                                                 __in           _shader_variable_type     type,
                                                                                 __in           uint32_t                  array_size,
                                                                                 __in           _structure_id             structure,
                                                                                 __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_ub(__in      __notnull ogl_shader_constructor    constructor,
                                                                          __in                _variable_type            variable_type,
                                                                          __in                _layout_qualifier         layout_qualifiers,
                                                                          __in                _shader_variable_type     type,
                                                                          __in                uint32_t                  array_size,
                                                                          __in                _uniform_block_id         uniform_block,
                                                                          __in      __notnull system_hashed_ansi_string name,
                                                                          __out_opt           _variable_id*             out_variable_id);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure_variable_to_ub(__in __notnull ogl_shader_constructor    constructor,
                                                                            __in           _structure_id             structure,
                                                                            __in           _uniform_block_id         uniform_block,
                                                                            __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API _uniform_block_id ogl_shader_constructor_add_uniform_block(__in __notnull ogl_shader_constructor    constructor,
                                                                              __in __notnull _layout_qualifier         layout_qualifiers,
                                                                              __in __notnull system_hashed_ansi_string name);
/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure(__in  __notnull ogl_shader_constructor    constructor,
                                                             __in  __notnull system_hashed_ansi_string name,
                                                             __out __notnull _structure_id*            out_result_id_ptr);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_append_to_function_body(__in __notnull ogl_shader_constructor    constructor,
                                                                       __in           _function_id              function_id,
                                                                       __in __notnull system_hashed_ansi_string body_to_append);

/** TODO */
PUBLIC EMERALD_API ogl_shader_constructor ogl_shader_constructor_create(__in           ogl_shader_type,
                                                                        __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API unsigned int ogl_shader_constructor_get_amount_of_variables_in_ub(__in __notnull ogl_shader_constructor constructor,
                                                                                     __in           _uniform_block_id      ub_id);

/** TODO */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_constructor_get_shader_body(__in __notnull ogl_shader_constructor constructor);

/** TODO */
PUBLIC EMERALD_API bool ogl_shader_constructor_is_general_variable_in_ub(__in __notnull ogl_shader_constructor    constructor,
                                                                         __in           _uniform_block_id         uniform_block,
                                                                         __in __notnull system_hashed_ansi_string var_name);

/** TODO */
PUBLIC EMERALD_API void ogl_shader_constructor_set_function_body(__in __notnull ogl_shader_constructor    constructor,
                                                                 __in           _function_id              function_id,
                                                                 __in __notnull system_hashed_ansi_string body);

/** TODO.
 *
 *  @param uniform_block Must be 0 (that is: refer to the default UB).
 */
PUBLIC EMERALD_API bool ogl_shader_constructor_set_general_variable_default_value(__in      __notnull ogl_shader_constructor constructor,
                                                                                  __in                _uniform_block_id      uniform_block,
                                                                                  __in                _variable_id           variable_id,
                                                                                  __in      __notnull const void*            data,
                                                                                  __out_opt           uint32_t*              out_n_bytes_to_read);

#endif /* OGL_SHADER_CONSTRUCTOR_H */
