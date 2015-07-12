/**
 *
 * Emerald (kbi/elude @2014-2015)
 *
 */
#include "shared.h"
#include "collada/collada_data_name_array.h"
#include "collada/collada_data_sampler.h"
#include "collada/collada_data_source.h"
#include "system/system_assertions.h"
#include "system/system_hash64.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes a single <input> node embedded within <sampler> */
typedef struct _collada_data_sampler_input
{
    collada_data_sampler_input_semantic semantic;
    collada_data_source                 source;

    system_resizable_vector             source_interpolation_data; /* stores collada_data_sampler_input_interpolation items for INTERPOLATION input */

     _collada_data_sampler_input();
    ~_collada_data_sampler_input();
} _collada_data_sampler_input;

/** Describes a single <sampler> instance */
typedef struct _collada_data_sampler
{
    system_hashed_ansi_string id;
    system_hash64map          inputs; /* key: collada_data_sampler_input_semantic, value: _collada_data_sampler_input* */

     _collada_data_sampler();
    ~_collada_data_sampler();
} _collada_data_sampler;


/** TODO */
_collada_data_sampler_input::_collada_data_sampler_input()
{
    semantic                  = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_UNKNOWN;
    source                    = NULL;
    source_interpolation_data = NULL;
}

/** TODO */
_collada_data_sampler_input::~_collada_data_sampler_input()
{
    /* NOTE: Do not release 'source' - the descriptor does not own it */

    if (source_interpolation_data != NULL)
    {
        system_resizable_vector_release(source_interpolation_data);

        source_interpolation_data = NULL;
    }
}

/** TODO */
_collada_data_sampler::_collada_data_sampler()
{
    id     = system_hashed_ansi_string_get_default_empty_string();
    inputs = NULL;
}

/** TODO */
_collada_data_sampler::~_collada_data_sampler()
{
    if (inputs != NULL)
    {
        system_hash64                input_hash = 0;
        _collada_data_sampler_input* input_ptr  = NULL;

        while (system_hash64map_get_element_at(inputs,
                                               0, /* index */
                                              &input_ptr,
                                              &input_hash) )
        {
            delete input_ptr;
            input_ptr = NULL;

            system_hash64map_remove(inputs,
                                    input_hash);
        }
    }
}


