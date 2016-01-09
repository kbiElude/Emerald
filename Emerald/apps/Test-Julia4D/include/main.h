/**
 *
 * Object browser test app (kbi/elude @2012)
 *
 */
#ifndef MAIN_H
#define MAIN_H

#include "demo/demo_types.h"


/** TODO */
const float* main_get_data_vector();

/** TODO */
float main_get_epsilon();

/** TODO */
float main_get_escape_threshold();

/** TODO */
const float* main_get_light_color();

/** TODO */
const float* main_get_light_position();

/** TODO */
int main_get_max_iterations();

/** TODO */
system_matrix4x4 main_get_projection_matrix();

/** TODO */
float main_get_raycast_radius_multiplier();

/** TODO */
bool main_get_shadows_status();

/** TODO */
float main_get_specularity();


extern demo_flyby _flyby;


#endif /* MAIN_H */
