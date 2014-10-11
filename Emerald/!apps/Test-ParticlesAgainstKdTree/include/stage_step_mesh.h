/**
 *
 *
 */
#ifndef STAGE_STEP_MESH_H
#define STAGE_STEP_MESH_H

#include "shared.h"
#include "ogl/ogl_types.h"

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_mesh_deinit(__in __notnull ogl_context,
                                                          __in __notnull ogl_pipeline);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_mesh_init(__in __notnull ogl_context,
                                                        __in __notnull ogl_pipeline,
                                                        __in           uint32_t /* stage_id */);

PUBLIC uint32_t stage_particle_get_stage_step_id();

#endif /* STAGE_STEP_MESH_H */
