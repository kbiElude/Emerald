/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SURFACE_H
#define COLLADA_DATA_SURFACE_H

#include "collada/collada_data_image.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_surface_property
{
    /* collada_data_image */
    COLLADA_DATA_SURFACE_PROPERTY_TEXTURE,

    /* bool */
    COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS,

    /* Always last */
    COLLADA_DATA_SURFACE_PROPERTY_COUNT
};


/** TODO */
PUBLIC collada_data_surface collada_data_surface_create(tinyxml2::XMLElement*     element_ptr,
                                                        system_hashed_ansi_string id,
                                                        system_hash64map          images_by_id_map);

/** TODO */
PUBLIC void collada_data_surface_get_property(const collada_data_surface    surface,
                                              collada_data_surface_property property,
                                              void*                         out_data_ptr);

/** TODO */
PUBLIC void collada_data_surface_release(collada_data_surface surface);

/** TODO */
PUBLIC void collada_data_surface_set_property(collada_data_surface          surface,
                                              collada_data_surface_property property,
                                              const void*                   value_ptr);

#endif /* COLLADA_DATA_SAMPLER2D_H */
