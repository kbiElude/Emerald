/**
 *
 * Test demo application @2015
 *
 */
#ifndef STATE_H
#define STATE_H

#include "scene/scene_light.h"


/** TODO */
PUBLIC void state_deinit();

/** TODO */
PUBLIC void state_get_current_frame_properties(__out __notnull scene*                out_current_scene,
                                               __out __notnull scene_camera*         out_current_scene_camera,
                                               __out __notnull ogl_scene_renderer*   out_current_renderer,
                                               __out           system_timeline_time* out_current_frame_time);

/** TODO */
PUBLIC ogl_texture_internalformat state_get_color_shadow_map_internalformat();

/** TODO */
PUBLIC ogl_texture_internalformat state_get_depth_shadow_map_internalformat();

/** TODO */
PUBLIC scene_light_shadow_map_algorithm state_get_shadow_map_algorithm();

/** TODO */
PUBLIC scene_light_shadow_map_pointlight_algorithm state_get_shadow_map_pointlight_algorithm();

/** TODO */
PUBLIC unsigned int state_get_shadow_map_size();

/** TODO */
PUBLIC ogl_pipeline state_get_pipeline();

/** TODO */
PUBLIC uint32_t state_get_pipeline_stage_id();

/** TODO */
PUBLIC void state_init();

/** TODO */
PUBLIC void state_set_color_shadow_map_internalformat(__in ogl_texture_internalformat new_internalformat);

/** TODO */
PUBLIC void state_set_depth_shadow_map_internalformat(__in ogl_texture_internalformat new_internalformat);

/** TODO */
PUBLIC void state_set_shadow_map_algorithm(__in scene_light_shadow_map_algorithm sm_algo);

/** TODO */
PUBLIC void state_set_shadow_map_pointlight_algorithm(__in scene_light_shadow_map_pointlight_algorithm pl_algo);

/** TODO */
PUBLIC void state_set_shadow_map_size(__in unsigned int new_shadow_map_size);

#endif /* STATE_H */