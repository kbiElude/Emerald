/**
 *
 * Emerald (kbi/elude @2012-2015)
 * 
 * RGB=>Yxy color space conversion implementation. Uses "tex" sampler2D uniform, passes alpha channel unmodified.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_RGB_TO_YXY_H
#define SHADERS_FRAGMENT_RGB_TO_YXY_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_rgb_to_Yxy,
                             shaders_fragment_rgb_to_Yxy)


/** Creates a shaders_fragment_rgb_to_yxy object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_rgb_to_yxy instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_rgb_to_Yxy shaders_fragment_rgb_to_Yxy_create(ogl_context,
                                                                                  system_hashed_ansi_string name,
                                                                                  bool                      convert_to_log_Yxy);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_rgb_to_yxy Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_rgb_to_Yxy_get_shader(shaders_fragment_rgb_to_Yxy);

#endif /* SHADERS_FRAGMENT_RGB_TO_YXY_H */