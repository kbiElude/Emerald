/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef STAGE_STEP_BACKGROUND_H
#define STAGE_STEP_BACKGROUND_H

#include "system/system_types.h"
#include "ogl/ogl_types.h"
#include "ral/ral_types.h"

/** TODO */
PUBLIC void stage_step_background_deinit(ral_context context);

/** TODO */
PUBLIC ral_texture stage_step_background_get_background_texture();

/** TODO */
PUBLIC ral_texture stage_step_background_get_result_texture();

/** TODO */
PUBLIC void stage_step_background_init(ral_context  context,
                                       ogl_pipeline pipeline,
                                       uint32_t     stage_id);

#endif /* STAGE_STEP_BACKGROUND_H */
