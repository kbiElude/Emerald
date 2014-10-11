/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_ANIMATION_H
#define COLLADA_DATA_ANIMATION_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_animation_property
{
    COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL, /* not settable, collada_data_channel */
    COLLADA_DATA_ANIMATION_PROPERTY_ID,      /* not settable, system_hashed_ansi_string */
    COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER  /* not settable, collada_data_sampler */
};

/** TODO */
PUBLIC collada_data_animation collada_data_animation_create(__in __notnull tinyxml2::XMLElement* current_animation_element_ptr,
                                                            __in __notnull collada_data          data);

/** TODO */
PUBLIC EMERALD_API void collada_data_animation_get_property(__in  __notnull collada_data_animation          animation,
                                                            __in            collada_data_animation_property property,
                                                            __out __notnull void*                           out_result);

/** TODO */
PUBLIC void collada_data_animation_release(__in __post_invalid collada_data_animation animation);

#endif /* COLLADA_DATA_ANIMATION_H */
