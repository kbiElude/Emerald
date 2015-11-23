/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#include "shared.h"
#include "ogl/ogl_shader_constructor.h"
#include "ral/ral_types.h"
#include "system/system_assertions.h"
#include "system/system_hashed_ansi_string.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"
#include <sstream>

/* Forward definitions */
struct _ogl_shader_constructor_function;
struct _ogl_shader_constructor_function_argument;
struct _ogl_shader_constructor_structure;
struct _ogl_shader_constructor_uniform_block;
struct _ogl_shader_constructor_variable;

/** Internal type definitions */
typedef struct _ogl_shader_constructor_function
{
    /* Stores _ogl_shader_constructor_function_argument* items */
    system_resizable_vector   arguments;
    system_hashed_ansi_string body;
    _shader_variable_type     returned_value_type;
    system_hashed_ansi_string name;

    _ogl_shader_constructor_function()
    {
        arguments           = system_resizable_vector_create(4 /* capacity */);
        body                = NULL;
        returned_value_type = TYPE_UNKNOWN;
        name                = NULL;
    }

    ~_ogl_shader_constructor_function()
    {
        if (arguments != NULL)
        {
            system_resizable_vector_release(arguments);

            arguments = NULL;
        }
    }
} _ogl_shader_constructor_function;


typedef struct _ogl_shader_constructor_variable
{
    system_hashed_ansi_string name;

    /* If this is not NULL, this variable is actually a structure */
    _ogl_shader_constructor_structure* structure_ptr;

    /* Otherwise, this is a normal variable */
    uint32_t              array_size;
    char*                 default_data;
    _layout_qualifier     layout_qualifiers;
    _shader_variable_type type;
    _variable_type        variable_type;

    _ogl_shader_constructor_variable()
    {
        array_size        = 0;
        default_data      = NULL;
        layout_qualifiers = LAYOUT_QUALIFIER_NONE;
        name              = NULL;
        structure_ptr     = NULL;
        type              = TYPE_UNKNOWN;
        variable_type     = VARIABLE_TYPE_UNKNOWN;
    }

    ~_ogl_shader_constructor_variable()
    {
        if (default_data != NULL)
        {
            delete [] default_data;

            default_data = NULL;
        }
    }
} _ogl_shader_constructor_variable;


typedef struct _ogl_shader_constructor_function_argument
{
    _ogl_shader_constructor_variable* properties;
    _shader_argument_qualifier        qualifier;

    _ogl_shader_constructor_function_argument()
    {
        properties = new (std::nothrow) _ogl_shader_constructor_variable;
        qualifier  = SHADER_ARGUMENT_QUALIFIER_UNKNOWN;
    }

    ~_ogl_shader_constructor_function_argument()
    {
        if (properties != NULL)
        {
            delete properties;

            properties = NULL;
        }
    }
} _ogl_shader_constructor_function_argument;


typedef struct _ogl_shader_constructor_structure
{
    /* Stores _ogl_shader_constructor_variable* items */
    system_resizable_vector   members;
    system_hashed_ansi_string name;

    _ogl_shader_constructor_structure()
    {
        members = system_resizable_vector_create(4 /* capacity */);
        name    = NULL;
    }

    ~_ogl_shader_constructor_structure()
    {
        if (members != NULL)
        {
            system_resizable_vector_release(members);

            members = NULL;
        }
    }
} _ogl_shader_constructor_structure;


typedef struct _ogl_shader_constructor_uniform_block
{
    bool                      is_default_ub;
    /* Stores _ogl_shader_constructor_variable* items */
    system_hashed_ansi_string name;
    system_resizable_vector   variables;

    _ogl_shader_constructor_uniform_block()
    {
        is_default_ub = false;
        name          = NULL;
        variables     = system_resizable_vector_create(4 /* capacity */);
    }

    ~_ogl_shader_constructor_uniform_block()
    {
        if (variables != NULL)
        {
            system_resizable_vector_release(variables);

            variables = NULL;
        }
    }
} _ogl_shader_constructor_uniform_block;


typedef struct _ogl_shader_constructor
{
    system_hashed_ansi_string body;
    bool                      dirty;
    system_hashed_ansi_string name;
    ral_shader_type           shader_type;

    /* Stores _ogl_shader_constructor_function* items. 0th item corresponds to main(). */
    system_resizable_vector functions;
    /* Stores _ogl_shader_constructor_structure* items */
    system_resizable_vector structures;
    /* Stores _ogl_shader_constructor_uniform_block* items. 0th item corresponds to default
     * uniform block.
     */
    system_resizable_vector uniform_blocks;


    _ogl_shader_constructor()
    {
        body           = system_hashed_ansi_string_get_default_empty_string();
        dirty          = true;
        functions      = system_resizable_vector_create(4 /* capacity */);
        name           = NULL;
        shader_type    = RAL_SHADER_TYPE_UNKNOWN;
        structures     = system_resizable_vector_create(4 /* capacity */);
        uniform_blocks = system_resizable_vector_create(4 /* capacity */);
    }

    ~_ogl_shader_constructor()
    {
        if (functions != NULL)
        {
            system_resizable_vector_release(functions);

            functions = NULL;
        }

        if (structures != NULL)
        {
            system_resizable_vector_release(structures);

            structures = NULL;
        }

        if (uniform_blocks != NULL)
        {
            system_resizable_vector_release(uniform_blocks);

            uniform_blocks = NULL;
        }
    }
    REFCOUNT_INSERT_VARIABLES
} _ogl_shader_constructor;


/** Reference counter impl */
REFCOUNT_INSERT_IMPLEMENTATION(ogl_shader_constructor,
                               ogl_shader_constructor,
                              _ogl_shader_constructor);

/** Internal variables */

