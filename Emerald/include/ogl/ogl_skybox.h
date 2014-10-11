/**
 *
 * Emerald (kbi/elude @2012)
 *
 */
#ifndef OGL_SKYBOX_H
#define OGL_SKYBOX_H

#include "ogl/ogl_types.h"
#include "sh/sh_types.h"

REFCOUNT_INSERT_DECLARATIONS(ogl_skybox, ogl_skybox)

typedef enum
{
    OGL_SKYBOX_LIGHT_PROJECTION_SH,
    OGL_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,
    /** TODO: OGL_SKYBOX_CUBEMAP_TEXTURE */
} _ogl_skybox_type;

/** TODO */
PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_light_projection_sh(__in __notnull ogl_context               context,
                                                                    __in __notnull sh_samples                samples,
                                                                    __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API ogl_skybox ogl_skybox_create_spherical_projection_texture(__in __notnull ogl_context               context,
                                                                             __in __notnull ogl_texture               texture,
                                                                             __in __notnull system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API void ogl_skybox_draw(__in __notnull ogl_skybox       skybox,
                                        __in           ogl_texture      light_sh_data_tbo,
                                        __in __notnull system_matrix4x4 modelview,
                                        __in __notnull system_matrix4x4 inverted_projection);

#endif /* OGL_SKYBOX_H */