/** TODO */
PRIVATE _collada_data_sampler_input* _collada_data_sampler_create_sampler_input(tinyxml2::XMLElement* input_element_ptr,
                                                                                system_hash64map      source_by_name_map)
{
    _collada_data_sampler_input*        result                    = NULL;
    system_hashed_ansi_string           semantic_has              = NULL;
    collada_data_sampler_input_semantic semantic                  = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_UNKNOWN;
    collada_data_source                 source                    = NULL;
    system_resizable_vector             source_interpolation_data = NULL;

    /* Retrieve input node attributes */
    const char* semantic_name = input_element_ptr->Attribute("semantic");
    const char* source_name   = input_element_ptr->Attribute("source");

    ASSERT_DEBUG_SYNC(semantic_name != NULL,
                      "No semantic attribute defined for an <input> node");
    ASSERT_DEBUG_SYNC(source_name   != NULL,
                      "No source attribute defined for an <input> node");

    if (semantic_name == NULL ||
        source_name   == NULL)
    {
        goto end;
    }

    /* Convert semantic to one of internal enums */
    semantic_has = system_hashed_ansi_string_create(semantic_name);

    if (system_hashed_ansi_string_is_equal_to_raw_string(semantic_has, "IN_TANGENT") )    semantic = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_IN_TANGENT;   else
    if (system_hashed_ansi_string_is_equal_to_raw_string(semantic_has, "INPUT") )         semantic = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INPUT;        else
    if (system_hashed_ansi_string_is_equal_to_raw_string(semantic_has, "INTERPOLATION") ) semantic = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION;else
    if (system_hashed_ansi_string_is_equal_to_raw_string(semantic_has, "OUT_TANGENT") )   semantic = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_OUT_TANGENT;  else
    if (system_hashed_ansi_string_is_equal_to_raw_string(semantic_has, "OUTPUT") )        semantic = COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_OUTPUT;       else
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unrecognized semantic type [%s]",
                          semantic_name);

        goto end;
    }

    /* Find a source instance we need to refer to */
    if (source_name[0] == '#')
    {
        source_name++;
    }

    if (!system_hash64map_get(source_by_name_map,
                              system_hash64_calculate(source_name,
                                                      strlen(source_name) ),
                             &source))
    {
        ASSERT_DEBUG_SYNC(false,
                          "Unable to find a source named [%s] for <sampler> node",
                          source_name);

        goto end;
    }

    /* Is this INTERPOLATION input? If so, we need to convert all the strings to enum representation */
    if (semantic == COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION)
    {
        collada_data_name_array name_array = NULL;
        uint32_t                n_values   = 0;

        collada_data_source_get_source_name_data(source,
                                                &name_array);
        collada_data_name_array_get_property    (name_array,
                                                 COLLADA_DATA_NAME_ARRAY_PROPERTY_N_VALUES,
                                                &n_values);

        ASSERT_DEBUG_SYNC(n_values != 0,
                          "0 values stored in a name array");

        if (n_values != 0)
        {
            source_interpolation_data = system_resizable_vector_create(n_values);

            for (uint32_t n_value = 0;
                          n_value < n_values;
                        ++n_value)
            {
                system_hashed_ansi_string                value      = collada_data_name_array_get_value_at_index(name_array,
                                                                                                                 n_value);
                collada_data_sampler_input_interpolation value_enum = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_UNKNOWN;

                if (system_hashed_ansi_string_is_equal_to_raw_string(value, "BEZIER") )  value_enum = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_BEZIER; else
                if (system_hashed_ansi_string_is_equal_to_raw_string(value, "BSPLINE") ) value_enum = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_BSPLINE;else
                if (system_hashed_ansi_string_is_equal_to_raw_string(value, "HERMITE") ) value_enum = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_HERMITE;else
                if (system_hashed_ansi_string_is_equal_to_raw_string(value, "LINEAR") )  value_enum = COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_LINEAR; else
                {
                    ASSERT_DEBUG_SYNC(false,
                                      "Unrecognized interpolation type [%s]",
                                      system_hashed_ansi_string(value) );
                }

                system_resizable_vector_push(source_interpolation_data,
                                             (void*) value_enum);
            } /* for (all values) */
        } /* if (n_values != 0) */
    } /* if (semantic == COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION) */

    /* Cool, we can form the result descriptor */
    result = new (std::nothrow) _collada_data_sampler_input;

    ASSERT_DEBUG_SYNC(result != NULL,
                      "Out of memory");

    if (result == NULL)
    {
        goto end;
    }

    result->semantic                  = semantic;
    result->source                    = source;
    result->source_interpolation_data = source_interpolation_data;

end:
    return result;
}