/* Forward declarations */
PRIVATE void        _ogl_shader_constructor_bake_body                           (_ogl_shader_constructor*               constructor_ptr);
PRIVATE std::string _ogl_shader_constructor_get_argument_list_string            (system_resizable_vector                arguments);
PRIVATE std::string _ogl_shader_constructor_get_function_declaration_string     (_ogl_shader_constructor_function*      function_ptr);
PRIVATE std::string _ogl_shader_constructor_get_layout_qualifier_string         (_layout_qualifier                      qualifier);
PRIVATE std::string _ogl_shader_constructor_get_shader_argument_qualifier_string(_shader_argument_qualifier             qualifier);
PRIVATE std::string _ogl_shader_constructor_get_structure_declaration_string    (_ogl_shader_constructor_structure*     structure_ptr);
PRIVATE const char* _ogl_shader_constructor_get_type_string                     (_shader_variable_type                  type);
PRIVATE std::string _ogl_shader_constructor_get_uniform_block_declaration_string(_ogl_shader_constructor_uniform_block* ub_ptr);
PRIVATE std::string _ogl_shader_constructor_get_variable_declaration_string     (_ogl_shader_constructor_variable*      variable_ptr);
PRIVATE const char* _ogl_shader_constructor_get_variable_type_string            (_variable_type                         variable_type);
PRIVATE void        _ogl_shader_constructor_release                             (void*                                  constructor);

/** TODO */
PRIVATE void _ogl_shader_constructor_bake_body(_ogl_shader_constructor* constructor_ptr)
{
    std::stringstream body_sstream;

    /* Sanity checks */
    ASSERT_DEBUG_SYNC(constructor_ptr->dirty,
                      "Baking body for a non-dirty constructor");

    /* Add version information */
    body_sstream << "#version 430 core\n\n";

    /* Add structure declarations */
    unsigned int n_structures = 0;

    system_resizable_vector_get_property(constructor_ptr->structures,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_structures);

    for (unsigned int n_structure = 0;
                      n_structure < n_structures;
                    ++n_structure)
    {
        _ogl_shader_constructor_structure* structure_ptr = NULL;

        if (system_resizable_vector_get_element_at(constructor_ptr->structures,
                                                   n_structure,
                                                  &structure_ptr) )
        {
            body_sstream << _ogl_shader_constructor_get_structure_declaration_string(structure_ptr)
                         << "\n";
        }
        else
        {
            LOG_ERROR("Could not retrieve structure descriptor at index [%u]",
                      n_structure);
        }
    } /* for (all structures) */

    /* Add uniform block declarations */
    unsigned int n_uniform_blocks = 0;

    system_resizable_vector_get_property(constructor_ptr->uniform_blocks,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_uniform_blocks);

    for (unsigned int n_ub = 0;
                      n_ub < n_uniform_blocks;
                    ++n_ub)
    {
        _ogl_shader_constructor_uniform_block* ub_ptr = NULL;

        if (system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                   n_ub,
                                                  &ub_ptr) )
        {
            body_sstream << _ogl_shader_constructor_get_uniform_block_declaration_string(ub_ptr)
                         << "\n";
        }
        else
        {
            LOG_ERROR("Could not retrieve uniform block descriptor at index [%u]",
                      n_ub);
        }
    } /* for (all uniform blocks) */

    /* Add function bodies */
    unsigned int n_functions = 0;

    system_resizable_vector_get_property(constructor_ptr->functions,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_functions);

    for (unsigned int n_function = 0;
                      n_function < n_functions;
                    ++n_function)
    {
        /* main() is assigned an id of 0. We want it declared as the very last function. */

        unsigned int                      current_function_id = (n_function + 1) % n_functions;
        _ogl_shader_constructor_function* function_ptr        = NULL;

        if (system_resizable_vector_get_element_at(constructor_ptr->functions,
                                                   current_function_id,
                                                  &function_ptr) )
        {
            body_sstream << _ogl_shader_constructor_get_function_declaration_string(function_ptr)
                         << "\n";
        }
        else
        {
            LOG_ERROR("Could not retrieve function descriptor at index [%u]",
                     n_function);
        }
    } /* for (all functions) */

    /* We're done. Store the body and lower the dirty flag */
    constructor_ptr->body  = system_hashed_ansi_string_create(body_sstream.str().c_str() );
    constructor_ptr->dirty = false;
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_argument_list_string(system_resizable_vector arguments)
{
    unsigned int      n_arguments = 0;
    std::stringstream result;

    system_resizable_vector_get_property(arguments,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_arguments);

    for (unsigned int n_argument = 0;
                      n_argument < n_arguments;
                    ++n_argument)
    {
        _ogl_shader_constructor_function_argument* argument_ptr = NULL;

        if (system_resizable_vector_get_element_at(arguments,
                                                   n_argument,
                                                  &argument_ptr) )
        {
            /* in/inout/out */
            std::string qualifier_string = _ogl_shader_constructor_get_shader_argument_qualifier_string(argument_ptr->qualifier);

            result << qualifier_string << " "
                   << _ogl_shader_constructor_get_variable_declaration_string(argument_ptr->properties);

            /* separators */
            if (n_argument != (n_arguments - 1) )
            {
                result << ", ";
            }
        } /* if (system_resizable_vector_get_element_at(arguments, n_argument, &argument_ptr) ) */
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve function argument at index [%d]",
                              n_argument);
        }
    } /* for (all arguments) */

    return result.str();
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_function_declaration_string(_ogl_shader_constructor_function* function_ptr)
{
    std::stringstream result_sstream;

    /* Function declaration */
    const char* returned_value_type_string = _ogl_shader_constructor_get_type_string(function_ptr->returned_value_type);

    result_sstream << returned_value_type_string                               << " "
                   << system_hashed_ansi_string_get_buffer(function_ptr->name) << " "
                   << "("
                   << _ogl_shader_constructor_get_argument_list_string(function_ptr->arguments)
                   << ")\n"
                      "{\n"
                   << system_hashed_ansi_string_get_buffer(function_ptr->body)
                   << "\n"
                      "}\n";

    return result_sstream.str();

}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_layout_qualifier_string(_layout_qualifier qualifier)
{
    bool              first_qualifier = true;
    std::stringstream result_sstream;

    if (qualifier != LAYOUT_QUALIFIER_NONE)
    {
        result_sstream << "layout (";

        if ((qualifier & LAYOUT_QUALIFIER_COLUMN_MAJOR) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "column_major";
        }

        if ((qualifier & LAYOUT_QUALIFIER_PACKED) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "packed";
        }

        if ((qualifier & LAYOUT_QUALIFIER_ROW_MAJOR) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "row_major";
        }

        if ((qualifier & LAYOUT_QUALIFIER_SHARED) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "shared";
        }

        if ((qualifier & LAYOUT_QUALIFIER_STD140) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "std140";
        }

        if ((qualifier & LAYOUT_QUALIFIER_STD430) != 0)
        {
            if (!first_qualifier)
            {
                result_sstream << ", ";
            }

            result_sstream << "std430";
        }

        result_sstream << ") ";
    } /* if (qualifier != LAYOUT_QUALIFIER_NONE) */

    return result_sstream.str();
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_shader_argument_qualifier_string(_shader_argument_qualifier qualifier)
{
    const char* result = "[?]";

    switch (qualifier)
    {
        case SHADER_ARGUMENT_QUALIFIER_IN:    result = "in";   break;
        case SHADER_ARGUMENT_QUALIFIER_INOUT: result = "inout"; break;
        case SHADER_ARGUMENT_QUALIFIER_OUT:   result = "out";   break;

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized shader argument qualifier [%d]",
                              qualifier);
        }
    } /* switch (qualifier) */

    return result;
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_structure_declaration_string(_ogl_shader_constructor_structure* structure_ptr)
{
    std::stringstream result_sstream;

    /* Structure declaration */
    result_sstream << "struct " << system_hashed_ansi_string_get_buffer(structure_ptr->name) << "\n"
                      "{\n";

    /* Members */
    unsigned int n_members = 0;

    system_resizable_vector_get_property(structure_ptr->members,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_members);

    for (unsigned int n_member = 0;
                      n_member < n_members;
                    ++n_member)
    {
        _ogl_shader_constructor_variable* variable_ptr = NULL;

        if (system_resizable_vector_get_element_at(structure_ptr->members,
                                                   n_member,
                                                  &variable_ptr) )
        {
            result_sstream << _ogl_shader_constructor_get_variable_declaration_string(variable_ptr)
                           << ";\n";
        }
        else
        {
            LOG_ERROR("Could not retrieve structure member descriptor at index [%u]",
                      n_member);
        }
    } /* for (all members) */

    /* Structure closure */
    result_sstream << "};\n";

    return result_sstream.str();
}

