/**
 *
 * Emerald (kbi/elude @2012-2014)
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_UBER_H
#define SHADERS_VERTEX_UBER_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_uber, shaders_vertex_uber)

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
PUBLIC EMERALD_API void shaders_vertex_uber_add_passthrough_input_attribute(__in __notnull shaders_vertex_uber       uber,
                                                                            __in __notnull system_hashed_ansi_string in_attribute_name,
                                                                            __in __notnull _shader_variable_type     in_attribute_variable_type,
                                                                            __in __notnull system_hashed_ansi_string out_attribute_name);

/** TODO */
PUBLIC EMERALD_API shaders_vertex_uber_item_id shaders_vertex_uber_add_light(__in __notnull shaders_vertex_uber       uber,
                                                                             __in           shaders_vertex_uber_light light);

/** TODO
 * 
 *  @return shaders_vertex_uber instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_uber shaders_vertex_uber_create(__in __notnull ogl_context,
                                                                  __in __notnull system_hashed_ansi_string);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_item_type(__in __notnull shaders_vertex_uber            uber,
                                                          __in           shaders_vertex_uber_item_id    item_id,
                                                          __out          shaders_vertex_uber_item_type* out_item_type);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_get_light_type(__in  __notnull shaders_vertex_uber         uber,
                                                           __in            shaders_vertex_uber_item_id item_id,
                                                           __out           shaders_vertex_uber_light*  out_light_type);

/** TODO */
PUBLIC EMERALD_API uint32_t shaders_vertex_uber_get_n_items(__in __notnull shaders_vertex_uber);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_uber Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_uber_get_shader(__in __notnull shaders_vertex_uber);

/** TODO */
PUBLIC EMERALD_API bool shaders_vertex_uber_is_dirty(__in __notnull shaders_vertex_uber);

/** TODO */
PUBLIC EMERALD_API void shaders_vertex_uber_recompile(__in __notnull shaders_vertex_uber uber);

#endif /* SHADERS_VERTEX_UBER_H */