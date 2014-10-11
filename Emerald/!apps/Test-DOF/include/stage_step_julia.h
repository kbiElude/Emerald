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
PUBLIC void stage_step_julia_deinit(ogl_context context);

/** TODO */
PUBLIC ogl_texture stage_step_julia_get_color_texture();

/** TODO */
PUBLIC ogl_texture stage_step_julia_get_depth_texture();

/** TODO */
PUBLIC GLuint stage_step_julia_get_fbo_id();

/** TODO */
PUBLIC void stage_step_julia_init(ogl_context context, ogl_pipeline pipeline, uint32_t stage_id);

#endif /* STAGE_STEP_JULIA_H */
