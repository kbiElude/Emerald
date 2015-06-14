/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_input.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes <input> node contents for a given set value */
typedef struct _collada_data_input_set
{
    int                 offset;
    collada_data_source source;

    explicit _collada_data_input_set(int in_offset, collada_data_source in_source)
    {
        offset = in_offset;
        source = in_source;
    }
} _collada_data_input_set;

/** Describes <input> node contents. */
typedef struct _collada_data_input
{
    /* Maps set indices to _collada_data_input_set instances */
    system_hash64map         sets;
    _collada_data_input_type type;

    explicit _collada_data_input(_collada_data_input_type in_type);
            ~_collada_data_input();
} _collada_data_input;


/** TODO */
_collada_data_input::_collada_data_input(_collada_data_input_type in_type)
{
    sets = system_hash64map_create(sizeof(_collada_data_input_set*) );
    type = in_type;
}

/** TODO */
_collada_data_input::~_collada_data_input()
{
    if (sets != NULL)
    {
        _collada_data_input_set* input_set_ptr  = NULL;
        system_hash64            input_set_hash = 0;

        while (system_hash64map_get_element_at(sets, 0, &input_set_ptr, NULL) )
        {
            delete input_set_ptr;
            input_set_ptr = NULL;

            system_hash64map_remove(sets, input_set_hash);
        }

        system_hash64map_release(sets);
        sets = NULL;
    } /* if (sets != NULL) */
}

/* Please see header for spec */
PUBLIC void collada_data_input_add_input_set(__in __notnull collada_data_input     input,
                                             __in           unsigned int           n_set_id,
                                             __in __notnull collada_data_input_set input_set)
{
    _collada_data_input* input_ptr = (_collada_data_input*) input;

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(input_ptr->sets, n_set_id),
                      "Set descriptor already stored.");

    system_hash64map_insert(input_ptr->sets,
                            n_set_id,
                            input_set,
                            NULL,
                            NULL);
}

/* Please see header for spec */
PUBLIC collada_data_input collada_data_input_create(__in _collada_data_input_type type)
{
    _collada_data_input* new_input = new (std::nothrow) _collada_data_input(type);

    ASSERT_ALWAYS_SYNC(new_input != NULL, "Out of memory");

    return (collada_data_input) new_input;
}

/* Please see header for spec */
PUBLIC collada_data_input_set collada_data_input_set_create(__in           int                 offset,
                                                            __in __notnull collada_data_source source)
{
    _collada_data_input_set* new_input_set = new (std::nothrow) _collada_data_input_set(offset, source);

    ASSERT_ALWAYS_SYNC(new_input_set != NULL, "Out of memory");
    return (collada_data_input_set) new_input_set;
}

/* Please see header for properties */
PUBLIC _collada_data_input_type collada_data_input_convert_from_string(__in __notnull system_hashed_ansi_string input_type_string)
{
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "BINORMAL"))        return COLLADA_DATA_INPUT_TYPE_BINORMAL;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "COLOR"))           return COLLADA_DATA_INPUT_TYPE_COLOR;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "CONTINUITY"))      return COLLADA_DATA_INPUT_TYPE_CONTINUITY;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "IMAGE"))           return COLLADA_DATA_INPUT_TYPE_IMAGE;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "INPUT"))           return COLLADA_DATA_INPUT_TYPE_INPUT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "IN_TANGENT"))      return COLLADA_DATA_INPUT_TYPE_IN_TANGENT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "INTERPOLATION"))   return COLLADA_DATA_INPUT_TYPE_INTERPOLATION;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "INV_BIND_MATRIX")) return COLLADA_DATA_INPUT_TYPE_INV_BIND_MATRIX;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "JOINT"))           return COLLADA_DATA_INPUT_TYPE_JOINT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "LINEAR_STEPS"))    return COLLADA_DATA_INPUT_TYPE_LINEAR_STEPS;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "MORPH_TARGET"))    return COLLADA_DATA_INPUT_TYPE_MORPH_TARGET;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "MORPH_WEIGHT"))    return COLLADA_DATA_INPUT_TYPE_MORPH_WEIGHT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "NORMAL"))          return COLLADA_DATA_INPUT_TYPE_NORMAL;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "OUTPUT"))          return COLLADA_DATA_INPUT_TYPE_OUTPUT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "OUT_TANGENT"))     return COLLADA_DATA_INPUT_TYPE_OUT_TANGENT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "POSITION"))        return COLLADA_DATA_INPUT_TYPE_POSITION;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "TANGENT"))         return COLLADA_DATA_INPUT_TYPE_TANGENT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "TEXBINORMAL"))     return COLLADA_DATA_INPUT_TYPE_TEXBINORMAL;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "TEXCOORD"))        return COLLADA_DATA_INPUT_TYPE_TEXCOORD;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "TEXTANGENT"))      return COLLADA_DATA_INPUT_TYPE_TEXTANGENT;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "UV"))              return COLLADA_DATA_INPUT_TYPE_UV;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "VERTEX"))          return COLLADA_DATA_INPUT_TYPE_VERTEX;
    if (system_hashed_ansi_string_is_equal_to_raw_string(input_type_string, "WEIGHT"))          return COLLADA_DATA_INPUT_TYPE_WEIGHT;

    ASSERT_DEBUG_SYNC(false, "Unrecognized input type string");

    return COLLADA_DATA_INPUT_TYPE_UNDEFINED;
}

/* Please see header for properties */
PUBLIC void collada_data_input_get_properties(__in      __notnull collada_data_input        input,
                                              __in                unsigned int              n_set,
                                              __out_opt           unsigned int*             out_n_sets,
                                              __out_opt           int*                      out_offset,
                                              __out_opt           collada_data_source*      out_source,
                                              __out_opt           _collada_data_input_type* out_type)
{
    _collada_data_input*     input_ptr = (_collada_data_input*) input;
    _collada_data_input_set* set_ptr   = NULL;

    if (out_n_sets != NULL)
    {
        system_hash64map_get_property(input_ptr->sets,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                      out_n_sets);
    }

    if (out_type != NULL)
    {
        *out_type = input_ptr->type;
    }

    if (system_hash64map_get(input_ptr->sets,
                             n_set,
                            &set_ptr) )
    {
        if (out_offset != NULL)
        {
            *out_offset = set_ptr->offset;
        }

        if (out_source != NULL)
        {
            *out_source = set_ptr->source;
        }
    }
}