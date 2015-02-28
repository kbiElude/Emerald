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
PUBLIC void state_get_current_frame_properties(__out __notnull scene*                out_current_scene,
                                               __out __notnull scene_camera*         out_current_scene_camera,
                                               __out __notnull ogl_scene_renderer*   out_current_renderer,
                                               __out           system_timeline_time* out_current_frame_time);

/** TODO */
PUBLIC ogl_pipeline state_get_pipeline();

/** TODO */
PUBLIC uint32_t state_get_pipeline_stage_id();

/** TODO */
PUBLIC void state_init();

#endif /* STATE_H */