/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 * Generic 3x3 convolution fragment shader. The user must provide the following output variables in a 
 * vertex shader assigned to the same program as the fragment shader:
 *
 * - uv 
 *
 * User also must set the "texture" uniform for the program.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_CONVOLUTION3X3_H
#define SHADERS_FRAGMENT_CONVOLUTION3X3_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_convolution3x3,
                             shaders_fragment_convolution3x3)


/** Creates and compiles a 3x3 convolution fragment shader, using user's mask.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param float*                    Input 3x3 mask for the kernel.
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return Shaders_fragment_convolution3x3 instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_convolution3x3 shaders_fragment_convolution3x3_create(ral_context,
                                                                                          const float*,
                                                                                          system_hashed_ansi_string);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_convolution3x3 Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_convolution3x3_get_shader(shaders_fragment_convolution3x3);


#endif /* SHADERS_FRAGMENT_CONVOLUTION3X3_H */