/** TODO */
PRIVATE const char* _ogl_shader_constructor_get_type_string(_shader_variable_type type)
{
    const char* result ="[?]";

    switch (type)
    {
        case TYPE_BOOL:                   result = "bool";                   break;
        case TYPE_BVEC2:                  result = "bvec2";                  break;
        case TYPE_BVEC3:                  result = "bvec3";                  break;
        case TYPE_BVEC4:                  result = "bvec4";                  break;
        case TYPE_DMAT2:                  result = "dmat2";                  break;
        case TYPE_DMAT2X3:                result = "dmat2x3";                break;
        case TYPE_DMAT2X4:                result = "dmat2x4";                break;
        case TYPE_DMAT3:                  result = "dmat3";                  break;
        case TYPE_DMAT3X2:                result = "dmat3x2";                break;
        case TYPE_DMAT3X4:                result = "dmat3x4";                break;
        case TYPE_DMAT4:                  result = "dmat4";                  break;
        case TYPE_DMAT4X2:                result = "dmat4x2";                break;
        case TYPE_DMAT4X3:                result = "dmat4x3";                break;
        case TYPE_DVEC2:                  result = "dvec2";                  break;
        case TYPE_DVEC3:                  result = "dvec3";                  break;
        case TYPE_DVEC4:                  result = "dvec4";                  break;
        case TYPE_FLOAT:                  result = "float";                  break;
        case TYPE_INT:                    result = "int";                    break;
        case TYPE_ISAMPLER1D:             result = "isampler1D";             break;
        case TYPE_ISAMPLER1DARRAY:        result = "isampler1DArray";        break;
        case TYPE_ISAMPLER2D:             result = "isampler2D";             break;
        case TYPE_ISAMPLER2DARRAY:        result = "isampler2DArray";        break;
        case TYPE_ISAMPLER2DMS:           result = "isampler2DMS";           break;
        case TYPE_ISAMPLER2DMSARRAY:      result = "isampler2DMSArray";      break;
        case TYPE_ISAMPLER2DRECT:         result = "isampler2DRect";         break;
        case TYPE_ISAMPLER3D:             result = "isampler3D";             break;
        case TYPE_ISAMPLERBUFFER:         result = "isamplerBuffer";         break;
        case TYPE_ISAMPLERCUBE:           result = "isamplerCube";           break;
        case TYPE_ISAMPLERCUBEARRAY:      result = "isamplerCubeArray";      break;
        case TYPE_IVEC2:                  result = "ivec2";                  break;
        case TYPE_IVEC3:                  result = "ivec3";                  break;
        case TYPE_IVEC4:                  result = "ivec4";                  break;
        case TYPE_MAT2:                   result = "mat2";                   break;
        case TYPE_MAT2X3:                 result = "mat2x3";                 break;
        case TYPE_MAT2X4:                 result = "mat2x4";                 break;
        case TYPE_MAT3:                   result = "mat3";                   break;
        case TYPE_MAT3X2:                 result = "mat3x2";                 break;
        case TYPE_MAT3X4:                 result = "mat3x4";                 break;
        case TYPE_MAT4:                   result = "mat4";                   break;
        case TYPE_MAT4X2:                 result = "mat4x2";                 break;
        case TYPE_MAT4X3:                 result = "mat4x3";                 break;
        case TYPE_SAMPLER1D:              result = "sampler1D";              break;
        case TYPE_SAMPLER1DARRAY:         result = "sampler1DArray";         break;
        case TYPE_SAMPLER1DARRAYSHADOW:   result = "sampler1DArrayShadow";   break;
        case TYPE_SAMPLER1DSHADOW:        result = "sampler1DShadow";        break;
        case TYPE_SAMPLER2D:              result = "sampler2D";              break;
        case TYPE_SAMPLER2DARRAY:         result = "sampler2DArray";         break;
        case TYPE_SAMPLER2DARRAYSHADOW:   result = "sampler2DArrayShadow";   break;
        case TYPE_SAMPLER2DMS:            result = "sampler2DMS";            break;
        case TYPE_SAMPLER2DMSARRAY:       result = "sampler2DMSArray";       break;
        case TYPE_SAMPLER2DRECT:          result = "sampler2DRect";          break;
        case TYPE_SAMPLER2DRECTSHADOW:    result = "sampler2DRectShadow";    break;
        case TYPE_SAMPLER2DSHADOW:        result = "sampler2DShadow";        break;
        case TYPE_SAMPLER3D:              result = "sampler3D";              break;
        case TYPE_SAMPLERBUFFER:          result = "samplerBuffer";          break;
        case TYPE_SAMPLERCUBE:            result = "samplerCube";            break;
        case TYPE_SAMPLERCUBEARRAY:       result = "samplerCubeArray";       break;
        case TYPE_SAMPLERCUBEARRAYSHADOW: result = "samplerCubeArrayShadow"; break;
        case TYPE_SAMPLERCUBESHADOW:      result = "samplerCubeShadow";      break;
        case TYPE_UINT:                   result = "uint";                   break;
        case TYPE_USAMPLER1D:             result = "usampler1D";             break;
        case TYPE_USAMPLER1DARRAY:        result = "usampler1DArray";        break;
        case TYPE_USAMPLER2D:             result = "usampler2D";             break;
        case TYPE_USAMPLER2DARRAY:        result = "usampler2DArray";        break;
        case TYPE_USAMPLER2DMS:           result = "usampler2DMS";           break;
        case TYPE_USAMPLER2DMSARRAY:      result = "usampler2DMSArray";      break;
        case TYPE_USAMPLER2DRECT:         result = "usampler2DRect";         break;
        case TYPE_USAMPLER3D:             result = "usampler3D";             break;
        case TYPE_USAMPLERBUFFER:         result = "usamplerBuffer";         break;
        case TYPE_USAMPLERCUBE:           result = "usamplerCube";           break;
        case TYPE_USAMPLERCUBEARRAY:      result = "usamplerCubeArray";      break;
        case TYPE_UVEC2:                  result = "uvec2";                  break;
        case TYPE_UVEC3:                  result = "uvec3";                  break;
        case TYPE_UVEC4:                  result = "uvec4";                  break;
        case TYPE_VEC2:                   result = "vec2";                   break;
        case TYPE_VEC3:                   result = "vec3";                   break;
        case TYPE_VEC4:                   result = "vec4";                   break;
        case TYPE_VOID:                   result = "void";                   break;

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized type");
        }
    } /* switch (type) */

    return result;
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_uniform_block_declaration_string(_ogl_shader_constructor_uniform_block* ub_ptr)
{
    std::stringstream result_sstream;

    /* Uniform block declaration */
    unsigned int n_variables = 0;

    system_resizable_vector_get_property(ub_ptr->variables,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_variables);

    if (n_variables > 0)
    {
        if (!ub_ptr->is_default_ub)
        {
            result_sstream << "uniform " << system_hashed_ansi_string_get_buffer(ub_ptr->name) << "\n"
                              "{\n";
        }

        /* Proceed with variable declarations */
        for (unsigned int n_variable = 0;
                          n_variable < n_variables;
                        ++n_variable)
        {
            _ogl_shader_constructor_variable* variable_ptr = NULL;

            if (system_resizable_vector_get_element_at(ub_ptr->variables,
                                                       n_variable,
                                                      &variable_ptr) )
            {
                result_sstream << _ogl_shader_constructor_get_variable_declaration_string(variable_ptr) << ";\n";
            }
            else
            {
                LOG_ERROR("Could not release variable descriptor at index [%u]",
                          n_variable);
            }
        } /* for (all variables) */

        /* Close the UB declaration */
        if (!ub_ptr->is_default_ub)
        {
            result_sstream << "};\n";
        }
    }

    return result_sstream.str();
}

