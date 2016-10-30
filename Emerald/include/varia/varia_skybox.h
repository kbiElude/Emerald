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
    /* not settable; uint32_t */
    VARIA_SKYBOX_PROPERTY_INV_PROJ_MAT4_OFFSET,

    /* not settable; uint32_t */
    VARIA_SKYBOX_PROPERTY_MV_MAT4_OFFSET,

    /* not settable; ral_program_block_buffer */
    VARIA_SKYBOX_PROPERTY_PROGRAM_BLOCK_BUFFER,

} varia_skybox_property;

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
PUBLIC EMERALD_API void varia_skybox_get_property(varia_skybox          skybox,
                                                  varia_skybox_property property,
                                                  void*                 out_result_ptr);
/** TODO.
 *
 *  Note: update_task is a user-specified present task which updates buffer memory with correspondingly:
 *
 *  1) modelview matrix (mat4 "mv" uniform)
 *  2) inverted projection matrix (mat4 "inv_projection" uniform)
 *
 *  Matrix data needs to be uploaded to a buffer, whose manager which can be retrieved by querying varia_skybox's
 *  relevant property. The task needs to expose a single output, referring to the buffer memory.
 *
 *
 *  Result present task's structure:
 *
 *  Output 0: Texture view to render the skybox on.
 */
PUBLIC EMERALD_API ral_present_task varia_skybox_get_present_task(varia_skybox     skybox,
                                                                  ral_texture_view target_texture_view,
                                                                  ral_present_task matrix_update_task);

#endif /* VARIA_SKYBOX_H */