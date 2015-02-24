/**
 *
 * Test demo application @2015
 *
 */
#ifndef STATE_H
#define STATE_H

/** TODO */
PUBLIC void state_deinit();

/** TODO */
PUBLIC system_timeline_time state_get_animation_duration_time(unsigned int n_scene);

/** TODO */
PUBLIC ogl_pipeline state_get_pipeline();

/** TODO */
PUBLIC uint32_t state_get_pipeline_stage_id();

/** TODO */
PUBLIC scene state_get_scene(unsigned int n_scene);

/** TODO */
PUBLIC ogl_scene_renderer state_get_scene_renderer(unsigned int n_scene);

/** TODO */
PUBLIC void state_init();

#endif /* STATE_H */