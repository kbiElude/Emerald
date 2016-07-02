/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * RGB=>Yxy color space conversion implementation. Uses "tex" sampler2D uniform, passes alpha channel unmodified.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_RGB_TO_YXY_H
#define SHADERS_FRAGMENT_RGB_TO_YXY_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_rgb_to_Yxy,
                             shaders_fragment_rgb_to_Yxy)


/** Creates a shaders_fragment_rgb_to_yxy object instance.
 *
 *  @param context            Context to create the shader in.
 *  @param name               TODO
 *  @param convert_to_log_Yxy TODO
 * 
 *  @return shaders_fragment_rgb_to_yxy instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_rgb_to_Yxy shaders_fragment_rgb_to_Yxy_create(ral_context               context,
                                                                                  system_hashed_ansi_string name,
                                                                                  bool                      convert_to_log_Yxy);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shader Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_rgb_to_Yxy_get_shader(shaders_fragment_rgb_to_Yxy shader);

#endif /* SHADERS_FRAGMENT_RGB_TO_YXY_H */