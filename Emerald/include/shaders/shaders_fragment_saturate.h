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
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_saturate,
                             shaders_fragment_saturate)


/** Creates a shaders_fragment_saturate object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_saturate instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_saturate shaders_fragment_saturate_create(__in __notnull ogl_context,
                                                                              __in __notnull system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_saturate Shader instance to retrieve the object from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_saturate_get_shader(__in __notnull shaders_fragment_saturate);

#endif /* SHADERS_FRAGMENT_SATURATE_H */