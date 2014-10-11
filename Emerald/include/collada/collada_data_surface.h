/**
 *
 * Emerald (kbi/elude @2014)
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
    COLLADA_DATA_SURFACE_PROPERTY_TEXTURE,                  /* collada_data_image */
    COLLADA_DATA_SURFACE_PROPERTY_TEXTURE_REQUIRES_MIPMAPS, /* bool */

    /* Always last */
    COLLADA_DATA_SURFACE_PROPERTY_COUNT
};


/** TODO */
PUBLIC collada_data_surface collada_data_surface_create(__in __notnull tinyxml2::XMLElement*     element_ptr,
                                                        __in __notnull system_hashed_ansi_string id,
                                                        __in __notnull system_hash64map          images_by_id_map);

/** TODO */
PUBLIC void collada_data_surface_get_property(__in  __notnull const collada_data_surface          surface,
                                              __in                  collada_data_surface_property property,
                                              __out __notnull void*                               out_data);

/** TODO */
PUBLIC void collada_data_surface_release(__in __notnull __post_invalid collada_data_surface surface);

/** TODO */
PUBLIC void collada_data_surface_set_property(__in __notnull collada_data_surface          surface,
                                              __in           collada_data_surface_property property,
                                              __in __notnull const void*                   value_ptr);

#endif /* COLLADA_DATA_SAMPLER2D_H */
