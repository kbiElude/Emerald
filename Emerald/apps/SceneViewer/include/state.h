/**
 *
 * Internal Emerald scene viewer (kbi/elude @2014-2015)
 *
 */
#ifndef STATE_H
#define STATE_H

/** TODO */
PUBLIC void state_deinit();

/** TODO */
PUBLIC unsigned int state_get_active_camera_index();

/** TODO */
PUBLIC unsigned int state_get_active_camera_path_index();

/** TODO */
PUBLIC system_time state_get_animation_duration_time();

/** TODO */
PUBLIC void** state_get_camera_indices();

/** TODO */
PUBLIC system_hashed_ansi_string* state_get_camera_names();

/** TODO */
PUBLIC system_resizable_vector state_get_cameras();

/** TODO */
PUBLIC void** state_get_camera_path_indices();

/** TODO */
PUBLIC system_hashed_ansi_string* state_get_camera_path_names();

/** TODO */
PUBLIC ogl_curve_renderer state_get_curve_renderer();

/** TODO */
PUBLIC ogl_curve_item_id state_get_curve_renderer_item_id();

/** TODO */
PUBLIC system_time state_get_last_frame_time();

/** TODO */
PUBLIC bool state_get_playback_status();

/** TODO */
PUBLIC void state_lock_current_camera(scene_camera* out_current_camera,
                                      bool*         out_is_flyby);

/** TODO */
PUBLIC uint32_t state_get_number_of_cameras();

/** TODO */
PUBLIC ogl_pipeline state_get_pipeline();

/** TODO */
PUBLIC uint32_t state_get_pipeline_stage_id();

/** TODO */
PUBLIC scene state_get_scene();

/** TODO */
PUBLIC ogl_scene_renderer state_get_scene_renderer();

/** TODO */
PUBLIC bool state_init(system_hashed_ansi_string scene_filename);

/** TODO */
PUBLIC void state_set_active_camera_index(unsigned int index);

/** TODO */
PUBLIC void state_set_active_camera_path_index(unsigned int index);

/** TODO */
PUBLIC void state_set_last_frame_time(system_time time);

/** TODO */
PUBLIC void state_set_playback_status(bool new_status);

/** TODO */
PUBLIC void state_unlock_current_camera();

#endif /* STATE_H */