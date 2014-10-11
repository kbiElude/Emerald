/**
 *
 * Emerald (kbi/elude @2012)
 * 
 * Filmic tone mapping operator. Exposes uniforms:
 *
 * 1) exposure
 * 2) A (shoulder strength)                 - pre-baked:0.22
 * 3) B (linear strength)                   - pre-baked:0.30
 * 4) C (linear angle)                      - pre-baked:0.10
 * 5) D (toe strength)                      - pre-baked:0.20
 * 6) E (toe numerator)                     - pre-baked:0.01
 * 7) F (toe denominator) <E/F - toe angle> - pre-baked:0.30
 * 8) W (linear white point value)          - pre-baked:11.2
 * 9) Exposure bias                         - 2.0
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE_H
#define SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE_H

#include "gfx/gfx_types.h"
#include "ogl/ogl_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_filmic_customizable, shaders_fragment_texture2D_filmic_customizable)


/** Creates a shaders_fragment_texture2D_filmic_customizable object instance.
 *
 *  @param ogl_context               Context to create the shader in.
 *  @param bool                      TODO
 *  @param system_hashed_ansi_string TODO
 * 
 *  @return shaders_fragment_texture2D_filmic_customizable instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_filmic_customizable shaders_fragment_texture2D_filmic_customizable_create(__in __notnull ogl_context, __in bool, __in __notnull system_hashed_ansi_string name);

/** Retrieves ogl_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_filmic_customizable Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ogl_shader instance.
 **/
PUBLIC EMERALD_API ogl_shader shaders_fragment_texture2D_filmic_customizable_get_shader(__in __notnull shaders_fragment_texture2D_filmic_customizable);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_FILMIC_H */