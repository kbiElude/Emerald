/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_animation.h"
#include "collada/collada_data_channel.h"
#include "collada/collada_data_sampler.h"
#include "collada/collada_data_source.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_log.h"
#include "system/system_resizable_vector.h"

/** Describes a single animation, as described by <library_animations>/<animation> */
typedef struct _collada_data_animation
{
    collada_data              data;
    system_hashed_ansi_string id;

    /* Holds collada_data_source instances */
    collada_data_channel    channel;
    collada_data_sampler    sampler;
    system_resizable_vector sources;
    system_hash64map        sources_by_id_map;

    _collada_data_animation();
   ~_collada_data_animation();
} _collada_data_animation;

/** TODO */
_collada_data_animation::_collada_data_animation()
{
    channel           = NULL;
    id                = system_hashed_ansi_string_get_default_empty_string();
    sampler           = NULL;
    sources           = system_resizable_vector_create(4, /* capacity */
                                                       sizeof(collada_data_source) );
    sources_by_id_map = system_hash64map_create       (sizeof(collada_data_source) );
}

/** TODO */
_collada_data_animation::~_collada_data_animation()
{
    if (channel != NULL)
    {
        collada_data_channel_release(channel);

        channel = NULL;
    }

    if (sampler != NULL)
    {
        collada_data_sampler_release(sampler);

        sampler = NULL;
    }

    if (sources != NULL)
    {
        collada_data_source source = NULL;

        while (system_resizable_vector_pop(sources, &source) )
        {
            collada_data_source_release(source);

            source = NULL;
        }

        system_resizable_vector_release(sources);
        sources = NULL;
    } /* if (sources != NULL) */

    if (sources_by_id_map != NULL)
    {
        system_hash64map_release(sources_by_id_map);

        sources_by_id_map = NULL;
    }
}


/** TODO */
PUBLIC collada_data_animation collada_data_animation_create(__in __notnull tinyxml2::XMLElement* current_animation_element_ptr,
                                                            __in __notnull collada_data          data)
{
    _collada_data_animation* new_animation_ptr = new (std::nothrow) _collada_data_animation;

    ASSERT_DEBUG_SYNC(new_animation_ptr != NULL, "Out of memory")
    if (new_animation_ptr != NULL)
    {
        new_animation_ptr->data = data;
        new_animation_ptr->id   = system_hashed_ansi_string_create(current_animation_element_ptr->Attribute("id") );

        /* Iterate through all source elements */
        tinyxml2::XMLElement* source_element_ptr = current_animation_element_ptr->FirstChildElement("source");

        while (source_element_ptr != NULL)
        {
            collada_data_source new_source = collada_data_source_create(source_element_ptr,
                                                                        data,
                                                                        new_animation_ptr->id);

            ASSERT_DEBUG_SYNC(new_source != NULL,
                              "Could not create a collada_data_source instance");
            if (new_source != NULL)
            {
                /* Add the source to the vector.. */
                system_resizable_vector_push(new_animation_ptr->sources,
                                             new_source);

                /* ..and to a map that maps ids to actual source instances */
                system_hashed_ansi_string new_source_id = NULL;

                collada_data_source_get_property(new_source,
                                                 COLLADA_DATA_SOURCE_PROPERTY_ID,
                                                &new_source_id);

                system_hash64map_insert(new_animation_ptr->sources_by_id_map,
                                        system_hashed_ansi_string_get_hash(new_source_id),
                                        new_source,
                                        NULL,  /* on_remove_callback */
                                        NULL); /* on_remove_callback_user_arg */
            }

            /* Move to next source element */
            source_element_ptr = source_element_ptr->NextSiblingElement("source");
        } /* while (source_element_ptr != NULL) */

        /* There should exactly one channel and sampler node */
        tinyxml2::XMLElement* channel_element_ptr = current_animation_element_ptr->FirstChildElement("channel");
        tinyxml2::XMLElement* sampler_element_ptr = current_animation_element_ptr->FirstChildElement("sampler");

        ASSERT_DEBUG_SYNC(channel_element_ptr != NULL,
                          "No <channel> node defined for <animation>");
        ASSERT_DEBUG_SYNC(sampler_element_ptr != NULL,
                          "No <sampler> node defined for <animation>");
        ASSERT_DEBUG_SYNC(channel_element_ptr->NextSiblingElement("channel") == NULL,
                          "More than one <channel> node defined for <animation>");
        ASSERT_DEBUG_SYNC(sampler_element_ptr->NextSiblingElement("sampler") == NULL,
                          "More than one <sampler> node defined for <animation>");

        /* Process the sampler node first */
        collada_data_sampler sampler = collada_data_sampler_create(sampler_element_ptr,
                                                                   new_animation_ptr->sources_by_id_map);

        /* Continue with channels */
        collada_data_channel channel = collada_data_channel_create(channel_element_ptr,
                                                                   sampler,
                                                                   data);

        /* Store the results in the descriptor */
        new_animation_ptr->channel = channel;
        new_animation_ptr->sampler = sampler;
    } /* if (new_animation_ptr != NULL) */

    return (collada_data_animation) new_animation_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_animation_get_property(__in  __notnull collada_data_animation          animation,
                                                            __in            collada_data_animation_property property,
                                                            __out __notnull void*                           out_result)
{
    _collada_data_animation* animation_ptr = (_collada_data_animation*) animation;

    switch (property)
    {
        case COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL:
        {
            *((collada_data_channel*) out_result) = animation_ptr->channel;

            break;
        }

        case COLLADA_DATA_ANIMATION_PROPERTY_ID:
        {
            *((system_hashed_ansi_string*) out_result) = animation_ptr->id;

            break;
        }

        case COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER:
        {
            *((collada_data_sampler*) out_result) = animation_ptr->sampler;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_animation_property value");
        }
    } /* switch (property) */
}

/* Please see header for spec */
PUBLIC void collada_data_animation_release(__in __post_invalid collada_data_animation animation)
{
    if (animation != NULL)
    {
        delete (_collada_data_animation*) animation;

        animation = NULL;
    }
}