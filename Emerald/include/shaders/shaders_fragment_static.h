/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_STATIC_H
#define SHADERS_FRAGMENT_STATIC_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_static,
                             shaders_fragment_static)


/** Creates a shaders_fragment_static object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    TODO
 * 
 *  @return shaders_fragment_static instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_static shaders_fragment_static_create(ral_context               context,
                                                                          system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_static_get_shader(shaders_fragment_static shader);

#endif /* SHADERS_FRAGMENT_STATIC_H */