/** TODO */
PRIVATE std::string _ogl_shader_constructor_get_variable_declaration_string(_ogl_shader_constructor_variable* variable_ptr)
{
    std::stringstream result_sstream;

    /* layout qualifiers */
    if (variable_ptr->layout_qualifiers != LAYOUT_QUALIFIER_NONE)
    {
        std::string layout_qualifiers_string = _ogl_shader_constructor_get_layout_qualifier_string(variable_ptr->layout_qualifiers);

        result_sstream << layout_qualifiers_string;
    }

    /* variable type */
    const char* variable_type_string = _ogl_shader_constructor_get_variable_type_string(variable_ptr->variable_type);

    result_sstream << variable_type_string << " ";

    if (variable_ptr->structure_ptr != NULL)
    {
        /* type */
        const char* type_string = system_hashed_ansi_string_get_buffer(variable_ptr->structure_ptr->name);

        result_sstream << type_string << " ";
    }
    else
    {
        /* type */
        std::string type_string = _ogl_shader_constructor_get_type_string(variable_ptr->type);

        result_sstream << type_string << " ";
    }

    /* name of the variable */
    result_sstream << system_hashed_ansi_string_get_buffer(variable_ptr->name);

    /* arrayed? */
    if (variable_ptr->array_size > 0)
    {
        result_sstream << "[" << variable_ptr->array_size << "]";
    }

    /* default data? */
    if (variable_ptr->default_data != NULL)
    {
        /* TODO: Yeah, current impl is over-simplified. Implement when needed */
        ASSERT_DEBUG_SYNC(variable_ptr->array_size == 0         &&
                          variable_ptr->type       == TYPE_MAT4,
                          "TODO: Unsupported case");

        result_sstream << " = mat4(";

        for (uint32_t n_element = 0;
                      n_element < 16;
                    ++n_element)
        {
            result_sstream << ((float*) variable_ptr->default_data)[n_element];

            if (n_element != 15)
            {
                result_sstream << ", ";
            }
        } /* for (all mat4 elements) */

        result_sstream << ")";
    }

    return result_sstream.str();
}

