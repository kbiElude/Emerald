/**
 *
 * Emerald (kbi/elude @2015)
 *
 * TODO
 */
#ifndef RAGL_TEXTURES_H
#define RAGL_TEXTURES_H

#include "ogl/ogl_types.h"
#include "raGL/raGL_texture.h"
#include "ral/ral_types.h"
#include "system/system_types.h"


DECLARE_HANDLE(raGL_textures);


/* NOTE: Image memory manager is ref-counted, in order to correctly support context sharing for
 *       the OpenGL back-end. */
REFCOUNT_INSERT_DECLARATIONS(raGL_textures,
                             raGL_textures);


/** TODO.
 *
 *  NOTE: This functionality should only be accessed via raGL_backend_*().
 *
 *  TODO
 **/
PUBLIC raGL_textures raGL_textures_create(ogl_context context);

/** TODO */
PUBLIC raGL_texture raGL_textures_get_texture_from_pool(raGL_textures textures,
                                                        ral_texture   texture_ral);

/** TODO
 *
 *  NOTE: This function will soon be removed! DO NOT use.
 */
PUBLIC raGL_texture raGL_textures_get_texture_from_pool_with_create_info(raGL_textures                  textures,
                                                                         const ral_texture_create_info* info_ptr);

/** TODO */
PUBLIC void raGL_textures_return_to_pool(raGL_textures textures,
                                         raGL_texture  texture);


#endif /* RAGL_TEXTURES_H */