/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Basic saturation implementation. The fragment shader uses "uv" input attribute.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_SATURATE_H
#define SHADERS_FRAGMENT_SATURATE_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_saturate,
                             shaders_fragment_saturate)


/** Creates a shaders_fragment_saturate object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    TODO
 * 
 *  @return shaders_fragment_saturate instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_saturate shaders_fragment_saturate_create(ral_context               context,
                                                                              system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance.
 *
 *  @param shaders_fragment_saturate Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_saturate_get_shader(shaders_fragment_saturate shader);

#endif /* SHADERS_FRAGMENT_SATURATE_H */