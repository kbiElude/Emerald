/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_sampler2D.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Describes data stored in <sampler2D> node */
typedef struct _collada_data_sampler2D
{
    system_hashed_ansi_string    id;
    _collada_data_sampler_filter mag_filter;
    _collada_data_sampler_filter min_filter;
    collada_data_surface         surface;

    _collada_data_sampler2D();
} _collada_data_sampler2D;


_collada_data_sampler2D::_collada_data_sampler2D()
{
    id         = system_hashed_ansi_string_get_default_empty_string();
    mag_filter = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
    min_filter = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;
    surface    = NULL;
}

/** TODO */
PUBLIC _collada_data_sampler_filter _collada_data_get_sampler_filter_from_filter_node(tinyxml2::XMLElement* element_ptr)
{
    const char*                  node_text = element_ptr->GetText();
    _collada_data_sampler_filter result    = COLLADA_DATA_SAMPLER_FILTER_UNKNOWN;

    if (strcmp(node_text, "NONE")                   == 0) result = COLLADA_DATA_SAMPLER_FILTER_NONE;                   else
    if (strcmp(node_text, "NEAREST")                == 0) result = COLLADA_DATA_SAMPLER_FILTER_NEAREST;                else
    if (strcmp(node_text, "LINEAR")                 == 0) result = COLLADA_DATA_SAMPLER_FILTER_LINEAR;                 else
    if (strcmp(node_text, "NEAREST_MIPMAP_NEAREST") == 0) result = COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_NEAREST; else
    if (strcmp(node_text, "LINEAR_MIPMAP_NEAREST")  == 0) result = COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_NEAREST;  else
    if (strcmp(node_text, "NEAREST_MIPMAP_LINEAR")  == 0) result = COLLADA_DATA_SAMPLER_FILTER_NEAREST_MIPMAP_LINEAR;  else
    if (strcmp(node_text, "LINEAR_MIPMAP_LINEAR")   == 0) result = COLLADA_DATA_SAMPLER_FILTER_LINEAR_MIPMAP_LINEAR;   else
    {
        ASSERT_DEBUG_SYNC(false, "Unrecognized <fx_sampler_filter_common> node value");
    }

    return result;
}

/** TODO */
PUBLIC collada_data_sampler2D collada_data_sampler2D_create(system_hashed_ansi_string id,
                                                            tinyxml2::XMLElement*     element_ptr,
                                                            system_hash64map          surfaces_by_id_map)
{
    tinyxml2::XMLElement* magfilter_element_ptr = element_ptr->FirstChildElement("magfilter");
    tinyxml2::XMLElement* minfilter_element_ptr = element_ptr->FirstChildElement("minfilter");
    tinyxml2::XMLElement* source_element_ptr    = element_ptr->FirstChildElement("source");

    ASSERT_DEBUG_SYNC(source_element_ptr != NULL, "Source is NULL");

    /* Spawn the descriptor */
    _collada_data_sampler2D* new_sampler = new (std::nothrow) _collada_data_sampler2D;

    ASSERT_ALWAYS_SYNC(new_sampler != NULL, "Out of memory");
    if (new_sampler != NULL)
    {
        /* Identify the surface the sampler is to operate on */
        system_hashed_ansi_string surface_name = system_hashed_ansi_string_create(source_element_ptr->GetText() );
        collada_data_surface      surface      = NULL;

        system_hash64map_get(surfaces_by_id_map, system_hashed_ansi_string_get_hash(surface_name), &surface);
        ASSERT_DEBUG_SYNC(surface != NULL,
                          "Could not find a surface by the name of [%s]",
                          system_hashed_ansi_string_get_buffer(surface_name) );

        /* Fill the descriptor */
        new_sampler->id         = id;
        new_sampler->mag_filter = _collada_data_get_sampler_filter_from_filter_node(magfilter_element_ptr);
        new_sampler->min_filter = _collada_data_get_sampler_filter_from_filter_node(minfilter_element_ptr);
        new_sampler->surface    = surface;

        /* If this sampler uses mipmap filtering, flag the requirement in the texture descriptor */
        if (new_sampler->min_filter != GL_NEAREST && new_sampler->min_filter != GL_LINEAR)
        {
            bool new_value = true;

            collada_data_surface_set_property(new_sampler->surface,
                                              COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS,
                                              &new_value);
        }
    } /* if (new_sampler != NULL) */

    return (collada_data_sampler2D) new_sampler;
}

/* Please see header for spec */
PUBLIC void collada_data_sampler2D_get_properties(const collada_data_sampler2D        sampler,
                                                       collada_data_surface*         out_surface,
                                                       _collada_data_sampler_filter* out_mag_filter,
                                                       _collada_data_sampler_filter* out_min_filter)
{
    const _collada_data_sampler2D* sampler_ptr = (const _collada_data_sampler2D*) sampler;

    if (out_surface != NULL)
    {
        *out_surface = sampler_ptr->surface;
    }

    if (out_mag_filter != NULL)
    {
        *out_mag_filter = sampler_ptr->mag_filter;
    }

    if (out_min_filter != NULL)
    {
        *out_min_filter = sampler_ptr->min_filter;
    }
}

/* Please see header for spec */
PUBLIC void collada_data_sampler2D_release(collada_data_sampler2D sampler)
{
    delete (_collada_data_sampler2D*) sampler;

    sampler = NULL;
}