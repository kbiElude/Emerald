/**
 *
 * Emerald (kbi/elude @2014-2016)
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

    /* NOTE: LW-specific */
    COLLADA_DATA_EFFECT_PROPERTY_UV_MAP_NAME,

    /* always last */
    COLLADA_DATA_EFFECT_COUNT
};

/** TODO */
PUBLIC collada_data_effect collada_data_effect_create(tinyxml2::XMLElement* current_effect_element_ptr,
                                                      system_hash64map      images_by_id_map);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_properties(collada_data_effect    effect,
                                                           _collada_data_shading* out_shading_ptr,
                                                           int*                   out_shading_factor_item_bitmask_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_property(const collada_data_effect          effect,
                                                               collada_data_effect_property property,
                                                               void*                        out_data_ptr);

/** TODO */
PUBLIC EMERALD_API collada_data_shading_factor_item collada_data_effect_get_shading_factor_item_by_texcoord(collada_data_effect       effect,
                                                                                                            system_hashed_ansi_string texcoord_name);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_properties(collada_data_effect               effect,
                                                                               collada_data_shading_factor_item item,
                                                                               collada_data_shading_factor*      out_type_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_float_properties(collada_data_effect              effect,
                                                                                     collada_data_shading_factor_item item,
                                                                                     float*                           out_float4_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_float4_properties(collada_data_effect              effect,
                                                                                      collada_data_shading_factor_item item,
                                                                                      float*                           out_float4_ptr);

/** TODO */
PUBLIC EMERALD_API void collada_data_effect_get_shading_factor_item_texture_properties(collada_data_effect              effect,
                                                                                       collada_data_shading_factor_item item,
                                                                                       collada_data_image*              out_image_ptr,
                                                                                       _collada_data_sampler_filter*    out_mag_filter_ptr,
                                                                                       _collada_data_sampler_filter*    out_min_filter_ptr,
                                                                                       system_hashed_ansi_string*       out_texcoord_name_ptr);

/** TODO */
PUBLIC void collada_data_effect_release(collada_data_effect effect);

#endif /* COLLADA_DATA_EFFECT_H */
