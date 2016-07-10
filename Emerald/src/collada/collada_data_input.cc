/**
 *
 * Emerald (kbi/elude @2014-2016)
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

    explicit _collada_data_input_set(int                 in_offset,
                                     collada_data_source in_source)
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
    if (sets != nullptr)
    {
        _collada_data_input_set* input_set_ptr  = nullptr;
        system_hash64            input_set_hash = 0;

        while (system_hash64map_get_element_at(sets,
                                               0,
                                              &input_set_ptr,
                                               nullptr) )
        {
            delete input_set_ptr;
            input_set_ptr = nullptr;

            system_hash64map_remove(sets,
                                    input_set_hash);
        }

        system_hash64map_release(sets);
        sets = nullptr;
    }
}

/* Please see header for spec */
PUBLIC void collada_data_input_add_input_set(collada_data_input     input,
                                             unsigned int           n_set_id,
                                             collada_data_input_set input_set)
{
    _collada_data_input* input_ptr = reinterpret_cast<_collada_data_input*>(input);

    ASSERT_DEBUG_SYNC(!system_hash64map_contains(input_ptr->sets,
                                                 n_set_id),
                      "Set descriptor already stored.");

    system_hash64map_insert(input_ptr->sets,
                            n_set_id,
                            input_set,
                            nullptr,
                            nullptr);
}

/* Please see header for spec */
PUBLIC collada_data_input collada_data_input_create(_collada_data_input_type type)
{
    _collada_data_input* new_input = new (std::nothrow) _collada_data_input(type);

    ASSERT_ALWAYS_SYNC(new_input != nullptr,
                       "Out of memory");

    return reinterpret_cast<collada_data_input>(new_input);
}

/* Please see header for spec */
PUBLIC collada_data_input_set collada_data_input_set_create(int                 offset,
                                                            collada_data_source source)
{
    _collada_data_input_set* new_input_set = new (std::nothrow) _collada_data_input_set(offset,
                                                                                        source);

    ASSERT_ALWAYS_SYNC(new_input_set != nullptr,
                       "Out of memory");

    return (collada_data_input_set) new_input_set;
}

/* Please see header for properties */
PUBLIC _collada_data_input_type collada_data_input_convert_from_string(system_hashed_ansi_string input_type_string)
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
PUBLIC void collada_data_input_get_properties(const collada_data_input  input,
                                              unsigned int              n_set,
                                              unsigned int*             out_n_sets_ptr,
                                              int*                      out_offset_ptr,
                                              collada_data_source*      out_source_ptr,
                                              _collada_data_input_type* out_type_ptr)
{
    const _collada_data_input* input_ptr = reinterpret_cast<const _collada_data_input*>(input);
    _collada_data_input_set*   set_ptr   = nullptr;

    if (out_n_sets_ptr != nullptr)
    {
        system_hash64map_get_property(input_ptr->sets,
                                      SYSTEM_HASH64MAP_PROPERTY_N_ELEMENTS,
                                      out_n_sets_ptr);
    }

    if (out_type_ptr != nullptr)
    {
        *out_type_ptr = input_ptr->type;
    }

    if (system_hash64map_get(input_ptr->sets,
                             n_set,
                            &set_ptr) )
    {
        if (out_offset_ptr != nullptr)
        {
            *out_offset_ptr = set_ptr->offset;
        }

        if (out_source_ptr != nullptr)
        {
            *out_source_ptr = set_ptr->source;
        }
    }
}