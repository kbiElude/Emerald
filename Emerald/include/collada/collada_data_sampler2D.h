/**
 *
 * Emerald (kbi/elude @2014)
 *
 * Private functions only.
 */
#ifndef COLLADA_DATA_SAMPLER2D_H
#define COLLADA_DATA_SAMPLER2D_H

#include "collada/collada_data_surface.h"
#include "collada/collada_types.h"
#include "tinyxml2.h"

/** TODO */
PUBLIC collada_data_sampler2D collada_data_sampler2D_create(__in __notnull system_hashed_ansi_string id,
                                                            __in __notnull tinyxml2::XMLElement*     element_ptr,
                                                            __in __notnull system_hash64map          surfaces_by_id_map);

/** TODO */
PUBLIC void collada_data_sampler2D_get_properties(__in     __notnull const collada_data_sampler2D        sampler,
                                                  __out_opt                collada_data_surface*         out_surface,
                                                  __out_opt                _collada_data_sampler_filter* out_mag_filter,
                                                  __out_opt                _collada_data_sampler_filter* out_min_filter);

/** TODO */
PUBLIC _collada_data_sampler_filter collada_data_get_sampler_filter_from_filter_node(__in __notnull tinyxml2::XMLElement* element_ptr);

/** TODO */
PUBLIC void collada_data_sampler2D_release(__in __notnull __post_invalid collada_data_sampler2D sampler);

#endif /* COLLADA_DATA_SAMPLER2D_H */
