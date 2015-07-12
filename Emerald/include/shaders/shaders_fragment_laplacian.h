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
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_laplacian,
                             shaders_fragment_laplacian)


/** Creates a shaders_fragment_laplacian object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_laplacian instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_laplacian shaders_fragment_laplacian_create(ogl_context,
                                                                                system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_laplacian Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_laplacian_get_shader(shaders_fragment_laplacian);

#endif /* SHADERS_FRAGMENT_LAPLACIAN_H */