/** Please see header for spec */
PUBLIC collada_data_sampler collada_data_sampler_create(tinyxml2::XMLElement* element_ptr,
                                                        system_hash64map      source_by_name_map)
{
    tinyxml2::XMLElement*  input_element_ptr        = NULL;
    _collada_data_sampler* new_sampler_instance_ptr = NULL;

    /* Retrieve sampler ID */
    const char* sampler_id = element_ptr->Attribute("id");

    ASSERT_DEBUG_SYNC(sampler_id != NULL,
                      "url attribute missing");

    if (sampler_id == NULL)
    {
        goto end;
    }

    /* Allocate space for the descriptor */
    new_sampler_instance_ptr = new (std::nothrow) _collada_data_sampler;

    ASSERT_DEBUG_SYNC(new_sampler_instance_ptr != NULL,
                      "Out of memory");

    if (new_sampler_instance_ptr == NULL)
    {
        goto end;
    }

    /* Fill the descriptor */
    new_sampler_instance_ptr->id     = system_hashed_ansi_string_create(sampler_id);
    new_sampler_instance_ptr->inputs = system_hash64map_create         (sizeof(_collada_data_sampler_input*) );

    ASSERT_DEBUG_SYNC(new_sampler_instance_ptr->inputs != NULL,
                      "Could not spawn inputs hash map");

    if (new_sampler_instance_ptr->inputs == NULL)
    {
        delete new_sampler_instance_ptr;
        new_sampler_instance_ptr = NULL;

        goto end;
    }

    /* Iterate over all inputs and fill the inputs vector */
    input_element_ptr = element_ptr->FirstChildElement("input");

    while (input_element_ptr != NULL)
    {
        _collada_data_sampler_input* new_input_ptr = _collada_data_sampler_create_sampler_input(input_element_ptr,
                                                                                                source_by_name_map);

        ASSERT_DEBUG_SYNC(new_input_ptr != NULL,
                          "Could not spawn a sampler input descriptor");

        if (new_input_ptr == NULL)
        {
            goto end;
        }

        system_hash64map_insert(new_sampler_instance_ptr->inputs,
                                new_input_ptr->semantic,
                                new_input_ptr,
                                NULL,  /* on_remove_callback */
                                NULL); /* on_remove_callback_user_arg */

        /* Move to next input element */
        input_element_ptr = input_element_ptr->NextSiblingElement("input");
    }

end:
    return (collada_data_sampler) new_sampler_instance_ptr;
}

/** Please see header for spec */
PUBLIC bool collada_data_sampler_get_input_property(collada_data_sampler                sampler,
                                                    collada_data_sampler_input_semantic semantic,
                                                    collada_data_sampler_input_property property,
                                                    void*                               out_result)
{
    _collada_data_sampler_input* input_ptr   = NULL;
    bool                         result      = false;
    _collada_data_sampler*       sampler_ptr = (_collada_data_sampler*) sampler;

    if (system_hash64map_get(sampler_ptr->inputs,
                             semantic,
                            &input_ptr) )
    {
        switch (property)
        {
            case COLLADA_DATA_SAMPLER_INPUT_PROPERTY_INTERPOLATION_DATA:
            {
                ASSERT_DEBUG_SYNC(semantic == COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION,
                                  "COLLADA_DATA_SAMPLER_INPUT_PROPERTY_INTERPOLATION_DATA query executed for invalid semantic");

                *((system_resizable_vector*) out_result) = input_ptr->source_interpolation_data;
                result                                   = (input_ptr->source_interpolation_data != NULL);

                break;
            }

            case COLLADA_DATA_SAMPLER_INPUT_PROPERTY_SOURCE:
            {
                *(collada_data_source*) out_result = input_ptr->source;
                result                             = true;

                break;
            }

            default:
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Unrecognized collada_data_sampler_input_property value");
            }
        } /* switch (property) */
    }
    else
    {
        ASSERT_DEBUG_SYNC(false,
                          "<sampler> does not define requested semantic.");
    }

    return result;
}

/** Please see header for spec */
PUBLIC void collada_data_sampler_get_property(collada_data_sampler          sampler,
                                              collada_data_sampler_property property,
                                              void*                         out_result)
{
    _collada_data_sampler* sampler_ptr = (_collada_data_sampler*) sampler;

    switch (property)
    {
        case COLLADA_DATA_SAMPLER_PROPERTY_ID:
        {
            *((system_hashed_ansi_string *) out_result) = sampler_ptr->id;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_sampler_property value");
        }
    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void collada_data_sampler_release(collada_data_sampler sampler_instance)
{
    delete (_collada_data_sampler*) sampler_instance;

    sampler_instance = NULL;
}