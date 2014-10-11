/**
 *
 * Emerald (kbi/elude @2014)
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
PUBLIC EMERALD_API collada_data_material collada_data_material_create(__in __notnull tinyxml2::XMLElement* current_material_element_ptr,
                                                                      __in __notnull system_hash64map      effects_by_id_map,
                                                                      __in __notnull system_hash64map      materials_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_get_properties(__in      __notnull collada_data_material      material,
                                                             __out_opt           collada_data_effect*       out_effect,
                                                             __out_opt           system_hashed_ansi_string* out_id,
                                                             __out_opt           system_hashed_ansi_string* out_name);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_get_property(__in  __notnull const collada_data_material          material,
                                                           __in                  collada_data_material_property property,
                                                           __out __notnull       void*                          out_value_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_material_release(__in __notnull __post_invalid collada_data_material material);

#endif /* COLLADA_DATA_MATERIAL_H */