/** TODO */
PRIVATE const char* _ogl_shader_constructor_get_variable_type_string(_variable_type variable_type)
{
    const char* result = "[?]";

    switch (variable_type)
    {
        case VARIABLE_TYPE_CONST:            result = "const";   break;
        case VARIABLE_TYPE_INPUT_ATTRIBUTE:  result = "in";      break;
        case VARIABLE_TYPE_NONE:             result = "";        break;
        case VARIABLE_TYPE_OUTPUT_ATTRIBUTE: result = "out";     break;
        case VARIABLE_TYPE_UNIFORM:          result = "uniform"; break;

        default:
        {
            ASSERT_DEBUG_SYNC(false, "Unrecognized variable type");
        }
    } /* switch (variable_type) */

    return result;
}

/** TODO */
PRIVATE void _ogl_shader_constructor_release(void* constructor)
{
    _ogl_shader_constructor* constructor_ptr = (_ogl_shader_constructor*) constructor;

    /* Nothing to be done */
}


/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_function(ogl_shader_constructor    constructor,
                                                            system_hashed_ansi_string name,
                                                            _shader_variable_type     returned_value_type,
                                                            _function_id*             out_new_function_id_ptr)
{
    _ogl_shader_constructor*          constructor_ptr    = (_ogl_shader_constructor*) constructor;
    _ogl_shader_constructor_function* new_descriptor_ptr = NULL;
    bool                              result             = false;
    _function_id                      result_id          = 0;

    system_resizable_vector_get_property(constructor_ptr->functions,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result_id);

    /* Make sure the function is not already added */
    for (unsigned int n_function = 0;
                      n_function < result_id /* total count */;
                    ++n_function)
    {
        _ogl_shader_constructor_function* function_ptr = NULL;

        if (system_resizable_vector_get_element_at(constructor_ptr->functions,
                                                   n_function,
                                                  &function_ptr) )
        {
            if (system_hashed_ansi_string_is_equal_to_hash_string(function_ptr->name,
                                                                  name) )
            {
                LOG_ERROR("Function [%s] is already added to the shader",
                          system_hashed_ansi_string_get_buffer(name) );

                goto end;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve %dth function descriptor",
                              n_function);
        }
    } /* for (all function descriptors) */

    /* Form the new descriptor */
    new_descriptor_ptr = new (std::nothrow) _ogl_shader_constructor_function;

    ASSERT_ALWAYS_SYNC(new_descriptor_ptr != NULL,
                       "Out of memory");

    if (new_descriptor_ptr != NULL)
    {
        new_descriptor_ptr->name                = name;
        new_descriptor_ptr->returned_value_type = returned_value_type;

        /* Store the descriptor */
        system_resizable_vector_push(constructor_ptr->functions,
                                     new_descriptor_ptr);

        /* All good */
        *out_new_function_id_ptr = result_id;
        result                   = true;
    }

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_shader_constructor_add_function_argument(ogl_shader_constructor     constructor,
                                                                     _function_id               function_id,
                                                                     _shader_argument_qualifier qualifier,
                                                                     _shader_variable_type      argument_type,
                                                                     system_hashed_ansi_string  argument_name)
{
    _ogl_shader_constructor*                   constructor_ptr    = (_ogl_shader_constructor*) constructor;
    unsigned int                               n_arguments        = 0;
    _ogl_shader_constructor_function_argument* new_descriptor_ptr = NULL;

    /* Identify the function descriptor */
    _ogl_shader_constructor_function* function_ptr = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->functions,
                                                function_id,
                                               &function_ptr) )
    {
        LOG_ERROR("Could not retrieve function descriptor for function id [%u]",
                  function_id);

        goto end;
    }

    /* Make sure the argument has not already been added */
    system_resizable_vector_get_property(function_ptr->arguments,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_arguments);

    for (unsigned int n_argument = 0;
                      n_argument < n_arguments;
                    ++n_argument)
    {
        _ogl_shader_constructor_function_argument* argument_ptr = NULL;

        if (!system_resizable_vector_get_element_at(function_ptr->arguments,
                                                    n_argument,
                                                   &argument_ptr) )
        {
            LOG_ERROR("Could not retrieve function argument at index [%u]",
                      n_argument);
        }
        else
        {
            if (system_hashed_ansi_string_is_equal_to_hash_string(argument_ptr->properties->name,
                                                                  argument_name) )
            {
                LOG_ERROR("Argument [%s] has already been added to the list of arguments for function [%s]",
                          system_hashed_ansi_string_get_buffer(argument_name),
                          system_hashed_ansi_string_get_buffer(function_ptr->name) );

                goto end;
            }
        }
    } /* for (all arguments) */

    /* Form the new descriptor */
    new_descriptor_ptr = new (std::nothrow) _ogl_shader_constructor_function_argument;

    ASSERT_ALWAYS_SYNC(new_descriptor_ptr != NULL,
                       "Out of memory");

    if (new_descriptor_ptr != NULL)
    {
        new_descriptor_ptr->qualifier                 = qualifier;
        new_descriptor_ptr->properties->array_size    = 0;
        new_descriptor_ptr->properties->name          = argument_name;
        new_descriptor_ptr->properties->type          = argument_type;
        new_descriptor_ptr->properties->variable_type = VARIABLE_TYPE_NONE;

        /* Store the descriptor */
        system_resizable_vector_push(function_ptr->arguments, new_descriptor_ptr);
    }

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_structure(ogl_shader_constructor    constructor,
                                                                                 _shader_variable_type     type,
                                                                                 uint32_t                  array_size,
                                                                                 _structure_id             structure,
                                                                                 system_hashed_ansi_string name)
{
    _ogl_shader_constructor*          constructor_ptr = (_ogl_shader_constructor*) constructor;
    bool                              result          = false;
    _ogl_shader_constructor_variable* variable_ptr    = NULL;

    /* Retrieve structure descriptor */
    _ogl_shader_constructor_structure* structure_ptr = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->structures,
                                                structure,
                                               &structure_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of structure with id [%u]",
                  structure);

        goto end;
    }

    /* Form new variable descriptor */
    variable_ptr = new (std::nothrow) _ogl_shader_constructor_variable;

    ASSERT_ALWAYS_SYNC(variable_ptr != NULL,
                       "Out of memory");

    if (variable_ptr == NULL)
    {
        goto end;
    }

    variable_ptr->array_size = array_size;
    variable_ptr->name       = name;
    variable_ptr->type       = type;

    /* Store the descriptor */
    system_resizable_vector_push(structure_ptr->members,
                                 variable_ptr);

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_general_variable_to_ub(ogl_shader_constructor    constructor,
                                                                          _variable_type            variable_type,
                                                                          _layout_qualifier         layout_qualifiers,
                                                                          _shader_variable_type     type,
                                                                          uint32_t                  array_size,
                                                                          _uniform_block_id         uniform_block,
                                                                          system_hashed_ansi_string name,
                                                                          _variable_id*             out_variable_id)
{
    _ogl_shader_constructor*               constructor_ptr      = (_ogl_shader_constructor*) constructor;
    unsigned int                           n_existing_variables = 0;
    bool                                   result               = false;
    _ogl_shader_constructor_uniform_block* ub_ptr               = NULL;
    _ogl_shader_constructor_variable*      variable_ptr         = NULL;

    /* Sanity checks */
    if (variable_type == VARIABLE_TYPE_INPUT_ATTRIBUTE &&
        uniform_block != 0)
    {
        LOG_ERROR("Cannot add an input attribute to a non-default uniform block.");

        goto end;
    }

    if (variable_type == VARIABLE_TYPE_OUTPUT_ATTRIBUTE &&
        uniform_block != 0)
    {
        LOG_ERROR("Cannot add an output attribute to a non-default uniform block.");

        goto end;
    }

    /* Retrieve UB descriptor */
    if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                uniform_block,
                                               &ub_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of UB with id [%u]",
                  uniform_block);

        goto end;
    }

    /* Make sure that the variable has not already been added. If it is already a recognized
     * var, make sure the type matches */
    system_resizable_vector_get_property(ub_ptr->variables,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_existing_variables);

    for (unsigned int n_variable = 0;
                      n_variable < n_existing_variables;
                    ++n_variable)
    {
        _ogl_shader_constructor_variable* variable_ptr = NULL;

        if (system_resizable_vector_get_element_at(ub_ptr->variables,
                                                   n_variable,
                                                  &variable_ptr) )
        {
            if (system_hashed_ansi_string_is_equal_to_hash_string(variable_ptr->name,
                                                                  name) )
            {
                /* Is this a match ? */
                if (variable_ptr->array_size    != array_size ||
                    variable_ptr->type          != type       ||
                    variable_ptr->variable_type != variable_type)
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Variable collision detected!");
                }
                else
                {
                    result = true;
                }

                /* Do not add the duplicate variable */
                goto end;
            }
        }
        else
        {
            ASSERT_DEBUG_SYNC(false, "Could not retrieve variable descriptor");
        }
    } /* for (all known variables) */

    /* Form new variable descriptor */
    variable_ptr = new (std::nothrow) _ogl_shader_constructor_variable;

    ASSERT_ALWAYS_SYNC(variable_ptr != NULL,
                       "Out of memory");

    if (variable_ptr == NULL)
    {
        goto end;
    }

    variable_ptr->array_size        = array_size;
    variable_ptr->layout_qualifiers = layout_qualifiers;
    variable_ptr->name              = name;
    variable_ptr->type              = type;
    variable_ptr->variable_type     = variable_type;

    /* Store the descriptor */
    if (out_variable_id != NULL)
    {
        system_resizable_vector_get_property(ub_ptr->variables,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                             out_variable_id);
    }

    system_resizable_vector_push(ub_ptr->variables,
                                 variable_ptr);

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure(ogl_shader_constructor    constructor,
                                                             system_hashed_ansi_string name,
                                                             _structure_id*            out_result_id_ptr)
{
    _ogl_shader_constructor*           constructor_ptr   = (_ogl_shader_constructor*) constructor;
    _ogl_shader_constructor_structure* new_structure_ptr = NULL;
    bool                               result            = false;
    _structure_id                      result_id         = 0;

    /* Make sure the structure has not already been added */
    unsigned int n_structures = 0;

    system_resizable_vector_get_property(constructor_ptr->structures,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_structures);

    for (unsigned int n_structure = 0;
                      n_structure < n_structures;
                    ++n_structure)
    {
        _ogl_shader_constructor_structure* structure_ptr = NULL;

        if (!system_resizable_vector_get_element_at(constructor_ptr->structures,
                                                    n_structure,
                                                   &structure_ptr) )
        {
            LOG_ERROR("Could not retrieve structure descriptor at index [%u]",
                      n_structure);
        }
        else
        {
            if (system_hashed_ansi_string_is_equal_to_hash_string(structure_ptr->name,
                                                                  name) )
            {
                LOG_ERROR("Structure [%s] is already added",
                          system_hashed_ansi_string_get_buffer(name) );

                goto end;
            }
        }
    } /* for (all added structures) */

    /* Form a new descriptor */
     new_structure_ptr = new (std::nothrow) _ogl_shader_constructor_structure;

    ASSERT_ALWAYS_SYNC(new_structure_ptr != NULL,
                       "Out of memory");

    if (new_structure_ptr == NULL)
    {
        goto end;
    }

    new_structure_ptr->name = name;

    /* Store it */
    result_id = n_structures;

    system_resizable_vector_push(constructor_ptr->structures,
                                 new_structure_ptr);

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* All done! */
    result = true;

    if (out_result_id_ptr != NULL)
    {
        *out_result_id_ptr = result_id;
    }

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_add_structure_variable_to_ub(ogl_shader_constructor    constructor,
                                                                            _structure_id             structure,
                                                                            _uniform_block_id         uniform_block,
                                                                            system_hashed_ansi_string name)
{
    _ogl_shader_constructor*               constructor_ptr = (_ogl_shader_constructor*) constructor;
    bool                                   result          = false;
    _ogl_shader_constructor_uniform_block* ub_ptr          = NULL;
    _ogl_shader_constructor_variable*      variable_ptr    = NULL;

    /* Retrieve structure descriptor */
    _ogl_shader_constructor_structure* structure_ptr = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->structures,
                                                structure,
                                               &structure_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of structure with id [%u]",
                  structure);

        goto end;
    }

    /* Retrieve UB descriptor */
    if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                uniform_block,
                                               &ub_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of UB with id [%u]",
                  uniform_block);

        goto end;
    }

    /* Form new variable descriptor */
    variable_ptr = new (std::nothrow) _ogl_shader_constructor_variable;

    ASSERT_ALWAYS_SYNC(variable_ptr != NULL,
                       "Out of memory");

    if (variable_ptr == NULL)
    {
        goto end;
    }

    variable_ptr->name          = name;
    variable_ptr->structure_ptr = structure_ptr;

    /* Store the descriptor */
    system_resizable_vector_push(ub_ptr->variables,
                                 variable_ptr);

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* All done */
    result = true;

