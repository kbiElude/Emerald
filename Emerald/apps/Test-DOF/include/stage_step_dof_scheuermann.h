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
PUBLIC ral_present_task stage_step_dof_scheuermann_get_blur_present_task();

/** TODO */
PUBLIC ral_present_task stage_step_dof_scheuermann_get_downsample_present_task(ral_context      context,
                                                                               ral_texture_view julia_color_rt_texture_view);

/** TODO */
PUBLIC ral_present_task stage_step_dof_scheuermann_get_present_task(ral_context      context,
                                                                    ral_texture_view bg_texture_view,
                                                                    ral_texture_view dof_scheuermann_blurred_texture_view,
                                                                    ral_texture_view julia_color_texture_view,
                                                                    ral_texture_view julia_depth_texture_view);

/** TODO */
PUBLIC ral_texture_view stage_step_dof_get_blurred_texture_view();

/** TODO */
PUBLIC void stage_step_dof_scheuermann_init(ral_context  context);

#endif /* STAGE_STEP_DOF_SCHEUERMANN_H */
