/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_JULIA_H
#define STAGE_STEP_JULIA_H

#include "system/system_types.h"

/** TODO */
PUBLIC void stage_step_julia_deinit(ral_context context);

/** TODO */
PUBLIC ral_present_task stage_step_julia_get_present_task();

/** TODO */
PUBLIC void stage_step_julia_init(ral_context      context,
                                  ral_texture_view color_rt_texture_view,
                                  ral_texture_view depth_rt_texture_view);

#endif /* STAGE_STEP_JULIA_H */
