/**
 *
 * Test demo application @2015
 *
 */
#include "shared.h"
#include "ogl/ogl_pipeline.h"
#include "ogl/ogl_scene_renderer.h"
#include "scene/scene.h"
#include "scene/scene_camera.h"
#include "scene/scene_graph.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"


float                _animation_duration_float = 0.0f;
system_timeline_time _animation_duration_time  = 0;
ogl_pipeline         _pipeline                 = NULL;
uint32_t             _pipeline_stage_id        = -1;
scene                _scene                    = NULL;
system_timeline_time _scene_duration           = 0;
ogl_scene_renderer   _scene_renderer           = NULL;


/** Please see header for spec */
PUBLIC void state_deinit()
{
    if (_scene != NULL)
    {
        scene_release(_scene);

        _scene = NULL;
    }

    if (_scene_renderer != NULL)
    {
        ogl_scene_renderer_release(_scene_renderer);

        _scene_renderer = NULL;
    }

    if (_pipeline != NULL)
    {
        ogl_pipeline_release(_pipeline);

        _pipeline = NULL;
    }
}

/** Please see header for spec */
PUBLIC system_timeline_time state_get_animation_duration_time()
{
    return _animation_duration_time;
}

/** Please see header for spec */
PUBLIC ogl_pipeline state_get_pipeline()
{
    return _pipeline;
}

/** Please see header for spec */
PUBLIC uint32_t state_get_pipeline_stage_id()
{
    return _pipeline_stage_id;
}

/** Please see header for spec */
PUBLIC scene state_get_scene()
{
    return _scene;
}

/** Please see header for spec */
PUBLIC ogl_scene_renderer state_get_scene_renderer()
{
    return _scene_renderer;
}

/** Please see header for spec */
PUBLIC void state_init(__in __notnull system_hashed_ansi_string scene_filename)
{
    const float camera_start_position[3] = {0, 0, 0};

    /* Load the scene */
    _scene = scene_load(_context,
                        scene_filename);

    /* Determine animation duration */
    float                animation_duration_float = 0.0f;
    system_timeline_time animation_duration       = 0;

    scene_get_property(_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &animation_duration_float);

    _scene_duration = system_time_get_timeline_time_for_msec( uint32_t(animation_duration_float * 1000.0f) );

    /* Determine the animation duration. */
    scene_get_property(_scene,
                       SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                      &_animation_duration_float);

    _animation_duration_time = system_time_get_timeline_time_for_msec( uint32_t(_animation_duration_float * 1000.0f) );

    /* Carry on initializing */
    _scene_renderer = ogl_scene_renderer_create(_context,
                                                _scene);

    /* Construct the pipeline object */
    _pipeline = ogl_pipeline_create(_context,
                                    true, /* should_overlay_performance_info */
                                    system_hashed_ansi_string_create("Pipeline") );

    _pipeline_stage_id = ogl_pipeline_add_stage(_pipeline);

    ogl_pipeline_add_stage_step(_pipeline,
                                _pipeline_stage_id,
                                system_hashed_ansi_string_create("Scene rendering"),
                                _render_scene,
                                NULL);
}
