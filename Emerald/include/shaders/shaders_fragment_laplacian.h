/**
 *
 * Emerald (kbi/elude @2012-2105)
 * 
 * 3x3 diagonal Laplacian operator implementation for 2D textures. In order to apply the operator, execute an off-screen full-screen
 * pass with the fragment shader. The fragment shaders use "uv" attribute.
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_LAPLACIAN_H
#define SHADERS_FRAGMENT_LAPLACIAN_H

#include "gfx/gfx_types.h"
#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_laplacian,
                             shaders_fragment_laplacian)


/** Creates a shaders_fragment_laplacian object instance.
 *
 *  @param context Context to create the shader in.
 *  @param name    Name to use for the new object.
 * 
 *  @return shaders_fragment_laplacian instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_laplacian shaders_fragment_laplacian_create(ral_context               context,
                                                                                system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance.
 *
 *  @param shader Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_laplacian_get_shader(shaders_fragment_laplacian shader);

#endif /* SHADERS_FRAGMENT_LAPLACIAN_H */