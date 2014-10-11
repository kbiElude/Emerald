/**
 *
 * Emerald (kbi/elude @2014)
 *
 */
#include "shared.h"
#include "collada/collada_data_image.h"
#include "collada/collada_data_surface.h"
#include "system/system_assertions.h"
#include "system/system_hash64map.h"
#include "system/system_resizable_vector.h"

/** Indecently simplified representation of <surface> node contents.
 *
 *  It is assumed the surface is always of 2D type.
 **/
typedef struct _collada_data_surface
{
    system_hashed_ansi_string id;
    collada_data_image        texture;
} _collada_data_surface;


/** TODO */
PUBLIC collada_data_surface collada_data_surface_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                        __in __notnull system_hashed_ansi_string id,
                                                        __in __notnull system_hash64map          images_by_id_map)
{
    /* Sanity check */
    const char* surface_type = element_ptr->Attribute("type");

    ASSERT_DEBUG_SYNC(strcmp(surface_type, "2D") == 0, "Only 2D surfaces are supported at the moment");

    /* Spawn the descriptor */
    _collada_data_surface* new_surface_ptr = new (std::nothrow) _collada_data_surface;

    ASSERT_ALWAYS_SYNC(new_surface_ptr != NULL, "Out of memory");
    if (new_surface_ptr != NULL)
    {
        new_surface_ptr->id = id;

        /* Extract texture name */
        tinyxml2::XMLElement* init_from_node_ptr = element_ptr->FirstChildElement("init_from");

        ASSERT_DEBUG_SYNC(init_from_node_ptr != NULL, "<init_from> node not defined in <newparam> node");
        if (init_from_node_ptr != NULL)
        {
            const char* texture_name_ptr = init_from_node_ptr->GetText();
            ASSERT_DEBUG_SYNC(texture_name_ptr != NULL, "Texture name defined in <init_from> is NULL");

            /* Find the image */
            collada_data_image        image            = NULL;
            system_hashed_ansi_string texture_name_has = system_hashed_ansi_string_create(texture_name_ptr);

            if (system_hash64map_get(images_by_id_map, system_hashed_ansi_string_get_hash(texture_name_has), &image))
            {
                ASSERT_DEBUG_SYNC(image != NULL, "Image descriptor is NULL");

                new_surface_ptr->texture = image;
            }
            else
            {
                ASSERT_DEBUG_SYNC(false,
                                  "Could not retrieve image descriptor for image name [%s]",
                                  system_hashed_ansi_string_get_buffer(texture_name_has)
                                 );
            }
        } /* if (init_from_node_ptr != NULL) */
    } /* if (new_surface_ptr != NULL) */

    return (collada_data_surface) new_surface_ptr;
}

/** Please see header for specification */
PUBLIC void collada_data_surface_get_property(__in  __notnull const collada_data_surface          surface,
                                              __in                  collada_data_surface_property property,
                                              __out __notnull void*                               out_data)
{
    const _collada_data_surface* surface_ptr = (const _collada_data_surface*) surface;

    switch (property)
    {
        case COLLADA_DATA_SURFACE_PROPERTY_TEXTURE:
        {
            *((collada_data_image*) out_data) = surface_ptr->texture;

            break;
        }

        case COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS:
        {
            collada_data_image_get_property(surface_ptr->texture,
                                            COLLADA_DATA_IMAGE_PROPERTY_REQUIRES_MIPMAPS,
                                            out_data);

            break;
        } /* case COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS: */


    } /* switch (property) */
}

/** Please see header for spec */
PUBLIC void collada_data_surface_release(__in __notnull __post_invalid collada_data_surface surface)
{
    delete (_collada_data_surface*) surface;

    surface = NULL;
}

/** Please see header for specification */
PUBLIC void collada_data_surface_set_property(__in __notnull collada_data_surface          surface,
                                              __in           collada_data_surface_property property,
                                              __in __notnull const void*                   value_ptr)
{
    _collada_data_surface* surface_ptr = (_collada_data_surface*) surface;

    switch (property)
    {
        case COLLADA_DATA_SURFACE_PROPERTY_TEXTURE:
        {
            surface_ptr->texture = *((collada_data_image*) value_ptr);

            break;
        }

        case COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS:
        {
            collada_data_image_set_property(surface_ptr->texture,
                                            COLLADA_DATA_IMAGE_PROPERTY_REQUIRES_MIPMAPS,
                                            value_ptr);

            break;
        }

        default:
        {
            ASSERT_DEBUG_SYNC(false,
                              "Unrecognized COLLADA surface property requested");
        }
    } /* switch (property) */

}
