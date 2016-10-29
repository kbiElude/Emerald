/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_BACKGROUND_H
#define STAGE_STEP_BACKGROUND_H

#include "system/system_types.h"
#include "ral/ral_types.h"

/** TODO */
PUBLIC void stage_step_background_deinit(ral_context context);

/** TODO */
PUBLIC ral_texture_view stage_step_background_get_bg_texture_view();

/** TODO */
PUBLIC ral_present_task stage_step_background_get_present_task();

/** TODO */
PUBLIC void stage_step_background_init(ral_context context);

#endif /* STAGE_STEP_BACKGROUND_H */
