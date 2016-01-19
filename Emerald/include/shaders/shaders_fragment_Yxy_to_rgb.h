/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * Yxy=>RGB color space conversion implementation. Uses "tex" sampler2D uniform, passes unmodified alpha channel.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_YXY_TO_RGB_H
#define SHADERS_FRAGMENT_YXY_TO_RGB_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_Yxy_to_rgb,
                             shaders_fragment_Yxy_to_rgb)


/** Creates a shaders_fragment_Yxy_to_rgb object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    TODO
 * 
 *  @return shaders_fragment_Yxy_to_rgb instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_Yxy_to_rgb shaders_fragment_Yxy_to_rgb_create(ral_context               context,
                                                                                  system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_Yxy_to_rgb_get_shader(shaders_fragment_Yxy_to_rgb shader);

#endif /* SHADERS_FRAGMENT_YXY_TO_RGB_H */