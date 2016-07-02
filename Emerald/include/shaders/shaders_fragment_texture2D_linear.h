/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * Linear tone mapping operator. Only exposes "exposure" float uniform (in a data uniform block)
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_LINEAR_H
#define SHADERS_FRAGMENT_TEXTURE2D_LINEAR_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_linear,
                             shaders_fragment_texture2D_linear)


/** Creates a shaders_fragment_texture2D_linear object instance.
 *
 *  @param context         Context to create the shader in.
 *  @param should_revert_y TODO
 *  @param name            TODO
 * 
 *  @return shaders_fragment_texture2D_linear instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_linear shaders_fragment_texture2D_linear_create(ral_context               context,
                                                                                              bool                      should_revert_y,
                                                                                              system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_linear Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_linear_get_shader(shaders_fragment_texture2D_linear linear);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_LINEAR_H */