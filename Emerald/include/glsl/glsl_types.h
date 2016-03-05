#ifndef GLSL_TYPES_H
#define GLSL_TYPES_H

#include "system/system_types.h"

DECLARE_HANDLE(glsl_shader_constructor);

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
} glsl_shader_constructor_layout_qualifier;

typedef enum
{
    SHADER_ARGUMENT_QUALIFIER_IN,
    SHADER_ARGUMENT_QUALIFIER_INOUT,
    SHADER_ARGUMENT_QUALIFIER_OUT,

    /* Always last */
    SHADER_ARGUMENT_QUALIFIER_UNKNOWN
} glsl_shader_constructor_shader_argument_qualifier;

typedef enum
{
    VARIABLE_TYPE_CONST,
    VARIABLE_TYPE_INPUT_ATTRIBUTE,
    VARIABLE_TYPE_NONE, /* only used internally for function arguments */
    VARIABLE_TYPE_OUTPUT_ATTRIBUTE,
    VARIABLE_TYPE_UNIFORM,

    /* Always last */
    VARIABLE_TYPE_UNKNOWN
} glsl_shader_constructor_variable_type;

typedef unsigned int glsl_shader_constructor_function_id;
typedef unsigned int glsl_shader_constructor_structure_id;
typedef unsigned int glsl_shader_constructor_uniform_block_id;
typedef unsigned int glsl_shader_constructor_variable_id;


#endif /* GLSL_TYPES_H */