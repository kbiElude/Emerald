/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_EFFECT_H
#define COLLADA_DATA_EFFECT_H

#include "collada/collada_data_image.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

enum collada_data_effect_property
{
    COLLADA_DATA_EFFECT_PROPERTY_ID,
    COLLADA_DATA_EFFECT_PROPERTY_UV_MAP_NAME, /* NOTE: LW-specific */

    /* always last */
    COLLADA_DATA_EFFECT_COUNT
};

/** TODO */
PUBLIC collada_data_effect collada_data_effect_create(__in __notnull tinyxml2::XMLElement* current_effect_element_ptr,
                                                      __in __notnull system_hash64map      images_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_properties(__in      __notnull collada_data_effect    effect,
                                                           __out_opt           _collada_data_shading* out_shading,
                                                           __out_opt           int*                   out_shading_factor_item_bitmask);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_property(__in  __notnull const collada_data_effect          effect,
                                                         __in                  collada_data_effect_property property,
                                                         __out __notnull       void*                        out_data_ptr);

/** TODO */
PUBLIC EMERALD_API collada_data_shading_factor_item collada_data_effect_get_shading_factor_item_by_texcoord(__in __notnull collada_data_effect       effect,
                                                                                                            __in __notnull system_hashed_ansi_string texcoord_name);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_properties(__in      __notnull collada_data_effect               effect,
                                                                               __in                collada_data_shading_factor_item item,
                                                                               __out_opt           collada_data_shading_factor*      out_type);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_float4_properties(__in __notnull      collada_data_effect              effect,
                                                                                      __in                collada_data_shading_factor_item item,
                                                                                      __out_ecount_opt(4) float*                           out_float4);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_texture_properties(__in __notnull collada_data_effect              effect,
                                                                                       __in           collada_data_shading_factor_item item,
                                                                                       __out_opt      collada_data_image*              out_image,
                                                                                       __out_opt      _collada_data_sampler_filter*    out_mag_filter,
                                                                                       __out_opt      _collada_data_sampler_filter*    out_min_filter,
                                                                                       __out_opt      system_hashed_ansi_string*       out_texcoord_name);

/** TODO */
PUBLIC void collada_data_effect_release(__in __notnull __post_invalid collada_data_effect effect);

#endif /* COLLADA_DATA_EFFECT_H */