end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_shader_constructor_append_to_function_body(ogl_shader_constructor    constructor,
                                                                       _function_id              function_id,
                                                                       system_hashed_ansi_string body_to_append)
{
    std::stringstream                 body_sstream;
    _ogl_shader_constructor*          constructor_ptr = (_ogl_shader_constructor*) constructor;
    _ogl_shader_constructor_function* function_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->functions,
                                                function_id,
                                               &function_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of function with id [%u]",
                  function_id);

        goto end;
    }

    /* Form the new body */
    body_sstream << system_hashed_ansi_string_get_buffer(function_ptr->body)
                 << system_hashed_ansi_string_get_buffer(body_to_append);

    /* Update function descriptor */
    function_ptr->body     = system_hashed_ansi_string_create(body_sstream.str().c_str() );
    constructor_ptr->dirty = true;

    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API _uniform_block_id ogl_shader_constructor_add_uniform_block(ogl_shader_constructor    constructor,
                                                                              _layout_qualifier         layout_qualifiers,
                                                                              system_hashed_ansi_string name)
{
    _ogl_shader_constructor*               constructor_ptr = (_ogl_shader_constructor*) constructor;
    bool                                   is_default_ub   = false;
    _uniform_block_id                      result_id       = 0;
    _ogl_shader_constructor_uniform_block* ub_ptr          = NULL;

    /* Make sure the uniform block has not already been added */
    unsigned int n_uniform_blocks = 0;

    system_resizable_vector_get_property(constructor_ptr->uniform_blocks,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_uniform_blocks);

    for (unsigned int n_uniform_block = 0;
                      n_uniform_block < n_uniform_blocks;
                    ++n_uniform_block)
    {
        _ogl_shader_constructor_uniform_block* ub_ptr = NULL;

        if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                    n_uniform_block,
                                                   &ub_ptr) )
        {
            LOG_ERROR("Could not retrieve uniform block descriptor at index [%u]",
                      n_uniform_block);
        }
        else
        {
            if (system_hashed_ansi_string_is_equal_to_hash_string(ub_ptr->name,
                                                                  name) )
            {
                LOG_ERROR("Uniform block [%s] is already added",
                          system_hashed_ansi_string_get_buffer(name) );

                goto end;
            }
        }
    } /* for (all added uniform blocks) */

    /* Do not allow unnamed uniform blocks *unless* this is the first call for this constructor,
     * in which case the call is made to initialize default uniform block.
     **/
    if ( (system_hashed_ansi_string_get_length(name) == 0 ||
         name                                        == NULL) )
    {
        uint32_t n_ubs = 0;

        system_resizable_vector_get_property(constructor_ptr->uniform_blocks,
                                             SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                            &n_ubs);

        if (n_ubs > 0)
        {
            LOG_ERROR("Unnamed uniform blocks are not allowed.");

            goto end;
        }
        else
        {
            is_default_ub = true;
        }
    }

    /* Form a new descriptor */
    ub_ptr = new (std::nothrow) _ogl_shader_constructor_uniform_block;

    if (ub_ptr == NULL)
    {
        ASSERT_ALWAYS_SYNC(false,
                           "Out of memory");

        goto end;
    }

    system_resizable_vector_get_property(constructor_ptr->uniform_blocks,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result_id);

    ub_ptr->is_default_ub = is_default_ub;
    ub_ptr->name          = name;

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* Store the descriptor */
    system_resizable_vector_push(constructor_ptr->uniform_blocks,
                                 ub_ptr);

    /* All done! */
