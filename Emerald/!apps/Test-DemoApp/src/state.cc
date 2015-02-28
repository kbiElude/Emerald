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
#include "system/system_log.h"
#include "app_config.h"
#include "include/main.h"
#include "state.h"

/* A descriptor for a single scene */
typedef struct _scene_descriptor
{
    system_timeline_time duration;
    float                duration_float;
    ogl_scene_renderer   renderer;
    scene                this_scene;

    _scene_descriptor()
    {
        duration       = 0;
        duration_float = 0.0f;
        renderer       = NULL;
        this_scene     = NULL;
    }

    ~_scene_descriptor()
    {
        ASSERT_DEBUG_SYNC(renderer == NULL,
                          "ogl_scene_renderer not deinitialized but _scene_descriptor instance about to be deallocated");

        ASSERT_DEBUG_SYNC(this_scene == NULL,
                          "scene not deinitialized but _scene_descriptor instance about to be deallocated");
    }
} _scene_descriptor;

/* Array of scene filenames to use for the demo */
const char* scene_filenames[] =
{
    "blob/scene1/test",
    "blob/scene2/test",
    "blob/scene3/test"
};
const unsigned int n_scene_filenames = sizeof(scene_filenames) / sizeof(scene_filenames[0]);

/* Other stuff */
ogl_pipeline         _pipeline                 = NULL;
uint32_t             _pipeline_stage_id        = -1;
_scene_descriptor    _scenes[n_scene_filenames];


/** Please see header for spec */
PUBLIC void state_deinit()
{
    for (unsigned int n_scene = 0;
                      n_scene < n_scene_filenames;
                    ++n_scene)
    {
        if (_scenes[n_scene].renderer != NULL)
        {
            ogl_scene_renderer_release(_scenes[n_scene].renderer);

            _scenes[n_scene].renderer = NULL;
        }

        if (_scenes[n_scene].this_scene != NULL)
        {
            scene_release(_scenes[n_scene].this_scene);

            _scenes[n_scene].this_scene = NULL;
        }
    } /* for (all scenes) */

    if (_pipeline != NULL)
    {
        ogl_pipeline_release(_pipeline);

        _pipeline = NULL;
    }
}

/** Please see header for spec */
PUBLIC system_timeline_time state_get_animation_duration_time(unsigned int n_scene)
{
    ASSERT_DEBUG_SYNC(n_scene < n_scene_filenames,
                      "Invalid scene index");

    return _scenes[n_scene].duration;
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
PUBLIC scene state_get_scene(unsigned int n_scene)
{
    ASSERT_DEBUG_SYNC(n_scene < n_scene_filenames,
                      "Invalid scene index");

    return _scenes[n_scene].this_scene;
}

/** Please see header for spec */
PUBLIC ogl_scene_renderer state_get_scene_renderer(unsigned int n_scene)
{
    ASSERT_DEBUG_SYNC(n_scene < n_scene_filenames,
                      "Invalid scene index");

    return _scenes[n_scene].renderer;
}

/** Please see header for spec */
PUBLIC void state_init()
{
    const float camera_start_position[3] = {0, 0, 0};

    /* Load the scenes */
    system_timeline_time loading_time_start = system_time_now();

    for (unsigned int n_scene = 0;
                      n_scene < n_scene_filenames;
                    ++n_scene)
    {
        _scenes[n_scene].this_scene = scene_load(_context,
                                                 system_hashed_ansi_string_create(scene_filenames[n_scene]) );

        /* Determine animation duration */
        float                animation_duration_float = 0.0f;
        system_timeline_time animation_duration       = 0;

        scene_get_property(_scenes[n_scene].this_scene,
                           SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                          &animation_duration_float);

        _scenes[n_scene].duration = system_time_get_timeline_time_for_msec( uint32_t(animation_duration_float * 1000.0f) );

        /* Determine the animation duration. */
        scene_get_property(_scenes[n_scene].this_scene,
                           SCENE_PROPERTY_MAX_ANIMATION_DURATION,
                          &_scenes[n_scene].duration_float);

        _scenes[n_scene].duration = system_time_get_timeline_time_for_msec( uint32_t(_scenes[n_scene].duration_float * 1000.0f) );

        /* Initialize scene renderers */
        _scenes[n_scene].renderer = ogl_scene_renderer_create(_context,
                                                              _scenes[n_scene].this_scene);
    }

    system_timeline_time loading_time_end  = system_time_now();
    uint32_t             loading_time_msec = 0;

    system_time_get_msec_for_timeline_time(loading_time_end - loading_time_start,
                                          &loading_time_msec);

    LOG_INFO("Scene loading time: %d.%4f s",
                   loading_time_msec / 1000,
             float(loading_time_msec % 1000) / 1000.0f);

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
