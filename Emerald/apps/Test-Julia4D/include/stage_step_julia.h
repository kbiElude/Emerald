/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_JULIA_H
#define STAGE_STEP_JULIA_H

#include "system/system_types.h"
#include "ogl/ogl_types.h"

/** TODO */
PUBLIC void stage_step_julia_deinit(ral_context context);

/** TODO */
PUBLIC void stage_step_julia_init(ral_context  context,
                                  ogl_pipeline pipeline,
                                  uint32_t     stage_id);

#endif /* STAGE_STEP_JULIA_H */
