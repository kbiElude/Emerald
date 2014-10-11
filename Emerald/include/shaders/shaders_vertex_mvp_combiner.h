/**
 *
 * Emerald (kbi/elude @2012)
 *
 * TODO
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_MVP_COMBINER_H
#define SHADERS_VERTEX_MVP_COMBINER_H

#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_mvp_combiner, shaders_vertex_mvp_combiner)


/** Creates a new mvp combiner vertex shader object instance.
 *
 *  @param ogl_context               OGL context to create the shader in.
 *  @param system_hashed_ansi_string Name to use for the object.
 * 
 *  @return shaders_vertex_mvp_combiner instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_mvp_combiner shaders_vertex_mvp_combiner_create(__in __notnull ogl_context,
                                                                                  __in __notnull system_hashed_ansi_string);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_mvp_combiner Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_mvp_combiner_get_shader(__in __notnull shaders_vertex_mvp_combiner);


#endif /* SHADERS_VERTEX_MVP_COMBINER_H */