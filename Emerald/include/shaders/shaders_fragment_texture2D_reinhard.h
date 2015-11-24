/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Reinhardt tone mapping operator. Only exposes "exposure" float uniform (in data uniform block)
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_REINHARDT_H
#define SHADERS_FRAGMENT_TEXTURE2D_REINHARDT_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_reinhardt,
                             shaders_fragment_texture2D_reinhardt)


/** Creates a shaders_fragment_texture2D_reinhardt object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param bool                      TODO
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_texture2D_reinhardt instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_reinhardt shaders_fragment_texture2D_reinhardt_create(ral_context               context,
                                                                                                    bool                      should_revert_y,
                                                                                                    system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_reinhardt Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_texture2D_reinhardt_get_shader(shaders_fragment_texture2D_reinhardt);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_REINHARDT_H */