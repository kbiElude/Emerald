/**
 *
 * Emerald (kbi/elude @2012)
 *
 * TODO
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_COMBINEDMVP_UBO_H
#define SHADERS_VERTEX_COMBINEDMVP_UBO_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_combinedmvp_ubo,
                             shaders_vertex_combinedmvp_ubo)


/** Creates a new combined mvp UBO vertex shader object instance.
 *
 *  @param ogl_context               OGL context to create the shader in.
 *  @param uint32_t                  TODO
 *  @param system_hashed_ansi_string Name to use for the object.
 * 
 *  @return shaders_vertex_combinedmvp_ubo instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_combinedmvp_ubo shaders_vertex_combinedmvp_ubo_create(__in __notnull ogl_context,
                                                                                        __in           uint32_t, /* n matrices */
                                                                                        __in __notnull system_hashed_ansi_string);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_combinedmvp_ubo Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_combinedmvp_ubo_get_shader(__in __notnull shaders_vertex_combinedmvp_ubo);


#endif /* SHADERS_VERTEX_COMBINEDMVP_UBO_H */