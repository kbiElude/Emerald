/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_DOF_SCHEUERMANN_H
#define STAGE_STEP_DOF_SCHEUERMANN_H

#include "system/system_types.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"

/** TODO */
PUBLIC void stage_step_dof_scheuermann_deinit(ral_context context);

/** TODO */
PUBLIC ral_framebuffer stage_step_dof_scheuermann_get_combination_fbo();

/** TODO */
PUBLIC ral_texture stage_step_dof_scheuermann_get_combined_texture();

/** TODO */
PUBLIC ral_texture stage_step_dof_scheuermann_get_downsampled_blurred_texture();

/** TODO */
PUBLIC ral_texture stage_step_dof_scheuermann_get_downsampled_texture();

/** TODO */
PUBLIC void stage_step_dof_scheuermann_init(ral_context  context,
                                            ogl_pipeline pipeline,
                                            uint32_t     stage_id);

#endif /* STAGE_STEP_DOF_SCHEUERMANN_H */
