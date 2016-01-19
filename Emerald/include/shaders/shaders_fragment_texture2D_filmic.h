/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Filmic tone mapping operator. Only exposes "exposure" float uniform.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_FILMIC_H
#define SHADERS_FRAGMENT_TEXTURE2D_FILMIC_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_filmic,
                             shaders_fragment_texture2D_filmic)


/** Creates a shaders_fragment_texture2D_filmic object instance.
 *
 *  @param context         Context to create the shader in.
 *  @param should_revert_y TODO
 *  @param name            TODO
 * 
 *  @return shaders_fragment_texture2D_filmic instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_filmic shaders_fragment_texture2D_filmic_create(ral_context               context,
                                                                                              bool                      should_revert_y,
                                                                                              system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_filmic Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_filmic_get_shader(shaders_fragment_texture2D_filmic filmic);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_FILMIC_H */