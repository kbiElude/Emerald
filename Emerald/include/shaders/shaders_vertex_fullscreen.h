/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * This shader can be used for performing a full-screen off-screen rendering pass. It can optionally
 * pass UV data to other shaders.
 *
 * After using a program, to which the vertex shader has been attached, draw a 4-vertices triangle fan
 * to render a full-screen quad.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_VERTEX_FULLSCREEN_H
#define SHADERS_VERTEX_FULLSCREEN_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_fullscreen,
                             shaders_vertex_fullscreen)


/** TODO
 * 
 *  @return shaders_vertex_fullscreen instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_fullscreen shaders_vertex_fullscreen_create(__in __notnull ogl_context               context,
                                                                              __in           bool                      export_uv,
                                                                              __in __notnull system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_vertex_fullscreen Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_vertex_fullscreen_get_shader(__in __notnull shaders_vertex_fullscreen fullscreen);


#endif /* SHADERS_VERTEX_FULLSCREEN_H */