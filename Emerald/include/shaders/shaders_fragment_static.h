/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_STATIC_H
#define SHADERS_FRAGMENT_STATIC_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_static, shaders_fragment_static)

typedef enum
{

    FRAGMENT_STATIC_ATTRIBUTE,
    FRAGMENT_STATIC_UNIFORM

} shaders_fragment_static_type;

/** Creates a shaders_fragment_static object instance.
 *
 *  @param ogl_context                  Context to create the shader in.
 *  @param shaders_fragment_static_type TODO
 *  @param system_hashed_ansi_string    TODO
 * 
 *  @return shaders_fragment_static instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_static shaders_fragment_static_create(__in __notnull ogl_context,
                                                                          __in           shaders_fragment_static_type,
                                                                          __in __notnull system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_static Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_static_get_shader(__in __notnull shaders_fragment_static);

#endif /* SHADERS_FRAGMENT_STATIC_H */