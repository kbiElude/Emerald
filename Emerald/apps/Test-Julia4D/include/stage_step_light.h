/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_LIGHT_H
#define STAGE_STEP_LIGHT_H

#include "system/system_types.h"

/** TODO */
PUBLIC void stage_step_light_deinit(ral_context context);

/** TODO */
PUBLIC void stage_step_light_init(ral_context      context,
                                  ral_texture_view color_rt_texture_view,
                                  ral_texture_view depth_rt_texture_view);

#endif /* STAGE_STEP_LIGHT_H */