end:
    return result_id;
}

/** Please see header for specification */
PUBLIC EMERALD_API ogl_shader_constructor ogl_shader_constructor_create(ral_shader_type           shader_type,
                                                                        system_hashed_ansi_string name)
{
    _ogl_shader_constructor* result = new (std::nothrow) _ogl_shader_constructor;

    if (result != NULL)
    {
        /* Store the properties */
        result->shader_type = shader_type;

        /* Create the default uniform block's descriptor */
        ogl_shader_constructor_add_uniform_block( (ogl_shader_constructor) result,
                                                  (_layout_qualifier)      0, /* no layout qualifiers */
                                                  system_hashed_ansi_string_get_default_empty_string() );

        /* Create main() function descriptor */
        _function_id main_function_id = (_function_id) 0xFFFFFFFF;

        ogl_shader_constructor_add_function( (ogl_shader_constructor) result,
                                             system_hashed_ansi_string_create("main"),
                                             TYPE_VOID,
                                             &main_function_id);

        ASSERT_DEBUG_SYNC(main_function_id == 0,
                          "main() function ID is not 0!");

        /* Initialize reference counting */
        REFCOUNT_INSERT_INIT_CODE_WITH_RELEASE_HANDLER(result,
                                                       _ogl_shader_constructor_release,
                                                       OBJECT_TYPE_OGL_SHADER_CONSTRUCTOR,
                                                       system_hashed_ansi_string_create_by_merging_two_strings("\\OpenGL Shader Constructors\\",
                                                                                                               system_hashed_ansi_string_get_buffer(name)) );
    }

    return (ogl_shader_constructor) result;
}

