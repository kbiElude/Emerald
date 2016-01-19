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
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_vertex_fullscreen,
                             shaders_vertex_fullscreen)


/** TODO
 * 
 *  @return shaders_vertex_fullscreen instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_vertex_fullscreen shaders_vertex_fullscreen_create(ral_context               context,
                                                                              bool                      export_uv,
                                                                              system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_vertex_fullscreen_get_shader(shaders_vertex_fullscreen shader);


#endif /* SHADERS_VERTEX_FULLSCREEN_H */