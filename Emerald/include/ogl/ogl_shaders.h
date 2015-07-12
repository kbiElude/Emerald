/**
 *
 * Emerald (kbi/elude @2015)
 *
 * ogl_shaders acts as a context-wide storage for created shaders.
 *
 * It caches all created shaders by their names, thanks to call-backs
 * coming from ogl_shader_create().
 *
 * Every time ogl_shader_create() is called, the function also checks
 * if a shader with a given name has not already been created. If so,
 * it only bumps up the ref counter of the ogl_shader instance and
 * returns the already initialized instance.
 *
 * The implementation itself is more than just straightforward, but the
 * separate module serves merely as an abstraction layer, rather than to
 * hide a complex logic of any kind.
 */
#ifndef OGL_SHADERS_H
#define OGL_SHADERS_H

#include "ogl/ogl_types.h"
#include "system/system_types.h"


/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC ogl_shaders ogl_shaders_create();

/** TODO
 *
 *  Internal usage only.
 **/
PUBLIC ogl_shader ogl_shaders_get_shader_by_name(ogl_shaders               shaders,
                                                 system_hashed_ansi_string shader_has);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void ogl_shaders_release(ogl_shaders shaders);

/** TODO.
 *
 *  Internal usage only.
 **/
PUBLIC void ogl_shaders_register_shader(ogl_shaders shaders,
                                        ogl_shader  shader);

/** TODO.
 *
 *  Internal usage only.
 */
PUBLIC void ogl_shaders_unregister_shader(ogl_shaders shaders,
                                          ogl_shader  shader);


#endif /* OGL_SHADERS_H */
