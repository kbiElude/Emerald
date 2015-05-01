/**
 *
 * Emerald (kbi/elude @2012-2015)
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

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_combinedmvp_generic,
                             shaders_vertex_combinedmvp_generic)


/** Creates a new combined mvp generic vertex shader object instance.
 *
 *  @param ogl_context               OGL context to create the shader in.
 *  @param system_hashed_ansi_string Name to use for the object.
 * 
 *  @return shaders_vertex_combinedmvp instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_generic shaders_vertex_combinedmvp_generic_create(__in __notnull ogl_context,
                                                                                                __in __notnull system_hashed_ansi_string);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_combinedmvp_generic Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_combinedmvp_generic_get_shader(__in __notnull shaders_vertex_combinedmvp_generic);


#endif /* SHADERS_VERTEX_COMBINEDMVP_GENERIC_H */