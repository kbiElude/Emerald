/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_DOF_SCHEUERMANN_H
#define STAGE_STEP_DOF_SCHEUERMANN_H

#include "system/system_types.h"
#include "ogl/ogl_types.h"

/** TODO */
PUBLIC void stage_step_dof_scheuermann_deinit(ogl_context context);

/** TODO */
PUBLIC GLuint stage_step_dof_scheuermann_get_combination_fbo_id();

/** TODO */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_combined_texture();

/** TODO */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_downsampled_blurred_texture();

/** TODO */
PUBLIC ogl_texture stage_step_dof_scheuermann_get_downsampled_texture();

/** TODO */
PUBLIC void stage_step_dof_scheuermann_init(ogl_context context, ogl_pipeline pipeline, uint32_t stage_id);

#endif /* STAGE_STEP_DOF_SCHEUERMANN_H */
