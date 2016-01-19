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
 *  @param context    Context to create the shader in.
 *  @param input_mask Input 3x3 mask for the kernel.
 *  @param name       TODO
 * 
 *  @return Shaders_fragment_convolution3x3 instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_convolution3x3 shaders_fragment_convolution3x3_create(ral_context               context,
                                                                                          const float*              input_mask,
                                                                                          system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_convolution3x3 Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_convolution3x3_get_shader(shaders_fragment_convolution3x3 shader);


#endif /* SHADERS_FRAGMENT_CONVOLUTION3X3_H */