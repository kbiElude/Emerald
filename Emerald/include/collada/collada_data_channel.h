/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_CHANNEL_H
#define COLLADA_DATA_CHANNEL_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_channel_property
{
    /* not settable, collada_data_sampler */
    COLLADA_DATA_CHANNEL_PROPERTY_SAMPLER,

    /* not settable, void* */
    COLLADA_DATA_CHANNEL_PROPERTY_TARGET,

    /* not settable, collada_data_channel_target_component */
    COLLADA_DATA_CHANNEL_PROPERTY_TARGET_COMPONENT,

    /* not settable, collada_data_channel_target_type */
    COLLADA_DATA_CHANNEL_PROPERTY_TARGET_TYPE
};

enum collada_data_channel_target_component
{
    COLLADA_DATA_CHANNEL_TARGET_COMPONENT_ANGLE,
    COLLADA_DATA_CHANNEL_TARGET_COMPONENT_X,
    COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Y,
    COLLADA_DATA_CHANNEL_TARGET_COMPONENT_Z,

    COLLADA_DATA_CHANNEL_TARGET_COMPONENT_UNKNOWN
};

enum collada_data_channel_target_type
{
    COLLADA_DATA_CHANNEL_TARGET_TYPE_CAMERA_INSTANCE,
    COLLADA_DATA_CHANNEL_TARGET_TYPE_GEOMETRY_INSTANCE,
    COLLADA_DATA_CHANNEL_TARGET_TYPE_LIGHT_INSTANCE,
    COLLADA_DATA_CHANNEL_TARGET_TYPE_TRANSFORMATION,

    COLLADA_DATA_CHANNEL_TARGET_TYPE_UNKNOWN
};

/** TODO */
PUBLIC collada_data_channel collada_data_channel_create(tinyxml2::XMLElement* channel_element_ptr,
                                                        collada_data_sampler  sampler,
                                                        collada_data          data);

/** TODO */
PUBLIC void collada_data_channel_get_property(collada_data_channel          channel,
                                              collada_data_channel_property property,
                                              void*                         out_result_ptr);

/** TODO */
PUBLIC void collada_data_channel_release(collada_data_channel channel);

#endif /* COLLADA_DATA_CHANNEL_H */
