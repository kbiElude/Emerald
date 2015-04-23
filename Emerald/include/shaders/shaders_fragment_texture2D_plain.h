/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_PLAIN_H
#define SHADERS_FRAGMENT_TEXTURE2D_PLAIN_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_plain,
                             shaders_fragment_texture2D_plain)


/** Creates a shaders_fragment_texture2D_plain object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param bool                      TODO
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_texture2D_plain instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_plain shaders_fragment_texture2D_plain_create(__in __notnull ogl_context               context,
                                                                                            __in           bool                      should_revert_y,
                                                                                            __in __notnull system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_plain Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_texture2D_plain_get_shader(__in __notnull shaders_fragment_texture2D_plain plain);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_PLAIN_H */