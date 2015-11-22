/**
 *
 * Emerald (kbi/elude @2012-2015)
 *
 */
#ifndef OGL_SKYBOX_H
#define OGL_SKYBOX_H

#include "ogl/ogl_types.h"
#include "ral/ral_types.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_skybox,
                             ogl_skybox)

typedef enum
{
#ifdef INCLUDE_OPENCL
    OGL_SKYBOX_LIGHT_PROJECTION_SH,
#endif

    OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,

    /** TODO: OGL_SKYBOX_CUBEMAP_TEXTURE */
} _ogl_skybox_type;

/** TODO */
#ifdef INCLUDE_OPENCL
    PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_light_projection_sh(ogl_context               context,
                                                                        sh_samples                samples,
                                                                        system_hashed_ansi_string name);
#endif

/** TODO */
PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_spherical_projection_texture(ogl_context               context,
                                                                             ral_texture               texture,
                                                                             system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ogl_skybox_draw(ogl_skybox       skybox,
                                        system_matrix4x4 modelview,
                                        system_matrix4x4 inverted_projection);

#endif /* OGL_SKYBOX_H */