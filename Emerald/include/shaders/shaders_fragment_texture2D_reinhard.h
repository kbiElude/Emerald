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
 *  @param context         Context to create the shader in.
 *  @param should_revert_y TODO
 *  @param name            TODO
 * 
 *  @return shaders_fragment_texture2D_reinhardt instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_reinhardt shaders_fragment_texture2D_reinhardt_create(ral_context               context,
                                                                                                    bool                      should_revert_y,
                                                                                                    system_hashed_ansi_string name);

/** Retrieves ralo_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_reinhardt_get_shader(shaders_fragment_texture2D_reinhardt shader);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_REINHARDT_H */