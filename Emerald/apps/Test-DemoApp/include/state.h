/**
 *
 * Test demo application @2015
 *
 */
#ifndef STATE_H
#define STATE_H

#include "postprocessing/postprocessing_blur_gaussian.h"
#include "ral/ral_types.h"
#include "scene/scene_light.h"


/** TODO */
PUBLIC void state_deinit();

/** TODO */
PUBLIC void state_get_current_frame_properties(scene*          out_current_scene,
                                               scene_camera*   out_current_scene_camera,
                                               scene_renderer* out_current_renderer,
                                               system_time*    out_current_frame_time);

/** TODO */
PUBLIC postprocessing_blur_gaussian_resolution state_get_color_shadow_map_blur_resolution();

/** TODO */
PUBLIC ral_texture_format state_get_color_shadow_map_format();

/** TODO */
PUBLIC ral_texture_format state_get_depth_shadow_map_format();

/** TODO */
PUBLIC scene_light_shadow_map_algorithm state_get_shadow_map_algorithm();

/** TODO */
PUBLIC scene_light_shadow_map_pointlight_algorithm state_get_shadow_map_pointlight_algorithm();

/** TODO */
PUBLIC unsigned int state_get_shadow_map_size();

/** TODO */
PUBLIC float state_get_shadow_map_vsm_blur_n_passes();

/** TODO */
PUBLIC unsigned int state_get_shadow_map_vsm_blur_taps();

/** TODO */
PUBLIC float state_get_shadow_map_vsm_cutoff();

/** TODO */
PUBLIC float state_get_shadow_map_vsm_max_variance();

/** TODO */
PUBLIC float state_get_shadow_map_vsm_min_variance();

/** TODO */
PUBLIC ogl_pipeline state_get_pipeline();

/** TODO */
PUBLIC uint32_t state_get_pipeline_stage_id();

/** TODO */
PUBLIC void state_init();

/** TODO */
PUBLIC void state_set_color_shadow_map_blur_resolution(postprocessing_blur_gaussian_resolution blur_resolution);

/** TODO */
PUBLIC void state_set_color_shadow_map_format(ral_texture_format new_format);

/** TODO */
PUBLIC void state_set_current_frame_time(system_time frame_time);

/** TODO */
PUBLIC void state_set_depth_shadow_map_format(ral_texture_format new_format);

/** TODO */
PUBLIC void state_set_shadow_map_algorithm(scene_light_shadow_map_algorithm sm_algo);

/** TODO */
PUBLIC void state_set_shadow_map_pointlight_algorithm(scene_light_shadow_map_pointlight_algorithm pl_algo);

/** TODO */
PUBLIC void state_set_shadow_map_size(unsigned int new_shadow_map_size);

/** TODO */
PUBLIC void state_set_shadow_map_vsm_blur_n_passes(float new_value);

/** TODO */
PUBLIC void state_set_shadow_map_vsm_blur_taps(unsigned int new_value);

/** TODO */
PUBLIC void state_set_shadow_map_vsm_cutoff(float new_value);

/** TODO */
PUBLIC void state_set_shadow_map_vsm_max_variance(float new_value);

/** TODO */
PUBLIC void state_set_shadow_map_vsm_min_variance(float new_value);

#endif /* STATE_H */