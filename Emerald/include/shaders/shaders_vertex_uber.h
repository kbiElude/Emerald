/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_UBER_H
#define SHADERS_VERTEX_UBER_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_uber,
                             shaders_vertex_uber)

enum shaders_vertex_uber_light
{
    SHADERS_VERTEX_UBER_LIGHT_NONE,
    SHADERS_VERTEX_UBER_LIGHT_SH_3_BANDS, /* has to be accompanied with UBER_DIFFUSE_LIGHT_PROJECTION_SH3 in fragment uber shader */
    SHADERS_VERTEX_UBER_LIGHT_SH_4_BANDS, /* has to be accompanied with UBER_DIFFUSE_LIGHT_PROJECTION_SH4 in fragment uber shader */
};

typedef enum
{
    SHADERS_VERTEX_UBER_ITEM_NONE,
    SHADERS_VERTEX_UBER_ITEM_LIGHT,

    /* Always last */
    SHADERS_VERTEX_UBER_ITEM_UNKNOWN
} shaders_vertex_uber_item_type;


typedef unsigned int shaders_vertex_uber_item_id;


/** TODO */
PUBLIC EMERALD_API void shaders_vertex_uber_add_passthrough_input_attribute(shaders_vertex_uber       uber,
                                                                            system_hashed_ansi_string in_attribute_name,
                                                                            _shader_variable_type     in_attribute_variable_type,
                                                                            system_hashed_ansi_string out_attribute_name);

/** TODO */
PUBLIC EMERALD_API shaders_vertex_uber_item_id shaders_vertex_uber_add_light(shaders_vertex_uber              uber,
                                                                             shaders_vertex_uber_light        light,
                                                                             shaders_fragment_uber_light_type light_type,
                                                                             bool                             is_shadow_caster);

/** TODO
 * 
 *  @return shaders_vertex_uber instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_uber shaders_vertex_uber_create(ral_context               context,
                                                                  system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_item_type(shaders_vertex_uber            uber,
                                                          shaders_vertex_uber_item_id    item_id,
                                                          shaders_vertex_uber_item_type* out_item_type);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_light_type(shaders_vertex_uber         uber,
                                                           shaders_vertex_uber_item_id item_id,
                                                           shaders_vertex_uber_light*  out_light_type);

/** TODO */
PUBLIC EMERALD_API uint32_t shaders_vertex_uber_get_n_items(shaders_vertex_uber);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_uber Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_uber_get_shader(shaders_vertex_uber);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_is_dirty(shaders_vertex_uber);

/** TODO */
PUBLIC EMERALD_API void shaders_vertex_uber_recompile(shaders_vertex_uber uber);

#endif /* SHADERS_VERTEX_UBER_H */