/** Please see header for specification */
PUBLIC EMERALD_API unsigned int ogl_shader_constructor_get_amount_of_variables_in_ub(ogl_shader_constructor constructor,
                                                                                     _uniform_block_id      ub_id)
{
    _ogl_shader_constructor* constructor_ptr = (_ogl_shader_constructor*) constructor;
    unsigned int             result          = 0;

    /* Identify the uniform block descriptor */
    _ogl_shader_constructor_uniform_block* ub_ptr = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                ub_id,
                                               &ub_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of uniform block with id [%u]",
                  ub_id);

        goto end;
    }

    system_resizable_vector_get_property(ub_ptr->variables,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &result);

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API system_hashed_ansi_string ogl_shader_constructor_get_shader_body(ogl_shader_constructor constructor)
{
    _ogl_shader_constructor* constructor_ptr = (_ogl_shader_constructor*) constructor;

    if (constructor_ptr->dirty)
    {
        _ogl_shader_constructor_bake_body(constructor_ptr);
    }

    return constructor_ptr->body;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_is_general_variable_in_ub(ogl_shader_constructor    constructor,
                                                                         _uniform_block_id         uniform_block,
                                                                         system_hashed_ansi_string var_name)
{
    _ogl_shader_constructor*               constructor_ptr = (_ogl_shader_constructor*) constructor;
    uint32_t                               n_variables     = 0;
    bool                                   result          = false;
    _ogl_shader_constructor_uniform_block* ub_ptr          = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                uniform_block,
                                               &ub_ptr) )
    {
        ASSERT_DEBUG_SYNC(false,
                          "Invalid Uniform Block ID");

        goto end;
    }

    /* TODO: Optimize */
    system_resizable_vector_get_property(ub_ptr->variables,
                                         SYSTEM_RESIZABLE_VECTOR_PROPERTY_N_ELEMENTS,
                                        &n_variables);

    LOG_ERROR("ogl_shader_constructor_is_general_variable_in_ub(): Slow code path!");

    for (uint32_t n_variable = 0;
                  n_variable < n_variables;
                ++n_variable)
    {
        _ogl_shader_constructor_variable* variable_ptr = NULL;

        if (!system_resizable_vector_get_element_at(ub_ptr->variables,
                                                    n_variable,
                                                   &variable_ptr) )
        {
            ASSERT_DEBUG_SYNC(false,
                              "Could not retrieve UB variable descriptor at index [%d]",
                              n_variable);

            goto end;
        }

        if (system_hashed_ansi_string_is_equal_to_hash_string(var_name,
                                                              variable_ptr->name) )
        {
            /* Found it */
            result = true;

            goto end;
        }
    } /* for (all UB variables) */

    /* All done */
end:
    return result;
}

/** Please see header for specification */
PUBLIC EMERALD_API void ogl_shader_constructor_set_function_body(ogl_shader_constructor    constructor,
                                                                 _function_id              function_id,
                                                                 system_hashed_ansi_string body)
{
    _ogl_shader_constructor*          constructor_ptr = (_ogl_shader_constructor*) constructor;
    _ogl_shader_constructor_function* function_ptr    = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->functions,
                                                function_id,
                                               &function_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of function with id [%u]",
                  function_id);

        goto end;
    }

    /* Set the body */
    function_ptr->body = body;

    /* Mark the constructor as dirty */
    constructor_ptr->dirty = true;

    /* All done */
end:
    ;
}

/** Please see header for specification */
PUBLIC EMERALD_API bool ogl_shader_constructor_set_general_variable_default_value(ogl_shader_constructor constructor,
                                                                                  _uniform_block_id      uniform_block,
                                                                                  _variable_id           variable_id,
                                                                                  const void*            data,
                                                                                  uint32_t*              out_n_bytes_to_read)
{
    _ogl_shader_constructor*          constructor_ptr = (_ogl_shader_constructor*) constructor;
    bool                              result          = false;
    _ogl_shader_constructor_variable* variable_ptr    = NULL;

    /* Retrieve UB descriptor */
    _ogl_shader_constructor_uniform_block* ub_ptr = NULL;

    if (!system_resizable_vector_get_element_at(constructor_ptr->uniform_blocks,
                                                uniform_block,
                                               &ub_ptr) )
    {
        LOG_ERROR("Could not retrieve descriptor of UB with id [%u]",
                  uniform_block);

        goto end;
    }

    /* Retrieve variable descriptor */
    if (!system_resizable_vector_get_element_at(ub_ptr->variables,
                                                variable_id,
                                               &variable_ptr) )
    {
        LOG_ERROR("Could not retrieve variable descriptor at index [%u]",
                  variable_id);

        goto end;
    }

    /* Sanity checks: we currently support only a single case, so make sure all other valid
     *                ones result in an assertion failure.
     *
     * TODO: Expand if needed.
     */
    if (variable_ptr->type != TYPE_MAT4)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_shader_constructor_set_general_variable_default_value() only supports mat4's atm.");

        goto end;
    }

    if (variable_ptr->array_size != 0)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_shader_constructor_set_general_variable_default_value() only supports non-arrayed variables atm.");

        goto end;
    }

    if (variable_ptr->structure_ptr != NULL)
    {
        ASSERT_DEBUG_SYNC(false,
                          "Requested variable is a structure.");

        goto end;
    }

    if (variable_ptr->variable_type != VARIABLE_TYPE_CONST)
    {
        ASSERT_DEBUG_SYNC(false,
                          "ogl_shader_constructor_set_general_variable_default_value() only supports constant variables atm.");

        goto end;
    }

    /* Carry on.. */
    if (out_n_bytes_to_read != NULL)
    {
        *out_n_bytes_to_read = sizeof(float) * 4 * 4;
    }

    if (data != NULL)
    {
        if (variable_ptr->default_data != NULL)
        {
            delete [] variable_ptr->default_data;

            variable_ptr->default_data = NULL;
        }

        variable_ptr->default_data = new (std::nothrow) char[16 * sizeof(float)];

        ASSERT_DEBUG_SYNC(variable_ptr->default_data != NULL,
                          "Out of memory");

        memcpy(variable_ptr->default_data,
               data,
               sizeof(float) * 16);
    }

    /* All done */
    result = true;
end:
    return result;
};
