/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SAMPLER_H
#define COLLADA_DATA_SAMPLER_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

typedef enum collada_data_sampler_input_interpolation
{
    COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_BEZIER,
    COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_BSPLINE,
    COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_HERMITE,
    COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_LINEAR,

    COLLADA_DATA_SAMPLER_INPUT_INTERPOLATION_UNKNOWN
};

typedef enum collada_data_sampler_input_property
{
    COLLADA_DATA_SAMPLER_INPUT_PROPERTY_INTERPOLATION_DATA, /* not settable, vector of collada_data_sampler_input_interpolation as defined by <input>
                                                             * with INTERPOLATION semantic */
    COLLADA_DATA_SAMPLER_INPUT_PROPERTY_SOURCE
};

typedef enum collada_data_sampler_input_semantic
{
    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INPUT,
    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_INTERPOLATION,
    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_IN_TANGENT,
    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_OUT_TANGENT,
    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_OUTPUT,

    COLLADA_DATA_SAMPLER_INPUT_SEMANTIC_UNKNOWN
};

typedef enum collada_data_sampler_property
{
    COLLADA_DATA_SAMPLER_PROPERTY_ID, /* not settable, system_hashed_ansi_string */
};

/** TODO */
PUBLIC collada_data_sampler collada_data_sampler_create(__in __notnull tinyxml2::XMLElement* element_ptr,
                                                        __in __notnull system_hash64map      source_by_name_map);

/** TODO */
PUBLIC bool collada_data_sampler_get_input_property(__in  __notnull collada_data_sampler                sampler,
                                                    __in            collada_data_sampler_input_semantic semantic,
                                                    __in            collada_data_sampler_input_property property,
                                                    __out __notnull void*                               out_result);

/** TODO */
PUBLIC void collada_data_sampler_get_property(__in  __notnull collada_data_sampler          sampler,
                                              __in            collada_data_sampler_property property,
                                              __out __notnull void*                         out_result);

/** TODO */
PUBLIC void collada_data_sampler_release(__in __notnull __post_invalid collada_data_sampler sampler_instance);

#endif /* COLLADA_DATA_SAMPLER_H */
