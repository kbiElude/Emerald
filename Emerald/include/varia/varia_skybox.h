/**
 *
 * Emerald (kbi/elude @2012-2016)
 *
 */
#ifndef VARIA_SKYBOX_H
#define VARIA_SKYBOX_H

#include "ral/ral_types.h"
#include "sh/sh_types.h"
#include "varia/varia_types.h"

REFCOUNT_INSERT_DECLARATIONS(varia_skybox,
                             varia_skybox)

typedef enum
{
#ifdef INCLUDE_OPENCL
    VARIA_SKYBOX_LIGHT_PROJECTION_SH,
#endif

    VARIA_SKYBOX_SPHERICAL_PROJECTION_TEXTURE,

    /** TODO: VARIA_SKYBOX_CUBEMAP_TEXTURE */
} _varia_skybox_type;

/** TODO */
#ifdef INCLUDE_OPENCL
    PUBLIC EMERALD_API varia_skybox varia_skybox_create_light_projection_sh(ral_context               context,
                                                                            sh_samples                samples,
                                                                            system_hashed_ansi_string name);
#endif

/** TODO */
PUBLIC EMERALD_API varia_skybox varia_skybox_create_spherical_projection_texture(ral_context               context,
                                                                                 ral_texture               texture,
                                                                                 system_hashed_ansi_string name);

/** TODO */
PUBLIC EMERALD_API ral_present_task varia_skybox_get_present_task(varia_skybox     skybox,
                                                                  system_matrix4x4 modelview,
                                                                  system_matrix4x4 inverted_projection);

#endif /* VARIA_SKYBOX_H */