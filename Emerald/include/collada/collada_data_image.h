/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_IMAGE_H
#define COLLADA_DATA_IMAGE_H

#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_image_property
{
    COLLADA_DATA_IMAGE_PROPERTY_ID,               /* system_hashed_ansi_string */
    COLLADA_DATA_IMAGE_PROPERTY_REQUIRES_MIPMAPS, /* bool */

    /* Always last */
    COLLADA_DATA_IMAGE_PROPERTY_COUNT
};

/** TODO */
PUBLIC collada_data_image collada_data_image_create(tinyxml2::XMLElement* current_image_element_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_image_get_properties(collada_data_image         image,
                                                          system_hashed_ansi_string* out_name,
                                                          system_hashed_ansi_string* out_file_name,
                                                          system_hashed_ansi_string* out_file_name_with_path,
                                                          bool*                      out_requires_mipmaps);

/** TODO */
PUBLIC EMERALD_API void collada_data_image_get_property(const collada_data_image          image,
                                                              collada_data_image_property property,
                                                        void*                             out_data_ptr);

/** TODO */
PUBLIC void collada_data_image_release(collada_data_image image);

/** TODO */
PUBLIC void collada_data_image_set_property(collada_data_image          image,
                                            collada_data_image_property property,
                                            const void*                 data);

#endif /* COLLADA_DATA_IMAGE_H */
