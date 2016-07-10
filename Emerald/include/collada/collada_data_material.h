/**
 *
 * Emerald (kbi/elude @2014-2016)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_MATERIAL_H
#define COLLADA_DATA_MATERIAL_H

#include "collada_data_effect.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_material_property
{
    COLLADA_DATA_MATERIAL_PROPERTY_ID,

    /* Always last */
    COLLADA_DATA_MATERIAL_PROPERTY_COUNT
};

/** TODO */
PUBLIC EMERALD_API collada_data_material collada_data_material_create(tinyxml2::XMLElement* current_material_element_ptr,
                                                                      system_hash64map      effects_by_id_map,
                                                                      system_hash64map      materials_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_get_properties(collada_data_material      material,
                                                             collada_data_effect*       out_effect_ptr,
                                                             system_hashed_ansi_string* out_id_ptr,
                                                             system_hashed_ansi_string* out_name_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_get_property(const collada_data_material    material,
                                                           collada_data_material_property property,
                                                           void*                          out_value_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_release(collada_data_material material);

#endif /* COLLADA_DATA_MATERIAL_H */
