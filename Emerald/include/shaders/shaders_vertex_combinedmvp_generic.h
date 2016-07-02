/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 * The vertex shader uses the following variables:
 *
 * INPUT   in_position - model-space position attribute;
 * INPUT   in_uv       - uv attribute;
 * UNIFORM mvp         - projection*view*model matrix; (exposed in dataVS uniform block)
 * OUTPUT  uv          - passes in_uv value.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_COMBINEDMVP_GENERIC_H
#define SHADERS_VERTEX_COMBINEDMVP_GENERIC_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_combinedmvp_generic,
                             shaders_vertex_combinedmvp_generic)


/** Creates a new combined mvp generic vertex shader object instance.
 *
 *  @param context OGL context to create the shader in.
 *  @param name    Name to use for the object.
 * 
 *  @return shaders_vertex_combinedmvp instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_generic shaders_vertex_combinedmvp_generic_create(ral_context               context,
                                                                                                system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_vertex_combinedmvp_generic_get_shader(shaders_vertex_combinedmvp_generic shader);


#endif /* SHADERS_VERTEX_COMBINEDMVP_GENERIC_H */