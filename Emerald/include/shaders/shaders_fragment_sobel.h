/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * 3x3 Sobel operator implementation for 2D textures. In order to apply the operator, first execute an off-screen full-screen
 * pass with DX fragment shader, then another one with DY fragment shader. The fragment shaders use "uv" attribute.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_SOBEL_H
#define SHADERS_FRAGMENT_SOBEL_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_sobel,
                             shaders_fragment_sobel)


/** Creates a shaders_fragment_sobel object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    TODO
 * 
 *  @return Shaders_fragment_convolution3x3 instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_sobel shaders_fragment_sobel_create(ral_context               context,
                                                                        system_hashed_ansi_string name);

/** Retrieves ral_shader object for calculating horizontal derivatives associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_convolution3x3 Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_sobel_get_dx_shader(shaders_fragment_sobel shader);

/** Retrieves ral_shader object for calculating vertical derivatives associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_convolution3x3 Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_sobel_get_dy_shader(shaders_fragment_sobel shader);

#endif /* SHADERS_FRAGMENT_SOBEL_H */