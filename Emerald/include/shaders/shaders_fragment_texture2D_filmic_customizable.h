/**
 *
 * Emerald (kbi/elude @2012-2016)
 * 
 * Filmic tone mapping operator. The data uniform block exposes:
 *
 * 1) exposure
 * 2) A (shoulder strength)                 - suggested setting:0.22
 * 3) B (linear strength)                   - suggested setting:0.30
 * 4) C (linear angle)                      - suggested setting:0.10
 * 5) D (toe strength)                      - suggested setting:0.20
 * 6) E (toe numerator)                     - suggested setting:0.01
 * 7) F (toe denominator) <E/F - toe angle> - suggested setting:0.30
 * 8) W (linear white point value)          - suggested setting:11.2
 * 9) Exposure bias                         - suggested setting:2.0
 *
 * The implementation is reference counter-based.
 */
#ifndef SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE_H
#define SHADERS_FRAGMENT_TEXTURE2D_FILMIC_CUSTOMIZABLE_H

#include "ral/ral_types.h"

REFCOUNT_INSERT_DECLARATIONS(shaders_fragment_texture2D_filmic_customizable,
                             shaders_fragment_texture2D_filmic_customizable)


/** Creates a shaders_fragment_texture2D_filmic_customizable object instance.
 *
 *  @param context         Context to create the shader in.
 *  @param should_revert_y TODO
 *  @param name            TODO
 * 
 *  @return shaders_fragment_texture2D_filmic_customizable instance if successful, NULL otherwise.
 */
PUBLIC EMERALD_API shaders_fragment_texture2D_filmic_customizable shaders_fragment_texture2D_filmic_customizable_create(ral_context               context,
                                                                                                                        bool                      should_revert_y,
                                                                                                                        system_hashed_ansi_string name);

/** Retrieves ral_shader object associated with the instance. Do not release the object or modify it in any way.
 *
 *  @param shaders_fragment_texture2D_filmic_customizable Shader instance to retrieve the shader from. Cannot be NULL.
 *
 *  @return ral_shader instance.
 **/
PUBLIC EMERALD_API ral_shader shaders_fragment_texture2D_filmic_customizable_get_shader(shaders_fragment_texture2D_filmic_customizable filmic);

#endif /* SHADERS_FRAGMENT_TEXTURE2D_FILMIC_H */