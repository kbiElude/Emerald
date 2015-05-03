/**
 *
 *
 */
#ifndef STAGE_STEP_PARTICLES_H
#define STAGE_STEP_PARTICLES_H

#include "shared.h"
#include "ogl/ogl_types.h"

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_particles_deinit(__in __notnull ogl_context,
                                                               __in __notnull ogl_pipeline);

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_step_particles_init(__in __notnull ogl_context,
                                                             __in __notnull ogl_pipeline,
                                                             __in           uint32_t /* stage_id */);

/** TODO */
PUBLIC bool stage_step_particles_get_debug();

/** TODO */
PUBLIC float stage_step_particles_get_decay();

/** TODO */
PUBLIC float stage_step_particles_get_dt();

/** TODO */
PUBLIC float stage_step_particles_get_gravity();

/** TODO */
PUBLIC float stage_step_particles_get_maximum_mass_delta();

/** TODO */
PUBLIC float stage_step_particles_get_minimum_mass();

/** TODO */
PUBLIC float stage_step_particles_get_spread();

/** TODO */
PUBLIC void stage_step_particles_iterate_frame();

/** TODO */
PUBLIC void stage_step_particles_reset();

/** TODO */
PUBLIC void stage_step_particles_set_debug(bool);

/** TODO */
PUBLIC void stage_step_particles_set_decay(__in float new_decay);

/** TODO */
PUBLIC void stage_step_particles_set_dt(__in float new_dt);

/** TODO */
PUBLIC void stage_step_particles_set_gravity(__in float new_gravity);

/** TODO */
PUBLIC void stage_step_particles_set_maximum_mass_delta(__in float new_max_mass_delta);

/** TODO */
PUBLIC void stage_step_particles_set_minimum_mass(__in float new_min_mass);

/** TODO */
PUBLIC void stage_step_particles_set_spread(__in float new_spread);

#endif /* STAGE_STEP_PARTICLES_H */
