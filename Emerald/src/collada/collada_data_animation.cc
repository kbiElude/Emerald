/**
 *
 * Emerald (kbi/elude @2014-2016)
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
    channel           = nullptr;
    id                = system_hashed_ansi_string_get_default_empty_string();
    sampler           = nullptr;
    sources           = system_resizable_vector_create(4 /* capacity */);
    sources_by_id_map = system_hash64map_create       (sizeof(collada_data_source) );
}

/** TODO */
_collada_data_animation::~_collada_data_animation()
{
    if (channel != nullptr)
    {
        collada_data_channel_release(channel);

        channel = nullptr;
    }

    if (sampler != nullptr)
    {
        collada_data_sampler_release(sampler);

        sampler = nullptr;
    }

    if (sources != nullptr)
    {
        collada_data_source source = nullptr;

        while (system_resizable_vector_pop(sources, &source) )
        {
            collada_data_source_release(source);

            source = nullptr;
        }

        system_resizable_vector_release(sources);
        sources = nullptr;
    }

    if (sources_by_id_map != nullptr)
    {
        system_hash64map_release(sources_by_id_map);

        sources_by_id_map = nullptr;
    }
}


/** TODO */
PUBLIC collada_data_animation collada_data_animation_create(tinyxml2::XMLElement* current_animation_element_ptr,
                                                            collada_data          data)
{
    _collada_data_animation* new_animation_ptr = new (std::nothrow) _collada_data_animation;

    ASSERT_DEBUG_SYNC(new_animation_ptr != nullptr, "Out of memory")
    if (new_animation_ptr != nullptr)
    {
        new_animation_ptr->data = data;
        new_animation_ptr->id   = system_hashed_ansi_string_create(current_animation_element_ptr->Attribute("id") );

        /* Iterate through all source elements */
        tinyxml2::XMLElement* source_element_ptr = current_animation_element_ptr->FirstChildElement("source");

        while (source_element_ptr != nullptr)
        {
            collada_data_source new_source = collada_data_source_create(source_element_ptr,
                                                                        data,
                                                                        new_animation_ptr->id);

            ASSERT_DEBUG_SYNC(new_source != nullptr,
                              "Could not create a collada_data_source instance");
            if (new_source != nullptr)
            {
                /* Add the source to the vector.. */
                system_resizable_vector_push(new_animation_ptr->sources,
                                             new_source);

                /* ..and to a map that maps ids to actual source instances */
                system_hashed_ansi_string new_source_id = nullptr;

                collada_data_source_get_property(new_source,
                                                 COLLADA_DATA_SOURCE_PROPERTY_ID,
                                                &new_source_id);

                system_hash64map_insert(new_animation_ptr->sources_by_id_map,
                                        system_hashed_ansi_string_get_hash(new_source_id),
                                        new_source,
                                        nullptr,  /* on_remove_callback */
                                        nullptr); /* on_remove_callback_user_arg */
            }

            /* Move to next source element */
            source_element_ptr = source_element_ptr->NextSiblingElement("source");
        }

        /* There should exactly one channel and sampler node */
        tinyxml2::XMLElement* channel_element_ptr = current_animation_element_ptr->FirstChildElement("channel");
        tinyxml2::XMLElement* sampler_element_ptr = current_animation_element_ptr->FirstChildElement("sampler");

        ASSERT_DEBUG_SYNC(channel_element_ptr != nullptr,
                          "No <channel> node defined for <animation>");
        ASSERT_DEBUG_SYNC(sampler_element_ptr != nullptr,
                          "No <sampler> node defined for <animation>");
        ASSERT_DEBUG_SYNC(channel_element_ptr->NextSiblingElement("channel") == nullptr,
                          "More than one <channel> node defined for <animation>");
        ASSERT_DEBUG_SYNC(sampler_element_ptr->NextSiblingElement("sampler") == nullptr,
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
    }

    return (collada_data_animation) new_animation_ptr;
}

/** Please see header for specification */
PUBLIC EMERALD_API void collada_data_animation_get_property(collada_data_animation          animation,
                                                            collada_data_animation_property property,
                                                            void*                           out_result_ptr)
{
    _collada_data_animation* animation_ptr = reinterpret_cast<_collada_data_animation*>(animation);

    switch (property)
    {
        case COLLADA_DATA_ANIMATION_PROPERTY_CHANNEL:
        {
            *reinterpret_cast<collada_data_channel*>(out_result_ptr) = animation_ptr->channel;

            break;
        }

        case COLLADA_DATA_ANIMATION_PROPERTY_ID:
        {
            *reinterpret_cast<system_hashed_ansi_string*>(out_result_ptr) = animation_ptr->id;

            break;
        }

        case COLLADA_DATA_ANIMATION_PROPERTY_SAMPLER:
        {
            *reinterpret_cast<collada_data_sampler*>(out_result_ptr) = animation_ptr->sampler;

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized collada_data_animation_property value");
        }
    }
}

/* Please see header for spec */
PUBLIC void collada_data_animation_release(collada_data_animation animation)
{
    if (animation != nullptr)
    {
        delete reinterpret_cast<_collada_data_animation*>(animation);

        animation = nullptr;
    }
}