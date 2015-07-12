/**
 *
 * Particles as a pipeline stage (in case we want to reuse this)
 *
 */
#ifndef STAGE_PARTICLE_H
#define STAGE_PARTICLE_H

#include "shared.h"
#include "ogl/ogl_types.h"

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_particle_deinit(ogl_context,
                                                         ogl_pipeline);

/** TODO */
PUBLIC float stage_particle_get_decay();

/** TODO */
PUBLIC float stage_particle_get_dt();

/** TODO */
PUBLIC float stage_particle_get_gravity();

/** TODO */
PUBLIC float stage_particle_get_maximum_mass_delta();

/** TODO */
PUBLIC float stage_particle_get_minimum_mass();

/** TODO */
PUBLIC float stage_particle_get_spread();

/** TODO */
PUBLIC uint32_t stage_particle_get_stage_id();

/** TODO */
PUBLIC RENDERING_CONTEXT_CALL void stage_particle_init(ogl_context,
                                                       ogl_pipeline);

/** TODO */
PUBLIC void stage_particle_set_decay(float new_decay);

/** TODO */
PUBLIC void stage_particle_set_dt(float new_dt);

/** TODO */
PUBLIC void stage_particle_set_gravity(float new_gravity);

/** TODO */
PUBLIC void stage_particle_set_maximum_mass_delta(float new_max_mass_delta);

/** TODO */
PUBLIC void stage_particle_set_minimum_mass(float new_min_mass);

/** TODO */
PUBLIC void stage_particle_set_spread(float new_spread);

/** TODO */
PUBLIC void stage_particle_reset();

#endif /* STAGE_PARTICLE_H */
